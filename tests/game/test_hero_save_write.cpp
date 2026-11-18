#include "gl/game/hero_save_write.hpp"

#include "gl/assets/save_item_convert.hpp"

#include <array>
#include <cstdint>
#include <doctest/doctest.h>
#include <span>
#include <vector>

namespace {
void put_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}

gl::assets::SaveItemRecord make_item(std::uint32_t id, std::uint32_t count) {
    gl::assets::SaveItemRecord item;
    item.item_id = id;
    item.count = count;
    item.durability = 50;
    item.type = 18;
    item.flags = 0xC0000000u;
    item.style = 7;
    item.mod_count = 1;
    item.modifiers.push_back({{{3, 1, 80, 0x00001E00u, 0xFFFFFFFFu}}});
    return item;
}

std::vector<std::uint8_t> make_body(const std::vector<gl::assets::SaveItemRecord> &c0,
                                    const std::vector<gl::assets::SaveItemRecord> &c1,
                                    const std::vector<gl::assets::SaveItemRecord> &c2,
                                    const std::array<std::uint32_t, 3> &flags) {
    const std::array<const std::vector<gl::assets::SaveItemRecord> *, 3> c{&c0, &c1, &c2};
    std::vector<std::uint8_t> out;
    put_u32(out, 123);
    put_u32(out, 131);
    put_u32(out, 1);
    for (std::uint32_t index = 0; index < 0x11C / 4; ++index)
        put_u32(out, index);
    put_u32(out, 263);
    put_u32(out, 264);
    put_u32(out, 5);
    for (std::uint32_t index = 0; index < 0x600 / 4; ++index)
        put_u32(out, 0);
    for (std::size_t index = 0; index < 3; ++index) {
        put_u32(out, flags[index]);
        if (flags[index] == 0)
            continue;
        put_u32(out, static_cast<std::uint32_t>(c[index]->size()));
        for (const auto &item : *c[index]) {
            const auto encoded = gl::assets::encode_item_record(item);
            REQUIRE(encoded.has_value());
            out.insert(out.end(), encoded->begin(), encoded->end());
        }
    }
    put_u32(out, 0x2BB4);
    put_u32(out, 0x2BB8);
    for (std::uint32_t index = 0; index < 0x80 / 4; ++index)
        put_u32(out, index);
    put_u32(out, 0);
    put_u32(out, 0);
    return out;
}

gl::game::HeroLiveState live_from(const gl::assets::HeroSaveBody &body) {
    gl::game::HeroLiveState live;
    live.tile_x = body.tile_x;
    live.tile_y = body.tile_y;
    live.direction = body.direction;
    live.hp = body.hp;
    live.ep = body.ep;
    live.ap = body.ap;
    for (std::size_t index = 0; index < 3; ++index) {
        live.present[index] = body.containers[index].flag != 0;
        for (const auto &item : body.containers[index].items)
            live.containers[index].push_back(gl::assets::inventory_item_from_save(item));
    }
    return live;
}
} // namespace

TEST_CASE("make_hero_save_body is byte-identical when nothing changes") {
    const auto original = make_body({make_item(812, 2)}, {make_item(913, 5)}, {}, {1, 1, 0});
    const auto loaded = gl::assets::parse_hero_save_body(original);
    REQUIRE(loaded.has_value());
    const auto live = live_from(loaded.value());
    const auto rebuilt = gl::game::make_hero_save_body(loaded.value(), live);
    const auto encoded = gl::assets::encode_hero_save_body(rebuilt);
    REQUIRE(encoded.has_value());
    CHECK(encoded.value() == original);
}

