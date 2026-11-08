#include "gl/assets/inv_table.hpp"
#include "gl/assets/vfs.hpp"

#include <cstdlib>
#include <doctest/doctest.h>
#include <iostream>
#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

bool is_inventory_path(std::string_view path) {
    constexpr std::string_view prefix = "scripts/inventory/";
    constexpr std::string_view suffix = ".inv";
    return path.size() > prefix.size() + suffix.size() && path.starts_with(prefix) &&
           path.ends_with(suffix);
}
} // namespace

TEST_CASE("inv_table real: every .inv loot table parses" * doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    std::size_t files = 0;
    std::size_t failures = 0;
    std::size_t items = 0;
    for (const auto &entry : vfs->list()) {
        if (!is_inventory_path(entry.path))
            continue;
        ++files;
        auto bytes = vfs->read(entry.path);
        if (!bytes) {
            ++failures;
            std::cout << "read failure: " << entry.path << ": " << bytes.error().message << '\n';
            continue;
        }
        auto table = parse_inv(bytes.value());
        if (!table) {
            ++failures;
            std::cout << "parse failure: " << entry.path << ": " << table.error().message << '\n';
            continue;
        }
        items += table->items.size();
    }

    std::cout << "real data .inv: files=" << files << " items=" << items << " failures=" << failures
              << '\n';
    CHECK(files == 238);
    CHECK(failures == 0);
    CHECK(items == 11543);
}

TEST_CASE("inv_table real: representative .inv tables" * doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    {
        auto table = load_inv(vfs.value(), "inv001_swordsman");
        REQUIRE(table.has_value());
        CHECK(table->regenerate_chance == doctest::Approx(0.75));
        CHECK(table->chance_decrease == doctest::Approx(2.0));
        CHECK(table->quantity_decrease == doctest::Approx(1.0));
        CHECK(table->level_offset == 4);
        REQUIRE(table->items.size() == 165);
        const InvItem &first = table->items.front();
        CHECK(first.tech_name == "MON_1_0_1");
        CHECK(first.min_level == -100);
        CHECK(first.max_level == 10);
        CHECK(first.chance_percent == doctest::Approx(100.0));
        CHECK(first.qty_min == 50);
        CHECK(first.qty_max == 100);
    }

    {
        auto table = load_inv(vfs.value(), "hero_items");
        REQUIRE(table.has_value());
        REQUIRE(table->items.size() == 1);
        CHECK(table->items.front().tech_name == "MON_1_0_1");
        CHECK(table->items.front().min_level == 1);
        CHECK(table->items.front().max_level == 100);
        CHECK(table->items.front().qty_min == 1000);
        CHECK(table->items.front().qty_max == 1000);
    }

    {
        auto table = load_inv(vfs.value(), "boxes/swords");
        REQUIRE(table.has_value());
        REQUIRE(table->items.size() == 1);
        CHECK(table->items.front().tech_name == "SWR_1_02_2");
        CHECK(table->items.front().min_level == 1);
        CHECK(table->items.front().max_level == 100);
        CHECK(table->items.front().chance_percent == doctest::Approx(100.0));
        CHECK(table->items.front().qty_min == 5);
        CHECK(table->items.front().qty_max == 7);
    }

    {
        auto table = load_inv(vfs.value(), "boxes/iron");
        REQUIRE(table.has_value());
        CHECK(table->items.empty());
    }

    struct Count {
        const char *name;
        std::size_t items;
    };
    const Count counts[] = {
        {"inv004_maceman_hammer", 41}, {"inv010_alchemist", 278}, {"q35", 297},
        {"unique/rtisch", 207},        {"inv011_caster", 409},    {"party/karno", 409},
        {"party/zemfira", 409},
    };
    for (const Count &expected : counts) {
        CAPTURE(expected.name);
        auto table = load_inv(vfs.value(), expected.name);
        REQUIRE(table.has_value());
        CHECK(table->items.size() == expected.items);
    }
}
