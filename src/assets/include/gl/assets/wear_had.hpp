#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {

struct WearHadAnimation {
    std::uint32_t type_mask = 0;
    std::uint32_t rows = 0;
    std::uint32_t columns = 0;
    std::uint32_t delay_ms = 0;
    std::uint32_t frame_width = 0;
    std::uint32_t frame_height = 0;
    std::int32_t anchor_x = 0;
    std::int32_t anchor_y = 0;
    float movement_x = 0.0f;
    float movement_y = 0.0f;
    std::int32_t layer_offset_x = 0;
    std::int32_t layer_offset_y = 0;
    std::int32_t shadow_offset_x = 0;
    std::int32_t shadow_offset_y = 0;
    std::uint32_t shadow_width = 0;
    std::uint32_t shadow_height = 0;
    std::int32_t shadow_anchor_x = 0;
    std::int32_t shadow_anchor_y = 0;
};

Result<std::vector<WearHadAnimation>> parse_wear_had(std::span<const std::uint8_t> data);

} // namespace gl::assets
