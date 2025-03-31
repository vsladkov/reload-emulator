#=== EXECUTABLE: oric

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_executable(oric WIN32 ./src/oric.c)
else()
    add_executable(oric ./src/oric.c)
endif()

target_link_libraries(oric platform)

if (CMAKE_SYSTEM_NAME STREQUAL Emscripten)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_link_options(oric PUBLIC --shell-file ../sokol/shell.html)
    target_link_options(oric PUBLIC -sUSE_WEBGL2=1 -sNO_FILESYSTEM=1 -sASSERTIONS=0 -sMALLOC=emmalloc --closure=1)
endif()

if (MSVC)
    target_compile_options(oric PUBLIC /W3)
else()
    target_compile_options(oric PUBLIC -Wall -Wextra -Wsign-compare)
endif()

# explicitly strip dead code
if (CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_SYSTEM_NAME STREQUAL Emscripten)
    target_link_options(oric PRIVATE LINKER:-dead_strip)
endif()
