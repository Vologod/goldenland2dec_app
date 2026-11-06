add_library(gl_game STATIC
    src/dialog_path.cpp
    src/game_script_host.cpp
    src/hero.cpp
    src/hero_outfit_resolver.cpp
    src/hero_wear.cpp
    src/inventory_view.cpp
    src/level_loader.cpp
    src/level_simulation.cpp
    src/mask_selection.cpp
    src/msvc_random.cpp
    src/pathfinder.cpp
    src/person.cpp
    src/person_movement.cpp
    src/person_runtime.cpp
    src/walk_grid.cpp
)
target_include_directories(gl_game PUBLIC include)
target_link_libraries(gl_game PUBLIC gl_assets gl_script)
gl_enable_warnings(gl_game)
