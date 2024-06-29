#pragma once

// ay38910psg.h
//
// AY-3-8910/2/3 sound chip emulator
//
// ## zlib/libpng license
//
// Copyright (c) 2023 Veselin Sladkov
// Copyright (c) 2018 Andre Weissflog
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

#ifdef __cplusplus
extern "C" {
#endif

// Pin definitions.
//
// Note that the BC2 is not emulated since it is usually always
// set to active when not connected to a CP1610 processor. The
// remaining BDIR and BC1 pins are interpreted as follows:
//
// |BDIR|BC1|
// +----+---+
// |  0 | 0 |  INACTIVE
// |  0 | 1 |  READ FROM PSG
// |  1 | 0 |  WRITE TO PSG
// |  1 | 1 |  LATCH ADDRESS

// AY-3-8910 registers
#define AY38910PSG_REG_PERIOD_A_FINE     (0)
#define AY38910PSG_REG_PERIOD_A_COARSE   (1)
#define AY38910PSG_REG_PERIOD_B_FINE     (2)
#define AY38910PSG_REG_PERIOD_B_COARSE   (3)
#define AY38910PSG_REG_PERIOD_C_FINE     (4)
#define AY38910PSG_REG_PERIOD_C_COARSE   (5)
#define AY38910PSG_REG_PERIOD_NOISE      (6)
#define AY38910PSG_REG_ENABLE            (7)
#define AY38910PSG_REG_AMP_A             (8)
#define AY38910PSG_REG_AMP_B             (9)
#define AY38910PSG_REG_AMP_C             (10)
#define AY38910PSG_REG_ENV_PERIOD_FINE   (11)
#define AY38910PSG_REG_ENV_PERIOD_COARSE (12)
#define AY38910PSG_REG_ENV_SHAPE_CYCLE   (13)
#define AY38910PSG_REG_IO_PORT_A         (14)  // Not on AY-3-8913
#define AY38910PSG_REG_IO_PORT_B         (15)  // Not on AY-3-8912/3
// Number of registers
#define AY38910PSG_NUM_REGISTERS (16)
// Error-accumulation precision boost
#define AY38910PSG_FIXEDPOINT_SCALE (16)
// Number of channels
#define AY38910PSG_NUM_CHANNELS (3)
// DC adjustment buffer length
#define AY38910PSG_DCADJ_BUFLEN (512)

// IO port names
#define AY38910PSG_PORT_A (0)
#define AY38910PSG_PORT_B (1)

// Envelope shape bits
#define AY38910PSG_ENV_HOLD      (1 << 0)
#define AY38910PSG_ENV_ALTERNATE (1 << 1)
#define AY38910PSG_ENV_ATTACK    (1 << 2)
#define AY38910PSG_ENV_CONTINUE  (1 << 3)

// Callbacks for input/output on I/O ports
// FIXME: These should be integrated into the tick function eventually
typedef uint8_t (*ay38910psg_in_t)(int port_id, void* user_data);
typedef void (*ay38910psg_out_t)(int port_id, uint8_t data, void* user_data);

// Chip subtypes
typedef enum { AY38910PSG_TYPE_8910 = 0, AY38910PSG_TYPE_8912, AY38910PSG_TYPE_8913 } ay38910psg_type_t;

// Setup parameters for ay38910psg_init() call
typedef struct {
    ay38910psg_type_t type;   // Chip type (default 0 is AY-3-8910)
    float magnitude;          // Output sample magnitude, from 0.0 (silence) to 1.0 (max volume)
    ay38910psg_in_t in_cb;    // I/O port input callback
    ay38910psg_out_t out_cb;  // I/O port output callback
    void* user_data;          // Optional user-data for callbacks
} ay38910psg_desc_t;

// Tone channel
typedef struct {
    uint16_t period;
    uint16_t counter;
    uint32_t bit;
    uint32_t tone_disable;
    uint32_t noise_disable;
} ay38910psg_tone_t;

// Noise channel
typedef struct {
    uint16_t period;
    uint16_t counter;
    uint32_t rng;
    uint32_t bit;
} ay38910psg_noise_t;

// Envelope generator
typedef struct {
    uint16_t period;
    uint16_t counter;
    bool shape_holding;
    bool shape_hold;
    uint8_t shape_counter;
    uint8_t shape_state;
} ay38910psg_env_t;

// AY-3-8910
typedef struct {
    ay38910psg_type_t type;   // Chip type
    ay38910psg_in_t in_cb;    // Port-input callback
    ay38910psg_out_t out_cb;  // Port-output callback
    void* user_data;          // Optional user-data for callbacks
    uint8_t addr;             // 4-bit address latch
    union {                   // Register bank
        uint8_t reg[AY38910PSG_NUM_REGISTERS];
        struct {
            uint8_t period_a_fine;
            uint8_t period_a_coarse;
            uint8_t period_b_fine;
            uint8_t period_b_coarse;
            uint8_t period_c_fine;
            uint8_t period_c_coarse;
            uint8_t period_noise;
            uint8_t enable;
            uint8_t amp_a;
            uint8_t amp_b;
            uint8_t amp_c;
            uint8_t period_env_fine;
            uint8_t period_env_coarse;
            uint8_t env_shape_cycle;
            uint8_t port_a;
            uint8_t port_b;
        };
    };
    ay38910psg_tone_t tone[AY38910PSG_NUM_CHANNELS];  // 3 tone channels
    ay38910psg_noise_t noise;                         // Noise generator
    ay38910psg_env_t env;                             // Envelope generator

    // Sample generation
    float mag;
    float sample;
    float dcadj_sum;
    uint32_t dcadj_pos;
    float dcadj_buf[AY38910PSG_DCADJ_BUFLEN];
} ay38910psg_t;

// Initialize AY-3-8910 instance
void ay38910psg_init(ay38910psg_t* c, const ay38910psg_desc_t* desc);
// Reset AY-3-8910 instance
void ay38910psg_reset(ay38910psg_t* c);

void ay38910psg_tick_channels(ay38910psg_t* c);

void ay38910psg_tick_envelope_generator(ay38910psg_t* c);

void ay38910psg_tick_sample_generator(ay38910psg_t* c);

uint8_t ay38910psg_read(ay38910psg_t* c);

void ay38910psg_write(ay38910psg_t* c, uint8_t data);

void ay38910psg_latch_address(ay38910psg_t* c, uint8_t data);

// Prepare ay38910psg_t snapshot for saving
void ay38910psg_snapshot_onsave(ay38910psg_t* snapshot);
// Fixup ay38910psg_t snapshot after loading
void ay38910psg_snapshot_onload(ay38910psg_t* snapshot, ay38910psg_t* sys);

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

// Register width bit masks
static const uint8_t _ay38910psg_reg_mask[AY38910PSG_NUM_REGISTERS] = {
    0xFF,  // AY38910PSG_REG_PERIOD_A_FINE
    0x0F,  // AY38910PSG_REG_PERIOD_A_COARSE
    0xFF,  // AY38910PSG_REG_PERIOD_B_FINE
    0x0F,  // AY38910PSG_REG_PERIOD_B_COARSE
    0xFF,  // AY38910PSG_REG_PERIOD_C_FINE
    0x0F,  // AY38910PSG_REG_PERIOD_C_COARSE
    0x1F,  // AY38910PSG_REG_PERIOD_NOISE
    0xFF,  // AY38910PSG_REG_ENABLE
    0x1F,  // AY38910PSG_REG_AMP_A (0..3: 4-bit volume, 4: use envelope)
    0x1F,  // AY38910PSG_REG_AMP_B (0..3: 4-bit volume, 4: use envelope)
    0x1F,  // AY38910PSG_REG_AMP_C (0..3: 4-bit volume, 4: use envelope)
    0xFF,  // AY38910PSG_REG_ENV_PERIOD_FINE
    0xFF,  // AY38910PSG_REG_ENV_PERIOD_COARSE
    0x0F,  // AY38910PSG_REG_ENV_SHAPE_CYCLE
    0xFF,  // AY38910PSG_REG_IO_PORT_A
    0xFF,  // AY38910PSG_REG_IO_PORT_B
};

// Volume table from: https://github.com/true-grue/ayumi/blob/master/ayumi.c
static const float _ay38910psg_volumes[16] = {0.0f,
                                              0.00999465934234f,
                                              0.0144502937362f,
                                              0.0210574502174f,
                                              0.0307011520562f,
                                              0.0455481803616f,
                                              0.0644998855573f,
                                              0.107362478065f,
                                              0.126588845655f,
                                              0.20498970016f,
                                              0.292210269322f,
                                              0.372838941024f,
                                              0.492530708782f,
                                              0.635324635691f,
                                              0.805584802014f,
                                              1.0f};

// Canned envelope generator shapes
static const uint8_t _ay38910psg_shapes[16][32] = {
    // CONTINUE ATTACK ALTERNATE HOLD
    // 0 0 X X
    {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // 0 1 X X
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // 1 0 0 0
    {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
    // 1 0 0 1
    {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // 1 0 1 0
    {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    // 1 0 1 1
    {15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
     15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
    // 1 1 0 0
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    // 1 1 0 1
    {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
     15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
    // 1 1 1 0
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
    // 1 1 1 1
    {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

// DC adjustment filter from StSound, this moves an "offcenter"
// signal back to the zero-line (e.g. the volume-level output
// from the chip simulation which is >0.0 gets converted to
// a +/- sample value)

static float _ay38910psg_dcadjust(ay38910psg_t* c, float s) {
    c->dcadj_sum -= c->dcadj_buf[c->dcadj_pos];
    c->dcadj_sum += s;
    c->dcadj_buf[c->dcadj_pos] = s;
    c->dcadj_pos = (c->dcadj_pos + 1) & (AY38910PSG_DCADJ_BUFLEN - 1);
    return s - (c->dcadj_sum / AY38910PSG_DCADJ_BUFLEN);
}

// Update computed values after registers have been reprogrammed
static void _ay38910psg_update_values(ay38910psg_t* c) {
    for (int i = 0; i < AY38910PSG_NUM_CHANNELS; i++) {
        ay38910psg_tone_t* chn = &c->tone[i];
        // "...Note also that due to the design technique used in the Tone Period
        // count-down, the lowest period value is 000000000001 (divide by 1)
        // and the highest period value is 111111111111 (divide by 4095)

        chn->period = (c->reg[2 * i + 1] << 8) | (c->reg[2 * i]);
        if (0 == chn->period) {
            chn->period = 1;
        }
        // Set 'enable bit' actually means 'disabled'
        chn->tone_disable = (c->enable >> i) & 1;
        chn->noise_disable = (c->enable >> (3 + i)) & 1;
    }
    // Noise generator values
    c->noise.period = c->period_noise;
    if (c->noise.period == 0) {
        c->noise.period = 1;
    }
    // Envelope generator values
    c->env.period = (c->period_env_coarse << 8) | c->period_env_fine;
    if (c->env.period == 0) {
        c->env.period = 1;
    }
}

// Reset the env shape generator, only called when env-shape register is updated
static void _ay38910psg_restart_env_shape(ay38910psg_t* c) {
    c->env.shape_holding = false;
    c->env.shape_counter = 0;
    if (!(c->env_shape_cycle & AY38910PSG_ENV_CONTINUE) || (c->env_shape_cycle & AY38910PSG_ENV_HOLD)) {
        c->env.shape_hold = true;
    } else {
        c->env.shape_hold = false;
    }
}

void ay38910psg_init(ay38910psg_t* c, const ay38910psg_desc_t* desc) {
    CHIPS_ASSERT(c && desc);
    memset(c, 0, sizeof(*c));
    // Note: input and output callbacks are optional
    c->in_cb = desc->in_cb;
    c->out_cb = desc->out_cb;
    c->user_data = desc->user_data;
    c->type = desc->type;
    c->noise.rng = 1;
    c->mag = desc->magnitude;
    _ay38910psg_update_values(c);
    _ay38910psg_restart_env_shape(c);
}

void ay38910psg_reset(ay38910psg_t* c) {
    CHIPS_ASSERT(c);
    c->addr = 0;
    for (int i = 0; i < AY38910PSG_NUM_REGISTERS; i++) {
        c->reg[i] = 0;
    }
    _ay38910psg_update_values(c);
    _ay38910psg_restart_env_shape(c);
}

void ay38910psg_tick_channels(ay38910psg_t* c) {
    // Tick the tone channels
    for (int i = 0; i < AY38910PSG_NUM_CHANNELS; i++) {
        ay38910psg_tone_t* chn = &c->tone[i];
        chn->counter += 8;
        if (chn->counter >= chn->period) {
            chn->counter = 0;
            chn->bit ^= 1;
        }
    }

    // Tick the noise channel
    c->noise.counter += 8;
    if (c->noise.counter >= c->noise.period) {
        c->noise.counter = 0;
        c->noise.bit ^= 1;
        if (c->noise.bit) {
            // Random number generator from MAME:
            // https://github.com/mamedev/mame/blob/master/src/devices/sound/ay8910.cpp
            // The Random Number Generator of the 8910 is a 17-bit shift
            // register. The input to the shift register is bit0 XOR bit3
            // (bit0 is the output). This was verified on AY-3-8910 and YM2149 chips.
            c->noise.rng ^= (((c->noise.rng & 1) ^ ((c->noise.rng >> 3) & 1)) << 17);
            c->noise.rng >>= 1;
        }
    }
}

void ay38910psg_tick_envelope_generator(ay38910psg_t* c) {
    // Tick the envelope generator
    c->env.counter += 8;
    if (c->env.counter >= c->env.period) {
        c->env.counter = 0;
        if (!c->env.shape_holding) {
            c->env.shape_counter = (c->env.shape_counter + 1) & 0x1F;
            if (c->env.shape_hold && (0x1F == c->env.shape_counter)) {
                c->env.shape_holding = true;
            }
        }
        c->env.shape_state = _ay38910psg_shapes[c->env_shape_cycle][c->env.shape_counter];
    }
}

void ay38910psg_tick_sample_generator(ay38910psg_t* c) {
    float sm = 0.0f;
    for (int i = 0; i < AY38910PSG_NUM_CHANNELS; i++) {
        const ay38910psg_tone_t* chn = &c->tone[i];
        int vol_enable = (chn->bit | chn->tone_disable) & ((c->noise.rng & 1) | (chn->noise_disable));
        if (vol_enable) {
            float vol;
            if (0 == (c->reg[AY38910PSG_REG_AMP_A + i] & (1 << 4))) {
                // Fixed amplitude
                vol = _ay38910psg_volumes[c->reg[AY38910PSG_REG_AMP_A + i] & 0x0F];
            } else {
                // Envelope control
                vol = _ay38910psg_volumes[c->env.shape_state];
            }
            sm += vol;
        }
    }
    // c->sample = _ay38910psg_dcadjust(c, sm) * c->mag;
    c->sample = sm * c->mag;
}

uint8_t ay38910psg_read(ay38910psg_t* c) {
    // Read from register using the currently latched address.
    // See 'write' for why the latched address must be in the
    // valid register range to have an effect.

    if (c->addr < AY38910PSG_NUM_REGISTERS) {
        // Handle port input:

        // If port A or B is in input mode, first call the port
        // input callback to update the port register content.

        // input/output mode is defined by bits 6 and 7 of
        // the 'enable' register:
        //     bit6 = 0: port A in input mode
        //     bit7 = 0: port B in input mode

        if (c->addr == AY38910PSG_REG_IO_PORT_A) {
            if ((c->enable & (1 << 6)) == 0) {
                if (c->in_cb) {
                    c->port_a = c->in_cb(AY38910PSG_PORT_A, c->user_data);
                } else {
                    c->port_a = 0xFF;
                }
            }
        } else if (c->addr == AY38910PSG_REG_IO_PORT_B) {
            if ((c->enable & (1 << 7)) == 0) {
                if (c->in_cb) {
                    c->port_b = c->in_cb(AY38910PSG_PORT_B, c->user_data);
                } else {
                    c->port_b = 0xFF;
                }
            }
        }

        return c->reg[c->addr];
    } else {
        return 0xFF;
    }
}

void ay38910psg_write(ay38910psg_t* c, uint8_t data) {
    // Write to register using the currently latched address.
    // The whole 8-bit address is considered, the low 4 bits
    // are the register index, and the upper bits are burned
    // into the chip as a 'chip select' and are usually 0
    // (this emulator assumes they are 0, so addresses greater
    // are ignored for reading and writing)

    if (c->addr < AY38910PSG_NUM_REGISTERS) {
        // write register content, and update dependent values
        c->reg[c->addr] = data & _ay38910psg_reg_mask[c->addr];
        _ay38910psg_update_values(c);
        if (c->addr == AY38910PSG_REG_ENV_SHAPE_CYCLE) {
            _ay38910psg_restart_env_shape(c);
        }
        // Handle port output:
        //
        // If port A or B is in output mode, call the
        // port output callback to notify the outer world
        // about the new register value.
        //
        // input/output mode is defined by bits 6 and 7 of
        // the 'enable' register
        //     bit6 = 1: port A in output mode
        //     bit7 = 1: port B in output mode

        else if (c->addr == AY38910PSG_REG_IO_PORT_A) {
            if (c->enable & (1 << 6)) {
                if (c->out_cb) {
                    c->out_cb(AY38910PSG_PORT_A, c->port_a, c->user_data);
                }
            }
        } else if (c->addr == AY38910PSG_REG_IO_PORT_B) {
            if (c->enable & (1 << 7)) {
                if (c->out_cb) {
                    c->out_cb(AY38910PSG_PORT_B, c->port_b, c->user_data);
                }
            }
        }
    }
}

void ay38910psg_latch_address(ay38910psg_t* c, uint8_t data) { c->addr = data; }

void ay38910psg_snapshot_onsave(ay38910psg_t* snapshot) {
    CHIPS_ASSERT(snapshot);
    snapshot->in_cb = 0;
    snapshot->out_cb = 0;
    snapshot->user_data = 0;
}

void ay38910psg_snapshot_onload(ay38910psg_t* snapshot, ay38910psg_t* sys) {
    CHIPS_ASSERT(snapshot && sys);
    snapshot->in_cb = sys->in_cb;
    snapshot->out_cb = sys->out_cb;
    snapshot->user_data = sys->user_data;
}
#endif  // CHIPS_IMPL
