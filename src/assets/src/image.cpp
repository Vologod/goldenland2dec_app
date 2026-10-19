#include "gl/assets/image.hpp"
#include <stb_image.h>
#include <stb_image_write.h>

namespace gl::assets {
Result<Image> decode_image(std::span<const std::uint8_t> data) {
    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()),
                                            &w, &h, &channels, 4);
    if (!pixels) return Error{ErrorCode::format, stbi_failure_reason()};
    Image image;
    image.width = static_cast<std::uint32_t>(w);
    image.height = static_cast<std::uint32_t>(h);
    image.rgba.resize(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < image.rgba.size(); ++i) {
        image.rgba[i] = Rgba{pixels[i * 4 + 0], pixels[i * 4 + 1], pixels[i * 4 + 2], pixels[i * 4 + 3]};
    }
    stbi_image_free(pixels);
    return image;
}

bool write_png(const std::filesystem::path& path, const Image& image) {
    const std::string narrow = path.string();
    return stbi_write_png(narrow.c_str(), static_cast<int>(image.width),
                          static_cast<int>(image.height), 4, image.rgba.data(),
                          static_cast<int>(image.width) * 4) != 0;
}
}
