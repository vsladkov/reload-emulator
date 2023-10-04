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

#define PRODOS_HDD_BYTES_PER_BLOCK 512

// ProDOS hard disk error codes
#define PRODOS_HDD_ERR_OK    (0x00)
#define PRODOS_HDD_ERR_IO    (0x27)
#define PRODOS_HDD_ERR_NODEV (0x28)
#define PRODOS_HDD_ERR_WPROT (0x2B)

// ProDOS hard disk drive state
typedef struct {
    bool valid;
    bool image_dirty;
    bool write_protected;
    bool po_image_loaded;
    uint8_t* po_image;
    uint32_t po_image_blocks;
} prodos_hdd_t;

// ProDOS hard disk drive interface

// Initialize a new hard disk drive
void prodos_hdd_init(prodos_hdd_t* sys);

// Discard the hard disk drive
void prodos_hdd_discard(prodos_hdd_t* sys);

// Reset the hard disk drive
void prodos_hdd_reset(prodos_hdd_t* sys);

// Insert a new disk file
bool prodos_hdd_insert_disk(prodos_hdd_t* sys, uint8_t* po_image, uint32_t po_image_size);

// Remove the disk file
void prodos_hdd_remove_disk(prodos_hdd_t* sys);

// Return true if the disk is currently inserted
bool prodos_hdd_is_disk_inserted(prodos_hdd_t* sys);

// Return the number of blocks 
uint32_t prodos_hdd_get_blocks(prodos_hdd_t* sys);

// Read a block from the hard disk drive
uint8_t prodos_hdd_read_block(prodos_hdd_t* sys, uint16_t buffer, uint32_t block, mem_t* mem);

// Write a block to the hard disk drive
uint8_t prodos_hdd_write_block(prodos_hdd_t* sys, uint16_t buffer, uint32_t block, mem_t* mem);

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

void prodos_hdd_init(prodos_hdd_t* sys) {
    CHIPS_ASSERT(sys && !sys->valid);
    memset(sys, 0, sizeof(prodos_hdd_t));
    sys->valid = true;
    sys->image_dirty = false;
    sys->write_protected = true;
    sys->po_image_loaded = false;
}

void prodos_hdd_discard(prodos_hdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void prodos_hdd_reset(prodos_hdd_t* sys) { CHIPS_ASSERT(sys && sys->valid); }

bool prodos_hdd_insert_disk(prodos_hdd_t* sys, uint8_t* po_image, uint32_t po_image_size) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->po_image = po_image;
    sys->po_image_blocks = po_image_size / PRODOS_HDD_BYTES_PER_BLOCK;
    sys->po_image_loaded = true;
    return true;
}

void prodos_hdd_remove_disk(prodos_hdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->po_image_loaded = false;
    sys->image_dirty = false;
}

bool prodos_hdd_is_disk_inserted(prodos_hdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return sys->po_image_loaded;
}

uint32_t prodos_hdd_get_blocks(prodos_hdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return sys->po_image_blocks;
}

uint8_t prodos_hdd_read_block(prodos_hdd_t* sys, uint16_t buffer, uint32_t block, mem_t* mem) {
    CHIPS_ASSERT(sys && sys->valid);
    if (block >= sys->po_image_blocks) {
        return PRODOS_HDD_ERR_IO;
    }

    mem_write_range(mem, buffer, sys->po_image + block * PRODOS_HDD_BYTES_PER_BLOCK, PRODOS_HDD_BYTES_PER_BLOCK);
    return PRODOS_HDD_ERR_OK;
}

uint8_t prodos_hdd_write_block(prodos_hdd_t* sys, uint16_t buffer, uint32_t block, mem_t* mem) {
    CHIPS_ASSERT(sys && sys->valid);
    if (block >= sys->po_image_blocks) {
        return PRODOS_HDD_ERR_IO;
    }
    if (sys->write_protected) {
        return PRODOS_HDD_ERR_WPROT;
    }
    sys->image_dirty = true;

    memcpy(sys->po_image + block * PRODOS_HDD_BYTES_PER_BLOCK, mem_readptr(mem, buffer), PRODOS_HDD_BYTES_PER_BLOCK);
    return PRODOS_HDD_ERR_OK;
}

#endif /* CHIPS_IMPL */
