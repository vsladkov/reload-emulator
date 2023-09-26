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

// Max size of the tape image
#define ORIC_TD_MAX_TAPE_SIZE 0  //(512 * 1024)

// Oric tape drive state
typedef struct {
    uint8_t port;
    bool valid;
    uint32_t size;
    uint32_t pos;
    uint32_t bit_pos;
    uint8_t buf[ORIC_TD_MAX_TAPE_SIZE];
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
bool oric_td_insert_tape(oric_td_t* sys, chips_range_t data);

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

static int tick_count = 0;

void oric_td_tick(oric_td_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    // if (tick_count < 208) {
    if (tick_count < 52) {
        tick_count++;
        return;
    }
    if (oric_td_is_motor_on(sys) && (sys->size > 0) && (sys->pos < sys->size)) {
        uint8_t b = sys->buf[sys->pos];
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
    tick_count = 0;
}

static uint8_t _current_level = 0;
static uint8_t _shifter;
static uint8_t _shift_count = 0;

void _flush_output(oric_td_t* sys) {
    for (int i = 0; i < 8 - _shift_count; i++) {
        _shifter = (_shifter << 1) + 1;
    }
    sys->buf[sys->size++] = _shifter;
}

static void _output_half_period(oric_td_t* sys, uint8_t length) {
    for (int i = 0; i < length; i++) {
        _shifter = (_shifter << 1) + _current_level;
        _shift_count++;
        if (_shift_count == 8) {
            _shift_count = 0;
            sys->buf[sys->size++] = _shifter;
        }
    }
    _current_level ^= 1;
}

static void _output_bit(oric_td_t* sys, uint8_t b) {
    _output_half_period(sys, 1);
    _output_half_period(sys, b ? 1 : 2);
}

static void _output_byte(oric_td_t* sys, uint8_t b) {
    uint8_t parity = 1;
    _output_half_period(sys, 1);
    _output_bit(sys, 0);
    for (int i = 0; i < 8; i++) {
        uint8_t bit = b & 1;
        parity += bit;
        _output_bit(sys, bit);
        b >>= 1;
    }
    _output_bit(sys, parity & 1);
    _output_bit(sys, 1);
    _output_bit(sys, 1);
    _output_bit(sys, 1);
}

static bool _find_synchro(chips_range_t data, uint32_t* pos) {
    const uint8_t* ptr = (uint8_t*)data.ptr;
    int synchro_state = 0;
    while (*pos < data.size) {
        uint8_t b = ptr[(*pos)++];
        if (b == 0x16) {
            if (synchro_state < 3) synchro_state++;
        } else if (b == 0x24 && synchro_state == 3) {
            return true;
        } else {
            synchro_state = 0;
        }
    }
    return false;
}

static void _output_big_synchro(oric_td_t* sys) {
    for (int i = 0; i < 259; i++) {
        _output_byte(sys, 0x16);
    }
    _output_byte(sys, 0x24);
}

static bool _output_file(oric_td_t* sys, chips_range_t data, uint32_t* pos) {
    const uint8_t* ptr = (uint8_t*)data.ptr;
    uint8_t header[9];
    uint32_t i;

    i = 0;
    while (*pos < data.size && i < 9) {
        uint8_t b = ptr[(*pos)++];
        header[i++] = b;
        _output_byte(sys, b);
    }
    if (*pos >= data.size) {
        return false;
    }

    uint8_t b;
    while (*pos < data.size && ((b = ptr[(*pos)++]) != 0)) {
        _output_byte(sys, b);
    }
    if (*pos >= data.size) {
        return false;
    }
    _output_byte(sys, 0);

    for (int i = 0; i < 6; i++) {
        _output_half_period(sys, 1);
    }

    uint32_t start = header[6] * 256 + header[7];
    uint32_t end = header[4] * 256 + header[5];
    uint32_t size = end - start + 1;
    i = 0;
    while (*pos < data.size && i < size) {
        uint8_t b = ptr[(*pos)++];
        _output_byte(sys, b);
        i++;
    }
    if (*pos == data.size && i < size) {
        return false;
    }

    for (int i = 0; i < 2; i++) {
        _output_half_period(sys, 1);
    }

    return true;
}

bool oric_td_insert_tape(oric_td_t* sys, chips_range_t data) {
    for (int i = 0; i < 5; i++) {
        _output_half_period(sys, 1);
    }

    uint32_t pos = 0;
    while (pos < data.size) {
        if (_find_synchro(data, &pos)) {
            _output_big_synchro(sys);
            if (!_output_file(sys, data, &pos)) {
                return false;
            }
        }
    }
    _flush_output(sys);

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
