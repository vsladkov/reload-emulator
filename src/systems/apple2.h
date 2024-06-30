#pragma once

// apple2.h
//
// An Apple ][ emulator in a C header.
//
// Do this:
// ~~~C
// #define CHIPS_IMPL
// ~~~
// before you include this file in *one* C or C++ file to create the
// implementation.
//
// Optionally provide the following macros with your own implementation
//
// ~~~C
// CHIPS_ASSERT(c)
// ~~~
//     your own assert macro (default: assert(c))
//
// You need to include the following headers before including apple2.h:
//
// - chips/chips_common.h
// - chips/wdc65C02cpu.h | chips/mos6502cpu.h
// - chips/beeper.h
// - chips/kbd.h
// - chips/mem.h
// - chips/clk.h
// - devices/apple2_lc.h
// - devices/disk2_fdd.h
// - devices/disk2_fdc.h
// - devices/apple2_fdc_rom.h
// - devices/prodos_hdd.h
// - devices/prodos_hdc.h
// - devices/prodos_hdc_rom.h
//
// ## The Apple ][
//
//
// TODO!
//
// ## Links
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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <stdio.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bump snapshot version when apple2_t memory layout changes
#define APPLE2_SNAPSHOT_VERSION (1)

#define APPLE2_FREQUENCY (1021800)

#define APPLE2_SCREEN_WIDTH     560  // (280 * 2)
#define APPLE2_SCREEN_HEIGHT    192  // (192)
#define APPLE2_FRAMEBUFFER_SIZE ((APPLE2_SCREEN_WIDTH / 2) * APPLE2_SCREEN_HEIGHT)

// Config parameters for apple2_init()
typedef struct {
    bool fdc_enabled;         // Set to true to enable floppy disk controller emulation
    bool hdc_enabled;         // Set to true to enable hard disk controller emulation
    bool hdc_internal_flash;  // Set to true to use internal flash
    chips_debug_t debug;      // Optional debugging hook
    chips_audio_desc_t audio;
    struct {
        chips_range_t rom;
        chips_range_t character_rom;
        chips_range_t fdc_rom;
        chips_range_t hdc_rom;
    } roms;
} apple2_desc_t;

// Apple II emulator state
typedef struct {
    MOS6502CPU_T cpu;
    beeper_t beeper;
    kbd_t kbd;
    mem_t mem;
    bool valid;
    chips_debug_t debug;

    chips_audio_callback_t audio_callback;

    uint8_t ram[0xC000];
    uint8_t *rom;
    uint8_t *character_rom;
    uint8_t *fdc_rom;
    uint8_t *hdc_rom;

    apple2_lc_t lc;  // Language card

    bool text;
    bool mixed;
    bool page2;
    bool hires;

    bool flash;
    uint32_t flash_timer_ticks;

    bool text_page1_dirty;
    bool text_page2_dirty;
    bool hires_page1_dirty;
    bool hires_page2_dirty;

    uint8_t fb[APPLE2_FRAMEBUFFER_SIZE];

    disk2_fdc_t fdc;  // Disk II floppy disk controller

    prodos_hdc_t hdc;  // ProDOS hard disk controller

    uint8_t kbd_last_key;

    uint8_t paddl0;
    uint8_t paddl1;
    uint8_t paddl2;
    uint8_t paddl3;

    uint16_t paddl0_ticks_left;
    uint16_t paddl1_ticks_left;
    uint16_t paddl2_ticks_left;
    uint16_t paddl3_ticks_left;

    bool butn0;
    bool butn1;
    bool butn2;

    uint32_t system_ticks;
} apple2_t;

// Apple2 interface

// Initialize a new Apple2 instance
void apple2_init(apple2_t *sys, const apple2_desc_t *desc);
// Discard Apple2 instance
void apple2_discard(apple2_t *sys);
// Reset a Apple2 instance
void apple2_reset(apple2_t *sys);

void apple2_tick(apple2_t *sys);

