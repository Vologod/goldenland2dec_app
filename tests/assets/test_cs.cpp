#include "gl/assets/bytes.hpp"
#include "gl/assets/cs.hpp"
#include <cstdint>
#include <cstring>
#include <doctest/doctest.h>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
void put_u32(Bytes& bytes, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
}

void put_i32(Bytes& bytes, std::int32_t value) { put_u32(bytes, static_cast<std::uint32_t>(value)); }

void put_f64(Bytes& bytes, double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (std::size_t i = 0; i < 8; ++i)
        bytes.push_back(static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFF));
}

void put_cstr(Bytes& bytes, std::string_view text) {
    for (char c : text) bytes.push_back(static_cast<std::uint8_t>(c));
    bytes.push_back(0);
}

Bytes with_size(Bytes body) {
    Bytes out;
    put_u32(out, static_cast<std::uint32_t>(body.size() + 4));
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

Bytes encrypt_legacy(Bytes plain) {
    for (std::size_t i = 4; i < plain.size(); ++i)
        plain[i] ^= static_cast<std::uint8_t>(i + 0x55);
    return plain;
}

Bytes sample_stream() {
    Bytes body;
    put_i32(body, 7);
    put_i32(body, 1);
    put_i32(body, 2);
    put_i32(body, 3);
    put_i32(body, 4);

    put_i32(body, 21);
    put_f64(body, 3.5);

    put_i32(body, 22);
    const std::uint8_t hello[] = {0xCF, 0xF0, 0xE8, 0xE2, 0xE5, 0xF2, 0};
    body.insert(body.end(), std::begin(hello), std::end(hello));

    put_i32(body, 23);
    put_cstr(body, "counter");

    put_i32(body, 24);
    put_f64(body, -2.25);

    put_i32(body, 48);
    put_i32(body, 10);
    put_i32(body, 11);
    put_f64(body, 0x1000004);
    put_i32(body, 5);
    put_i32(body, 6);
    put_i32(body, -1);

    put_i32(body, 49);
    put_i32(body, -1);
    put_i32(body, 7);

    put_i32(body, 50);
    put_i32(body, 0);
    put_i32(body, 5);
    put_i32(body, 6);
    put_i32(body, -1);
    return with_size(body);
}
} // namespace

TEST_CASE("cs: parses every node payload shape") {
    auto result = parse_cs(sample_stream());
    REQUIRE(result.has_value());
    const auto& nodes = result.value();
    REQUIRE(nodes.size() == 8);

    CHECK(nodes[0].opcode == 7);
    CHECK(nodes[0].a == 1);
    CHECK(nodes[0].b == 2);
    CHECK(nodes[0].c == 3);
    CHECK(nodes[0].d == 4);
    CHECK(nodes[0].value == -1.0);
    CHECK(nodes[0].text.empty());

    CHECK(nodes[1].opcode == 21);
    CHECK(nodes[1].value == 3.5);
    CHECK(nodes[1].a == -1);

    CHECK(nodes[2].opcode == 22);
    CHECK(nodes[2].text == "Привет");

    CHECK(nodes[3].opcode == 23);
    CHECK(nodes[3].text == "counter");

    CHECK(nodes[4].opcode == 24);
    CHECK(nodes[4].value == -2.25);

    CHECK(nodes[5].opcode == 48);
    CHECK(nodes[5].c == 10);
    CHECK(nodes[5].d == 11);
    CHECK(nodes[5].value == static_cast<double>(0x1000004));
    CHECK(nodes[5].args[0] == 5);
    CHECK(nodes[5].args[1] == 6);
    for (std::size_t i = 2; i < nodes[5].args.size(); ++i) CHECK(nodes[5].args[i] == -1);

    CHECK(nodes[6].opcode == 49);
    CHECK(nodes[6].c == -1);
    CHECK(nodes[6].d == 7);

    CHECK(nodes[7].opcode == 50);
    CHECK(nodes[7].a == 0);
    CHECK(nodes[7].b == 5);
    CHECK(nodes[7].c == 6);
    CHECK(nodes[7].d == -1);
}

TEST_CASE("cs: assign-style opcodes share the four-integer payload") {
    Bytes body;
    put_i32(body, 3);
    put_i32(body, 9);
    put_i32(body, 8);
    put_i32(body, 7);
    put_i32(body, 6);
    put_i32(body, 50);
    put_i32(body, 1);
    put_i32(body, 2);
    put_i32(body, 3);
    put_i32(body, 4);
    auto result = parse_cs(with_size(body));
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 2);
    CHECK(result->at(0).opcode == 3);
    CHECK(result->at(0).a == 9);
    CHECK(result->at(0).b == 8);
    CHECK(result->at(0).c == 7);
    CHECK(result->at(0).d == 6);
    CHECK(result->at(1).opcode == 50);
    CHECK(result->at(1).d == 4);
}

TEST_CASE("cs: decodes a legacy offset-xored script") {
    auto result = parse_cs(encrypt_legacy(sample_stream()));
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 8);
    CHECK(result->at(0).opcode == 7);
    CHECK(result->at(0).d == 4);
    CHECK(result->at(2).text == "Привет");
    CHECK(result->at(5).value == static_cast<double>(0x1000004));
    CHECK(result->at(5).args[1] == 6);
}

