#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/diary_overlay.hpp>
#include <gl/assets/vfs.hpp>

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    doctest::Context context(argc, argv);
    return context.run();
}

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

struct SoftwareRenderer {
    SoftwareRenderer(int width, int height) {
        surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
        if (surface != nullptr) renderer = SDL_CreateSoftwareRenderer(surface);
    }
    ~SoftwareRenderer() {
        if (renderer != nullptr) SDL_DestroyRenderer(renderer);
        if (surface != nullptr) SDL_FreeSurface(surface);
    }
    SoftwareRenderer(const SoftwareRenderer &) = delete;
    SoftwareRenderer &operator=(const SoftwareRenderer &) = delete;

    SDL_Surface *surface = nullptr;
    SDL_Renderer *renderer = nullptr;
};

std::optional<gl::render::Font> load_game_font(SDL_Renderer *renderer,
                                               const gl::assets::GameVfs &vfs) {
    auto bytes = vfs.read("engineres/fonts/ttf/pala.ttf");
    if (!bytes) return std::nullopt;
    auto font = gl::render::Font::load_from_memory(
        renderer, std::span<const std::uint8_t>(bytes->data(), bytes->size()), 18.0f,
        "engineres/fonts/ttf/pala.ttf");
    if (!font) return std::nullopt;
    return std::move(font.value());
}

std::vector<std::uint8_t> read_pixels(SDL_Renderer *renderer) {
    std::vector<std::uint8_t> pixels(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, pixels.data(),
                                 1024 * 4) == 0);
    return pixels;
}

std::size_t region_difference(const std::vector<std::uint8_t> &a,
                              const std::vector<std::uint8_t> &b, int x, int y, int w,
                              int h) {
    std::size_t different = 0;
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) {
            const std::size_t offset =
                (static_cast<std::size_t>(py) * 1024 + static_cast<std::size_t>(px)) * 4;
            if (a[offset] != b[offset] || a[offset + 1] != b[offset + 1] ||
                a[offset + 2] != b[offset + 2])
                ++different;
        }
    }
    return different;
}

std::size_t dark_pixels(const std::vector<std::uint8_t> &pixels, int x, int y, int w,
                        int h) {
    std::size_t count = 0;
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) {
            const std::size_t offset =
                (static_cast<std::size_t>(py) * 1024 + static_cast<std::size_t>(px)) * 4;
            if (pixels[offset] < 90 && pixels[offset + 1] < 90 && pixels[offset + 2] < 90)
                ++count;
        }
    }
    return count;
}

std::size_t non_black(const std::vector<std::uint8_t> &pixels, int x, int y, int w, int h) {
    std::size_t count = 0;
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) {
            const std::size_t offset =
                (static_cast<std::size_t>(py) * 1024 + static_cast<std::size_t>(px)) * 4;
            if (pixels[offset] > 16 || pixels[offset + 1] > 16 || pixels[offset + 2] > 16)
                ++count;
        }
    }
    return count;
}

gl::game::GameDatetime datetime(std::uint32_t month, std::uint32_t day) {
    gl::game::GameDatetime value;
    value.year = 2024;
    value.month = month;
    value.day = day;
    value.hour = 10;
    value.minute = 30;
    return value;
}

std::vector<gl::game::DiaryRow> make_rows() {
    gl::game::DiaryRow first;
    first.key = 1;
    first.text_id = 7;
    first.start = datetime(3, 5);
    first.title = "Золотой топор Гродвала";
    first.text = "Первое задание: найти старосту деревни и поговорить с ним.";
    gl::game::DiaryRow second;
    second.key = 2;
    second.text_id = 9;
    second.start = datetime(4, 6);
    second.end = datetime(4, 7);
    second.title = "Пятый ученик";
    second.text.clear();
    for (int line = 0; line < 40; ++line)
        second.text += "Строка " + std::to_string(line) +
                       " длинного текста для проверки прокрутки.\n";
    gl::game::DiaryRow third;
    third.key = 3;
    third.text_id = 12;
    third.start = datetime(5, 8);
    third.title = "Третье задание";
    third.text = "Третье задание.";
    return {first, second, third};
}
} // namespace

