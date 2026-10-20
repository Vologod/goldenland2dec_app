#include "gl/assets/bytes.hpp"
#include "gl/assets/save.hpp"
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <initializer_list>
#include <span>
#include <string>

using namespace gl::assets;

namespace {
constexpr const char kMagic[] = "Goldenland II save ";

void put_u16(Bytes& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFF);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

void put_u32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
}

Bytes make_header(std::initializer_list<std::uint8_t> title) {
    Bytes bytes(112, 0);
    for (std::size_t i = 0; i < 20; ++i) bytes[i] = static_cast<std::uint8_t>(kMagic[i]);
    put_u32(bytes, 20, 17);
    put_u16(bytes, 24, 2022);
    put_u16(bytes, 26, 11);
    put_u16(bytes, 30, 7);
    put_u16(bytes, 32, 10);
    put_u16(bytes, 34, 20);
    put_u16(bytes, 36, 30);
    std::size_t offset = 40;
    for (std::uint8_t byte : title) bytes[offset++] = byte;
    return bytes;
}
} // namespace

TEST_CASE("save: parses an ASCII header") {
    const Bytes data = make_header({'Q', 'u', 'i', 'c', 'k', ' ', 's', 'a', 'v', 'e'});
    auto result = parse_save_header(data);
    REQUIRE(result.has_value());
    CHECK(result->version == 17);
    CHECK(result->year == 2022);
    CHECK(result->month == 11);
    CHECK(result->day == 7);
    CHECK(result->time_a == 10);
    CHECK(result->time_b == 20);
    CHECK(result->time_c == 30);
    CHECK(result->title == "Quick save");
}

TEST_CASE("save: decodes a cp1251 title") {
    const Bytes data = make_header({0xD1, 0xE5, 0xEC, 0xE5, 0xE9, 0xF1, 0xF2, 0xE2,
                                    0xEE, 0x20, 0xEA, 0xF3, 0xEA, 0xF3, 0xFF});
    auto result = parse_save_header(data);
    REQUIRE(result.has_value());
    CHECK(result->title == "Семейство кукуя");
}

TEST_CASE("save: accepts an empty title") {
    const Bytes data = make_header({});
    auto result = parse_save_header(data);
    REQUIRE(result.has_value());
    CHECK(result->title.empty());
}

TEST_CASE("save: keeps a full 64-byte title without NUL") {
    const std::string expected(64, 'x');
    const Bytes data = make_header({});
    auto mutable_data = data;
    for (std::size_t i = 0; i < 64; ++i) mutable_data[40 + i] = static_cast<std::uint8_t>('x');
    auto result = parse_save_header(mutable_data);
    REQUIRE(result.has_value());
    CHECK(result->title == expected);
}

TEST_CASE("save: trims trailing spaces from the title") {
    const Bytes base = make_header({});
    auto data = base;
    const std::string title = "Slot 1   ";
    for (std::size_t i = 0; i < title.size(); ++i)
        data[40 + i] = static_cast<std::uint8_t>(title[i]);
    auto result = parse_save_header(data);
    REQUIRE(result.has_value());
    CHECK(result->title == "Slot 1");
}

TEST_CASE("save: rejects data shorter than the header") {
    const Bytes data = make_header({'Q'});
    const auto short_data = std::span<const std::uint8_t>(data.data(), 111);
    auto result = parse_save_header(short_data);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("save: rejects a bad magic") {
    auto data = make_header({'Q'});
    data[0] = 'X';
    auto result = parse_save_header(data);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}