TEST_CASE("cs: accepts a header-only file") {
    auto result = parse_cs(with_size({}));
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("cs: rejects a file shorter than the size field") {
    const Bytes data = {1, 2, 3};
    auto result = parse_cs(data);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("cs: rejects a size field that does not match the data") {
    auto smaller = with_size({});
    smaller[0] = 8;
    auto result = parse_cs(smaller);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);

    Bytes trailing = with_size({});
    trailing.push_back(0xAA);
    auto trailing_result = parse_cs(trailing);
    REQUIRE_FALSE(trailing_result.has_value());
    CHECK(trailing_result.error().code == gl::ErrorCode::format);
}

TEST_CASE("cs: rejects an invalid opcode") {
    for (std::int32_t opcode : {-1, 25, 47, 51, 1000}) {
        Bytes body;
        put_i32(body, opcode);
        auto result = parse_cs(with_size(body));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("cs: rejects a truncated integer payload") {
    Bytes body;
    put_i32(body, 7);
    put_i32(body, 1);
    put_i32(body, 2);
    auto result = parse_cs(with_size(body));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("cs: rejects a truncated number payload") {
    Bytes body;
    put_i32(body, 24);
    put_u32(body, 0);
    auto result = parse_cs(with_size(body));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("cs: rejects an unterminated string payload") {
    Bytes body;
    put_i32(body, 22);
    put_cstr(body, "abc");
    body.pop_back();
    auto result = parse_cs(with_size(body));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("cs: rejects function args without a -1 terminator") {
    Bytes body;
    put_i32(body, 48);
    put_i32(body, 0);
    put_i32(body, 0);
    put_f64(body, 0x1000004);
    for (std::int32_t i = 0; i < 9; ++i) put_i32(body, i);
    auto result = parse_cs(with_size(body));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("cs: rejects a truncated function payload") {
    Bytes body;
    put_i32(body, 48);
    put_i32(body, 0);
    put_i32(body, 0);
    put_f64(body, 0x1000004);
    put_i32(body, 1);
    auto result = parse_cs(with_size(body));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("cs: rejects a truncated jump payload") {
    Bytes body;
    put_i32(body, 49);
    put_i32(body, 1);
    auto result = parse_cs(with_size(body));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("cs: opcode names cover the known table and unknown ids") {
    CHECK(std::string(cs_opcode_name(0)) == "||");
    CHECK(std::string(cs_opcode_name(1)) == "^^");
    CHECK(std::string(cs_opcode_name(2)) == "&&");
    CHECK(std::string(cs_opcode_name(3)) == "|");
    CHECK(std::string(cs_opcode_name(4)) == "^");
    CHECK(std::string(cs_opcode_name(5)) == "&");
    CHECK(std::string(cs_opcode_name(6)) == "!=");
    CHECK(std::string(cs_opcode_name(7)) == "==");
    CHECK(std::string(cs_opcode_name(8)) == ">=");
    CHECK(std::string(cs_opcode_name(9)) == "<=");
    CHECK(std::string(cs_opcode_name(10)) == ">");
    CHECK(std::string(cs_opcode_name(11)) == "<");
    CHECK(std::string(cs_opcode_name(12)) == "<<");
    CHECK(std::string(cs_opcode_name(13)) == ">>");
    CHECK(std::string(cs_opcode_name(14)) == "+");
    CHECK(std::string(cs_opcode_name(15)) == "-");
    CHECK(std::string(cs_opcode_name(16)) == "*");
    CHECK(std::string(cs_opcode_name(17)) == "/");
    CHECK(std::string(cs_opcode_name(18)) == "%");
    CHECK(std::string(cs_opcode_name(19)) == "~");
    CHECK(std::string(cs_opcode_name(20)) == "!");
    CHECK(std::string(cs_opcode_name(21)) == "num_var");
    CHECK(std::string(cs_opcode_name(22)) == "str");
    CHECK(std::string(cs_opcode_name(23)) == "str_var");
    CHECK(std::string(cs_opcode_name(24)) == "num");
    CHECK(std::string(cs_opcode_name(48)) == "func");
    CHECK(std::string(cs_opcode_name(49)) == "jmp");
    CHECK(std::string(cs_opcode_name(50)) == "assign");
    CHECK(std::string(cs_opcode_name(25)) == "unknown");
    CHECK(std::string(cs_opcode_name(-1)) == "unknown");
}

TEST_CASE("cs: opcode validity matches the name table") {
    for (std::int32_t opcode = 0; opcode <= 24; ++opcode) CHECK(cs_opcode_valid(opcode));
    CHECK(cs_opcode_valid(48));
    CHECK(cs_opcode_valid(49));
    CHECK(cs_opcode_valid(50));
    CHECK_FALSE(cs_opcode_valid(25));
    CHECK_FALSE(cs_opcode_valid(47));
    CHECK_FALSE(cs_opcode_valid(51));
    CHECK_FALSE(cs_opcode_valid(-1));
}

TEST_CASE("cs: function names cover known ids and unknown ids") {
    CHECK(std::string(cs_function_name(0x1000000)) == "Exit");
    CHECK(std::string(cs_function_name(0x1000001)) == "Signal");
    CHECK(std::string(cs_function_name(0x1000004)) == "D_Say");
    CHECK(std::string(cs_function_name(0x1000005)) == "D_CloseDialog");
    CHECK(std::string(cs_function_name(0x1000006)) == "D_Answer");
    CHECK(std::string(cs_function_name(0x4000000)) == "RS_GetPersonParameterI");
    CHECK(std::string(cs_function_name(0x7000014)) == "RS_SetDoorState");
    CHECK(std::string(cs_function_name(0x12345678)) == "unknown");
    CHECK(std::string(cs_function_name(0)) == "unknown");
}
