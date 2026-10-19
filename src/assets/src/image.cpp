#include "gl/assets/image.hpp"
#include <climits>
#include <cstring>
#include <new>
#include <stb_image.h>
#include <stb_image_write.h>

namespace gl::assets {
namespace {
constexpr std::uint64_t kMaxImagePixels = 64ull * 1024 * 1024;
}

Result<Image> decode_image(std::span<const std::uint8_t> data) {
    if (data.size() > static_cast<std::size_t>(INT_MAX))
        return Error{ErrorCode::format, "image too large"};
    int w = 0, h = 0, channels = 0;
    if (!stbi_info_from_memory(data.data(), static_cast<int>(data.size()), &w, &h, &channels))
        return Error{ErrorCode::format, stbi_failure_reason()};
    if (w <= 0 || h <= 0) return Error{ErrorCode::format, "bad image dimensions"};
    auto pixel_count = static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h);
    if (pixel_count > kMaxImagePixels)
        return Error{ErrorCode::format, "image pixel limit exceeded"};
    stbi_uc* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()),
                                            &w, &h, &channels, 4);
    if (!pixels) return Error{ErrorCode::format, stbi_failure_reason()};
    if (w <= 0 || h <= 0) {
        stbi_image_free(pixels);
        return Error{ErrorCode::format, "bad image dimensions"};
    }
    pixel_count = static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h);
    if (pixel_count > kMaxImagePixels) {
        stbi_image_free(pixels);
        return Error{ErrorCode::format, "image pixel limit exceeded"};
    }
    Image image;
    image.width = static_cast<std::uint32_t>(w);
    image.height = static_cast<std::uint32_t>(h);
    static_assert(sizeof(Rgba) == 4);
    try {
        image.rgba.resize(static_cast<std::size_t>(pixel_count));
        std::memcpy(image.rgba.data(), pixels, image.rgba.size() * sizeof(Rgba));
    } catch (const std::bad_alloc&) {
        stbi_image_free(pixels);
        return Error{ErrorCode::format, "image allocation failed"};
    }
    stbi_image_free(pixels);
    return image;
}

bool write_png(const std::filesystem::path& path, const Image& image) {
    if (image.width == 0 || image.height == 0) return false;
    if (image.width > static_cast<std::uint32_t>(INT_MAX / 4) ||
        image.height > static_cast<std::uint32_t>(INT_MAX))
        return false;
    const auto expected = static_cast<std::uint64_t>(image.width) * image.height;
    if (expected != image.rgba.size()) return false;
    const std::string narrow = path.string();
    return stbi_write_png(narrow.c_str(), static_cast<int>(image.width),
                          static_cast<int>(image.height), 4, image.rgba.data(),
                          static_cast<int>(image.width) * 4) != 0;
}
}
