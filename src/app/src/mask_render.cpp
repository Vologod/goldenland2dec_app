#include "gl/app/mask_render.hpp"

#include <cstddef>
#include <cstdint>

namespace gl::app {

gl::assets::Image compose_mask_foreground(const gl::assets::Image& background,
                                          const gl::assets::Image& mask, int x, int y) {
    gl::assets::Image out;
    if (mask.width == 0 || mask.height == 0 ||
        mask.rgba.size() != static_cast<std::size_t>(mask.width) * mask.height) {
        return out;
    }

    out.width = mask.width;
    out.height = mask.height;
    out.rgba.assign(static_cast<std::size_t>(mask.width) * mask.height,
                    gl::assets::Rgba{});

    for (std::uint32_t py = 0; py < mask.height; ++py) {
        for (std::uint32_t px = 0; px < mask.width; ++px) {
            const std::size_t index =
                static_cast<std::size_t>(py) * mask.width + px;
            if (mask.rgba[index].a >= 128) continue;

            const int bx = x + static_cast<int>(px);
            const int by = y + static_cast<int>(py);
            if (bx < 0 || by < 0 || bx >= static_cast<int>(background.width) ||
                by >= static_cast<int>(background.height)) {
                continue;
            }

            const std::size_t source =
                static_cast<std::size_t>(by) * background.width + bx;
            out.rgba[index] = gl::assets::Rgba{background.rgba[source].r,
                                                background.rgba[source].g,
                                                background.rgba[source].b, 255};
        }
    }
    return out;
}

}
