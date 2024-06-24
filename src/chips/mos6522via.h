#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Register indices
#define MOS6522VIA_REG_RB     (0)   // Input / output register B
#define MOS6522VIA_REG_RA     (1)   // Input / output register A
#define MOS6522VIA_REG_DDRB   (2)   // Data direction B
#define MOS6522VIA_REG_DDRA   (3)   // Data direction A
#define MOS6522VIA_REG_T1CL   (4)   // T1 low-order latch / counter
#define MOS6522VIA_REG_T1CH   (5)   // T1 high-order counter
#define MOS6522VIA_REG_T1LL   (6)   // T1 low-order latches
#define MOS6522VIA_REG_T1LH   (7)   // T1 high-order latches
#define MOS6522VIA_REG_T2CL   (8)   // T2 low-order latch / counter
#define MOS6522VIA_REG_T2CH   (9)   // T2 high-order counter
#define MOS6522VIA_REG_SR     (10)  // Shift register
#define MOS6522VIA_REG_ACR    (11)  // Auxiliary control register
#define MOS6522VIA_REG_PCR    (12)  // Peripheral control register
#define MOS6522VIA_REG_IFR    (13)  // Interrupt flag register
#define MOS6522VIA_REG_IER    (14)  // Interrupt enable register
#define MOS6522VIA_REG_RA_NOH (15)  // Input / output A without handshake

// PCR test macros (MAME naming)
#define MOS6522VIA_PCR_CA1_LOW_TO_HIGH(c)  (c->pcr & 0x01)
#define MOS6522VIA_PCR_CA1_HIGH_TO_LOW(c)  (!(c->pcr & 0x01))
#define MOS6522VIA_PCR_CB1_LOW_TO_HIGH(c)  (c->pcr & 0x10)
#define MOS6522VIA_PCR_CB1_HIGH_TO_LOW(c)  (!(c->pcr & 0x10))
#define MOS6522VIA_PCR_CA2_INPUT(c)        (!(c->pcr & 0x08))
#define MOS6522VIA_PCR_CA2_LOW_TO_HIGH(c)  ((c->pcr & 0x0c) == 0x04)
#define MOS6522VIA_PCR_CA2_HIGH_TO_LOW(c)  ((c->pcr & 0x0c) == 0x00)
#define MOS6522VIA_PCR_CA2_IND_IRQ(c)      ((c->pcr & 0x0a) == 0x02)
#define MOS6522VIA_PCR_CA2_OUTPUT(c)       (c->pcr & 0x08)
#define MOS6522VIA_PCR_CA2_AUTO_HS(c)      ((c->pcr & 0x0c) == 0x08)
#define MOS6522VIA_PCR_CA2_HS_OUTPUT(c)    ((c->pcr & 0x0e) == 0x08)
#define MOS6522VIA_PCR_CA2_PULSE_OUTPUT(c) ((c->pcr & 0x0e) == 0x0a)
#define MOS6522VIA_PCR_CA2_FIX_OUTPUT(c)   ((c->pcr & 0x0c) == 0x0c)
#define MOS6522VIA_PCR_CA2_OUTPUT_LEVEL(c) ((c->pcr & 0x02) >> 1)
#define MOS6522VIA_PCR_CB2_INPUT(c)        (!(c->pcr & 0x80))
#define MOS6522VIA_PCR_CB2_LOW_TO_HIGH(c)  ((c->pcr & 0xc0) == 0x40)
#define MOS6522VIA_PCR_CB2_HIGH_TO_LOW(c)  ((c->pcr & 0xc0) == 0x00)
#define MOS6522VIA_PCR_CB2_IND_IRQ(c)      ((c->pcr & 0xa0) == 0x20)
#define MOS6522VIA_PCR_CB2_OUTPUT(c)       (c->pcr & 0x80)
#define MOS6522VIA_PCR_CB2_AUTO_HS(c)      ((c->pcr & 0xc0) == 0x80)
#define MOS6522VIA_PCR_CB2_HS_OUTPUT(c)    ((c->pcr & 0xe0) == 0x80)
#define MOS6522VIA_PCR_CB2_PULSE_OUTPUT(c) ((c->pcr & 0xe0) == 0xa0)
#define MOS6522VIA_PCR_CB2_FIX_OUTPUT(c)   ((c->pcr & 0xc0) == 0xc0)
#define MOS6522VIA_PCR_CB2_OUTPUT_LEVEL(c) ((c->pcr & 0x20) >> 5)

