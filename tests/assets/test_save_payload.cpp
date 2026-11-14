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
std::vector<std::uint8_t> make_rginf_body() {
    std::vector<std::uint8_t> body(36);
    for (std::size_t i = 0; i < body.size(); ++i)
        body[i] = static_cast<std::uint8_t>(0x40 + i);
    return body;
}
std::vector<std::uint8_t> make_pnames_body() {
    std::vector<std::uint8_t> body;
    put_string(body, "L1_1.P1");
    put_string(body, "L2_2.P2");
    put_u32(body, 0);
    return body;
}
std::vector<std::uint8_t> make_type8_body() {
    std::vector<std::uint8_t> body;
    put_u32(body, 0x11223344);
    put_u32(body, 0x55667788);
    put_u32(body, 0);
    return body;
}
std::vector<std::uint8_t> make_type20_body() {
    std::vector<std::uint8_t> body;
    put_u32(body, 0);
    put_u32(body, 0x20);
    put_u32(body, 0x21);
    put_u32(body, 0);
    put_u32(body, 0);
    put_u32(body, 0x22);
    put_u32(body, 0x23);
    return body;
}
std::vector<std::uint8_t> make_type10_body() {
    std::vector<std::uint8_t> body;
    put_u32(body, 0);
    put_u32(body, 0x10);
    put_u32(body, 0x11);
    put_u32(body, 0);
    put_u32(body, 0);
    put_u32(body, 0x12);
    put_u32(body, 0);
    put_u32(body, 0x13);
    put_u32(body, 0);
    return body;
}
std::vector<std::uint8_t> make_type2_body() {
    std::vector<std::uint8_t> body;
    put_u32(body, 0);
    put_u32(body, 1);
    put_u32(body, 2);
    put_u32(body, 3);
    put_u32(body, 4);
    put_u32(body, 5);
    put_u32(body, 6);
    put_u32(body, 7);
    body.insert(body.end(), 0x20, 0x20);
    put_u32(body, 8);
    put_u32(body, 9);
    put_u32(body, 0);
    put_u32(body, 10);
    put_u32(body, 11);
    put_u32(body, 12);
    put_u32(body, 13);
    put_u32(body, 0);
    put_u32(body, 0);
    put_u32(body, 14);
    body.insert(body.end(), 0x11c, 0x11);
    put_u32(body, 15);
    put_u32(body, 16);
    put_u32(body, 17);
    put_u32(body, 18);
    put_u32(body, 19);
    put_u32(body, 0);
    put_u32(body, 0);
    put_u32(body, 0);
    body.insert(body.end(), 0x80, 0x80);
    put_u32(body, 0);
    return body;
}
std::vector<std::uint8_t> make_ents_body() {
    std::vector<std::uint8_t> body;
    put_u32(body, 8);
    const auto entity = make_type8_body();
    body.insert(body.end(), entity.begin(), entity.end());
    put_u32(body, 1);
    return body;
}
std::vector<std::uint8_t> make_blocks(const std::vector<std::uint8_t> &pnames,
                                      const std::vector<std::uint8_t> &rginf,
                                      const std::vector<std::uint8_t> &ents) {
    std::vector<std::uint8_t> d;
    put_block(d, "BT_PNAME", pnames);
    put_block(d, "BT_RGINF", rginf);
    put_block(d, "BT_ENTS ", ents);
    return d;
}
std::vector<std::uint8_t> make_section_blocks() {
    return make_blocks(make_pnames_body(), make_rginf_body(), make_ents_body());
}
std::vector<std::uint8_t> make_section(const std::vector<std::uint8_t> &raw_blocks) {
    std::vector<std::uint8_t> section;
    put_u32(section, static_cast<std::uint32_t>(raw_blocks.size()));
    std::vector<std::uint8_t> compressed(mz_compressBound(raw_blocks.size()));
    mz_ulong length = static_cast<mz_ulong>(compressed.size());
    REQUIRE(mz_compress(compressed.data(), &length, raw_blocks.data(), raw_blocks.size()) == MZ_OK);
    compressed.resize(static_cast<std::size_t>(length));
    put_u32(section, static_cast<std::uint32_t>(compressed.size()));
    section.insert(section.end(), compressed.begin(), compressed.end());
    return section;
}
std::vector<std::uint8_t> make_tail_entry(std::string_view name,
                                          const std::vector<std::uint8_t> &section) {
    std::vector<std::uint8_t> out;
    put_string(out, name);
    put_u32(out, static_cast<std::uint32_t>(section.size()));
    out.insert(out.end(), section.begin(), section.end());
    return out;
}
std::vector<std::uint8_t> make_tail(const std::vector<std::vector<std::uint8_t>> &entries) {
    std::vector<std::uint8_t> out;
    put_u32(out, static_cast<std::uint32_t>(entries.size()));
    for (const auto &entry : entries) out.insert(out.end(), entry.begin(), entry.end());
    return out;
}
std::vector<std::uint8_t> make_default_tail(std::string_view level) {
    return make_tail({make_tail_entry(level, make_section(make_section_blocks()))});
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
std::vector<std::uint8_t> make_save(const std::vector<std::uint8_t> &body,
                                    const std::vector<std::uint8_t> &tail = {}) {
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
    file.insert(file.end(), tail.begin(), tail.end());
    return file;
}
std::vector<std::uint8_t> make_save_with_blocks(const std::vector<std::uint8_t> &blocks) {
    return make_save(make_body("L93_4", make_vars()),
                     make_tail({make_tail_entry("L93_4", make_section(blocks))}));
}
std::uint32_t le_u32(const std::vector<std::uint8_t> &data, std::size_t offset) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(data[offset + static_cast<std::size_t>(i)]) << (8 * i);
    return value;
}
} // namespace

