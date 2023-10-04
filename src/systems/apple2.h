#pragma once
/*#
    # apple2p.h

    An Apple II emulator in a C header.

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C or C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation

    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    You need to include the following headers before including apple2.h:

    - chips/chips_common.h
    - chips/beeper.h
    - chips/kbd.h
    - chips/mem.h
    - chips/clk.h
    - devices/apple2_lc.h
    - devices/disk2_fdd.h
    - devices/disk2_fdc.h
    - devices/prodos_hdd.h
    - devices/prodos_hdc.h
    - devices/prodos_hdc_rom.h

    ## The Apple II


    TODO!

    ## Links

    ## zlib/libpng license

    Copyright (c) 2023 Veselin Sladkov
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
#*/
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

#define APPLE2_FREQUENCY             (1021800)
#define APPLE2_MAX_AUDIO_SAMPLES     (2048)     // Max number of audio samples in internal sample buffer
#define APPLE2_DEFAULT_AUDIO_SAMPLES (2048)     // Default number of samples in internal sample buffer
#define APPLE2_MAX_TAPE_SIZE         (1 << 16)  // Max size of tape file in bytes

#define APPLE2_SCREEN_WIDTH     560  // (280 * 2)
#define APPLE2_SCREEN_HEIGHT    192  // (192)
#define APPLE2_FRAMEBUFFER_SIZE ((APPLE2_SCREEN_WIDTH / 2) * APPLE2_SCREEN_HEIGHT)

#define APPLE2_LANGUAGE_CARD_READ  (1)
#define APPLE2_LANGUAGE_CARD_WRITE (2)

// Config parameters for apple2_init()
typedef struct {
    bool fdc_enabled;     // Set to true to enable floppy disk controller emulation
    bool hdc_enabled;     // Set to true to enable hard disk controller emulation
    chips_debug_t debug;  // Optional debugging hook
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
    // m6502_t cpu;
    beeper_t beeper;
    kbd_t kbd;
    mem_t mem;
    bool valid;
    chips_debug_t debug;

    struct {
        chips_audio_callback_t callback;
        int num_samples;
        int sample_pos;
        uint8_t sample_buffer[APPLE2_MAX_AUDIO_SAMPLES];
    } audio;

    uint8_t ram[0xC000];
    uint8_t *rom;
    uint8_t *character_rom;
    uint8_t *fdc_rom;
    uint8_t *hdc_rom;

    apple2_lc_t lc;  // Language card

    bool text;
    bool mix;
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

    uint8_t last_key_code;

    uint32_t system_ticks;
} apple2_t;

// Apple2 interface

// initialize a new Apple2 instance
void apple2_init(apple2_t *sys, const apple2_desc_t *desc);
// discard Apple2 instance
void apple2_discard(apple2_t *sys);
// reset a Apple2 instance
void apple2_reset(apple2_t *sys);

void apple2_tick(apple2_t *sys);

// tick Apple2 instance for a given number of microseconds, return number of executed ticks
uint32_t apple2_exec(apple2_t *sys, uint32_t micro_seconds);
// send a key-down event to the Apple2 Atmos
void apple2_key_down(apple2_t *sys, int key_code);
// send a key-up event to the Apple2 Atmos
void apple2_key_up(apple2_t *sys, int key_code);
// take a snapshot, patches pointers to zero or offsets, returns snapshot version
uint32_t apple2_save_snapshot(apple2_t *sys, apple2_t *dst);
// load a snapshot, returns false if snapshot version doesn't match
bool apple2_load_snapshot(apple2_t *sys, uint32_t version, apple2_t *src);

void apple2_screen_update(apple2_t *sys);

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

static void _apple2_init_memorymap(apple2_t *sys);

// clang-format off
static uint8_t __not_in_flash() _apple2_artifact_color_lut[1<<7] = {
	0x00,0x00,0x00,0x00,0x88,0x00,0x00,0x00,0x11,0x11,0x55,0x11,0x99,0x99,0xDD,0xFF,
	0x22,0x22,0x66,0x66,0xAA,0xAA,0xEE,0xEE,0x33,0x33,0x33,0x33,0xBB,0xBB,0xFF,0xFF,
	0x00,0x00,0x44,0x44,0xCC,0xCC,0xCC,0xCC,0x55,0x55,0x55,0x55,0x99,0x99,0xDD,0xFF,
	0x00,0x22,0x66,0x66,0xEE,0xAA,0xEE,0xEE,0x77,0x77,0x77,0x77,0xFF,0xFF,0xFF,0xFF,
	0x00,0x00,0x00,0x00,0x88,0x88,0x88,0x88,0x11,0x11,0x55,0x11,0x99,0x99,0xDD,0xFF,
	0x00,0x22,0x66,0x66,0xAA,0xAA,0xAA,0xAA,0x33,0x33,0x33,0x33,0xBB,0xBB,0xFF,0xFF,
	0x00,0x00,0x44,0x44,0xCC,0xCC,0xCC,0xCC,0x11,0x11,0x55,0x55,0x99,0x99,0xDD,0xDD,
	0x00,0x22,0x66,0x66,0xEE,0xAA,0xEE,0xEE,0xFF,0xFF,0xFF,0x77,0xFF,0xFF,0xFF,0xFF
};
// clang-format on

