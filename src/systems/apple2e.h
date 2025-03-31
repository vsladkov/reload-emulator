#pragma once

// apple2e.h
//
// An Apple //e emulator in a C header.
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
// You need to include the following headers before including apple2e.h:
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
// ## The Apple //e
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

// Bump snapshot version when apple2e_t memory layout changes
#define APPLE2E_SNAPSHOT_VERSION (1)

#define APPLE2E_FREQUENCY (1021800)

#define APPLE2E_SCREEN_WIDTH     560  // (280 * 2)
#define APPLE2E_SCREEN_HEIGHT    192  // (192)
#define APPLE2E_FRAMEBUFFER_SIZE ((APPLE2E_SCREEN_WIDTH / 2) * APPLE2E_SCREEN_HEIGHT)

#define PALETTE_BITS 4
#define PALETTE_SIZE (1 << PALETTE_BITS)

#define RGBA8(r, g, b) (0xFF000000 | (r << 16) | (g << 8) | (b))

// clang-format off
static const uint32_t apple2e_palette[PALETTE_SIZE] = {
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

// Config parameters for apple2e_init()
typedef struct {
    bool fdc_enabled;         // Set to true to enable floppy disk controller emulation
    bool hdc_enabled;         // Set to true to enable hard disk controller emulation
    bool hdc_internal_flash;  // Set to true to use internal flash
    chips_debug_t debug;      // Optional debugging hook
    chips_audio_desc_t audio;
    struct {
        chips_range_t rom;
        chips_range_t character_rom;
        chips_range_t keyboard_rom;
        chips_range_t fdc_rom;
        chips_range_t hdc_rom;
    } roms;
} apple2e_desc_t;

// Apple //e emulator state
typedef struct {
    MOS6502CPU_T cpu;
    beeper_t beeper;
    kbd_t kbd;
    mem_t mem;
    bool valid;
    chips_debug_t debug;

    chips_audio_callback_t audio_callback;

    uint8_t ram[0x10000];
    uint8_t aux_ram[0x10000];
    uint8_t *rom;
    uint8_t *character_rom;
    uint8_t *keyboard_rom;
    uint8_t *fdc_rom;
    uint8_t *hdc_rom;

    bool text;
    bool mixed;
    bool page2;
    bool hires;
    bool dhires;
    bool flash;
    bool _80col;
    bool altcharset;

    bool _80store;
    bool ramrd, ramwrt;
    bool altzp;
    bool intcxrom;
    bool slotc3rom;

    bool lcram, lcbnk2, prewrite, write_enabled;

    bool ioudis;
    bool vbl;

    uint32_t flash_timer_ticks;

    bool text_page1_dirty;
    bool text_page2_dirty;
    bool hires_page1_dirty;
    bool hires_page2_dirty;

    uint8_t fb[APPLE2E_FRAMEBUFFER_SIZE];

    disk2_fdc_t fdc;  // Disk II floppy disk controller

    prodos_hdc_t hdc;  // ProDOS hard disk controller

    uint8_t kbd_last_key;
    bool kbd_open_apple_pressed;
    bool kbd_solid_apple_pressed;

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
    uint16_t vbl_ticks;
} apple2e_t;

// Apple2e interface

// Initialize a new Apple2e instance
void apple2e_init(apple2e_t *sys, const apple2e_desc_t *desc);
// Discard Apple2e instance
void apple2e_discard(apple2e_t *sys);
// Reset a Apple2e instance
void apple2e_reset(apple2e_t *sys);

void apple2e_tick(apple2e_t *sys);

// Tick Apple2e instance for a given number of microseconds, return number of executed ticks
uint32_t apple2e_exec(apple2e_t *sys, uint32_t micro_seconds);
// Take snapshot, patches pointers to zero or offsets, returns snapshot version
uint32_t apple2e_save_snapshot(apple2e_t *sys, apple2e_t *dst);
// Load snapshot, returns false if snapshot version doesn't match
bool apple2e_load_snapshot(apple2e_t *sys, uint32_t version, apple2e_t *src);

void apple2e_screen_update(apple2e_t *sys);

#ifdef __cplusplus
}  // extern "C"
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h> /* memcpy, memset */
#ifndef CHIPS_ASSERT
#include <assert.h>
#define CHIPS_ASSERT(c) assert(c)
#endif

static void _apple2e_init_memorymap(apple2e_t *sys);

