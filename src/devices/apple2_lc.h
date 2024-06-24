#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APPLE2_LC_READ_ENABLED  (1)
#define APPLE2_LC_WRITE_ENABLED (2)

// Config parameters for apple2_lc_init()
typedef struct {
    mem_t* sys_mem;
    uint8_t* sys_rom;
} apple2_lc_desc_t;

// Apple II language card state
typedef struct {
    bool valid;

    mem_t* sys_mem;
    uint8_t* sys_rom;

    uint8_t ram[0x4000];

    uint16_t current_bank;
    uint8_t state;
    bool prewrite;
} apple2_lc_t;

// Apple II language card controller interface

// Initialize a new language card
void apple2_lc_init(apple2_lc_t* dev, const apple2_lc_desc_t* desc);

// Discard the language card
void apple2_lc_discard(apple2_lc_t* dev);

// Reset the language card
void apple2_lc_reset(apple2_lc_t* dev);

void apple2_lc_control(apple2_lc_t* dev, uint8_t offset, bool rw);

// Prepare a new language card snapshot for saving
void apple2_lc_snapshot_onsave(apple2_lc_t* snapshot);

// Fix up the language card snapshot after loading
void apple2_lc_snapshot_onload(apple2_lc_t* snapshot, apple2_lc_t* dev);

#ifdef __cplusplus
} // extern "C"
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>  // memcpy, memset
#ifndef CHIPS_ASSERT
#include <assert.h>
#define CHIPS_ASSERT(c) assert(c)
#endif

void apple2_lc_init(apple2_lc_t* dev, const apple2_lc_desc_t* desc) {
    CHIPS_ASSERT(dev && !dev->valid);
    memset(dev, 0, sizeof(apple2_lc_t));
    dev->sys_mem = desc->sys_mem;
    dev->sys_rom = desc->sys_rom;
    dev->valid = true;
    dev->current_bank = 0x1000;
    dev->state &= ~APPLE2_LC_READ_ENABLED;
    dev->state |= APPLE2_LC_WRITE_ENABLED;
    dev->prewrite = false;
    mem_map_rw(dev->sys_mem, 0, 0xD000, 0x1000, dev->sys_rom, dev->ram + dev->current_bank);
    mem_map_rw(dev->sys_mem, 0, 0xE000, 0x2000, dev->sys_rom + 0x1000, dev->ram + 0x2000);
}

void apple2_lc_discard(apple2_lc_t* dev) {
    CHIPS_ASSERT(dev && dev->valid);
    dev->valid = false;
}

void apple2_lc_reset(apple2_lc_t* dev) { CHIPS_ASSERT(dev && dev->valid); }

void apple2_lc_control(apple2_lc_t* dev, uint8_t offset, bool rw) {
    CHIPS_ASSERT(dev && dev->valid);
    if ((offset & 1) == 0) {
        dev->prewrite = false;
        dev->state &= ~APPLE2_LC_WRITE_ENABLED;
    }

    if (!rw) {
        dev->prewrite = false;
    } else if ((offset & 1) == 1) {
        if (dev->prewrite == false) {
            dev->prewrite = true;
        } else {
            dev->state |= APPLE2_LC_WRITE_ENABLED;
        }
    }

    switch (offset & 3) {
        case 0:
        case 3: {
            dev->state |= APPLE2_LC_READ_ENABLED;
            break;
        }

        case 1:
        case 2: {
            dev->state &= ~APPLE2_LC_READ_ENABLED;
            break;
        }
    }

    if (!(offset & 8)) {
        dev->current_bank = 0x1000;
    } else {
        dev->current_bank = 0;
    }

    if (dev->state & APPLE2_LC_READ_ENABLED) {
        if (dev->state & APPLE2_LC_WRITE_ENABLED) {
            mem_map_ram(dev->sys_mem, 0, 0xD000, 0x1000, dev->ram + dev->current_bank);
            mem_map_ram(dev->sys_mem, 0, 0xE000, 0x2000, dev->ram + 0x2000);
        } else {
            mem_map_rom(dev->sys_mem, 0, 0xD000, 0x1000, dev->ram + dev->current_bank);
            mem_map_rom(dev->sys_mem, 0, 0xE000, 0x2000, dev->ram + 0x2000);
        }
    } else {
        if (dev->state & APPLE2_LC_WRITE_ENABLED) {
            mem_map_rw(dev->sys_mem, 0, 0xD000, 0x1000, dev->sys_rom, dev->ram + dev->current_bank);
            mem_map_rw(dev->sys_mem, 0, 0xE000, 0x2000, dev->sys_rom + 0x1000, dev->ram + 0x2000);
        } else {
            mem_map_rom(dev->sys_mem, 0, 0xD000, 0x3000, dev->sys_rom);
        }
    }
}

void apple2_lc_snapshot_onsave(apple2_lc_t* snapshot) { CHIPS_ASSERT(snapshot); }

void apple2_lc_snapshot_onload(apple2_lc_t* snapshot, apple2_lc_t* dev) { CHIPS_ASSERT(snapshot && dev); }

#endif  // CHIPS_IMPL
