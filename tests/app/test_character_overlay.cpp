#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/character_overlay.hpp>
#include <gl/app/hero_portrait.hpp>
#include <gl/assets/hero_equipment.hpp>
#include <gl/assets/herowear.hpp>
#include <gl/assets/sdb.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/game/hero_outfit_resolver.hpp>
#include <gl/game/hero_wear.hpp>
#include <gl/render/font.hpp>
#include <gl/render/texture.hpp>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
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

std::vector<std::uint8_t> snapshot(SDL_Renderer *renderer, SDL_Surface *surface) {
    SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                         surface->pixels, surface->pitch);
    const auto *pixels = static_cast<const std::uint8_t *>(surface->pixels);
    return {pixels, pixels + static_cast<std::size_t>(surface->pitch) * surface->h};
}

std::size_t region_difference(const std::vector<std::uint8_t> &a,
                              const std::vector<std::uint8_t> &b, int x, int y,
                              int w, int h, int stride) {
    std::size_t different = 0;
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) {
            const std::size_t offset =
                static_cast<std::size_t>(py) * static_cast<std::size_t>(stride) +
                static_cast<std::size_t>(px) * 4;
            if (a[offset] != b[offset] || a[offset + 1] != b[offset + 1] ||
                a[offset + 2] != b[offset + 2])
                ++different;
        }
    }
    return different;
}

gl::game::CharacterSheet make_sheet() {
    gl::game::CharacterSheet sheet;
    sheet.name = "Hero";
    auto add = [](std::vector<gl::game::CharacterRow> &rows, const char *label,
                  const char *value, bool modeled = true) {
        rows.push_back(gl::game::CharacterRow{label, value, modeled});
    };
    for (int i = 0; i < 8; ++i) add(sheet.primary, "PRIMARY", std::to_string(i + 1).c_str());
    for (int i = 0; i < 4; ++i) add(sheet.personality, "PERSON", std::to_string(i).c_str());
    for (int i = 0; i < 15; ++i)
        add(sheet.characteristics, "CHAR", i < 5 ? "1" : "-", i < 5);
    for (int i = 0; i < 27; ++i) add(sheet.skills, "SKILL", std::to_string(i).c_str());
    return sheet;
}

std::optional<gl::render::Font> load_font(SDL_Renderer *renderer,
                                          const gl::assets::GameVfs &vfs, float size) {
    auto data = vfs.read("engineres/fonts/ttf/arial.ttf");
    if (!data) return std::nullopt;
    auto font = gl::render::Font::load_from_memory(
        renderer, std::span<const std::uint8_t>(data->data(), data->size()), size, "arial");
    if (!font) return std::nullopt;
    return std::optional<gl::render::Font>(std::move(font.value()));
}
} // namespace

TEST_CASE("character overlay opens, hits tabs and close" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    gl::app::CharacterOverlay overlay;
    CHECK(overlay.open(nullptr, vfs.value(), make_sheet(), nullptr));
    CHECK(overlay.visible());
    CHECK(overlay.tab() == gl::game::CharacterTab::skills);
    CHECK(overlay.hit_test(5, 10) == gl::app::CharacterRegion::tab);
    CHECK(overlay.hit_test(930, 700) == gl::app::CharacterRegion::close);
    CHECK(overlay.hit_test(680, 20) == gl::app::CharacterRegion::inventory_tab);
    CHECK(overlay.handle_mouse(5, 10, true));
    CHECK(overlay.tab() == gl::game::CharacterTab::personality);
    CHECK(overlay.handle_key(SDLK_3));
    CHECK(overlay.tab() == gl::game::CharacterTab::skills);
    CHECK_FALSE(overlay.take_inventory_tab_request());
    CHECK(overlay.handle_mouse(680, 20, true));
    CHECK_FALSE(overlay.visible());
    CHECK(overlay.take_inventory_tab_request());
    CHECK_FALSE(overlay.take_inventory_tab_request());
}

TEST_CASE("character overlay key C and close hit close the sheet" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(nullptr, vfs.value(), make_sheet(), nullptr));
    REQUIRE(overlay.handle_mouse(930, 700, false));
    CHECK(overlay.visible());
    REQUIRE(overlay.handle_mouse(930, 700, true));
    CHECK_FALSE(overlay.visible());

    REQUIRE(overlay.open(nullptr, vfs.value(), make_sheet(), nullptr));
    CHECK(overlay.handle_key(SDLK_c));
    CHECK_FALSE(overlay.visible());
}

