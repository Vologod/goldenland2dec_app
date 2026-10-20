#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {
struct LaoInfo {
    std::uint32_t height = 0, delay = 0;
};

Result<std::vector<LaoInfo>> parse_lao(std::span<const std::uint8_t> data);
}
