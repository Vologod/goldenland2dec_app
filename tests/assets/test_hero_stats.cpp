#include "gl/assets/hero_stats.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
void put_i32(std::vector<std::uint8_t> &body, std::size_t offset, std::int32_t value) {
    for (int i = 0; i < 4; ++i)
        body[offset + i] =
            static_cast<std::uint8_t>((static_cast<std::uint32_t>(value) >> (i * 8)) & 0xFF);
}
} // namespace

TEST_CASE("hero_stats use the BT_HERO stat block") {
    std::vector<std::uint8_t> body(0x134, 0);
    put_i32(body, 0x00C + 0x00, 3);
    put_i32(body, 0x00C + 0x04, 8);
    put_i32(body, 0x00C + 0x08, 7);
    put_i32(body, 0x00C + 0x0C, 15);
    put_i32(body, 0x00C + 0x10, 13);
    put_i32(body, 0x00C + 0x14, 8);
    put_i32(body, 0x00C + 0x18, 7);
    put_i32(body, 0x00C + 0x1C, 11);
    put_i32(body, 0x00C + 0x20, 1234);
    put_i32(body, 0x128, 263);
    put_i32(body, 0x12C, 5);
    put_i32(body, 0x130, 99);
    put_i32(body, 0x00C + 0x02C + 4 * 6, 10);
    put_i32(body, 0x00C + 0x02C + 4 * 13, 15);
    put_i32(body, 0x00C + 0x02C + 4 * 24, 15);
    put_i32(body, 0x00C + 0x02C + 4 * 26, 10);

    const auto stats = gl::assets::parse_hero_stats(body);
    REQUIRE(stats);
    CHECK(stats->strength == 3);
    CHECK(stats->constitution == 8);
    CHECK(stats->dexterity == 7);
    CHECK(stats->perception == 15);
    CHECK(stats->intelligence == 13);
    CHECK(stats->wisdom == 8);
    CHECK(stats->luck == 7);
    CHECK(stats->reputation == 11);
    CHECK(stats->experience == 1234);
    CHECK(stats->life == 263);
    CHECK(stats->energy == 5);
    CHECK(stats->extra == 99);
    CHECK(stats->skills[6] == 10);
    CHECK(stats->skills[13] == 15);
    CHECK(stats->skills[24] == 15);
    CHECK(stats->skills[26] == 10);
    CHECK(stats->skills[0] == 0);
}

TEST_CASE("hero_stats reject a short body") {
    const std::array<std::uint8_t, 16> body{};
    CHECK_FALSE(gl::assets::parse_hero_stats(body));
}