// clang-format off
static uint8_t __not_in_flash() _apple2e_artifact_color_lut[1<<7] = {
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

static inline uint32_t _apple2e_rotl4b(uint32_t n, uint32_t count) { return (n >> (-count & 3)) & 0xF; }

static inline uint32_t _apple2e_rotl4(uint32_t n, uint32_t count) { return _apple2e_rotl4b(n * 0x11, count); }

static uint16_t __not_in_flash() _apple2e_double_7_bits_lut[128];

static void _apple2e_init_double_7_bits_lut() {
    for (uint8_t bits = 0; bits < 128; ++bits) {
        uint16_t result = 0;
        for (int i = 6; i >= 0; i--) {
            result <<= 1;
            uint8_t bit = bits & (1 << i) ? 1 : 0;
            result |= bit;
            result <<= 1;
            result |= bit;
        }
        _apple2e_double_7_bits_lut[bits] = result;
    }
}

static inline uint16_t _apple2e_double_7_bits(uint8_t bits) { return _apple2e_double_7_bits_lut[bits]; }

void apple2e_init(apple2e_t *sys, const apple2e_desc_t *desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) {
        CHIPS_ASSERT(desc->debug.stopped);
    }

    _apple2e_init_double_7_bits_lut();

    memset(sys, 0, sizeof(apple2e_t));
    sys->valid = true;
    sys->debug = desc->debug;
    sys->audio_callback = desc->audio.callback;

    CHIPS_ASSERT(desc->roms.rom.ptr && (desc->roms.rom.size == 0x4000));
    CHIPS_ASSERT(desc->roms.character_rom.ptr && (desc->roms.character_rom.size == 0x1000));
    CHIPS_ASSERT(desc->roms.keyboard_rom.ptr && (desc->roms.keyboard_rom.size == 0x800));
    CHIPS_ASSERT(desc->roms.fdc_rom.ptr && (desc->roms.fdc_rom.size == 0x100));
    CHIPS_ASSERT(desc->roms.hdc_rom.ptr && (desc->roms.hdc_rom.size == 0x100));
    sys->rom = desc->roms.rom.ptr;
    sys->character_rom = desc->roms.character_rom.ptr;
    sys->fdc_rom = desc->roms.fdc_rom.ptr;
    sys->hdc_rom = desc->roms.hdc_rom.ptr;

    MOS6502CPU_INIT(&sys->cpu, &(MOS6502CPU_DESC_T){0});

    beeper_init(&sys->beeper, &(beeper_desc_t){
                                  .tick_hz = APPLE2E_FREQUENCY,
                                  .sound_hz = CHIPS_DEFAULT(desc->audio.sample_rate, 44100),
                                  .base_volume = CHIPS_DEFAULT(desc->audio.volume, 1.0f),
                              });

    // setup memory map and keyboard matrix
    _apple2e_init_memorymap(sys);

    sys->flash_timer_ticks = APPLE2E_FREQUENCY / 2;

    sys->ioudis = true;

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
            if (CHIPS_ARRAY_SIZE(apple2_msc_images) > 0) {
                prodos_hdd_insert_disk_msc(&sys->hdc.hdd[0], apple2_msc_images[0]);
            }
        }
    }
}

void apple2e_discard(apple2e_t *sys) {
    CHIPS_ASSERT(sys && sys->valid);
    if (sys->fdc.valid) {
        disk2_fdc_discard(&sys->fdc);
    }
    if (sys->hdc.valid) {
        prodos_hdc_discard(&sys->hdc);
    }
    sys->valid = false;
}

void apple2e_reset(apple2e_t *sys) {
    CHIPS_ASSERT(sys && sys->valid);
    beeper_reset(&sys->beeper);
    if (sys->fdc.valid) {
        disk2_fdc_reset(&sys->fdc);
    }
    if (sys->hdc.valid) {
        prodos_hdc_reset(&sys->hdc);
    }
    MOS6502CPU_RESET(&sys->cpu);
}

typedef struct {
    uint8_t *read_ptr;
    uint8_t *write_ptr;
} mem_bank_t;

static mem_bank_t mem_bank_0000[2];
static mem_bank_t mem_bank_0200[4];
static mem_bank_t mem_bank_0400[4];
static mem_bank_t mem_bank_0800[4];
static mem_bank_t mem_bank_2000[4];
static mem_bank_t mem_bank_4000[4];

static void _apple2e_text_bank_update(apple2e_t *sys) {
    int ramwr = (sys->ramrd ? 1 : 0) | (sys->ramwrt ? 2 : 0);
    int bank = ramwr;
    if (sys->_80store) {
        bank = sys->page2 ? 3 : 0;
    }
    mem_map_rw(&sys->mem, 0, 0x0400, 0x400, mem_bank_0400[bank].read_ptr, mem_bank_0400[bank].write_ptr);
}

static void _apple2e_hires_bank_update(apple2e_t *sys) {
    int ramwr = (sys->ramrd ? 1 : 0) | (sys->ramwrt ? 2 : 0);
    int bank = ramwr;
    if ((sys->_80store) && (sys->hires)) {
        bank = sys->page2 ? 3 : 0;
    }
    mem_map_rw(&sys->mem, 0, 0x2000, 0x2000, mem_bank_2000[bank].read_ptr, mem_bank_2000[bank].write_ptr);
}

