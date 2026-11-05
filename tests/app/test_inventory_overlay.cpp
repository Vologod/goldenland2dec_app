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
}

TEST_CASE("inventory overlay hit-tests hot and puppet slot masks") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    const int hot_x[] = {17, 14, 43, 72, 101, 130, 159, 188, 217, 246};
    for (const int x : hot_x)
        CHECK(overlay.hit_test(x + 13, 637) == gl::app::InventoryRegion::hot_slot);

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
}

TEST_CASE("inventory overlay renders a background and icons" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

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
    CHECK(region_difference(14, 624, 27, 27) > 100);

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
