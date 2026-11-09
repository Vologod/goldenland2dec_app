#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/trade_overlay.hpp>
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

std::vector<gl::assets::InventoryItem> make_items(std::size_t count, std::uint32_t first_id,
                                                  std::uint32_t type = 0) {
    std::vector<gl::assets::InventoryItem> items;
    items.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        items.push_back(make_item(first_id + static_cast<std::uint32_t>(i), type));
    return items;
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
                              const std::vector<std::uint8_t> &b, int x, int y, int w, int h) {
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

struct SoftwareRenderer {
    SoftwareRenderer(int width, int height) {
        surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
        if (surface != nullptr)
            renderer = SDL_CreateSoftwareRenderer(surface);
    }

    ~SoftwareRenderer() {
        if (renderer != nullptr)
            SDL_DestroyRenderer(renderer);
        if (surface != nullptr)
            SDL_FreeSurface(surface);
    }

    SoftwareRenderer(const SoftwareRenderer &) = delete;
    SoftwareRenderer &operator=(const SoftwareRenderer &) = delete;

    SDL_Surface *surface = nullptr;
    SDL_Renderer *renderer = nullptr;
};

} // namespace

TEST_CASE("trade layout hit-tests the npc and hero rows") {
    gl::app::TradeLayout layout;

    CHECK(layout.npc_slot_at(347.0f, 26.0f) == std::optional<std::size_t>{0});
    CHECK(layout.npc_slot_at(895.0f + 54.0f, 26.0f + 54.0f) == std::optional<std::size_t>{9});
    CHECK_FALSE(layout.npc_slot_at(346.0f, 26.0f).has_value());
    CHECK_FALSE(layout.npc_slot_at(950.0f, 26.0f).has_value());

    CHECK(layout.hero_slot_at(140.0f, 703.0f) == std::optional<std::size_t>{0});
    CHECK(layout.hero_slot_at(811.0f + 54.0f, 703.0f + 54.0f) == std::optional<std::size_t>{11});
    CHECK_FALSE(layout.hero_slot_at(139.0f, 703.0f).has_value());

    CHECK(layout.hit_test(350.0f, 30.0f) == gl::app::TradeRegion::npc_slot);
    CHECK(layout.hit_test(150.0f, 710.0f) == gl::app::TradeRegion::hero_slot);
    CHECK(layout.hit_test(500.0f, 300.0f) == gl::app::TradeRegion::none);
}

TEST_CASE("trade layout hit-tests filters, scroll, exit and drop") {
    gl::app::TradeLayout layout;

    CHECK(layout.npc_filter_at(350.0f + 26.0f, 94.0f + 10.0f) == std::optional<std::size_t>{0});
    CHECK(layout.npc_filter_at(439.0f + 26.0f, 94.0f + 10.0f) == std::optional<std::size_t>{1});
    CHECK(layout.npc_filter_at(531.0f + 26.0f, 95.0f + 10.0f) == std::optional<std::size_t>{2});
    CHECK(layout.npc_filter_at(893.0f + 26.0f, 95.0f + 10.0f) == std::optional<std::size_t>{6});
    CHECK(layout.npc_filter_at(350.0f + 26.0f, 94.5f) == std::optional<std::size_t>{0});
    CHECK_FALSE(layout.npc_filter_at(531.0f + 26.0f, 94.5f).has_value());
    CHECK(layout.npc_filter_at(531.0f + 26.0f, 95.0f) == std::optional<std::size_t>{2});
    CHECK(layout.hit_test(350.0f + 26.0f, 94.0f + 10.0f) == gl::app::TradeRegion::npc_filter);
    CHECK(layout.hit_test(531.0f + 26.0f, 95.0f + 10.0f) == gl::app::TradeRegion::npc_filter);

    CHECK(layout.hero_filter_at(201.0f + 28.0f, 672.0f + 11.0f) == std::optional<std::size_t>{0});
    CHECK(layout.hero_filter_at(745.0f + 28.0f, 672.0f + 11.0f) == std::optional<std::size_t>{6});
    CHECK(layout.hit_test(201.0f + 28.0f, 672.0f + 11.0f) == gl::app::TradeRegion::hero_filter);

    CHECK(layout.hit_test(325.0f, 55.0f) == gl::app::TradeRegion::npc_scroll_left);
    CHECK(layout.hit_test(967.0f, 53.0f) == gl::app::TradeRegion::npc_scroll_right);
    CHECK(layout.hit_test(123.0f, 729.0f) == gl::app::TradeRegion::hero_scroll_left);
    CHECK(layout.hit_test(883.0f, 730.0f) == gl::app::TradeRegion::hero_scroll_right);

    CHECK(layout.hit_test(957.0f, 728.0f) == gl::app::TradeRegion::exit);
    CHECK(layout.hit_test(59.0f, 727.0f) == gl::app::TradeRegion::drop);
}

