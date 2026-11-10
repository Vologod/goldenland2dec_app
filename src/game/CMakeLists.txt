add_library(gl_game STATIC
    src/dialog_path.cpp
    src/game_script_host.cpp
    src/hero.cpp
    src/hero_outfit_resolver.cpp
    src/hero_wear.cpp
    src/inventory_runtime.cpp
    src/inventory_view.cpp
    src/level_loader.cpp
    src/level_simulation.cpp
    src/mask_selection.cpp
    src/msvc_random.cpp
    src/npc_loot.cpp
    src/pathfinder.cpp
    src/person.cpp
    src/person_movement.cpp
    src/person_runtime.cpp
    src/special_engine.cpp
    src/trade_price.cpp
    src/walk_grid.cpp
)
target_include_directories(gl_game PUBLIC include)
target_link_libraries(gl_game PUBLIC gl_assets gl_script)
gl_enable_warnings(gl_game)

# The trade price formula must use IEEE-754 binary64 (x87 PC=53 equivalent).
# Disable fused multiply-add contraction so a single expression cannot be
# evaluated with extra intermediate precision on GCC/Clang targets (Android
# arm64) or MSVC.
if (MSVC)
    set_source_files_properties(src/trade_price.cpp src/special_engine.cpp PROPERTIES COMPILE_OPTIONS "/fp:precise")
else()
    set_source_files_properties(src/trade_price.cpp src/special_engine.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
endif()
