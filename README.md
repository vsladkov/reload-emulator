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

## Build ROMs
Original firmware is not distributed with emulator sources. Please, make sure you have the proper license to use and build the headers from your own binaries.

The structure of the header files is described in roms/roms_md5.txt file together with file names and MD5 checksums.

You can use bin2hdr tool to generate firmware header files. Please, make sure MD5 checksums are identical.


## Build games
Emulator is working with embedded NIB files and C headers.

You can use dsk2nib tool to convert DSK image to NIB image, then bin2hdr to generate headers.

Please, make sure you have the proper license to use the games.