TEST_CASE("diary overlay renders the frame, list and detail" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    SoftwareRenderer canvas(1024, 768);
    REQUIRE(canvas.renderer != nullptr);
    auto font = load_game_font(canvas.renderer, vfs.value());
    REQUIRE(font.has_value());

    const auto rows = make_rows();
    gl::app::DiaryOverlay overlay;
    REQUIRE(overlay.open(vfs.value(), rows));
    overlay.set_font(&font.value());
    CHECK(overlay.visible());
    CHECK(overlay.row_count() == 3);
    CHECK(overlay.selected() == 0);

    SDL_SetRenderDrawColor(canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(canvas.renderer);
    overlay.render(canvas.renderer, 1024, 768, &font.value());
    const auto pixels = read_pixels(canvas.renderer);
    CHECK(non_black(pixels, 10, 10, 1000, 740) > 200000);
    CHECK(non_black(pixels, 70, 110, 340, 540) > 5000);
    CHECK(non_black(pixels, 455, 110, 490, 540) > 5000);
    CHECK(non_black(pixels, 909, 687, 71, 65) > 500);
    CHECK(dark_pixels(pixels, 455, 110, 490, 540) > 200);
}

TEST_CASE("diary overlay selection, scroll and close" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    SoftwareRenderer canvas(1024, 768);
    REQUIRE(canvas.renderer != nullptr);
    auto font = load_game_font(canvas.renderer, vfs.value());
    REQUIRE(font.has_value());

    const auto rows = make_rows();
    gl::app::DiaryOverlay overlay;
    REQUIRE(overlay.open(vfs.value(), rows));
    overlay.set_font(&font.value());
    overlay.render(canvas.renderer, 1024, 768, &font.value());
    const auto before = read_pixels(canvas.renderer);

    CHECK(overlay.handle_key(SDLK_DOWN));
    CHECK(overlay.selected() == 1);
    overlay.render(canvas.renderer, 1024, 768, &font.value());
    const auto selected = read_pixels(canvas.renderer);
    CHECK(overlay.handle_wheel(-1));
    CHECK(overlay.detail_scroll() > 0);
    overlay.render(canvas.renderer, 1024, 768, &font.value());
    const auto after = read_pixels(canvas.renderer);
    CHECK(region_difference(before, selected, 455, 110, 490, 540) > 100);
    CHECK(region_difference(selected, after, 455, 165, 490, 485) > 0);

    for (int i = 0; i < 100; ++i) overlay.handle_wheel(-1);
    CHECK(overlay.detail_max_scroll() > 0);
    CHECK(overlay.detail_scroll() == overlay.detail_max_scroll());
    for (int i = 0; i < 100; ++i) overlay.handle_wheel(1);
    CHECK(overlay.detail_scroll() == 0);

    CHECK(overlay.handle_mouse(944, 719, true));
    CHECK_FALSE(overlay.visible());
}

TEST_CASE("diary overlay page arrows scroll list and detail" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    SoftwareRenderer canvas(1024, 768);
    REQUIRE(canvas.renderer != nullptr);
    auto font = load_game_font(canvas.renderer, vfs.value());
    REQUIRE(font.has_value());

    std::vector<gl::game::DiaryRow> rows;
    for (int i = 0; i < 60; ++i) {
        gl::game::DiaryRow row;
        row.key = static_cast<std::uint32_t>(i);
        row.start = datetime(3, 5);
        row.title = "Квест " + std::to_string(i);
        row.text.clear();
        for (int line = 0; line < 40; ++line)
            row.text += "Строка " + std::to_string(line) + " длинного описания.\n";
        rows.push_back(std::move(row));
    }
    gl::app::DiaryOverlay overlay;
    REQUIRE(overlay.open(vfs.value(), rows));
    overlay.set_font(&font.value());
    overlay.render(canvas.renderer, 1024, 768, &font.value());
    const auto base = read_pixels(canvas.renderer);

    CHECK(overlay.list_scroll() == 0);
    CHECK(overlay.selected() == 0);
    CHECK(overlay.handle_mouse(118, 726, true));
    overlay.render(canvas.renderer, 1024, 768, &font.value());
    const auto pressed = read_pixels(canvas.renderer);
    CHECK(region_difference(base, pressed, 98, 696, 41, 61) > 20);
    overlay.release();
    overlay.render(canvas.renderer, 1024, 768, &font.value());
    const auto released = read_pixels(canvas.renderer);
    CHECK(region_difference(base, released, 98, 696, 41, 61) == 0);
    CHECK(overlay.list_scroll() > 0);
    CHECK(overlay.selected() == overlay.list_scroll());
    CHECK(overlay.handle_mouse(58, 726, true));
    CHECK(overlay.list_scroll() == 0);
    CHECK(overlay.selected() == 0);

    CHECK(overlay.detail_scroll() == 0);
    CHECK(overlay.handle_mouse(498, 726, true));
    CHECK(overlay.detail_scroll() > 0);
    CHECK(overlay.handle_mouse(438, 726, true));
    CHECK(overlay.detail_scroll() == 0);
}

TEST_CASE("diary overlay handles an empty journal" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    SoftwareRenderer canvas(1024, 768);
    REQUIRE(canvas.renderer != nullptr);
    auto font = load_game_font(canvas.renderer, vfs.value());
    REQUIRE(font.has_value());

    gl::app::DiaryOverlay overlay;
    const std::vector<gl::game::DiaryRow> rows;
    REQUIRE(overlay.open(vfs.value(), rows));
    overlay.set_font(&font.value());
    overlay.render(canvas.renderer, 1024, 768, &font.value());
    CHECK(overlay.row_count() == 0);
    CHECK(overlay.handle_key(SDLK_DOWN));
    CHECK(overlay.handle_wheel(1));
}
