add_executable(gl_game_tests
    test_main.cpp
    test_dialog_path.cpp
    test_game_script_host.cpp
    test_hero.cpp
    test_hero_outfit_resolver.cpp
    test_hero_wear.cpp
    test_inventory_runtime.cpp
    test_inventory_runtime_real.cpp
    test_inventory_view.cpp
    test_level_simulation.cpp
    test_mask_selection.cpp
    test_msvc_random.cpp
    test_npc_loot.cpp
    test_npc_loot_specials.cpp
    test_pathfinder.cpp
    test_person_loader.cpp
    test_person_movement.cpp
    test_person_runtime.cpp
    test_person_runtime_real.cpp
    test_person_rng.cpp
    test_real_person_loader.cpp
    test_special_engine.cpp
    test_special_gates.cpp
    test_stability.cpp
    test_trade_price.cpp
    test_walk_grid.cpp
)
target_include_directories(gl_game_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_game_tests PRIVATE gl_game)
gl_enable_warnings(gl_game_tests)

add_test(NAME gl_game_tests COMMAND gl_game_tests)

