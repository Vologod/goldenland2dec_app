#include "gl/assets/bytes.hpp"
#include "gl/assets/hero_equipment.hpp"

#include <doctest/doctest.h>

#include <algorithm>
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
