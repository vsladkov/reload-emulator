// apple2e.c
//
// ## zlib/libpng license
//
// Copyright (c) 2023 Veselin Sladkov
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the
// use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//     1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software in a
//     product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//     2. Altered source versions must be plainly marked as such, and must not
//     be misrepresented as being the original software.
//     3. This notice may not be removed or altered from any source
//     distribution.

#define CHIPS_IMPL

#define MEM_PAGE_SHIFT (9U)

#define RGBA8(r, g, b) (0xFF000000 | (r << 16) | (g << 8) | (b))

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <pico/platform.h>
#include "pico/stdlib.h"

#ifdef OLIMEX_NEO6502
#include "roms/apple2ee_roms.h"
#else
#include "roms/apple2e_roms.h"
#endif
#include "images/apple2_images.h"
// #include "images/apple2_nib_images.h"

#include "tusb.h"
#include "ff.h"

#include "chips/chips_common.h"
#ifdef OLIMEX_NEO6502
#include "chips/wdc65C02cpu.h"
#else
#include "chips/mos6502cpu.h"
#endif
#include "chips/beeper.h"
#include "chips/kbd.h"
#include "chips/mem.h"
#include "chips/clk.h"
#include "devices/apple2_lc.h"
#include "devices/disk2_fdd.h"
#include "devices/disk2_fdc.h"
#include "devices/apple2_fdc_rom.h"
#include "devices/prodos_hdd_msc.h"
#include "devices/prodos_hdc.h"
#include "devices/prodos_hdc_rom.h"
#include "systems/apple2e.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/vreg.h"
#include "hardware/interp.h"
#include "pico/multicore.h"

#include "tmds_encode.h"

#include "common_dvi_pin_configs.h"
#include "dvi.h"
#include "dvi_serialiser.h"

#include "audio.h"

typedef struct {
    uint32_t version;
    apple2e_t apple2e;
} apple2e_snapshot_t;

typedef struct {
    apple2e_t apple2e;
    uint32_t frame_time_us;
    uint32_t ticks;
    // double emu_time_ms;
} state_t;

static state_t __not_in_flash() state;

// Audio streaming callback
static void audio_callback(const uint8_t sample, void *user_data) {
    (void)user_data;
    audio_push_sample(sample);
}

// Get apple2e_desc_t struct based on joystick type
apple2e_desc_t apple2e_desc(void) {
    return (apple2e_desc_t){
        .fdc_enabled = false,
        .hdc_enabled = true,
        .hdc_internal_flash = false,
        .audio =
            {
                .callback = {.func = audio_callback},
                .sample_rate = 44100,
            },
        .roms =
            {
                .rom = {.ptr = apple2e_rom, .size = sizeof(apple2e_rom)},
                .character_rom = {.ptr = apple2e_character_rom, .size = sizeof(apple2e_character_rom)},
                .fdc_rom = {.ptr = apple2_fdc_rom, .size = sizeof(apple2_fdc_rom)},
                .hdc_rom = {.ptr = prodos_hdc_rom, .size = sizeof(prodos_hdc_rom)},
            },
    };
}

void app_init(void) {
    apple2e_desc_t desc = apple2e_desc();
    apple2e_init(&state.apple2e, &desc);
}

#ifdef OLIMEX_NEO6502
// TMDS bit clock 400 MHz
// DVDD 1.3V
#define FRAME_WIDTH  800
#define FRAME_HEIGHT 600
#define VREG_VSEL    VREG_VOLTAGE_1_30
#define DVI_TIMING   dvi_timing_800x600p_60hz
#else
// TMDS bit clock 400 MHz
// DVDD 1.3V
#define FRAME_WIDTH  800
#define FRAME_HEIGHT 600
#define VREG_VSEL    VREG_VOLTAGE_1_30
#define DVI_TIMING   dvi_timing_800x600p_60hz
#endif  // OLIMEX_NEO6502

uint32_t __not_in_flash() tmds_palette[PALETTE_SIZE * 6];
uint32_t __not_in_flash() empty_tmdsbuf[3 * FRAME_WIDTH / DVI_SYMBOLS_PER_WORD];

uint8_t __not_in_flash() scanbuf[FRAME_WIDTH];

