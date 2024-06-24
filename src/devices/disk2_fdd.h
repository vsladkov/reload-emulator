#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISK2_FDD_TRACKS_PER_DISK   35
#define DISK2_FDD_SECTORS_PER_TRACK 16
#define DISK2_FDD_BYTES_PER_SECTOR  256
#define DISK2_FDD_BYTES_PER_TRACK   (DISK2_FDD_SECTORS_PER_TRACK * DISK2_FDD_BYTES_PER_SECTOR)
#define DISK2_FDD_DSK_IMAGE_SIZE    (DISK2_FDD_TRACKS_PER_DISK * DISK2_FDD_BYTES_PER_TRACK)

#define DISK2_FDD_BYTES_PER_NIB_SECTOR 374
#define DISK2_FDD_BYTES_PER_NIB_TRACK  (DISK2_FDD_SECTORS_PER_TRACK * DISK2_FDD_BYTES_PER_NIB_SECTOR)
#define DISK2_FDD_NIB_IMAGE_SIZE       (DISK2_FDD_TRACKS_PER_DISK * DISK2_FDD_BYTES_PER_NIB_TRACK)

// Disk II floppy disk drive state
typedef struct {
    bool valid;
    uint8_t motor_state;
    uint32_t motor_timer_ticks;
    uint8_t half_track;
    uint16_t offset;
    bool image_dirty;
    bool write_protected;
    bool nib_image_loaded;
    uint8_t control_bits;
    uint8_t write_ready;
    int nib_image_offset;
    uint8_t* nib_image;
} disk2_fdd_t;

// Disk II floppy disk drive interface

// Initialize a new floppy disk drive
void disk2_fdd_init(disk2_fdd_t* sys);

// Discard the floppy disk drive
void disk2_fdd_discard(disk2_fdd_t* sys);

// Reset the floppy disk drive
void disk2_fdd_reset(disk2_fdd_t* sys);

void disk2_fdd_tick(disk2_fdd_t* sys);

// Insert a new disk file
bool disk2_fdd_insert_disk(disk2_fdd_t* sys, uint8_t* nib_image);

// Remove the disk file
void disk2_fdd_remove_disk(disk2_fdd_t* sys);

// Return true if the disk is currently inserted
bool disk2_fdd_is_disk_inserted(disk2_fdd_t* sys);

void disk2_fdd_set_motor_on(disk2_fdd_t* sys);

void disk2_fdd_set_motor_off(disk2_fdd_t* sys);

// Return true if the floppy disk drive motor is on
bool disk2_fdd_is_motor_on(disk2_fdd_t* sys);

uint8_t disk2_fdd_read_byte(disk2_fdd_t* sys);

void disk2_fdd_write_byte(disk2_fdd_t* sys, uint8_t byte);

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

void disk2_fdd_init(disk2_fdd_t* sys) {
    CHIPS_ASSERT(sys && !sys->valid);
    memset(sys, 0, sizeof(disk2_fdd_t));
    sys->valid = true;
    sys->half_track = 0;
    sys->offset = 0;
    sys->image_dirty = false;
    sys->write_protected = true;
    sys->motor_state = 0;
    sys->motor_timer_ticks = 0;
    sys->control_bits = 0;
    sys->write_ready = 0x80;
    sys->nib_image_loaded = false;
}

void disk2_fdd_discard(disk2_fdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void disk2_fdd_reset(disk2_fdd_t* sys) { CHIPS_ASSERT(sys && sys->valid); }

void disk2_fdd_tick(disk2_fdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    if (sys->motor_timer_ticks > 0) {
        sys->motor_timer_ticks--;
        if (sys->motor_timer_ticks == 0) {
            sys->motor_state = 0;
        }
    }
}

bool disk2_fdd_insert_disk(disk2_fdd_t* sys, uint8_t* nib_image) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->nib_image_offset = 0;
    sys->nib_image = nib_image;
    sys->nib_image_loaded = true;
    return true;
}

void disk2_fdd_remove_disk(disk2_fdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->nib_image_loaded = false;
    sys->image_dirty = false;
}

bool disk2_fdd_is_disk_inserted(disk2_fdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return sys->nib_image_loaded;
}

void disk2_fdd_set_motor_on(disk2_fdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->motor_state = 0x20;
}

void disk2_fdd_set_motor_off(disk2_fdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->motor_state = 0;
}

bool disk2_fdd_is_motor_on(disk2_fdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return sys->motor_state != 0;
}

static void _disk2_fdd_update_offset(disk2_fdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    if (sys->motor_state) {
        sys->offset = (sys->offset + 1) % DISK2_FDD_BYTES_PER_NIB_TRACK;
    }
}

uint8_t disk2_fdd_read_byte(disk2_fdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->write_ready = 0x80;
    switch (sys->control_bits) {
        case 0:
            if (!sys->nib_image_loaded) {
                return 0xFF;
            }
            _disk2_fdd_update_offset(sys);
            return *(sys->nib_image + (sys->half_track / 2) * DISK2_FDD_BYTES_PER_NIB_TRACK + sys->offset);

        case 1:
            return sys->write_protected | sys->motor_state;

        case 2:
            return sys->write_ready;

        default:
            return 0;
    }
}

void disk2_fdd_write_byte(disk2_fdd_t* sys, uint8_t byte) {
    CHIPS_ASSERT(sys && sys->valid);

    if (!sys->nib_image_loaded || sys->write_protected || byte < 0x96) {
        return;
    }

    if (sys->motor_state && sys->control_bits == 3 && sys->write_ready) {
        printf("disk2_fdd_write_byte: offset=%d, byte=%02x\n", sys->offset, byte);

        _disk2_fdd_update_offset(sys);
        *(sys->nib_image + (sys->half_track / 2) * DISK2_FDD_BYTES_PER_NIB_TRACK + sys->offset) = byte;
        sys->image_dirty = true;
        sys->write_ready = 0;
    }
}

#endif  // CHIPS_IMPL
