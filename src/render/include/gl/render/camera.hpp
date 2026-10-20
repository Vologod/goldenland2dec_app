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

void draw_background(SDL_Renderer* renderer, const Texture& texture, const Camera& camera,
                     int screen_w, int screen_h);

void draw_texture(SDL_Renderer* renderer, const Texture& texture, const Camera& camera,
                  float world_x, float world_y, int screen_w, int screen_h);

}
