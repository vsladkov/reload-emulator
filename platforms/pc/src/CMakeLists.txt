#=== LIBRARY: platform

# add headers to the the file list because they are useful to have in IDEs
set(SOKOL_HEADERS
    ../lib/sokol/sokol_gfx.h
    ../lib/sokol/sokol_app.h
    ../lib/sokol/sokol_audio.h
    ../lib/sokol/sokol_glue.h
    )

add_library(platform INTERFACE)
target_sources(platform INTERFACE clock.c gfx.c prof.c sokol.c ${SOKOL_HEADERS})

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    # compile sokol.c as Objective-C
    target_compile_options(platform INTERFACE -x objective-c)
    target_link_libraries(platform INTERFACE
        "-framework QuartzCore"
        "-framework Cocoa"
        "-framework MetalKit"
        "-framework Metal"
        "-framework OpenGL"
        "-framework AudioToolbox")
else()
    if (CMAKE_SYSTEM_NAME STREQUAL Linux)
        target_link_libraries(platform INTERFACE X11 Xi Xcursor GL asound dl m)
        target_link_libraries(platform INTERFACE Threads::Threads)
    endif()
endif()
