add_executable(gl_script_tests
    test_main.cpp
    test_globals.cpp
    test_vm.cpp
)
target_include_directories(gl_script_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_script_tests PRIVATE gl_script)
gl_enable_warnings(gl_script_tests)

add_test(NAME gl_script_tests COMMAND gl_script_tests)
