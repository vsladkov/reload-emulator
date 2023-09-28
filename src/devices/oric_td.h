#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Tape drive port bits
#define ORIC_TD_PORT_MOTOR  (1 << 0)
#define ORIC_TD_PORT_READ   (1 << 1)
#define ORIC_TD_PORT_WRITE  (1 << 2)
#define ORIC_TD_PORT_PLAY   (1 << 3)
#define ORIC_TD_PORT_RECORD (1 << 4)

// Oric tape drive state
typedef struct {
    uint8_t port;
    bool valid;
    uint32_t pos;
    uint32_t bit_pos;
    uint32_t size;
    uint8_t* wave_image;
} oric_td_t;

// Oric tape drive interface

// Initialize a new tape drive
void oric_td_init(oric_td_t* sys);

// Discard the tape drive
void oric_td_discard(oric_td_t* sys);

// Reset the tape drive
void oric_td_reset(oric_td_t* sys);

// Tick the tape drive
void oric_td_tick(oric_td_t* sys);

// Insert a new tape file
bool oric_td_insert_tape(oric_td_t* sys, uint8_t* wave_image);

// Remove the tape file
void oric_td_remove_tape(oric_td_t* sys);

// Return true if the tape is currently inserted
bool oric_td_is_tape_inserted(oric_td_t* sys);

// Start playing the tape (press the Play button)
void oric_td_play(oric_td_t* sys);

// Start recording the tape (press the Record button)
void oric_td_record(oric_td_t* sys);

// Stop the tape (press the Stop button)
void oric_td_stop(oric_td_t* sys);

// Return true if the tape drive motor is on
bool oric_td_is_motor_on(oric_td_t* sys);

// Prepare a new tape drive snapshot for saving
void oric_td_snapshot_onsave(oric_td_t* snapshot);

// Fix up the tape drive snapshot after loading
void oric_td_snapshot_onload(oric_td_t* snapshot, oric_td_t* sys);

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

void oric_td_init(oric_td_t* sys) {
    CHIPS_ASSERT(sys && !sys->valid);
    memset(sys, 0, sizeof(oric_td_t));
    sys->valid = true;
    sys->bit_pos = 7;
}

void oric_td_discard(oric_td_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void oric_td_reset(oric_td_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->port = 0;
    sys->size = 0;
    sys->pos = 0;
    sys->bit_pos = 7;
}

void oric_td_tick(oric_td_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    if (oric_td_is_motor_on(sys) && (sys->size > 0) && (sys->pos < sys->size)) {
        uint8_t b = sys->wave_image[sys->pos];
        b >>= sys->bit_pos;
        if (b & 1) {
            sys->port |= ORIC_TD_PORT_READ;
        } else {
            sys->port &= ~ORIC_TD_PORT_READ;
        }
        if (sys->bit_pos == 0) {
            sys->bit_pos = 7;
            sys->pos++;
        } else {
            sys->bit_pos--;
        }
    }
}

bool oric_td_insert_tape(oric_td_t* sys, uint8_t* wave_image) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->size = wave_image[0] | (wave_image[1] << 8) | (wave_image[2] << 16) | (wave_image[3] << 24);
    sys->pos = 0;
    sys->wave_image = &wave_image[4];
    return true;
}

void oric_td_remove_tape(oric_td_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    oric_td_stop(sys);
    sys->size = 0;
    sys->pos = 0;
}

bool oric_td_is_tape_inserted(oric_td_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return sys->size > 0;
}

void oric_td_play(oric_td_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    // motor on, play button down
    sys->port &= (ORIC_TD_PORT_MOTOR | ORIC_TD_PORT_PLAY);
}

void oric_td_record(oric_td_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    // motor on, record button down
    sys->port &= (ORIC_TD_PORT_MOTOR | ORIC_TD_PORT_RECORD);
}

void oric_td_stop(oric_td_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    // Set motor off, play and record buttons up
    sys->port &= ~(ORIC_TD_PORT_MOTOR | ORIC_TD_PORT_PLAY | ORIC_TD_PORT_RECORD);
}

bool oric_td_is_motor_on(oric_td_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return 0 != (sys->port & ORIC_TD_PORT_MOTOR);
}

void oric_td_snapshot_onsave(oric_td_t* snapshot) {
    CHIPS_ASSERT(snapshot);
    snapshot->port = 0;
}

void oric_td_snapshot_onload(oric_td_t* snapshot, oric_td_t* sys) {
    CHIPS_ASSERT(snapshot && sys);
    snapshot->port = sys->port;
}

#endif /* CHIPS_IMPL */
