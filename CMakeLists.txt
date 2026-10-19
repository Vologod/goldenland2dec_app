cmake_minimum_required(VERSION 3.20)
project(goldenland2dec LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(GL_BUILD_TESTS "Build tests" ON)
option(GL_BUILD_TOOLS "Build CLI tools" ON)

add_subdirectory(third_party/miniz)

add_library(gl_assets STATIC
    src/assets/src/log.cpp
    src/assets/src/byte_reader.cpp
    src/assets/src/cp1251.cpp
    src/assets/src/pak.cpp
    src/assets/src/vfs.cpp
    src/assets/src/csx.cpp
    src/assets/src/sdb.cpp
    src/assets/src/itm.cpp
    src/assets/src/lvl.cpp
    src/assets/src/image.cpp
    src/assets/src/stb_impl.cpp
)
target_include_directories(gl_assets PUBLIC src/assets/include)
target_link_libraries(gl_assets PUBLIC miniz)
target_include_directories(gl_assets PRIVATE third_party/stb)
include(cmake/CompilerWarnings.cmake)
gl_enable_warnings(gl_assets)

include(cmake/SDL2.cmake)

add_subdirectory(src/platform)
add_subdirectory(src/render)
add_subdirectory(src/game)

if (GL_BUILD_TOOLS)
    add_subdirectory(tools/glpak)
    add_subdirectory(tools/glview)
endif()

if (GL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/assets)
    if (GL_BUILD_TOOLS)
        add_subdirectory(tests/tools)
    endif()
endif()