#define _APPLE2_DEFAULT(val, def) (((val) != 0) ? (val) : (def))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#ifdef PICO_NEO6502
#define _APPLE2_GPIO_MASK       (0xFF)
#define _APPLE2_GPIO_SHIFT_BITS (0)
#define _APPLE2_OE1_PIN         (8)
#define _APPLE2_OE2_PIN         (9)
#define _APPLE2_OE3_PIN         (10)
#define _APPLE2_RW_PIN          (11)
#define _APPLE2_CLOCK_PIN       (21)
#define _APPLE2_AUDIO_PIN       (20)
#define _APPLE2_RESET_PIN       (26)
#else
#define _APPLE2_GPIO_MASK       (0x3FC)
#define _APPLE2_GPIO_SHIFT_BITS (2)
#define _APPLE2_OE1_PIN         (10)
#define _APPLE2_OE2_PIN         (11)
#define _APPLE2_OE3_PIN         (20)
#define _APPLE2_RW_PIN          (21)
#define _APPLE2_CLOCK_PIN       (22)
#define _APPLE2_AUDIO_PIN       (27)
#define _APPLE2_RESET_PIN       (26)
#endif  // PICO_NEO6502

void apple2_init(apple2_t *sys, const apple2_desc_t *desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) {
        CHIPS_ASSERT(desc->debug.stopped);
    }

    memset(sys, 0, sizeof(apple2_t));
    sys->valid = true;
    sys->debug = desc->debug;
    sys->audio.callback = desc->audio.callback;
    sys->audio.num_samples = _APPLE2_DEFAULT(desc->audio.num_samples, APPLE2_DEFAULT_AUDIO_SAMPLES);
    CHIPS_ASSERT(sys->audio.num_samples <= APPLE2_MAX_AUDIO_SAMPLES);

    CHIPS_ASSERT(desc->roms.rom.ptr && (desc->roms.rom.size == 0x4000));
    CHIPS_ASSERT(desc->roms.character_rom.ptr && (desc->roms.character_rom.size == 0x800));
    CHIPS_ASSERT(desc->roms.fdc_rom.ptr && (desc->roms.fdc_rom.size == 0x100));
    CHIPS_ASSERT(desc->roms.hdc_rom.ptr && (desc->roms.hdc_rom.size == 0x100));
    sys->rom = desc->roms.rom.ptr;
    sys->character_rom = desc->roms.character_rom.ptr;
    sys->fdc_rom = desc->roms.fdc_rom.ptr;
    sys->hdc_rom = desc->roms.hdc_rom.ptr;

    // sys->pins = m6502_init(&sys->cpu, &(m6502_desc_t){0});

    gpio_init_mask(_APPLE2_GPIO_MASK);

    gpio_init(_APPLE2_OE1_PIN);  // OE1
    gpio_set_dir(_APPLE2_OE1_PIN, GPIO_OUT);
    gpio_put(_APPLE2_OE1_PIN, 1);

    gpio_init(_APPLE2_OE2_PIN);  // OE2
    gpio_set_dir(_APPLE2_OE2_PIN, GPIO_OUT);
    gpio_put(_APPLE2_OE2_PIN, 1);

    gpio_init(_APPLE2_OE3_PIN);  // OE3
    gpio_set_dir(_APPLE2_OE3_PIN, GPIO_OUT);
    gpio_put(_APPLE2_OE3_PIN, 1);

    gpio_init(_APPLE2_RW_PIN);  // RW
    gpio_set_dir(_APPLE2_RW_PIN, GPIO_IN);

    gpio_init(_APPLE2_CLOCK_PIN);  // CLOCK
    gpio_set_dir(_APPLE2_CLOCK_PIN, GPIO_OUT);

    gpio_init(_APPLE2_RESET_PIN);  // RESET
    gpio_set_dir(_APPLE2_RESET_PIN, GPIO_OUT);

    gpio_put(_APPLE2_RESET_PIN, 0);
    sleep_ms(1);
    gpio_put(_APPLE2_RESET_PIN, 1);

    beeper_init(&sys->beeper, &(beeper_desc_t){
                                  .tick_hz = APPLE2_FREQUENCY,
                                  .sound_hz = _APPLE2_DEFAULT(desc->audio.sample_rate, 22050),
                                  .base_volume = _APPLE2_DEFAULT(desc->audio.volume, 1.0f),
                              });

    // setup memory map and keyboard matrix
    _apple2_init_memorymap(sys);

    apple2_lc_init(&sys->lc, &(apple2_lc_desc_t){&sys->mem, sys->rom});

    sys->text = false;
    sys->mix = false;
    sys->page2 = false;
    sys->hires = false;

    sys->flash = false;
    sys->flash_timer_ticks = APPLE2_FREQUENCY / 2;

    sys->last_key_code = 0x0D | 0x80;

    // Optionally setup floppy disk controller
    if (desc->fdc_enabled) {
        disk2_fdc_init(&sys->fdc);
        if (ARRAY_SIZE(apple2_nib_images) > 0) {
            disk2_fdd_insert_disk(&sys->fdc.fdd[0], apple2_nib_images[0]);
        }
    }

    // Optionally setup hard disk controller
    if (desc->hdc_enabled) {
        prodos_hdc_init(&sys->hdc);
        if (ARRAY_SIZE(apple2_po_images) > 0) {
            prodos_hdd_insert_disk(&sys->hdc.hdd[0], apple2_po_images[0], apple2_po_image_sizes[0]);
        }
    }
}

