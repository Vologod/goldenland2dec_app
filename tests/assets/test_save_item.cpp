#include "gl/assets/save_item.hpp"

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
