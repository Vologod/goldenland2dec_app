add_executable(glgame_screenshot_tests test_glgame_screenshot.cpp)
target_include_directories(glgame_screenshot_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest
                                                           ${CMAKE_SOURCE_DIR}/third_party/stb)
target_link_libraries(glgame_screenshot_tests PRIVATE gl_assets)
target_compile_definitions(glgame_screenshot_tests PRIVATE GLGAME_EXE="$<TARGET_FILE:glgame>")
add_dependencies(glgame_screenshot_tests glgame)
gl_enable_warnings(glgame_screenshot_tests)

add_test(NAME glgame_screenshot_tests COMMAND glgame_screenshot_tests)
set_tests_properties(glgame_screenshot_tests PROPERTIES SKIP_RETURN_CODE 77)
