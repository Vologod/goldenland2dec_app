add_executable(glscript main.cpp)
target_link_libraries(glscript PRIVATE gl_game)
gl_enable_warnings(glscript)
