#include "gl/assets/pad.hpp"

#include <array>
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

void append_animation(std::vector<std::uint8_t> &out, PadAnimationType type,
                      std::int32_t delay = kDelay, std::int32_t frames_per_row = kFramesPerRow,
                      std::int32_t rows = kRows, std::int32_t shadow_rows = kShadowRows,
                      std::int32_t seed = 0) {
    const auto crops_size = static_cast<std::uint32_t>(rows) * frames_per_row * 8;
    const auto shadow_size = static_cast<std::uint32_t>(shadow_rows) * frames_per_row * 8;

    put_u32(out, static_cast<std::uint32_t>(type));
    const std::size_t size_offset = out.size();
    put_i32(out, 0);
    put_i32(out, delay);
    put_i32(out, frames_per_row);
    put_i32(out, 32);
    put_i32(out, 24);
    put_i32(out, 7);
    put_i32(out, 9);
    put_f32(out, 1.5F);
    put_f32(out, -2.25F);
    put_u32(out, crops_size);
    for (std::int32_t row = 0; row < rows; ++row) {
        for (std::int32_t frame = 0; frame < frames_per_row; ++frame) {
            put_i16(out, static_cast<std::int16_t>(seed + row * 10 + frame));
            put_i16(out, static_cast<std::int16_t>(seed + row * 20 + frame));
            put_i16(out, static_cast<std::int16_t>(5 + frame));
            put_i16(out, static_cast<std::int16_t>(6 + row));
        }
    }

    put_i32(out, 16);
    put_i32(out, 8);
    put_i32(out, 3);
    put_i32(out, 4);
    put_u32(out, shadow_size);
    for (std::int32_t row = 0; row < shadow_rows; ++row) {
        for (std::int32_t frame = 0; frame < frames_per_row; ++frame) {
            put_i16(out, static_cast<std::int16_t>(seed + 100 + row));
            put_i16(out, static_cast<std::int16_t>(seed + 200 + frame));
            put_i16(out, static_cast<std::int16_t>(7 + frame));
            put_i16(out, static_cast<std::int16_t>(8 + row));
        }
    }
    set_i32(out, size_offset, static_cast<std::int32_t>(out.size() - size_offset));
}

std::vector<std::uint8_t> make_pad(std::vector<PadAnimationType> types, std::int32_t seed = 0,
                                   std::int32_t delay = kDelay) {
    std::uint32_t masks = 0;
    for (PadAnimationType type : types)
        masks |= static_cast<std::uint32_t>(type);

    std::vector<std::uint8_t> out;
    out.push_back('P');
    out.push_back('A');
    out.push_back('D');
    out.push_back(' ');
    put_u32(out, 0);
    put_u32(out, masks);
    for (std::size_t i = 0; i < types.size(); ++i)
        append_animation(out, types[i], delay, kFramesPerRow, kRows, kShadowRows,
                         seed + static_cast<std::int32_t>(i) * 1000);
    set_u32(out, 4, static_cast<std::uint32_t>(out.size()));
    return out;
}

std::vector<std::uint8_t> make_one_animation_pad() { return make_pad({PadAnimationType::rt_stay}); }

