# Replace TMDS with 10 bit UART (same baud rate):
# add_definitions(-DDVI_SERIAL_DEBUG=1)
# add_definitions(-DRUN_FROM_CRYSTAL)

add_executable(apple2
	${CMAKE_CURRENT_SOURCE_DIR}/src/apple2.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils.S
	${CMAKE_CURRENT_SOURCE_DIR}/../../src/hid_app.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../src/audio.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../src/utils.S
)

target_compile_options(apple2 PRIVATE -Wall)

target_compile_definitions(apple2 PRIVATE
	# DVI_DEFAULT_SERIAL_CONFIG=pico_sock_cfg
	DVI_DEFAULT_SERIAL_CONFIG=pico_neo6502_cfg
	PICO_NEO6502=1
)

target_link_libraries(apple2
	pico_stdlib
	pico_multicore
	pico_util
	libdvi
	tinyusb_host
)

# create map/bin/hex file etc.
pico_add_extra_outputs(apple2)

pico_enable_stdio_usb(apple2 0)
pico_enable_stdio_uart(apple2 1)
