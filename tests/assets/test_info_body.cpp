#include "gl/assets/save_payload.hpp"

#include <array>
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {
std::uint32_t u32_at(const std::vector<std::uint8_t> &data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}
} // namespace

TEST_CASE("encode_info_body writes the BT_INFO order") {
    gl::assets::SaveInfo info;
    info.session = "S";
    info.field_218 = 7;
    info.time.fill(0xAB);
    info.description = "D";
    info.level_id = "L1_1";
    info.party_level = 9;
    info.field_26c8 = {1, 2, 3, 4, 5, 6, 7, 8};

    const auto encoded = gl::assets::encode_info_body(info);
    REQUIRE(encoded.has_value());
    const auto &data = encoded.value();
    std::size_t cursor = 0;
    REQUIRE(u32_at(data, cursor) == 1);
    CHECK(data[cursor + 4] == 'S');
    cursor += 5;
    CHECK(u32_at(data, cursor) == 7);
    cursor += 4;
    CHECK(data[cursor] == 0xAB);
    cursor += 0x20;
    REQUIRE(u32_at(data, cursor) == 1);
    CHECK(data[cursor + 4] == 'D');
    cursor += 5;
    REQUIRE(u32_at(data, cursor) == 4);
    CHECK(std::string(data.begin() + static_cast<std::ptrdiff_t>(cursor + 4),
                      data.begin() + static_cast<std::ptrdiff_t>(cursor + 8)) == "L1_1");
    cursor += 8;
    CHECK(u32_at(data, cursor) == 9);
    cursor += 4;
    for (std::uint32_t index = 0; index < 8; ++index)
        CHECK(data[cursor + index] == index + 1);
    CHECK(cursor + 8 == data.size());
}

TEST_CASE("encode_info_body rejects an empty level id") {
    gl::assets::SaveInfo info;
    const auto encoded = gl::assets::encode_info_body(info);
    REQUIRE_FALSE(encoded.has_value());
    CHECK(encoded.error().code == gl::ErrorCode::format);
}
