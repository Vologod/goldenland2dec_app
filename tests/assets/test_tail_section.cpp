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

TEST_CASE("encode_tail_section frames RGINF, PNAME and ENTS") {
    gl::assets::SaveRginf rginf;
    rginf.latch = 7;
    for (std::uint32_t index = 0; index < 8; ++index)
        rginf.datetime[index] = 100 + index;
    const std::array<std::string, 2> names{"A", "BC"};
    const std::array<gl::assets::SaveEntity, 2> entities{
        gl::assets::SaveEntity{2, {0xDE, 0xAD}},
        gl::assets::SaveEntity{8, {}},
    };

    const auto encoded = gl::assets::encode_tail_section(rginf, names, entities);
    REQUIRE(encoded.has_value());
    const auto &data = encoded.value();
    const std::uint32_t pname_size = 4 + 1 + 4 + 2 + 4;
    const std::size_t rginf_off = 12 + pname_size;
    const std::size_t ents = rginf_off + 12 + 36;
    REQUIRE(data.size() > ents + 12 + 4);

    CHECK(std::string(data.begin(), data.begin() + 8) == "BT_PNAME");
    CHECK(u32_at(data, 8) == pname_size);
    CHECK(u32_at(data, 12) == 1);
    CHECK(data[16] == 'A');
    CHECK(u32_at(data, 17) == 2);

    CHECK(std::string(data.begin() + rginf_off, data.begin() + rginf_off + 8) == "BT_RGINF");
    CHECK(u32_at(data, rginf_off + 8) == 36);
    CHECK(u32_at(data, rginf_off + 12) == 7);
    CHECK(u32_at(data, rginf_off + 16) == 100);
    CHECK(u32_at(data, rginf_off + 44) == 107);

    CHECK(std::string(data.begin() + ents, data.begin() + ents + 8) == "BT_ENTS ");
    const std::uint32_t ents_size = u32_at(data, ents + 8);
    CHECK(ents_size == 4 + 2 + 4 + 0 + 4);
    CHECK(u32_at(data, ents + 12) == 2);
    CHECK(data[ents + 16] == 0xDE);
    CHECK(data[ents + 17] == 0xAD);
    CHECK(u32_at(data, ents + 18) == 8);
    CHECK(u32_at(data, ents + 22) == 1);
}

TEST_CASE("encode_tail_section writes an empty PNAME terminator") {
    gl::assets::SaveRginf rginf;
    const std::vector<gl::assets::SaveEntity> entities;
    const auto encoded = gl::assets::encode_tail_section(rginf, {}, entities);
    REQUIRE(encoded.has_value());
    const auto &data = encoded.value();
    REQUIRE(data.size() >= 16);
    CHECK(u32_at(data, 8) == 4);
    CHECK(u32_at(data, 12) == 0);
}

TEST_CASE("encode_tail_section rejects a NUL in a name") {
    gl::assets::SaveRginf rginf;
    const std::string name_with_nul("A\0B", 3);
    const std::vector<std::string> names{name_with_nul};
    const std::vector<gl::assets::SaveEntity> entities;
    const auto encoded = gl::assets::encode_tail_section(rginf, names, entities);
    REQUIRE_FALSE(encoded.has_value());
    CHECK(encoded.error().code == gl::ErrorCode::format);
}
