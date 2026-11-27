#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/character_overlay.hpp>
#include <gl/assets/hero_equipment.hpp>
#include <gl/assets/image.hpp>
#include <gl/assets/item_catalog.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/render/font.hpp>
#include <gl/render/texture.hpp>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
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
    sheet.name = "Р’РµСЂС‚Р°СЃ";
    sheet.characteristics_title = "РҐРђР РђРљРўР•Р РРЎРўРРљР";
    sheet.skills_title = "РќРђР’Р«РљР";
    sheet.state_title = "РЎРћРЎРўРћРЇРќРР•";
    sheet.combat_title = "Р‘РћР•Р’Р«Р• РҐРђР РђРљРўР•Р РРЎРўРРљР";
    sheet.resistance_title = "РЎРћРџР РћРўРР’Р›Р•РќРР•";
    sheet.immunity_title = "РРњРњРЈРќРРўР•Рў Рљ РњРђР“РР";
    sheet.skill_points_label = "РћС‡РєРё СЂР°СЃРїСЂРµРґРµР»РµРЅРёСЏ";
    auto add = [](std::vector<gl::game::CharacterRow> &rows, const char *label,
                  const char *value, bool modeled = true) {
        rows.push_back(gl::game::CharacterRow{label, value, modeled});
    };
    for (int i = 0; i < 7; ++i) add(sheet.primary, "PRIMARY", "1");
    sheet.experience_points = gl::game::CharacterRow{"РћС‡РєРё РѕРїС‹С‚Р°", "0", true};
    for (int i = 0; i < 6; ++i) add(sheet.state, "STATE", i < 5 ? "1" : "вЂ”", i < 5);
    for (int i = 0; i < 10; ++i) add(sheet.combat, "COMBAT", "вЂ”", false);
    for (int i = 0; i < 12; ++i) add(sheet.resistance, "RESIST", "вЂ”", false);
    for (int i = 0; i < 6; ++i) add(sheet.immunity, "IMMUNE", "вЂ”", false);
    for (int group = 0; group < 3; ++group) {
        gl::game::CharacterGroup entry;
        entry.title = "GROUP";
        for (int i = 0; i < 9; ++i) add(entry.rows, "SKILL", "1");
        sheet.skill_groups.push_back(std::move(entry));
    }
    return sheet;
}

std::optional<gl::render::Font> load_font(SDL_Renderer *renderer,
                                          const gl::assets::GameVfs &vfs, float size) {
    auto data = vfs.read("engineres/fonts/ttf/pala.ttf");
    if (!data) return std::nullopt;
    auto font = gl::render::Font::load_from_memory(
        renderer, std::span<const std::uint8_t>(data->data(), data->size()), size, "pala");
    if (!font) return std::nullopt;
    return std::optional<gl::render::Font>(std::move(font.value()));
}
} // namespace

TEST_CASE("character overlay opens, hits both tabs and close" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    gl::app::CharacterOverlay overlay;
    CHECK(overlay.open(nullptr, vfs.value(), make_sheet(), nullptr, nullptr));
    CHECK(overlay.visible());
    CHECK(overlay.tab() == gl::game::CharacterTab::characteristics);
    CHECK(overlay.hit_test(300, 10) == gl::app::CharacterRegion::tab);
    CHECK(overlay.hit_test(700, 10) == gl::app::CharacterRegion::tab);
    CHECK(overlay.hit_test(930, 700) == gl::app::CharacterRegion::close);
    CHECK(overlay.handle_mouse(700, 10, true));
    CHECK(overlay.tab() == gl::game::CharacterTab::skills);
    CHECK(overlay.handle_key(SDLK_1));
    CHECK(overlay.tab() == gl::game::CharacterTab::characteristics);
    CHECK(overlay.handle_key(SDLK_2));
    CHECK(overlay.tab() == gl::game::CharacterTab::skills);
}

TEST_CASE("character overlay key C and close hit close the sheet" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(nullptr, vfs.value(), make_sheet(), nullptr, nullptr));
    REQUIRE(overlay.handle_mouse(930, 700, false));
    CHECK(overlay.visible());
    REQUIRE(overlay.handle_mouse(930, 700, true));
    CHECK_FALSE(overlay.visible());

    REQUIRE(overlay.open(nullptr, vfs.value(), make_sheet(), nullptr, nullptr));
    CHECK(overlay.handle_key(SDLK_c));
    CHECK_FALSE(overlay.visible());
}

