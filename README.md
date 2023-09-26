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
# Checkout tinyusb & pico-sdk as git submodules
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
