#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include "gl/render/font.hpp"

#include <SDL.h>
#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
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

int main(int argc, char** argv) {
    const bool has_font = gl::render::find_system_font().has_value();
    doctest::Context context(argc, argv);
    const int result = context.run();
    if (!has_font && result == 0) {
        return 77;
    }
    return result;
}

TEST_CASE("font rejects unsupported pixel heights") {
    SoftwareCanvas canvas(64, 64);
    const gl::Result<gl::render::Font> too_large =
        gl::render::Font::load_from_file(canvas.renderer(), "no/such/font.ttf", 100000.0f);
    REQUIRE_FALSE(too_large.has_value());
    CHECK(too_large.error().code == gl::ErrorCode::argument);

    const gl::Result<gl::render::Font> too_small =
        gl::render::Font::load_from_file(canvas.renderer(), "no/such/font.ttf", 4.0f);
    REQUIRE_FALSE(too_small.has_value());
    CHECK(too_small.error().code == gl::ErrorCode::argument);

    const float invalid_heights[] = {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
    };
    for (const float height : invalid_heights) {
        const gl::Result<gl::render::Font> invalid =
            gl::render::Font::load_from_file(canvas.renderer(), "no/such/font.ttf", height);
        REQUIRE_FALSE(invalid.has_value());
        CHECK(invalid.error().code == gl::ErrorCode::argument);
    }
}

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

TEST_CASE("control characters are normalized consistently") {
    const std::optional<std::filesystem::path> path = gl::render::find_system_font();
    if (!path.has_value()) {
        return;
    }

    SoftwareCanvas canvas(120, 80);
    const gl::Result<gl::render::Font> loaded =
        gl::render::Font::load_from_file(canvas.renderer(), *path, 20.0f);
    REQUIRE(loaded.has_value());
    const gl::render::Font& font = loaded.value();

    CHECK(font.measure("a\tb").x == doctest::Approx(font.measure("a    b").x));
    CHECK(font.measure("a\rb").x == doctest::Approx(font.measure("ab").x));
    CHECK(font.measure("a\x01"
                       "\x02"
                       "b")
              .x == doctest::Approx(font.measure("ab").x));
    CHECK(font.measure("a\r\nb").y == doctest::Approx(font.measure("a\nb").y));
    const std::string ignored_controls = std::string("\x01") + "\r";
    CHECK(font.measure(ignored_controls).x == 0.0f);
    CHECK(font.measure(ignored_controls).y == 0.0f);
    CHECK(font.wrap(ignored_controls, 100.0f).empty());

    const std::vector<std::string> lines = font.wrap("a\n   \nb", 1000.0f);
    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == "a");
    CHECK(lines[1].empty());
    CHECK(lines[2] == "b");

    font.draw_text(canvas.renderer(),
                   "\x01"
                   "\x02\r\t",
                   4.0f, 4.0f, gl::render::Color{255, 255, 255, 255});
    CHECK(non_black_pixel_count(canvas.read_pixels()) == 0);
}

TEST_CASE("font atlas covers cp1251 punctuation and symbols") {
    const std::optional<std::filesystem::path> path = gl::render::find_system_font();
    if (!path.has_value()) {
        return;
    }

    SoftwareCanvas canvas(360, 120);
    const gl::Result<gl::render::Font> loaded =
        gl::render::Font::load_from_file(canvas.renderer(), *path, 24.0f);
    REQUIRE(loaded.has_value());
    const gl::render::Font& font = loaded.value();

    const std::string phrase = "«Привет» № 5 ° © ® ™";
    CHECK(font.measure(phrase).x > 0.0f);
    for (const char32_t codepoint : {U'«', U'»', U'№', U'°', U'©', U'®', U'™'}) {
        CAPTURE(static_cast<std::uint32_t>(codepoint));
        CHECK(font.has_glyph(codepoint));
    }
    CHECK(font.has_glyph(U'П'));

    CHECK(font.measure("a\xC2\xA0"
                       "b")
              .x == doctest::Approx(font.measure("a b").x));

    font.draw_text(canvas.renderer(), phrase, 4.0f, 4.0f,
                   gl::render::Color{255, 255, 255, 255});
    const std::size_t painted = non_black_pixel_count(canvas.read_pixels());
    CAPTURE(painted);
    CHECK(painted > 100);
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
