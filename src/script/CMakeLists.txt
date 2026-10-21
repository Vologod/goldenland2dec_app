add_library(gl_script STATIC
    src/dialog_session.cpp
    src/globals.cpp
    src/host.cpp
    src/script_vm.cpp
)
target_include_directories(gl_script PUBLIC include)
target_link_libraries(gl_script PUBLIC gl_assets)
gl_enable_warnings(gl_script)
