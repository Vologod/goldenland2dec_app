add_executable(glpak main.cpp)
target_link_libraries(glpak PRIVATE gl_assets)
gl_enable_warnings(glpak)
