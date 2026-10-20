add_executable(gl_core_tests
    test_main.cpp
    test_config.cpp
    test_command.cpp
    test_console.cpp
)
target_include_directories(gl_core_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_core_tests PRIVATE gl_core)
gl_enable_warnings(gl_core_tests)

add_test(NAME gl_core_tests COMMAND gl_core_tests)

add_executable(gl_core_real_tests
    test_real_main.cpp
    test_real_config.cpp
)
target_include_directories(gl_core_real_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_core_real_tests PRIVATE gl_core)
gl_enable_warnings(gl_core_real_tests)

add_test(NAME gl_core_real_tests COMMAND gl_core_real_tests)
set_tests_properties(gl_core_real_tests PROPERTIES SKIP_RETURN_CODE 77)
