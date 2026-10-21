add_executable(gl_game_tests
    test_main.cpp
    test_person_loader.cpp
    test_real_person_loader.cpp
)
target_include_directories(gl_game_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_game_tests PRIVATE gl_game)
gl_enable_warnings(gl_game_tests)

add_test(NAME gl_game_tests COMMAND gl_game_tests)
