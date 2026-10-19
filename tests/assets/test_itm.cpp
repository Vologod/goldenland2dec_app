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
void put_requirements(Bytes& b) {
    for (int i = 0; i < 7; ++i) put_u32(b, 0);
}
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

TEST_CASE("ITM: parses a sword with damage") {
    Bytes b = base(0, 0);
    put_requirements(b);
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
    put_requirements(b);
    put_u32(b, 50); put_u32(b, 25); // health, mana
    put_u32(b, 0); // effects
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    CHECK(item->type == ItemType::potion);
    CHECK(item->has_healing);
    CHECK(item->healing_health == 50);
    CHECK(item->healing_mana == 25);
}

TEST_CASE("ITM: reads a shell string when flag bit 4 is set") {
    Bytes b = base(0, 1u << 4);
    put_str(b, "puppets/shell.bmp");
    put_requirements(b);
    for (int i = 0; i < 6; ++i) put_u32(b, 0);
    put_u32(b, 0);
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    CHECK(item->shell == "puppets/shell.bmp");
}

TEST_CASE("ITM: shell is empty when flag bit 4 is clear") {
    Bytes b = base(0, 0);
    put_requirements(b);
    for (int i = 0; i < 6; ++i) put_u32(b, 0);
    put_u32(b, 0);
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    CHECK(item->shell.empty());
}

TEST_CASE("ITM: staff has damage, spell and charge blocks") {
    Bytes b = base(6, 0);
    put_requirements(b);
    for (int i = 0; i < 6; ++i) put_u32(b, static_cast<std::uint32_t>(10 + i));
    put_u32(b, 321); // spell id
    put_u32(b, 0);   // reserved
    put_u32(b, 7);   // charge
    put_u32(b, 0);   // effects count
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    CHECK(item->has_damage);
    CHECK(item->has_spell);
    CHECK(item->has_charge);
    CHECK(item->spell_id == 321);
    CHECK(item->charge == 7);
    CHECK(item->damage[0] == 10);
}

TEST_CASE("ITM: book and scroll carry spell blocks") {
    for (std::uint32_t type : {21u, 22u}) {
        Bytes b = base(type, 0);
        put_requirements(b);
        put_u32(b, 400 + type);
        put_u32(b, 0);
        put_u32(b, 0);
        auto item = parse_item(b);
        REQUIRE(item.has_value());
        CHECK(item->type == static_cast<ItemType>(type));
        CHECK(item->has_spell);
        CHECK_FALSE(item->has_damage);
        CHECK_FALSE(item->has_charge);
        CHECK(item->spell_id == 400 + type);
    }
}

TEST_CASE("ITM: recipe block has three components") {
    Bytes b = base(26, 0);
    put_requirements(b);
    put_u32(b, 11); put_u32(b, 22); put_u32(b, 33);
    put_u32(b, 0);
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    CHECK(item->has_recipe);
    CHECK(item->recipe[0] == 11);
    CHECK(item->recipe[1] == 22);
    CHECK(item->recipe[2] == 33);
}

TEST_CASE("ITM: ammo types carry a damage_type block") {
    for (std::uint32_t type : {8u, 9u, 10u}) {
        Bytes b = base(type, 0);
        put_requirements(b);
        put_u32(b, 5 + type);
        put_u32(b, 0);
        auto item = parse_item(b);
        REQUIRE(item.has_value());
        CHECK(item->has_damage_type);
        CHECK_FALSE(item->has_damage);
        CHECK(item->damage_type == 5 + type);
    }
}

TEST_CASE("ITM: parses non-empty effects with timings") {
    Bytes b = base(1, 0);
    put_requirements(b);
    for (int i = 0; i < 6; ++i) put_u32(b, 0);
    put_u32(b, 2);
    put_u32(b, 1); put_u32(b, 2); put_u32(b, 3);
    b.push_back(2); b.push_back(5); b.push_back(1); b.push_back(7);
    put_u32(b, 4); put_u32(b, 5); put_u32(b, 6);
    b.push_back(0); b.push_back(9); b.push_back(0); b.push_back(3);
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    REQUIRE(item->effects.size() == 2);
    CHECK(item->effects[0].type == 1);
    CHECK(item->effects[0].amount == 2);
    CHECK(item->effects[0].flags == 3);
    CHECK(item->effects[0].duration.hi == 2);
    CHECK(item->effects[0].duration.lo == 5);
    CHECK(item->effects[0].duration.turns() == 125);
    CHECK(item->effects[0].delay.turns() == 37);
    CHECK(item->effects[1].type == 4);
    CHECK(item->effects[1].duration.turns() == 9);
    CHECK(item->effects[1].delay.turns() == 3);
}

TEST_CASE("ITM: rejects truncation in major blocks") {
    SUBCASE("shell") {
        Bytes b = base(0, 1u << 4);
        auto item = parse_item(b);
        REQUIRE_FALSE(item.has_value());
        CHECK(item.error().code == gl::ErrorCode::format);
    }
    SUBCASE("requirements") {
        Bytes b = base(0, 0);
        auto item = parse_item(b);
        REQUIRE_FALSE(item.has_value());
        CHECK(item.error().code == gl::ErrorCode::format);
    }
    SUBCASE("damage") {
        Bytes b = base(0, 0);
        put_requirements(b);
        for (int i = 0; i < 5; ++i) put_u32(b, 0);
        auto item = parse_item(b);
        REQUIRE_FALSE(item.has_value());
        CHECK(item.error().code == gl::ErrorCode::format);
    }
    SUBCASE("spell") {
        Bytes b = base(21, 0);
        put_requirements(b);
        put_u32(b, 123);
        auto item = parse_item(b);
        REQUIRE_FALSE(item.has_value());
        CHECK(item.error().code == gl::ErrorCode::format);
    }
    SUBCASE("effects") {
        Bytes b = base(21, 0);
        put_requirements(b);
        put_u32(b, 123); put_u32(b, 0);
        put_u32(b, 1);
        auto item = parse_item(b);
        REQUIRE_FALSE(item.has_value());
        CHECK(item.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("ITM: rejects an unreasonable effects count") {
    Bytes b = base(0, 0);
    put_requirements(b);
    for (int i = 0; i < 6; ++i) put_u32(b, 0);
    put_u32(b, 1001);
    auto item = parse_item(b);
    REQUIRE_FALSE(item.has_value());
    CHECK(item.error().code == gl::ErrorCode::format);
}

TEST_CASE("ITM: unknown item types pass through without conditional blocks") {
    Bytes b = base(99, 0);
    put_requirements(b);
    put_u32(b, 0);
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    CHECK(item->type == static_cast<ItemType>(99));
    CHECK_FALSE(item->has_damage);
    CHECK_FALSE(item->has_spell);
    CHECK_FALSE(item->has_charge);
    CHECK_FALSE(item->has_healing);
    CHECK_FALSE(item->has_recipe);
    CHECK_FALSE(item->has_damage_type);
    CHECK(item->effects.empty());
}

TEST_CASE("ITM: rejects trailing bytes") {
    Bytes b = base(99, 0);
    put_requirements(b);
    put_u32(b, 0);
    b.push_back(0x7F);
    auto item = parse_item(b);
    REQUIRE_FALSE(item.has_value());
    CHECK(item.error().code == gl::ErrorCode::format);
}

TEST_CASE("ITM: rejects a bad magic") {
    Bytes b = base(0, 0);
    b[0] = 0;
    auto item = parse_item(b);
    REQUIRE_FALSE(item.has_value());
    CHECK(item.error().code == gl::ErrorCode::format);
}