TEST_CASE("character overlay renders the two original pages" *
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
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), nullptr, nullptr));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.set_tab(gl::game::CharacterTab::characteristics);
    overlay.render(software.renderer, 1024, 768);
    const auto characteristics = snapshot(software.renderer, software.surface);
    overlay.set_tab(gl::game::CharacterTab::skills);
    overlay.render(software.renderer, 1024, 768);
    const auto skills = snapshot(software.renderer, software.surface);

    const int stride = software.surface->pitch;
    CHECK(region_difference(characteristics, skills, 265, 0, 759, 35, stride) > 0);
    CHECK(region_difference(characteristics, skills, 285, 100, 360, 220, stride) > 0);
    CHECK(region_difference(characteristics, skills, 285, 120, 500, 460, stride) > 0);
}

TEST_CASE("character overlay draws bottom inventory icons" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    SoftwareRenderer software(1024, 768);
    REQUIRE(software.renderer != nullptr);
    auto title = load_font(software.renderer, vfs.value(), 18.0f);
    auto rows = load_font(software.renderer, vfs.value(), 16.0f);
    REQUIRE(title.has_value());
    REQUIRE(rows.has_value());

    gl::assets::HeroInventory inventory;
    gl::assets::InventoryItem item;
    item.item_id = 953;
    item.type = 19;
    item.count = 1;
    inventory.items.push_back(item);

    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), &inventory,
                         &catalog.value()));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    const auto with_items = snapshot(software.renderer, software.surface);
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), nullptr, nullptr));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    const auto without_items = snapshot(software.renderer, software.surface);
    CHECK(region_difference(with_items, without_items, 140, 703, 670, 55,
                            software.surface->pitch) > 0);
}

TEST_CASE("character overlay keeps the baked doll from the background" *
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
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), nullptr, nullptr));
    overlay.set_fonts(&title.value(), &rows.value());
    SDL_FillRect(software.surface, nullptr, 0);
    overlay.render(software.renderer, 1024, 768);
    (void)snapshot(software.renderer, software.surface);

    auto background = vfs.value().read("engineres/newinv/inventory_secondary.bmp");
    REQUIRE(background.has_value());
    auto image = gl::assets::decode_image(background.value());
    REQUIRE(image.has_value());

    SDL_Surface *surface = software.surface;
    REQUIRE(SDL_LockSurface(surface) == 0);
    std::size_t different = 0;
    for (int py = 296; py < 616; ++py) {
        const auto *row = reinterpret_cast<const std::uint32_t *>(
            static_cast<const std::uint8_t *>(surface->pixels) + py * surface->pitch);
        for (int px = 8; px < 278; ++px) {
            std::uint8_t r = 0, g = 0, b = 0, a = 0;
            SDL_GetRGBA(row[px], surface->format, &r, &g, &b, &a);
            const auto &pixel = image->rgba[static_cast<std::size_t>(py) * image->width +
                                           static_cast<std::size_t>(px)];
            if (r != pixel.r || g != pixel.g || b != pixel.b) ++different;
        }
    }
    SDL_UnlockSurface(surface);
    CHECK(different <= 8);
}

struct DiffBox {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

std::optional<DiffBox> diff_bbox(SDL_Surface *surface, const gl::assets::Image &image,
                                 int x0, int y0, int x1, int y1) {
    std::optional<DiffBox> box;
    for (int py = y0; py < y1; ++py) {
        const auto *row = reinterpret_cast<const std::uint32_t *>(
            static_cast<const std::uint8_t *>(surface->pixels) + py * surface->pitch);
        for (int px = x0; px < x1; ++px) {
            std::uint8_t r = 0, g = 0, b = 0, a = 0;
            SDL_GetRGBA(row[px], surface->format, &r, &g, &b, &a);
            const auto &pixel = image.rgba[static_cast<std::size_t>(py) * image.width +
                                           static_cast<std::size_t>(px)];
            if (r == pixel.r && g == pixel.g && b == pixel.b) continue;
            if (!box) box = DiffBox{px, py, px, py};
            box->x0 = std::min(box->x0, px);
            box->y0 = std::min(box->y0, py);
            box->x1 = std::max(box->x1, px);
            box->y1 = std::max(box->y1, py);
        }
    }
    return box;
}

void render_and_lock(SDL_Renderer *renderer, SDL_Surface *surface) {
    (void)snapshot(renderer, surface);
    REQUIRE(SDL_LockSurface(surface) == 0);
}

int box_center(const DiffBox &box) { return (box.x0 + box.x1) / 2; }
int box_center_y(const DiffBox &box) { return (box.y0 + box.y1) / 2; }

TEST_CASE("character overlay centers text in the original boxes" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    SoftwareRenderer software(1024, 768);
    REQUIRE(software.renderer != nullptr);
    auto title = load_font(software.renderer, vfs.value(), 17.0f);
    auto rows = load_font(software.renderer, vfs.value(), 16.0f);
    REQUIRE(title.has_value());
    REQUIRE(rows.has_value());

    auto secondary_data = vfs.value().read("engineres/newinv/inventory_secondary.bmp");
    REQUIRE(secondary_data.has_value());
    auto secondary = gl::assets::decode_image(secondary_data.value());
    REQUIRE(secondary.has_value());

    auto sheet = make_sheet();
    sheet.name = "NAME";
    sheet.characteristics_title = "TAB1";
    sheet.skills_title = "TAB2";
    sheet.state_title = "STATE";
    sheet.combat_title = "COMBAT";
    sheet.resistance_title = "RESIST";
    sheet.immunity_title = "IMMUNE";
    sheet.skill_points_label = "POINTS";

    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(software.renderer, vfs.value(), sheet, nullptr, nullptr));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    render_and_lock(software.renderer, software.surface);