TEST_CASE("save payload: parses blocks and BT_INFO") {
    const auto vars = [] {
        std::vector<std::uint8_t> v;
        put_u32(v, 0);
        return v;
    }();
    const auto file = make_save(make_body("L93_4", vars), make_default_tail("L93_4"));
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
    const auto file = make_save(make_body("L93_4", make_vars()), make_default_tail("L93_4"));
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
    const auto file = make_save(make_body("L93_4", vars), make_default_tail("L93_4"));
    CHECK_FALSE(gl::assets::parse_save_payload(file).has_value());
}

TEST_CASE("save payload: parses tail and selected BT_RGINF/BT_ENTS") {
    const auto file = make_save(make_body("L93_4", make_vars()), make_default_tail("L93_4"));
    auto payload = gl::assets::parse_save_payload(file);
    REQUIRE(payload.has_value());
    REQUIRE(payload->tail.size() == 1u);
    CHECK(payload->tail[0].name == "L93_4");
    REQUIRE(payload->level_tail_index.has_value());
    CHECK(*payload->level_tail_index == 0u);
    CHECK(payload->rginf == make_rginf_body());
    CHECK(payload->rginf.size() == 36u);
    CHECK(payload->ents == make_ents_body());

    const auto &entry = payload->tail[0];
    const auto rginf = make_rginf_body();
    CHECK(entry.rginf.latch == le_u32(rginf, 0));
    for (std::size_t i = 0; i < entry.rginf.datetime.size(); ++i)
        CHECK(entry.rginf.datetime[i] == le_u32(rginf, 4 + 4 * i));
    CHECK(entry.person_names == std::vector<std::string>{"L1_1.P1", "L2_2.P2"});
    REQUIRE(entry.entities.size() == 1u);
    CHECK(entry.entities[0].type == 8u);
    CHECK(entry.entities[0].body == make_type8_body());
    CHECK(entry.entities[0].body.size() == 12u);
}

TEST_CASE("save payload: parses minimal typed entity bodies") {
    std::vector<std::uint8_t> ents;
    const auto append = [&ents](std::uint32_t type, const std::vector<std::uint8_t> &body) {
        put_u32(ents, type);
        ents.insert(ents.end(), body.begin(), body.end());
    };
    append(0x20, make_type20_body());
    append(0x10, make_type10_body());
    append(2, make_type2_body());
    append(8, make_type8_body());
    put_u32(ents, 1);

    const auto file =
        make_save_with_blocks(make_blocks(make_pnames_body(), make_rginf_body(), ents));
    auto payload = gl::assets::parse_save_payload(file);
    REQUIRE(payload.has_value());
    REQUIRE(payload->tail.size() == 1u);
    const auto &entry = payload->tail[0];
    REQUIRE(entry.entities.size() == 4u);

    CHECK(entry.entities[0].type == 0x20u);
    CHECK(entry.entities[0].body == make_type20_body());
    CHECK(entry.entities[0].body.size() == 28u);

    CHECK(entry.entities[1].type == 0x10u);
    CHECK(entry.entities[1].body == make_type10_body());
    CHECK(entry.entities[1].body.size() == 36u);

    CHECK(entry.entities[2].type == 2u);
    CHECK(entry.entities[2].body == make_type2_body());
    CHECK(entry.entities[2].body.size() == 552u);

    CHECK(entry.entities[3].type == 8u);
    CHECK(entry.entities[3].body == make_type8_body());
    CHECK(entry.entities[3].body.size() == 12u);

    CHECK(entry.person_names.size() == 2u);
}

