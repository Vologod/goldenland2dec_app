#pragma once
#include "gl/assets/result.hpp"
#include "gl/assets/save_payload.hpp"
#include <cstdint>
#include <string>

namespace gl::game {
struct SavePersonRecord {
    std::string name;
    std::int32_t current_tile_x = 0;
    std::int32_t current_tile_y = 0;
    std::int32_t anchor_tile_x = 0;
    std::int32_t anchor_tile_y = 0;
    std::uint8_t direction = 0;
    std::int32_t route_type = 0;
    std::string route_name;
    std::int32_t radius = 0;
    std::int32_t delay_min = 0;
    std::int32_t delay_max = 0;
    std::int32_t tribe_id = 0;
    std::string dialog;
    std::string inventory;
    std::int32_t hp = 0;
    std::int32_t energy = 0;
    std::int32_t route_index = 0;
};

Result<SavePersonRecord> decode_save_person(const gl::assets::SaveEntity &entity);
} // namespace gl::game
