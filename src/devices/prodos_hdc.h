#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ProDOS HDC softswitches
#define PRODOS_HDC_RC_A (0x00)  // return code register A
#define PRODOS_HDC_RC_X (0x01)  // return code register X
#define PRODOS_HDC_RC_Y (0x02)  // return code register Y
#define PRODOS_HDC_PARA (0x07)  // paravirtualization trigger

#define PRODOS_HDC_MAGIC (0x65)
#define PRODOS_HDC_MAX_DRIVES 2

// ProDOS disk driver parameters
#define PRODOS_DRV_COMMAND (0x0042)
#define PRODOS_DRV_UNIT    (0x0043)
#define PRODOS_DRV_BUFFER  (0x0044)
#define PRODOS_DRV_BLOCK   (0x0046)

// ProDOS disk driver commands
#define PRODOS_CMD_STATUS (0x00)
#define PRODOS_CMD_READ   (0x01)
#define PRODOS_CMD_WRITE  (0x02)
#define PRODOS_CMD_FORMAT (0x04)

// ProDOS hard disk controller state
typedef struct {
    bool valid;
    uint8_t return_code[3];
    prodos_hdd_t hdd[PRODOS_HDC_MAX_DRIVES];
} prodos_hdc_t;

// ProDOS hard disk controller interface

// Initialize a new hard disk controller
void prodos_hdc_init(prodos_hdc_t* sys);

// Discard the hard disk controller
void prodos_hdc_discard(prodos_hdc_t* sys);

// Reset the hard disk controller
void prodos_hdc_reset(prodos_hdc_t* sys);

uint8_t prodos_hdc_read_byte(prodos_hdc_t* sys, uint8_t addr);

void prodos_hdc_write_byte(prodos_hdc_t* sys, uint8_t addr, uint8_t byte, mem_t* mem);

// Prepare a new hard disk controller snapshot for saving
void prodos_hdc_snapshot_onsave(prodos_hdc_t* snapshot);

// Fix up the hard disk controller snapshot after loading
void prodos_hdc_snapshot_onload(prodos_hdc_t* snapshot, prodos_hdc_t* sys);

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

void prodos_hdc_init(prodos_hdc_t* sys) {
    CHIPS_ASSERT(sys && !sys->valid);
    memset(sys, 0, sizeof(prodos_hdc_t));
    sys->valid = true;
    prodos_hdd_init(&sys->hdd[0]);
}

void prodos_hdc_discard(prodos_hdc_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
    prodos_hdd_discard(&sys->hdd[0]);
}

void prodos_hdc_reset(prodos_hdc_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    prodos_hdd_reset(&sys->hdd[0]);
}

uint8_t prodos_hdc_read_byte(prodos_hdc_t* sys, uint8_t addr) {
    if (addr > PRODOS_HDC_RC_Y) {
        return 0;
    }
    return sys->return_code[addr];
}

void prodos_hdc_write_byte(prodos_hdc_t* sys, uint8_t addr, uint8_t byte, mem_t* mem) {
    if (addr != PRODOS_HDC_PARA || byte != PRODOS_HDC_MAGIC) {
        return;
    }

    prodos_hdd_t* hdd = &sys->hdd[0];

    if (mem_rd(mem, PRODOS_DRV_UNIT) != 0x70 || !prodos_hdd_is_disk_inserted(hdd)) {
        sys->return_code[PRODOS_HDC_RC_A] = PRODOS_HDD_ERR_NODEV;
        return;
    }

    uint32_t blocks = prodos_hdd_get_blocks(hdd);

    uint16_t buffer = mem_rd16(mem, PRODOS_DRV_BUFFER);
    uint16_t block  = mem_rd16(mem, PRODOS_DRV_BLOCK);

    switch (mem_rd(mem, PRODOS_DRV_COMMAND)) {
        case PRODOS_CMD_STATUS:
            sys->return_code[PRODOS_HDC_RC_A] = PRODOS_HDD_ERR_OK;
            sys->return_code[PRODOS_HDC_RC_X] = blocks >> 0 & 0xFF;
            sys->return_code[PRODOS_HDC_RC_Y] = blocks >> 8 & 0xFF;
            return;
        case PRODOS_CMD_READ:
            sys->return_code[PRODOS_HDC_RC_A] = prodos_hdd_read_block(hdd, buffer, block, mem);
            return;
        case PRODOS_CMD_WRITE:
            sys->return_code[PRODOS_HDC_RC_A] = prodos_hdd_write_block(hdd, buffer, block, mem);
            return;
    }

    sys->return_code[PRODOS_HDC_RC_A] = PRODOS_HDD_ERR_IO;
    return;
}

void prodos_hdc_snapshot_onsave(prodos_hdc_t* snapshot) { CHIPS_ASSERT(snapshot); }

void prodos_hdc_snapshot_onload(prodos_hdc_t* snapshot, prodos_hdc_t* sys) { CHIPS_ASSERT(snapshot && sys); }

#endif /* CHIPS_IMPL */
