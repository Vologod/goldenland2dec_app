#include "gl/assets/bytes.hpp"
#include "gl/assets/lao.hpp"
#include <cstdint>
#include <doctest/doctest.h>
#include <initializer_list>
#include <utility>

using namespace gl::assets;

namespace {
void put_u32(Bytes& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

Bytes make_lao(std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> pairs) {
    Bytes bytes;
    for (const auto& [height, delay] : pairs) {
        put_u32(bytes, height);
        put_u32(bytes, delay);
    }
    return bytes;
}
} // namespace

TEST_CASE("LAO: parses height and delay pairs") {
    const Bytes data = make_lao({{48, 100}, {64, 250}, {96, 1000}});
    auto result = parse_lao(data);
    REQUIRE(result.has_value());
    const auto& entries = result.value();
    REQUIRE(entries.size() == 3);
    CHECK(entries[0].height == 48);
    CHECK(entries[0].delay == 100);
    CHECK(entries[1].height == 64);
    CHECK(entries[1].delay == 250);
    CHECK(entries[2].height == 96);
    CHECK(entries[2].delay == 1000);
}

TEST_CASE("LAO: accepts an empty file") {
    const Bytes data;
    auto result = parse_lao(data);
    REQUIRE(result.has_value());
    CHECK(result.value().empty());
}

TEST_CASE("LAO: rejects a size that is not a multiple of 8") {
    SUBCASE("single truncated entry") {
        const Bytes data = {0x10, 0x00, 0x00, 0x00};
        auto result = parse_lao(data);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("one trailing byte") {
        Bytes data = make_lao({{32, 100}});
        data.push_back(0);
        auto result = parse_lao(data);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
}
