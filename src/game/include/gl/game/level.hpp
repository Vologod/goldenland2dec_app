#pragma once
#include "gl/assets/image.hpp"
#include "gl/assets/result.hpp"
#include "gl/assets/vfs.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gl::game {
struct LevelStatic {
    std::int32_t x = 0, y = 0;
    gl::assets::Image image;
};

struct LevelAnimation {
    std::int32_t x = 0, y = 0;
    std::uint32_t delay_ms = 0;
    std::vector<gl::assets::Image> frames;
};

struct Level {
    std::string name, pack;
    std::uint32_t width = 0, height = 0;
    gl::assets::Image background;
    std::vector<LevelStatic> statics;
    std::vector<LevelAnimation> animations;
};

gl::Result<Level> load_level(gl::assets::GameVfs &vfs, std::string_view level_name);
} // namespace gl::game