// ACR test macros (MAME naming)
#define MOS6522VIA_ACR_PA_LATCH_ENABLE(c) (c->acr & 0x01)
#define MOS6522VIA_ACR_PB_LATCH_ENABLE(c) (c->acr & 0x02)
#define MOS6522VIA_ACR_SR_DISABLED(c)     (!(c->acr & 0x1c))
#define MOS6522VIA_ACR_SI_T2_CONTROL(c)   ((c->acr & 0x1c) == 0x04)
#define MOS6522VIA_ACR_SI_O2_CONTROL(c)   ((c->acr & 0x1c) == 0x08)
#define MOS6522VIA_ACR_SI_EXT_CONTROL(c)  ((c->acr & 0x1c) == 0x0c)
#define MOS6522VIA_ACR_SO_T2_RATE(c)      ((c->acr & 0x1c) == 0x10)
#define MOS6522VIA_ACR_SO_T2_CONTROL(c)   ((c->acr & 0x1c) == 0x14)
#define MOS6522VIA_ACR_SO_O2_CONTROL(c)   ((c->acr & 0x1c) == 0x18)
#define MOS6522VIA_ACR_SO_EXT_CONTROL(c)  ((c->acr & 0x1c) == 0x1c)
#define MOS6522VIA_ACR_T1_SET_PB7(c)      (c->acr & 0x80)
#define MOS6522VIA_ACR_T1_CONTINUOUS(c)   (c->acr & 0x40)
#define MOS6522VIA_ACR_T2_COUNT_PB6(c)    (c->acr & 0x20)

// Interrupt bits
#define MOS6522VIA_IRQ_CA2 (1 << 0)
#define MOS6522VIA_IRQ_CA1 (1 << 1)
#define MOS6522VIA_IRQ_SR  (1 << 2)
#define MOS6522VIA_IRQ_CB2 (1 << 3)
#define MOS6522VIA_IRQ_CB1 (1 << 4)
#define MOS6522VIA_IRQ_T2  (1 << 5)
#define MOS6522VIA_IRQ_T1  (1 << 6)
#define MOS6522VIA_IRQ_ANY (1 << 7)

// Delay-pipeline bit offsets
#define MOS6522VIA_PIP_TIMER_COUNT (0)
#define MOS6522VIA_PIP_TIMER_LOAD  (8)
#define MOS6522VIA_PIP_IRQ         (0)

// I/O port state
typedef struct {
    uint8_t inpr;
    uint8_t outr;
    uint8_t ddr;
    bool c1_in;
    bool c1_out;
    bool c1_triggered;
    bool c2_in;
    bool c2_out;
    bool c2_triggered;
} mos6522via_port_t;

// Timer state
typedef struct {
    uint16_t latch;   // 16-bit initial value latch, NOTE: T2 only has an 8-bit latch
    int32_t counter;  // 16-bit counter
    bool t_bit;       // Toggles between true and false when counter underflows
    bool t_out;       // True for 1 cycle when counter underflow

    // Merged delay-pipelines:
    //   2-cycle 'counter active':   bits 0..7
    //   1-cycle 'force load':       bits 8..16
    uint16_t pip;
} mos6522via_timer_t;

// Interrupt state (same as mos6522via_int_t)
typedef struct {
    uint8_t ier;  // interrupt enable register
    uint8_t ifr;  // interrupt flag register
    uint16_t pip;
} mos6522via_int_t;

// Internal mos6522via state
typedef struct {
    mos6522via_port_t pa;
    mos6522via_port_t pb;
    mos6522via_timer_t t1;
    mos6522via_timer_t t2;
    mos6522via_int_t intr;
    uint8_t acr;  // Auxilary control register
    uint8_t pcr;  // Peripheral control register
    bool pb6_triggered;
} mos6522via_t;

// Initialize a new 6522 instance
void mos6522via_init(mos6522via_t* c);
// Reset an existing 6522 instance
void mos6522via_reset(mos6522via_t* c);
// Tick the mos6522via
bool mos6522via_tick(mos6522via_t* c, uint8_t cycles);

