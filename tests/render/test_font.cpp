#include "gl/render/font.hpp"

#include <SDL.h>
#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
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

    SoftwareCanvas(const SoftwareCanvas&) = delete;
    SoftwareCanvas& operator=(const SoftwareCanvas&) = delete;

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

std::size_t non_black_pixel_count(const std::vector<std::uint32_t>& pixels) {
    std::size_t count = 0;
    for (const std::uint32_t pixel : pixels) {
        if ((pixel & 0x00FFFFFFu) != 0) {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST_CASE("missing font file reports an error") {
    SoftwareCanvas canvas(64, 64);
    const gl::Result<gl::render::Font> font =
        gl::render::Font::load_from_file(canvas.renderer(), "no/such/font.ttf", 18.0f);
    REQUIRE_FALSE(font.has_value());
    CHECK(font.error().code == gl::ErrorCode::io);
}

TEST_CASE("system font measures Cyrillic text") {
    const std::optional<std::filesystem::path> path = gl::render::find_system_font();
    if (!path.has_value()) {
        return;
    }

    SoftwareCanvas canvas(64, 64);
    const gl::Result<gl::render::Font> loaded =
        gl::render::Font::load_from_file(canvas.renderer(), *path, 20.0f);
    REQUIRE(loaded.has_value());
    const gl::render::Font& font = loaded.value();

    CHECK(font.line_height() > 0.0f);
    CHECK(font.measure("Hello").x > 0.0f);
    CHECK(font.measure("Привет").x > 0.0f);
    CHECK(font.measure("Привет").y > 0.0f);
    CHECK(font.measure("a\nb").y > font.line_height());
}

TEST_CASE("wrap splits long Cyrillic text on spaces") {
    const std::optional<std::filesystem::path> path = gl::render::find_system_font();
    if (!path.has_value()) {
        return;
    }

    SoftwareCanvas canvas(64, 64);
    const gl::Result<gl::render::Font> loaded =
        gl::render::Font::load_from_file(canvas.renderer(), *path, 20.0f);
    REQUIRE(loaded.has_value());
    const gl::render::Font& font = loaded.value();

    const std::vector<std::string> lines = font.wrap(
        "Это очень длинная фраза на русском языке, которая не помещается в узкую ширину.", 140.0f);
    REQUIRE(lines.size() >= 2);
    for (const std::string& line : lines) {
        CHECK_FALSE(line.empty());
        CHECK(font.measure(line).x <= 160.0f);
    }

    const std::vector<std::string> paragraphs = font.wrap("Первая строка\nВторая строка", 1000.0f);
    REQUIRE(paragraphs.size() == 2);
    CHECK(paragraphs[0] == "Первая строка");
    CHECK(paragraphs[1] == "Вторая строка");
}

TEST_CASE("ttf font draws visible Cyrillic pixels") {
    const std::optional<std::filesystem::path> path = gl::render::find_system_font();
    if (!path.has_value()) {
        return;
    }

    SoftwareCanvas canvas(240, 80);
    const gl::Result<gl::render::Font> loaded =
        gl::render::Font::load_from_file(canvas.renderer(), *path, 24.0f);
    REQUIRE(loaded.has_value());
    const gl::render::Font& font = loaded.value();

    font.draw_text(canvas.renderer(), "Привет, мир!", 4.0f, 4.0f,
                   gl::render::Color{255, 255, 255, 255});
    const std::size_t painted = non_black_pixel_count(canvas.read_pixels());
    CAPTURE(painted);
    CHECK(painted > 100);
}

TEST_CASE("ttf font draws nothing for empty text") {
    const std::optional<std::filesystem::path> path = gl::render::find_system_font();
    if (!path.has_value()) {
        return;
    }

    SoftwareCanvas canvas(120, 40);
    const gl::Result<gl::render::Font> loaded =
        gl::render::Font::load_from_file(canvas.renderer(), *path, 20.0f);
    REQUIRE(loaded.has_value());
    const gl::render::Font& font = loaded.value();

    font.draw_text(canvas.renderer(), "", 2.0f, 2.0f, gl::render::Color{255, 255, 255, 255});
    CHECK(non_black_pixel_count(canvas.read_pixels()) == 0);
}
