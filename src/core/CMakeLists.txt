add_library(gl_core STATIC
    src/config.cpp
    src/command.cpp
    src/console.cpp
    src/state.cpp
    src/server.cpp
    src/save_manager.cpp
)
target_include_directories(gl_core PUBLIC include)
target_link_libraries(gl_core PUBLIC gl_assets)
gl_enable_warnings(gl_core)
