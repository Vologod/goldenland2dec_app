#include <doctest/doctest.h>
#include "gl/assets/csx.hpp"
#include <string>

using namespace gl::assets;

namespace {
void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}
Bytes make_csx_with_transparent(Bytes rows, std::vector<std::uint32_t> offsets,
                                std::uint32_t width, std::uint32_t height,
                                std::vector<std::uint32_t> palette,
                                std::uint32_t transparent_bgr) {
    Bytes b;
    put_u32(b, static_cast<std::uint32_t>(palette.size()));
    put_u32(b, transparent_bgr);
    for (auto c : palette) put_u32(b, c);
    put_u32(b, width);
    put_u32(b, height);
    for (auto o : offsets) put_u32(b, o);
    b.insert(b.end(), rows.begin(), rows.end());
    return b;
}
Bytes make_csx(Bytes rows, std::vector<std::uint32_t> offsets, std::uint32_t width,
               std::uint32_t height, std::vector<std::uint32_t> palette) {
    return make_csx_with_transparent(rows, offsets, width, height, palette, palette[1]);
}
}

TEST_CASE("CSX: decodes literals, repeats and transparency") {
    // Palette: black, green (transparent), red in BGR.
    std::vector<std::uint32_t> palette = {0x000000, 0x00FF00, 0xFF0000};
    // Row 0: idx0, idx2, run idx1 x2. Row 1: transparent x4.
    Bytes row0 = {0x00, 0x02, 0x6A, 0x01, 0x02};
    Bytes row1 = {0x6C, 0x04};
    Bytes rows = row0; rows.insert(rows.end(), row1.begin(), row1.end());
    std::vector<std::uint32_t> offsets = {0, static_cast<std::uint32_t>(row0.size()),
                                          static_cast<std::uint32_t>(rows.size())};
    Bytes data = make_csx(rows, offsets, 4, 2, palette);
    auto img = decode_csx(data);
    REQUIRE(img.has_value());
    CHECK(img->width == 4);
    CHECK(img->height == 2);
    CHECK(img->transparent_index == 1);
    CHECK(img->indices == Bytes{0, 2, 1, 1, 1, 1, 1, 1});
    CHECK(img->rgba[0].r == 0x00);
    CHECK(img->rgba[0].a == 0xFF);
    CHECK(img->rgba[1].r == 0xFF); // red
    CHECK(img->rgba[2].a == 0x00); // transparent
    CHECK(img->rgba[4].a == 0x00);
}

TEST_CASE("CSX: rejects a row with the wrong pixel count") {
    std::vector<std::uint32_t> palette = {0x000000, 0x00FF00};
    Bytes rows = {0x00, 0x00};
    std::vector<std::uint32_t> offsets = {0, 2};
    Bytes data = make_csx(rows, offsets, 4, 1, palette);
    auto img = decode_csx(data);
    REQUIRE_FALSE(img.has_value());
    CHECK(img.error().code == gl::ErrorCode::format);
}

TEST_CASE("CSX: literal escape 0x6B for special palette indices") {
    std::vector<std::uint32_t> palette(256, 0x000000);
    palette[0x69] = 0xFF0000; // red in BGR
    palette[1] = 0x00FF00;    // transparent
    Bytes rows = {0x6B, 0x69};
    std::vector<std::uint32_t> offsets = {0, 2};
    Bytes data = make_csx(rows, offsets, 1, 1, palette);
    auto img = decode_csx(data);
    REQUIRE(img.has_value());
    CHECK(img->indices[0] == 0x69);
    CHECK(img->rgba[0].r == 0xFF);
}

TEST_CASE("CSX: 0x69 decodes a transparent pixel") {
    std::vector<std::uint32_t> palette = {0x000000, 0x00FF00};
    Bytes rows = {0x69};
    std::vector<std::uint32_t> offsets = {0, 1};
    Bytes data = make_csx(rows, offsets, 1, 1, palette);
    auto img = decode_csx(data);
    REQUIRE(img.has_value());
    CHECK(img->transparent_index == 1);
    CHECK(img->indices == Bytes{1});
    CHECK(img->rgba[0].a == 0x00);
}

