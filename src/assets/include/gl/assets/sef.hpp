#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <string>

namespace gl::assets {
struct SefData {
    std::string pack;
    bool internal_location = false;
    bool exit_to_global_map = false;
    int weather = -1;
};

Result<SefData> parse_sef(std::span<const std::uint8_t> data);
}
