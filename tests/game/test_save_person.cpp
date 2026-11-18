#include "gl/assets/save_item.hpp"
#include "gl/game/save_person.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {
void put_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}

void put_string(std::vector<std::uint8_t> &out, const std::string &text) {
    put_u32(out, static_cast<std::uint32_t>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
}

gl::assets::SaveItemRecord make_item(std::uint32_t id) {
    gl::assets::SaveItemRecord item;
    item.item_id = id;
    item.count = 2;
    item.durability = 50;
    item.type = 18;
    item.flags = 0xC0000000u;
    item.style = 7;
    item.price = 85;
    item.weight_x10 = 1;
    item.unknown_a = 3;
    item.material = 0xFFFFFFFFu;
    item.mod_count = 1;
    item.modifiers.push_back({{{3, 1, 80, 0x00001E00u, 0xFFFFFFFFu}}});
    return item;
}

void append_container(std::vector<std::uint8_t> &out, std::uint32_t flag,
                      const std::vector<gl::assets::SaveItemRecord> &items,
                      const std::vector<std::uint8_t> &raw_items = {}) {
    put_u32(out, flag);
    if (flag == 0)
        return;
    if (!raw_items.empty()) {
        out.insert(out.end(), raw_items.begin(), raw_items.end());
        return;
    }
    put_u32(out, static_cast<std::uint32_t>(items.size()));
    for (const auto &item : items) {
        const auto encoded = gl::assets::encode_item_record(item);
        REQUIRE(encoded.has_value());
        out.insert(out.end(), encoded->begin(), encoded->end());
    }
}

std::vector<std::uint8_t> make_body() {
    std::vector<std::uint8_t> out;
    put_string(out, "L0.P1");
    put_u32(out, 40);
    put_u32(out, 2);
    put_u32(out, 123);
    put_u32(out, 131);
    put_u32(out, 120);
    put_u32(out, 128);
    put_u32(out, 7);
    for (std::uint32_t index = 0; index < 8; ++index)
        put_u32(out, 0x100 + index);
    put_u32(out, 3);
    put_u32(out, 2);
    put_string(out, "route1");
    put_u32(out, 13);
    put_u32(out, 6000);
    put_u32(out, 15000);
    put_u32(out, 16);
    put_string(out, "P1.cs");
    put_string(out, "inv1.inv");
    put_u32(out, 1);
    for (std::uint32_t index = 0; index < 0x11C / 4; ++index)
        put_u32(out, index);
    put_u32(out, 0x1234);
    put_u32(out, 263);
    put_u32(out, 264);
    put_u32(out, 5);
    put_u32(out, 9);
    append_container(out, 1, {make_item(812)});
    append_container(out, 1, {});
    std::vector<std::uint8_t> raw_c2;
    put_u32(raw_c2, 1);
    const auto c2_item = gl::assets::encode_item_record(make_item(918));
    REQUIRE(c2_item.has_value());
    raw_c2.insert(raw_c2.end(), c2_item->begin(), c2_item->end());
    append_container(out, 1, {}, raw_c2);
    for (std::uint32_t index = 0; index < 0x80 / 4; ++index)
        put_u32(out, 0x200 + index);
    put_u32(out, 1);
    for (std::uint32_t index = 0; index < 0x14 / 4; ++index)
        put_u32(out, 0x300 + index);
    return out;
}
} // namespace

TEST_CASE("decode save person maps the full type-2 body") {
    const auto body = make_body();
    const auto record = gl::game::decode_save_person(gl::assets::SaveEntity{2, body});
    REQUIRE(record.has_value());
    CHECK(record->name == "L0.P1");
    CHECK(record->field_40 == 40);
    CHECK(record->field_44 == 2);
    CHECK(record->current_tile_x == 123);
    CHECK(record->current_tile_y == 131);
    CHECK(record->anchor_tile_x == 120);
    CHECK(record->anchor_tile_y == 128);
    CHECK(record->field_654 == 7);
    CHECK(record->field_658[0] == 0x00);
    CHECK(record->field_658[1] == 0x01);
    CHECK(record->direction == 3);
    CHECK(record->route_type == 2);
    CHECK(record->route_name == "route1");
    CHECK(record->radius == 13);
    CHECK(record->delay_min == 6000);
    CHECK(record->delay_max == 15000);
    CHECK(record->tribe_id == 16);
    CHECK(record->dialog == "P1.cs");
    CHECK(record->inventory == "inv1.inv");
    CHECK(record->field_6cc == 1);
    CHECK(record->field_ff0[0] == 0x00);
    CHECK(record->field_ff0[3] == 0x00);
    CHECK(record->field_f50 == 0x1234);
    CHECK(record->hp == 263);
    CHECK(record->energy == 264);
    CHECK(record->ap == 5);
    CHECK(record->route_index == 9);
    REQUIRE(record->containers[0].items.size() == 1);
    CHECK(record->containers[0].flag == 1);
    CHECK(record->containers[0].items[0].item_id == 812);
    CHECK(record->containers[1].flag == 1);
    CHECK(record->containers[1].items.empty());
    CHECK(record->containers[2].flag == 1);
    CHECK(record->containers[2].items.size() == 1);
    CHECK(record->containers[2].items[0].item_id == 918);
    CHECK(record->tail80[0] == 0x00);
    CHECK(record->tail80[1] == 0x02);
    CHECK(record->tail_count == 1);
    REQUIRE(record->extra.size() == 1);
    CHECK(record->extra[0][0] == 0x00);
}

TEST_CASE("encode save person body round-trips byte-identically") {
    const auto body = make_body();
    const auto record = gl::game::decode_save_person(gl::assets::SaveEntity{2, body});
    REQUIRE(record.has_value());
    const auto encoded = gl::game::encode_save_person_body(record.value());
    REQUIRE(encoded.has_value());
    CHECK(encoded.value() == body);
}

TEST_CASE("encode save person body applies live fields") {
    auto record = gl::game::decode_save_person(gl::assets::SaveEntity{2, make_body()});
    REQUIRE(record.has_value());
    record->current_tile_x = 7;
    record->current_tile_y = 9;
    record->direction = 5;
    record->hp = 11;
    record->energy = 12;
    record->ap = 13;
    record->route_index = 4;
    record->containers[0].items.clear();

    const auto encoded = gl::game::encode_save_person_body(record.value());
    REQUIRE(encoded.has_value());
    const auto parsed =
        gl::game::decode_save_person(gl::assets::SaveEntity{2, encoded.value()});
    REQUIRE(parsed.has_value());
    CHECK(parsed->current_tile_x == 7);
    CHECK(parsed->current_tile_y == 9);
    CHECK(parsed->direction == 5);
    CHECK(parsed->hp == 11);
    CHECK(parsed->energy == 12);
    CHECK(parsed->ap == 13);
    CHECK(parsed->route_index == 4);
    CHECK(parsed->containers[0].items.empty());
}

TEST_CASE("decode save person rejects truncation") {
    auto body = make_body();
    for (std::size_t size = 0; size < body.size(); size += 37) {
        const auto record = gl::game::decode_save_person(
            gl::assets::SaveEntity{2, std::vector<std::uint8_t>(body.begin(),
                                                                body.begin() + size)});
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
}
