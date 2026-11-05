#include "gl/assets/bytes.hpp"
#include "gl/assets/hero_equipment.hpp"

#include <doctest/doctest.h>
#include <miniz.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using namespace gl::assets;

namespace {
constexpr std::size_t kFixedPrefix = 0x734;
constexpr std::size_t kRecordFixed = 0xC4;
constexpr std::uint32_t kAbsent = 0xFFFFFFFFu;

void put_u32(Bytes &bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
}

void add_u32(Bytes &bytes, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
}

Bytes make_record(std::uint32_t item_id, std::uint32_t type, std::uint32_t flags,
                  std::uint32_t style, std::uint32_t subcount, std::uint32_t count = 1) {
    Bytes record(kRecordFixed + subcount * 0x14, 0);
    put_u32(record, 0x08, type);
    put_u32(record, 0x0C, flags);
    put_u32(record, 0x20, style);
    put_u32(record, 0xA8, item_id);
    put_u32(record, 0xB0, count);
    put_u32(record, 0xC0, subcount);
    return record;
}

void append_container(Bytes &body, std::uint32_t id, const std::vector<Bytes> &records) {
    add_u32(body, id);
    if (id == kAbsent)
        return;
    add_u32(body, static_cast<std::uint32_t>(records.size()));
    for (const Bytes &record : records)
        body.insert(body.end(), record.begin(), record.end());
}

Bytes make_body(const std::vector<Bytes> &container0, const std::vector<Bytes> &container1,
                const std::vector<Bytes> &container2) {
    Bytes body(kFixedPrefix, 0);
    append_container(body, 1, container0);
    append_container(body, 1, container1);
    append_container(body, 1, container2);
    return body;
}

Bytes make_header() {
    Bytes header(112, 0);
    constexpr std::array<std::uint8_t, 20> magic = {'G', 'o', 'l', 'd', 'e', 'n', 'l',
                                                    'a', 'n', 'd', ' ', 'I', 'I', ' ',
                                                    's', 'a', 'v', 'e', ' ', '\0'};
    for (std::size_t i = 0; i < magic.size(); ++i)
        header[i] = magic[i];
    put_u32(header, 20, 17);
    return header;
}

Bytes zlib_compress(std::span<const std::uint8_t> data) {
    Bytes out(mz_compressBound(data.size()));
    mz_ulong out_len = out.size();
    const int status = mz_compress(out.data(), &out_len, data.data(), data.size());
    if (status != MZ_OK)
        return {};
    out.resize(out_len);
    return out;
}

Bytes make_save(const Bytes &decompressed) {
    Bytes file = make_header();
    const Bytes compressed = zlib_compress(decompressed);
    file.insert(file.end(), compressed.begin(), compressed.end());
    return file;
}

Bytes make_hero_payload(const Bytes &hero_body) {
    Bytes payload = {'j', 'u', 'n', 'k', 'B', 'T', '_', 'H', 'E', 'R', 'O', ' '};
    add_u32(payload, static_cast<std::uint32_t>(hero_body.size()));
    payload.insert(payload.end(), hero_body.begin(), hero_body.end());
    return payload;
}
} // namespace

TEST_CASE("parse_hero_equipment reads third BT_HERO container") {
    const Bytes body = make_body({}, {}, {make_record(318, 12, 0x100000, 13, 0)});

    auto parsed = parse_hero_equipment(body);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->items.size() == 1);
    CHECK(parsed->items[0].item_id == 318);
    CHECK(parsed->items[0].type == 12);
    CHECK(parsed->items[0].flags == 0x100000);
    CHECK(parsed->items[0].style == 13);
}

TEST_CASE("parse_hero_equipment skips an absent container and continues") {
    Bytes body(kFixedPrefix, 0);
    append_container(body, kAbsent, {});
    append_container(body, 1, {});
    append_container(body, 1, {make_record(397, 14, 0x200000, 11, 2)});

    auto parsed = parse_hero_equipment(body);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->items.size() == 1);
    CHECK(parsed->items[0].item_id == 397);
    CHECK(parsed->items[0].flags == 0x200000);
}

TEST_CASE("parse_hero_equipment advances by subrecord stride") {
    const Bytes body = make_body(
        {}, {}, {make_record(72, 15, 0x040000, 11, 2), make_record(280, 11, 0x004000, 12, 1)});

    auto parsed = parse_hero_equipment(body);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->items.size() == 2);
    CHECK(parsed->items[0].item_id == 72);
    CHECK(parsed->items[0].type == 15);
    CHECK(parsed->items[0].style == 11);
    CHECK(parsed->items[1].item_id == 280);
    CHECK(parsed->items[1].type == 11);
    CHECK(parsed->items[1].style == 12);
}

TEST_CASE("parse_hero_equipment returns empty items when equipped container is absent") {
    Bytes body(kFixedPrefix, 0);
    append_container(body, 1, {});
    append_container(body, 1, {});
    append_container(body, kAbsent, {});

    auto parsed = parse_hero_equipment(body);
    REQUIRE(parsed.has_value());
    CHECK(parsed->items.empty());
}

