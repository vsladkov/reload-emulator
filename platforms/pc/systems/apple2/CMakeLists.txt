#=== EXECUTABLE: apple2

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_executable(apple2 WIN32 ./src/apple2.c)
else()
    add_executable(apple2 ./src/apple2.c)
endif()

target_link_libraries(apple2 platform)

if (CMAKE_SYSTEM_NAME STREQUAL Emscripten)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_link_options(apple2 PUBLIC --shell-file ../sokol/shell.html)
    target_link_options(apple2 PUBLIC -sUSE_WEBGL2=1 -sNO_FILESYSTEM=1 -sASSERTIONS=0 -sMALLOC=emmalloc --closure=1)
endif()

if (MSVC)
    target_compile_options(apple2 PUBLIC /W3)
else()
    target_compile_options(apple2 PUBLIC -Wall -Wextra -Wsign-compare)
endif()

# explicitly strip dead code
if (CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_SYSTEM_NAME STREQUAL Emscripten)
    target_link_options(apple2 PRIVATE LINKER:-dead_strip)
endif()