// Tick Apple2 instance for a given number of microseconds, return number of executed ticks
uint32_t apple2_exec(apple2_t *sys, uint32_t micro_seconds);
// Take a snapshot, patches pointers to zero or offsets, returns snapshot version
uint32_t apple2_save_snapshot(apple2_t *sys, apple2_t *dst);
// Load a snapshot, returns false if snapshot version doesn't match
bool apple2_load_snapshot(apple2_t *sys, uint32_t version, apple2_t *src);

void apple2_screen_update(apple2_t *sys);

#ifdef __cplusplus
}  // extern "C"
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
#include <assert.h>
#define CHIPS_ASSERT(c) assert(c)
#endif

static void _apple2_init_memorymap(apple2_t *sys);

// clang-format off
static uint8_t __not_in_flash() _apple2_artifact_color_lut[1<<7] = {
	0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00, 0x11, 0x11, 0x55, 0x11, 0x99, 0x99, 0xDD, 0xFF,
	0x22, 0x22, 0x66, 0x66, 0xAA, 0xAA, 0xEE, 0xEE, 0x33, 0x33, 0x33, 0x33, 0xBB, 0xBB, 0xFF, 0xFF,
	0x00, 0x00, 0x44, 0x44, 0xCC, 0xCC, 0xCC, 0xCC, 0x55, 0x55, 0x55, 0x55, 0x99, 0x99, 0xDD, 0xFF,
	0x00, 0x22, 0x66, 0x66, 0xEE, 0xAA, 0xEE, 0xEE, 0x77, 0x77, 0x77, 0x77, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x88, 0x11, 0x11, 0x55, 0x11, 0x99, 0x99, 0xDD, 0xFF,
	0x00, 0x22, 0x66, 0x66, 0xAA, 0xAA, 0xAA, 0xAA, 0x33, 0x33, 0x33, 0x33, 0xBB, 0xBB, 0xFF, 0xFF,
	0x00, 0x00, 0x44, 0x44, 0xCC, 0xCC, 0xCC, 0xCC, 0x11, 0x11, 0x55, 0x55, 0x99, 0x99, 0xDD, 0xDD,
	0x00, 0x22, 0x66, 0x66, 0xEE, 0xAA, 0xEE, 0xEE, 0xFF, 0xFF, 0xFF, 0x77, 0xFF, 0xFF, 0xFF, 0xFF
};
// clang-format on

extern bool msc_inquiry_complete;

static inline uint32_t _apple2_rotl4b(uint32_t n, uint32_t count) { return (n >> (-count & 3)) & 0xF; }

static uint16_t __not_in_flash() _apple2_double_7_bits_lut[128];

static void _apple2_init_double_7_bits_lut() {
    for (uint8_t bits = 0; bits < 128; ++bits) {
        uint16_t result = 0;
        for (int i = 6; i >= 0; i--) {
            result <<= 1;
            uint8_t bit = bits & (1 << i) ? 1 : 0;
            result |= bit;
            result <<= 1;
            result |= bit;
        }
        _apple2_double_7_bits_lut[bits] = result;
    }
}

static inline uint16_t _apple2_double_7_bits(uint8_t bits) { return _apple2_double_7_bits_lut[bits]; }

static inline uint8_t _apple2_reverse_7_bits(uint8_t bits) {
    uint8_t result = 0;
    for (int i = 0; i < 7; i++) {
        result <<= 1;
        result |= bits & 1;
        bits >>= 1;
    }
    return result;
}

