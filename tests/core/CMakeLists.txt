add_executable(gl_core_tests
    test_main.cpp
    test_config.cpp
)
target_include_directories(gl_core_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_core_tests PRIVATE gl_core)
gl_enable_warnings(gl_core_tests)

add_test(NAME gl_core_tests COMMAND gl_core_tests)