uint8_t mos6522via_read(mos6522via_t* c, uint8_t addr);

void mos6522via_write(mos6522via_t* c, uint8_t addr, uint8_t data);

uint8_t mos6522via_get_pa(mos6522via_t* c);

void mos6522via_set_pa(mos6522via_t* c, uint8_t data);

bool mos6522via_get_ca1(mos6522via_t* c);

void mos6522via_set_ca1(mos6522via_t* c, bool state);

bool mos6522via_get_ca2(mos6522via_t* c);

void mos6522via_set_ca2(mos6522via_t* c, bool state);

uint8_t mos6522via_get_pb(mos6522via_t* c);

void mos6522via_set_pb(mos6522via_t* c, uint8_t data);

bool mos6522via_get_cb1(mos6522via_t* c);

void mos6522via_set_cb1(mos6522via_t* c, bool state);

bool mos6522via_get_cb2(mos6522via_t* c);

void mos6522via_set_cb2(mos6522via_t* c, bool state);

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

static void _mos6522via_init_port(mos6522via_port_t* p) {
    p->inpr = 0xFF;
    p->outr = 0;
    p->ddr = 0;
    p->c1_in = false;
    p->c1_out = true;
    p->c1_triggered = false;
    p->c2_in = false;
    p->c2_out = true;
    p->c2_triggered = false;
}

static void _mos6522via_init_timer(mos6522via_timer_t* t, bool is_reset) {
    // Counters and latches are not initialized at reset
    if (!is_reset) {
        t->latch = 0xFFFF;
        t->counter = 0;
        t->t_bit = false;
    }
    t->t_out = false;
    t->pip = 0;
}

static void _mos6522via_init_interrupt(mos6522via_int_t* intr) {
    intr->ier = 0;
    intr->ifr = 0;
    intr->pip = 0;
}

void mos6522via_init(mos6522via_t* c) {
    CHIPS_ASSERT(c);
    memset(c, 0, sizeof(*c));
    _mos6522via_init_port(&c->pa);
    _mos6522via_init_port(&c->pb);
    _mos6522via_init_timer(&c->t1, false);
    _mos6522via_init_timer(&c->t2, false);
    _mos6522via_init_interrupt(&c->intr);
    c->acr = 0;
    c->pcr = 0;
    c->t1.latch = 0xFFFF;
    c->t2.latch = 0xFFFF;
    c->pb6_triggered = false;
}

// "The RESET input clears all internal registers to logic 0,
// (except T1, T2 and SR). This places all peripheral interface lines
// in the input state, disables the timers, shift registers etc. and
// disables interrupting from the chip"
void mos6522via_reset(mos6522via_t* c) {
    CHIPS_ASSERT(c);
    _mos6522via_init_port(&c->pa);
    _mos6522via_init_port(&c->pb);
    _mos6522via_init_timer(&c->t1, true);
    _mos6522via_init_timer(&c->t2, true);
    _mos6522via_init_interrupt(&c->intr);
    c->acr = 0;
    c->pcr = 0;
    c->pb6_triggered = false;
}

// Delay pipeline macros
// Set or clear a new state at pipeline pos
#define _MOS6522VIA_PIP_SET(pip, offset, pos) \
    { pip |= (1 << (offset + pos)); }
#define _MOS6522VIA_PIP_CLR(pip, offset, pos) \
    { pip &= ~(1 << (offset + pos)); }
// Reset an entire pipeline
#define _MOS6522VIA_PIP_RESET(pip, offset) \
    { pip &= ~(0xFF << offset); }
// Test pipeline state, pos 0 is the 'output bit'
#define _MOS6522VIA_PIP_TEST(pip, offset, pos) (0 != (pip & (1 << (offset + pos))))

static inline uint8_t _mos6522via_merge_pb7(mos6522via_t* c, uint8_t data) {
    if (MOS6522VIA_ACR_T1_SET_PB7(c)) {
        data &= ~(1 << 7);
        if (c->t1.t_bit) {
            data |= (1 << 7);
        }
    }
    return data;
}

static inline void _mos6522via_set_intr(mos6522via_t* c, uint8_t data) { c->intr.ifr |= data; }

