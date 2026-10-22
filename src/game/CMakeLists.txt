add_library(gl_game STATIC
    src/level_loader.cpp
    src/person.cpp
    src/person_movement.cpp
)
target_include_directories(gl_game PUBLIC include)
target_link_libraries(gl_game PUBLIC gl_assets)
gl_enable_warnings(gl_game)
