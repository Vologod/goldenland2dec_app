add_library(gl_render STATIC
    src/texture.cpp
    src/camera.cpp
    src/animation.cpp
)
target_include_directories(gl_render PUBLIC include)
target_link_libraries(gl_render PUBLIC gl_platform gl_assets)
gl_enable_warnings(gl_render)