void append_tail_table(std::vector<std::uint8_t> &out, std::uint32_t count) {
    put_u32(out, count);
    for (std::uint32_t i = 0; i < count; ++i) {
        put_i32(out, static_cast<std::int32_t>(i));
        put_i32(out, -static_cast<std::int32_t>(i));
    }
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

TEST_CASE("pad: multiple animations are returned in file order") {
    auto data = make_pad(
        {PadAnimationType::rt_stay, PadAnimationType::rt_fun, PadAnimationType::tb_go}, 500);
    auto parsed = parse_pad(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->size() == 3);
    CHECK(parsed->at(0).type == PadAnimationType::rt_stay);
    CHECK(parsed->at(1).type == PadAnimationType::rt_fun);
    CHECK(parsed->at(2).type == PadAnimationType::tb_go);
    CHECK(parsed->at(0).crops[0][0].x == 500);
    CHECK(parsed->at(1).crops[0][0].x == 1500);
    CHECK(parsed->at(2).crops[0][0].x == 2500);
}

TEST_CASE("pad: go animation types are recognised") {
    CHECK(pad_is_go(PadAnimationType::tb_go));
    CHECK(pad_is_go(PadAnimationType::rt_go));
    CHECK_FALSE(pad_is_go(PadAnimationType::rt_stay));
    CHECK_FALSE(pad_is_go(PadAnimationType::die));
}

TEST_CASE("pad: person direction rows mirror reversed directions") {
    const std::array<std::uint8_t, 5> forward{0, 1, 2, 3, 4};
    for (std::uint8_t direction : forward) {
        const PadDirectionRow row = pad_person_row(PadAnimationType::rt_stay, direction);
        CHECK(row.row == direction);
        CHECK_FALSE(row.mirror);
    }

    CHECK(pad_person_row(PadAnimationType::rt_stay, 5).row == 3);
    CHECK(pad_person_row(PadAnimationType::rt_stay, 5).mirror);
    CHECK(pad_person_row(PadAnimationType::rt_stay, 6).row == 2);
    CHECK(pad_person_row(PadAnimationType::rt_stay, 6).mirror);
    CHECK(pad_person_row(PadAnimationType::rt_stay, 7).row == 1);
    CHECK(pad_person_row(PadAnimationType::rt_stay, 7).mirror);

    CHECK(pad_person_row(PadAnimationType::tb_go, 8).row == 8);
    CHECK_FALSE(pad_person_row(PadAnimationType::tb_go, 8).mirror);
    CHECK(pad_person_row(PadAnimationType::rt_go, 9).row == 7);
    CHECK(pad_person_row(PadAnimationType::rt_go, 9).mirror);
    CHECK(pad_person_row(PadAnimationType::tb_go, 15).row == 1);
    CHECK(pad_person_row(PadAnimationType::tb_go, 15).mirror);
}

TEST_CASE("pad: shadow direction rows never mirror") {
    for (std::uint8_t direction = 0; direction < 8; ++direction) {
        const PadDirectionRow row = pad_shadow_row(PadAnimationType::rt_stay, direction);
        CHECK(row.row == direction);
        CHECK_FALSE(row.mirror);
    }
    for (std::uint8_t direction = 0; direction < 16; ++direction) {
        const PadDirectionRow row = pad_shadow_row(PadAnimationType::tb_go, direction);
        CHECK(row.row == direction);
        CHECK_FALSE(row.mirror);
    }
}

TEST_CASE("pad: delay is preserved verbatim including non-positive values") {
    auto zero = make_pad({PadAnimationType::rt_stay}, 0, 0);
    auto zero_parsed = parse_pad(zero);
    REQUIRE(zero_parsed.has_value());
    CHECK(zero_parsed->front().delay == 0);

    auto negative = make_pad({PadAnimationType::rt_stay}, 0, -7);
    auto negative_parsed = parse_pad(negative);
    REQUIRE(negative_parsed.has_value());
    CHECK(negative_parsed->front().delay == -7);
}

TEST_CASE("pad: tail table and repeated animation records are ignored") {
    auto data = make_one_animation_pad();
    append_tail_table(data, 9);
    append_animation(data, PadAnimationType::rt_stay, 50, 1, 1, 0, 9000);
    set_u32(data, 4, static_cast<std::uint32_t>(data.size()));

    auto parsed = parse_pad(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->size() == 1);
    CHECK(parsed->front().crops[0][0].x == 0);
}

TEST_CASE("pad: arbitrary bytes after all masks are rejected") {
    auto data = make_one_animation_pad();
    put_u32(data, 0xDEADBEEFU);
    put_u32(data, 0);
    set_u32(data, 4, static_cast<std::uint32_t>(data.size()));

    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
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

TEST_CASE("pad: corrupt shadow table is rejected") {
    auto data = make_one_animation_pad();
    set_u32(data, 200, 7);
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: duplicate animation type is rejected") {
    auto data = make_pad({PadAnimationType::rt_stay, PadAnimationType::rt_fun});
    set_u32(data, kFileSize, static_cast<std::uint32_t>(PadAnimationType::rt_stay));
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: animation type missing from header mask is rejected") {
    auto data = make_one_animation_pad();
    set_u32(data, 12, static_cast<std::uint32_t>(PadAnimationType::rt_fun));
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: zero mask with a non-empty body is rejected") {
    auto data = make_one_animation_pad();
    set_u32(data, 8, 0);
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: data size smaller than the header is rejected") {
    auto data = make_one_animation_pad();
    set_u32(data, 4, 11);
    auto parsed = parse_pad(data);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);
}

TEST_CASE("pad: data size beyond the input is rejected") {
    auto data = make_one_animation_pad();
    set_u32(data, 4, static_cast<std::uint32_t>(data.size() + 1));
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
