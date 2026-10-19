add_library(gl_platform STATIC
    src/sdl_context.cpp
)
target_include_directories(gl_platform PUBLIC include)
target_link_libraries(gl_platform PUBLIC SDL2::SDL2)
gl_enable_warnings(gl_platform)