TEST_CASE("save payload: rejects malformed entity snapshots") {
    const auto parse_blocks = [](const std::vector<std::uint8_t> &blocks) {
        return gl::assets::parse_save_payload(make_save_with_blocks(blocks));
    };

    SUBCASE("unknown entity type") {
        std::vector<std::uint8_t> ents;
        put_u32(ents, 9);
        put_u32(ents, 1);
        auto payload =
            parse_blocks(make_blocks(make_pnames_body(), make_rginf_body(), ents));
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("truncated entity body") {
        std::vector<std::uint8_t> ents;
        put_u32(ents, 8);
        put_u32(ents, 0x11223344);
        auto payload =
            parse_blocks(make_blocks(make_pnames_body(), make_rginf_body(), ents));
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("missing entity terminator") {
        std::vector<std::uint8_t> ents;
        put_u32(ents, 8);
        const auto entity = make_type8_body();
        ents.insert(ents.end(), entity.begin(), entity.end());
        auto payload =
            parse_blocks(make_blocks(make_pnames_body(), make_rginf_body(), ents));
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("RGINF is not 36 bytes") {
        auto rginf = make_rginf_body();
        rginf.pop_back();
        auto payload = parse_blocks(
            make_blocks(make_pnames_body(), rginf, make_ents_body()));
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("PNAME is unterminated") {
        std::vector<std::uint8_t> pnames;
        put_string(pnames, "L1_1.P1");
        auto payload =
            parse_blocks(make_blocks(pnames, make_rginf_body(), make_ents_body()));
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("save payload: selects the matching tail entry by level") {
    const auto tail = make_tail({
        make_tail_entry("L1_1", make_section(make_section_blocks())),
        make_tail_entry("L93_4", make_section(make_section_blocks())),
    });
    const auto file = make_save(make_body("L93_4", make_vars()), tail);
    auto payload = gl::assets::parse_save_payload(file);
    REQUIRE(payload.has_value());
    REQUIRE(payload->tail.size() == 2u);
    REQUIRE(payload->level_tail_index.has_value());
    CHECK(*payload->level_tail_index == 1u);
    CHECK(payload->rginf == make_rginf_body());
    CHECK(payload->ents == make_ents_body());
}

TEST_CASE("save payload: rejects missing selected tail entry") {
    const auto file = make_save(make_body("L93_4", make_vars()), make_default_tail("L6_3"));
    CHECK_FALSE(gl::assets::parse_save_payload(file).has_value());
}

TEST_CASE("save payload: rejects bad tail section zlib") {
    auto section = make_section(make_section_blocks());
    section[8] ^= 0xff;
    const auto file = make_save(make_body("L93_4", make_vars()),
                                make_tail({make_tail_entry("L93_4", section)}));
    CHECK_FALSE(gl::assets::parse_save_payload(file).has_value());
}

TEST_CASE("save payload: rejects a truncated block body") {
    std::vector<std::uint8_t> body;
    put_block(body, "BT_SCRN ", {});
    put_tag(body, "BT_INFO ");
    put_u32(body, 1024);
    const auto file = make_save(body, make_default_tail("L93_4"));
    CHECK_FALSE(gl::assets::parse_save_payload(file).has_value());
}

TEST_CASE("save payload: rejects wrong block order") {
    std::vector<std::uint8_t> body;
    put_block(body, "BT_SCRN ", {});
    put_block(body, "BT_VARS ", make_vars());
    put_block(body, "BT_INFO ", make_info("L93_4"));
    put_block(body, "BT_TRIBE", {});
    put_block(body, "BT_DIARY", {});
    put_block(body, "BT_HSTRY", {});
    put_block(body, "BT_HERO ", {});
    const auto file = make_save(body, make_default_tail("L93_4"));
    CHECK_FALSE(gl::assets::parse_save_payload(file).has_value());
}

TEST_CASE("save payload: rejects trailing bytes after the seventh block") {
    auto body = make_body("L93_4", make_vars());
    body.push_back(0x00);
    const auto file = make_save(body, make_default_tail("L93_4"));
    CHECK_FALSE(gl::assets::parse_save_payload(file).has_value());
}