static void _apple2e_aux_bank_update(apple2e_t *sys) {
    int ramwr = (sys->ramrd ? 1 : 0) | (sys->ramwrt ? 2 : 0);

    mem_map_rw(&sys->mem, 0, 0x0200, 0x200, mem_bank_0200[ramwr].read_ptr, mem_bank_0200[ramwr].write_ptr);

    if (!sys->_80store) {
        _apple2e_text_bank_update(sys);
    }

    mem_map_rw(&sys->mem, 0, 0x0800, 0x1800, mem_bank_0800[ramwr].read_ptr, mem_bank_0800[ramwr].write_ptr);

    if (!((sys->_80store) && (sys->hires))) {
        _apple2e_hires_bank_update(sys);
    }

    mem_map_rw(&sys->mem, 0, 0x4000, 0x8000, mem_bank_4000[ramwr].read_ptr, mem_bank_4000[ramwr].write_ptr);
}

static void _apple2e_lc_bank_update(apple2e_t *sys) {
    uint8_t *ram_ptr = sys->altzp ? sys->aux_ram : sys->ram;
    uint16_t bank_offset = 0xC000 + (sys->lcbnk2 ? 0x1000 : 0x0000);

    if (sys->lcram) {
        if (sys->write_enabled) {
            mem_map_ram(&sys->mem, 0, 0xD000, 0x1000, ram_ptr + bank_offset);
            mem_map_ram(&sys->mem, 0, 0xE000, 0x2000, ram_ptr + 0xE000);
        } else {
            mem_map_rom(&sys->mem, 0, 0xD000, 0x1000, ram_ptr + bank_offset);
            mem_map_rom(&sys->mem, 0, 0xE000, 0x2000, ram_ptr + 0xE000);
        }
    } else {
        if (sys->write_enabled) {
            mem_map_rw(&sys->mem, 0, 0xD000, 0x1000, sys->rom + 0x1000, ram_ptr + bank_offset);
            mem_map_rw(&sys->mem, 0, 0xE000, 0x2000, sys->rom + 0x2000, ram_ptr + 0xE000);
        } else {
            mem_map_rom(&sys->mem, 0, 0xD000, 0x3000, sys->rom + 0x1000);
        }
    }
}

static void _apple2e_altzp_update(apple2e_t *sys) {
    mem_map_rw(&sys->mem, 0, 0x0000, 0x200, mem_bank_0000[sys->altzp].read_ptr, mem_bank_0000[sys->altzp].write_ptr);
    _apple2e_lc_bank_update(sys);
}

static void _apple2e_lc_control(apple2e_t *sys, uint8_t offset, bool rw) {
    if ((offset & 1) == 0) {
        sys->prewrite = false;
        sys->write_enabled = false;
    }

    if (!rw) {
        sys->prewrite = false;
    } else if ((offset & 1) == 1) {
        if (!sys->prewrite) {
            sys->prewrite = true;
        } else {
            sys->write_enabled = true;
        }
    }

    switch (offset & 3) {
        case 0:
        case 3: {
            sys->lcram = true;
            break;
        }

        case 1:
        case 2: {
            sys->lcram = false;
            break;
        }
    }

    sys->lcbnk2 = !(offset & 8);

    _apple2e_lc_bank_update(sys);
}

static void _apple2e_mem_c000_c00f_w(apple2e_t *sys, uint16_t addr) {
    switch (addr & 0xF) {
        case 0x00:  // 80STOREOFF
            if (sys->_80store) {
                sys->_80store = false;
                _apple2e_text_bank_update(sys);
                _apple2e_hires_bank_update(sys);
            }
            break;

        case 0x01:  // 80STOREON
            if (!sys->_80store) {
                sys->_80store = true;
                _apple2e_text_bank_update(sys);
                _apple2e_hires_bank_update(sys);
            }
            break;

        case 0x02:  // RAMRDOFF
            if (sys->ramrd) {
                sys->ramrd = false;
                _apple2e_aux_bank_update(sys);
            }
            break;

        case 0x03:  // RAMRDON
            if (!sys->ramrd) {
                sys->ramrd = true;
                _apple2e_aux_bank_update(sys);
            }
            break;

        case 0x04:  // RAMWRTOFF
            if (sys->ramwrt) {
                sys->ramwrt = false;
                _apple2e_aux_bank_update(sys);
            }
            break;

        case 0x05:  // RAMWRTON
            if (!sys->ramwrt) {
                sys->ramwrt = true;
                _apple2e_aux_bank_update(sys);
            }
            break;

        case 0x06:  // INTCXROMOFF
            sys->intcxrom = false;
            break;

        case 0x07:  // INTCXROMON
            sys->intcxrom = true;
            break;

        case 0x08:  // ALTZPOFF
            if (sys->altzp) {
                sys->altzp = false;
                _apple2e_altzp_update(sys);
            }
            break;

        case 0x09:  // ALTZPON
            if (!sys->altzp) {
                sys->altzp = true;
                _apple2e_altzp_update(sys);
            }
            break;

        case 0x0A:  // SETINTC3ROM
            sys->slotc3rom = false;
            break;

        case 0x0B:  // SETSLOTC3ROM
            sys->slotc3rom = true;
            break;

        case 0x0C:  // 80COLOFF
            sys->_80col = false;
            break;

        case 0x0D:  // 80COLON
            sys->_80col = true;
            break;

        case 0x0E:  // ALTCHARSETOFF
            sys->altcharset = false;
            break;

        case 0x0F:  // ALTCHARSETON
            sys->altcharset = true;
            break;

        default:
            break;
    }
}

