add_library(gl_render STATIC
    src/texture.cpp
    src/camera.cpp
    src/animation.cpp
    src/text.cpp
    src/utf8.cpp
    src/font.cpp
    src/stb_truetype_impl.cpp
)
target_include_directories(gl_render PUBLIC include)
target_include_directories(gl_render PRIVATE ${CMAKE_SOURCE_DIR}/third_party/stb)
target_link_libraries(gl_render PUBLIC gl_platform gl_assets)
gl_enable_warnings(gl_render)
