#include "gl/assets/bytes.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/hero_stats.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

std::vector<std::uint32_t> sorted_ids(const gl::assets::HeroEquipment &equipment) {
    std::vector<std::uint32_t> ids;
    ids.reserve(equipment.items.size());
    for (const auto &item : equipment.items)
        ids.push_back(item.item_id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

const gl::assets::EquippedItem *find_item(const gl::assets::HeroEquipment &equipment,
                                          std::uint32_t item_id) {
    for (const auto &item : equipment.items)
        if (item.item_id == item_id)
            return &item;
    return nullptr;
}
} // namespace

TEST_CASE("real save equipped layout slot0/slot2" * doctest::skip(!game_dir_available())) {
    const std::pair<int, std::vector<std::uint32_t>> slots[] = {
        {0, {72, 397, 280, 318, 80, 233, 554, 141, 486, 353, 245}},
        {2, {486, 359, 554, 66, 278, 317, 108, 396, 99, 236, 243}},
    };

    for (const auto &[slot, expected] : slots) {
        const gl::assets::Bytes file = read_save(slot);
        REQUIRE(!file.empty());
        auto body = gl::assets::decode_save_body(file);
        REQUIRE(body.has_value());
        auto equipment = gl::assets::parse_hero_equipment(body.value());
        REQUIRE(equipment.has_value());
        REQUIRE(equipment->items.size() == expected.size());
        std::vector<std::uint32_t> sorted_expected = expected;
        std::sort(sorted_expected.begin(), sorted_expected.end());
        CHECK(sorted_ids(equipment.value()) == sorted_expected);
    }
}

TEST_CASE("real save inventory container counts" * doctest::skip(!game_dir_available())) {
    struct Expected {
        int slot;
        std::size_t hot;
        std::size_t items;
        std::size_t equipped;
    };
    const Expected slots[] = {{0, 8, 117, 11}, {2, 9, 140, 11}};

    for (const auto &expected : slots) {
        auto body = gl::assets::decode_save_body(read_save(expected.slot));
        REQUIRE(body.has_value());
        auto inventory = gl::assets::parse_hero_inventory(body.value());
        REQUIRE(inventory.has_value());
        CHECK(inventory->hot.size() == expected.hot);
        CHECK(inventory->items.size() == expected.items);
        CHECK(inventory->equipped.size() == expected.equipped);
    }
}

TEST_CASE("real save inventory hot slots match research" * doctest::skip(!game_dir_available())) {
    const std::pair<int, std::vector<std::uint32_t>> slots[] = {
        {0, {1554, 1554, 1521, 953, 1575, 1579, 953, 953}},
        {2, {953, 953, 1532, 1532, 953, 1575, 953, 953, 953}},
    };

    for (const auto &[slot, expected] : slots) {
        auto body = gl::assets::decode_save_body(read_save(slot));
        REQUIRE(body.has_value());
        auto inventory = gl::assets::parse_hero_inventory(body.value());
        REQUIRE(inventory.has_value());
        REQUIRE(inventory->hot.size() == expected.size());
        std::vector<std::uint32_t> ids;
        ids.reserve(inventory->hot.size());
        for (const auto &item : inventory->hot) {
            ids.push_back(item.item_id);
            CHECK(item.count >= 1);
        }
        CHECK(ids == expected);
    }
}

TEST_CASE("real save equipped items are not stacked" * doctest::skip(!game_dir_available())) {
    for (int slot : {0, 2}) {
        auto body = gl::assets::decode_save_body(read_save(slot));
        REQUIRE(body.has_value());
        auto inventory = gl::assets::parse_hero_inventory(body.value());
        REQUIRE(inventory.has_value());
        for (const auto &item : inventory->equipped)
            CHECK(item.count == 1);
    }
}

TEST_CASE("real save hero_stats match research" * doctest::skip(!game_dir_available())) {
    struct Expected {
        int slot;
        gl::assets::HeroStats stats;
    };
    const Expected slots[] = {
        {0, {30, 25, 18, 12, 5, 7, 9, -40, 81099, 263, 0, 5}},
        {2, {24, 21, 22, 10, 8, 8, 10, -50, 65133, 201, 0, 51}},
    };

    for (const auto &expected : slots) {
        auto body = gl::assets::decode_save_body(read_save(expected.slot));
        REQUIRE(body.has_value());
        auto stats = gl::assets::parse_hero_stats(body.value());
        REQUIRE(stats.has_value());
        CHECK(stats->strength == expected.stats.strength);
        CHECK(stats->constitution == expected.stats.constitution);
        CHECK(stats->dexterity == expected.stats.dexterity);
        CHECK(stats->perception == expected.stats.perception);
        CHECK(stats->intelligence == expected.stats.intelligence);
        CHECK(stats->wisdom == expected.stats.wisdom);
        CHECK(stats->luck == expected.stats.luck);
        CHECK(stats->reputation == expected.stats.reputation);
        CHECK(stats->experience == expected.stats.experience);
        CHECK(stats->life == expected.stats.life);
        CHECK(stats->energy == expected.stats.energy);
        CHECK(stats->extra == expected.stats.extra);
    }
}

TEST_CASE("real save hero skills match research" * doctest::skip(!game_dir_available())) {
    const std::pair<int, std::array<std::int32_t, 27>> slots[] = {
        {0,
         {0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 15, 0, 0, 1, 0, 0, 1, 14, 0, 1, 0, 15, 0, 10}},
        {2,
         {0, 0, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 15, 0, 1, 5, 0, 0, 1, 1, 1, 5, 0, 15, 0, 10}},
    };

    for (const auto &[slot, expected] : slots) {
        auto body = gl::assets::decode_save_body(read_save(slot));
        REQUIRE(body.has_value());
        auto stats = gl::assets::parse_hero_stats(body.value());
        REQUIRE(stats.has_value());
        for (std::size_t id = 0; id < expected.size(); ++id)
            CHECK(stats->skills[id] == expected[id]);
    }
}

TEST_CASE("real save equipped fields match research" * doctest::skip(!game_dir_available())) {
    const std::pair<int, std::uint32_t> armor[] = {{0, 318}, {2, 317}};
    const std::pair<int, std::uint32_t> elbarms[] = {{0, 397}, {2, 396}};
    const std::pair<int, std::uint32_t> helmet[] = {{0, 280}, {2, 278}};

    for (int index = 0; index < 2; ++index) {
        auto body = gl::assets::decode_save_body(read_save(armor[index].first));
        REQUIRE(body.has_value());
        auto equipment = gl::assets::parse_hero_equipment(body.value());
        REQUIRE(equipment.has_value());

        const auto *armor_item = find_item(equipment.value(), armor[index].second);
        REQUIRE(armor_item != nullptr);
        CHECK(armor_item->type == 12);
        CHECK((armor_item->flags & 0xFFF000u) == 0x100000u);

        const auto *elbarms_item = find_item(equipment.value(), elbarms[index].second);
        REQUIRE(elbarms_item != nullptr);
        CHECK(elbarms_item->type == 14);
        CHECK((elbarms_item->flags & 0xFFF000u) == 0x200000u);

        const auto *helmet_item = find_item(equipment.value(), helmet[index].second);
        REQUIRE(helmet_item != nullptr);
        CHECK(helmet_item->type == 11);
        CHECK((helmet_item->flags & 0xFFF000u) == 0x004000u);
    }
}
