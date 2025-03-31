// oric.c
//
// ## zlib/libpng license
//
// Copyright (c) 2025 Veselin Sladkov
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

#define __in_flash()
#define __not_in_flash()

#define RGBA8(b, g, r) (0xFF000000 | (r << 16) | (g << 8) | (b))

#include <stdlib.h>

#include "roms/oric_roms.h"
#include "images/oric_images.h"

#include "chips/chips_common.h"
#include "common.h"
#include "chips/mos6502cpu.h"
#include "chips/mos6522via.h"
#include "chips/ay38910psg.h"
#include "chips/beeper.h"
#include "chips/kbd.h"
#include "chips/mem.h"
#include "chips/clk.h"
#include "devices/oric_td.h"
#include "devices/disk2_fdd.h"
#include "devices/disk2_fdc.h"
#include "devices/oric_fdc_rom.h"
#include "systems/oric.h"

typedef struct {
    uint32_t version;
    oric_t oric;
} oric_snapshot_t;

typedef struct {
    oric_t oric;
    uint32_t frame_time_us;
    uint32_t ticks;
    double emu_time_ms;
} state_t;

static state_t state;

#define BORDER_TOP (8)
#define BORDER_LEFT (8)
#define BORDER_RIGHT (8)
#define BORDER_BOTTOM (16)

// Audio streaming callback
static void audio_callback(const uint8_t sample, void *user_data) {
    (void)user_data;

    static float samples[1024];
    static int sample_index = 0;
    samples[sample_index++] = (float)sample / 255.0f;
    if (sample_index == 1024) {
        saudio_push(samples, 1024);
        sample_index = 0;
    }
}

// Get oric_desc_t struct based on configuration
oric_desc_t oric_desc(void) {
    return (oric_desc_t){
        .td_enabled = true,  // Enable tape drive
        .fdc_enabled = true, // Enable floppy disk controller
        .audio =
            {
                .callback = {.func = audio_callback},
                .sample_rate = 44100,
            },
        .roms =
            {
                .rom = {.ptr = oric_rom, .size = sizeof(oric_rom)},
                .boot_rom = {.ptr = oric_fdc_rom, .size = sizeof(oric_fdc_rom)},
            },
    };
}

// original_size * 2 (byte per pixel) = original_size * 2
static uint8_t oric_frame_buffer[ORIC_FRAMEBUFFER_SIZE * 2];

static void oric_update_frame_buffer(oric_t* sys) {
    const int src_bytes_per_row = ORIC_SCREEN_WIDTH / 2;
    const int dst_bytes_per_row = ORIC_SCREEN_WIDTH;
    
    for (int row = 0; row < ORIC_SCREEN_HEIGHT; row++) {
        const uint8_t* src_row = &sys->fb[row * src_bytes_per_row];
        uint8_t* dst_row = &oric_frame_buffer[row * dst_bytes_per_row];
        
        for (int col = 0; col < src_bytes_per_row; col++) {
            uint8_t pixel = src_row[col];
            dst_row[col * 2] = (pixel >> 4) & 0x0F;
            dst_row[col * 2 + 1] = pixel & 0x0F;
        }
    }
}

chips_display_info_t oric_display_info(oric_t* sys) {
    const chips_display_info_t res = {
        .frame = {
            .dim = {
                .width = ORIC_SCREEN_WIDTH,
                .height = ORIC_SCREEN_HEIGHT,
            },
            .bytes_per_pixel = 1,
            .buffer = {
                .ptr = sys ? oric_frame_buffer : 0,
                .size = ORIC_FRAMEBUFFER_SIZE * 2,
            }
        },
        .screen = {
            .x = 0,
            .y = 0,
            .width = ORIC_SCREEN_WIDTH,
            .height = ORIC_SCREEN_HEIGHT,
        },
        .palette = {
            .ptr = sys ? oric_palette : 0,
            .size = sizeof(oric_palette),
        }
    };
    CHIPS_ASSERT(((sys == 0) && (res.frame.buffer.ptr == 0)) || ((sys != 0) && (res.frame.buffer.ptr != 0)));
    CHIPS_ASSERT(((sys == 0) && (res.palette.ptr == 0)) || ((sys != 0) && (res.palette.ptr != 0)));
    return res;
}

void app_init(void) {
    saudio_setup(&(saudio_desc){
        .logger.func = slog_func,
    });

    oric_desc_t desc = oric_desc();
    oric_init(&state.oric, &desc);
    gfx_init(&(gfx_desc_t){
        .disable_speaker_icon = sargs_exists("disable-speaker-icon"),
        .border = {
            .left = BORDER_LEFT,
            .right = BORDER_RIGHT,
            .top = BORDER_TOP,
            .bottom = BORDER_BOTTOM,
        },
        .display_info = oric_display_info(&state.oric),
    });
    clock_init();
    prof_init();
}

static void kbd_raw_key_down(int code);
static void kbd_raw_key_up(int code);
static void draw_status_bar(void);

void app_frame(void) {
    state.frame_time_us = clock_frame_time();
    const uint64_t emu_start_time = stm_now();
    state.ticks = oric_exec(&state.oric, state.frame_time_us);
    state.emu_time_ms = stm_ms(stm_since(emu_start_time));
    draw_status_bar();
    oric_update_frame_buffer(&state.oric);
    gfx_draw(oric_display_info(&state.oric));
}

