#include "gl/assets/csx_frames.hpp"
#include <cstdint>
#include <doctest/doctest.h>

using namespace gl::assets;

namespace {
CsxImage make_strip() {
    CsxImage image;
    image.width = 2;
    image.height = 6;
    image.rgba.reserve(12);
    for (std::uint32_t row = 0; row < image.height; ++row) {
        for (std::uint32_t column = 0; column < image.width; ++column) {
            const auto value = static_cast<std::uint8_t>(row * 10 + column);
            image.rgba.push_back(Rgba{value, static_cast<std::uint8_t>(value + 1),
                                      static_cast<std::uint8_t>(value + 2),
                                      static_cast<std::uint8_t>(row + 1)});
        }
    }
    return image;
}
} // namespace

TEST_CASE("CSX frames: slices a 2x6 strip into 2x2 frames") {
    const CsxImage image = make_strip();
    auto result = slice_csx_frames(image, 2);
    REQUIRE(result.has_value());
    const auto& frames = result.value();
    REQUIRE(frames.size() == 3);
    for (const auto& frame : frames) {
        CHECK(frame.width == 2);
        CHECK(frame.height == 2);
        CHECK(frame.rgba.size() == 4);
    }
    CHECK(frames[0].rgba[0].r == 0);
    CHECK(frames[0].rgba[1].r == 1);
    CHECK(frames[0].rgba[2].r == 10);
    CHECK(frames[0].rgba[3].r == 11);
    CHECK(frames[0].rgba[3].a == 2);
    CHECK(frames[1].rgba[0].r == 20);
    CHECK(frames[1].rgba[3].r == 31);
    CHECK(frames[2].rgba[0].r == 40);
    CHECK(frames[2].rgba[0].g == 41);
    CHECK(frames[2].rgba[0].b == 42);
    CHECK(frames[2].rgba[0].a == 5);
    CHECK(frames[2].rgba[3].r == 51);
}

TEST_CASE("CSX frames: last frame may be shorter") {
    const CsxImage image = make_strip();
    auto result = slice_csx_frames(image, 4);
    REQUIRE(result.has_value());
    const auto& frames = result.value();
    REQUIRE(frames.size() == 2);
    CHECK(frames[0].height == 4);
    CHECK(frames[0].rgba.size() == 8);
    CHECK(frames[0].rgba[7].r == 31);
    CHECK(frames[1].width == 2);
    CHECK(frames[1].height == 2);
    REQUIRE(frames[1].rgba.size() == 4);
    CHECK(frames[1].rgba[0].r == 40);
    CHECK(frames[1].rgba[3].r == 51);
}

TEST_CASE("CSX frames: rejects invalid frame heights") {
    const CsxImage image = make_strip();
    SUBCASE("zero") {
        auto result = slice_csx_frames(image, 0);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("taller than the image") {
        auto result = slice_csx_frames(image, 7);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("CSX frames: rejects empty images") {
    SUBCASE("zero width") {
        CsxImage image;
        image.height = 4;
        auto result = slice_csx_frames(image, 2);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
    SUBCASE("zero height") {
        CsxImage image;
        image.width = 2;
        auto result = slice_csx_frames(image, 1);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
}