TEST_CASE("character overlay renders different tab content" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    SoftwareRenderer software(1024, 768);
    REQUIRE(software.renderer != nullptr);
    auto title = load_font(software.renderer, vfs.value(), 18.0f);
    auto rows = load_font(software.renderer, vfs.value(), 16.0f);
    REQUIRE(title.has_value());
    REQUIRE(rows.has_value());

    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), nullptr));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.set_tab(gl::game::CharacterTab::skills);
    overlay.render(software.renderer, 1024, 768);
    const auto skills = snapshot(software.renderer, software.surface);
    overlay.set_tab(gl::game::CharacterTab::characteristics);
    overlay.render(software.renderer, 1024, 768);
    const auto characteristics = snapshot(software.renderer, software.surface);
    overlay.set_tab(gl::game::CharacterTab::personality);
    overlay.render(software.renderer, 1024, 768);
    const auto personality = snapshot(software.renderer, software.surface);
    overlay.set_tab(gl::game::CharacterTab::state);
    overlay.render(software.renderer, 1024, 768);
    const auto state = snapshot(software.renderer, software.surface);
    overlay.set_tab(gl::game::CharacterTab::resistance);
    overlay.render(software.renderer, 1024, 768);
    const auto resistance = snapshot(software.renderer, software.surface);

    const int stride = software.surface->pitch;
    CHECK(region_difference(skills, characteristics, 289, 126, 140, 80, stride) > 0);
    CHECK(region_difference(skills, characteristics, 296, 109, 155, 60, stride) > 0);
    CHECK(region_difference(personality, skills, 296, 100, 272, 140, stride) > 0);
    CHECK(region_difference(state, resistance, 296, 290, 650, 40, stride) == 0);
    CHECK(region_difference(skills, state, 296, 290, 650, 40, stride) > 0);
    CHECK(region_difference(skills, personality, 20, 60, 130, 80, stride) == 0);
}

TEST_CASE("character overlay renders the equipped doll in the doll region" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    SoftwareRenderer software(1024, 768);
    REQUIRE(software.renderer != nullptr);
    auto title = load_font(software.renderer, vfs.value(), 18.0f);
    auto rows = load_font(software.renderer, vfs.value(), 16.0f);
    REQUIRE(title.has_value());
    REQUIRE(rows.has_value());
    gl::game::HeroOutfit outfit;
    outfit.wear_set = "noweapon_thrw";

    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), &outfit));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    const auto with_doll = snapshot(software.renderer, software.surface);
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), nullptr));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    const auto without_doll = snapshot(software.renderer, software.surface);
    CHECK(region_difference(with_doll, without_doll, 8, 296, 270, 320,
                            software.surface->pitch) > 100);
}

TEST_CASE("hero portrait composes the equipped idle frame" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    gl::game::HeroOutfit outfit;
    outfit.wear_set = "noweapon_thrw";
    SoftwareRenderer software(300, 340);
    REQUIRE(software.renderer != nullptr);
    gl::app::HeroPortrait portrait;
    REQUIRE(portrait.load(software.renderer, vfs.value(), outfit));
    REQUIRE(portrait.ready());
    CHECK(portrait.animation_type() == gl::assets::PadAnimationType::tb_stay);
    SDL_SetRenderDrawColor(software.renderer, 0, 0, 0, 255);
    SDL_RenderClear(software.renderer);
    portrait.render(software.renderer, 0.0f, 0.0f, 300.0f, 340.0f);
    const auto pixels = snapshot(software.renderer, software.surface);
    std::size_t non_black = 0;
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
        if (pixels[i] != 0 || pixels[i + 1] != 0 || pixels[i + 2] != 0) ++non_black;
    CHECK(non_black > 100);

    portrait.clear();
    CHECK_FALSE(portrait.ready());
}

TEST_CASE("hero portrait loads the real slot0 outfit" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    const std::filesystem::path path =
        std::filesystem::path(dir) / "Saves" / "slot0.sav";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    auto body = gl::assets::decode_save_body(bytes);
    REQUIRE(body.has_value());
    auto equipment = gl::assets::parse_hero_equipment(body.value());
    REQUIRE(equipment.has_value());
    auto tech_data = vfs->read("sdb/items/tech_names.sdb");
    REQUIRE(tech_data.has_value());
    auto tech = gl::assets::Sdb::parse(tech_data.value());
    REQUIRE(tech.has_value());
    auto wear_data = vfs->read("scripts/herowear.scr");
    REQUIRE(wear_data.has_value());
    auto wear = gl::assets::parse_herowear(wear_data.value());
    REQUIRE(wear.has_value());
    auto outfit = gl::game::resolve_hero_outfit(equipment.value(), tech.value(), wear.value());
    REQUIRE(outfit.has_value());

    SoftwareRenderer software(300, 340);
    REQUIRE(software.renderer != nullptr);
    gl::app::HeroPortrait portrait;
    REQUIRE(portrait.load(software.renderer, vfs.value(), outfit.value()));
    CHECK(portrait.ready());
    CHECK(portrait.animation_type() == gl::assets::PadAnimationType::tb_stay);
}