void apple2_init(apple2_t *sys, const apple2_desc_t *desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) {
        CHIPS_ASSERT(desc->debug.stopped);
    }

    _apple2_init_double_7_bits_lut();

    memset(sys, 0, sizeof(apple2_t));
    sys->valid = true;
    sys->debug = desc->debug;
    sys->audio_callback = desc->audio.callback;

    CHIPS_ASSERT(desc->roms.rom.ptr && (desc->roms.rom.size == 0x4000));
    CHIPS_ASSERT(desc->roms.character_rom.ptr && (desc->roms.character_rom.size == 0x800));
    CHIPS_ASSERT(desc->roms.fdc_rom.ptr && (desc->roms.fdc_rom.size == 0x100));
    CHIPS_ASSERT(desc->roms.hdc_rom.ptr && (desc->roms.hdc_rom.size == 0x100));
    sys->rom = desc->roms.rom.ptr;
    sys->character_rom = desc->roms.character_rom.ptr;
    sys->fdc_rom = desc->roms.fdc_rom.ptr;
    sys->hdc_rom = desc->roms.hdc_rom.ptr;

    MOS6502CPU_INIT(&sys->cpu, &(MOS6502CPU_DESC_T){0});

    beeper_init(&sys->beeper, &(beeper_desc_t){
                                  .tick_hz = APPLE2_FREQUENCY,
                                  .sound_hz = CHIPS_DEFAULT(desc->audio.sample_rate, 44100),
                                  .base_volume = CHIPS_DEFAULT(desc->audio.volume, 1.0f),
                              });

    // setup memory map and keyboard matrix
    _apple2_init_memorymap(sys);

    apple2_lc_init(&sys->lc, &(apple2_lc_desc_t){&sys->mem, sys->rom});

    sys->flash_timer_ticks = APPLE2_FREQUENCY / 2;

    sys->kbd_last_key = 0x0D | 0x80;

    sys->paddl0 = 0x80;
    sys->paddl1 = 0x80;
    sys->paddl2 = 0x80;
    sys->paddl3 = 0x80;

    // Optionally setup floppy disk controller
    if (desc->fdc_enabled) {
        disk2_fdc_init(&sys->fdc);
        if (CHIPS_ARRAY_SIZE(apple2_nib_images) > 0) {
            disk2_fdd_insert_disk(&sys->fdc.fdd[0], apple2_nib_images[0]);
        }
    }

    // Optionally setup hard disk controller
    if (desc->hdc_enabled) {
        prodos_hdc_init(&sys->hdc);
        if (desc->hdc_internal_flash) {
            if (CHIPS_ARRAY_SIZE(apple2_po_images) > 0) {
                prodos_hdd_insert_disk_internal(&sys->hdc.hdd[0], apple2_po_images[0], apple2_po_image_sizes[0]);
            }
        } else {
            while (!msc_inquiry_complete) {
                sleep_us(16666);
                tuh_task();
            }
            if (CHIPS_ARRAY_SIZE(apple2_msc_images) > 0) {
                prodos_hdd_insert_disk_msc(&sys->hdc.hdd[0], apple2_msc_images[0]);
            }
        }
    }
}

void apple2_discard(apple2_t *sys) {
    CHIPS_ASSERT(sys && sys->valid);
    apple2_lc_discard(&sys->lc);
    if (sys->fdc.valid) {
        disk2_fdc_discard(&sys->fdc);
    }
    if (sys->hdc.valid) {
        prodos_hdc_discard(&sys->hdc);
    }
    sys->valid = false;
}

void apple2_reset(apple2_t *sys) {
    CHIPS_ASSERT(sys && sys->valid);
    apple2_lc_reset(&sys->lc);
    beeper_reset(&sys->beeper);
    if (sys->fdc.valid) {
        disk2_fdc_reset(&sys->fdc);
    }
    if (sys->hdc.valid) {
        prodos_hdc_reset(&sys->hdc);
    }
    MOS6502CPU_RESET(&sys->cpu);
}

