#include "gl/script/globals.hpp"
#include <doctest/doctest.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using gl::script::parse_globals;
using gl::script::ScriptGlobal;

namespace {
std::vector<std::uint8_t> bytes_of(std::string_view text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}
} // namespace

TEST_CASE("globals: parses int, DWORD, float and string") {
    auto data = bytes_of("int A 1\n"
                         "DWORD B 4294967295\n"
                         "float C 0.5\n"
                         "string S \"hello world\"\n");
    auto parsed = parse_globals(data);
    REQUIRE(parsed.has_value());
    const auto& globals = parsed.value();
    REQUIRE(globals.size() == 4);

    CHECK(globals[0].type == "int");
    CHECK(globals[0].name == "A");
    REQUIRE(std::holds_alternative<std::int32_t>(globals[0].value));
    CHECK(std::get<std::int32_t>(globals[0].value) == 1);

    CHECK(globals[1].type == "DWORD");
    CHECK(globals[1].name == "B");
    REQUIRE(std::holds_alternative<std::uint32_t>(globals[1].value));
    CHECK(std::get<std::uint32_t>(globals[1].value) == 4294967295u);

    CHECK(globals[2].type == "float");
    CHECK(globals[2].name == "C");
    REQUIRE(std::holds_alternative<double>(globals[2].value));
    CHECK(std::get<double>(globals[2].value) == doctest::Approx(0.5));

    CHECK(globals[3].type == "string");
    CHECK(globals[3].name == "S");
    REQUIRE(std::holds_alternative<std::string>(globals[3].value));
    CHECK(std::get<std::string>(globals[3].value) == "hello world");
}

TEST_CASE("globals: skips comments, blank lines and malformed lines") {
    auto data = bytes_of("// header\r\n"
                         "\r\n"
                         "   // indented comment\r\n"
                         "int A 5 // inline comment\r\n"
                         "int Missing\r\n"
                         "float\r\n"
                         "nonsense\r\n"
                         "bogus B 3\r\n"
                         "DWORD C 7\r\n");
    auto parsed = parse_globals(data);
    REQUIRE(parsed.has_value());
    const auto& globals = parsed.value();
    REQUIRE(globals.size() == 2);
    CHECK(globals[0].name == "A");
    CHECK(std::get<std::int32_t>(globals[0].value) == 5);
    CHECK(globals[1].name == "C");
    CHECK(std::get<std::uint32_t>(globals[1].value) == 7);
}

TEST_CASE("globals: decodes cp1251 strings and names to UTF-8") {
    std::vector<std::uint8_t> data;
    const std::string_view prefix = "string ";
    data.insert(data.end(), prefix.begin(), prefix.end());
    data.push_back(0xCF); // П
    data.push_back(0xF0); // р
    data.push_back(0xE8); // и
    data.push_back(0xE2); // в
    data.push_back(0xE5); // е
    data.push_back(0xF2); // т
    data.push_back(' ');
    data.push_back('"');
    data.push_back(0xCF);
    data.push_back(0xF0);
    data.push_back(0xE8);
    data.push_back(0xE2);
    data.push_back(0xE5);
    data.push_back(0xF2);
    data.push_back('"');
    data.push_back('\n');

    auto parsed = parse_globals(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value().size() == 1);
    CHECK(parsed.value()[0].name == "Привет");
    CHECK(std::get<std::string>(parsed.value()[0].value) == "Привет");
}

TEST_CASE("globals: skips a UTF-8 byte order mark") {
    std::vector<std::uint8_t> data{0xEF, 0xBB, 0xBF};
    const auto body = bytes_of("int A 1\n");
    data.insert(data.end(), body.begin(), body.end());

    auto parsed = parse_globals(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value().size() == 1);
    CHECK(parsed.value()[0].name == "A");
    CHECK(std::get<std::int32_t>(parsed.value()[0].value) == 1);
}

TEST_CASE("globals: accepts an unterminated quoted string and warns") {
    auto data = bytes_of("string S \"hello\n"
                         "int A 1\n");
    auto parsed = parse_globals(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value().size() == 2);
    CHECK(parsed.value()[0].name == "S");
    REQUIRE(std::holds_alternative<std::string>(parsed.value()[0].value));
    CHECK(std::get<std::string>(parsed.value()[0].value) == "hello");
    CHECK(parsed.value()[1].name == "A");
}

TEST_CASE("globals: empty input yields empty list") {
    auto parsed = parse_globals(std::span<const std::uint8_t>{});
    REQUIRE(parsed.has_value());
    CHECK(parsed.value().empty());
}
