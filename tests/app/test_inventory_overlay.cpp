#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/inventory_overlay.hpp>
#include <gl/assets/image.hpp>
#include <gl/assets/item_catalog.hpp>
#include <gl/assets/vfs.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
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

gl::assets::InventoryItem make_item(std::uint32_t item_id, std::uint32_t type,
                                    std::uint32_t count = 1, std::uint32_t flags = 0) {
    gl::assets::InventoryItem item;
    item.item_id = item_id;
    item.type = type;
    item.count = count;
    item.flags = flags;
    return item;
}

gl::assets::HeroInventory make_inventory() {
    gl::assets::HeroInventory inventory;
    for (std::uint32_t i = 0; i < 20; ++i)
        inventory.items.push_back(make_item(1000 + i, 0, i == 0 ? 3u : 1u));
    inventory.hot.push_back(make_item(1554, 22, 1, 0x00001008));
    inventory.hot.push_back(make_item(953, 19, 5, 0x8000200C));
    inventory.equipped.push_back(make_item(72, 15, 1, 0x0004016C));
    inventory.equipped.push_back(make_item(318, 12, 1, 0xC0100204));
    return inventory;
}

std::optional<gl::assets::HeroInventory> load_real_inventory(int slot) {
    const char *dir = std::getenv("GL_GAME_DIR");
    if (dir == nullptr)
        return std::nullopt;
    const std::filesystem::path path =
        std::filesystem::path(dir) / "Saves" / ("slot" + std::to_string(slot) + ".sav");
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    auto body = gl::assets::decode_save_body(bytes);
    if (!body)
        return std::nullopt;
    auto inventory = gl::assets::parse_hero_inventory(body.value());
    if (!inventory)
        return std::nullopt;
    return std::move(inventory.value());
}

std::optional<gl::render::Font> load_game_font(SDL_Renderer *renderer,
                                               const gl::assets::GameVfs &vfs) {
    auto bytes = vfs.read("engineres/fonts/ttf/arial.ttf");
    if (!bytes)
        return std::nullopt;
    auto font = gl::render::Font::load_from_memory(
        renderer, std::span<const std::uint8_t>(bytes->data(), bytes->size()), 24.0f,
        "engineres/fonts/ttf/arial.ttf");
    if (!font)
        return std::nullopt;
    return std::move(font.value());
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

std::size_t count_pure_magenta(const std::vector<std::uint8_t> &pixels, int x, int y, int w,
                               int h) {
    std::size_t count = 0;
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) {
            const std::size_t offset =
                (static_cast<std::size_t>(py) * 1024 + static_cast<std::size_t>(px)) * 4;
            if (pixels[offset] == 255 && pixels[offset + 1] == 0 && pixels[offset + 2] == 255)
                ++count;
        }
    }
    return count;
}

std::size_t count_magenta_pixels(const gl::assets::Image &image) {
    std::size_t count = 0;
    for (const gl::assets::Rgba &pixel : image.rgba) {
        if (pixel.r == 255 && pixel.g == 0 && pixel.b == 255)
            ++count;
    }
    return count;
}

std::size_t count_opaque_magenta_pixels(const gl::assets::Image &image) {
    std::size_t count = 0;
    for (const gl::assets::Rgba &pixel : image.rgba) {
        if (pixel.r == 255 && pixel.g == 0 && pixel.b == 255 && pixel.a != 0)
            ++count;
    }
    return count;
}

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

} // namespace

TEST_CASE("inventory overlay opens visible with an empty selection") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;

    CHECK(overlay.open(vfs, catalog, make_inventory()));
    CHECK(overlay.visible());
    CHECK(overlay.view().filter_mask == 0);
    CHECK(overlay.view().page == 0);
    CHECK_FALSE(overlay.view().selected.has_value());
    CHECK(overlay.visible_count() == 20);
    CHECK(overlay.selected_name().empty());
}

TEST_CASE("inventory overlay selects the first bottom slot item") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    CHECK(overlay.hit_test(167, 730) == gl::app::InventoryRegion::bottom_slot);
    CHECK(overlay.handle_mouse(167, 730, true));
    REQUIRE(overlay.view().selected.has_value());
    CHECK(*overlay.view().selected == 0);
    CHECK(overlay.selected_name() == "item 1000");
}

TEST_CASE("inventory overlay filters reset the item page") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    CHECK(overlay.hit_test(229, 683) == gl::app::InventoryRegion::filter);
    CHECK(overlay.handle_mouse(883, 730, true));
    CHECK(overlay.view().page == 1);
    CHECK(overlay.handle_mouse(229, 683, true));
    CHECK(overlay.view().filter_mask == 1);
    CHECK(overlay.view().page == 0);
    CHECK(overlay.visible_count() == 20);
}

