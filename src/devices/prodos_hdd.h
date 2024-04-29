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

#define PRODOS_HDD_IMAGE_TYPE_INTERNAL (0)
#define PRODOS_HDD_IMAGE_TYPE_MSC      (1)

// ProDOS hard disk drive state
typedef struct {
    bool valid;
    FIL fil;
    uint8_t* po_image;
    uint8_t image_type;
    uint32_t image_blocks;
    bool image_loaded;
    bool write_protected;
} prodos_hdd_t;

// ProDOS hard disk drive interface

// Initialize a new hard disk drive
void prodos_hdd_init(prodos_hdd_t* sys);

// Discard the hard disk drive
void prodos_hdd_discard(prodos_hdd_t* sys);

// Reset the hard disk drive
void prodos_hdd_reset(prodos_hdd_t* sys);

// Insert a new disk file (USB flash drive)
bool prodos_hdd_insert_disk_msc(prodos_hdd_t* sys, const char* file_name);

// Insert a new disk file (internal flash)
bool prodos_hdd_insert_disk_internal(prodos_hdd_t* sys, uint8_t* po_image, uint32_t po_image_size);

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
    sys->image_type = PRODOS_HDD_IMAGE_TYPE_INTERNAL;
    sys->image_loaded = false;
}

void prodos_hdd_discard(prodos_hdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void prodos_hdd_reset(prodos_hdd_t* sys) { CHIPS_ASSERT(sys && sys->valid); }

bool prodos_hdd_insert_disk_msc(prodos_hdd_t* sys, const char* file_name) {
    CHIPS_ASSERT(sys && sys->valid);
    FRESULT res = f_open(&sys->fil, file_name, FA_READ | FA_WRITE);
    if (res != FR_OK) {
        printf("Error %u opening file %s\r\n", res, file_name);
        return false;
    }
    sys->image_type = PRODOS_HDD_IMAGE_TYPE_MSC;
    sys->image_blocks = f_size(&sys->fil) / PRODOS_HDD_BYTES_PER_BLOCK;
    sys->image_loaded = true;
    sys->write_protected = false;

    return true;
}

bool prodos_hdd_insert_disk_internal(prodos_hdd_t* sys, uint8_t* po_image, uint32_t po_image_size) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->po_image = po_image;
    sys->image_type = PRODOS_HDD_IMAGE_TYPE_INTERNAL;
    sys->image_blocks = po_image_size / PRODOS_HDD_BYTES_PER_BLOCK;
    sys->image_loaded = true;
    sys->write_protected = true;

    return true;
}

void prodos_hdd_remove_disk(prodos_hdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    if (sys->image_type == PRODOS_HDD_IMAGE_TYPE_MSC) {
        f_close(&sys->fil);
    }
    sys->image_loaded = false;
}

bool prodos_hdd_is_disk_inserted(prodos_hdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return sys->image_loaded;
}

uint32_t prodos_hdd_get_blocks(prodos_hdd_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return sys->image_blocks;
}

uint8_t prodos_hdd_read_block(prodos_hdd_t* sys, uint16_t buffer, uint32_t block, mem_t* mem) {
    CHIPS_ASSERT(sys && sys->valid);
    if (block >= sys->image_blocks) {
        return PRODOS_HDD_ERR_IO;
    }

    if (sys->image_type == PRODOS_HDD_IMAGE_TYPE_MSC) {
        // USB flash drive
        uint8_t* buf = mem_writeptr(mem, buffer);
        FRESULT res;
        res = f_lseek(&sys->fil, block * PRODOS_HDD_BYTES_PER_BLOCK);
        if (res != FR_OK) {
            printf("Error %u reading from file\r\n", res);
            return PRODOS_HDD_ERR_IO;
        }
        UINT nread;
        res = f_read(&sys->fil, buf, PRODOS_HDD_BYTES_PER_BLOCK, &nread);
        if (res != FR_OK || nread != PRODOS_HDD_BYTES_PER_BLOCK) {
            printf("Error %u reading from file\r\n", res);
            return PRODOS_HDD_ERR_IO;
        }
    } else {
        // Internal flash
        mem_write_range(mem, buffer, sys->po_image + block * PRODOS_HDD_BYTES_PER_BLOCK, PRODOS_HDD_BYTES_PER_BLOCK);
    }

    return PRODOS_HDD_ERR_OK;
}

uint8_t prodos_hdd_write_block(prodos_hdd_t* sys, uint16_t buffer, uint32_t block, mem_t* mem) {
    printf("prodos_hdd_write_block\r\n");
    CHIPS_ASSERT(sys && sys->valid);
    if (block >= sys->image_blocks) {
        return PRODOS_HDD_ERR_IO;
    }
    if (sys->write_protected) {
        return PRODOS_HDD_ERR_WPROT;
    }

    if (sys->image_type == PRODOS_HDD_IMAGE_TYPE_MSC) {
        // USB flash drive
        FRESULT res;
        res = f_lseek(&sys->fil, block * PRODOS_HDD_BYTES_PER_BLOCK);
        if (res != FR_OK) {
            printf("Error %u writing to file\r\n", res);
            return PRODOS_HDD_ERR_IO;
        }
        UINT nwritten;
        res = f_write(&sys->fil, mem_readptr(mem, buffer), PRODOS_HDD_BYTES_PER_BLOCK, &nwritten);
        if (res != FR_OK || nwritten != PRODOS_HDD_BYTES_PER_BLOCK) {
            printf("Error %u writing to file\r\n", res);
            return PRODOS_HDD_ERR_IO;
        }
        f_sync(&sys->fil);
    }

    return PRODOS_HDD_ERR_OK;
}

#endif /* CHIPS_IMPL */
