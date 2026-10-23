add_executable(glgame_screenshot_tests test_glgame_screenshot.cpp)
target_include_directories(glgame_screenshot_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest
                                                           ${CMAKE_SOURCE_DIR}/third_party/stb)
target_link_libraries(glgame_screenshot_tests PRIVATE gl_render gl_assets SDL2::SDL2)
target_compile_definitions(glgame_screenshot_tests PRIVATE GLGAME_EXE="$<TARGET_FILE:glgame>")
add_dependencies(glgame_screenshot_tests glgame)
gl_enable_warnings(glgame_screenshot_tests)

add_test(NAME glgame_screenshot_tests COMMAND glgame_screenshot_tests)
set_tests_properties(glgame_screenshot_tests PROPERTIES SKIP_RETURN_CODE 77)

add_executable(person_render_tests test_person_render.cpp)
target_include_directories(person_render_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
target_link_libraries(person_render_tests PRIVATE gl_app)
gl_enable_warnings(person_render_tests)

add_test(NAME person_render_tests COMMAND person_render_tests)

add_executable(level_click_tests test_level_click.cpp)
target_include_directories(level_click_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
target_link_libraries(level_click_tests PRIVATE gl_app)
gl_enable_warnings(level_click_tests)

add_test(NAME level_click_tests COMMAND level_click_tests)