static void _apple2e_mem_c010_c01f_r(apple2e_t *sys, uint16_t addr) {
    uint8_t data = 0;
    switch (addr & 0x1F) {
        case 0x11:  // read LCBNK2
            data = sys->lcbnk2 ? 0x80 : 0x00;
            break;

        case 0x12:  // read LCRAM
            data = sys->lcram ? 0x80 : 0x00;
            break;

        case 0x13:  // read RAMRD
            data = sys->ramrd ? 0x80 : 0x00;
            break;

        case 0x14:  // read RAMWRT
            data = sys->ramwrt ? 0x80 : 0x00;
            break;

        case 0x15:  // read INTCXROM
            data = sys->intcxrom ? 0x80 : 0x00;
            break;

        case 0x16:  // read ALTZP
            data = sys->altzp ? 0x80 : 0x00;
            break;

        case 0x17:  // read SLOTC3ROM
            data = sys->slotc3rom ? 0x80 : 0x00;
            break;

        case 0x18:  // read 80STORE
            data = sys->_80store ? 0x80 : 0x00;
            break;

        case 0x19:  // read VBL
            data = sys->vbl ? 0x80 : 0x00;
            break;

        case 0x1A:  // read TEXT
            data = sys->text ? 0x80 : 0x00;
            break;

        case 0x1B:  //  read MIXED
            data = sys->mixed ? 0x80 : 0x00;
            break;

        case 0x1C:  // read PAGE2
            data = sys->page2 ? 0x80 : 0x00;
            break;

        case 0x1D:  // read HIRES
            data = sys->hires ? 0x80 : 0x00;
            break;

        case 0x1E:  // read ALTCHARSET
            data = sys->altcharset ? 0x80 : 0x00;
            break;

        case 0x1F:  // read 80COL
            data = sys->_80col ? 0x80 : 0x00;
            break;

        default:
            break;
    }
    MOS6502CPU_SET_DATA(&sys->cpu, data);
}

