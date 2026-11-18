#include "gl/assets/hero_save_body.hpp"

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

std::vector<std::uint8_t> make_body(std::vector<gl::assets::SaveItemRecord> c0,
                                    std::uint32_t c1_flag, std::uint32_t c2_flag,
                                    std::uint32_t extra_count, std::uint32_t attached_count,
                                    std::vector<std::uint8_t> attached_raw = {}) {
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
    put_u32(out, 1);
    put_u32(out, static_cast<std::uint32_t>(c0.size()));
    for (const auto &item : c0) {
        const auto encoded = gl::assets::encode_item_record(item);
        REQUIRE(encoded.has_value());
        out.insert(out.end(), encoded->begin(), encoded->end());
    }
    put_u32(out, c1_flag);
    if (c1_flag != 0)
        put_u32(out, 0);
    put_u32(out, c2_flag);
    if (c2_flag != 0)
        put_u32(out, 0);
    put_u32(out, 0x2BB4);
    put_u32(out, 0x2BB8);
    for (std::uint32_t index = 0; index < 0x80 / 4; ++index)
        put_u32(out, index);
    put_u32(out, extra_count);
    for (std::uint32_t index = 0; index < extra_count * 0x14 / 4; ++index)
        put_u32(out, index);
    put_u32(out, attached_count);
    out.insert(out.end(), attached_raw.begin(), attached_raw.end());
    return out;
}

gl::assets::SaveItemRecord make_item(std::uint32_t id) {
    gl::assets::SaveItemRecord item;
    item.item_id = id;
    item.count = 2;
    item.durability = 50;
    item.type = 18;
    item.flags = 0xC0000000u;
    item.style = 7;
    item.mod_count = 1;
    item.modifiers.push_back({{{3, 1, 80, 0x00001E00u, 0xFFFFFFFFu}}});
    return item;
}
} // namespace

TEST_CASE("hero save body parses the fixed layout") {
    const auto body = make_body({make_item(812)}, 1, 0, 1, 2, {0xAA, 0xBB});
    const auto parsed = gl::assets::parse_hero_save_body(body);
    REQUIRE(parsed.has_value());
    CHECK(parsed->tile_x == 123);
    CHECK(parsed->tile_y == 131);
    CHECK(parsed->direction == 1);
    CHECK(parsed->hp == 263);
    CHECK(parsed->ep == 264);
    CHECK(parsed->ap == 5);
    CHECK(parsed->containers[0].flag == 1);
    REQUIRE(parsed->containers[0].items.size() == 1);
    CHECK(parsed->containers[0].items[0].item_id == 812);
    CHECK(parsed->containers[1].flag == 1);
    CHECK(parsed->containers[1].items.empty());
    CHECK(parsed->containers[2].flag == 0);
    CHECK(parsed->field_2bb4 == 0x2BB4);
    CHECK(parsed->field_2bb8 == 0x2BB8);
    CHECK(parsed->extra.size() == 1);
    CHECK((parsed->extra[0][0] == 0 && parsed->extra[0][0x10] == 4));
    CHECK(parsed->attached_count == 2);
    CHECK((parsed->attached_raw == std::vector<std::uint8_t>{0xAA, 0xBB}));
}

TEST_CASE("hero save body round-trips byte-identically") {
    const auto body = make_body({make_item(812), make_item(913)}, 1, 0, 1, 2, {0xAA, 0xBB});
    const auto parsed = gl::assets::parse_hero_save_body(body);
    REQUIRE(parsed.has_value());
    const auto encoded = gl::assets::encode_hero_save_body(parsed.value());
    REQUIRE(encoded.has_value());
    CHECK(encoded.value() == body);
}

TEST_CASE("hero save body preserves an absent container without a count") {
    const auto body = make_body({}, 0, 0, 0, 0);
    const auto parsed = gl::assets::parse_hero_save_body(body);
    REQUIRE(parsed.has_value());
    CHECK(parsed->containers[0].flag == 1);
    CHECK(parsed->containers[0].items.empty());
    CHECK(parsed->containers[1].flag == 0);
    CHECK(parsed->containers[2].flag == 0);
    const auto encoded = gl::assets::encode_hero_save_body(parsed.value());
    REQUIRE(encoded.has_value());
    CHECK(encoded.value() == body);
}

TEST_CASE("hero save body rejects truncation") {
    auto body = make_body({make_item(812)}, 1, 0, 1, 2, {0xAA, 0xBB});
    for (std::size_t size = 0; size < 0x734; ++size) {
        CAPTURE(size);
        const auto parsed =
            gl::assets::parse_hero_save_body(std::span<const std::uint8_t>(body.data(), size));
        REQUIRE_FALSE(parsed.has_value());
        CHECK(parsed.error().code == gl::ErrorCode::format);
    }
    SUBCASE("container count without items") {
        std::vector<std::uint8_t> truncated(body.begin(), body.begin() + 0x73C);
        const auto parsed = gl::assets::parse_hero_save_body(truncated);
        REQUIRE_FALSE(parsed.has_value());
        CHECK(parsed.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("hero save body rejects truncation inside item, extra and attached count") {
    auto body = make_body({make_item(812)}, 1, 0, 1, 2, {0xAA, 0xBB});
    const auto item_size = gl::assets::encode_item_record(make_item(812));
    REQUIRE(item_size.has_value());
    const std::size_t c0_item = 0x734 + 8;
    const std::size_t c1_flag = c0_item + item_size->size();
    const std::size_t extra_count = c1_flag + 8 + 4 + 8 + 0x80;
    const std::size_t extra_start = extra_count + 4;
    SUBCASE("inside container item") {
        const auto parsed = gl::assets::parse_hero_save_body(
            std::span<const std::uint8_t>(body.data(), c0_item + 10));
        REQUIRE_FALSE(parsed.has_value());
        CHECK(parsed.error().code == gl::ErrorCode::format);
    }
    SUBCASE("inside extra record") {
        const auto parsed = gl::assets::parse_hero_save_body(
            std::span<const std::uint8_t>(body.data(), extra_start + 10));
        REQUIRE_FALSE(parsed.has_value());
        CHECK(parsed.error().code == gl::ErrorCode::format);
    }
    SUBCASE("inside attached count") {
        const auto parsed = gl::assets::parse_hero_save_body(
            std::span<const std::uint8_t>(body.data(), extra_start + 0x14 + 2));
        REQUIRE_FALSE(parsed.has_value());
        CHECK(parsed.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("hero save body rejects an unreasonable extra count") {
    auto body = make_body({}, 1, 1, 0, 0);
    std::size_t offset = 0x734;
    for (const auto &container : std::array<std::uint32_t, 3>{1, 1, 1}) {
        offset += 4;
        if (container != 0)
            offset += 4;
    }
    offset += 8 + 0x80;
    CHECK(body.size() > offset + 4);
    body[offset] = 0xFF;
    body[offset + 1] = 0xFF;
    body[offset + 2] = 0xFF;
    body[offset + 3] = 0xFF;
    const auto parsed = gl::assets::parse_hero_save_body(body);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}
