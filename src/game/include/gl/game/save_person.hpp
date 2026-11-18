#pragma once
#include "gl/assets/result.hpp"
#include "gl/assets/save_item.hpp"
#include "gl/assets/save_payload.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gl::game {
struct SaveContainer {
    bool present = false;
    std::uint32_t flag = 0;
    std::vector<gl::assets::SaveItemRecord> items;
    std::vector<std::uint8_t> raw_items;
};

struct SavePersonRecord {
    std::string name;
    std::uint32_t field_40 = 0;
    std::uint32_t field_44 = 0;
    std::int32_t current_tile_x = 0;
    std::int32_t current_tile_y = 0;
    std::int32_t anchor_tile_x = 0;
    std::int32_t anchor_tile_y = 0;
    std::uint32_t field_654 = 0;
    std::array<std::uint8_t, 0x20> field_658{};
    std::uint8_t direction = 0;
    std::int32_t route_type = 0;
    std::string route_name;
    std::int32_t radius = 0;
    std::int32_t delay_min = 0;
    std::int32_t delay_max = 0;
    std::int32_t tribe_id = 0;
    std::string dialog;
    std::string inventory;
    std::uint32_t field_6cc = 0;
    std::array<std::uint8_t, 0x11C> field_ff0{};
    std::uint32_t field_f50 = 0;
    std::int32_t hp = 0;
    std::int32_t energy = 0;
    std::int32_t ap = 0;
    std::int32_t route_index = 0;
    std::array<SaveContainer, 3> containers;
    std::array<std::uint8_t, 0x80> tail80{};
    std::uint32_t tail_count = 0;
    std::vector<std::array<std::uint8_t, 0x14>> extra;
};

Result<SavePersonRecord> decode_save_person(const gl::assets::SaveEntity &entity);
Result<std::vector<std::uint8_t>> encode_save_person_body(const SavePersonRecord &record);
} // namespace gl::game
