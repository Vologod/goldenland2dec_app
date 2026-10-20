#pragma once

#include <SDL.h>

#include <cstdint>
#include <string_view>

namespace gl::render {

struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

void draw_text(SDL_Renderer* renderer, std::string_view ascii, float x, float y, float scale,
               Color color);

float text_width(std::string_view ascii, float scale);

float text_height(float scale);

}