static void _apple2e_mem_c000_c0ff_rw(apple2e_t *sys, uint16_t addr, bool rw) {
    switch (addr & 0xFF) {
        case 0x10:
            sys->kbd_last_key &= 0x7F;
            break;

        case 0x50:  // TEXTOFF
            sys->text = false;
            break;

        case 0x51:  // TEXTON
            sys->text = true;
            break;

        case 0x52:  // MIXEDOFF
            sys->mixed = false;
            break;

        case 0x53:  // MIXEDON
            sys->mixed = true;
            break;

        case 0x54:  // PAGE20FF
            if (sys->page2) {
                sys->page2 = false;
                if (sys->_80store) {
                    _apple2e_text_bank_update(sys);
                    if (sys->hires) {
                        _apple2e_hires_bank_update(sys);
                    }
                }
            }
            break;

        case 0x55:  // PAGE20N
            if (!sys->page2) {
                sys->page2 = true;
                if (sys->_80store) {
                    _apple2e_text_bank_update(sys);
                    if (sys->hires) {
                        _apple2e_hires_bank_update(sys);
                    }
                }
            }
            break;

        case 0x56:  // HIRESOFF
            if (sys->hires) {
                sys->hires = false;
                if (sys->_80store) {
                    _apple2e_hires_bank_update(sys);
                }
            }
            break;

        case 0x57:  // HIRESON
            if (!sys->hires) {
                sys->hires = true;
                if (sys->_80store) {
                    _apple2e_hires_bank_update(sys);
                }
            }
            break;

        case 0x5E:  // DHIRESON
            if (sys->ioudis) {
                sys->dhires = true;
            }
            break;

        case 0x5F:  // DHIRESOFF
            if (sys->ioudis) {
                sys->dhires = false;
            }
            break;

        case 0x61:  // Button 0 or Open Apple
        case 0x69:
            if (rw) {
                MOS6502CPU_SET_DATA(&sys->cpu, sys->butn0 || sys->kbd_open_apple_pressed ? 0x80 : 00);
            }
            break;

        case 0x62:  // Botton 1 or Solid Apple
        case 0x6A:
            if (rw) {
                MOS6502CPU_SET_DATA(&sys->cpu, sys->butn1 || sys->kbd_solid_apple_pressed ? 0x80 : 00);
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

        case 0x7E:
            if (rw) {
                // read IOUDIS
                MOS6502CPU_SET_DATA(&sys->cpu, sys->ioudis ? 0x00 : 0x80);
            } else {
                // IOUDISON
                sys->ioudis = true;
            }
            break;

        case 0x7F:
            if (rw) {
                // read DHIRES
                MOS6502CPU_SET_DATA(&sys->cpu, sys->dhires ? 0x00 : 0x80);
            } else {
                // IOUDISOFF
                sys->ioudis = false;
            }
            break;

        default:
            if ((addr >= 0xC000) && (addr <= 0xC00F)) {
                // Keyboard latch
                if (rw) {
                    MOS6502CPU_SET_DATA(&sys->cpu, sys->kbd_last_key);
                } else {
                    _apple2e_mem_c000_c00f_w(sys, addr);
                }
            } else if ((addr > 0xC010) && (addr <= 0xC01F)) {  // 0xC010 processed above
                if (rw) {
                    _apple2e_mem_c010_c01f_r(sys, addr);
                }
            } else if ((addr >= 0xC030) && (addr <= 0xC03F)) {
                // Speaker
                beeper_toggle(&sys->beeper);
            } else if ((addr >= 0xC070) && (addr <= 0xC07F)) {
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
                // 16K Language Card
                _apple2e_lc_control(sys, addr & 0xF, rw);
                if (rw) {
                    MOS6502CPU_SET_DATA(&sys->cpu, 0xFF);
                }
            } else if ((addr >= 0xC0E0) && (addr <= 0xC0EF)) {
                // Disk II FDC
                if (rw) {
                    // Memory read
                    MOS6502CPU_SET_DATA(&sys->cpu, sys->fdc.valid ? disk2_fdc_read_byte(&sys->fdc, addr & 0xF) : 0x00);
                } else {
                    // Memory write
                    disk2_fdc_write_byte(&sys->fdc, addr & 0xF, MOS6502CPU_GET_DATA(&sys->cpu));
                }
            } else if ((addr >= 0xC0F0) && (addr <= 0xC0FF)) {
                // ProDOS HDC
                if (rw) {
                    // Memory read
                    MOS6502CPU_SET_DATA(&sys->cpu, sys->hdc.valid ? prodos_hdc_read_byte(&sys->hdc, addr & 0xF) : 0x00);
                } else {
                    // Memory write
                    prodos_hdc_write_byte(&sys->hdc, addr & 0xF, MOS6502CPU_GET_DATA(&sys->cpu), &sys->mem);
                }
            }
            break;
    }
}

static void _apple2e_mem_rw(apple2e_t *sys, uint16_t addr, bool rw) {
    if ((addr >= 0xC000) && (addr <= 0xCFFF)) {
        if ((addr >= 0xC000) && (addr <= 0xC0FF)) {
            // Apple //e I/O Page
            _apple2e_mem_c000_c0ff_rw(sys, addr, rw);
        } else if ((addr >= 0xC300) && (addr <= 0xC3FF) && !sys->intcxrom) {
            if (rw) {
                // Memory read
                MOS6502CPU_SET_DATA(&sys->cpu, sys->slotc3rom ? 0x00 : mem_rd(&sys->mem, addr));
            }
        } else if ((addr >= 0xC600) && (addr <= 0xC6FF) && !sys->intcxrom) {
            // Disk II boot rom
            if (rw) {
                // Memory read
                MOS6502CPU_SET_DATA(&sys->cpu, sys->fdc.valid ? sys->fdc_rom[addr & 0xFF] : 0x00);
            }
        } else if ((addr >= 0xC700) && (addr <= 0xC7FF) && !sys->intcxrom) {
            // Hard disk boot rom
            if (rw) {
                // Memory read
                MOS6502CPU_SET_DATA(&sys->cpu, sys->hdc.valid ? sys->hdc_rom[addr & 0xFF] : 0x00);
            }
        } else if ((addr >= 0xC100) && (addr <= 0xCFFF)) {
            if (rw) {
                // Memory read
                MOS6502CPU_SET_DATA(&sys->cpu, mem_rd(&sys->mem, addr));
            }
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

void apple2e_tick(apple2e_t *sys) {
    if (sys->vbl_ticks == 12480) {
        sys->vbl = true;
    }

    if (sys->vbl_ticks < 17030) {
        sys->vbl_ticks++;
    } else {
        sys->vbl_ticks = 0;
        sys->vbl = false;
    }

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

    _apple2e_mem_rw(sys, sys->cpu.addr, sys->cpu.rw);

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
            sys->flash_timer_ticks = APPLE2E_FREQUENCY / 2;
            if (!sys->page2) {
                sys->text_page1_dirty = true;
            } else {
                sys->text_page2_dirty = true;
            }
        }
    }

    sys->system_ticks++;
}

uint32_t apple2e_exec(apple2e_t *sys, uint32_t micro_seconds) {
    CHIPS_ASSERT(sys && sys->valid);
    uint32_t num_ticks = clk_us_to_ticks(APPLE2E_FREQUENCY, micro_seconds);
    // uint32_t num_ticks = 50;
    if (0 == sys->debug.callback.func) {
        // run without debug callback
        for (uint32_t ticks = 0; ticks < num_ticks; ticks++) {
            apple2e_tick(sys);
        }
    } else {
        // run with debug callback
        for (uint32_t ticks = 0; (ticks < num_ticks) && !(*sys->debug.stopped); ticks++) {
            apple2e_tick(sys);
            sys->debug.callback.func(sys->debug.callback.user_data, 0);
        }
    }
    // kbd_update(&sys->kbd, micro_seconds);
    apple2e_screen_update(sys);

    // printf("executed %d ticks\n", num_ticks);
    return num_ticks;
}

static void _apple2e_init_memorymap(apple2e_t *sys) {
    mem_init(&sys->mem);
    for (int addr = 0; addr < 0x10000; addr += 2) {
        sys->ram[addr] = 0;
        sys->ram[addr + 1] = 0xFF;
        sys->aux_ram[addr] = 0;
        sys->aux_ram[addr + 1] = 0xFF;
    }

    memcpy(mem_bank_0000,
           (mem_bank_t[]){
               {sys->ram, sys->ram},
               {sys->aux_ram, sys->aux_ram},
           },
           sizeof(mem_bank_0000));

    memcpy(mem_bank_0200,
           (mem_bank_t[]){
               {sys->ram + 0x200, sys->ram + 0x200},
               {sys->aux_ram + 0x200, sys->ram + 0x200},
               {sys->ram + 0x200, sys->aux_ram + 0x200},
               {sys->aux_ram + 0x200, sys->aux_ram + 0x200},
           },
           sizeof(mem_bank_0200));

    memcpy(mem_bank_0400,
           (mem_bank_t[]){
               {sys->ram + 0x400, sys->ram + 0x400},
               {sys->aux_ram + 0x400, sys->ram + 0x400},
               {sys->ram + 0x400, sys->aux_ram + 0x400},
               {sys->aux_ram + 0x400, sys->aux_ram + 0x400},
           },
           sizeof(mem_bank_0400));

    memcpy(mem_bank_0800,
           (mem_bank_t[]){
               {sys->ram + 0x800, sys->ram + 0x800},
               {sys->aux_ram + 0x800, sys->ram + 0x800},
               {sys->ram + 0x800, sys->aux_ram + 0x800},
               {sys->aux_ram + 0x800, sys->aux_ram + 0x800},
           },
           sizeof(mem_bank_0800));

    memcpy(mem_bank_2000,
           (mem_bank_t[]){
               {sys->ram + 0x2000, sys->ram + 0x2000},
               {sys->aux_ram + 0x2000, sys->ram + 0x2000},
               {sys->ram + 0x2000, sys->aux_ram + 0x2000},
               {sys->aux_ram + 0x2000, sys->aux_ram + 0x2000},
           },
           sizeof(mem_bank_2000));

    memcpy(mem_bank_4000,
           (mem_bank_t[]){
               {sys->ram + 0x4000, sys->ram + 0x4000},
               {sys->aux_ram + 0x4000, sys->ram + 0x4000},
               {sys->ram + 0x4000, sys->aux_ram + 0x4000},
               {sys->aux_ram + 0x4000, sys->aux_ram + 0x4000},
           },
           sizeof(mem_bank_4000));

    mem_map_ram(&sys->mem, 0, 0x0000, 0xC000, sys->ram);
    mem_map_rom(&sys->mem, 0, 0xC000, 0x1000, sys->rom);
    mem_map_rw(&sys->mem, 0, 0xD000, 0x1000, sys->rom + 0x1000, sys->ram + 0xD000);
    mem_map_rw(&sys->mem, 0, 0xE000, 0x2000, sys->rom + 0x2000, sys->ram + 0xE000);

    sys->lcbnk2 = true;
    sys->lcram = false;
    sys->prewrite = false;
    sys->write_enabled = true;
}

uint32_t apple2e_save_snapshot(apple2e_t *sys, apple2e_t *dst) {
    CHIPS_ASSERT(sys && dst);
    *dst = *sys;
    chips_debug_snapshot_onsave(&dst->debug);
    chips_audio_callback_snapshot_onsave(&dst->audio_callback);
    // m6502_snapshot_onsave(&dst->cpu);
    disk2_fdc_snapshot_onsave(&dst->fdc);
    mem_snapshot_onsave(&dst->mem, sys);
    return APPLE2E_SNAPSHOT_VERSION;
}

bool apple2e_load_snapshot(apple2e_t *sys, uint32_t version, apple2e_t *src) {
    CHIPS_ASSERT(sys && src);
    if (version != APPLE2E_SNAPSHOT_VERSION) {
        return false;
    }
    static apple2e_t im;
    im = *src;
    chips_debug_snapshot_onload(&im.debug, &sys->debug);
    chips_audio_callback_snapshot_onload(&im.audio_callback, &sys->audio_callback);
    // m6502_snapshot_onload(&im.cpu, &sys->cpu);
    disk2_fdc_snapshot_onload(&im.fdc, &sys->fdc);
    mem_snapshot_onload(&im.mem, sys);
    *sys = im;
    return true;
}

static void _apple2e_render_line_monochrome(uint8_t *out, uint16_t *in, int start_col, int stop_col) {
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

static void _apple2e_render_line_color(uint8_t *out, uint16_t *in, int start_col, int stop_col, bool is_80col) {
    uint32_t w = in[start_col] << 3;

    for (int col = start_col; col < stop_col; col++) {
        if (col + 1 < 40) {
            w += in[col + 1] << 17;
        }

        for (int b = 0; b < 7; b++) {
            uint8_t c1 = _apple2e_rotl4b(_apple2e_artifact_color_lut[w & 0x7F], col * 14 + b * 2 + is_80col);
            w >>= 1;
            uint8_t c2 = _apple2e_rotl4b(_apple2e_artifact_color_lut[w & 0x7F], col * 14 + b * 2 + 1 + is_80col);
            w >>= 1;
            out[col * 7 + b] = (c1 << 4) | (c2 & 0x0F);
        }
    }
}

static uint8_t _apple2e_get_text_character(apple2e_t *sys, uint8_t code, uint16_t row) {
    uint8_t invert_mask = 0x7F;

    if (!sys->altcharset) {
        if ((code >= 0x40) && (code <= 0x7f)) {
            code &= 0x3f;

            if (sys->flash) {
                invert_mask ^= 0x7F;
            }
        }
    } else {
        if ((code >= 0x60) && (code <= 0x7f)) {
            code |= 0x80;         // map to lowercase normal
            invert_mask ^= 0x7F;  // and flip the color
        }
    }

    /* look up the character data */
    uint8_t bits = sys->character_rom[code * 8 + row];
    bits = bits & 0x7F;
    bits ^= invert_mask;
    return bits;
}

static uint8_t *_apple2e_get_fb_addr(apple2e_t *sys, uint16_t row) {
    return &sys->fb[row * (APPLE2E_SCREEN_WIDTH / 2)];
}

static void _apple2e_lores_update(apple2e_t *sys, uint16_t begin_row, uint16_t end_row) {
    if ((!sys->page2 && !sys->text_page1_dirty) || (sys->page2 && !sys->text_page2_dirty)) {
        return;
    }

    bool _double = sys->dhires && sys->_80col;

    uint16_t start_address = sys->page2 && !sys->_80store ? 0x0800 : 0x0400;

    uint16_t start_row = (begin_row / 8) * 8;
    uint16_t stop_row = ((end_row / 8) + 1) * 8;

    for (int row = start_row; row < stop_row; row += 4) {
        uint16_t address = start_address + ((((row / 8) & 0x07) << 7) | (((row / 8) & 0x18) * 5));
        uint8_t *vram_row = &sys->ram[address];
        uint8_t *vaux_row = &sys->aux_ram[address];

#define NIBBLE(byte) (((byte) >> (row & 4)) & 0x0F)
        uint8_t *p = _apple2e_get_fb_addr(sys, row);

        for (int col = 0; col < 40; col++) {
            uint8_t c;
            if (_double) {
                c = _apple2e_rotl4(NIBBLE(vaux_row[col]), 1);
                for (int b = 0; b < 3; b++) {
                    *p = c << 4;
                    *p++ |= c;
                }
                *p = c << 4;
                c = NIBBLE(vram_row[col]);
                *p++ |= c;
                for (int b = 0; b < 3; b++) {
                    *p = c << 4;
                    *p++ |= c;
                }
            } else {
                c = NIBBLE(vram_row[col]);
                for (int b = 0; b < 7; b++) {
                    *p = c << 4;
                    *p++ |= c;
                }
            }
        }
#undef NIBBLE

        for (int y = 1; y < 4; y++) {
            memcpy(_apple2e_get_fb_addr(sys, row + y), _apple2e_get_fb_addr(sys, row), 40 * 7);
        }
    }

    if (!sys->page2) {
        sys->text_page1_dirty = false;
    } else {
        sys->text_page2_dirty = false;
    }
}

static void _apple2e_text_update(apple2e_t *sys, uint16_t begin_row, uint16_t end_row) {
    if ((!sys->page2 && !sys->text_page1_dirty) || (sys->page2 && !sys->text_page2_dirty)) {
        return;
    }

    uint16_t start_address = sys->page2 && !sys->_80store ? 0x0800 : 0x0400;

    uint16_t start_row = (begin_row / 8) * 8;
    uint16_t stop_row = ((end_row / 8) + 1) * 8;

    for (int row = start_row; row < stop_row; row++) {
        uint16_t address = start_address + ((((row / 8) & 0x07) << 7) | (((row / 8) & 0x18) * 5));
        uint8_t *vram_row = &sys->ram[address];
        uint8_t *vaux_row = &sys->aux_ram[address];

        uint16_t words[40];

        for (int col = 0; col < 40; col++) {
            if (sys->_80col) {
                words[col] = _apple2e_get_text_character(sys, vaux_row[col], row & 7) +
                             (_apple2e_get_text_character(sys, vram_row[col], row & 7) << 7);
            } else {
                words[col] = _apple2e_double_7_bits(_apple2e_get_text_character(sys, vram_row[col], row & 7));
            }
        }

        _apple2e_render_line_monochrome(_apple2e_get_fb_addr(sys, row), words, 0, 40);
    }

    if (!sys->page2) {
        sys->text_page1_dirty = false;
    } else {
        sys->text_page2_dirty = false;
    }
}

static void _apple2e_dhgr_update(apple2e_t *sys, uint16_t begin_row, uint16_t end_row) {
    if ((!sys->page2 && !sys->hires_page1_dirty) || (sys->page2 && !sys->hires_page2_dirty)) {
        return;
    }

    uint16_t start_address = sys->page2 && !sys->_80store ? 0x4000 : 0x2000;

    for (int row = begin_row; row <= end_row; row++) {
        uint32_t address = start_address + (((row / 8) & 0x07) << 7) + (((row / 8) & 0x18) * 5) + ((row & 7) << 10);
        uint8_t *vram_row = &sys->ram[address];
        uint8_t *vaux_row = &sys->aux_ram[address];

        uint16_t words[40];

        for (int col = 0; col < 40; col++) {
            words[col] = ((vaux_row[col] & 0x7F) | ((vram_row[col] & 0x7F) << 7)) & 0x3FFF;
        }

        _apple2e_render_line_color(_apple2e_get_fb_addr(sys, row), words, 0, 40, true);
    }

    if (!sys->page2) {
        sys->hires_page1_dirty = false;
    } else {
        sys->hires_page2_dirty = false;
    }
}

static void _apple2e_hgr_update(apple2e_t *sys, uint16_t begin_row, uint16_t end_row) {
    if ((!sys->page2 && !sys->hires_page1_dirty) || (sys->page2 && !sys->hires_page2_dirty)) {
        return;
    }

    uint16_t start_address = sys->page2 && !sys->_80store ? 0x4000 : 0x2000;

    for (int row = begin_row; row <= end_row; row++) {
        uint32_t address = start_address + (((row / 8) & 0x07) << 7) + (((row / 8) & 0x18) * 5) + ((row & 7) << 10);
        uint8_t *vram_row = &sys->ram[address];

        uint16_t words[40];

        uint16_t last_output_bit = 0;

        for (int col = 0; col < 40; col++) {
            uint16_t w = _apple2e_double_7_bits(vram_row[col] & 0x7F);
            if (vram_row[col] & 0x80) {
                w = (w << 1 | last_output_bit) & 0x3FFF;
            };
            words[col] = w;
            last_output_bit = w >> 13;
        }

        _apple2e_render_line_color(_apple2e_get_fb_addr(sys, row), words, 0, 40, false);
    }

    if (!sys->page2) {
        sys->hires_page1_dirty = false;
    } else {
        sys->hires_page2_dirty = false;
    }
}

void apple2e_screen_update(apple2e_t *sys) {
    uint16_t text_start_row = 0;

    if (!sys->text) {
        text_start_row = 192 - (sys->mixed ? 32 : 0);

        if (sys->hires) {
            if (sys->dhires && sys->_80col) {
                _apple2e_dhgr_update(sys, 0, text_start_row - 1);
            } else {
                _apple2e_hgr_update(sys, 0, text_start_row - 1);
            }
        } else {
            _apple2e_lores_update(sys, 0, text_start_row - 1);
        }
    }

    if (text_start_row < 192) {
        _apple2e_text_update(sys, text_start_row, 191);
    }
}

#endif  // CHIPS_IMPL
