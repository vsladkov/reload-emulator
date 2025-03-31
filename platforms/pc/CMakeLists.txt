cmake_minimum_required(VERSION 3.20)
project(pc)
set(CMAKE_C_STANDARD 11)

# Linux -pthread shenanigans
if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/lib/sokol
	${CMAKE_CURRENT_SOURCE_DIR}/lib/sokol/util
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/../../src
	)

add_subdirectory(src)
add_subdirectory(systems)
