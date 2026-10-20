#include <doctest/doctest.h>
#include "gl/assets/cp1251.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace {
std::string from_cp1251(std::string_view bytes) {
    return gl::assets::cp1251_to_utf8(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
}
}

TEST_CASE("cp1251: ASCII passes through") {
    const std::uint8_t s[] = {'H','e','l','l','o'};
    CHECK(gl::assets::cp1251_to_utf8(s) == "Hello");
}

TEST_CASE("cp1251: Cyrillic word") {
    const std::uint8_t s[] = {0xCF,0xF0,0xE8,0xE2,0xE5,0xF2}; // Привет
    CHECK(gl::assets::cp1251_to_utf8(s) == "Привет");
}

TEST_CASE("cp1251: special characters") {
    const std::uint8_t s[] = {0xA8,0xB8,0x88,0xB9}; // Ё ё € №
    CHECK(gl::assets::cp1251_to_utf8(s) == "Ёё€№");
}

TEST_CASE("paths normalize case and separators") {
    CHECK(gl::assets::normalize_virtual_path("Scripts\\DIALOGS\\Foo.CS") == "scripts/dialogs/foo.cs");
    CHECK(gl::assets::normalize_virtual_path("/A//B/") == "a/b");
    CHECK(gl::assets::normalize_virtual_path("ПРИВЕТ") == "привет");
}

TEST_CASE("paths normalize UTF-8 Cyrillic") {
    CHECK(gl::assets::normalize_virtual_path("ЁЖ") == "ёж");
    CHECK(gl::assets::normalize_virtual_path("привет") == "привет");
}

TEST_CASE("paths keep valid UTF-8 non-Cyrillic unchanged") {
    CHECK(gl::assets::normalize_virtual_path("caf\xC3\xA9") == "caf\xC3\xA9");
    CHECK(gl::assets::normalize_virtual_path("\xE4\xBD\xA0\xE5\xA5\xBD") ==
          "\xE4\xBD\xA0\xE5\xA5\xBD");
}

TEST_CASE("paths fall back to cp1251 for non-UTF-8 input") {
    const std::string pri_upper("\xCF\xF0\xE8\xE2\xE5\xF2", 6);
    const std::string pri_lower("\xEF\xF0\xE8\xE2\xE5\xF2", 6);
    CHECK(gl::assets::normalize_virtual_path(pri_upper) == pri_lower);

    const std::string sestry_upper("\xD1\xB8\xF1\xF2\xF0\xFB", 6);
    const std::string sestry_lower("\xF1\xB8\xF1\xF2\xF0\xFB", 6);
    CHECK(gl::assets::normalize_virtual_path(sestry_upper) == sestry_lower);
}

TEST_CASE("cp1251: UTF-8 to cp1251 round-trip") {
    const std::string utf8 = "Привет, Ёё€№!";
    const std::string bytes = gl::assets::utf8_to_cp1251(utf8);
    CHECK(from_cp1251(bytes) == utf8);
}

TEST_CASE("cp1251: UTF-8 ASCII passes through") {
    CHECK(gl::assets::utf8_to_cp1251("Hello, world!") == "Hello, world!");
}

TEST_CASE("cp1251: unmappable UTF-8 becomes question mark") {
    CHECK(gl::assets::utf8_to_cp1251("\xF0\x9F\x98\x80") == "?");
    CHECK(gl::assets::utf8_to_cp1251("caf\xC3\xA9") == "caf?");
    CHECK(gl::assets::utf8_to_cp1251("\xFF") == "?");
}

TEST_CASE("unsafe paths are rejected") {
    CHECK_FALSE(gl::assets::is_safe_relative_path("../x"));
    CHECK_FALSE(gl::assets::is_safe_relative_path("..\\x"));
    CHECK_FALSE(gl::assets::is_safe_relative_path("a/../../x"));
    CHECK_FALSE(gl::assets::is_safe_relative_path("/abs"));
    CHECK_FALSE(gl::assets::is_safe_relative_path("\\\\srv\\x"));
    CHECK_FALSE(gl::assets::is_safe_relative_path("C:/x"));
    CHECK_FALSE(gl::assets::is_safe_relative_path(""));
    CHECK(gl::assets::is_safe_relative_path("scripts/dialogs/x.cs"));
}