TEST_CASE("trade layout reports money and row geometry") {
    gl::app::TradeLayout layout;

    CHECK(gl::app::TradeLayout::kCanvasWidth == 1024.0f);
    CHECK(gl::app::TradeLayout::kCanvasHeight == 768.0f);
    CHECK(gl::app::TradeLayout::kNpcMoneyY == 126);
    CHECK(gl::app::TradeLayout::kHeroMoneyY == 612);
    CHECK(gl::app::TradeLayout::kNpcY == 26);
    CHECK(gl::app::TradeLayout::kHeroY == 703);
    CHECK(gl::app::TradeLayout::kIconSize == 55);
    CHECK(layout.npc_row_size() == 10);
    CHECK(layout.hero_row_size() == 12);
    CHECK(gl::app::TradeLayout::max_scroll(25, 10) == 15);
    CHECK(gl::app::TradeLayout::max_scroll(7, 10) == 0);
}

TEST_CASE("trade overlay opens visible with money and both lists") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::TradeOverlay overlay;

    const auto npc = make_items(16, 400);
    const auto hero = make_items(20, 900);
    REQUIRE(overlay.open(vfs, catalog, "L13_1.P517_Boat_Trader", npc, 42142, hero, 43875));

    CHECK(overlay.visible());
    CHECK(overlay.npc_name() == "L13_1.P517_Boat_Trader");
    CHECK(overlay.npc_items().size() == 16);
    CHECK(overlay.hero_items().size() == 20);
    CHECK(overlay.npc_money() == 42142);
    CHECK(overlay.hero_money() == 43875);
    CHECK(overlay.npc_scroll() == 0);
    CHECK(overlay.hero_scroll() == 0);
    CHECK(overlay.npc_filter() == 0);
    CHECK(overlay.hero_filter() == 0);
    CHECK(overlay.selection() == gl::app::TradeSelection::none);
}

TEST_CASE("trade overlay npc scroll steps, clamps and resets on filter") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::TradeOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, "npc", make_items(25, 400), 100, make_items(20, 900), 50));

    CHECK(overlay.npc_max_scroll() == 15);
    CHECK(overlay.handle_mouse(967, 53, true));
    CHECK(overlay.npc_scroll() == 1);
    CHECK(overlay.handle_wheel(1));
    CHECK(overlay.npc_scroll() == 4);
    CHECK(overlay.handle_wheel(-1));
    CHECK(overlay.npc_scroll() == 1);

    for (int i = 0; i < 100; ++i)
        CHECK(overlay.handle_mouse(967, 53, true));
    CHECK(overlay.npc_scroll() == 15);
    CHECK(overlay.handle_mouse(967, 53, true));
    CHECK(overlay.npc_scroll() == 15);

    CHECK(overlay.handle_mouse(376, 104, true));
    CHECK(overlay.npc_filter() == 1);
    CHECK(overlay.npc_scroll() == 0);

    CHECK(overlay.handle_mouse(325, 55, true));
    CHECK(overlay.npc_scroll() == 0);
}

TEST_CASE("trade overlay hero scroll steps, clamps and filters by proven mask") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::TradeOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, "npc", {}, 0, make_items(20, 900, 0), 0));

    CHECK(overlay.hero_max_scroll() == 8);
    CHECK(overlay.handle_mouse(883, 730, true));
    CHECK(overlay.hero_scroll() == 1);
    CHECK(overlay.handle_wheel(1, 883, 730));
    CHECK(overlay.hero_scroll() == 4);
    CHECK(overlay.handle_mouse(123, 729, true));
    CHECK(overlay.hero_scroll() == 3);

    CHECK(overlay.handle_mouse(229, 683, true));
    CHECK(overlay.hero_filter() == 1);
    CHECK(overlay.hero_scroll() == 0);
    CHECK(overlay.hero_max_scroll() == 8);

    gl::app::TradeOverlay masked;
    REQUIRE(masked.open(vfs, catalog, "npc", {}, 0, make_items(20, 900, 0), 0));
    CHECK(masked.handle_mouse(299, 683, true));
    CHECK(masked.hero_filter() == 2);
    CHECK(masked.hero_max_scroll() == 0);
    CHECK(masked.handle_mouse(167, 730, true));
    CHECK(masked.selection() == gl::app::TradeSelection::none);
}

