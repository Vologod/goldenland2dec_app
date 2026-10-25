#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {

struct IadCrop {
    std::int16_t x1 = 0;
    std::int16_t y1 = 0;
    std::int16_t x2 = 0;
    std::int16_t y2 = 0;
};

struct IadBlock {
    std::uint32_t type = 0;
    std::uint32_t size = 0;
    std::uint32_t frames = 0;
    std::uint32_t frame_width = 0;
    std::uint32_t frame_height = 0;
    std::int32_t layer_offset_x = 0;
    std::int32_t layer_offset_y = 0;
    std::vector<IadCrop> crops;
    std::uint32_t shadow_frame_width = 0;
    std::uint32_t shadow_frame_height = 0;
    std::int32_t shadow_layer_offset_x = 0;
    std::int32_t shadow_layer_offset_y = 0;
    std::vector<IadCrop> shadow_crops;
};

Result<std::vector<IadBlock>> parse_iad(std::span<const std::uint8_t> data);

} // namespace gl::assets
