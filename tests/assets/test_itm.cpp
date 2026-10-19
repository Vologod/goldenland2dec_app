#include <doctest/doctest.h>
#include "gl/assets/bytes.hpp"
#include "gl/assets/itm.hpp"

using namespace gl::assets;

namespace {
void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}
void put_u64(Bytes& b, std::uint64_t v) { put_u32(b, static_cast<std::uint32_t>(v)); put_u32(b, static_cast<std::uint32_t>(v >> 32)); }
void put_str(Bytes& b, const std::string& s) { put_u32(b, static_cast<std::uint32_t>(s.size())); b.insert(b.end(), s.begin(), s.end()); }
Bytes base(std::uint32_t type, std::uint32_t flags) {
    Bytes b;
    put_u32(b, 4);          // magic
    put_u32(b, 100);        // description id
    put_u32(b, type);
    put_u32(b, flags);
    put_u32(b, 1);          // max stack
    put_str(b, "");         // puppet
    put_str(b, "");         // icon
    put_u64(b, 0);          // unknown
    put_u32(b, 10);         // price
    put_u32(b, 20);         // weight
    for (int i = 0; i < 6; ++i) put_u32(b, 0); // unknown dwords
    put_u32(b, 0xFFFFFFFF); // material
    return b;
}
}

TEST_CASE("ITM: parses a sword with damage and effects") {
    Bytes b = base(0, 0);
    put_u32(b, 0); put_u32(b, 0); put_u32(b, 0); put_u32(b, 0);
    put_u32(b, 0); put_u32(b, 0); put_u32(b, 0);
    for (int i = 0; i < 6; ++i) put_u32(b, static_cast<std::uint32_t>(i + 1));
    put_u32(b, 0); // effects count
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    CHECK(item->type == ItemType::sword);
    CHECK(item->has_damage);
    CHECK(item->damage[0] == 1);
    CHECK(item->damage[5] == 6);
    CHECK(item->effects.empty());
}

TEST_CASE("ITM: parses a potion with healing") {
    Bytes b = base(19, 0);
    put_u32(b, 0); put_u32(b, 0); put_u32(b, 0); put_u32(b, 0);
    put_u32(b, 0); put_u32(b, 0); put_u32(b, 0);
    put_u32(b, 50); put_u32(b, 25); // health, mana
    put_u32(b, 0); // effects
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    CHECK(item->type == ItemType::potion);
    CHECK(item->has_healing);
    CHECK(item->healing_health == 50);
    CHECK(item->healing_mana == 25);
}

TEST_CASE("ITM: rejects a bad magic") {
    Bytes b = base(0, 0);
    b[0] = 0;
    auto item = parse_item(b);
    REQUIRE_FALSE(item.has_value());
    CHECK(item.error().code == gl::ErrorCode::format);
}