TEST_CASE("trade overlay wheel scrolls the row under the cursor") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::TradeOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, "npc", make_items(25, 400), 0, make_items(20, 900), 0));

    CHECK(overlay.handle_wheel(1, 167, 730));
    CHECK(overlay.hero_scroll() == 3);
    CHECK(overlay.npc_scroll() == 0);

    CHECK(overlay.handle_wheel(1, 500, 50));
    CHECK(overlay.npc_scroll() == 3);
    CHECK(overlay.hero_scroll() == 3);
}

TEST_CASE("trade overlay selects npc and hero items read-only") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::TradeOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, "npc", make_items(16, 400), 0, make_items(20, 900), 0));

    CHECK(overlay.handle_mouse(360, 40, true));
    CHECK(overlay.selection() == gl::app::TradeSelection::npc);
    CHECK(overlay.selected_npc() == std::optional<std::size_t>{0});

    CHECK(overlay.handle_mouse(150, 710, true));
    CHECK(overlay.selection() == gl::app::TradeSelection::hero);
    CHECK(overlay.selected_hero() == std::optional<std::size_t>{0});

    CHECK_FALSE(overlay.handle_mouse(500, 300, true));
    CHECK(overlay.selection() == gl::app::TradeSelection::hero);
    CHECK(overlay.handle_mouse(347, 26, true));
    CHECK(overlay.selection() == gl::app::TradeSelection::npc);
    CHECK(overlay.selected_npc() == std::optional<std::size_t>{0});
}

TEST_CASE("trade overlay exit and escape close it") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::TradeOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, "npc", {}, 0, {}, 0));

    CHECK(overlay.hit_test(957, 728) == gl::app::TradeRegion::exit);
    CHECK(overlay.handle_mouse(957, 728, true));
    CHECK_FALSE(overlay.visible());

    REQUIRE(overlay.open(vfs, catalog, "npc", {}, 0, {}, 0));
    CHECK(overlay.handle_key(SDLK_ESCAPE));
    CHECK_FALSE(overlay.visible());
    CHECK_FALSE(overlay.handle_key(SDLK_ESCAPE));

    REQUIRE(overlay.open(vfs, catalog, "npc", {}, 0, {}, 0));
    CHECK(overlay.handle_key(SDLK_AC_BACK));
    CHECK_FALSE(overlay.visible());
}

TEST_CASE("trade overlay hit-tests letterboxed screens") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::TradeOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, "npc", make_items(4, 400), 0, make_items(4, 900), 0));
    SoftwareRenderer canvas(900, 600);
    REQUIRE(canvas.renderer != nullptr);

    overlay.render(canvas.renderer, 900, 600);
    CHECK(overlay.hit_test(50 + 321, 55) == gl::app::TradeRegion::npc_slot);
    CHECK(overlay.hit_test(50 + 748, 569) == gl::app::TradeRegion::exit);
    CHECK(overlay.hit_test(20, 571) == gl::app::TradeRegion::none);
    CHECK(overlay.hit_test(957, 728) == gl::app::TradeRegion::none);
}