static inline void _mos6522via_clear_intr(mos6522via_t* c, uint8_t data) {
    c->intr.ifr &= ~data;
    // Clear main interrupt flag?
    if (0 == (c->intr.ifr & c->intr.ier & 0x7F)) {
        c->intr.ifr &= 0x7F;
        // Cancel any interrupts in the delay pipeline
        _MOS6522VIA_PIP_RESET(c->intr.pip, MOS6522VIA_PIP_IRQ);
    }
}

static inline void _mos6522via_clear_pa_intr(mos6522via_t* c) {
    _mos6522via_clear_intr(c, MOS6522VIA_IRQ_CA1 | (MOS6522VIA_PCR_CA2_IND_IRQ(c) ? 0 : MOS6522VIA_IRQ_CA2));
}

static inline void _mos6522via_clear_pb_intr(mos6522via_t* c) {
    _mos6522via_clear_intr(c, MOS6522VIA_IRQ_CB1 | (MOS6522VIA_PCR_CB2_IND_IRQ(c) ? 0 : MOS6522VIA_IRQ_CB2));
}

static inline void _mos6522via_write_ier(mos6522via_t* c, uint8_t data) {
    if (data & 0x80) {
        c->intr.ier |= data & 0x7F;
    } else {
        c->intr.ier &= ~(data & 0x7F);
    }
}

static inline void _mos6522via_write_ifr(mos6522via_t* c, uint8_t data) {
    if (data & MOS6522VIA_IRQ_ANY) {
        data = 0x7F;
    }
    _mos6522via_clear_intr(c, data);
}

// On timer behaviour:
//
// http://forum.6502.org/viewtopic.php?f=4&t=2901
//
// (essentially: T1 is always reloaded from latch, both in continuous
// and oneshot mode, while T2 is never reloaded)

static void _mos6522via_tick_t1(mos6522via_t* c, uint8_t cycles) {
    mos6522via_timer_t* t = &c->t1;

    // Decrement counter
    if (_MOS6522VIA_PIP_TEST(t->pip, MOS6522VIA_PIP_TIMER_COUNT, 0)) {
        t->counter -= cycles;
    }

    // Timer underflow
    t->t_out = (t->counter < 0);
    if (t->t_out) {
        t->counter = 0xFFFF;
        // Continuous or oneshot mode
        if (MOS6522VIA_ACR_T1_CONTINUOUS(c)) {
            t->t_bit = !t->t_bit;
            // Trigger T1 interrupt on each underflow
            _mos6522via_set_intr(c, MOS6522VIA_IRQ_T1);

            _MOS6522VIA_PIP_SET(t->pip, MOS6522VIA_PIP_TIMER_LOAD, 1);
        } else {
            if (!t->t_bit) {
                /* trigger T1 only once */
                _mos6522via_set_intr(c, MOS6522VIA_IRQ_T1);
                t->t_bit = true;
            }
        }
        // Reload T1 from latch on each underflow,
        // this happens both in oneshot and continous mode
        // _MOS6522VIA_PIP_SET(t->pip, MOS6522VIA_PIP_TIMER_LOAD, 1);
    }

    // Reload timer from latch
    if (_MOS6522VIA_PIP_TEST(t->pip, MOS6522VIA_PIP_TIMER_LOAD, 0)) {
        t->counter = t->latch;
    }
}

static void _mos6522via_tick_t2(mos6522via_t* c, uint8_t cycles) {
    mos6522via_timer_t* t = &c->t2;

    // Either decrement on PB6, or on tick
    if (MOS6522VIA_ACR_T2_COUNT_PB6(c)) {
        // Count falling edge of PB6
        if (c->pb6_triggered) {
            t->counter--;
        }
    } else if (_MOS6522VIA_PIP_TEST(t->pip, MOS6522VIA_PIP_TIMER_COUNT, 0)) {
        t->counter -= cycles;
    }

    // Underflow?
    t->t_out = (t->counter < 0);
    if (t->t_out) {
        t->counter = 0xFFFF;
        // t2 is always oneshot
        if (!t->t_bit) {
            // FIXME: 6526-style "Timer B Bug"
            _mos6522via_set_intr(c, MOS6522VIA_IRQ_T2);
            t->t_bit = true;
        }
        // NOTE: T2 never reloads from latch on hitting zero
    }
}

