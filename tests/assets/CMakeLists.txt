add_executable(gl_assets_tests
    test_main.cpp
    test_smoke.cpp
    test_result.cpp
    test_byte_reader.cpp
)
target_include_directories(gl_assets_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_assets_tests PRIVATE gl_assets)
gl_enable_warnings(gl_assets_tests)

add_test(NAME gl_assets_tests COMMAND gl_assets_tests)
