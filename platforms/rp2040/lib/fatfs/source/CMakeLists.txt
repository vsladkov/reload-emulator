cmake_minimum_required(VERSION 3.13)

add_library(fatfs INTERFACE)
target_sources(fatfs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/ffunicode.c
)
target_include_directories(fatfs INTERFACE ${CMAKE_CURRENT_LIST_DIR})
