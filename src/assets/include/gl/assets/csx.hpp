#pragma once
#include "gl/assets/bytes.hpp"
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {
struct Rgba { std::uint8_t r = 0, g = 0, b = 0, a = 0; };

struct CsxImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t transparent_index = 0;
    std::vector<std::uint32_t> palette_bgr;
    std::vector<std::uint8_t> indices;
    std::vector<Rgba> rgba;
};

Result<CsxImage> decode_csx(std::span<const std::uint8_t> data);
}
