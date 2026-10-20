#include "gl/render/texture.hpp"

#include <climits>
#include <cstdint>
#include <string>
#include <utility>

namespace gl::render {

Texture::Texture(SDL_Texture* texture, int width, int height) noexcept
    : texture_(texture), width_(width), height_(height) {}

Texture::~Texture() { reset(); }

Texture::Texture(Texture&& other) noexcept
    : texture_(std::exchange(other.texture_, nullptr)),
      width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)) {}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        reset();
        texture_ = std::exchange(other.texture_, nullptr);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
    }
    return *this;
}

void Texture::reset() noexcept {
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    width_ = 0;
    height_ = 0;
}

Result<Texture> Texture::from_image(SDL_Renderer* renderer, const assets::Image& image) {
    if (renderer == nullptr) return Error{ErrorCode::argument, "cannot create texture: null renderer"};
    if (image.width == 0 || image.height == 0)
        return Error{ErrorCode::format, "cannot create texture: empty image"};
    if (image.width > static_cast<std::uint32_t>(INT_MAX) ||
        image.height > static_cast<std::uint32_t>(INT_MAX))
        return Error{ErrorCode::format, "cannot create texture: image dimensions overflow"};

    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(image.width) * static_cast<std::uint64_t>(image.height);
    if (pixel_count != image.rgba.size())
        return Error{ErrorCode::format, "cannot create texture: RGBA buffer size mismatch"};

    const int width = static_cast<int>(image.width);
    const int height = static_cast<int>(image.height);
    if (width > INT_MAX / 4)
        return Error{ErrorCode::format, "cannot create texture: row size overflow"};

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC, width, height);
    if (texture == nullptr)
        return Error{ErrorCode::io, std::string("SDL_CreateTexture failed: ") + SDL_GetError()};

    if (SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) != 0) {
        const std::string error = SDL_GetError();
        SDL_DestroyTexture(texture);
        return Error{ErrorCode::io, "SDL_SetTextureBlendMode failed: " + error};
    }

    if (SDL_UpdateTexture(texture, nullptr, image.rgba.data(), width * 4) != 0) {
        const std::string error = SDL_GetError();
        SDL_DestroyTexture(texture);
        return Error{ErrorCode::io, "SDL_UpdateTexture failed: " + error};
    }

    return Texture(texture, width, height);
}

}
