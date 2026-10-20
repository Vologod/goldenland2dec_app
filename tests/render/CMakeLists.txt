add_executable(glview_screenshot_tests test_glview_screenshot.cpp)
target_include_directories(glview_screenshot_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest
                                                           ${CMAKE_SOURCE_DIR}/third_party/stb)
target_link_libraries(glview_screenshot_tests PRIVATE gl_render gl_assets SDL2::SDL2main
                                                            SDL2::SDL2)
target_compile_definitions(glview_screenshot_tests PRIVATE GLVIEW_EXE="$<TARGET_FILE:glview>")
add_dependencies(glview_screenshot_tests glview)
gl_enable_warnings(glview_screenshot_tests)

add_test(NAME glview_screenshot_tests COMMAND glview_screenshot_tests)
set_tests_properties(glview_screenshot_tests PROPERTIES SKIP_RETURN_CODE 77)