static void _apple2_mem_c000_c0ff_rw(apple2_t *sys, uint16_t addr, bool rw) {
    switch (addr & 0xFF) {
        case 0x00:
            if (rw) {
                if (sys->kbd_last_key != 0) {
                    MOS6502CPU_SET_DATA(&sys->cpu, sys->kbd_last_key);
                }
            }
            break;

        case 0x10:
            sys->kbd_last_key &= 0x7F;
            break;

        case 0x30:
            beeper_toggle(&sys->beeper);
            break;

        case 0x50:
            sys->text = false;
            break;

        case 0x51:
            sys->text = true;
            break;

        case 0x52:
            sys->mixed = false;
            break;

        case 0x53:
            sys->mixed = true;
            break;

        case 0x54:
            sys->page2 = false;
            break;

        case 0x55:
            sys->page2 = true;
            break;

        case 0x56:
            sys->hires = false;
            break;

        case 0x57:
            sys->hires = true;
            break;

        case 0x61:  // Button 0
        case 0x69:
            if (rw) {
                MOS6502CPU_SET_DATA(&sys->cpu, sys->butn0 ? 0x80 : 00);
            }
            break;

        case 0x62:  // Botton 1
        case 0x6A:
            if (rw) {
                MOS6502CPU_SET_DATA(&sys->cpu, sys->butn1 ? 0x80 : 00);
            }
            break;

        case 0x63:  // Botton 2
        case 0x6B:
            if (rw) {
                MOS6502CPU_SET_DATA(&sys->cpu, sys->butn2 ? 0x80 : 00);
            }
            break;

        case 0x64:  // Joystick 1 X axis
        case 0x6C:
            if (rw) {
                MOS6502CPU_SET_DATA(&sys->cpu, sys->paddl0_ticks_left > 0 ? 0x80 : 00);
            }
            break;

        case 0x65:  // Joystick 1 Y axis
        case 0x6D:
            if (rw) {
                MOS6502CPU_SET_DATA(&sys->cpu, sys->paddl1_ticks_left > 0 ? 0x80 : 00);
            }
            break;

        case 0x66:  // Joystick 2 X axis
        case 0x6E:
            if (rw) {
                MOS6502CPU_SET_DATA(&sys->cpu, sys->paddl2_ticks_left > 0 ? 0x80 : 00);
            }
            break;

        case 0x67:  // Joystick 2 Y axis
        case 0x6F:
            if (rw) {
                MOS6502CPU_SET_DATA(&sys->cpu, sys->paddl3_ticks_left > 0 ? 0x80 : 00);
            }
            break;

        default:
            if ((addr >= 0xC070) && (addr <= 0xC07F)) {
                // Joystick
                if (sys->paddl0_ticks_left == 0) {
                    sys->paddl0_ticks_left = sys->paddl0 * 11;
                }
                if (sys->paddl1_ticks_left == 0) {
                    sys->paddl1_ticks_left = sys->paddl1 * 11;
                }
                if (sys->paddl2_ticks_left == 0) {
                    sys->paddl2_ticks_left = sys->paddl2 * 11;
                }
                if (sys->paddl3_ticks_left == 0) {
                    sys->paddl3_ticks_left = sys->paddl3 * 11;
                }
            } else if ((addr >= 0xC080) && (addr <= 0xC08F)) {
                // Apple II 16K Language Card
                apple2_lc_control(&sys->lc, addr & 0xF, rw);
                if (rw) {
                    MOS6502CPU_SET_DATA(&sys->cpu, 0xFF);
                }
            } else if ((addr >= 0xC0E0) && (addr <= 0xC0EF)) {
                // Disk II FDC
                if (sys->fdc.valid) {
                    if (rw) {
                        // Memory read
                        MOS6502CPU_SET_DATA(&sys->cpu, disk2_fdc_read_byte(&sys->fdc, addr & 0xF));
                    } else {
                        // Memory write
                        disk2_fdc_write_byte(&sys->fdc, addr & 0xF, MOS6502CPU_GET_DATA(&sys->cpu));
                    }
                } else {
                    if (rw) {
                        MOS6502CPU_SET_DATA(&sys->cpu, 0x00);
                    }
                }
            } else if ((addr >= 0xC0F0) && (addr <= 0xC0FF)) {
                // ProDOS HDC
                if (sys->hdc.valid) {
                    if (rw) {
                        // Memory read
                        MOS6502CPU_SET_DATA(&sys->cpu, prodos_hdc_read_byte(&sys->hdc, addr & 0xF));
                    } else {
                        // Memory write
                        prodos_hdc_write_byte(&sys->hdc, addr & 0xF, MOS6502CPU_GET_DATA(&sys->cpu), &sys->mem);
                    }
                } else {
                    if (rw) {
                        MOS6502CPU_SET_DATA(&sys->cpu, 0x00);
                    }
                }
            }
            break;
    }
}

