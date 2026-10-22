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
                             "\t{\r\n"
                             "\t\tposition 1 2\r\n"
                             "\t}\r\n"
                             "}\r\n";
    auto result = parse_sef(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->pack == "l1_1");
    CHECK_FALSE(result->internal_location);
    CHECK(result->exit_to_global_map);
    CHECK(result->weather == 2);
}

TEST_CASE("SEF: parses persons with optional fields") {
    const std::string text =
        "pack: \"l1_1\"\r\n"
        "persons:\r\n"
        "{\r\n"
        "\tname:\t\"L1_1.First\"\r\n"
        "\t{\r\n"
        "\t\tposition\t133 12\r\n"
        "\t\tliterary_name\t1 // display name\r\n"
        "\t\tdirection\t\"DOWN_RIGHT\"\r\n"
        "\t\troute_type\t\"STAY\"\r\n"
        "\t\troute\t\"track1\"\r\n"
        "\t\tradius\t15\r\n"
        "\t\tdelay_min\t0\r\n"
        "\t\tdelay_max\t9000\r\n"
        "\t\ttribe\t\"\xCF\xF0\xE8\xE2\xE5\xF2\"\r\n"
        "\t\tscr_dialog\t\"l1_1\\l1_1.first.d1.age\"\r\n"
        "\t\tscr_inv\t\"p387\"\r\n"
        "\t}\r\n"
        "\tname: \"Second\"\r\n"
        "\t{\r\n"
        "\t\tposition\t1 2\r\n"
        "\t}\r\n"
        "}\r\n"
        "points_entrance:\r\n"
        "{\r\n"
        "\tname: \"ignored\"\r\n"
        "\t{\r\n"
        "\t\tposition\t9 9\r\n"
        "\t}\r\n"
        "}\r\n";
    auto result = parse_sef(as_bytes(text));
    REQUIRE(result.has_value());
    REQUIRE(result->persons.size() == 2);

    const SefPerson& first = result->persons.at(0);
    CHECK(first.name == "L1_1.First");
    CHECK(first.x == 133);
    CHECK(first.y == 12);
    CHECK(first.direction == "DOWN_RIGHT");
    CHECK(first.route_type == "STAY");
    CHECK(first.route == "track1");
    CHECK(first.radius == 15);
    CHECK(first.delay_min == 0);
    CHECK(first.delay_max == 9000);
    CHECK(first.tribe == "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");
    CHECK(first.dialog == "l1_1\\l1_1.first.d1.age");
    CHECK(first.inventory == "p387");

    const SefPerson& second = result->persons.at(1);
    CHECK(second.name == "Second");
    CHECK(second.x == 1);
    CHECK(second.y == 2);
    CHECK(second.direction.empty());
    CHECK(second.route_type.empty());
    CHECK(second.route.empty());
    CHECK(second.radius == 0);
    CHECK(second.delay_min == 0);
    CHECK(second.delay_max == 0);
    CHECK(second.tribe.empty());
    CHECK(second.dialog.empty());
    CHECK(second.inventory.empty());
}

TEST_CASE("SEF: accepts one-line persons separated by semicolons") {
    const std::string text =
        "pack: \"l1_1\"\n"
        "persons: { name: \"A\"; { position 1 2; direction \"UP\"; route_type \"STAY\"; "
        "radius 3; delay_min 4; delay_max 5; tribe \"t\"; scr_dialog \"d\"; scr_inv \"i\" } "
        "name: \"B\" { position -1 0; direction \"LEFT\" } }\n";
    auto result = parse_sef(as_bytes(text));
    REQUIRE(result.has_value());
    REQUIRE(result->persons.size() == 2);
    CHECK(result->persons.at(0).name == "A");
    CHECK(result->persons.at(0).direction == "UP");
    CHECK(result->persons.at(0).radius == 3);
    CHECK(result->persons.at(0).delay_min == 4);
    CHECK(result->persons.at(0).delay_max == 5);
    CHECK(result->persons.at(0).tribe == "t");
    CHECK(result->persons.at(0).dialog == "d");
    CHECK(result->persons.at(0).inventory == "i");
    CHECK(result->persons.at(1).name == "B");
    CHECK(result->persons.at(1).x == -1);
    CHECK(result->persons.at(1).y == 0);
}