TEST_CASE("CSX: fallback transparent index outside the palette is accepted") {
    std::vector<std::uint32_t> palette = {0x000000, 0x00FF00};
    Bytes rows = {0x69};
    std::vector<std::uint32_t> offsets = {0, 1};
    Bytes data = make_csx_with_transparent(rows, offsets, 1, 1, palette, 0x123456);
    auto img = decode_csx(data);
    REQUIRE(img.has_value());
    CHECK(img->transparent_index == 0x56);
    CHECK(img->indices == Bytes{0x56});
    CHECK(img->rgba[0].r == 0x00);
    CHECK(img->rgba[0].g == 0x00);
    CHECK(img->rgba[0].b == 0x00);
    CHECK(img->rgba[0].a == 0x00);
}

TEST_CASE("CSX: non-transparent colors outside the palette are rejected") {
    std::vector<std::uint32_t> palette = {0x000000, 0x00FF00};
    Bytes rows = {0x02};
    std::vector<std::uint32_t> offsets = {0, 1};
    Bytes data = make_csx(rows, offsets, 1, 1, palette);
    auto img = decode_csx(data);
    REQUIRE_FALSE(img.has_value());
    CHECK(img.error().code == gl::ErrorCode::format);
}

TEST_CASE("CSX: duplicate palette entries resolve to the highest index") {
    std::vector<std::uint32_t> palette = {0x000000, 0x112233, 0x112233};
    Bytes rows = {0x69};
    std::vector<std::uint32_t> offsets = {0, 1};
    Bytes data = make_csx_with_transparent(rows, offsets, 1, 1, palette, 0x112233);
    auto img = decode_csx(data);
    REQUIRE(img.has_value());
    CHECK(img->transparent_index == 2);
    CHECK(img->rgba[0].a == 0x00);
}

TEST_CASE("CSX: rejects decreasing and out-of-range row offsets") {
    std::vector<std::uint32_t> palette = {0x000000, 0x00FF00};
    SUBCASE("decreasing") {
        Bytes rows = {0x00};
        std::vector<std::uint32_t> offsets = {1, 0};
        Bytes data = make_csx(rows, offsets, 1, 1, palette);
        auto img = decode_csx(data);
        REQUIRE_FALSE(img.has_value());
        CHECK(img.error().code == gl::ErrorCode::format);
    }
    SUBCASE("out of range") {
        Bytes rows = {0x00};
        std::vector<std::uint32_t> offsets = {0, 2};
        Bytes data = make_csx(rows, offsets, 1, 1, palette);
        auto img = decode_csx(data);
        REQUIRE_FALSE(img.has_value());
        CHECK(img.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("CSX: rejects truncated run operands") {
    std::vector<std::uint32_t> palette = {0x000000, 0x00FF00};
    SUBCASE("0x6A without color and count") {
        Bytes rows = {0x6A};
        std::vector<std::uint32_t> offsets = {0, 1};
        Bytes data = make_csx(rows, offsets, 1, 1, palette);
        auto img = decode_csx(data);
        REQUIRE_FALSE(img.has_value());
        CHECK(img.error().code == gl::ErrorCode::format);
    }
    SUBCASE("0x6B without literal") {
        Bytes rows = {0x6B};
        std::vector<std::uint32_t> offsets = {0, 1};
        Bytes data = make_csx(rows, offsets, 1, 1, palette);
        auto img = decode_csx(data);
        REQUIRE_FALSE(img.has_value());
        CHECK(img.error().code == gl::ErrorCode::format);
    }
    SUBCASE("0x6C without count") {
        Bytes rows = {0x6C};
        std::vector<std::uint32_t> offsets = {0, 1};
        Bytes data = make_csx(rows, offsets, 1, 1, palette);
        auto img = decode_csx(data);
        REQUIRE_FALSE(img.has_value());
        CHECK(img.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("CSX: rejects images above the total pixel limit") {
    std::vector<std::uint32_t> palette = {0x000000, 0x00FF00};
    Bytes data = make_csx({}, {}, 2048, 8193, palette);
    auto img = decode_csx(data);
    REQUIRE_FALSE(img.has_value());
    CHECK(img.error().code == gl::ErrorCode::format);
    CHECK(img.error().message.find("pixel") != std::string::npos);
}
