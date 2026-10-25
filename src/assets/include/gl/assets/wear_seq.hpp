#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {
struct WearSequence {
    std::uint32_t stride = 0;
    std::uint32_t rows = 0;
    std::vector<std::uint8_t> order;
};
Result<WearSequence> parse_wear_seq(std::span<const std::uint8_t> data);
} // namespace gl::assets