struct dvi_inst dvi0;

void tmds_palette_init() { tmds_setup_palette24_symbols(apple2e_palette, tmds_palette, PALETTE_SIZE); }

void kbd_raw_key_down(int code) {
    if (isascii(code)) {
        if (isupper(code)) {
            code = tolower(code);
        } else if (islower(code)) {
            code = toupper(code);
        }
    }

    if (code == 0x14F) {
        // Arrow right
        code = 0x15;
    } else if (code == 0x150) {
        // Arrow left
        code = 0x08;
    } else if (code == 0x151) {
        // Arrow down
        code = 0x0A;
    } else if (code == 0x152) {
        // Arrow up
        code = 0x0B;
    }

    apple2e_t *sys = &state.apple2e;

    switch (code) {
        case 0x13A:  // F1
        case 0x13B:  // F2
        case 0x13C:  // F3
        case 0x13D:  // F4
        case 0x13E:  // F5
        case 0x13F:  // F6
        case 0x140:  // F7
        case 0x141:  // F8
        case 0x142:  // F9
        {
            if (sys->fdc.valid) {
                uint8_t index = code - 0x13A;
                if (CHIPS_ARRAY_SIZE(apple2_nib_images) > index) {
                    disk2_fdd_insert_disk(&sys->fdc.fdd[0], apple2_nib_images[index]);
                }
            }
            break;
        }

        case 0x145:  // F12
            apple2e_reset(sys);
            break;

        case 0x1E3:  // GUI LEFT
            sys->kbd_open_apple_pressed = true;
            break;

        case 0x1E7:  // GUI RIGHT
            sys->kbd_solid_apple_pressed = true;
            break;

        default:
            if (code < 128) {
                sys->kbd_last_key = code | 0x80;
            }
            break;
    }
    // printf("Key down: %d\n", code);
}

void kbd_raw_key_up(int code) {
    if (isascii(code)) {
        if (isupper(code)) {
            code = tolower(code);
        } else if (islower(code)) {
            code = toupper(code);
        }
    }

    apple2e_t *sys = &state.apple2e;

    switch (code) {
        case 0x1E3:  // GUI LEFT
            sys->kbd_open_apple_pressed = false;
            break;

        case 0x1E7:  // GUI RIGHT
            sys->kbd_solid_apple_pressed = false;
            break;

        default:
            break;
    }
    // printf("Key up: %d\n", code);
}

void gamepad_state_update(uint8_t index, uint8_t hat_state, uint32_t button_state) {
    apple2e_t *sys = &state.apple2e;

    sys->paddl0 = 0x80;
    sys->paddl1 = 0x80;
    sys->paddl2 = 0x80;
    sys->paddl3 = 0x80;

    switch (hat_state) {

        case GAMEPAD_HAT_CENTERED:
            break;

        case GAMEPAD_HAT_UP:
            if (index == 0) {
                sys->paddl1 = 0x00;
            } else {
                sys->paddl3 = 0x00;
            }
            break;

        case GAMEPAD_HAT_UP_RIGHT:
            if (index == 0) {
                sys->paddl0 = 0xFF;
                sys->paddl1 = 0x00;
            } else {
                sys->paddl2 = 0xFF;
                sys->paddl3 = 0x00;
            }
            break;

        case GAMEPAD_HAT_RIGHT:
            if (index == 0) {
                sys->paddl0 = 0xFF;
            } else {
                sys->paddl2 = 0xFF;
            }
            break;

        case GAMEPAD_HAT_DOWN_RIGHT:
            if (index == 0) {
                sys->paddl0 = 0xFF;
                sys->paddl1 = 0xFF;
            } else {
                sys->paddl2 = 0xFF;
                sys->paddl3 = 0xFF;
            }
            break;

        case GAMEPAD_HAT_DOWN:
            if (index == 0) {
                sys->paddl1 = 0xFF;
            } else {
                sys->paddl3 = 0xFF;
            }
            break;

        case GAMEPAD_HAT_DOWN_LEFT:
            if (index == 0) {
                sys->paddl0 = 0x00;
                sys->paddl1 = 0xFF;
            } else {
                sys->paddl2 = 0x00;
                sys->paddl3 = 0xFF;
            }
            break;

        case GAMEPAD_HAT_LEFT:
            if (index == 0) {
                sys->paddl0 = 0x00;
            } else {
                sys->paddl2 = 0x00;
            }
            break;

        case GAMEPAD_HAT_UP_LEFT:
            if (index == 0) {
                sys->paddl0 = 0x00;
                sys->paddl1 = 0x00;
            } else {
                sys->paddl2 = 0x00;
                sys->paddl3 = 0x00;
            }
            break;

        default:
            break;
    }

    sys->butn0 = false;
    sys->butn1 = false;
    sys->butn2 = false;

    if (button_state & GAMEPAD_BUTTON_A) {
        if (index == 0) {
            sys->butn0 = true;
        } else {
            sys->butn2 = true;
        }
    }
    if (button_state & GAMEPAD_BUTTON_B) {
        if (index == 0) {
            sys->butn1 = true;
        }
    }
    // printf("Gamepad state update: %d %d %d\n", index, hat_state, button_state);
}

