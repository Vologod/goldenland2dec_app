#include "gl/assets/bytes.hpp"
#include "gl/assets/lvl.hpp"
#include <array>
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace gl::assets;

namespace {
struct Block {
    std::string name;
    Bytes payload;
};

void put_u16(Bytes& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

void put_i32(Bytes& b, std::int32_t v) { put_u32(b, static_cast<std::uint32_t>(v)); }

void put_string(Bytes& b, std::string_view raw) {
    put_u32(b, static_cast<std::uint32_t>(raw.size()));
    b.insert(b.end(), raw.begin(), raw.end());
}

void put_block(Bytes& out, std::string_view name, const Bytes& payload) {
    out.insert(out.end(), name.begin(), name.end());
    put_u32(out, static_cast<std::uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
}

Bytes assemble(const std::vector<Block>& blocks) {
    Bytes out;
    for (const auto& block : blocks) put_block(out, block.name, block.payload);
    return out;
}

Bytes description_entry(std::uint16_t param1, std::uint16_t param2, std::uint32_t number,
                        std::int32_t x, std::int32_t y, std::string_view raw_name) {
    Bytes b;
    put_u16(b, param1);
    put_u16(b, param2);
    put_u32(b, number);
    put_i32(b, x);
    put_i32(b, y);
    put_string(b, raw_name);
    return b;
}

Block make_lver() {
    Bytes p;
    put_u16(p, 3);
    put_u16(p, 7);
    return {"BLK_LVER", p};
}

Block make_mpsz() {
    Bytes p;
    put_u32(p, 640);
    put_u32(p, 480);
    return {"BLK_MPSZ", p};
}

Block make_mhdr() {
    Bytes p;
    put_u32(p, 2);
    put_u32(p, 2);
    for (std::uint16_t chunk = 0; chunk < 4; ++chunk) {
        for (std::uint16_t tile = 0; tile < 4; ++tile) {
            put_u16(p, static_cast<std::uint16_t>(chunk * 4 + tile));
            put_u16(p, static_cast<std::uint16_t>(chunk * 10 + tile));
            put_u16(p, static_cast<std::uint16_t>(chunk == 3 && tile == 3 ? 0xFFFF : 1));
        }
    }
    return {"BLK_MHDR", p};
}

Block make_mdsc() {
    Bytes p;
    put_u32(p, 2);
    put_u32(p, 0);
    put_u32(p, 1);
    put_u32(p, 10);
    put_u32(p, 20);
    put_u32(p, 99);
    put_u32(p, 42);
    put_u32(p, 300);
    put_u32(p, 400);
    return {"BLK_MDSC", p};
}

Block make_sdsc() {
    Bytes p;
    put_u32(p, 2);
    const Bytes a =
        description_entry(1, 2, 3, -5, 7, std::string_view("\xCF\xF0\xE8\xE2\xE5\xF2", 6));
    p.insert(p.end(), a.begin(), a.end());
    const Bytes b = description_entry(4, 5, 6, 8, 9, "static second");
    p.insert(p.end(), b.begin(), b.end());
    return {"BLK_SDSC", p};
}

Block make_adsc() {
    Bytes p;
    put_u32(p, 1);
    const Bytes e = description_entry(7, 8, 9, 11, 12, "anim");
    p.insert(p.end(), e.begin(), e.end());
    return {"BLK_ADSC", p};
}

Block make_tdsc() {
    Bytes p;
    put_u32(p, 1);
    const Bytes e = description_entry(13, 14, 15, 16, 17, "trigger");
    p.insert(p.end(), e.begin(), e.end());
    return {"BLK_TDSC", p};
}

Block make_cgrp() {
    Bytes p;
    put_u32(p, 2);
    put_string(p, "default");
    put_u32(p, 0);
    put_string(p, "CG_door");
    put_u32(p, 2);
    put_u16(p, 1);
    put_u16(p, 2);
    put_u16(p, 3);
    put_u16(p, 4);
    return {"BLK_CGRP", p};
}

Bytes senv_payload(std::uint32_t sound_count) {
    Bytes p;
    put_i32(p, 5);
    put_u32(p, 0);
    put_u32(p, 0);
    put_u32(p, 0);
    put_u32(p, sound_count);
    put_string(p, "");
    put_string(p, "");
    put_string(p, "");
    return p;
}

Block make_senv() { return {"BLK_SENV", senv_payload(0)}; }

Block make_wthr() {
    Bytes p;
    put_u16(p, 1);
    put_u16(p, 2);
    return {"BLK_WTHR", p};
}

Block make_door() {
    Bytes p;
    put_u32(p, 1);
    for (int i = 0; i < 6; ++i) put_string(p, "");
    return {"BLK_DOOR", p};
}

Block make_lfls() {
    Bytes p;
    put_u32(p, 5);
    return {"BLK_LFLS", p};
}

std::vector<Block> valid_blocks() {
    return {make_lver(),   make_mpsz(), make_mhdr(), make_mdsc(),
            make_sdsc(),   make_adsc(), make_tdsc(), make_cgrp(),
            make_senv(),   make_wthr(), make_door(), make_lfls()};
}

Bytes make_valid() { return assemble(valid_blocks()); }

std::size_t find_block_payload(const Bytes& data, std::string_view name) {
    std::size_t off = 0;
    while (off + 12 <= data.size()) {
        const std::string block_name(reinterpret_cast<const char*>(data.data() + off), 8);
        const std::uint32_t size = static_cast<std::uint32_t>(data[off + 8]) |
                                   (static_cast<std::uint32_t>(data[off + 9]) << 8) |
                                   (static_cast<std::uint32_t>(data[off + 10]) << 16) |
                                   (static_cast<std::uint32_t>(data[off + 11]) << 24);
        if (block_name == name) return off + 12;
        off += 12 + size;
    }
    return data.size();
}

void set_u32(Bytes& data, std::size_t offset, std::uint32_t value) {
    data[offset] = static_cast<std::uint8_t>(value & 0xFF);
    data[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    data[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    data[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}
} // namespace

TEST_CASE("LVL: parses all 12 blocks") {
    const Bytes data = make_valid();
    auto result = parse_lvl(data);
    REQUIRE(result.has_value());
    const LvlData& lvl = result.value();
    CHECK(lvl.version.major == 3);
    CHECK(lvl.version.minor == 7);
    CHECK(lvl.map_size.pixel_width == 640);
    CHECK(lvl.map_size.pixel_height == 480);
    CHECK(lvl.chunk_width == 2);
    CHECK(lvl.chunk_height == 2);
    REQUIRE(lvl.chunks.size() == 4);
    CHECK(lvl.chunks[0][0].relief == 0);
    CHECK(lvl.chunks[0][0].sound == 0);
    CHECK(lvl.chunks[0][0].mask == 1);
    CHECK(lvl.chunks[3][3].relief == 15);
    CHECK(lvl.chunks[3][3].sound == 33);
    CHECK(lvl.chunks[3][3].mask == 0xFFFF);

    REQUIRE(lvl.masks.size() == 2);
    CHECK(lvl.masks[0].number == 1);
    CHECK(lvl.masks[0].x == 10);
    CHECK(lvl.masks[0].y == 20);
    CHECK(lvl.masks[1].number == 42);
    CHECK(lvl.masks[1].x == 300);
    CHECK(lvl.masks[1].y == 400);

    REQUIRE(lvl.statics.size() == 2);
    CHECK(lvl.statics[0].param1 == 1);
    CHECK(lvl.statics[0].param2 == 2);
    CHECK(lvl.statics[0].number == 3);
    CHECK(lvl.statics[0].x == -5);
    CHECK(lvl.statics[0].y == 7);
    CHECK(lvl.statics[0].name == "Привет");
    CHECK(lvl.statics[1].name == "static second");

    REQUIRE(lvl.animations.size() == 1);
    CHECK(lvl.animations[0].name == "anim");
    CHECK(lvl.animations[0].x == 11);
    REQUIRE(lvl.triggers.size() == 1);
    CHECK(lvl.triggers[0].name == "trigger");
    CHECK(lvl.triggers[0].y == 17);

    REQUIRE(lvl.cell_groups.size() == 2);
    CHECK(lvl.cell_groups[0].name == "default");
    CHECK(lvl.cell_groups[0].cells.empty());
    REQUIRE(lvl.cell_groups[1].cells.size() == 2);
    CHECK(lvl.cell_groups[1].cells[0] == std::pair<std::uint16_t, std::uint16_t>{1, 2});
    CHECK(lvl.cell_groups[1].cells[1] == std::pair<std::uint16_t, std::uint16_t>{3, 4});
    CHECK(lvl.level_floors == 5);
}

TEST_CASE("LVL: validates SENV sound records") {
    auto blocks = valid_blocks();
    Bytes payload = senv_payload(1);
    put_string(payload, "sound.wav");
    payload.insert(payload.end(), 48, 0);
    blocks[8].payload = std::move(payload);
    auto result = parse_lvl(assemble(blocks));
    CHECK(result.has_value());
}

TEST_CASE("LVL: rejects a wrong block name") {
    auto blocks = valid_blocks();
    blocks[0].name = "BLK_BAD!";
    auto result = parse_lvl(assemble(blocks));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("LVL: rejects blocks out of order") {
    auto blocks = valid_blocks();
    std::swap(blocks[0], blocks[1]);
    auto result = parse_lvl(assemble(blocks));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("LVL: rejects truncated block headers and payloads") {
    SUBCASE("header") {
        Bytes data = make_valid();
        data.resize(5);
        auto result = parse_lvl(data);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("payload") {
        Bytes data = make_valid();
        data.resize(data.size() - 1);
        auto result = parse_lvl(data);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("LVL: rejects a block size beyond the file") {
    Bytes data = make_valid();
    set_u32(data, 24, 0xFFFFFFFFu);
    auto result = parse_lvl(data);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("LVL: rejects counts beyond the payload") {
    SUBCASE("masks") {
        Bytes data = make_valid();
        set_u32(data, find_block_payload(data, "BLK_MDSC"), 1000);
        auto result = parse_lvl(data);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("descriptions") {
        Bytes data = make_valid();
        set_u32(data, find_block_payload(data, "BLK_SDSC"), 1000);
        auto result = parse_lvl(data);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("cell groups") {
        Bytes data = make_valid();
        set_u32(data, find_block_payload(data, "BLK_CGRP"), 1000);
        auto result = parse_lvl(data);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("doors") {
        Bytes data = make_valid();
        set_u32(data, find_block_payload(data, "BLK_DOOR"), 100);
        auto result = parse_lvl(data);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("LVL: rejects a malformed tile header") {
    SUBCASE("truncated header") {
        auto blocks = valid_blocks();
        blocks[2].payload = {1, 0, 0, 0};
        auto result = parse_lvl(assemble(blocks));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("overflowing dimensions") {
        auto blocks = valid_blocks();
        blocks[2].payload.clear();
        put_u32(blocks[2].payload, 0x80000000u);
        put_u32(blocks[2].payload, 0x40000000u);
        auto result = parse_lvl(assemble(blocks));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("dimensions do not match payload") {
        Bytes data = make_valid();
        set_u32(data, find_block_payload(data, "BLK_MHDR"), 3);
        auto result = parse_lvl(data);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("LVL: rejects a string longer than its payload") {
    Bytes data = make_valid();
    const std::size_t payload = find_block_payload(data, "BLK_SDSC");
    set_u32(data, payload + 4 + 16, 0xFFFFFFFFu);
    auto result = parse_lvl(data);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("LVL: rejects invalid environment, weather and door payloads") {
    SUBCASE("SENV too short") {
        auto blocks = valid_blocks();
        blocks[8].payload.resize(31);
        auto result = parse_lvl(assemble(blocks));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("SENV sound path truncated") {
        auto blocks = valid_blocks();
        blocks[8].payload = senv_payload(1);
        auto result = parse_lvl(assemble(blocks));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("SENV sound record truncated") {
        auto blocks = valid_blocks();
        Bytes payload = senv_payload(1);
        put_string(payload, "sound.wav");
        payload.insert(payload.end(), 47, 0);
        blocks[8].payload = std::move(payload);
        auto result = parse_lvl(assemble(blocks));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("SENV sound count exceeds payload") {
        auto blocks = valid_blocks();
        blocks[8].payload = senv_payload(1000);
        auto result = parse_lvl(assemble(blocks));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("SENV trailing bytes") {
        auto blocks = valid_blocks();
        blocks[8].payload = senv_payload(0);
        blocks[8].payload.push_back(0);
        auto result = parse_lvl(assemble(blocks));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("WTHR wrong size") {
        auto blocks = valid_blocks();
        blocks[9].payload = {1, 0, 2};
        auto result = parse_lvl(assemble(blocks));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("DOOR strings overflow") {
        Bytes data = make_valid();
        const std::size_t payload = find_block_payload(data, "BLK_DOOR");
        set_u32(data, payload + 4, 0xFFFFFFFFu);
        auto result = parse_lvl(data);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("LVL: rejects trailing data") {
    Bytes data = make_valid();
    data.push_back(0);
    auto result = parse_lvl(data);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}
