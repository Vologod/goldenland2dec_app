#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {

struct IadCrop {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t w = 0;
    std::int16_t h = 0;
};

struct IadBlock {
    std::uint32_t type = 0;
    std::uint32_t size = 0;
    std::uint32_t frames = 0;
    std::uint32_t frame_width = 0;
    std::uint32_t frame_height = 0;
    std::uint32_t image_width = 0;
    std::uint32_t image_height = 0;
    std::vector<IadCrop> crops;
};

Result<std::vector<IadBlock>> parse_iad(std::span<const std::uint8_t> data);

} // namespace gl::assets
