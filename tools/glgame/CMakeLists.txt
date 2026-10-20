add_executable(glgame main.cpp)
target_include_directories(glgame PRIVATE ${CMAKE_SOURCE_DIR}/third_party/stb)
target_link_libraries(glgame PRIVATE gl_app gl_core gl_render gl_platform gl_game gl_assets
                                    SDL2::SDL2main SDL2::SDL2)
gl_enable_warnings(glgame)
