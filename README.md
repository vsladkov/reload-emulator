# Reload - Portable Retro Computers Emulator

Supported machines:

### Apple //e
  - 128 KB RAM installed 
  - Extended 80 column card in the AUX slot
  - Disk II controller and 1 drive in slot 6
  - ProDOS hard disk controller in slot 7

### Apple ][+
  - 64 KB RAM installed
  - 16K Language Card in slot 0,
  - Disk II controller and 1 drive in slot 6
  - ProDOS hard disk controller in slot 7

### Oric Atmos
  - 64 KB RAM installed
  - 16 KB Overlay RAM installed
  - Disk II controller installed and 1 drive

## Requirements
### Tools
- cmake
- arm-none-eabi-gcc

### External Libraries (included as git submodules)
- pico-sdk
- PicoDVI
- tinyusb

## Quickstart 

```bash
# Checkout pico-sdk & PicoDVI & tinyusb as git submodules
cd platforms/pico-6502/lib
git submodule update --init -- pico-sdk PicoDVI tinyusb

cd ..

# Build
mkdir build && cd build
cmake ..
make

# Done
find . -type f -name "*.uf2" -ls
```

## Building firmware
Original firmware is not distributed with emulator sources. Please, make sure you have the proper license to use and build the headers from your own binaries.

The structure of the header files is described in roms/roms_md5.txt file together with file names and MD5 checksums.

You can use bin2hdr tool to generate firmware header files. Please, make sure MD5 checksums are identical.


## Building games
### UPDATE: Apple //e and Apple ][+ emulators now support USB flash drives with FAT and ExFAT file systems.

Apple //e and Apple ][+ emulators are working with embedded ProDOS images and NIB images as C headers.

Oric emulator is working with embedded NIB and WAVE images as C headers.

Building headers from DSK file (Apple ][, Oric):

```bash
# Moon Patrol.DSK
dsk2nib -i Moon Patrol.DSK -o Moon Patrol.NIB
bin2hdr -i Moon Patrol.NIB -o moon_patrol.h -a moon_patrol_nib_image

# Karateka.DSK
dsk2nib -i Karateka.DSK -o Karateka.NIB
bin2hdr -i Karateka.NIB -o karateka.h -a karateka_nib_image

#ProDOS 
bin2hdr -i neo6502.po -o neo6502.h -a neo6502_po_image
```

Example apple2_images.h file for Apple ][ emulator:

```
#include "moon_patrol.h"
#include "karateka.h"
#include "neo6502.h"

uint8_t* apple2_nib_images[] = {
    moon_patrol_nib_image,
    karateka_nib_image,
};

uint8_t* apple2_po_images[] = {
    neo6502_po_image,
};

uint32_t apple2_po_image_sizes[] = {
    sizeof(neo6502_po_image),
};

char* apple2_msc_images[] = {
    "Total Replay v5.0.hdv",
};

```

Building headers from TAP file (Oric):

```bash
# Pulsoids-UK.TAP
tap2wave -i Pulsoids-UK.TAP -o Pulsoids-UK.WAVE
bin2hdr -i Pulsoids-UK.WAVE -o pulsoids.h -a pulsoids_wave_image
```

Example oric_images.h file for Oric emulator:

```
#include "rdos_231.h"
#include "oric_games.h"
#include "pulsoids.h"

uint8_t* oric_nib_images[] = {
    rdos_231_nib_image,
    oric_games_nib_image,
};

uint8_t* oric_wave_images[] = {
    pulsoids_wave_image,
};
```

First NIB file is loaded in floppy disk drive on startup. Images can be loaded dynamically at runtime in floppy disk drive and / or tape drive using F1-F9.

Up to 9 disk images can be embedded in single UF2 binary (for 2MB flash).

Please, make sure you have the proper license to use the games.

## Hardware
### Neo6502
For Oric emulator to work correctly you must connect pin 10 of UEXT connector (GPIO 25) to pin 24 of 6502 bus connector (IRQ) using external wire.

To use F12 as RESET button (both Apple ][ and Oric) you have to connect pin 9 of UEXT connector (GPIO 26) to pin 40 of 6502 bus connector (RESET).

To use F11 as NMI button (Oric) you have to connect pin 8 of UEXT connector (GPIO 27) to pin 26 of 6502 bus connector (NMI).