static void _mos6522via_tick_pipeline(mos6522via_t* c) {
    // Feed counter pipelines, both counters are always counting
    _MOS6522VIA_PIP_SET(c->t1.pip, MOS6522VIA_PIP_TIMER_COUNT, 2);
    _MOS6522VIA_PIP_SET(c->t2.pip, MOS6522VIA_PIP_TIMER_COUNT, 2);

    // Interrupt pipeline
    if (c->intr.ifr & c->intr.ier) {
        _MOS6522VIA_PIP_SET(c->intr.pip, MOS6522VIA_PIP_IRQ, 1);
    }

    // Tick pipelines forward
    c->t1.pip = (c->t1.pip >> 1) & 0x7F7F;
    c->t2.pip = (c->t2.pip >> 1) & 0x7F7F;
    c->intr.pip = (c->intr.pip >> 1) & 0x7F7F;
}

static void _mos6522via_update_cab(mos6522via_t* c) {
    if (c->pa.c1_triggered) {
        _mos6522via_set_intr(c, MOS6522VIA_IRQ_CA1);
        if (MOS6522VIA_PCR_CA2_AUTO_HS(c)) {
            c->pa.c2_out = true;
        }
    }
    if (c->pa.c2_triggered && MOS6522VIA_PCR_CA2_INPUT(c)) {
        _mos6522via_set_intr(c, MOS6522VIA_IRQ_CA2);
    }
    if (c->pb.c1_triggered) {
        _mos6522via_set_intr(c, MOS6522VIA_IRQ_CB1);
        if (MOS6522VIA_PCR_CB2_AUTO_HS(c)) {
            c->pb.c2_out = true;
        }
    }
    // FIXME: shift-in/out on CB1
    if (c->pb.c2_triggered && MOS6522VIA_PCR_CB2_INPUT(c)) {
        _mos6522via_set_intr(c, MOS6522VIA_IRQ_CB2);
    }
}

static bool _mos6522via_update_irq(mos6522via_t* c) {
    // Main interrupt bit (delayed by pip)
    if (_MOS6522VIA_PIP_TEST(c->intr.pip, MOS6522VIA_PIP_IRQ, 0)) {
        c->intr.ifr |= (1 << 7);
    }

    if (0 != (c->intr.ifr & (1 << 7))) {
        return true;
    }

    return false;
}

// Perform a tick
bool mos6522via_tick(mos6522via_t* c, uint8_t cycles) {
    _mos6522via_update_cab(c);
    _mos6522via_tick_t1(c, cycles);
    _mos6522via_tick_t2(c, cycles);
    bool irq = _mos6522via_update_irq(c);
    _mos6522via_tick_pipeline(c);
    return irq;
}

// Read a register
uint8_t mos6522via_read(mos6522via_t* c, uint8_t reg) {
    uint8_t data = 0;
    switch (reg) {
        case MOS6522VIA_REG_RB:
            if (MOS6522VIA_ACR_PB_LATCH_ENABLE(c)) {
                data = c->pb.inpr;
            } else {
                data = mos6522via_get_pb(c);
            }
            _mos6522via_clear_pb_intr(c);
            break;

        case MOS6522VIA_REG_RA:
            if (MOS6522VIA_ACR_PA_LATCH_ENABLE(c)) {
                data = c->pa.inpr;
            } else {
                data = mos6522via_get_pa(c);
            }
            _mos6522via_clear_pa_intr(c);
            if (MOS6522VIA_PCR_CA2_PULSE_OUTPUT(c) || MOS6522VIA_PCR_CA2_AUTO_HS(c)) {
                c->pa.c2_out = false;
            }
            if (MOS6522VIA_PCR_CA2_PULSE_OUTPUT(c)) {
                /* FIXME: pulse output delay pipeline */
            }
            break;

        case MOS6522VIA_REG_DDRB:
            data = c->pb.ddr;
            break;

        case MOS6522VIA_REG_DDRA:
            data = c->pa.ddr;
            break;

        case MOS6522VIA_REG_T1CL:
            data = c->t1.counter & 0xFF;
            _mos6522via_clear_intr(c, MOS6522VIA_IRQ_T1);
            break;

        case MOS6522VIA_REG_T1CH:
            data = c->t1.counter >> 8;
            break;

        case MOS6522VIA_REG_T1LL:
            data = c->t1.latch & 0xFF;
            break;

        case MOS6522VIA_REG_T1LH:
            data = c->t1.latch >> 8;
            break;

        case MOS6522VIA_REG_T2CL:
            data = c->t2.counter & 0xFF;
            _mos6522via_clear_intr(c, MOS6522VIA_IRQ_T2);
            break;

        case MOS6522VIA_REG_T2CH:
            data = c->t2.counter >> 8;
            break;

        case MOS6522VIA_REG_SR:
            /* FIXME */
            break;

        case MOS6522VIA_REG_ACR:
            data = c->acr;
            break;

        case MOS6522VIA_REG_PCR:
            data = c->pcr;
            break;

        case MOS6522VIA_REG_IFR:
            data = c->intr.ifr;
            break;

        case MOS6522VIA_REG_IER:
            data = c->intr.ier | 0x80;
            break;

        case MOS6522VIA_REG_RA_NOH:
            if (MOS6522VIA_ACR_PA_LATCH_ENABLE(c)) {
                data = c->pa.inpr;
            } else {
                data = mos6522via_get_pa(c);
            }
            break;
    }
    return data;
}

