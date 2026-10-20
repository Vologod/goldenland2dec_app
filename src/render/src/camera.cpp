#include "gl/render/camera.hpp"

#include <algorithm>
#include <cmath>

namespace gl::render {

SDL_FRect to_screen(const Camera& camera, float world_x, float world_y, float w, float h) {
    return SDL_FRect{(world_x - camera.x) * camera.zoom, (world_y - camera.y) * camera.zoom,
                     w * camera.zoom, h * camera.zoom};
}

void clamp_camera(Camera& camera, float world_width, float world_height, int screen_w,
                  int screen_h) {
    if (!(camera.zoom > 0.0f) || !std::isfinite(camera.zoom)) return;
    if (!(world_width > 0.0f) || !(world_height > 0.0f)) return;
    if (!std::isfinite(world_width) || !std::isfinite(world_height)) return;
    if (screen_w <= 0 || screen_h <= 0) return;

    const float view_width = static_cast<float>(screen_w) / camera.zoom;
    const float view_height = static_cast<float>(screen_h) / camera.zoom;

    if (view_width >= world_width) {
        camera.x = (world_width - view_width) * 0.5f;
    } else {
        camera.x = std::clamp(camera.x, 0.0f, world_width - view_width);
    }

    if (view_height >= world_height) {
        camera.y = (world_height - view_height) * 0.5f;
    } else {
        camera.y = std::clamp(camera.y, 0.0f, world_height - view_height);
    }
}

void pan_camera(Camera& camera, float direction_x, float direction_y,
                float speed_px_per_second, float delta_seconds, float world_width,
                float world_height, int screen_w, int screen_h) {
    if (!(camera.zoom > 0.0f) || !std::isfinite(camera.zoom)) return;
    if (!std::isfinite(direction_x) || !std::isfinite(direction_y)) return;
    if (!(speed_px_per_second > 0.0f) || !std::isfinite(speed_px_per_second)) return;
    if (!(delta_seconds > 0.0f) || !std::isfinite(delta_seconds)) return;

    const float length = std::sqrt(direction_x * direction_x + direction_y * direction_y);
    if (!(length > 0.0f)) return;

    const float step = speed_px_per_second / camera.zoom * delta_seconds;
    camera.x += direction_x / length * step;
    camera.y += direction_y / length * step;
    clamp_camera(camera, world_width, world_height, screen_w, screen_h);
}

void zoom_camera_at(Camera& camera, float screen_x, float screen_y, float factor,
                    float world_width, float world_height, int screen_w, int screen_h,
                    float min_zoom, float max_zoom) {
    if (!(camera.zoom > 0.0f) || !std::isfinite(camera.zoom)) return;
    if (!(factor > 0.0f) || !std::isfinite(factor)) return;
    if (!std::isfinite(screen_x) || !std::isfinite(screen_y)) return;

    const float world_x = camera.x + screen_x / camera.zoom;
    const float world_y = camera.y + screen_y / camera.zoom;
    camera.zoom = std::clamp(camera.zoom * factor, min_zoom, max_zoom);
    camera.x = world_x - screen_x / camera.zoom;
    camera.y = world_y - screen_y / camera.zoom;
    clamp_camera(camera, world_width, world_height, screen_w, screen_h);
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

void draw_texture(SDL_Renderer* renderer, const Texture& texture, const Camera& camera,
                  float world_x, float world_y, int screen_w, int screen_h) {
    if (renderer == nullptr || texture.get() == nullptr) return;
    if (texture.width() <= 0 || texture.height() <= 0) return;
    if (screen_w <= 0 || screen_h <= 0) return;
    if (!(camera.zoom > 0.0f) || !std::isfinite(camera.zoom)) return;
    if (!std::isfinite(camera.x) || !std::isfinite(camera.y)) return;
    if (!std::isfinite(world_x) || !std::isfinite(world_y)) return;

    const SDL_FRect destination =
        to_screen(camera, world_x, world_y, static_cast<float>(texture.width()),
                  static_cast<float>(texture.height()));

    const float screen_width = static_cast<float>(screen_w);
    const float screen_height = static_cast<float>(screen_h);
    if (destination.x + destination.w <= 0.0f || destination.y + destination.h <= 0.0f ||
        destination.x >= screen_width || destination.y >= screen_height) {
        return;
    }

    SDL_RenderCopyF(renderer, texture.get(), nullptr, &destination);
}

}