TEST_CASE("SEF: rejects a truncated persons block") {
    const std::string text = "pack: \"l1_1\"\n"
                             "persons: { name: \"A\" { position 1 } }\n";
    auto result = parse_sef(as_bytes(text));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
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

TEST_CASE("SEF: reports line numbers in errors") {
    const std::string text = "pack: \"l1_1\"\n"
                             "weather: rainy\n";
    auto result = parse_sef(as_bytes(text));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
    CHECK(result.error().message.find("line 2") != std::string::npos);
}

TEST_CASE("SEF: rejects a missing pack") {
    const std::string text = "internal_location: 1\n"
                             "weather: 2\n";
    auto result = parse_sef(as_bytes(text));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("SEF parses cell_groups") {
    const std::string text =
        "pack: \"L1_1\"\n"
        "cell_groups:\n{\n"
        "  name: \"track1\"\n  {\n"
        "    cell_00 161 127\n"
        "    cell_01 144 134\n"
        "  }\n"
        "  name: \"empty\"\n  {\n  }\n"
        "}\n";
    auto sef = gl::assets::parse_sef(as_bytes(text));
    REQUIRE(sef.has_value());
    REQUIRE(sef->cell_groups.size() == 2);
    CHECK(sef->cell_groups[0].name == "track1");
    REQUIRE(sef->cell_groups[0].cells.size() == 2);
    CHECK(sef->cell_groups[0].cells[0].first == 161);
    CHECK(sef->cell_groups[0].cells[0].second == 127);
    CHECK(sef->cell_groups[1].name == "empty");
    CHECK(sef->cell_groups[1].cells.empty());
}

TEST_CASE("SEF: rejects a cell_group with a non-numeric coordinate") {
    const std::string text =
        "pack: \"L1_1\"\n"
        "cell_groups:\n{\n"
        "  name: \"track1\"\n  {\n"
        "    cell_00 abc 127\n"
        "  }\n"
        "}\n";
    auto sef = gl::assets::parse_sef(as_bytes(text));
    REQUIRE_FALSE(sef.has_value());
    CHECK(sef.error().code == gl::ErrorCode::format);
    CHECK(sef.error().message.find("bad SEF cell coordinate") != std::string::npos);
}

TEST_CASE("SEF: rejects a cell_group cell missing its second coordinate") {
    const std::string text =
        "pack: \"L1_1\"\n"
        "cell_groups:\n{\n"
        "  name: \"track1\"\n  {\n"
        "    cell_00 161\n"
        "  }\n"
        "}\n";
    auto sef = gl::assets::parse_sef(as_bytes(text));
    REQUIRE_FALSE(sef.has_value());
    CHECK(sef.error().code == gl::ErrorCode::format);
    CHECK(sef.error().message.find("bad SEF cell coordinate") != std::string::npos);
}

TEST_CASE("SEF: rejects a negative cell_group coordinate") {
    const std::string text =
        "pack: \"L1_1\"\n"
        "cell_groups:\n{\n"
        "  name: \"track1\"\n  {\n"
        "    cell_00 -1 127\n"
        "  }\n"
        "}\n";
    auto sef = gl::assets::parse_sef(as_bytes(text));
    REQUIRE_FALSE(sef.has_value());
    CHECK(sef.error().code == gl::ErrorCode::format);
    CHECK(sef.error().message.find("out of range") != std::string::npos);
}

TEST_CASE("SEF: rejects a cell_group coordinate above 0xFFFF") {
    const std::string text =
        "pack: \"L1_1\"\n"
        "cell_groups:\n{\n"
        "  name: \"track1\"\n  {\n"
        "    cell_00 70000 127\n"
        "  }\n"
        "}\n";
    auto sef = gl::assets::parse_sef(as_bytes(text));
    REQUIRE_FALSE(sef.has_value());
    CHECK(sef.error().code == gl::ErrorCode::format);
    CHECK(sef.error().message.find("out of range") != std::string::npos);
}
