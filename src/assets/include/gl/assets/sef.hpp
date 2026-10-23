#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace gl::assets {
struct SefPerson {
    std::string name;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::string direction;
    std::string route_type;
    std::string route;
    std::int32_t radius = 0;
    std::int32_t delay_min = 0;
    std::int32_t delay_max = 0;
    std::string tribe;
    std::string dialog;
    std::string inventory;
};

struct SefCellGroup {
    std::string name;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> cells;
};

struct SefEntrance {
    std::string name;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint8_t direction = 0;
};

struct SefData {
    std::string pack;
    bool internal_location = false;
    bool exit_to_global_map = false;
    int weather = -1;
    std::vector<SefPerson> persons;
    std::vector<SefCellGroup> cell_groups;
    std::vector<SefEntrance> entrances;
};

Result<SefData> parse_sef(std::span<const std::uint8_t> data);
}