// Write a register
void mos6522via_write(mos6522via_t* c, uint8_t reg, uint8_t data) {
    switch (reg) {
        case MOS6522VIA_REG_RB:
            c->pb.outr = data;
            _mos6522via_clear_pb_intr(c);
            if (MOS6522VIA_PCR_CB2_AUTO_HS(c)) {
                c->pb.c2_out = false;
            }
            break;

        case MOS6522VIA_REG_RA:
            c->pa.outr = data;
            _mos6522via_clear_pa_intr(c);
            if (MOS6522VIA_PCR_CA2_PULSE_OUTPUT(c) || MOS6522VIA_PCR_CA2_AUTO_HS(c)) {
                c->pa.c2_out = false;
            }
            if (MOS6522VIA_PCR_CA2_PULSE_OUTPUT(c)) {
                /* FIXME: pulse output delay pipeline */
            }
            break;

        case MOS6522VIA_REG_DDRB:
            c->pb.ddr = data;
            break;

        case MOS6522VIA_REG_DDRA:
            c->pa.ddr = data;
            break;

        case MOS6522VIA_REG_T1CL:
        case MOS6522VIA_REG_T1LL:
            c->t1.latch = (c->t1.latch & 0xFF00) | data;
            break;

        case MOS6522VIA_REG_T1CH:
            c->t1.latch = (data << 8) | (c->t1.latch & 0x00FF);
            _mos6522via_clear_intr(c, MOS6522VIA_IRQ_T1);
            c->t1.t_bit = false;
            c->t1.counter = c->t1.latch;
            break;

        case MOS6522VIA_REG_T1LH:
            c->t1.latch = (data << 8) | (c->t1.latch & 0x00FF);
            _mos6522via_clear_intr(c, MOS6522VIA_IRQ_T1);
            break;

        case MOS6522VIA_REG_T2CL:
            c->t2.latch = (c->t2.latch & 0xFF00) | data;
            break;

        case MOS6522VIA_REG_T2CH:
            c->t2.latch = (data << 8) | (c->t2.latch & 0x00FF);
            _mos6522via_clear_intr(c, MOS6522VIA_IRQ_T2);
            c->t2.t_bit = false;
            c->t2.counter = c->t2.latch;
            break;

        case MOS6522VIA_REG_SR:
            /* FIXME */
            break;

        case MOS6522VIA_REG_ACR:
            c->acr = data;
            /* FIXME: shift timer */
            /*
            if (MOS6522VIA_ACR_T1_CONTINUOUS(c)) {
                // FIXME: continuous counter delay?
                _MOS6522VIA_PIP_CLR(c->t1.pip, MOS6522VIA_PIP_TIMER_COUNT, 0);
                _MOS6522VIA_PIP_CLR(c->t1.pip, MOS6522VIA_PIP_TIMER_COUNT, 1);
            }
            */
            /* FIXME(?) this properly transitions T2 from counting PB6 to clock counter mode */
            if (!MOS6522VIA_ACR_T2_COUNT_PB6(c)) {
                _MOS6522VIA_PIP_CLR(c->t2.pip, MOS6522VIA_PIP_TIMER_COUNT, 0)
            }
            break;

        case MOS6522VIA_REG_PCR:
            c->pcr = data;
            if (MOS6522VIA_PCR_CA2_FIX_OUTPUT(c)) {
                c->pa.c2_out = MOS6522VIA_PCR_CA2_OUTPUT_LEVEL(c);
            }
            if (MOS6522VIA_PCR_CB2_FIX_OUTPUT(c)) {
                c->pb.c2_out = MOS6522VIA_PCR_CB2_OUTPUT_LEVEL(c);
            }
            break;

        case MOS6522VIA_REG_IFR:
            _mos6522via_write_ifr(c, data);
            break;

        case MOS6522VIA_REG_IER:
            _mos6522via_write_ier(c, data);
            break;

        case MOS6522VIA_REG_RA_NOH:
            c->pa.outr = data;
            break;
    }
}

