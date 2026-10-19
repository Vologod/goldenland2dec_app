#pragma once
#include "gl/assets/csx.hpp"
#include "gl/assets/result.hpp"
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace gl::assets {
struct Image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<Rgba> rgba;
};

Result<Image> decode_image(std::span<const std::uint8_t> data);
bool write_png(const std::filesystem::path& path, const Image& image);
}
