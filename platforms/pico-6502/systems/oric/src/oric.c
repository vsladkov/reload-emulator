/*
    oric.c
*/
#define CHIPS_IMPL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <pico/platform.h>
#include "pico/stdlib.h"

#include "roms/pravetz8d_roms.h"
#include "images/oric_images.h"

#include "chips/chips_common.h"
#include "chips/mos6522via.h"
#include "chips/ay38910psg.h"
#include "chips/kbd.h"
#include "chips/mem.h"
#include "chips/clk.h"
#include "devices/oric_td.h"
#include "devices/disk2_fdd.h"
#include "devices/disk2_fdc.h"
#include "devices/oric_fdc_rom.h"
#include "systems/oric.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/ssi.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/sem.h"

#include "tmds_encode.h"

#include "common_dvi_pin_configs.h"
#include "dvi.h"
#include "dvi_serialiser.h"

#include "audio.h"

#include "tusb.h"

typedef struct {
    uint32_t version;
    oric_t oric;
} oric_snapshot_t;

typedef struct {
    oric_t oric;
    uint32_t frame_time_us;
    uint32_t ticks;
    // double emu_time_ms;
} state_t;

state_t __not_in_flash() state;

// audio-streaming callback
static void push_audio(const uint8_t *samples, int num_samples, void *user_data) {
    (void)user_data;
    audio_play_once(samples, num_samples);
}

// get oric_desc_t struct based on joystick type
oric_desc_t oric_desc(void) {
    return (oric_desc_t){
        .td_enabled = true,
        .fdc_enabled = true,
        .audio =
            {
                .callback = {.func = push_audio},
                .sample_rate = 22050,
            },
        .roms =
            {
                .rom = {.ptr = oric_rom, .size = sizeof(oric_rom)},
                .boot_rom = {.ptr = oric_boot_rom, .size = sizeof(oric_boot_rom)},
            },
    };
}

void app_init(void) {
    oric_desc_t desc = oric_desc();
    oric_init(&state.oric, &desc);
}

// TMDS bit clock 295.2 MHz
// DVDD 1.2V
#define FRAME_WIDTH  800
#define FRAME_HEIGHT 480
#define VREG_VSEL    VREG_VOLTAGE_1_20
#define DVI_TIMING   dvi_timing_800x480p_60hz

// TMDS bit clock 400 MHz
// DVDD 1.3V
// #define FRAME_WIDTH  800
// #define FRAME_HEIGHT 600
// #define VREG_VSEL    VREG_VOLTAGE_1_30
// #define DVI_TIMING   dvi_timing_800x600p_60hz

// TMDS bit clock 252 MHz
// DVDD 1.2V (1.1V seems ok too)
// #define FRAME_WIDTH  640
// #define FRAME_HEIGHT 480
// #define VREG_VSEL    VREG_VOLTAGE_1_10
// #define DVI_TIMING   dvi_timing_640x480p_60hz

#define PALETTE_BITS 3
#define PALETTE_SIZE (1 << PALETTE_BITS)

#define RGBA8(r, g, b) (0xFF000000 | (r << 16) | (g << 8) | (b))

static const uint32_t oric_palette[PALETTE_SIZE] = {
    RGBA8(0x00, 0x00, 0x00), /* black */
    RGBA8(0xFF, 0x00, 0x00), /* red */
    RGBA8(0x00, 0xFF, 0x00), /* green */
    RGBA8(0xFF, 0xFF, 0x00), /* yellow */
    RGBA8(0x00, 0x00, 0xFF), /* blue */
    RGBA8(0xFF, 0x00, 0xFF), /* magenta */
    RGBA8(0x00, 0xFF, 0xFF), /* cyan */
    RGBA8(0xFF, 0xFF, 0xFF), /* white */
};

uint32_t __not_in_flash() tmds_palette[PALETTE_SIZE * 6];
uint32_t __not_in_flash() empty_tmdsbuf[3 * FRAME_WIDTH / DVI_SYMBOLS_PER_WORD];

uint8_t __not_in_flash() scanbuf[FRAME_WIDTH];

struct dvi_inst dvi0;

void tmds_palette_init() { tmds_setup_palette24_symbols(oric_palette, tmds_palette, PALETTE_SIZE); }

void kbd_raw_key_down(int code) {
    if (isascii(code)) {
        if (isupper(code)) {
            code = tolower(code);
        } else if (islower(code)) {
            code = toupper(code);
        }
    }
    oric_key_down(&state.oric, (code));
}

void kbd_raw_key_up(int code) {
    if (isascii(code)) {
        if (isupper(code)) {
            code = tolower(code);
        } else if (islower(code)) {
            code = toupper(code);
        }
    }
    oric_key_up(&state.oric, code);
}