TEST_CASE("real slot0 trade overlay renders background, icons and two money values" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto inventory = load_real_inventory(0);
    REQUIRE(inventory.has_value());

    std::vector<gl::assets::InventoryItem> npc(
        inventory->items.begin(),
        inventory->items.begin() + std::min<std::size_t>(10, inventory->items.size()));
    std::vector<gl::assets::InventoryItem> hero(
        inventory->items.begin() + static_cast<std::ptrdiff_t>(npc.size()), inventory->items.end());
    REQUIRE_FALSE(hero.empty());

    SoftwareRenderer canvas(1024, 768);
    REQUIRE(canvas.renderer != nullptr);
    auto font = load_game_font(canvas.renderer, vfs.value());
    REQUIRE(font.has_value());

    gl::app::TradeOverlay overlay;
    REQUIRE(overlay.open(vfs.value(), catalog.value(), "L13_1.P517_Boat_Trader", npc, 42142, hero,
                         43875));
    CHECK(overlay.handle_mouse(420, 40, true));
    CHECK_FALSE(overlay.selected_name().empty());

    SDL_SetRenderDrawColor(canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(canvas.renderer);
    overlay.render(canvas.renderer, 1024, 768, &font.value());

    std::vector<std::uint8_t> pixels(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32, pixels.data(),
                                 1024 * 4) == 0);

    std::size_t non_black = 0;
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
        if (pixels[i] > 16 || pixels[i + 1] > 16 || pixels[i + 2] > 16)
            ++non_black;
    }
    CHECK(non_black > 100000);
    CHECK(count_pure_magenta(pixels, 347, 26, 55, 55) == 0);

    SoftwareRenderer empty_canvas(1024, 768);
    REQUIRE(empty_canvas.renderer != nullptr);
    gl::app::TradeOverlay empty_overlay;
    REQUIRE(empty_overlay.open(vfs.value(), catalog.value(), "npc", {}, 0, {}, 0));
    SDL_SetRenderDrawColor(empty_canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(empty_canvas.renderer);
    empty_overlay.render(empty_canvas.renderer, 1024, 768, &font.value());

    std::vector<std::uint8_t> empty_pixels(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(empty_canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 empty_pixels.data(), 1024 * 4) == 0);

    CHECK(region_difference(pixels, empty_pixels, 347, 26, 55, 55) > 100);
    CHECK(region_difference(pixels, empty_pixels, 562, 126, 160, 30) > 0);
    CHECK(region_difference(pixels, empty_pixels, 562, 612, 160, 30) > 0);

    struct ScopedDir {
        std::filesystem::path path;
        ~ScopedDir() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    };
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto background_bytes = vfs->read("engineres/trade/trade.bmp");
    REQUIRE(background_bytes.has_value());
    ScopedDir background_dir{std::filesystem::temp_directory_path() /
                             ("gl_trade_bg_" + std::to_string(stamp))};
    const std::filesystem::path background_path =
        background_dir.path / "engineres" / "trade" / "trade.bmp";
    std::filesystem::create_directories(background_path.parent_path());
    {
        std::ofstream out(background_path, std::ios::binary);
        REQUIRE(out.good());
        out.write(reinterpret_cast<const char *>(background_bytes->data()),
                  static_cast<std::streamsize>(background_bytes->size()));
    }
    auto background_vfs = gl::assets::GameVfs::mount(background_dir.path);
    REQUIRE(background_vfs.has_value());

    SoftwareRenderer background_canvas(1024, 768);
    REQUIRE(background_canvas.renderer != nullptr);
    gl::app::TradeOverlay background_overlay;
    REQUIRE(background_overlay.open(background_vfs.value(), catalog.value(),
                                    "L13_1.P517_Boat_Trader", npc, 42142, hero, 43875));
    SDL_SetRenderDrawColor(background_canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(background_canvas.renderer);
    background_overlay.render(background_canvas.renderer, 1024, 768, &font.value());
    std::vector<std::uint8_t> background_pixels(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(background_canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 background_pixels.data(), 1024 * 4) == 0);

    CHECK(region_difference(pixels, background_pixels, 350, 94, 52, 20) > 0);
    CHECK(region_difference(pixels, background_pixels, 531, 95, 52, 20) > 0);
    CHECK(region_difference(pixels, background_pixels, 201, 672, 56, 23) > 0);
    CHECK(region_difference(pixels, background_pixels, 312, 28, 27, 55) > 0);
    CHECK(region_difference(pixels, background_pixels, 954, 26, 27, 55) > 0);
    CHECK(region_difference(pixels, background_pixels, 110, 702, 27, 55) > 0);
    CHECK(region_difference(pixels, background_pixels, 870, 703, 26, 54) > 0);
    CHECK(region_difference(pixels, background_pixels, 920, 694, 75, 68) > 0);
    CHECK(region_difference(pixels, background_pixels, 19, 690, 81, 75) > 0);

    gl::assets::Image image;
    image.width = 1024;
    image.height = 768;
    image.rgba.resize(static_cast<std::size_t>(image.width) * image.height);
    std::memcpy(image.rgba.data(), pixels.data(), pixels.size());

    const char *screenshot_env = std::getenv("GL_TRADE_SCREENSHOT");
    const bool keep_screenshot = screenshot_env != nullptr;
    const std::filesystem::path output =
        keep_screenshot ? std::filesystem::path(screenshot_env)
                        : std::filesystem::temp_directory_path() /
                              ("gl_trade_overlay_" + std::to_string(stamp) + ".png");
    CHECK(gl::assets::write_png(output, image));
    CHECK(std::filesystem::file_size(output) > 10000);
    if (!keep_screenshot) {
        std::error_code ignored;
        std::filesystem::remove(output, ignored);
    }
}
