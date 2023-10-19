#pragma once
/*
    wdc65C02cpu.h    --

    TODO: docs

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
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
*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// initialize cpu
void wdc65C02cpu_init();
// reset cpu
void wdc65C02cpu_reset();

void wdc65C02cpu_nmi();

// tick the cpu
void wdc65C02cpu_tick(uint16_t* addr, bool* rw);

uint16_t wdc65C02cpu_get_address();

uint8_t wdc65C02cpu_get_data();

void wdc65C02cpu_set_data(uint8_t data);

void wdc65C02cpu_set_irq(bool state);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
#include <assert.h>
#define CHIPS_ASSERT(c) assert(c)
#endif

#ifdef PICO_NEO6502
#define _GPIO_MASK       (0xFF)
#define _GPIO_SHIFT_BITS (0)
#define _OE1_PIN         (8)
#define _OE2_PIN         (9)
#define _OE3_PIN         (10)
#define _RW_PIN          (11)
#define _CLOCK_PIN       (21)
#define _AUDIO_PIN       (20)
#define _RESET_PIN       (26)
#define _IRQ_PIN         (25)
#define _NMI_PIN         (27)
#else
#define _GPIO_MASK       (0x3FC)
#define _GPIO_SHIFT_BITS (2)
#define _OE1_PIN         (10)
#define _OE2_PIN         (11)
#define _OE3_PIN         (20)
#define _RW_PIN          (21)
#define _CLOCK_PIN       (22)
#define _AUDIO_PIN       (27)
#define _RESET_PIN       (26)
#define _IRQ_PIN         (28)
#endif  // PICO_NEO6502

void wdc65C02cpu_init() {
    gpio_init_mask(_GPIO_MASK);

    gpio_init(_OE1_PIN);  // OE1
    gpio_set_dir(_OE1_PIN, GPIO_OUT);
    gpio_put(_OE1_PIN, 1);

    gpio_init(_OE2_PIN);  // OE2
    gpio_set_dir(_OE2_PIN, GPIO_OUT);
    gpio_put(_OE2_PIN, 1);

    gpio_init(_OE3_PIN);  // OE3
    gpio_set_dir(_OE3_PIN, GPIO_OUT);
    gpio_put(_OE3_PIN, 1);

    gpio_init(_RW_PIN);  // RW
    gpio_set_dir(_RW_PIN, GPIO_IN);

    gpio_init(_CLOCK_PIN);  // CLOCK
    gpio_set_dir(_CLOCK_PIN, GPIO_OUT);

    gpio_init(_RESET_PIN);  // RESET
    gpio_set_dir(_RESET_PIN, GPIO_OUT);

    gpio_init(_IRQ_PIN);  // IRQ
    gpio_set_dir(_IRQ_PIN, GPIO_OUT);
    gpio_put(_IRQ_PIN, 1);

#ifdef PICO_NEO6502
    gpio_init(_NMI_PIN);  // NMI
    gpio_set_dir(_NMI_PIN, GPIO_OUT);
    gpio_put(_NMI_PIN, 1);
#endif  // PICO_NEO6502

    gpio_put(_RESET_PIN, 0);
    sleep_us(1000);
    gpio_put(_RESET_PIN, 1);
}

void wdc65C02cpu_reset() {
    gpio_put(_RESET_PIN, 0);
    sleep_us(1000);
    gpio_put(_RESET_PIN, 1);
}

void wdc65C02cpu_nmi() {
#ifdef PICO_NEO6502
    gpio_put(_NMI_PIN, 0);
    sleep_us(1000);
    gpio_put(_NMI_PIN, 1);
#endif  // PICO_NEO6502
}

void wdc65C02cpu_tick(uint16_t* addr, bool* rw) {
    gpio_put(_CLOCK_PIN, 0);

    *addr = wdc65C02cpu_get_address();

    *rw = gpio_get(_RW_PIN);

    gpio_put(_CLOCK_PIN, 1);
}

uint16_t wdc65C02cpu_get_address() {
    gpio_set_dir_masked(_GPIO_MASK, 0);

    gpio_put(_OE1_PIN, 0);
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
    uint16_t addr = (gpio_get_all() >> _GPIO_SHIFT_BITS) & 0xFF;
    gpio_put(_OE1_PIN, 1);

    gpio_put(_OE2_PIN, 0);
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
    addr |= (gpio_get_all() << (8 - _GPIO_SHIFT_BITS)) & 0xFF00;
    gpio_put(_OE2_PIN, 1);

    // printf("get addr: %04x\n", addr);

    return addr;
}

uint8_t wdc65C02cpu_get_data() {
    gpio_set_dir_masked(_GPIO_MASK, 0);

    gpio_put(_OE3_PIN, 0);
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
    uint8_t data = (gpio_get_all() >> _GPIO_SHIFT_BITS) & 0xFF;
    gpio_put(_OE3_PIN, 1);

    // printf("get data: %02x\n", data);

    return data;
}

void wdc65C02cpu_set_data(uint8_t data) {
    gpio_set_dir_masked(_GPIO_MASK, _GPIO_MASK);

    gpio_put_masked(_GPIO_MASK, data << _GPIO_SHIFT_BITS);
    gpio_put(_OE3_PIN, 0);
#ifndef PICO_NEO6502
    __asm volatile("nop\n");
    __asm volatile("nop\n");
#endif
    gpio_put(_OE3_PIN, 1);

    // printf("set data: %02x\n", data);
}

void wdc65C02cpu_set_irq(bool state) {
    gpio_put(_IRQ_PIN, state ? 0 : 1);
}

#endif /* CHIPS_IMPL */