TEST_CASE("parse_hero_equipment rejects a truncated fixed header") {
    const Bytes body(kFixedPrefix - 1, 0);
    auto parsed = parse_hero_equipment(body);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("parse_hero_equipment rejects a truncated record header") {
    Bytes body(kFixedPrefix, 0);
    append_container(body, 1, {});
    append_container(body, 1, {});
    add_u32(body, 1);
    add_u32(body, 1);
    body.resize(body.size() + 0x40);

    auto parsed = parse_hero_equipment(body);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("parse_hero_equipment rejects truncated subrecords") {
    Bytes body(kFixedPrefix, 0);
    append_container(body, 1, {});
    append_container(body, 1, {});
    add_u32(body, 1);
    add_u32(body, 1);
    Bytes record = make_record(318, 12, 0x100000, 13, 3);
    record.resize(record.size() - 1);
    body.insert(body.end(), record.begin(), record.end());

    auto parsed = parse_hero_equipment(body);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("parse_hero_inventory reads all three BT_HERO containers") {
    const Bytes body = make_body({make_record(1554, 22, 0x00001008, 4, 0, 2)},
                                 {make_record(318, 12, 0x100000, 13, 1, 5)},
                                 {make_record(72, 15, 0x040000, 11, 0, 7)});

    auto inventory = parse_hero_inventory(body);
    REQUIRE(inventory.has_value());
    REQUIRE(inventory->hot.size() == 1);
    REQUIRE(inventory->items.size() == 1);
    REQUIRE(inventory->equipped.size() == 1);

    CHECK(inventory->hot[0].item_id == 1554);
    CHECK(inventory->hot[0].type == 22);
    CHECK(inventory->hot[0].flags == 0x00001008);
    CHECK(inventory->hot[0].style == 4);
    CHECK(inventory->hot[0].count == 2);

    CHECK(inventory->items[0].item_id == 318);
    CHECK(inventory->items[0].type == 12);
    CHECK(inventory->items[0].flags == 0x100000);
    CHECK(inventory->items[0].style == 13);
    CHECK(inventory->items[0].count == 5);

    CHECK(inventory->equipped[0].item_id == 72);
    CHECK(inventory->equipped[0].type == 15);
    CHECK(inventory->equipped[0].flags == 0x040000);
    CHECK(inventory->equipped[0].style == 11);
    CHECK(inventory->equipped[0].count == 1);
}

TEST_CASE("parse_hero_inventory leaves an absent container empty") {
    Bytes body(kFixedPrefix, 0);
    append_container(body, kAbsent, {});
    append_container(body, 1, {make_record(486, 1, 0x00001025, 3, 0, 9)});
    append_container(body, kAbsent, {});

    auto inventory = parse_hero_inventory(body);
    REQUIRE(inventory.has_value());
    CHECK(inventory->hot.empty());
    REQUIRE(inventory->items.size() == 1);
    CHECK(inventory->items[0].item_id == 486);
    CHECK(inventory->items[0].count == 9);
    CHECK(inventory->equipped.empty());
}

TEST_CASE("parse_hero_inventory rejects truncated item subrecords") {
    Bytes body(kFixedPrefix, 0);
    append_container(body, 1, {});
    append_container(body, 1, {});
    add_u32(body, 1);
    add_u32(body, 1);
    Bytes record = make_record(318, 12, 0x100000, 13, 3);
    record.resize(record.size() - 1);
    body.insert(body.end(), record.begin(), record.end());

    auto inventory = parse_hero_inventory(body);
    REQUIRE_FALSE(inventory.has_value());
    CHECK(inventory.error().code == gl::ErrorCode::format);
}

TEST_CASE("decode_save_body inflates zlib and returns the BT_HERO body") {
    const Bytes hero_body = make_body({}, {}, {make_record(318, 12, 0x100000, 13, 0)});
    const Bytes file = make_save(make_hero_payload(hero_body));

    auto decoded = decode_save_body(file);
    REQUIRE(decoded.has_value());
    CHECK(decoded.value() == hero_body);
}

TEST_CASE("decode_save_body rejects a bad header") {
    Bytes file(112, 0);
    auto decoded = decode_save_body(file);
    REQUIRE_FALSE(decoded.has_value());
    CHECK(decoded.error().code == gl::ErrorCode::format);
}

TEST_CASE("decode_save_body rejects a file without a zlib stream") {
    const Bytes file = make_header();
    auto decoded = decode_save_body(file);
    REQUIRE_FALSE(decoded.has_value());
    CHECK(decoded.error().code == gl::ErrorCode::format);
}

TEST_CASE("decode_save_body rejects invalid zlib data") {
    Bytes file = make_header();
    for (int i = 0; i < 16; ++i)
        file.push_back(0x7F);
    auto decoded = decode_save_body(file);
    REQUIRE_FALSE(decoded.has_value());
    CHECK(decoded.error().code == gl::ErrorCode::format);
}

TEST_CASE("decode_save_body rejects data without a BT_HERO tag") {
    const Bytes file = make_save(Bytes(64, 'x'));
    auto decoded = decode_save_body(file);
    REQUIRE_FALSE(decoded.has_value());
    CHECK(decoded.error().code == gl::ErrorCode::format);
}

TEST_CASE("decode_save_body rejects a BT_HERO size past the payload") {
    const Bytes hero_body = make_body({}, {}, {});
    Bytes payload = {'B', 'T', '_', 'H', 'E', 'R', 'O', ' '};
    add_u32(payload, static_cast<std::uint32_t>(hero_body.size() + 10));
    payload.insert(payload.end(), hero_body.begin(), hero_body.end());
    const Bytes file = make_save(payload);

    auto decoded = decode_save_body(file);
    REQUIRE_FALSE(decoded.has_value());
    CHECK(decoded.error().code == gl::ErrorCode::format);
}