    auto name = diff_bbox(software.surface, secondary.value(), 0, 4, 265, 32);
    auto tab1 = diff_bbox(software.surface, secondary.value(), 265, 4, 615, 32);
    auto tab2 = diff_bbox(software.surface, secondary.value(), 615, 4, 1020, 32);
    auto state_hdr = diff_bbox(software.surface, secondary.value(), 352, 52, 580, 80);
    auto combat_hdr = diff_bbox(software.surface, secondary.value(), 352, 302, 580, 330);
    auto resist_hdr = diff_bbox(software.surface, secondary.value(), 709, 52, 937, 80);
    auto immune_hdr = diff_bbox(software.surface, secondary.value(), 713, 522, 941, 550);
    auto primary_label = diff_bbox(software.surface, secondary.value(), 22, 55, 152, 85);
    auto primary_value = diff_bbox(software.surface, secondary.value(), 196, 58, 228, 80);
    auto exp_value = diff_bbox(software.surface, secondary.value(), 214, 258, 256, 282);
    auto state_value = diff_bbox(software.surface, secondary.value(), 585, 98, 631, 124);
    auto combat_value = diff_bbox(software.surface, secondary.value(), 578, 342, 628, 368);
    auto resistance_value = diff_bbox(software.surface, secondary.value(), 946, 98, 999, 124);
    auto immunity1_value = diff_bbox(software.surface, secondary.value(), 772, 566, 818, 592);
    auto immunity2_value = diff_bbox(software.surface, secondary.value(), 946, 566, 996, 592);
    auto weight_value = diff_bbox(software.surface, secondary.value(), 497, 595, 627, 618);
    auto underline = diff_bbox(software.surface, secondary.value(), 270, 32, 430, 35);
    SDL_UnlockSurface(software.surface);

    REQUIRE(name.has_value());
    CHECK(std::abs(box_center(*name) - 140) <= 6);
    REQUIRE(tab1.has_value());
    CHECK(std::abs(box_center(*tab1) - 464) <= 6);
    REQUIRE(tab2.has_value());
    CHECK(std::abs(box_center(*tab2) - 833) <= 6);
    REQUIRE(state_hdr.has_value());
    CHECK(std::abs(box_center(*state_hdr) - 466) <= 6);
    REQUIRE(combat_hdr.has_value());
    CHECK(std::abs(box_center(*combat_hdr) - 466) <= 6);
    REQUIRE(resist_hdr.has_value());
    CHECK(std::abs(box_center(*resist_hdr) - 823) <= 6);
    REQUIRE(immune_hdr.has_value());
    CHECK(std::abs(box_center(*immune_hdr) - 827) <= 6);
    REQUIRE(primary_label.has_value());
    CHECK(primary_label->x0 >= 26);
    CHECK(primary_label->x0 <= 34);
    CHECK(primary_label->y0 >= 64);
    CHECK(primary_label->y0 <= 68);
    REQUIRE(primary_value.has_value());
    CHECK(std::abs(box_center(*primary_value) - 212) <= 6);
    CHECK(std::abs(box_center_y(*primary_value) - 72) <= 2);
    REQUIRE(exp_value.has_value());
    CHECK(std::abs(box_center(*exp_value) - 235) <= 8);
    REQUIRE(state_value.has_value());
    CHECK(std::abs(box_center(*state_value) - 608) <= 8);
    CHECK(std::abs(box_center_y(*state_value) - 115) <= 2);
    REQUIRE(combat_value.has_value());
    CHECK(std::abs(box_center(*combat_value) - 605) <= 8);
    REQUIRE(resistance_value.has_value());
    CHECK(std::abs(box_center(*resistance_value) - 974) <= 8);
    REQUIRE(immunity1_value.has_value());
    CHECK(std::abs(box_center(*immunity1_value) - 796) <= 8);
    REQUIRE(immunity2_value.has_value());
    CHECK(std::abs(box_center(*immunity2_value) - 972) <= 8);
    REQUIRE(weight_value.has_value());
    CHECK(std::abs(box_center(*weight_value) - 562) <= 8);
    CHECK_FALSE(underline.has_value());

