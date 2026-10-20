#include "gl/render/text.hpp"

#include <SDL.h>
#include <doctest.h>

#include <cstdint>
#include <vector>

namespace {

class SoftwareCanvas {
  public:
    explicit SoftwareCanvas(int width, int height)
        : surface_(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888)),
          renderer_(surface_ != nullptr ? SDL_CreateSoftwareRenderer(surface_) : nullptr) {
        REQUIRE(surface_ != nullptr);
        REQUIRE(renderer_ != nullptr);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
    }

    ~SoftwareCanvas() {
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        if (surface_ != nullptr) {
            SDL_FreeSurface(surface_);
        }
    }

    SoftwareCanvas(const SoftwareCanvas &) = delete;
    SoftwareCanvas &operator=(const SoftwareCanvas &) = delete;

    SDL_Renderer* renderer() const { return renderer_; }

    std::vector<std::uint32_t> read_pixels() const {
        std::vector<std::uint32_t> pixels(static_cast<std::size_t>(surface_->w) *
                                          static_cast<std::size_t>(surface_->h));
        REQUIRE(SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(),
                                     surface_->w * static_cast<int>(sizeof(std::uint32_t))) == 0);
        return pixels;
    }

  private:
    SDL_Surface* surface_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

bool has_non_black_pixel(const std::vector<std::uint32_t> &pixels) {
    for (const std::uint32_t pixel : pixels) {
        if ((pixel & 0x00FFFFFFu) != 0) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("text metrics scale with the requested size") {
    CHECK(gl::render::text_width("ABC", 2.0f) > 0.0f);
    CHECK(gl::render::text_width("ABC", 2.0f) ==
          doctest::Approx(2.0f * gl::render::text_width("ABC", 1.0f)));
    CHECK(gl::render::text_height(1.0f) > 0.0f);
}

TEST_CASE("non-ASCII bytes are substituted with question marks") {
    CHECK(gl::render::text_width("\xD0\x90", 1.0f) ==
          gl::render::text_width("??", 1.0f));
}

TEST_CASE("draw_text renders visible pixels") {
    SoftwareCanvas canvas(200, 50);
    gl::render::draw_text(canvas.renderer(), "HELLO", 1.0f, 1.0f, 2.0f,
                          gl::render::Color{255, 255, 255, 255});
    CHECK(has_non_black_pixel(canvas.read_pixels()));
}

TEST_CASE("draw_text with an empty string draws nothing") {
    SoftwareCanvas canvas(200, 50);
    gl::render::draw_text(canvas.renderer(), "", 1.0f, 1.0f, 2.0f,
                          gl::render::Color{255, 255, 255, 255});
    CHECK_FALSE(has_non_black_pixel(canvas.read_pixels()));
}
