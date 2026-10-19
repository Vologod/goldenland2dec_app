#include <doctest/doctest.h>
#include "gl/assets/image.hpp"
#include <chrono>
#include <climits>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace gl::assets;

namespace {
Bytes make_bmp_header(std::uint32_t width, std::uint32_t height) {
    Bytes b;
    auto u16 = [&](std::uint16_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); };
    auto u32 = [&](std::uint32_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
                                      b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF); };
    b.push_back('B'); b.push_back('M');
    u32(54); u32(0); u32(54);
    u32(40); u32(width); u32(height); u16(1); u16(24);
    u32(0); u32(0); u32(2835); u32(2835); u32(0); u32(0);
    return b;
}
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

TEST_CASE("Image: rejects an oversized header before pixel decoding") {
    Bytes bmp = make_bmp_header(100000, 100000);
    auto image = decode_image(bmp);
    REQUIRE_FALSE(image.has_value());
    CHECK(image.error().message.find("pixel limit") != std::string::npos);
}

TEST_CASE("Image: rejects buffers above INT_MAX") {
    const std::uint8_t byte = 0;
    std::span<const std::uint8_t> huge(&byte, static_cast<std::size_t>(INT_MAX) + 1);
    auto image = decode_image(huge);
    REQUIRE_FALSE(image.has_value());
    CHECK(image.error().code == gl::ErrorCode::format);
    CHECK(image.error().message.find("too large") != std::string::npos);
}

TEST_CASE("Image: write_png round-trips RGBA pixels") {
    Image input;
    input.width = 2;
    input.height = 2;
    input.rgba = {Rgba{255, 0, 0, 255}, Rgba{0, 255, 0, 255},
                  Rgba{0, 0, 255, 128}, Rgba{255, 255, 255, 0}};
    const auto path = std::filesystem::temp_directory_path() /
        ("gl_image_roundtrip_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".png");

    const bool wrote = write_png(path, input);
    CHECK(wrote);
    if (wrote) {
        std::ifstream file(path, std::ios::binary);
        Bytes png((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        auto decoded = decode_image(png);
        CHECK(decoded.has_value());
        if (decoded.has_value()) {
            CHECK(decoded->width == 2);
            CHECK(decoded->height == 2);
            CHECK(decoded->rgba.size() == 4);
            if (decoded->rgba.size() == 4) {
                CHECK(decoded->rgba[0].r == 255);
                CHECK(decoded->rgba[0].a == 255);
                CHECK(decoded->rgba[2].b == 255);
                CHECK(decoded->rgba[2].a == 128);
                CHECK(decoded->rgba[3].r == 255);
                CHECK(decoded->rgba[3].g == 255);
                CHECK(decoded->rgba[3].b == 255);
                CHECK(decoded->rgba[3].a == 0);
            }
        }
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("Image: write_png rejects inconsistent pixel buffers") {
    Image image;
    image.width = 2;
    image.height = 2;
    image.rgba = {Rgba{255, 0, 0, 255}};
    CHECK_FALSE(write_png("unused.png", image));
}
