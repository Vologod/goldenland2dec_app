#include "gl/assets/cp1251.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/game/save_person.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <string_view>
#include <vector>

namespace {
void put_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}

void put_string(std::vector<std::uint8_t> &out, std::string_view text) {
    const std::string encoded = gl::assets::utf8_to_cp1251(text);
    put_u32(out, static_cast<std::uint32_t>(encoded.size()));
    out.insert(out.end(), encoded.begin(), encoded.end());
}

struct PersonValues {
    std::string name = "L1_1.P387_Poison_Plotnik";
    std::uint32_t field_40 = 77;
    std::uint32_t field_44 = 2;
    std::uint32_t current_x = 133;
    std::uint32_t current_y = 12;
    std::uint32_t anchor_x = 129;
    std::uint32_t anchor_y = 9;
    std::uint32_t field_654 = 0;
    std::array<std::uint8_t, 0x20> field_658{};
    std::uint32_t direction = 5;
    std::uint32_t route_type = 2;
    std::string route = "track4";
    std::uint32_t radius = 13;
    std::uint32_t delay_min = 1000;
    std::uint32_t delay_max = 2000;
    std::uint32_t tribe = 7;
    std::string dialog = "l1_1/p387.poison.plotnik.d1.age";
    std::string inventory = "l1_1/p387.poison.plotnik";
    std::uint32_t field_6cc = 1;
    std::array<std::uint8_t, 0x11c> field_ff0{};
    std::uint32_t skipped = 0xDEADBEEF;
    std::uint32_t hp = 123;
    std::uint32_t energy = 45;
    std::uint32_t ap = 60;
    std::int32_t route_index = -1;
};

std::vector<std::uint8_t> person_prefix(const PersonValues &values) {
    std::vector<std::uint8_t> out;
    put_string(out, values.name);
    put_u32(out, values.field_40);
    put_u32(out, values.field_44);
    put_u32(out, values.current_x);
    put_u32(out, values.current_y);
    put_u32(out, values.anchor_x);
    put_u32(out, values.anchor_y);
    put_u32(out, values.field_654);
    out.insert(out.end(), values.field_658.begin(), values.field_658.end());
    put_u32(out, values.direction);
    put_u32(out, values.route_type);
    put_string(out, values.route);
    put_u32(out, values.radius);
    put_u32(out, values.delay_min);
    put_u32(out, values.delay_max);
    put_u32(out, values.tribe);
    put_string(out, values.dialog);
    put_string(out, values.inventory);
    put_u32(out, values.field_6cc);
    out.insert(out.end(), values.field_ff0.begin(), values.field_ff0.end());
    put_u32(out, values.skipped);
    put_u32(out, values.hp);
    put_u32(out, values.energy);
    put_u32(out, values.ap);
    put_u32(out, std::bit_cast<std::uint32_t>(values.route_index));
    return out;
}

void append_empty_containers_and_tail(std::vector<std::uint8_t> &out) {
    for (int index = 0; index < 3; ++index)
        put_u32(out, 0);
    out.insert(out.end(), 0x80, 0);
    put_u32(out, 0);
}

std::vector<std::uint8_t> person_body(const PersonValues &values) {
    std::vector<std::uint8_t> out = person_prefix(values);
    append_empty_containers_and_tail(out);
    return out;
}

gl::assets::SaveEntity person_entity(std::vector<std::uint8_t> body) {
    gl::assets::SaveEntity entity;
    entity.type = 2;
    entity.body = std::move(body);
    return entity;
}
} // namespace

TEST_CASE("decode save person maps type-2 fields") {
    PersonValues values;
    values.field_658.fill(0xAB);
    values.field_ff0.fill(0xCD);

    const auto record = gl::game::decode_save_person(person_entity(person_body(values)));
    REQUIRE(record.has_value());
    CHECK(record->name == values.name);
    CHECK(record->current_tile_x == 133);
    CHECK(record->current_tile_y == 12);
    CHECK(record->anchor_tile_x == 129);
    CHECK(record->anchor_tile_y == 9);
    CHECK(record->direction == 5);
    CHECK(record->route_type == 2);
    CHECK(record->route_name == "track4");
    CHECK(record->radius == 13);
    CHECK(record->delay_min == 1000);
    CHECK(record->delay_max == 2000);
    CHECK(record->tribe_id == 7);
    CHECK(record->dialog == values.dialog);
    CHECK(record->inventory == values.inventory);
    CHECK(record->hp == 123);
    CHECK(record->energy == 45);
    CHECK(record->route_index == -1);
}

TEST_CASE("decode save person rejects a non-person entity") {
    const auto body = person_body(PersonValues{});
    gl::assets::SaveEntity entity;
    entity.type = 8;
    entity.body = body;
    const auto record = gl::game::decode_save_person(entity);
    REQUIRE_FALSE(record.has_value());
    CHECK(record.error().code == gl::ErrorCode::format);
}

TEST_CASE("decode save person rejects a truncated body") {
    SUBCASE("missing name length") {
        const auto record = gl::game::decode_save_person(person_entity({0x01, 0x02}));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
    SUBCASE("name overruns the body") {
        std::vector<std::uint8_t> body = {0x10, 0x00, 0x00, 0x00, 'a'};
        const auto record = gl::game::decode_save_person(person_entity(std::move(body)));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
    SUBCASE("prefix ends inside route index") {
        auto prefix = person_prefix(PersonValues{});
        prefix.resize(prefix.size() - 2);
        const auto record = gl::game::decode_save_person(person_entity(std::move(prefix)));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
}
