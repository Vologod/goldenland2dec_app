#include "gl/assets/csx_frames.hpp"
#include <algorithm>
#include <cstddef>
#include <new>
#include <string>

namespace gl::assets {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }
}

Result<std::vector<Image>> slice_csx_frames(const CsxImage& image, std::uint32_t frame_height) {
    if (image.width == 0 || image.height == 0) {
        return format_error("CSX frame slice: empty image");
    }
    if (frame_height == 0 || frame_height > image.height) {
        return format_error("CSX frame slice: invalid frame height " +
                            std::to_string(frame_height));
    }
    const auto expected_pixels = static_cast<std::uint64_t>(image.width) * image.height;
    if (expected_pixels != image.rgba.size()) {
        return format_error("CSX frame slice: pixel buffer size mismatch");
    }
    const auto frame_count = static_cast<std::size_t>(
        (static_cast<std::uint64_t>(image.height) + frame_height - 1) / frame_height);
    try {
        std::vector<Image> frames;
        frames.reserve(frame_count);
        for (std::uint32_t top = 0; top < image.height; top += frame_height) {
            const std::uint32_t rows = std::min(frame_height, image.height - top);
            const auto offset = static_cast<std::size_t>(top) * image.width;
            const auto count = static_cast<std::size_t>(rows) * image.width;
            Image frame;
            frame.width = image.width;
            frame.height = rows;
            frame.rgba.assign(image.rgba.begin() + static_cast<std::ptrdiff_t>(offset),
                              image.rgba.begin() + static_cast<std::ptrdiff_t>(offset + count));
            frames.push_back(std::move(frame));
        }
        return frames;
    } catch (const std::bad_alloc&) {
        return format_error("CSX frame slice: allocation failed");
    }
}
}