    auto skills_data = vfs.value().read("engineres/newinv/inventory_skills.bmp");
    REQUIRE(skills_data.has_value());
    auto skills = gl::assets::decode_image(skills_data.value());
    REQUIRE(skills.has_value());
    overlay.set_tab(gl::game::CharacterTab::skills);
    overlay.render(software.renderer, 1024, 768);
    render_and_lock(software.renderer, software.surface);
    auto group_hdr = diff_bbox(software.surface, skills.value(), 529, 70, 781, 106);
    auto skill_label = diff_bbox(software.surface, skills.value(), 289, 120, 435, 150);
    auto skill_value = diff_bbox(software.surface, skills.value(), 454, 118, 500, 150);
    auto points_label = diff_bbox(software.surface, skills.value(), 515, 622, 720, 650);
    auto points_value = diff_bbox(software.surface, skills.value(), 735, 620, 790, 652);
    auto skill_value_col2 = diff_bbox(software.surface, skills.value(), 700, 118, 745, 150);
    auto skill_value_col3 = diff_bbox(software.surface, skills.value(), 947, 118, 990, 150);
    auto skill_value_group2 = diff_bbox(software.surface, skills.value(), 454, 305, 500, 335);
    auto skill_value_group3 = diff_bbox(software.surface, skills.value(), 454, 490, 500, 520);
    SDL_UnlockSurface(software.surface);

    REQUIRE(group_hdr.has_value());
    CHECK(std::abs(box_center(*group_hdr) - 650) <= 8);
    REQUIRE(skill_label.has_value());
    CHECK(skill_label->x0 >= 291);
    CHECK(skill_label->x0 <= 299);
    CHECK(skill_label->y0 >= 127);
    CHECK(skill_label->y0 <= 131);
    REQUIRE(skill_value.has_value());
    CHECK(std::abs(box_center(*skill_value) - 476) <= 8);
    REQUIRE(points_label.has_value());
    CHECK(points_label->x0 >= 522);
    CHECK(points_label->x0 <= 530);
    REQUIRE(points_value.has_value());
    CHECK(std::abs(box_center(*points_value) - 762) <= 10);
    REQUIRE(skill_value_col2.has_value());
    CHECK(std::abs(box_center(*skill_value_col2) - 722) <= 8);
    REQUIRE(skill_value_col3.has_value());
    CHECK(std::abs(box_center(*skill_value_col3) - 968) <= 8);
    REQUIRE(skill_value_group2.has_value());
    CHECK(std::abs(box_center(*skill_value_group2) - 476) <= 8);
    REQUIRE(skill_value_group3.has_value());
    CHECK(std::abs(box_center(*skill_value_group3) - 476) <= 8);
}

std::optional<DiffBox> snapshot_diff_bbox(const std::vector<std::uint8_t> &a,
                                          const std::vector<std::uint8_t> &b, int x0,
                                          int y0, int x1, int y1, int stride) {
    std::optional<DiffBox> box;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            const std::size_t offset = static_cast<std::size_t>(py) * stride + px * 4;
            if (a[offset] == b[offset] && a[offset + 1] == b[offset + 1] &&
                a[offset + 2] == b[offset + 2])
                continue;
            if (!box) box = DiffBox{px, py, px, py};
            box->x0 = std::min(box->x0, px);
            box->y0 = std::min(box->y0, py);
            box->x1 = std::max(box->x1, px);
            box->y1 = std::max(box->y1, py);
        }
    }
    return box;
}

gl::assets::HeroInventory equipped_inventory() {
    gl::assets::HeroInventory inventory;
    inventory.equipped.push_back({318, 1, 0x100000, 12, 0});
    inventory.equipped.push_back({280, 1, 0x4000, 11, 0});
    inventory.equipped.push_back({397, 1, 0x200000, 14, 0});
    return inventory;
}

