#include "gl/assets/save_item.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <doctest/doctest.h>
#include <vector>

namespace {
void put_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}

void set_u32(std::vector<std::uint8_t> &data, std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        data[offset + shift / 8] = static_cast<std::uint8_t>((value >> shift) & 0xFF);
}

std::vector<std::uint8_t> make_record(std::uint32_t subcount) {
    std::vector<std::uint8_t> out(0xC0, 0);
    put_u32(out, subcount);
    set_u32(out, 0x78, subcount);
    for (std::uint32_t index = 0; index < subcount; ++index)
        for (std::uint32_t word = 0; word < 5; ++word)
            put_u32(out, index * 10 + word);
    return out;
}
} // namespace

TEST_CASE("parse_item_record maps the fixed fields") {
    auto data = make_record(0);
    set_u32(data, 0x00, 0x14092248u);
    set_u32(data, 0x08, 18);
    set_u32(data, 0x0C, 0xC0000000u);
    set_u32(data, 0x20, 7);
    set_u32(data, 0xA8, 812);
    set_u32(data, 0xB0, 50);
    set_u32(data, 0xB4, 93);

    const auto parsed = gl::assets::parse_item_record(data);
    REQUIRE(parsed.has_value());
    CHECK(parsed->size == 0xC4);
    CHECK(parsed->record.item_id == 812);
    CHECK(parsed->record.count == 50);
    CHECK(parsed->record.durability == 93);
    CHECK(parsed->record.type == 18);
    CHECK(parsed->record.flags == 0xC0000000u);
    CHECK(parsed->record.style == 7);
    CHECK(parsed->record.mod_count == 0);
    CHECK(parsed->record.modifiers.empty());
}

TEST_CASE("parse_item_record keeps raw modifier words and reports its size") {
    auto data = make_record(2);
    data.push_back(0xAA);

    const auto parsed = gl::assets::parse_item_record(data);
    REQUIRE(parsed.has_value());
    CHECK(parsed->size == 0xC4 + 2 * 0x14);
    CHECK(parsed->record.mod_count == 2);
    REQUIRE(parsed->record.modifiers.size() == 2);
    CHECK((parsed->record.modifiers[0].words == std::array<std::uint32_t, 5>{0, 1, 2, 3, 4}));
    CHECK((parsed->record.modifiers[1].words == std::array<std::uint32_t, 5>{10, 11, 12, 13, 14}));
}

TEST_CASE("parse_item_record keeps the raw fixed record") {
    auto data = make_record(2);
    set_u32(data, 0x00, 0x14092248u);
    set_u32(data, 0x08, 18);
    set_u32(data, 0xA8, 812);
    set_u32(data, 0xB0, 50);
    set_u32(data, 0xB4, 93);

    const auto parsed = gl::assets::parse_item_record(data);
    REQUIRE(parsed.has_value());
    CHECK(parsed->record.has_raw);
    CHECK(parsed->record.raw.size() == 0xC0);
    CHECK(std::equal(parsed->record.raw.begin(), parsed->record.raw.end(), data.begin(),
                     data.begin() + 0xC0));
}

TEST_CASE("encode_item_record round-trips a parsed record") {
    SUBCASE("no modifiers") {
        auto data = make_record(0);
        set_u32(data, 0x00, 0x14092248u);
        set_u32(data, 0x04, 0x12345678u);
        set_u32(data, 0x08, 18);
        set_u32(data, 0x0C, 0xC0000000u);
        set_u32(data, 0x10, 85);
        set_u32(data, 0x14, 3);
        set_u32(data, 0x20, 7);
        set_u32(data, 0x30, 0xFFFFFFFFu);
        set_u32(data, 0xA4, 11);
        set_u32(data, 0xA8, 812);
        set_u32(data, 0xAC, 22);
        set_u32(data, 0xB0, 50);
        set_u32(data, 0xB4, 93);
        set_u32(data, 0xB8, 5);
        set_u32(data, 0xBC, 6);

        const auto parsed = gl::assets::parse_item_record(data);
        REQUIRE(parsed.has_value());
        const auto encoded = gl::assets::encode_item_record(parsed->record);
        REQUIRE(encoded.has_value());
        CHECK(encoded.value() == data);
    }
    SUBCASE("with modifiers") {
        auto data = make_record(2);
        const auto parsed = gl::assets::parse_item_record(data);
        REQUIRE(parsed.has_value());
        const auto encoded = gl::assets::encode_item_record(parsed->record);
        REQUIRE(encoded.has_value());
        CHECK(encoded.value() == data);
    }
}

TEST_CASE("encode_item_record builds a generated record from typed fields") {
    gl::assets::SaveItemRecord record;
    record.has_raw = false;
    record.item_id = 933;
    record.count = 3;
    record.durability = 77;
    record.type = 19;
    record.flags = 0xC0000284u;
    record.style = 9;
    record.price = 85;
    record.weight_x10 = 1;
    record.unknown_a = 3;
    record.material = 0xFFFFFFFFu;
    record.mod_count = 1;
    record.modifiers.push_back({{{7, 1, 80, 0x00001E01u, 0xFFFFFFFFu}}});

    const auto encoded = gl::assets::encode_item_record(record);
    REQUIRE(encoded.has_value());
    CHECK(encoded.value()[0x78] == 1);
    CHECK(encoded.value()[0x79] == 0);
    const auto parsed = gl::assets::parse_item_record(encoded.value());
    REQUIRE(parsed.has_value());
    CHECK(parsed->record.item_id == 933);
    CHECK(parsed->record.count == 3);
    CHECK(parsed->record.durability == 77);
    CHECK(parsed->record.type == 19);
    CHECK(parsed->record.flags == 0xC0000284u);
    CHECK(parsed->record.style == 9);
    CHECK(parsed->record.price == 85);
    CHECK(parsed->record.weight_x10 == 1);
    CHECK(parsed->record.unknown_a == 3);
    CHECK(parsed->record.material == 0xFFFFFFFFu);
    CHECK(parsed->record.mod_count == 1);
    REQUIRE(parsed->record.modifiers.size() == 1);
    CHECK((parsed->record.modifiers[0].words ==
           std::array<std::uint32_t, 5>{7, 1, 80, 0x00001E01u, 0xFFFFFFFFu}));
}

TEST_CASE("encode_item_record rejects an inconsistent modifier count") {
    gl::assets::SaveItemRecord record;
    record.mod_count = 1;
    const auto encoded = gl::assets::encode_item_record(record);
    REQUIRE_FALSE(encoded.has_value());
    CHECK(encoded.error().code == gl::ErrorCode::format);
}

TEST_CASE("parse_item_record rejects truncation") {
    SUBCASE("fixed header") {
        const std::vector<std::uint8_t> data(0xC3, 0);
        const auto parsed = gl::assets::parse_item_record(data);
        REQUIRE_FALSE(parsed.has_value());
        CHECK(parsed.error().code == gl::ErrorCode::format);
    }
    SUBCASE("subrecords") {
        auto data = make_record(1);
        data.pop_back();
        const auto parsed = gl::assets::parse_item_record(data);
        REQUIRE_FALSE(parsed.has_value());
        CHECK(parsed.error().code == gl::ErrorCode::format);
    }
}
