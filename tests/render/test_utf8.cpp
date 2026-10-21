#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "gl/render/utf8.hpp"

#include <doctest.h>

#include <string_view>
#include <vector>

namespace {

bool all_replacement(const std::vector<char32_t>& codepoints) {
    for (const char32_t codepoint : codepoints) {
        if (codepoint != 0xFFFD) {
            return false;
        }
    }
    return !codepoints.empty();
}

} // namespace

TEST_CASE("utf8_decode decodes ASCII") {
    const std::vector<char32_t> codepoints = gl::render::utf8_decode("Hello");
    REQUIRE(codepoints.size() == 5);
    CHECK(codepoints[0] == U'H');
    CHECK(codepoints[1] == U'e');
    CHECK(codepoints[4] == U'o');
}

TEST_CASE("utf8_decode decodes two-byte Cyrillic") {
    const std::vector<char32_t> codepoints = gl::render::utf8_decode("Привет");
    REQUIRE(codepoints.size() == 6);
    CHECK(codepoints[0] == U'\u041F');
    CHECK(codepoints[1] == U'\u0440');
    CHECK(codepoints[5] == U'\u0442');
}

TEST_CASE("utf8_decode decodes three-byte and four-byte sequences") {
    const std::vector<char32_t> three = gl::render::utf8_decode("\xE2\x82\xAC");
    REQUIRE(three.size() == 1);
    CHECK(three[0] == U'\u20AC');

    const std::vector<char32_t> four = gl::render::utf8_decode("\xF0\x9F\x98\x80");
    REQUIRE(four.size() == 1);
    CHECK(four[0] == U'\U0001F600');
}

TEST_CASE("utf8_decode keeps newlines and empty input") {
    const std::vector<char32_t> codepoints = gl::render::utf8_decode("a\nb");
    REQUIRE(codepoints.size() == 3);
    CHECK(codepoints[1] == U'\n');
    CHECK(gl::render::utf8_decode("").empty());
}

TEST_CASE("utf8_decode replaces invalid sequences") {
    CHECK(all_replacement(gl::render::utf8_decode("\xFF")));
    CHECK(all_replacement(gl::render::utf8_decode("\x80")));
    CHECK(all_replacement(gl::render::utf8_decode("\xC0\xAF")));
    CHECK(all_replacement(gl::render::utf8_decode("\xE2\x82")));
    CHECK(all_replacement(gl::render::utf8_decode("\xED\xA0\x80")));
    CHECK(all_replacement(gl::render::utf8_decode("\xF5\x80\x80\x80")));
}
