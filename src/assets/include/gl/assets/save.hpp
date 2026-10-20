#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <string>

namespace gl::assets {
struct SaveHeader {
    std::uint32_t version = 0;
    std::uint16_t year = 0, month = 0, day = 0;
    std::uint16_t time_a = 0, time_b = 0, time_c = 0;
    std::string title;
};

Result<SaveHeader> parse_save_header(std::span<const std::uint8_t> data);
}