static void _apple2_mem_rw(apple2_t *sys, uint16_t addr, bool rw) {
    if ((addr >= 0xC000) && (addr <= 0xC0FF)) {
        // Apple II I/O Page
        _apple2_mem_c000_c0ff_rw(sys, addr, rw);
    } else if ((addr >= 0xC600) && (addr <= 0xC6FF)) {
        // Disk II boot rom
        if (rw) {
            // Memory read
            MOS6502CPU_SET_DATA(&sys->cpu, sys->fdc.valid ? sys->fdc_rom[addr & 0xFF] : 0x00);
        }
    } else if ((addr >= 0xC700) && (addr <= 0xC7FF)) {
        // Hard disk boot rom
        if (rw) {
            // Memory read
            MOS6502CPU_SET_DATA(&sys->cpu, sys->hdc.valid ? sys->hdc_rom[addr & 0xFF] : 0x00);
        }
    } else {
        // Regular memory access
        if (rw) {
            // Memory read
            MOS6502CPU_SET_DATA(&sys->cpu, mem_rd(&sys->mem, addr));
        } else {
            // Memory write
            mem_wr(&sys->mem, addr, MOS6502CPU_GET_DATA(&sys->cpu));
            if (addr >= 0x400 && addr <= 0x7FF) {
                sys->text_page1_dirty = true;
            } else if (addr >= 0x800 && addr <= 0xBFF) {
                sys->text_page2_dirty = true;
            } else if (addr >= 0x2000 && addr <= 0x3FFF) {
                sys->hires_page1_dirty = true;
            } else if (addr >= 0x4000 && addr <= 0x5FFF) {
                sys->hires_page2_dirty = true;
            }
        }
    }
}

void apple2_tick(apple2_t *sys) {
    if (sys->paddl0_ticks_left > 0) {
        sys->paddl0_ticks_left--;
    }
    if (sys->paddl1_ticks_left > 0) {
        sys->paddl1_ticks_left--;
    }
    if (sys->paddl2_ticks_left > 0) {
        sys->paddl2_ticks_left--;
    }
    if (sys->paddl3_ticks_left > 0) {
        sys->paddl3_ticks_left--;
    }

    MOS6502CPU_TICK(&sys->cpu);

    _apple2_mem_rw(sys, sys->cpu.addr, sys->cpu.rw);

    // Update beeper
    if (beeper_tick(&sys->beeper)) {
        if (sys->audio_callback.func) {
            // New sample is ready
            sys->audio_callback.func((uint8_t)(sys->beeper.sample * 255.0f), sys->audio_callback.user_data);
        }
    }

    // Tick FDC
    if (sys->fdc.valid && (sys->system_ticks & 127) == 0) {
        disk2_fdc_tick(&sys->fdc);
    }

    if (sys->flash_timer_ticks > 0) {
        sys->flash_timer_ticks--;
        if (sys->flash_timer_ticks == 0) {
            sys->flash = !sys->flash;
            sys->flash_timer_ticks = APPLE2_FREQUENCY / 2;
            if (!sys->page2) {
                sys->text_page1_dirty = true;
            } else {
                sys->text_page2_dirty = true;
            }
        }
    }

    sys->system_ticks++;
}

uint32_t apple2_exec(apple2_t *sys, uint32_t micro_seconds) {
    CHIPS_ASSERT(sys && sys->valid);
    uint32_t num_ticks = clk_us_to_ticks(APPLE2_FREQUENCY, micro_seconds);
    // uint32_t num_ticks = 50;
    if (0 == sys->debug.callback.func) {
        // run without debug callback
        for (uint32_t ticks = 0; ticks < num_ticks; ticks++) {
            apple2_tick(sys);
        }
    } else {
        // run with debug callback
        for (uint32_t ticks = 0; (ticks < num_ticks) && !(*sys->debug.stopped); ticks++) {
            apple2_tick(sys);
            sys->debug.callback.func(sys->debug.callback.user_data, 0);
        }
    }
    // kbd_update(&sys->kbd, micro_seconds);
    apple2_screen_update(sys);

    // printf("executed %d ticks\n", num_ticks);
    return num_ticks;
}

