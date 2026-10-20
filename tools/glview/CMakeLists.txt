add_executable(glview main.cpp)
target_include_directories(glview PRIVATE ${CMAKE_SOURCE_DIR}/third_party/stb)
target_link_libraries(glview PRIVATE gl_render gl_game SDL2::SDL2main SDL2::SDL2)
gl_enable_warnings(glview)
