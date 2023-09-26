#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Disk II FDC softswitches
#define DISK2_FDC_PHASE0_OFF    (0x00)  // turn stepper motor phase 0 off
#define DISK2_FDC_PHASE0_ON     (0x01)  // turn stepper motor phase 0 on
#define DISK2_FDC_PHASE1_OFF    (0x02)  // turn stepper motor phase 1 off
#define DISK2_FDC_PHASE1_ON     (0x03)  // turn stepper motor phase 1 on
#define DISK2_FDC_PHASE2_OFF    (0x04)  // turn stepper motor phase 2 off
#define DISK2_FDC_PHASE2_ON     (0x05)  // turn stepper motor phase 2 on
#define DISK2_FDC_PHASE3_OFF    (0x06)  // turn stepper motor phase 3 off
#define DISK2_FDC_PHASE3_ON     (0x07)  // turn stepper motor phase 3 on
#define DISK2_FDC_MOTOR_OFF     (0x08)  // turn motor off
#define DISK2_FDC_MOTOR_ON      (0x09)  // turn motor on
#define DISK2_FDC_SELECT_DRIVE1 (0x0A)  // select drive 1
#define DISK2_FDC_SELECT_DRIVE2 (0x0B)  // select drive 2

//  Q6      Q7      Function
//  -------------------------------------------------
//  L       L       Read (disk to shift register)
//  L       H       Write (shift register to disk)
//  H       L       Sense write protect
//  H       H       Load shift register from data bus
//  -------------------------------------------------
#define DISK2_FDC_Q6L (0x0C)  // Q6 low
#define DISK2_FDC_Q6H (0x0D)  // Q6 high
#define DISK2_FDC_Q7L (0x0E)  // Q7 low
#define DISK2_FDC_Q7H (0x0F)  // Q7 high

// Helper macros to set and extract address and data to/from pin mask

// Extract 4-bit address bus from 64-bit pins
#define DISK2_FDC_GET_ADDR(p) ((uint16_t)(p & 0xFFFFULL))
// Merge 4-bit address bus value into 64-bit pins
#define DISK2_FDC_SET_ADDR(p, a) \
    { p = ((p & ~0xFFFFULL) | ((a)&0xFFFFULL)); }
// Extract 8-bit data bus from 64-bit pins
#define DISK2_FDC_GET_DATA(p) ((uint8_t)(((p)&0xFF0000ULL) >> 16))
// Merge 8-bit data bus value into 64-bit pins
#define DISK2_FDC_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }

// Floppy disk controller port bits
#define DISK2_FDC_PORT_READ  (1 << 0)
#define DISK2_FDC_PORT_WRITE (1 << 1)

#define DISK2_FDC_MAX_DRIVES 1

// Oric floppy disk controller state
typedef struct {
    bool valid;
    uint64_t pins;
    uint8_t selected_drive;
    disk2_fdd_t fdd[DISK2_FDC_MAX_DRIVES];
} disk2_fdc_t;

// Oric floppy disk controller interface

// Initialize a new floppy disk controller
void disk2_fdc_init(disk2_fdc_t* sys);

// Discard the floppy disk controller
void disk2_fdc_discard(disk2_fdc_t* sys);

// Reset the floppy disk controller
void disk2_fdc_reset(disk2_fdc_t* sys);

// Tick the floppy disk controller
void disk2_fdc_tick(disk2_fdc_t* sys);

bool disk2_fdc_insert_disk(disk2_fdc_t* sys, uint8_t drive, uint8_t* nib_image);

uint8_t disk2_fdc_read_byte(disk2_fdc_t* sys, uint8_t addr);

void disk2_fdc_write_byte(disk2_fdc_t* sys, uint8_t addr, uint8_t byte);

// Prepare a new floppy disk controller snapshot for saving
void disk2_fdc_snapshot_onsave(disk2_fdc_t* snapshot);

// Fix up the floppy disk controller snapshot after loading
void disk2_fdc_snapshot_onload(disk2_fdc_t* snapshot, disk2_fdc_t* sys);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>  // memcpy, memset
#ifndef CHIPS_ASSERT
#include <assert.h>
#define CHIPS_ASSERT(c) assert(c)
#endif

