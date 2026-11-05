#include "gl/assets/item_catalog.hpp"
#include "gl/assets/vfs.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
} // namespace

TEST_CASE("item catalog falls back on an empty virtual filesystem") {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("gl_item_catalog_" + std::to_string(stamp));
    std::filesystem::create_directories(dir);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    const ItemCatalogEntry *missing = catalog->find(424242);
    REQUIRE(missing != nullptr);
    CHECK(missing->name == "item 424242");
    CHECK(missing->description.empty());
    CHECK(missing->icon_path == "engineres/interface/bag");
    CHECK(catalog->find(424242) == missing);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("real item catalog resolves equipment names, stats and icons" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    const ItemCatalogEntry *armor = catalog->find(318);
    REQUIRE(armor != nullptr);
    CHECK(armor->name == "Алмазные латы");
    CHECK_FALSE(armor->description.empty());
    CHECK(armor->icon_path == "items/res/t_armour/almaz/00.bmp");
    CHECK(armor->item.price == 22294);
    CHECK(armor->item.weight_x10 == 98);
    CHECK(armor->item.type == ItemType::cuirass);
    CHECK(vfs->exists(armor->icon_path));
    CHECK(catalog->find(318) == armor);

    const ItemCatalogEntry *helmet = catalog->find(280);
    REQUIRE(helmet != nullptr);
    CHECK(helmet->name == "Шлем из черного металла");
    CHECK(helmet->icon_path == "items/res/t_helm/cherny/00.bmp");
    CHECK(vfs->exists(helmet->icon_path));

    const ItemCatalogEntry *leggings = catalog->find(397);
    REQUIRE(leggings != nullptr);
    CHECK(leggings->name == "Простые поножи из голубого металла");
    CHECK(leggings->icon_path == "items/res/t_elbarms/goluboi/01.bmp");
}

TEST_CASE("real item catalog falls back for a missing item" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    const ItemCatalogEntry *missing = catalog->find(0xFFFFFFFEu);
    REQUIRE(missing != nullptr);
    CHECK(missing->name == "item 4294967294");
    CHECK(missing->description.empty());
    CHECK(missing->icon_path == "engineres/interface/bag");
    CHECK(catalog->find(0xFFFFFFFEu) == missing);
}
