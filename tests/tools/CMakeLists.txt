add_executable(glpak_cli_tests test_glpak_extract.cpp)
target_include_directories(glpak_cli_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(glpak_cli_tests PRIVATE miniz)
target_compile_definitions(glpak_cli_tests PRIVATE GLPAK_EXE="$<TARGET_FILE:glpak>")
add_dependencies(glpak_cli_tests glpak)
gl_enable_warnings(glpak_cli_tests)

add_test(NAME glpak_cli_tests COMMAND glpak_cli_tests)

add_executable(gl_cs_tools_tests test_cs_tools.cpp)
target_include_directories(gl_cs_tools_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_compile_definitions(gl_cs_tools_tests PRIVATE
    GLPAK_EXE="$<TARGET_FILE:glpak>"
    GLSCRIPT_EXE="$<TARGET_FILE:glscript>"
)
add_dependencies(gl_cs_tools_tests glpak glscript)
gl_enable_warnings(gl_cs_tools_tests)

add_test(NAME gl_cs_tools_tests COMMAND gl_cs_tools_tests)
set_tests_properties(gl_cs_tools_tests PROPERTIES SKIP_RETURN_CODE 77)