uint8_t mos6522via_get_pa(mos6522via_t* c) { return (c->pa.inpr & ~c->pa.ddr) | (c->pa.outr & c->pa.ddr); }

void mos6522via_set_pa(mos6522via_t* c, uint8_t data) {
    // With latching enabled, only update input register when CA1 goes active
    if (MOS6522VIA_ACR_PA_LATCH_ENABLE(c)) {
        if (c->pa.c1_triggered) {
            c->pa.inpr = data;
        }
    } else {
        c->pa.inpr = data;
    }
}

bool mos6522via_get_ca1(mos6522via_t* c) { return c->pa.c1_out; }

void mos6522via_set_ca1(mos6522via_t* c, bool state) {
    c->pa.c1_triggered = (c->pa.c1_in != state) && ((state && MOS6522VIA_PCR_CA1_LOW_TO_HIGH(c)) ||
                                                    (!state && MOS6522VIA_PCR_CA1_HIGH_TO_LOW(c)));
    c->pa.c1_in = state;
}

bool mos6522via_get_ca2(mos6522via_t* c) { return c->pa.c2_out; }

void mos6522via_set_ca2(mos6522via_t* c, bool state) {
    c->pa.c2_triggered = (c->pa.c2_in != state) && ((state && MOS6522VIA_PCR_CA2_LOW_TO_HIGH(c)) ||
                                                    (!state && MOS6522VIA_PCR_CA2_HIGH_TO_LOW(c)));
    c->pa.c2_in = state;
}

uint8_t mos6522via_get_pb(mos6522via_t* c) {
    return _mos6522via_merge_pb7(c, (c->pb.inpr & ~c->pb.ddr) | (c->pb.outr & c->pb.ddr));
}

void mos6522via_set_pb(mos6522via_t* c, uint8_t data) {
    c->pb6_triggered = (c->pb.inpr & 0x40) && ((data & 0x40) == 0);
    // With latching enabled, only update input register when CB1 goes active
    if (MOS6522VIA_ACR_PB_LATCH_ENABLE(c)) {
        if (c->pb.c1_triggered) {
            c->pb.inpr = data;
        }
    } else {
        c->pb.inpr = data;
    }
}

bool mos6522via_get_cb1(mos6522via_t* c) { return c->pb.c1_out; }

void mos6522via_set_cb1(mos6522via_t* c, bool state) {
    c->pb.c1_triggered = (c->pb.c1_in != state) && ((state && MOS6522VIA_PCR_CB1_LOW_TO_HIGH(c)) ||
                                                    (!state && MOS6522VIA_PCR_CB1_HIGH_TO_LOW(c)));
    c->pb.c1_in = state;
}

bool mos6522via_get_cb2(mos6522via_t* c) { return c->pb.c2_out; }

void mos6522via_set_cb2(mos6522via_t* c, bool state) {
    c->pb.c2_triggered = (c->pb.c2_in != state) && ((state && MOS6522VIA_PCR_CB2_LOW_TO_HIGH(c)) ||
                                                    (!state && MOS6522VIA_PCR_CB2_HIGH_TO_LOW(c)));
    c->pb.c2_in = state;
}

#endif  // CHIPS_IMPL