void app_input(const sapp_event* event) {
    const bool shift = event->modifiers & SAPP_MODIFIER_SHIFT;
    const bool ctrl = event->modifiers & SAPP_MODIFIER_CTRL;
    switch (event->type) {
        int c;
        case SAPP_EVENTTYPE_CHAR:
            c = (int) event->char_code;
            if ((c > 0x20) && (c < 0x7F)) {
                // need to invert case (unshifted is upper caps, shifted is lower caps
                if (isupper(c)) {
                    c = tolower(c);
                } else if (islower(c)) {
                    c = toupper(c);
                }
                kbd_raw_key_down(c);
                kbd_raw_key_up(c);
            }
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
        case SAPP_EVENTTYPE_KEY_UP:
            switch (event->key_code) {
                case SAPP_KEYCODE_SPACE:        c = 0x20; break;
                case SAPP_KEYCODE_LEFT:         c = 0x08; break;  // Left arrow
                case SAPP_KEYCODE_RIGHT:        c = 0x15; break;  // Right arrow
                case SAPP_KEYCODE_DOWN:         c = 0x0A; break;  // Down arrow
                case SAPP_KEYCODE_UP:           c = 0x0B; break;  // Up arrow
                case SAPP_KEYCODE_ENTER:        c = 0x0D; break;  // Return
                case SAPP_KEYCODE_BACKSPACE:    c = 0x01; break;  // Delete
                case SAPP_KEYCODE_ESCAPE:       c = 0x1B; break;  // Escape
                case SAPP_KEYCODE_F1:           c = 0x13A; break;
                case SAPP_KEYCODE_F2:           c = 0x13B; break;
                case SAPP_KEYCODE_F3:           c = 0x13C; break;
                case SAPP_KEYCODE_F4:           c = 0x13D; break;
                case SAPP_KEYCODE_F5:           c = 0x13E; break;
                case SAPP_KEYCODE_F6:           c = 0x13F; break;
                case SAPP_KEYCODE_F7:           c = 0x140; break;
                case SAPP_KEYCODE_F8:           c = 0x141; break;
                case SAPP_KEYCODE_F9:           c = 0x142; break;
                case SAPP_KEYCODE_F10:          c = 0x143; break;
                case SAPP_KEYCODE_F11:          c = 0x144; break;
                case SAPP_KEYCODE_F12:          c = 0x145; break;
                default:                        c = 0; break;
            }
            if (c) {
                if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
                    kbd_raw_key_down(c);
                } else {
                    kbd_raw_key_up(c);
                }
            }
            break;
        default:
            break;
    }
}

void app_cleanup(void) {
    oric_discard(&state.oric);
    saudio_shutdown();
    gfx_shutdown();
    sargs_shutdown();
}

static void kbd_raw_key_down(int code) {
    if (isascii(code)) {
        if (isupper(code)) {
            code = tolower(code);
        } else if (islower(code)) {
            code = toupper(code);
        }
    }

    oric_t *sys = &state.oric;

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
            uint8_t index = code - 0x13A;
            int num_nib_images = CHIPS_ARRAY_SIZE(oric_nib_images);
            if (index < num_nib_images) {
                if (sys->fdc.valid) {
                    disk2_fdd_insert_disk(&sys->fdc.fdd[0], oric_nib_images[index]);
                }
            } else {
                index -= num_nib_images;
                if (index < CHIPS_ARRAY_SIZE(oric_wave_images)) {
                    if (sys->td.valid) {
                        oric_td_insert_tape(&sys->td, oric_wave_images[index]);
                    }
                }
            }
            break;
        }

        case 0x144:  // F11
            oric_nmi(sys);
            break;

        case 0x145:  // F12
            oric_reset(sys);
            break;

        default:
            kbd_key_down(&sys->kbd, code);
            break;
    }
}

static void kbd_raw_key_up(int code) {
    if (isascii(code)) {
        if (isupper(code)) {
            code = tolower(code);
        } else if (islower(code)) {
            code = toupper(code);
        }
    }

    kbd_key_up(&state.oric.kbd, code);
}

static void draw_status_bar(void) {
    prof_push(PROF_EMU, (float)state.emu_time_ms);
    prof_stats_t emu_stats = prof_stats(PROF_EMU);
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    sdtx_canvas(w, h);
    sdtx_color3b(255, 255, 255);
    sdtx_pos(1.0f, (h / 8.0f) - 1.5f);
    sdtx_printf("frame:%.2fms emu:%.2fms (min:%.2fms max:%.2fms) ticks:%d", (float)state.frame_time_us * 0.001f, emu_stats.avg_val, emu_stats.min_val, emu_stats.max_val, state.ticks);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    sargs_setup(&(sargs_desc){
        .argc=argc,
        .argv=argv,
        .buf_size = 512 * 1024,
    });
    const chips_display_info_t info = oric_display_info(0);
    return (sapp_desc) {
        .init_cb = app_init,
        .frame_cb = app_frame,
        .event_cb = app_input,
        .cleanup_cb = app_cleanup,
        .width = 4 * (info.screen.width + BORDER_LEFT + BORDER_RIGHT),
        .height = 4 * (info.screen.height + BORDER_TOP + BORDER_BOTTOM),
        .window_title = "Oric",
        .icon.sokol_default = true,
        .enable_dragndrop = true,
        .html5_bubble_mouse_events = true,
        .html5_update_document_title = true,
        .logger.func = slog_func,
    };
} 