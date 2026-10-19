#include <doctest/doctest.h>
#include "gl/assets/bytes.hpp"
#include "gl/assets/sdb.hpp"

using namespace gl::assets;

namespace {
void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}
Bytes entry(std::uint32_t id, const std::string& text) {
    Bytes b; put_u32(b, id); put_u32(b, static_cast<std::uint32_t>(text.size()));
    b.insert(b.end(), text.begin(), text.end()); return b;
}
}

TEST_CASE("SDB: parses plain files") {
    Bytes data = {'S','D','B',' '};
    Bytes e = entry(7, "Hello");
    data.insert(data.end(), e.begin(), e.end());
    auto sdb = Sdb::parse(data);
    REQUIRE(sdb.has_value());
    CHECK(sdb->had_signature());
    REQUIRE(sdb->find(7) != nullptr);
    CHECK(*sdb->find(7) == "Hello");
}

TEST_CASE("SDB: XOR 0xAA when signature is absent") {
    const std::uint8_t raw[] = {0xCF,0xF0,0xE8,0xE2,0xE5,0xF2}; // Привет
    Bytes text;
    for (std::uint8_t b : raw) text.push_back(static_cast<std::uint8_t>(b ^ 0xAA));
    Bytes data = entry(3, std::string(text.begin(), text.end()));
    auto sdb = Sdb::parse(data);
    REQUIRE(sdb.has_value());
    CHECK_FALSE(sdb->had_signature());
    REQUIRE(sdb->find(3) != nullptr);
    CHECK(*sdb->find(3) == "Привет");
}

TEST_CASE("SDB: rejects truncated entries") {
    Bytes data = {'S','D','B',' ', 1, 0, 0, 0, 10, 0, 0, 0};
    auto sdb = Sdb::parse(data);
    REQUIRE_FALSE(sdb.has_value());
    CHECK(sdb.error().code == gl::ErrorCode::format);
}
