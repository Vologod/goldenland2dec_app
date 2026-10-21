add_executable(glscript main.cpp)
target_link_libraries(glscript PRIVATE gl_script)
gl_enable_warnings(glscript)
