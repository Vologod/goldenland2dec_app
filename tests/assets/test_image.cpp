#include <doctest/doctest.h>
#include "gl/assets/image.hpp"

using namespace gl::assets;

namespace {
Bytes make_2x2_bmp() {
    Bytes b;
    auto u16 = [&](std::uint16_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); };
    auto u32 = [&](std::uint32_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
                                      b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF); };
    b.push_back('B'); b.push_back('M');
    u32(70); u32(0); u32(54);       // file header
    u32(40); u32(2); u32(2); u16(1); u16(24);
    u32(0); u32(16); u32(2835); u32(2835); u32(0); u32(0);
    // bottom row: blue, white; top row: red, white (BGR, rows padded to 8 bytes)
    b.insert(b.end(), {255,0,0, 255,255,255, 0,0});
    b.insert(b.end(), {0,0,255, 255,255,255, 0,0});
    return b;
}
}

TEST_CASE("Image: decodes a 2x2 BMP to RGBA") {
    Bytes bmp = make_2x2_bmp();
    auto image = decode_image(bmp);
    REQUIRE(image.has_value());
    CHECK(image->width == 2);
    CHECK(image->height == 2);
    // stb returns top-down; the BMP file stored the bottom row first.
    CHECK(image->rgba[0].r == 255); // red top-left
    CHECK(image->rgba[0].g == 0);
    CHECK(image->rgba[0].b == 0);
    CHECK(image->rgba[2].r == 0);   // blue bottom-left
    CHECK(image->rgba[2].b == 255);
    CHECK(image->rgba[3].r == 255); // white bottom-right
    CHECK(image->rgba[3].g == 255);
    CHECK(image->rgba[3].b == 255);
}

TEST_CASE("Image: rejects garbage") {
    const std::uint8_t data[] = {1,2,3,4};
    auto image = decode_image(data);
    CHECK_FALSE(image.has_value());
}