extern void apple2e_render_scanline(const uint32_t *pixbuf, uint32_t *scanbuf, size_t n_pix);
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

    apple2e_render_scanline(pixbuf, scanbuf, n_pix);
}

#define APPLE2E_EMPTY_LINES   ((FRAME_HEIGHT - APPLE2E_SCREEN_HEIGHT * 2) / 4)
#define APPLE2E_EMPTY_COLUMNS ((FRAME_WIDTH - APPLE2E_SCREEN_WIDTH) / 2)

static inline void __not_in_flash_func(render_empty_scanlines)() {
    for (int y = 0; y < APPLE2E_EMPTY_LINES; y += 2) {
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
    for (int y = 0; y < APPLE2E_SCREEN_HEIGHT; y += 2) {
        uint32_t *tmdsbuf;
        queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmdsbuf);
        render_scanline((const uint32_t *)(&state.apple2e.fb[y * 280]), (uint32_t *)(&scanbuf[APPLE2E_EMPTY_COLUMNS]),
                        280);
        tmds_encode_palette_data((const uint32_t *)scanbuf, tmds_palette, tmdsbuf, FRAME_WIDTH, PALETTE_BITS);
        queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmdsbuf);

        queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmdsbuf);
        render_scanline((const uint32_t *)(&state.apple2e.fb[(y + 1) * 280]),
                        (uint32_t *)(&scanbuf[APPLE2E_EMPTY_COLUMNS]), 280);
        tmds_encode_palette_data((const uint32_t *)scanbuf, tmds_palette, tmdsbuf, FRAME_WIDTH, PALETTE_BITS);
        queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmdsbuf);
    }
}

void __not_in_flash_func(core1_main()) {
    audio_init(_AUDIO_PIN, 44100);

    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    dvi_start(&dvi0);

    while (1) {
        render_empty_scanlines();
        render_frame();
        render_empty_scanlines();
    }

    __builtin_unreachable();
}

extern bool msc_inquiry_complete;

void wait_for_msc_ready(void) {
    while (!msc_inquiry_complete) {
        sleep_us(16666);
        tuh_task();
    }
}

int main() {
    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

    stdio_init_all();
    tusb_init();

    printf("Configuring DVI\n");

    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    tmds_palette_init();
    tmds_encode_palette_data((const uint32_t *)scanbuf, tmds_palette, empty_tmdsbuf, FRAME_WIDTH, PALETTE_BITS);

    printf("Core 1 start\n");
    hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);
    multicore_launch_core1(core1_main);

    wait_for_msc_ready();

    app_init();

    while (1) {
        uint32_t start_time_in_micros = time_us_32();

        uint32_t num_ticks = 17030;
        for (uint32_t ticks = 0; ticks < num_ticks; ticks++) {
            apple2e_tick(&state.apple2e);
        }

        apple2e_screen_update(&state.apple2e);
        tuh_task();

        uint32_t end_time_in_micros = time_us_32();
        uint32_t execution_time = end_time_in_micros - start_time_in_micros;
        // printf("%d us\n", execution_time);

        int sleep_time = 16666 - execution_time;
        if (sleep_time > 0) {
            sleep_us(sleep_time);
        }
    }

    __builtin_unreachable();
}
