#include "gl/render/camera.hpp"

#include <algorithm>
#include <cmath>

namespace gl::render {

SDL_FRect to_screen(const Camera& camera, float world_x, float world_y, float w, float h) {
    return SDL_FRect{(world_x - camera.x) * camera.zoom, (world_y - camera.y) * camera.zoom,
                     w * camera.zoom, h * camera.zoom};
}

void draw_background(SDL_Renderer* renderer, const Texture& texture, const Camera& camera,
                     int screen_w, int screen_h) {
    if (renderer == nullptr || texture.get() == nullptr) return;
    if (screen_w <= 0 || screen_h <= 0) return;
    if (texture.width() <= 0 || texture.height() <= 0) return;
    if (!(camera.zoom > 0.0f) || !std::isfinite(camera.zoom)) return;
    if (!std::isfinite(camera.x) || !std::isfinite(camera.y)) return;

    const float texture_width = static_cast<float>(texture.width());
    const float texture_height = static_cast<float>(texture.height());
    const float view_width = static_cast<float>(screen_w) / camera.zoom;
    const float view_height = static_cast<float>(screen_h) / camera.zoom;

    const float world_left = std::max(camera.x, 0.0f);
    const float world_top = std::max(camera.y, 0.0f);
    const float world_right = std::min(camera.x + view_width, texture_width);
    const float world_bottom = std::min(camera.y + view_height, texture_height);
    if (world_right <= world_left || world_bottom <= world_top) return;

    int left = static_cast<int>(std::floor(world_left));
    int top = static_cast<int>(std::floor(world_top));
    int right = static_cast<int>(std::ceil(world_right));
    int bottom = static_cast<int>(std::ceil(world_bottom));

    left = std::clamp(left, 0, texture.width());
    top = std::clamp(top, 0, texture.height());
    right = std::clamp(right, left, texture.width());
    bottom = std::clamp(bottom, top, texture.height());
    if (right <= left || bottom <= top) return;

    const SDL_Rect source{left, top, right - left, bottom - top};
    const SDL_FRect destination =
        to_screen(camera, static_cast<float>(source.x), static_cast<float>(source.y),
                  static_cast<float>(source.w), static_cast<float>(source.h));
    SDL_RenderCopyF(renderer, texture.get(), &source, &destination);
}

}