TEST_CASE("inventory overlay clears a selection hidden by a filter") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    CHECK(overlay.handle_mouse(167, 730, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::item);
    CHECK(overlay.view().selected.has_value());

    CHECK(overlay.handle_mouse(299, 683, true));
    CHECK(overlay.view().filter_mask == 2);
    CHECK(overlay.visible_count() == 0);
    CHECK_FALSE(overlay.view().selected.has_value());
    CHECK(overlay.selection() == gl::app::InventorySelection::none);
}

TEST_CASE("inventory overlay clears a selection when an empty slot is clicked") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    CHECK(overlay.handle_mouse(167, 730, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::item);
    CHECK(overlay.handle_mouse(85, 637, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::none);
    CHECK_FALSE(overlay.view().selected.has_value());

    CHECK(overlay.handle_mouse(167, 730, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::item);
    CHECK(overlay.handle_mouse(94, 334, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::none);
    CHECK_FALSE(overlay.view().selected.has_value());
}

TEST_CASE("inventory overlay draws only the active puppet weapon and shield set") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;

    gl::assets::HeroInventory active;
    active.equipped.push_back(make_item(1, 0, 1, 0x001000));
    active.equipped.push_back(make_item(2, 0, 1, 0x002000));

    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, active));
    CHECK(overlay.hit_test(30, 480) == gl::app::InventoryRegion::puppet_slot);
    CHECK(overlay.hit_test(18, 468) == gl::app::InventoryRegion::none);
    CHECK(overlay.handle_mouse(30, 480, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::equipped);
    REQUIRE(overlay.view().selected.has_value());
    CHECK(*overlay.view().selected == 0);

    gl::assets::HeroInventory only_b;
    only_b.equipped.push_back(make_item(2, 0, 1, 0x002000));
    gl::app::InventoryOverlay b_overlay;
    REQUIRE(b_overlay.open(vfs, catalog, only_b));
    CHECK(b_overlay.hit_test(18, 468) == gl::app::InventoryRegion::puppet_slot);
    CHECK(b_overlay.handle_mouse(18, 468, true));
    CHECK(b_overlay.selection() == gl::app::InventorySelection::equipped);
    REQUIRE(b_overlay.view().selected.has_value());
    CHECK(*b_overlay.view().selected == 0);

    SoftwareRenderer active_canvas(1024, 768);
    REQUIRE(active_canvas.renderer != nullptr);
    gl::app::InventoryOverlay active_overlay;
    REQUIRE(active_overlay.open(vfs, catalog, active));
    gl::app::InventoryOverlay hidden_b_overlay;
    gl::assets::HeroInventory hidden_b;
    hidden_b.equipped.push_back(make_item(1, 0, 1, 0x001000));
    REQUIRE(hidden_b_overlay.open(vfs, catalog, hidden_b));
    SDL_SetRenderDrawColor(active_canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(active_canvas.renderer);
    active_overlay.render(active_canvas.renderer, 1024, 768);
    std::vector<std::uint8_t> with_b(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(active_canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 with_b.data(), 1024 * 4) == 0);
    SDL_SetRenderDrawColor(active_canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(active_canvas.renderer);
    hidden_b_overlay.render(active_canvas.renderer, 1024, 768);
    std::vector<std::uint8_t> without_b(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(active_canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 without_b.data(), 1024 * 4) == 0);
    CHECK(region_difference(with_b, without_b, 16, 466, 61, 62) == 0);
}

TEST_CASE("real slot0 puppet overlap selects the active weapon A" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto inventory = load_real_inventory(0);
    REQUIRE(inventory.has_value());

    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs.value(), catalog.value(), inventory.value()));
    CHECK(overlay.hit_test(18, 468) == gl::app::InventoryRegion::none);
    CHECK(overlay.hit_test(30, 480) == gl::app::InventoryRegion::puppet_slot);
    CHECK(overlay.handle_mouse(30, 480, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::equipped);
    CHECK(overlay.selected_name() == "Топор Сумеречного Палача");
}

TEST_CASE("real slot0 shield B is selectable when shield A is absent" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto inventory = load_real_inventory(0);
    REQUIRE(inventory.has_value());

    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs.value(), catalog.value(), inventory.value()));
    CHECK(overlay.hit_test(238, 493) == gl::app::InventoryRegion::puppet_slot);
    CHECK(overlay.handle_mouse(238, 493, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::equipped);
    REQUIRE(overlay.view().selected.has_value());
    CHECK(inventory->equipped[*overlay.view().selected].item_id == 353);
    CHECK(overlay.selected_name() == "Крепкий костяной щит");
}

TEST_CASE("inventory overlay highlights the active filter") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));
    SoftwareRenderer canvas(1024, 768);
    REQUIRE(canvas.renderer != nullptr);

    SDL_SetRenderDrawColor(canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(canvas.renderer);
    overlay.render(canvas.renderer, 1024, 768);
    std::vector<std::uint8_t> all(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32, all.data(),
                                 1024 * 4) == 0);

    CHECK(overlay.handle_mouse(229, 683, true));
    SDL_SetRenderDrawColor(canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(canvas.renderer);
    overlay.render(canvas.renderer, 1024, 768);
    std::vector<std::uint8_t> filtered(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 filtered.data(), 1024 * 4) == 0);

    CHECK(region_difference(all, filtered, 201, 672, 56, 23) > 0);
    CHECK(region_difference(all, filtered, 745, 673, 56, 23) > 0);
}