static void _apple2_init_memorymap(apple2_t *sys) {
    mem_init(&sys->mem);
    for (int addr = 0; addr < 0xC000; addr += 2) {
        sys->ram[addr] = 0;
        sys->ram[addr + 1] = 0xFF;
    }
    mem_map_ram(&sys->mem, 0, 0x0000, 0xC000, sys->ram);
}

uint32_t apple2_save_snapshot(apple2_t *sys, apple2_t *dst) {
    CHIPS_ASSERT(sys && dst);
    *dst = *sys;
    chips_debug_snapshot_onsave(&dst->debug);
    chips_audio_callback_snapshot_onsave(&dst->audio_callback);
    // m6502_snapshot_onsave(&dst->cpu);
    disk2_fdc_snapshot_onsave(&dst->fdc);
    mem_snapshot_onsave(&dst->mem, sys);
    return APPLE2_SNAPSHOT_VERSION;
}

bool apple2_load_snapshot(apple2_t *sys, uint32_t version, apple2_t *src) {
    CHIPS_ASSERT(sys && src);
    if (version != APPLE2_SNAPSHOT_VERSION) {
        return false;
    }
    static apple2_t im;
    im = *src;
    chips_debug_snapshot_onload(&im.debug, &sys->debug);
    chips_audio_callback_snapshot_onload(&im.audio_callback, &sys->audio_callback);
    // m6502_snapshot_onload(&im.cpu, &sys->cpu);
    disk2_fdc_snapshot_onload(&im.fdc, &sys->fdc);
    mem_snapshot_onload(&im.mem, sys);
    *sys = im;
    return true;
}

static void _apple2_render_line_monochrome(uint8_t *out, uint16_t *in, int start_col, int stop_col) {
    uint32_t w = in[start_col];

    for (int col = start_col; col < stop_col; col++) {
        if (col + 1 < 40) {
            w += in[col + 1] << 14;
        }

        for (int b = 0; b < 7; b++) {
            uint8_t c1 = (w & 1) ? 15 : 0;
            w >>= 1;
            uint8_t c2 = (w & 1) ? 15 : 0;
            w >>= 1;
            out[col * 7 + b] = (c1 << 4) | (c2 & 0xF);
        }
    }
}

static void _apple2_render_line_color(uint8_t *out, uint16_t *in, int start_col, int stop_col) {
    uint32_t w = in[start_col] << 3;

    for (int col = start_col; col < stop_col; col++) {
        if (col + 1 < 40) {
            w += in[col + 1] << 17;
        }

        for (int b = 0; b < 7; b++) {
            uint8_t c1 = _apple2_rotl4b(_apple2_artifact_color_lut[w & 0x7F], col * 14 + b * 2);
            w >>= 1;
            uint8_t c2 = _apple2_rotl4b(_apple2_artifact_color_lut[w & 0x7F], col * 14 + b * 2 + 1);
            w >>= 1;
            out[col * 7 + b] = (c1 << 4) | (c2 & 0x0F);
        }
    }
}

static uint8_t _apple2_get_text_character(apple2_t *sys, uint8_t code, uint16_t row) {
    uint8_t invert_mask = 0;

    if ((code >= 0x40) && (code <= 0x7F)) {
        if (sys->flash) {
            invert_mask ^= 0x7F;
        }
    } else if (code < 0x40)  // inverse: flip FG and BG
    {
        invert_mask ^= 0x7F;
    }

    /* look up the character data */
    uint8_t bits = sys->character_rom[code * 8 + row];
    bits = bits & 0x7F;
    bits ^= invert_mask;
    return _apple2_reverse_7_bits(bits);
}

static uint8_t *_apple2_get_fb_addr(apple2_t *sys, uint16_t row) { return &sys->fb[row * (APPLE2_SCREEN_WIDTH / 2)]; }