static void _disk2_fdc_process_soft_switches(disk2_fdc_t* sys, uint8_t addr);

void disk2_fdc_init(disk2_fdc_t* sys) {
    CHIPS_ASSERT(sys && !sys->valid);
    memset(sys, 0, sizeof(disk2_fdc_t));
    sys->valid = true;
    disk2_fdd_init(&sys->fdd[0], dump_disk1_nib);
    // disk2_fdd_init(&sys->fdd[1], dump_disk2_nib);
}

void disk2_fdc_discard(disk2_fdc_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
    disk2_fdd_discard(&sys->fdd[0]);
    // disk2_fdd_discard(&sys->fdd[1]);
}

void disk2_fdc_reset(disk2_fdc_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    disk2_fdd_reset(&sys->fdd[0]);
    // disk2_fdd_reset(&sys->fdd[1]);
}

void disk2_fdc_tick(disk2_fdc_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    disk2_fdd_tick(&sys->fdd[0]);
    // disk2_fdd_tick(&sys->fdd[1]);
}

bool disk2_fdc_insert_disk(disk2_fdc_t* sys, uint8_t drive, uint8_t* nib_image) {
    CHIPS_ASSERT(sys && sys->valid);
    if (drive >= DISK2_FDC_MAX_DRIVES) {
        return false;
    }
    return disk2_fdd_insert_disk(&sys->fdd[drive], nib_image);
}

uint8_t disk2_fdc_read_byte(disk2_fdc_t* sys, uint8_t addr) {
    _disk2_fdc_process_soft_switches(sys, addr);
    if (addr & 1) {
        return 0;
    }
    disk2_fdd_t* fdd = &sys->fdd[sys->selected_drive];
    return disk2_fdd_read_byte(fdd);
}

void disk2_fdc_write_byte(disk2_fdc_t* sys, uint8_t addr, uint8_t byte) {
    _disk2_fdc_process_soft_switches(sys, addr);
    if ((addr & 1) == 0) {
        return;
    }
    disk2_fdd_t* fdd = &sys->fdd[sys->selected_drive];
    disk2_fdd_write_byte(fdd, byte);
}

void disk2_fdc_snapshot_onsave(disk2_fdc_t* snapshot) { CHIPS_ASSERT(snapshot); }

void disk2_fdc_snapshot_onload(disk2_fdc_t* snapshot, disk2_fdc_t* sys) { CHIPS_ASSERT(snapshot && sys); }

static void _disk2_fdc_process_soft_switches(disk2_fdc_t* sys, uint8_t addr) {
    disk2_fdd_t* fdd = &sys->fdd[sys->selected_drive];

    switch (addr & 0xF) {
        case DISK2_FDC_MOTOR_OFF:
            fdd->motor_timer_ticks = 1500000 / 128;
            break;

        case DISK2_FDC_MOTOR_ON:
            disk2_fdd_set_motor_on(fdd);
            fdd->motor_timer_ticks = 0;
            break;

        case DISK2_FDC_SELECT_DRIVE1:
        case DISK2_FDC_SELECT_DRIVE2:
            if (sys->selected_drive != (addr & 1)) {
                sys->selected_drive = addr & 1;
            }
            break;

        case DISK2_FDC_Q6L:
            fdd->control_bits &= 0xFE;
            break;

        case DISK2_FDC_Q6H:
            fdd->control_bits |= 0x01;
            break;

        case DISK2_FDC_Q7L:
            fdd->control_bits &= 0xFD;
            break;

        case DISK2_FDC_Q7H:
            fdd->control_bits |= 0x02;
            break;

        default: {
            uint8_t phase = (addr & 0x06) >> 1;

            if (addr & 0x01) {
                phase += 4;
                phase -= (fdd->half_track % 4);
                phase %= 4;

                if (phase == 1) {
                    if (fdd->half_track < (2 * DISK2_FDD_TRACKS_PER_DISK - 2)) {
                        fdd->half_track++;
                    }
                } else if (phase == 3) {
                    if (fdd->half_track > 0) {
                        fdd->half_track--;
                    }
                }
            }
        } break;
    }
}

#endif /* CHIPS_IMPL */
