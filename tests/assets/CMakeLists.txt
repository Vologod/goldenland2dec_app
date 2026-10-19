add_executable(gl_assets_tests
    test_main.cpp
    test_smoke.cpp
    test_result.cpp
    test_byte_reader.cpp
    test_cp1251.cpp
    test_pak.cpp
    test_vfs.cpp
    test_csx.cpp
    test_sdb.cpp
    test_itm.cpp
    test_image.cpp
)
target_include_directories(gl_assets_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_assets_tests PRIVATE gl_assets)
gl_enable_warnings(gl_assets_tests)

add_test(NAME gl_assets_tests COMMAND gl_assets_tests)

add_executable(gl_assets_real_tests test_main.cpp test_real_data.cpp)
target_include_directories(gl_assets_real_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_assets_real_tests PRIVATE gl_assets)
gl_enable_warnings(gl_assets_real_tests)
add_test(NAME gl_assets_real_tests COMMAND gl_assets_real_tests)