TEST_CASE("inventory overlay scroll buttons step one item and clamp") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    CHECK(overlay.hit_test(123, 729) == gl::app::InventoryRegion::scroll_left);
    CHECK(overlay.hit_test(883, 730) == gl::app::InventoryRegion::scroll_right);
    CHECK(overlay.handle_mouse(123, 729, true));
    CHECK(overlay.view().page == 0);
    CHECK(overlay.handle_mouse(883, 730, true));
    CHECK(overlay.view().page == 1);
    CHECK(overlay.handle_mouse(883, 730, true));
    CHECK(overlay.view().page == 2);
    CHECK(overlay.handle_mouse(123, 729, true));
    CHECK(overlay.view().page == 1);
}

TEST_CASE("inventory overlay exit button and escape close it") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    CHECK(overlay.hit_test(957, 728) == gl::app::InventoryRegion::exit);
    CHECK(overlay.handle_mouse(957, 728, true));
    CHECK_FALSE(overlay.visible());

    REQUIRE(overlay.open(vfs, catalog, make_inventory()));
    CHECK(overlay.handle_key(SDLK_ESCAPE));
    CHECK_FALSE(overlay.visible());
    CHECK_FALSE(overlay.handle_key(SDLK_ESCAPE));

    REQUIRE(overlay.open(vfs, catalog, make_inventory()));
    CHECK(overlay.handle_key(SDLK_AC_BACK));
    CHECK_FALSE(overlay.visible());
}

TEST_CASE("inventory overlay hit-tests hot and puppet slot masks") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    const int hot_x[] = {14, 43, 72, 101, 130, 159, 188, 217, 246};
    for (const int x : hot_x)
        CHECK(overlay.hit_test(x + 13, 637) == gl::app::InventoryRegion::hot_slot);
    CHECK(overlay.hit_test(42, 637) == gl::app::InventoryRegion::none);

    const int puppet_x[] = {67, 168, 0, 233, 4, 229, 22, 216, 16, 211, 16, 216};
    const int puppet_y[] = {307, 308, 354, 354, 402, 402, 472, 472, 466, 466, 547, 547};
    for (std::size_t i = 0; i < 12; ++i)
        CHECK(overlay.hit_test(puppet_x[i] + 27, puppet_y[i] + 27) ==
              gl::app::InventoryRegion::puppet_slot);

    CHECK(overlay.hit_test(464, 24) == gl::app::InventoryRegion::skills_tab);
    CHECK(overlay.hit_test(59, 727) == gl::app::InventoryRegion::drop);
    CHECK(overlay.hit_test(500, 200) == gl::app::InventoryRegion::none);

    CHECK(overlay.handle_mouse(464, 24, true));
    CHECK(overlay.visible());
    CHECK_FALSE(overlay.view().selected.has_value());
}