void apple2_discard(apple2_t *sys) {
    CHIPS_ASSERT(sys && sys->valid);
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
    beeper_reset(&sys->beeper);
    if (sys->fdc.valid) {
        disk2_fdc_reset(&sys->fdc);
    }
    if (sys->hdc.valid) {
        prodos_hdc_reset(&sys->hdc);
    }
    // reset cpu
    gpio_put(_APPLE2_RESET_PIN, 0);
    sleep_ms(1);
    gpio_put(_APPLE2_RESET_PIN, 1);
}

static uint16_t _m6502_get_address() {
    gpio_set_dir_masked(_APPLE2_GPIO_MASK, 0);

    gpio_put(_APPLE2_OE1_PIN, 0);
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
#ifndef PICO_NEO6502
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
#endif
    uint16_t addr = (gpio_get_all() >> _APPLE2_GPIO_SHIFT_BITS) & 0xFF;
    gpio_put(_APPLE2_OE1_PIN, 1);

    gpio_put(_APPLE2_OE2_PIN, 0);
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
#ifndef PICO_NEO6502
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
#endif
    addr |= (gpio_get_all() << (8 - _APPLE2_GPIO_SHIFT_BITS)) & 0xFF00;
    gpio_put(_APPLE2_OE2_PIN, 1);

    // printf("get addr: %04x\n", addr);

    return addr;
}

static uint8_t _m6502_get_data() {
    gpio_set_dir_masked(_APPLE2_GPIO_MASK, 0);

    gpio_put(_APPLE2_OE3_PIN, 0);
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
#ifndef PICO_NEO6502
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
#endif
    uint8_t data = (gpio_get_all() >> _APPLE2_GPIO_SHIFT_BITS) & 0xFF;
    gpio_put(_APPLE2_OE3_PIN, 1);

    // printf("get data: %02x\n", data);

    return data;
}

static void _m6502_set_data(uint8_t data) {
    gpio_set_dir_masked(_APPLE2_GPIO_MASK, _APPLE2_GPIO_MASK);

    gpio_put_masked(_APPLE2_GPIO_MASK, data << _APPLE2_GPIO_SHIFT_BITS);
    gpio_put(_APPLE2_OE3_PIN, 0);
#ifndef PICO_NEO6502
    __asm volatile("nop\n");
    __asm volatile("nop\n");
#endif
    gpio_put(_APPLE2_OE3_PIN, 1);

    // printf("set data: %02x\n", data);
}

