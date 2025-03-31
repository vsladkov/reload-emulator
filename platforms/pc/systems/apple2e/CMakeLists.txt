#=== EXECUTABLE: apple2e

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_executable(apple2e WIN32 ./src/apple2e.c)
else()
    add_executable(apple2e ./src/apple2e.c)
endif()

target_link_libraries(apple2e platform)

if (CMAKE_SYSTEM_NAME STREQUAL Emscripten)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_link_options(apple2e PUBLIC --shell-file ../sokol/shell.html)
    target_link_options(apple2e PUBLIC -sUSE_WEBGL2=1 -sNO_FILESYSTEM=1 -sASSERTIONS=0 -sMALLOC=emmalloc --closure=1)
endif()

if (MSVC)
    target_compile_options(apple2e PUBLIC /W3)
else()
    target_compile_options(apple2e PUBLIC -Wall -Wextra -Wsign-compare)
endif()

# explicitly strip dead code
if (CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_SYSTEM_NAME STREQUAL Emscripten)
    target_link_options(apple2e PRIVATE LINKER:-dead_strip)
endif()
