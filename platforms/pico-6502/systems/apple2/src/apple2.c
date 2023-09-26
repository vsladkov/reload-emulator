/*
    apple2.c
*/
#define CHIPS_IMPL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <pico/platform.h>
#include "pico/stdlib.h"

#include "roms/apple2_roms.h"
// #include "disks/neptune.h"
#include "disks/moon_patrol.h"
// #include "disks/karateka.h"
// #include "disks/lode_runner.h"

#include "chips/chips_common.h"
#include "chips/beeper.h"
#include "chips/kbd.h"
#include "chips/mem.h"
#include "chips/clk.h"
#include "devices/disk2_fdd.h"
#include "devices/disk2_fdc.h"
#include "systems/apple2.h"

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
    apple2_t apple2;
} apple2_snapshot_t;

typedef struct {
    apple2_t apple2;
    uint32_t frame_time_us;
    uint32_t ticks;
    // double emu_time_ms;
} state_t;

static state_t __not_in_flash() state;

// audio-streaming callback
static void push_audio(const uint8_t *samples, int num_samples, void *user_data) {
    (void)user_data;
    audio_play_once(samples, num_samples);
}

// get apple2_desc_t struct based on joystick type
apple2_desc_t apple2_desc(void) {
    return (apple2_desc_t){
        .fdc_enabled = true,
        .audio =
            {
                .callback = {.func = push_audio},
                .sample_rate = 22050,
            },
        .roms =
            {
                .rom = {.ptr = dump_apple2_rom, .size = sizeof(dump_apple2_rom)},
                .character_rom = {.ptr = dump_apple2_character_rom, .size = sizeof(dump_apple2_character_rom)},
                .boot_rom = {.ptr = dump_apple2_boot_rom, .size = sizeof(dump_apple2_boot_rom)},
            },
    };
}

void app_init(void) {
    apple2_desc_t desc = apple2_desc();
    apple2_init(&state.apple2, &desc);
}

// TMDS bit clock 252 MHz
// DVDD 1.2V (1.1V seems ok too)
#define FRAME_WIDTH  640
#define FRAME_HEIGHT 480
#define VREG_VSEL    VREG_VOLTAGE_1_10
#define DVI_TIMING   dvi_timing_640x480p_60hz

#define PALETTE_BITS 4
#define PALETTE_SIZE (1 << PALETTE_BITS)

#define RGBA8(r, g, b) (0xFF000000 | (r << 16) | (g << 8) | (b))

// clang-format off
static const uint32_t apple2_palette[PALETTE_SIZE] = {
    RGBA8(0x00, 0x00, 0x00), /* Black */
    RGBA8(0xA7, 0x0B, 0x4C), /* Dark Red */
    RGBA8(0x40, 0x1C, 0xF7), /* Dark Blue */
    RGBA8(0xE6, 0x28, 0xFF), /* Purple */
    RGBA8(0x00, 0x74, 0x40), /* Dark Green */
    RGBA8(0x80, 0x80, 0x80), /* Dark Gray */
    RGBA8(0x19, 0x90, 0xFF), /* Medium Blue */
    RGBA8(0xBF, 0x9C, 0xFF), /* Light Blue */
    RGBA8(0x40, 0x63, 0x00), /* Brown */
    RGBA8(0xE6, 0x6F, 0x00), /* Orange */
    RGBA8(0x80, 0x80, 0x80), /* Light Grey */
    RGBA8(0xFF, 0x8B, 0xBF), /* Pink */
    RGBA8(0x19, 0xD7, 0x00), /* Light Green */
    RGBA8(0xBF, 0xE3, 0x08), /* Yellow */
    RGBA8(0x58, 0xF4, 0xBF), /* Aquamarine */
    RGBA8(0xFF, 0xFF, 0xFF)  /* White */
};
// clang-format on

uint32_t __not_in_flash() tmds_palette[PALETTE_SIZE * 6];

uint8_t __not_in_flash() scanbuf[FRAME_WIDTH];
uint8_t __not_in_flash() empty_scanbuf[FRAME_WIDTH];

struct dvi_inst dvi0;

void tmds_palette_init() { tmds_setup_palette24_symbols(apple2_palette, tmds_palette, PALETTE_SIZE); }

void kbd_raw_key_down(int code) { apple2_key_down(&state.apple2, toupper(code)); }

void kbd_raw_key_up(int code) { apple2_key_up(&state.apple2, toupper(code)); }

extern void apple2_render_scanline(const uint32_t *pixbuf, uint32_t *line_buffer, size_t n_pix);

static inline void __not_in_flash_func(render_scanline)(const uint32_t *pixbuf, uint32_t *line_buffer,
                                                            size_t n_pix) {
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

    apple2_render_scanline(pixbuf, line_buffer, n_pix);
}

static inline void __not_in_flash_func(render_empty_scanlines)() {
    for (int y = 0; y < 24; y += 2) {
        uint32_t *tmds_buf;
        queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmds_buf);
        tmds_encode_palette_data((const uint32_t *)empty_scanbuf, tmds_palette, tmds_buf, FRAME_WIDTH,
                                 PALETTE_BITS);
        queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmds_buf);

        queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmds_buf);
        tmds_encode_palette_data((const uint32_t *)empty_scanbuf, tmds_palette, tmds_buf, FRAME_WIDTH,
                                 PALETTE_BITS);
        queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmds_buf);
    }
}

static inline void __not_in_flash_func(render_frame)() {
    for (int y = 0; y < 192; y += 2) {
        uint32_t *tmds_buf;
        queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmds_buf);
        render_scanline((const uint32_t *)(&state.apple2.fb[y * 280]), (uint32_t *)(&scanbuf[40]), 280);
        tmds_encode_palette_data((const uint32_t *)scanbuf, tmds_palette, tmds_buf, FRAME_WIDTH, PALETTE_BITS);
        queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmds_buf);

        queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmds_buf);
        render_scanline((const uint32_t *)(&state.apple2.fb[(y + 1) * 280]), (uint32_t *)(&scanbuf[40]), 280);
        tmds_encode_palette_data((const uint32_t *)scanbuf, tmds_palette, tmds_buf, FRAME_WIDTH, PALETTE_BITS);
        queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmds_buf);
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
    apple2_screen_update(&state.apple2);
    return true;
}

bool __not_in_flash_func(repeating_timer_1khz_callback)(struct repeating_timer *t) {
    // struct timeval tv;
    // gettimeofday(&tv,NULL);
    // unsigned long start_time_in_micros = 1000000 * tv.tv_sec + tv.tv_usec;

    uint32_t num_ticks = clk_us_to_ticks(APPLE2_FREQUENCY, 1000);
    for (uint32_t ticks = 0; ticks < num_ticks; ticks++) {
        apple2_tick(&state.apple2);
    }

    // gettimeofday(&tv,NULL);
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
    audio_init(_APPLE2_AUDIO_PIN, 22050);

    printf("Configuring DVI\n");

    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    printf("Core 1 start\n");
    hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);
    multicore_launch_core1(core1_main);

    struct repeating_timer timer_20hz;
    add_repeating_timer_us(-50000, repeating_timer_20hz_callback, NULL, &timer_20hz);

    struct repeating_timer timer_1khz;
    add_repeating_timer_us(-1000, repeating_timer_1khz_callback, NULL, &timer_1khz);

    while (1) __wfe();
    __builtin_unreachable();
}