void apple2_tick(apple2_t *sys) {
    gpio_put(_APPLE2_CLOCK_PIN, 0);

    uint16_t addr = _m6502_get_address();

    bool rw = gpio_get(_APPLE2_RW_PIN);

    gpio_put(_APPLE2_CLOCK_PIN, 1);

    if ((addr >= 0xC000) && (addr <= 0xC0FF)) {
        // Apple II I/O Page
        switch (addr) {
            case 0xC000:
                if (rw) {
                    if (sys->last_key_code != 0) {
                        _m6502_set_data(sys->last_key_code);
                    }
                }
                break;

            case 0xC010:
                sys->last_key_code &= 0x7F;
                break;

            case 0xC030:
                beeper_toggle(&sys->beeper);
                break;

            case 0xC050:
                sys->text = false;
                break;

            case 0xC051:
                sys->text = true;
                break;

            case 0xC052:
                sys->mix = false;
                break;

            case 0xC053:
                sys->mix = true;
                break;

            case 0xC054:
                sys->page2 = false;
                break;

            case 0xC055:
                sys->page2 = true;
                break;

            case 0xC056:
                sys->hires = false;
                break;

            case 0xC057:
                sys->hires = true;
                break;

            default:
                if ((addr >= 0xC080) && (addr <= 0xC08F)) {
                    // Apple II 16K Language Card
                    apple2_lc_control(&sys->lc, addr & 0xF, rw);
                    if (rw) {
                        _m6502_set_data(0xFF);
                    }
                } else if ((addr >= 0xC0E0) && (addr <= 0xC0EF)) {
                    // Disk II FDC
                    if (sys->fdc.valid) {
                        if (rw) {
                            // Memory read
                            _m6502_set_data(disk2_fdc_read_byte(&sys->fdc, addr & 0xF));
                        } else {
                            // Memory write
                            disk2_fdc_write_byte(&sys->fdc, addr & 0xF, _m6502_get_data());
                        }
                    } else {
                        if (rw) {
                            _m6502_set_data(0x00);
                        }
                    }
                } else if ((addr >= 0xC0F0) && (addr <= 0xC0FF)) {
                    // ProDOS HDC
                    if (sys->hdc.valid) {
                        if (rw) {
                            // Memory read
                            _m6502_set_data(prodos_hdc_read_byte(&sys->hdc, addr & 0xF));
                        } else {
                            // Memory write
                            prodos_hdc_write_byte(&sys->hdc, addr & 0xF, _m6502_get_data(), &sys->mem);
                        }
                    } else {
                        if (rw) {
                            _m6502_set_data(0x00);
                        }
                    }
                }
                break;
        }
    } else if ((addr >= 0xC600) && (addr <= 0xC6FF)) {
        if (sys->fdc.valid) {
            // Disk II boot rom
            if (rw) {
                // Memory read
                _m6502_set_data(sys->fdc_rom[addr & 0xFF]);
            }
        } else {
            if (rw) {
                _m6502_set_data(0x00);
            }
        }
    } else if ((addr >= 0xC700) && (addr <= 0xC7FF)) {
        if (sys->hdc.valid) {
            // Hard disk boot rom
            if (rw) {
                // Memory read
                _m6502_set_data(sys->hdc_rom[addr & 0xFF]);
            }
        } else {
            if (rw) {
                _m6502_set_data(0x00);
            }
        }
    } else {
        // Regular memory access
        if (rw) {
            // Memory read
            _m6502_set_data(mem_rd(&sys->mem, addr));
        } else {
            // Memory write
            mem_wr(&sys->mem, addr, _m6502_get_data());
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

    // Update beeper
    if (beeper_tick(&sys->beeper)) {
        // New audio sample ready
        // sys->audio.sample_buffer[sys->audio.sample_pos++] = (uint8_t)((sys->beeper.sample * 0.5f + 0.5f) *
        // 255.0f);
        sys->audio.sample_buffer[sys->audio.sample_pos++] = (uint8_t)(sys->beeper.sample * 255.0f);
        if (sys->audio.sample_pos == sys->audio.num_samples) {
            if (sys->audio.callback.func) {
                // New sample packet is ready
                sys->audio.callback.func(sys->audio.sample_buffer, sys->audio.num_samples,
                                         sys->audio.callback.user_data);
            }
            sys->audio.sample_pos = 0;
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
                sys->text_page1_dirty = true;
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
        sys->ram[addr + 1] = 0xff;
    }
    mem_map_ram(&sys->mem, 0, 0x0000, 0xC000, sys->ram);
}

void apple2_key_down(apple2_t *sys, int key_code) {
    CHIPS_ASSERT(sys && sys->valid);
    if (key_code == 0x150) {
        key_code = 0x08;
    } else if (key_code == 0x14F) {
        key_code = 0x15;
    }
    switch (key_code) {
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
                uint8_t index = key_code - 0x13A;
                if (ARRAY_SIZE(apple2_nib_images) > index) {
                    disk2_fdd_insert_disk(&sys->fdc.fdd[0], apple2_nib_images[index]);
                }
            }
            break;
        }

        case 0x145:  // F12
            apple2_reset(sys);
            break;

        default:
            if (key_code < 128) {
                sys->last_key_code = key_code | 0x80;
            }
            break;
    }
    // printf("key down: %d\n", key_code);
    // kbd_key_down(&sys->kbd, key_code);
}

void apple2_key_up(apple2_t *sys, int key_code) {
    CHIPS_ASSERT(sys && sys->valid);
    (void)key_code;
    // kbd_key_up(&sys->kbd, key_code);
}

uint32_t apple2_save_snapshot(apple2_t *sys, apple2_t *dst) {
    CHIPS_ASSERT(sys && dst);
    *dst = *sys;
    chips_debug_snapshot_onsave(&dst->debug);
    chips_audio_callback_snapshot_onsave(&dst->audio.callback);
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
    chips_audio_callback_snapshot_onload(&im.audio.callback, &sys->audio.callback);
    // m6502_snapshot_onload(&im.cpu, &sys->cpu);
    disk2_fdc_snapshot_onload(&im.fdc, &sys->fdc);
    mem_snapshot_onload(&im.mem, sys);
    *sys = im;
    return true;
}

static inline uint32_t _apple2_rotl4b(uint32_t n, uint32_t count) { return (n >> (-count & 3)) & 0x0F; }

static uint16_t _apple2_double_7_bits(uint8_t bits) {
    uint16_t result = 0;
    for (int i = 6; i >= 0; i--) {
        result <<= 1;
        uint8_t bit = bits & (1 << i) ? 1 : 0;
        result |= bit;
        result <<= 1;
        result |= bit;
    }
    return result;
}

static uint8_t _apple2_reverse_7_bits(uint8_t bits) {
    uint8_t result = 0;
    for (int i = 0; i < 7; i++) {
        result <<= 1;
        result |= bits & 1;
        bits >>= 1;
    }
    return result;
}

static void _apple2_render_line_monochrome(uint8_t *out, uint16_t *in, int start_col, int stop_col) {
    uint32_t w = in[start_col];

    for (int col = start_col; col < stop_col; col++) {
        if (col + 1 < 40) {
            w += in[col + 1] << 14;
        }

        // for (int b = 0; b < 14; b++) {
        //     out[col * 14 + b] = (w & 1) ? 15 : 0;
        //     w >>= 1;
        // }

        for (int b = 0; b < 7; b++) {
            uint8_t c1 = (w & 1) ? 15 : 0;
            w >>= 1;
            uint8_t c2 = (w & 1) ? 15 : 0;
            w >>= 1;
            out[col * 7 + b] = (c1 << 4) | (c2 & 0x0F);
        }
    }
}

static void _apple2_render_line_color(uint8_t *out, uint16_t *in, int start_col, int stop_col) {
    uint32_t w = in[start_col] << 3;

    for (int col = start_col; col < stop_col; col++) {
        if (col + 1 < 40) {
            w += in[col + 1] << 17;
        }

        // for (int b = 0; b < 14; b++) {
        //     out[col * 14 + b] = _apple2_rotl4b(_apple2_artifact_color_lut[w & 0x7F], col * 14 + b);
        //     w >>= 1;
        // }

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

    for (int row = start_row; row <= stop_row; row += 4) {
        uint16_t address = start_address + ((((row / 8) & 0x07) << 7) | (((row / 8) & 0x18) * 5));
        uint8_t *vram = sys->ram + address;

#define NIBBLE(byte) (((byte) >> (row & 4)) & 0x0F)
        uint8_t *p = _apple2_get_fb_addr(sys, row);

        for (int col = 0; col < 40; col++) {
            uint8_t c = NIBBLE(vram[col]);
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

        uint16_t words[40];

        for (int col = 0; col < 40; col++) {
            uint16_t word = _apple2_double_7_bits(_apple2_get_text_character(sys, sys->ram[address + col], row & 7));
            words[col] = word;
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
            uint16_t word = _apple2_double_7_bits(vram_row[col] & 0x7F);
            if (vram_row[col] & 0x80) word = (word * 2 + last_output_bit) & 0x3FFF;
            words[col] = word;
            last_output_bit = word >> 13;
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
        text_start_row = 192 - (sys->mix ? 32 : 0);

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
