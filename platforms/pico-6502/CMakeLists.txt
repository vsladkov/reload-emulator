cmake_minimum_required(VERSION 3.12)

project(pico-6502 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/pico-sdk)
set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb)
set(PICO_BOARD neo6502)

include(pico_sdk_import.cmake)

pico_sdk_init()

add_compile_options(-Wall)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/lib/PicoDVI/software/include
	${CMAKE_CURRENT_SOURCE_DIR}/../../src
	${CMAKE_CURRENT_SOURCE_DIR}/src
	)

add_subdirectory(lib/PicoDVI/software/libdvi)
add_subdirectory(systems)
