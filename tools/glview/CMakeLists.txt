add_executable(glview main.cpp)
target_link_libraries(glview PRIVATE gl_platform SDL2::SDL2main SDL2::SDL2)
gl_enable_warnings(glview)
