#include "gl/assets/pad.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <vector>

using namespace gl::assets;

namespace {
void put_u16(std::vector<std::uint8_t> &out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void put_i16(std::vector<std::uint8_t> &out, std::int16_t value) {
    put_u16(out, std::bit_cast<std::uint16_t>(value));
}

void put_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}

void put_i32(std::vector<std::uint8_t> &out, std::int32_t value) {
    put_u32(out, std::bit_cast<std::uint32_t>(value));
}

void put_f32(std::vector<std::uint8_t> &out, float value) {
    put_u32(out, std::bit_cast<std::uint32_t>(value));
}

void set_u32(std::vector<std::uint8_t> &out, std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out[offset + shift / 8] = static_cast<std::uint8_t>((value >> shift) & 0xFF);
}

void set_i32(std::vector<std::uint8_t> &out, std::size_t offset, std::int32_t value) {
    set_u32(out, offset, std::bit_cast<std::uint32_t>(value));
}

constexpr std::int32_t kDelay = 100;
constexpr std::int32_t kFramesPerRow = 2;
constexpr std::int32_t kRows = 8;
constexpr std::int32_t kShadowRows = 2;
constexpr std::uint32_t kCropsSize = static_cast<std::uint32_t>(kRows) * kFramesPerRow * 8;
constexpr std::uint32_t kShadowSize = static_cast<std::uint32_t>(kShadowRows) * kFramesPerRow * 8;
constexpr std::uint32_t kAnimationSize = 4 + 32 + 4 + kCropsSize + 16 + 4 + kShadowSize;
constexpr std::uint32_t kFileSize = 12 + 4 + kAnimationSize;

std::vector<std::uint8_t> make_one_animation_pad() {
    std::vector<std::uint8_t> out;
    out.push_back('P');
    out.push_back('A');
    out.push_back('D');
    out.push_back(' ');
    put_u32(out, kFileSize);
    put_u32(out, 1);

    put_u32(out, static_cast<std::uint32_t>(PadAnimationType::rt_stay));
    put_i32(out, static_cast<std::int32_t>(kAnimationSize));
    put_i32(out, kDelay);
    put_i32(out, kFramesPerRow);
    put_i32(out, 32);
    put_i32(out, 24);
    put_i32(out, 7);
    put_i32(out, 9);
    put_f32(out, 1.5F);
    put_f32(out, -2.25F);
    put_u32(out, kCropsSize);
    for (std::int32_t row = 0; row < kRows; ++row) {
        for (std::int32_t frame = 0; frame < kFramesPerRow; ++frame) {
            put_i16(out, static_cast<std::int16_t>(row * 10 + frame));
            put_i16(out, static_cast<std::int16_t>(row * 20 + frame));
            put_i16(out, static_cast<std::int16_t>(5 + frame));
            put_i16(out, static_cast<std::int16_t>(6 + row));
        }
    }

    put_i32(out, 16);
    put_i32(out, 8);
    put_i32(out, 3);
    put_i32(out, 4);
    put_u32(out, kShadowSize);
    for (std::int32_t row = 0; row < kShadowRows; ++row) {
        for (std::int32_t frame = 0; frame < kFramesPerRow; ++frame) {
            put_i16(out, static_cast<std::int16_t>(100 + row));
            put_i16(out, static_cast<std::int16_t>(200 + frame));
            put_i16(out, static_cast<std::int16_t>(7 + frame));
            put_i16(out, static_cast<std::int16_t>(8 + row));
        }
    }
    return out;
}
} // namespace

TEST_CASE("pad: one animation with crop and shadow tables") {
    const auto data = make_one_animation_pad();
    auto parsed = parse_pad(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->size() == 1);

    const PadAnimation &animation = parsed->front();
    CHECK(animation.type == PadAnimationType::rt_stay);
    CHECK(animation.delay == kDelay);
    CHECK(animation.frames_per_row == kFramesPerRow);
    CHECK(animation.frame_width == 32);
    CHECK(animation.frame_height == 24);
    CHECK(animation.anchor_x == 7);
    CHECK(animation.anchor_y == 9);
    CHECK(animation.movement_x == doctest::Approx(1.5F));
    CHECK(animation.movement_y == doctest::Approx(-2.25F));

    REQUIRE(animation.crops.size() == kRows);
    for (std::size_t row = 0; row < animation.crops.size(); ++row) {
        REQUIRE(animation.crops[row].size() == kFramesPerRow);
        for (std::size_t frame = 0; frame < animation.crops[row].size(); ++frame) {
            const PadCrop &crop = animation.crops[row][frame];
            CHECK(crop.x == static_cast<std::int16_t>(row * 10 + frame));
            CHECK(crop.y == static_cast<std::int16_t>(row * 20 + frame));
            CHECK(crop.w == static_cast<std::int16_t>(5 + frame));
            CHECK(crop.h == static_cast<std::int16_t>(6 + row));
        }
    }

    CHECK(animation.shadow_width == 16);
    CHECK(animation.shadow_height == 8);
    CHECK(animation.shadow_anchor_x == 3);
    CHECK(animation.shadow_anchor_y == 4);
    REQUIRE(animation.shadow_crops.size() == kShadowRows);
    for (std::size_t row = 0; row < animation.shadow_crops.size(); ++row) {
        REQUIRE(animation.shadow_crops[row].size() == kFramesPerRow);
        for (std::size_t frame = 0; frame < animation.shadow_crops[row].size(); ++frame) {
            const PadCrop &crop = animation.shadow_crops[row][frame];
            CHECK(crop.x == static_cast<std::int16_t>(100 + row));
            CHECK(crop.y == static_cast<std::int16_t>(200 + frame));
            CHECK(crop.w == static_cast<std::int16_t>(7 + frame));
            CHECK(crop.h == static_cast<std::int16_t>(8 + row));
        }
    }
}

TEST_CASE("pad: go animation types are recognised") {
    CHECK(pad_is_go(PadAnimationType::tb_go));
    CHECK(pad_is_go(PadAnimationType::rt_go));
    CHECK_FALSE(pad_is_go(PadAnimationType::rt_stay));
    CHECK_FALSE(pad_is_go(PadAnimationType::die));
}

TEST_CASE("pad: bad magic is rejected") {
    auto data = make_one_animation_pad();
    data[0] = 'X';
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: truncated data is rejected") {
    auto data = make_one_animation_pad();
    data.pop_back();
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: bad crop table size is rejected") {
    auto data = make_one_animation_pad();
    set_u32(data, 52, 7);
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: oversized crop table is rejected") {
    auto data = make_one_animation_pad();
    set_u32(data, 52, 0xFFFFFFFFU);
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: unknown animation type is rejected") {
    auto data = make_one_animation_pad();
    set_u32(data, 12, 3);
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: unknown animation mask is rejected") {
    auto data = make_one_animation_pad();
    set_u32(data, 8, 0x2000000U);
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: animation size must match its payload") {
    auto data = make_one_animation_pad();
    set_i32(data, 16, static_cast<std::int32_t>(kAnimationSize) - 4);
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}
