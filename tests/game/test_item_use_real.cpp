#include <doctest/doctest.h>

#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/itm.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/item_use.hpp"
#include "gl/game/person_runtime.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::vector<std::uint8_t> read_save(int slot) {
    const std::filesystem::path path =
        std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
        ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

gl::assets::Item read_item(gl::assets::GameVfs &vfs, const std::string &path) {
    auto bytes = vfs.read(path);
    REQUIRE(bytes.has_value());
    auto item = gl::assets::parse_item(std::span<const std::uint8_t>(bytes->data(), bytes->size()));
    REQUIRE(item.has_value());
    return item.value();
}
} // namespace

TEST_CASE("real 953 potion heals HP and consumes one" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const auto item = read_item(vfs.value(), "items/data/953.itm");
    CHECK(item.type == gl::assets::ItemType::potion);
    CHECK(item.healing_health == 100);

    auto body = gl::assets::decode_save_body(read_save(0));
    REQUIRE(body.has_value());
    auto stats = gl::assets::parse_hero_stats(body.value());
    REQUIRE(stats.has_value());
    auto inventory_data = gl::assets::parse_hero_inventory(body.value());
    REQUIRE(inventory_data.has_value());
    std::size_t index = inventory_data->items.size();
    for (std::size_t i = 0; i < inventory_data->items.size(); ++i) {
        if (inventory_data->items[i].item_id == 953) {
            index = i;
            break;
        }
    }
    REQUIRE(index < inventory_data->items.size());
    const std::uint32_t before = inventory_data->items[index].count;

    auto inventory = gl::game::InventoryRuntime::create(inventory_data.value(), {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats.value());
    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::life, 10.0);
    stats->life = 10;

    const auto result = gl::game::use_hero_item(item, gl::game::HeroItemContainer::main, index,
                                                stats.value(), runtime, inventory);
    CHECK(result.used);
    CHECK(result.consumed);
    const double max_life =
        runtime.get_parameter("Hero", gl::script::PersonParameter::maximum_life);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::life) ==
          doctest::Approx(std::min(max_life, 110.0)));
    REQUIRE(inventory.hero_inventory().items.size() == inventory_data->items.size());
    CHECK(inventory.hero_inventory().items[index].item_id == 953);
    CHECK(inventory.hero_inventory().items[index].count == before - 1);
}

TEST_CASE("real 948 potion applies direct strength and intelligence" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const auto item = read_item(vfs.value(), "items/data/948.itm");
    REQUIRE(item.type == gl::assets::ItemType::potion);

    gl::assets::HeroStats stats;
    stats.strength = 10;
    stats.intelligence = 10;
    gl::assets::HeroInventory hero;
    hero.items.push_back(gl::assets::InventoryItem(948, 1, 0, 19, 0));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);

    const auto result = gl::game::use_hero_item(item, gl::game::HeroItemContainer::main, 0, stats,
                                                runtime, inventory);
    CHECK(result.used);
    CHECK(result.effects_added == 0);
    CHECK(stats.strength == 11);
    CHECK(stats.intelligence == 9);
}

TEST_CASE("real 408 food appends a timed effect" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const auto item = read_item(vfs.value(), "items/data/408.itm");
    REQUIRE(item.type == gl::assets::ItemType::food);

    gl::assets::HeroStats stats;
    gl::assets::HeroInventory hero;
    hero.items.push_back(gl::assets::InventoryItem(408, 1, 0, 20, 0));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);

    const auto result = gl::game::use_hero_item(item, gl::game::HeroItemContainer::main, 0, stats,
                                                runtime, inventory);
    CHECK(result.used);
    CHECK(result.effects_added == 1);
    REQUIRE(runtime.effects("Hero").size() == 1);
    CHECK(runtime.effects("Hero")[0].words[0] == 30);
    CHECK(runtime.effects("Hero")[0].words[1] == 100);
    CHECK(static_cast<std::uint32_t>(runtime.effects("Hero")[0].words[3]) == 0x00000003);
}