TEST_CASE("inventory overlay selects hot and puppet records") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    CHECK(overlay.handle_mouse(27, 637, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::hot);
    CHECK(overlay.selected_name() == "item 1554");
    REQUIRE(overlay.view().selected.has_value());
    CHECK(*overlay.view().selected == 0);

    CHECK(overlay.handle_mouse(31, 429, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::equipped);
    CHECK(overlay.selected_name() == "item 72");
    REQUIRE(overlay.view().selected.has_value());
    CHECK(*overlay.view().selected == 0);

    CHECK(overlay.handle_mouse(167, 730, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::item);
    CHECK(overlay.selected_name() == "item 1000");
}

TEST_CASE("inventory overlay centers native icon sizes") {
    CHECK(gl::app::InventoryOverlay::native_icon_offset(55.0f, 22.0f) ==
          doctest::Approx(16.5f));
    CHECK(gl::app::InventoryOverlay::native_icon_offset(55.0f, 55.0f) ==
          doctest::Approx(0.0f));
    CHECK(gl::app::InventoryOverlay::native_icon_offset(27.0f, 55.0f) ==
          doctest::Approx(0.0f));
    CHECK(gl::app::InventoryOverlay::native_icon_offset(27.0f, 22.0f) ==
          doctest::Approx(2.5f));
}

TEST_CASE("inventory overlay scales and offsets hit-testing for letterboxed screens") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));
    SoftwareRenderer canvas(900, 600);
    REQUIRE(canvas.renderer != nullptr);

    overlay.render(canvas.renderer, 900, 600);
    CHECK(overlay.hit_test(50 + 131, 571) == gl::app::InventoryRegion::bottom_slot);
    CHECK(overlay.hit_test(50 + 748, 569) == gl::app::InventoryRegion::exit);
    CHECK(overlay.hit_test(20, 571) == gl::app::InventoryRegion::none);
    CHECK(overlay.hit_test(957, 728) == gl::app::InventoryRegion::none);

    overlay.render(canvas.renderer, 800, 600);
    CHECK(overlay.hit_test(131, 571) == gl::app::InventoryRegion::bottom_slot);
    CHECK(overlay.hit_test(748, 569) == gl::app::InventoryRegion::exit);
}

TEST_CASE("inventory overlay wheel scrolls three items and clamps") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    CHECK(overlay.handle_wheel(1));
    CHECK(overlay.view().page == 3);
    CHECK(overlay.handle_wheel(-1));
    CHECK(overlay.view().page == 0);
    CHECK(overlay.handle_wheel(-1));
    CHECK(overlay.view().page == 0);

    for (int i = 0; i < 100; ++i)
        CHECK(overlay.handle_wheel(1));
    CHECK(overlay.view().page == 8);
    CHECK(overlay.handle_wheel(1));
    CHECK(overlay.view().page == 8);
}

TEST_CASE("inventory overlay panel scrolls text with wheel, keys and clipping" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    SoftwareRenderer canvas(1024, 768);
    REQUIRE(canvas.renderer != nullptr);
    auto font = load_game_font(canvas.renderer, vfs.value());
    REQUIRE(font.has_value());

    gl::assets::HeroInventory inventory;
    inventory.items.push_back(make_item(1083, 22, 1));
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs.value(), catalog.value(), inventory));
    REQUIRE(overlay.handle_mouse(167, 730, true));
    CHECK(overlay.selected_name() != "item 1083");

    SDL_SetRenderDrawColor(canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(canvas.renderer);
    overlay.render(canvas.renderer, 1024, 768, &font.value());

    const std::size_t maximum = overlay.panel_max_scroll(font.value());
    REQUIRE(maximum > 0);
    CHECK(overlay.panel_scroll() == 0);

    std::vector<std::uint8_t> top(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 top.data(), 1024 * 4) == 0);

    CHECK(overlay.handle_wheel(1, 700, 200));
    CHECK(overlay.panel_scroll() > 0);
    const std::size_t first = overlay.panel_scroll();
    CHECK(overlay.handle_wheel(-1, 700, 200));
    CHECK(overlay.panel_scroll() < first);

    for (int i = 0; i < 100; ++i)
        CHECK(overlay.handle_wheel(1, 700, 200));
    CHECK(overlay.panel_scroll() == maximum);
    CHECK(overlay.handle_wheel(1, 700, 200));
    CHECK(overlay.panel_scroll() == maximum);

    for (int i = 0; i < 100; ++i)
        CHECK(overlay.handle_wheel(-1, 700, 200));
    CHECK(overlay.panel_scroll() == 0);

    CHECK(overlay.handle_key(SDLK_PAGEDOWN));
    CHECK(overlay.panel_scroll() > 0);
    CHECK(overlay.handle_key(SDLK_PAGEUP));
    CHECK(overlay.panel_scroll() == 0);

    CHECK(overlay.handle_key(SDLK_PAGEDOWN));
    SDL_SetRenderDrawColor(canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(canvas.renderer);
    overlay.render(canvas.renderer, 1024, 768, &font.value());
    std::vector<std::uint8_t> scrolled(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 scrolled.data(), 1024 * 4) == 0);
    CHECK(region_difference(top, scrolled, 664, 54, 340, 500) > 100);
    CHECK(region_difference(top, scrolled, 0, 0, 640, 768) == 0);
}