TEST_CASE("make_hero_save_body patches live hero fields") {
    const auto original = make_body({make_item(812, 2)}, {make_item(913, 5)}, {}, {1, 1, 0});
    const auto loaded = gl::assets::parse_hero_save_body(original);
    REQUIRE(loaded.has_value());
    auto live = live_from(loaded.value());
    live.tile_x = 7;
    live.tile_y = 9;
    live.direction = 3;
    live.hp = 11;
    live.ep = 12;
    live.ap = 13;

    const auto encoded =
        gl::assets::encode_hero_save_body(gl::game::make_hero_save_body(loaded.value(), live));
    REQUIRE(encoded.has_value());
    const auto parsed = gl::assets::parse_hero_save_body(encoded.value());
    REQUIRE(parsed.has_value());
    CHECK(parsed->tile_x == 7);
    CHECK(parsed->tile_y == 9);
    CHECK(parsed->direction == 3);
    CHECK(parsed->hp == 11);
    CHECK(parsed->ep == 12);
    CHECK(parsed->ap == 13);
    CHECK(parsed->containers[0].items.size() == 1);
    CHECK(parsed->containers[1].items.size() == 1);
    CHECK(parsed->containers[2].flag == 0);
}

TEST_CASE("make_hero_save_body rebuilds containers and generated items") {
    const auto original = make_body({make_item(812, 2)}, {make_item(913, 5)}, {}, {1, 1, 0});
    const auto loaded = gl::assets::parse_hero_save_body(original);
    REQUIRE(loaded.has_value());
    auto live = live_from(loaded.value());

    live.containers[0].clear();
    live.containers[1][0].count = 9;
    live.containers[1][0].durability = 17;
    live.containers[1][0].mods[0].value = -2;
    live.containers[1][0].mods[0].word3 = 0x00001E01u;
    gl::assets::InventoryItem generated;
    generated.item_id = 933;
    generated.count = 3;
    generated.durability = 77;
    generated.type = 19;
    generated.flags = 0xC0000284u;
    generated.style = 9;
    gl::assets::SpecialMod mod;
    mod.id = 7;
    mod.value = 1;
    mod.flags = 80;
    mod.word3 = 0x00001E01u;
    mod.word4 = 0xFFFFFFFFu;
    generated.mods.push_back(mod);
    live.containers[2] = {generated};
    live.present[2] = true;

    const auto encoded =
        gl::assets::encode_hero_save_body(gl::game::make_hero_save_body(loaded.value(), live));
    REQUIRE(encoded.has_value());
    const auto parsed = gl::assets::parse_hero_save_body(encoded.value());
    REQUIRE(parsed.has_value());
    CHECK(parsed->containers[0].flag == 1);
    CHECK(parsed->containers[0].items.empty());
    REQUIRE(parsed->containers[1].items.size() == 1);
    CHECK(parsed->containers[1].items[0].count == 9);
    CHECK(parsed->containers[1].items[0].durability == 17);
    REQUIRE(parsed->containers[1].items[0].modifiers.size() == 1);
    CHECK(parsed->containers[1].items[0].modifiers[0].words[1] == 0xFFFFFFFEu);
    CHECK(parsed->containers[1].items[0].modifiers[0].words[3] == 0x00001E01u);
    REQUIRE(parsed->containers[2].items.size() == 1);
    CHECK(parsed->containers[2].items[0].item_id == 933);
    CHECK(parsed->containers[2].items[0].count == 3);
    REQUIRE(parsed->containers[2].items[0].modifiers.size() == 1);
    CHECK((parsed->containers[2].items[0].modifiers[0].words ==
           std::array<std::uint32_t, 5>{7, 1, 80, 0x00001E01u, 0xFFFFFFFFu}));
}

TEST_CASE("make_hero_save_body keeps absent containers absent even with runtime items") {
    const auto original = make_body({}, {}, {}, {0, 0, 0});
    const auto loaded = gl::assets::parse_hero_save_body(original);
    REQUIRE(loaded.has_value());
    auto live = live_from(loaded.value());
    live.containers[0].push_back(gl::assets::inventory_item_from_save(make_item(812, 1)));
    live.present[0] = false;

    const auto encoded =
        gl::assets::encode_hero_save_body(gl::game::make_hero_save_body(loaded.value(), live));
    REQUIRE(encoded.has_value());
    const auto parsed = gl::assets::parse_hero_save_body(encoded.value());
    REQUIRE(parsed.has_value());
    CHECK(parsed->containers[0].flag == 0);
    CHECK(parsed->containers[0].items.empty());
}
