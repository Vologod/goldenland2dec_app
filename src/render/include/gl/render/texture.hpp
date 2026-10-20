#pragma once

#include "gl/assets/image.hpp"
#include "gl/assets/result.hpp"

#include <SDL.h>

namespace gl::render {

class Texture {
public:
    static Result<Texture> from_image(SDL_Renderer* renderer, const assets::Image& image);

    ~Texture();

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] SDL_Texture* get() const noexcept { return texture_; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }

private:
    Texture(SDL_Texture* texture, int width, int height) noexcept;

    void reset() noexcept;

    SDL_Texture* texture_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

}
