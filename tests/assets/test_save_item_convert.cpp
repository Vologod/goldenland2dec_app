#include "gl/assets/save_item_convert.hpp"

#include <array>
#include <cstdint>
#include <doctest/doctest.h>

namespace {
gl::assets::SaveItemRecord make_record() {
    gl::assets::SaveItemRecord record;
    record.has_raw = true;
    for (std::size_t index = 0; index < record.raw.size(); ++index)
        record.raw[index] = static_cast<std::uint8_t>(index & 0xFF);
    record.item_id = 812;
    record.count = 5;
    record.durability = 42;
    record.type = 18;
    record.flags = 0xC0000000u;
    record.style = 7;
    record.price = 85;
    record.weight_x10 = 1;
    record.unknown_a = 3;
    record.material = 0xFFFFFFFFu;
    record.mod_count = 1;
    record.modifiers.push_back({{{3, 1, 80, 0x00001E00u, 0xFFFFFFFFu}}});
    return record;
}
} // namespace

TEST_CASE("inventory_item_from_save preserves raw and modifier words") {
    const auto record = make_record();
    const auto item = gl::assets::inventory_item_from_save(record);
    CHECK(item.item_id == 812);
    CHECK(item.count == 5);
    CHECK(item.durability == 42);
    CHECK(item.flags == 0xC0000000u);
    CHECK(item.type == 18);
    CHECK(item.style == 7);
    CHECK(item.price == 85);
    CHECK(item.weight_x10 == 1);
    CHECK(item.unknown_a == 3);
    CHECK(item.material == 0xFFFFFFFFu);
    CHECK(item.mod_count == 1);
    REQUIRE(item.mods.size() == 1);
    CHECK(item.mods[0].id == 3);
    CHECK(item.mods[0].value == 1);
    CHECK(item.mods[0].flags == 80);
    CHECK(item.mods[0].word3 == 0x00001E00u);
    CHECK(item.mods[0].word4 == 0xFFFFFFFFu);
    REQUIRE(item.save_record.has_value());
    CHECK(item.save_record->has_raw);
    CHECK(item.save_record->raw == record.raw);
}

TEST_CASE("save_item_from_inventory patches typed fields and keeps raw bytes") {
    const auto record = make_record();
    auto item = gl::assets::inventory_item_from_save(record);
    item.count = 9;
    item.durability = 17;
    item.flags = 0x80000000u;
    item.price = 90;
    item.weight_x10 = 2;
    item.unknown_a = 4;
    item.material = 7;
    item.mods[0].value = -2;
    item.mods[0].word3 = 0x00001E01u;

    const auto saved = gl::assets::save_item_from_inventory(item);
    CHECK(saved.has_raw);
    CHECK(saved.raw == record.raw);
    CHECK(saved.item_id == 812);
    CHECK(saved.count == 9);
    CHECK(saved.durability == 17);
    CHECK(saved.flags == 0x80000000u);
    CHECK(saved.price == 90);
    CHECK(saved.weight_x10 == 2);
    CHECK(saved.unknown_a == 4);
    CHECK(saved.material == 7);
    CHECK(saved.mod_count == 1);
    REQUIRE(saved.modifiers.size() == 1);
    CHECK((saved.modifiers[0].words ==
           std::array<std::uint32_t, 5>{3, 0xFFFFFFFEu, 80, 0x00001E01u, 0xFFFFFFFFu}));
}

TEST_CASE("save_item_from_inventory builds a generated record") {
    gl::assets::InventoryItem item;
    item.item_id = 933;
    item.count = 2;
    item.durability = 100;
    item.type = 19;
    item.flags = 0xC0000284u;
    item.style = 9;
    item.price = 85;
    item.weight_x10 = 1;
    item.unknown_a = 3;
    item.material = 0xFFFFFFFFu;
    gl::assets::SpecialMod mod;
    mod.id = 7;
    mod.value = 1;
    mod.flags = 80;
    mod.word3 = 0x00001E01u;
    mod.word4 = 0xFFFFFFFFu;
    item.mods.push_back(mod);

    const auto saved = gl::assets::save_item_from_inventory(item);
    CHECK_FALSE(saved.has_raw);
    CHECK(saved.item_id == 933);
    CHECK(saved.count == 2);
    CHECK(saved.price == 85);
    CHECK(saved.weight_x10 == 1);
    CHECK(saved.unknown_a == 3);
    CHECK(saved.material == 0xFFFFFFFFu);
    CHECK(saved.mod_count == 1);
    REQUIRE(saved.modifiers.size() == 1);
    CHECK((saved.modifiers[0].words ==
           std::array<std::uint32_t, 5>{7, 1, 80, 0x00001E01u, 0xFFFFFFFFu}));
}