static void _apple2_lores_update(apple2_t *sys, uint16_t begin_row, uint16_t end_row) {
    if ((!sys->page2 && !sys->text_page1_dirty) || (sys->page2 && !sys->text_page2_dirty)) {
        return;
    }

    uint16_t start_address = sys->page2 ? 0x0800 : 0x0400;

    uint16_t start_row = (begin_row / 8) * 8;
    uint16_t stop_row = ((end_row / 8) + 1) * 8;

    for (int row = start_row; row < stop_row; row += 4) {
        uint16_t address = start_address + ((((row / 8) & 0x07) << 7) | (((row / 8) & 0x18) * 5));
        uint8_t *vram_row = &sys->ram[address];

#define NIBBLE(byte) (((byte) >> (row & 4)) & 0x0F)
        uint8_t *p = _apple2_get_fb_addr(sys, row);

        for (int col = 0; col < 40; col++) {
            uint8_t c = NIBBLE(vram_row[col]);
            for (int b = 0; b < 7; b++) {
                *p = c << 4;
                *p++ |= c;
            }
        }
#undef NIBBLE

        for (int y = 1; y < 4; y++) {
            memcpy(_apple2_get_fb_addr(sys, row + y), _apple2_get_fb_addr(sys, row), 40 * 7);
        }
    }

    if (!sys->page2) {
        sys->text_page1_dirty = false;
    } else {
        sys->text_page2_dirty = false;
    }
}

static void _apple2_text_update(apple2_t *sys, uint16_t begin_row, uint16_t end_row) {
    if ((!sys->page2 && !sys->text_page1_dirty) || (sys->page2 && !sys->text_page2_dirty)) {
        return;
    }

    uint16_t start_address = sys->page2 ? 0x0800 : 0x0400;

    uint16_t start_row = (begin_row / 8) * 8;
    uint16_t stop_row = ((end_row / 8) + 1) * 8;

    for (int row = start_row; row < stop_row; row++) {
        uint16_t address = start_address + ((((row / 8) & 0x07) << 7) | (((row / 8) & 0x18) * 5));
        uint8_t *vram_row = &sys->ram[address];

        uint16_t words[40];

        for (int col = 0; col < 40; col++) {
            words[col] = _apple2_double_7_bits(_apple2_get_text_character(sys, vram_row[col], row & 7));
        }

        _apple2_render_line_monochrome(_apple2_get_fb_addr(sys, row), words, 0, 40);
    }

    if (!sys->page2) {
        sys->text_page1_dirty = false;
    } else {
        sys->text_page2_dirty = false;
    }
}

static void _apple2_hgr_update(apple2_t *sys, uint16_t begin_row, uint16_t end_row) {
    if ((!sys->page2 && !sys->hires_page1_dirty) || (sys->page2 && !sys->hires_page2_dirty)) {
        return;
    }

    uint16_t start_address = sys->page2 ? 0x4000 : 0x2000;

    for (int row = begin_row; row <= end_row; row++) {
        uint32_t address = start_address + (((row / 8) & 0x07) << 7) + (((row / 8) & 0x18) * 5) + ((row & 7) << 10);
        uint8_t *vram_row = &sys->ram[address];

        uint16_t words[40];

        uint16_t last_output_bit = 0;

        for (int col = 0; col < 40; col++) {
            uint16_t w = _apple2_double_7_bits(vram_row[col] & 0x7F);
            if (vram_row[col] & 0x80) {
                w = (w << 1 | last_output_bit) & 0x3FFF;
            };
            words[col] = w;
            last_output_bit = w >> 13;
        }

        _apple2_render_line_color(_apple2_get_fb_addr(sys, row), words, 0, 40);
    }

    if (!sys->page2) {
        sys->hires_page1_dirty = false;
    } else {
        sys->hires_page2_dirty = false;
    }
}

void apple2_screen_update(apple2_t *sys) {
    uint16_t text_start_row = 0;

    if (!sys->text) {
        text_start_row = 192 - (sys->mixed ? 32 : 0);

        if (sys->hires) {
            _apple2_hgr_update(sys, 0, text_start_row - 1);
        } else {
            _apple2_lores_update(sys, 0, text_start_row - 1);
        }
    }

    if (text_start_row < 192) {
        _apple2_text_update(sys, text_start_row, 191);
    }
}

#endif  // CHIPS_IMPL