extern void oric_render_scanline_2x(const uint32_t *pixbuf, uint32_t *scanbuf, size_t n_pix);
extern void oric_render_scanline_3x(const uint32_t *pixbuf, uint32_t *scanbuf, size_t n_pix);
extern void copy_tmdsbuf(uint32_t *dest, const uint32_t *src);

static inline void __not_in_flash_func(render_scanline)(const uint32_t *pixbuf, uint32_t *scanbuf, size_t n_pix) {
    interp_config c;

    c = interp_default_config();
    interp_config_set_cross_result(&c, true);
    interp_config_set_shift(&c, 0);
    interp_config_set_mask(&c, 0, 3);
    interp_config_set_signed(&c, false);
    interp_set_config(interp0, 0, &c);

    c = interp_default_config();
    interp_config_set_cross_result(&c, false);
    interp_config_set_shift(&c, 4);
    interp_config_set_mask(&c, 0, 31);
    interp_config_set_signed(&c, false);
    interp_set_config(interp0, 1, &c);

    oric_render_scanline_3x(pixbuf, scanbuf, n_pix);
}

#define ORIC_EMPTY_LINES   ((FRAME_HEIGHT - ORIC_SCREEN_HEIGHT * 2) / 4)
#define ORIC_EMPTY_COLUMNS ((FRAME_WIDTH - ORIC_SCREEN_WIDTH * 3) / 2)

static inline void __not_in_flash_func(render_empty_scanlines)() {
    for (int y = 0; y < ORIC_EMPTY_LINES; y += 2) {
        uint32_t *tmdsbuf;
        queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmdsbuf);
        copy_tmdsbuf(tmdsbuf, empty_tmdsbuf);
        queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmdsbuf);

        queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmdsbuf);
        copy_tmdsbuf(tmdsbuf, empty_tmdsbuf);
        queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmdsbuf);
    }
}

static inline void __not_in_flash_func(render_frame)() {
    for (int y = 0; y < ORIC_SCREEN_HEIGHT; y += 2) {
        uint32_t *tmdsbuf;
        queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmdsbuf);
        render_scanline((const uint32_t *)(&state.oric.fb[y * 120]), (uint32_t *)(&scanbuf[ORIC_EMPTY_COLUMNS]), 120);
        tmds_encode_palette_data((const uint32_t *)scanbuf, tmds_palette, tmdsbuf, FRAME_WIDTH, PALETTE_BITS);
        queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmdsbuf);

        queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmdsbuf);
        render_scanline((const uint32_t *)(&state.oric.fb[(y + 1) * 120]), (uint32_t *)(&scanbuf[ORIC_EMPTY_COLUMNS]), 120);
        tmds_encode_palette_data((const uint32_t *)scanbuf, tmds_palette, tmdsbuf, FRAME_WIDTH, PALETTE_BITS);
        queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmdsbuf);
    }
}

void __not_in_flash_func(core1_main()) {
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    dvi_start(&dvi0);

    while (1) {
        render_empty_scanlines();
        render_frame();
        render_empty_scanlines();
    }

    __builtin_unreachable();
}

bool __not_in_flash_func(repeating_timer_20hz_callback)(struct repeating_timer *t) {
    tuh_task();
    audio_mixer_step();
    oric_screen_update(&state.oric);
    return true;
}

bool __not_in_flash_func(repeating_timer_1khz_callback)(struct repeating_timer *t) {
    // struct timeval tv;
    // gettimeofday(&tv, NULL);
    // unsigned long start_time_in_micros = 1000000 * tv.tv_sec + tv.tv_usec;

    uint32_t num_ticks = clk_us_to_ticks(ORIC_FREQUENCY, 1000);
    for (uint32_t ticks = 0; ticks < num_ticks; ticks++) {
        oric_tick(&state.oric);
    }
    kbd_update(&state.oric.kbd, 1000);

    // gettimeofday(&tv, NULL);
    // unsigned long end_time_in_micros = 1000000 * tv.tv_sec + tv.tv_usec;
    // int execution_time = end_time_in_micros - start_time_in_micros;
    // printf("%d us\n", execution_time);

    return true;
}

int main() {
    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

    stdio_init_all();

    app_init();
    tmds_palette_init();
    tusb_init();
    audio_init(_ORIC_AUDIO_PIN, 22050);

    printf("Configuring DVI\n");

    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    tmds_encode_palette_data((const uint32_t *)scanbuf, tmds_palette, empty_tmdsbuf, FRAME_WIDTH, PALETTE_BITS);

    printf("Core 1 start\n");
    // hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);
    multicore_launch_core1(core1_main);

    struct repeating_timer timer_20hz;
    add_repeating_timer_us(-50000, repeating_timer_20hz_callback, NULL, &timer_20hz);

    struct repeating_timer timer_1khz;
    add_repeating_timer_us(-1000, repeating_timer_1khz_callback, NULL, &timer_1khz);

    while (1) __wfe();
    __builtin_unreachable();
}
