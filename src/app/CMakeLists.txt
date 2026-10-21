add_library(gl_app STATIC
    src/level_screen.cpp
    src/person_render.cpp
)
target_include_directories(gl_app PUBLIC include)
target_link_libraries(gl_app PUBLIC gl_render gl_game)
gl_enable_warnings(gl_app)
