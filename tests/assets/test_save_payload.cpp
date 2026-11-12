#include "gl/assets/save_payload.hpp"
#include <doctest/doctest.h>
#include <miniz.h>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
void put_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void put_tag(std::vector<std::uint8_t> &out, std::string_view tag) {
    REQUIRE(tag.size() == 8);
    out.insert(out.end(), tag.begin(), tag.end());
}
void put_block(std::vector<std::uint8_t> &out, std::string_view tag,
               const std::vector<std::uint8_t> &body) {
    put_tag(out, tag);
    put_u32(out, static_cast<std::uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
}
void put_string(std::vector<std::uint8_t> &out, std::string_view text) {
    put_u32(out, static_cast<std::uint32_t>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
}
std::vector<std::uint8_t> make_info(std::string_view level) {
    std::vector<std::uint8_t> body;
    put_string(body, "session");
    put_u32(body, 0);
    body.insert(body.end(), 0x20, 0x11);
    put_string(body, "desc");
    put_string(body, level);
    put_u32(body, 40);
    for (int i = 0; i < 8; ++i) body.push_back(static_cast<std::uint8_t>(i));
    return body;
}
std::vector<std::uint8_t> make_vars() {
    std::vector<std::uint8_t> body;
    put_u32(body, 3);
    put_u32(body, 0);
    put_string(body, "voevoda_povtor");
    for (int i = 1; i <= 8; ++i) body.push_back(static_cast<std::uint8_t>(i));
    put_u32(body, 3);
    put_string(body, "note");
    put_string(body, "hello");
    put_u32(body, 2);
    put_string(body, "f");
    for (int i = 0; i < 8; ++i) body.push_back(static_cast<std::uint8_t>(0xf0 + i));
    return body;
}
std::vector<std::uint8_t> make_body(std::string_view level,
                                    const std::vector<std::uint8_t> &vars) {
    std::vector<std::uint8_t> d;
    put_block(d, "BT_SCRN ", {});
    put_block(d, "BT_INFO ", make_info(level));
    put_block(d, "BT_VARS ", vars);
    put_block(d, "BT_TRIBE", {});
    put_block(d, "BT_DIARY", {});
    put_block(d, "BT_HSTRY", {});
    put_block(d, "BT_HERO ", {});
    return d;
}
std::vector<std::uint8_t> make_save(const std::vector<std::uint8_t> &body) {
    std::vector<std::uint8_t> file(112, 0);
    const std::string magic = "Goldenland II save ";
    for (std::size_t i = 0; i < magic.size(); ++i) file[i] = magic[i];
    for (int i = 0; i < 4; ++i) file[20 + i] = static_cast<std::uint8_t>(17u >> (8 * i));
    std::vector<std::uint8_t> compressed(mz_compressBound(body.size()));
    mz_ulong length = static_cast<mz_ulong>(compressed.size());
    REQUIRE(mz_compress(compressed.data(), &length, body.data(), body.size()) == MZ_OK);
    compressed.resize(static_cast<std::size_t>(length));
    const auto put_at = [&file](std::size_t offset, std::uint32_t value) {
        for (int i = 0; i < 4; ++i)
            file[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    };
    put_at(0x68, static_cast<std::uint32_t>(body.size()));
    put_at(0x6c, static_cast<std::uint32_t>(compressed.size()));
    file.insert(file.end(), compressed.begin(), compressed.end());
    return file;
}
} // namespace

TEST_CASE("save payload: parses blocks and BT_INFO") {
    const auto vars = [] {
        std::vector<std::uint8_t> v;
        put_u32(v, 0);
        return v;
    }();
    const auto file = make_save(make_body("L93_4", vars));
    auto payload = gl::assets::parse_save_payload(file);
    REQUIRE(payload.has_value());
    CHECK(payload->info.level_id == "L93_4");
    CHECK(payload->info.field_218 == 0);
    CHECK(payload->info.party_level == 40);
    std::array<std::uint8_t, 0x20> expected_time;
    expected_time.fill(0x11);
    CHECK(payload->info.time == expected_time);
    std::array<std::uint8_t, 8> expected_tail;
    for (int i = 0; i < 8; ++i) expected_tail[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);
    CHECK(payload->info.field_26c8 == expected_tail);
    CHECK(payload->blocks.size() == 7u);
    CHECK(payload->blocks[0].tag == "BT_SCRN ");
    CHECK(payload->blocks[1].tag == "BT_INFO ");
}

TEST_CASE("save payload: parses BT_VARS") {
    const auto file = make_save(make_body("L93_4", make_vars()));
    auto payload = gl::assets::parse_save_payload(file);
    REQUIRE(payload.has_value());
    REQUIRE(payload->vars.size() == 3u);

    CHECK(payload->vars[0].type == 0);
    CHECK(payload->vars[0].name == "voevoda_povtor");
    CHECK(payload->vars[0].value ==
          std::array<std::uint8_t, 8>{1, 2, 3, 4, 5, 6, 7, 8});
    CHECK(payload->vars[0].text_value.empty());

    CHECK(payload->vars[1].type == 3);
    CHECK(payload->vars[1].name == "note");
    CHECK(payload->vars[1].text_value == "hello");

    CHECK(payload->vars[2].type == 2);
    CHECK(payload->vars[2].name == "f");
    CHECK(payload->vars[2].value ==
          std::array<std::uint8_t, 8>{0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7});
}

TEST_CASE("save payload: rejects unknown var type") {
    std::vector<std::uint8_t> vars;
    put_u32(vars, 1);
    put_u32(vars, 9);
    put_string(vars, "x");
    const auto file = make_save(make_body("L93_4", vars));
    CHECK_FALSE(gl::assets::parse_save_payload(file).has_value());
}
