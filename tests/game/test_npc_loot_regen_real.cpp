#include "gl/assets/inv_table.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/msvc_random.hpp"
#include "gl/game/npc_loot.hpp"
#include "gl/game/person.hpp"

#include <algorithm>
#include <cstdlib>
#include <doctest/doctest.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::optional<gl::game::ItemTemplate> generic_template(std::uint32_t) {
    return gl::game::ItemTemplate{18, 0, 0, 100000};
}

gl::game::PersonInstance person(std::string name, std::string inventory) {
    gl::game::PersonInstance value;
    value.name = std::move(name);
    value.inventory = std::move(inventory);
    return value;
}
} // namespace

TEST_CASE("real data: regenerate only the requested npc inventories" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    REQUIRE(names.has_value());

    std::vector<gl::game::PersonInstance> persons;
    persons.push_back(person("A", "inv012_civil"));
    persons.push_back(person("B", "inv012_civil"));
    persons.push_back(person("C", ""));

    auto single_runtime = gl::game::InventoryRuntime::create(
        gl::assets::HeroInventory{}, names.value(), generic_template);
    single_runtime.set_person_items("B", {gl::assets::InventoryItem{0xFFFFFFF0u, 1, 0, 0, 0}});
    gl::game::MsvcRandom single_rng(1);
    const auto single = gl::game::regenerate_npc_inventories(
        single_runtime, persons, {std::size_t{1}}, vfs.value(), names.value(),
        generic_template, 1, single_rng, nullptr, true);
    CHECK(single.requested == 1);
    CHECK(single.missing_table == 0);
    CHECK(single.populated == 1);
    CHECK(single.gate_failed == 0);
    CHECK(single_runtime.person_items("A").empty());
    const auto single_items = single_runtime.person_items("B");
    CHECK(std::none_of(single_items.begin(), single_items.end(), [](const auto &item) {
        return item.item_id == 0xFFFFFFF0u;
    }));

    auto combined_runtime = gl::game::InventoryRuntime::create(
        gl::assets::HeroInventory{}, names.value(), generic_template);
    gl::game::MsvcRandom combined_rng(1);
    const auto combined = gl::game::regenerate_npc_inventories(
        combined_runtime, persons, {std::size_t{0}, std::size_t{1}}, vfs.value(),
        names.value(), generic_template, 1, combined_rng, nullptr, true);
    CHECK(combined.requested == 2);
    CHECK(combined.populated == 2);
    CHECK(combined.gate_failed == 0);

    auto step_runtime = gl::game::InventoryRuntime::create(
        gl::assets::HeroInventory{}, names.value(), generic_template);
    gl::game::MsvcRandom step_rng(1);
    const auto first = gl::game::regenerate_npc_inventories(
        step_runtime, persons, {std::size_t{0}}, vfs.value(), names.value(),
        generic_template, 1, step_rng, nullptr, true);
    const auto second = gl::game::regenerate_npc_inventories(
        step_runtime, persons, {std::size_t{1}}, vfs.value(), names.value(),
        generic_template, 1, step_rng, nullptr, true);
    CHECK(first.populated == 1);
    CHECK(second.populated == 1);
    CHECK(combined_rng.state() == step_rng.state());
}
