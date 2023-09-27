# reload-emulator
Reload - Portable Retro Computers Emulator

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
Emulator is working with embedded NIB files as C headers.

You can use dsk2nib tool to convert DSK image to NIB image, then bin2hdr to generate headers.

Please, make sure you have the proper license to use the games.

## Hardware
### Neo6502
For Oric emulator to work correctly you must connect pin 10 of UEXT connector (GPIO 25) to pin 24 of 6502 bus connector (IRQ) using external wire.

To use F12 as RESET button (both Apple II and Oric) you have to connect pin 9 of UEXT connector (GPIO 26) to pin 40 of 6502 bus connector (RESET).

To use F11 as NMI button (Oric) you have to connect pin 8 of UEXT connector (GPIO 27) to pin 26 of 6502 bus connector (NMI).