TEST_CASE("inventory overlay panel scrolls before the first render" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    SoftwareRenderer canvas(1024, 768);
    REQUIRE(canvas.renderer != nullptr);
    auto font = load_game_font(canvas.renderer, vfs.value());
    REQUIRE(font.has_value());

    gl::assets::HeroInventory inventory;
    inventory.items.push_back(make_item(1083, 22, 1));
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs.value(), catalog.value(), inventory));
    REQUIRE(overlay.handle_mouse(167, 730, true));
    overlay.set_font(&font.value());
    REQUIRE(overlay.panel_max_scroll(font.value()) > 0);
    CHECK(overlay.panel_scroll() == 0);

    CHECK(overlay.handle_wheel(1, 700, 200));
    CHECK(overlay.panel_scroll() > 0);
    CHECK(overlay.handle_key(SDLK_PAGEDOWN));
}

TEST_CASE("inventory overlay renders a background and icons" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    const gl::assets::ItemCatalogEntry *armor = catalog->find(318);
    REQUIRE(armor != nullptr);
    auto icon_bytes = vfs->read(armor->icon_path);
    REQUIRE(icon_bytes.has_value());
    auto icon_image = gl::assets::decode_image(icon_bytes.value());
    REQUIRE(icon_image.has_value());
    CHECK(count_magenta_pixels(icon_image.value()) > 0);
    gl::app::InventoryOverlay::apply_magenta_key(icon_image.value());
    CHECK(count_opaque_magenta_pixels(icon_image.value()) == 0);

    gl::assets::HeroInventory inventory;
    inventory.items.push_back(make_item(318, 12, 1));
    inventory.items.push_back(make_item(486, 1, 2));
    inventory.items.push_back(make_item(953, 19, 5));
    inventory.items.push_back(make_item(1554, 22, 1));
    inventory.hot.push_back(make_item(1554, 22, 1, 0x00001008));
    inventory.equipped.push_back(make_item(72, 15, 1, 0x0004016C));
    inventory.equipped.push_back(make_item(318, 12, 1, 0xC0100204));

    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs.value(), catalog.value(), inventory));
    REQUIRE(overlay.handle_mouse(167, 730, true));

    SoftwareRenderer canvas(1024, 768);
    REQUIRE(canvas.renderer != nullptr);
    SDL_SetRenderDrawColor(canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(canvas.renderer);
    overlay.render(canvas.renderer, 1024, 768);

    std::vector<std::uint8_t> pixels(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 pixels.data(), 1024 * 4) == 0);

    std::size_t non_black = 0;
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
        if (pixels[i] > 16 || pixels[i + 1] > 16 || pixels[i + 2] > 16)
            ++non_black;
    }
    CHECK(non_black > 100000);
    CHECK(count_pure_magenta(pixels, 140, 703, 55, 55) == 0);
    CHECK(count_pure_magenta(pixels, 14, 624, 27, 27) == 0);
    CHECK(count_pure_magenta(pixels, 4, 402, 55, 55) == 0);

    SoftwareRenderer empty_canvas(1024, 768);
    REQUIRE(empty_canvas.renderer != nullptr);
    gl::app::InventoryOverlay empty_overlay;
    REQUIRE(empty_overlay.open(vfs.value(), catalog.value(), {}));
    SDL_SetRenderDrawColor(empty_canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(empty_canvas.renderer);
    empty_overlay.render(empty_canvas.renderer, 1024, 768);

    std::vector<std::uint8_t> empty_pixels(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(empty_canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 empty_pixels.data(), 1024 * 4) == 0);

    const auto region_difference = [&](int x, int y, int w, int h) {
        std::size_t different = 0;
        for (int py = y; py < y + h; ++py) {
            for (int px = x; px < x + w; ++px) {
                const std::size_t offset =
                    (static_cast<std::size_t>(py) * 1024 + static_cast<std::size_t>(px)) * 4;
                if (pixels[offset] != empty_pixels[offset] ||
                    pixels[offset + 1] != empty_pixels[offset + 1] ||
                    pixels[offset + 2] != empty_pixels[offset + 2])
                    ++different;
            }
        }
        return different;
    };
    CHECK(region_difference(140, 703, 55, 55) > 100);
    CHECK(region_difference(14, 624, 27, 27) > 20);

    gl::assets::Image image;
    image.width = 1024;
    image.height = 768;
    image.rgba.resize(static_cast<std::size_t>(image.width) * image.height);
    std::memcpy(image.rgba.data(), pixels.data(), pixels.size());

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path output =
        std::filesystem::temp_directory_path() /
        ("gl_inventory_overlay_" + std::to_string(stamp) + ".png");
    CHECK(gl::assets::write_png(output, image));
    CHECK(std::filesystem::file_size(output) > 10000);
    std::error_code ignored;
    std::filesystem::remove(output, ignored);
}
