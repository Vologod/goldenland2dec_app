#pragma once

#include "gl/render/texture.hpp"

#include <SDL.h>

namespace gl::render {

struct Camera {
    float x = 0.0f;
    float y = 0.0f;
    float zoom = 1.0f;
};

SDL_FRect to_screen(const Camera& camera, float world_x, float world_y, float w, float h);

void clamp_camera(Camera& camera, float world_width, float world_height, int screen_w,
                  int screen_h);

void pan_camera(Camera& camera, float direction_x, float direction_y,
                float speed_px_per_second, float delta_seconds, float world_width,
                float world_height, int screen_w, int screen_h);

void zoom_camera_at(Camera& camera, float screen_x, float screen_y, float factor,
                    float world_width, float world_height, int screen_w, int screen_h,
                    float min_zoom, float max_zoom);

void draw_background(SDL_Renderer* renderer, const Texture& texture, const Camera& camera,
                     int screen_w, int screen_h);

void draw_texture(SDL_Renderer* renderer, const Texture& texture, const Camera& camera,
                  float world_x, float world_y, int screen_w, int screen_h);

}
