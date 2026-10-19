#pragma once
#include "gl/assets/result.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace gl::assets {
struct LvlVersion {
    std::uint16_t major = 0, minor = 0;
};
struct LvlMapSize {
    std::uint32_t pixel_width = 1, pixel_height = 1;
};
struct LvlTile {
    std::uint16_t relief = 0, sound = 0, mask = 0xFFFF;
};
struct LvlDescription {
    std::uint16_t param1 = 0, param2 = 0;
    std::uint32_t number = 0;
    std::int32_t x = 0, y = 0;
    std::string name;
};
struct LvlMask {
    std::uint32_t number = 0, x = 0, y = 0;
};
struct LvlCellGroup {
    std::string name;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> cells;
};
struct LvlData {
    LvlVersion version;
    LvlMapSize map_size;
    std::uint32_t chunk_width = 0, chunk_height = 0;
    std::vector<std::array<LvlTile, 4>> chunks;
    std::vector<LvlMask> masks;
    std::vector<LvlDescription> statics, animations, triggers;
    std::vector<LvlCellGroup> cell_groups;
    std::uint32_t level_floors = 0;
};
Result<LvlData> parse_lvl(std::span<const std::uint8_t> data);
}