TEST_CASE("character overlay draws paper-doll equipment overlays" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    SoftwareRenderer software(1024, 768);
    REQUIRE(software.renderer != nullptr);
    auto title = load_font(software.renderer, vfs.value(), 17.0f);
    auto rows = load_font(software.renderer, vfs.value(), 16.0f);
    REQUIRE(title.has_value());
    REQUIRE(rows.has_value());

    gl::assets::HeroInventory bare_inventory;
    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), &bare_inventory,
                         &catalog.value()));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    const auto bare = snapshot(software.renderer, software.surface);

    gl::assets::HeroInventory inventory = equipped_inventory();
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), &inventory,
                         &catalog.value()));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    const auto dressed = snapshot(software.renderer, software.surface);

    const int stride = software.surface->pitch;
    CHECK(region_difference(dressed, bare, 90, 395, 110, 100, stride) > 500);
    CHECK(region_difference(dressed, bare, 125, 355, 50, 45, stride) > 50);
}

TEST_CASE("character overlay clips accessory overlays to the left half" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    SoftwareRenderer software(1024, 768);
    REQUIRE(software.renderer != nullptr);
    auto title = load_font(software.renderer, vfs.value(), 17.0f);
    auto rows = load_font(software.renderer, vfs.value(), 16.0f);
    REQUIRE(title.has_value());
    REQUIRE(rows.has_value());

    gl::assets::HeroInventory bare_inventory;
    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), &bare_inventory,
                         &catalog.value()));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    const auto bare = snapshot(software.renderer, software.surface);

    gl::assets::HeroInventory inventory;
    inventory.equipped.push_back({233, 1, 0x10000, 17, 0});
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), &inventory,
                         &catalog.value()));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    const auto ring = snapshot(software.renderer, software.surface);

    const auto box = snapshot_diff_bbox(ring, bare, 8, 480, 200, 505,
                                        software.surface->pitch);
    REQUIRE(box.has_value());
    CHECK(box->x0 >= 5);
    CHECK(box->x1 <= 141);
}

TEST_CASE("character overlay toggles weapon set from the inactive slot" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    gl::assets::HeroInventory inventory;
    inventory.equipped.push_back({1, 1, 0x001000u, 0, 0});
    inventory.equipped.push_back({2, 1, 0x002000u, 0, 0});
    inventory.equipped.push_back({3, 1, 0x400000u, 13, 0});
    inventory.equipped.push_back({4, 1, 0x800000u, 13, 0});

    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(nullptr, vfs.value(), make_sheet(), &inventory, &catalog.value()));

    CHECK(overlay.handle_mouse(18, 468, true));
    CHECK(overlay.take_weapon_set_toggle());
    CHECK_FALSE(overlay.take_weapon_set_toggle());

    CHECK(overlay.handle_mouse(75, 525, true));
    CHECK_FALSE(overlay.take_weapon_set_toggle());

    overlay.set_use_b_set(true);
    CHECK(overlay.handle_mouse(30, 480, true));
    CHECK_FALSE(overlay.take_weapon_set_toggle());
    CHECK(overlay.handle_mouse(75, 525, true));
    CHECK(overlay.take_weapon_set_toggle());
}

TEST_CASE("character overlay toggles from an empty inactive slot") {
    gl::assets::GameVfs vfs;
    gl::assets::HeroInventory inventory;
    inventory.equipped.push_back({1, 1, 0x001000u, 0, 0});

    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(nullptr, vfs, make_sheet(), &inventory, nullptr, true));
    CHECK(overlay.handle_mouse(30, 480, true));
    CHECK(overlay.take_weapon_set_toggle());
    CHECK_FALSE(overlay.take_weapon_set_toggle());
}

TEST_CASE("character overlay skips missing puppet assets" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    SoftwareRenderer software(1024, 768);
    REQUIRE(software.renderer != nullptr);
    auto title = load_font(software.renderer, vfs.value(), 17.0f);
    auto rows = load_font(software.renderer, vfs.value(), 16.0f);
    REQUIRE(title.has_value());
    REQUIRE(rows.has_value());

    gl::assets::HeroInventory bare_inventory;
    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), &bare_inventory,
                         &catalog.value()));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    const auto bare = snapshot(software.renderer, software.surface);

    gl::assets::HeroInventory inventory;
    inventory.equipped.push_back({999999, 1, 0x100000, 12, 0});
    REQUIRE(overlay.open(software.renderer, vfs.value(), make_sheet(), &inventory,
                         &catalog.value()));
    overlay.set_fonts(&title.value(), &rows.value());
    overlay.render(software.renderer, 1024, 768);
    const auto missing = snapshot(software.renderer, software.surface);

    CHECK(region_difference(missing, bare, 90, 395, 110, 100,
                            software.surface->pitch) <= 8);
}