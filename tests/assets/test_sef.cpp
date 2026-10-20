#include "gl/assets/sef.hpp"
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <span>
#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
std::span<const std::uint8_t> as_bytes(std::string_view text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}
} // namespace

TEST_CASE("SEF: extracts top-level fields") {
    const std::string text = "pack: \"l1_1\"\n"
                             "internal_location: 1\n"
                             "exit_to_global_map: 1\n"
                             "weather: 3\n";
    auto result = parse_sef(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->pack == "l1_1");
    CHECK(result->internal_location);
    CHECK(result->exit_to_global_map);
    CHECK(result->weather == 3);
}

TEST_CASE("SEF: handles comments, tabs, CRLF and the real globalmap key") {
    const std::string text = "\t// header comment\r\n"
                             "\tpack:\t\"L1_1\"\t// current pack\r\n"
                             "internal_location:\t0\r\n"
                             "exit_to_globalmap:\t1\r\n"
                             "weather:\t2 // rainy\r\n"
                             "persons:\r\n"
                             "{\r\n"
                             "\tname: \"ignored\"\r\n"
                             "}\r\n";
    auto result = parse_sef(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->pack == "l1_1");
    CHECK_FALSE(result->internal_location);
    CHECK(result->exit_to_global_map);
    CHECK(result->weather == 2);
}

TEST_CASE("SEF: defaults optional fields when absent") {
    const std::string text = "pack: \"l2_2\"\n";
    auto result = parse_sef(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->pack == "l2_2");
    CHECK_FALSE(result->internal_location);
    CHECK_FALSE(result->exit_to_global_map);
    CHECK(result->weather == -1);
}

TEST_CASE("SEF: rejects a missing pack") {
    const std::string text = "internal_location: 1\n"
                             "weather: 2\n";
    auto result = parse_sef(as_bytes(text));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}
