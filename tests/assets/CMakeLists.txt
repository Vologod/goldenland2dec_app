add_executable(gl_assets_tests
    test_main.cpp
    test_smoke.cpp
    test_result.cpp
    test_byte_reader.cpp
    test_cp1251.cpp
    test_pak.cpp
    test_vfs.cpp
    test_csx.cpp
    test_csx_frames.cpp
    test_sdb.cpp
    test_itm.cpp
    test_image.cpp
    test_lvl.cpp
    test_lao.cpp
    test_sef.cpp
)
target_include_directories(gl_assets_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_assets_tests PRIVATE gl_assets)
gl_enable_warnings(gl_assets_tests)

add_test(NAME gl_assets_tests COMMAND gl_assets_tests)

add_executable(gl_assets_real_tests
    test_real_main.cpp
    test_real_data.cpp
    test_real_levels.cpp
    test_level_loader.cpp
)
target_include_directories(gl_assets_real_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(gl_assets_real_tests PRIVATE gl_assets gl_game)
gl_enable_warnings(gl_assets_real_tests)
add_test(NAME gl_assets_real_tests COMMAND gl_assets_real_tests)
set_tests_properties(gl_assets_real_tests PROPERTIES SKIP_RETURN_CODE 77)
