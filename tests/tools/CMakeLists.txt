add_executable(glpak_cli_tests test_glpak_extract.cpp)
target_include_directories(glpak_cli_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(glpak_cli_tests PRIVATE miniz)
target_compile_definitions(glpak_cli_tests PRIVATE GLPAK_EXE="$<TARGET_FILE:glpak>")
add_dependencies(glpak_cli_tests glpak)
gl_enable_warnings(glpak_cli_tests)

add_test(NAME glpak_cli_tests COMMAND glpak_cli_tests)
