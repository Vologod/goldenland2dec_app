#include "gl/assets/iad.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace {
void put_u32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

void put_u16(std::vector<std::uint8_t>& data, std::size_t offset, std::uint16_t value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
}
} // namespace

TEST_CASE("parse_iad reads a block and its crop table") {
    std::vector<std::uint8_t> data(56, 0);
    std::memcpy(data.data(), "IAD ", 4);
    put_u32(data, 4, 1);
    put_u32(data, 8, 48);
    put_u32(data, 12, 12);
    put_u32(data, 16, 36);
    put_u32(data, 20, 37);
    put_u32(data, 24, 165);
    put_u32(data, 28, 154);
    put_u32(data, 32, 16);
    put_u16(data, 36, 1);
    put_u16(data, 38, 2);
    put_u16(data, 40, 32);
    put_u16(data, 42, 36);
    put_u16(data, 44, 3);
    put_u16(data, 46, 4);
    put_u16(data, 48, 20);
    put_u16(data, 50, 21);

    auto parsed = gl::assets::parse_iad(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->size() == 1);
    const gl::assets::IadBlock& block = parsed->front();
    CHECK(block.type == 1);
    CHECK(block.frames == 12);
    CHECK(block.frame_width == 36);
    CHECK(block.frame_height == 37);
    CHECK(block.image_width == 165);
    CHECK(block.image_height == 154);
    REQUIRE(block.crops.size() == 2);
    CHECK(block.crops[0].x == 1);
    CHECK(block.crops[0].y == 2);
    CHECK(block.crops[0].w == 32);
    CHECK(block.crops[0].h == 36);
    CHECK(block.crops[1].x == 3);
    CHECK(block.crops[1].y == 4);
}

TEST_CASE("parse_iad rejects bad magic and truncated tables") {
    std::vector<std::uint8_t> data(36, 0);
    std::memcpy(data.data(), "NOPE", 4);
    CHECK_FALSE(gl::assets::parse_iad(data).has_value());

    std::memcpy(data.data(), "IAD ", 4);
    put_u32(data, 4, 1);
    put_u32(data, 8, 32);
    put_u32(data, 32, 16);
    CHECK_FALSE(gl::assets::parse_iad(data).has_value());
}
