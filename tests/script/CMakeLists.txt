add_executable(gl_script_tests
    test_main.cpp
    test_globals.cpp
    test_vm.cpp
    test_dialogs.cpp
    test_dialog_session.cpp
    test_native_registry.cpp
    test_person_parameters.cpp
    test_inventory_natives.cpp
    test_inventory_mutations.cpp
    test_transfer_natives.cpp
)
target_include_directories(gl_script_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_script_tests PRIVATE gl_script)
gl_enable_warnings(gl_script_tests)

add_test(NAME gl_script_tests COMMAND gl_script_tests)

add_executable(gl_script_real_tests
    test_real_main.cpp
    test_real_dialogs.cpp
    test_real_dialog_session.cpp
    test_real_person_natives.cpp
    test_real_inventory_natives.cpp
    test_real_inventory_mutations.cpp
)
target_include_directories(gl_script_real_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_script_real_tests PRIVATE gl_script gl_game)
gl_enable_warnings(gl_script_real_tests)

add_test(NAME gl_script_real_tests COMMAND gl_script_real_tests)
set_tests_properties(gl_script_real_tests PROPERTIES SKIP_RETURN_CODE 77)
