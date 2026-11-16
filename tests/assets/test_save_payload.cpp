#include "gl/assets/save_payload.hpp"
#include "gl/assets/cp1251.hpp"
#include <doctest/doctest.h>
#include <miniz.h>
#include <array>
#include <cstdint>
#include <cstring>
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
    const std::string encoded = gl::assets::utf8_to_cp1251(text);
    put_u32(out, static_cast<std::uint32_t>(encoded.size()));
    out.insert(out.end(), encoded.begin(), encoded.end());
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
std::vector<std::uint8_t> make_tribe_body() {
    std::vector<std::uint8_t> body;
    put_u32(body, 2);
    put_u32(body, 5);
    put_string(body, "forest");
    put_u32(body, 9);
    put_string(body, "hills");
    for (int i = 0; i < 4; ++i) body.push_back(static_cast<std::uint8_t>(0x30 + i));
    return body;
}
std::vector<std::uint8_t> make_empty_diary() {
    std::vector<std::uint8_t> body;
    put_u32(body, 0);
    put_u32(body, 0);
    return body;
}
std::vector<std::uint8_t> make_empty_history() {
    std::vector<std::uint8_t> body;
    put_u32(body, 0);
    for (int slot = 0; slot < 40; ++slot) {
        put_u32(body, 0);
        put_u32(body, 0);
    }
    put_u32(body, 0);
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
std::vector<std::uint8_t> make_type8_container_body() {
    std::vector<std::uint8_t> body;
    put_u32(body, 0x11223344);
    put_u32(body, 0x55667788);
    put_u32(body, 1);
    put_u32(body, 1);
    body.insert(body.end(), 0xc0, 0xaa);
    put_u32(body, 1);
    body.insert(body.end(), 0x14, 0xbb);
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
                                    const std::vector<std::uint8_t> &vars,
                                    const std::vector<std::uint8_t> &tribe = make_tribe_body(),
                                    const std::vector<std::uint8_t> &diary = make_empty_diary(),
                                    const std::vector<std::uint8_t> &history = make_empty_history()) {
    std::vector<std::uint8_t> d;
    put_block(d, "BT_SCRN ", {});
    put_block(d, "BT_INFO ", make_info(level));
    put_block(d, "BT_VARS ", vars);
    put_block(d, "BT_TRIBE", tribe);
    put_block(d, "BT_DIARY", diary);
    put_block(d, "BT_HSTRY", history);
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

TEST_CASE("save payload: parses BT_TRIBE names") {
    std::vector<std::uint8_t> tribe;
    put_u32(tribe, 2);
    put_u32(tribe, 5);
    const std::array<std::uint8_t, 3> forest{0xCB, 0xE5, 0xF1};
    put_u32(tribe, static_cast<std::uint32_t>(forest.size()));
    tribe.insert(tribe.end(), forest.begin(), forest.end());
    put_u32(tribe, 9);
    put_string(tribe, "hills");
    for (int i = 0; i < 4; ++i) tribe.push_back(static_cast<std::uint8_t>(0x30 + i));

    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), tribe), make_default_tail("L93_4")));
    REQUIRE(payload.has_value());
    REQUIRE(payload->tribes.entries.size() == 2u);
    CHECK(payload->tribes.entries[0].id == 5u);
    CHECK(payload->tribes.entries[0].name == "\xD0\x9B\xD0\xB5\xD1\x81");
    CHECK(payload->tribes.entries[1].id == 9u);
    CHECK(payload->tribes.entries[1].name == "hills");
    REQUIRE(payload->tribes.matrix.size() == 4u);
    CHECK(payload->tribes.matrix[0] == 0x30);
    CHECK(payload->tribes.matrix[1] == 0x31);
    CHECK(payload->tribes.matrix[2] == 0x32);
    CHECK(payload->tribes.matrix[3] == 0x33);
    CHECK(payload->tribes.count() == 2u);
    CHECK(payload->tribes.index_of(5) == 0u);
    CHECK(payload->tribes.index_of(9) == 1u);
    CHECK_FALSE(payload->tribes.index_of(7).has_value());
    REQUIRE(payload->tribes.find(9) != nullptr);
    CHECK(payload->tribes.find(9)->name == "hills");
    REQUIRE(payload->tribes.find("hills") != nullptr);
    CHECK(payload->tribes.find("hills")->id == 9u);
    CHECK(payload->tribes.relation(5, 5) == 0x30);
    CHECK(payload->tribes.relation(5, 9) == 0x31);
    CHECK(payload->tribes.relation(9, 5) == 0x32);
    CHECK(payload->tribes.relation(9, 9) == 0x33);
    CHECK(payload->tribes.relation(5, 7) == 2);
    CHECK(payload->tribes.row(1).size() == 2u);
    CHECK(payload->tribes.row(1)[0] == 0x32);
    CHECK(payload->tribes.row(2).empty());
}

TEST_CASE("save payload: parses an empty BT_TRIBE table") {
    std::vector<std::uint8_t> tribe;
    put_u32(tribe, 0);
    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), tribe), make_default_tail("L93_4")));
    REQUIRE(payload.has_value());
    CHECK(payload->tribes.entries.empty());
    CHECK(payload->tribes.matrix.empty());
}

TEST_CASE("save payload: tribe relation uses entry order and defaults to two") {
    std::vector<std::uint8_t> tribe;
    put_u32(tribe, 3);
    put_u32(tribe, 7);
    put_string(tribe, "a");
    put_u32(tribe, 1);
    put_string(tribe, "b");
    put_u32(tribe, 4);
    put_string(tribe, "c");
    for (std::uint8_t value : std::array<std::uint8_t, 9>{3, 5, 6, 7, 3, 8, 9, 10, 3})
        tribe.push_back(value);

    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), tribe), make_default_tail("L93_4")));
    REQUIRE(payload.has_value());
    const auto &table = payload->tribes;
    CHECK(table.count() == 3u);
    CHECK(table.index_of(7) == 0u);
    CHECK(table.index_of(1) == 1u);
    CHECK(table.index_of(4) == 2u);
    CHECK(table.relation(7, 1) == 5);
    CHECK(table.relation(1, 7) == 7);
    CHECK(table.relation(4, 4) == 3);
    CHECK(table.relation(1, 1) == 3);
    CHECK(table.relation(9, 1) == 2);
}

TEST_CASE("save payload: inconsistent tribe table falls back to two") {
    gl::assets::SaveTribeTable table;
    table.entries.push_back(gl::assets::SaveTribeEntry{1, "a"});
    table.entries.push_back(gl::assets::SaveTribeEntry{2, "b"});
    table.matrix = {3};
    CHECK(table.relation(1, 1) == 2);
    CHECK(table.row(0).empty());
}

TEST_CASE("save payload: duplicate tribe names resolve to the first entry") {
    std::vector<std::uint8_t> tribe;
    put_u32(tribe, 2);
    put_u32(tribe, 1);
    put_string(tribe, "dup");
    put_u32(tribe, 2);
    put_string(tribe, "dup");
    for (std::uint8_t value : std::array<std::uint8_t, 4>{3, 0, 1, 3})
        tribe.push_back(value);

    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), tribe), make_default_tail("L93_4")));
    REQUIRE(payload.has_value());
    REQUIRE(payload->tribes.find("dup") != nullptr);
    CHECK(payload->tribes.find("dup")->id == 1u);
    CHECK(payload->tribes.find(2)->name == "dup");
}

TEST_CASE("save payload: rejects a truncated or oversized tribe matrix") {
    std::vector<std::uint8_t> truncated;
    put_u32(truncated, 2);
    put_u32(truncated, 5);
    put_string(truncated, "forest");
    put_u32(truncated, 9);
    put_string(truncated, "hills");
    truncated.push_back(0x30);

    auto short_payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), truncated), make_default_tail("L93_4")));
    REQUIRE_FALSE(short_payload.has_value());
    CHECK(short_payload.error().code == gl::ErrorCode::format);

    auto long_tribe = make_tribe_body();
    long_tribe.push_back(0x7F);
    auto long_payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), long_tribe), make_default_tail("L93_4")));
    REQUIRE_FALSE(long_payload.has_value());
    CHECK(long_payload.error().code == gl::ErrorCode::format);
}

TEST_CASE("save payload: rejects malformed BT_TRIBE") {
    const auto parse_tribe = [](const std::vector<std::uint8_t> &tribe) {
        return gl::assets::parse_save_payload(
            make_save(make_body("L93_4", make_vars(), tribe), make_default_tail("L93_4")));
    };

    SUBCASE("missing count") {
        auto payload = parse_tribe({});
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("missing entry") {
        std::vector<std::uint8_t> tribe;
        put_u32(tribe, 1);
        auto payload = parse_tribe(tribe);
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("string overruns the body") {
        std::vector<std::uint8_t> tribe;
        put_u32(tribe, 1);
        put_u32(tribe, 3);
        put_u32(tribe, 16);
        tribe.insert(tribe.end(), 2, static_cast<std::uint8_t>('x'));
        auto payload = parse_tribe(tribe);
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("matrix is truncated") {
        std::vector<std::uint8_t> tribe;
        put_u32(tribe, 1);
        put_u32(tribe, 3);
        put_string(tribe, "x");
        tribe.insert(tribe.end(), 3, 0);
        auto payload = parse_tribe(tribe);
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("matrix has trailing bytes") {
        auto tribe = make_tribe_body();
        tribe.push_back(0);
        auto payload = parse_tribe(tribe);
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("save_var_number decodes little-endian f64 bytes") {
    gl::assets::SaveVar var;
    var.type = 0;
    var.value = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x25, 0x40};
    const auto number = gl::assets::save_var_number(var);
    REQUIRE(number.has_value());
    CHECK(*number == 10.5);

    var.type = 1;
    CHECK(gl::assets::save_var_number(var).has_value());
    var.type = 2;
    CHECK(gl::assets::save_var_number(var).has_value());
    var.type = 3;
    CHECK_FALSE(gl::assets::save_var_number(var).has_value());
    var.type = 4;
    CHECK_FALSE(gl::assets::save_var_number(var).has_value());
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

TEST_CASE("save payload: parses a non-zero container body") {
    std::vector<std::uint8_t> ents;
    put_u32(ents, 8);
    const auto body = make_type8_container_body();
    ents.insert(ents.end(), body.begin(), body.end());
    put_u32(ents, 1);

    const auto file =
        make_save_with_blocks(make_blocks(make_pnames_body(), make_rginf_body(), ents));
    auto payload = gl::assets::parse_save_payload(file);
    REQUIRE(payload.has_value());
    REQUIRE(payload->tail.size() == 1u);
    REQUIRE(payload->tail[0].entities.size() == 1u);
    CHECK(payload->tail[0].entities[0].type == 8u);
    CHECK(payload->tail[0].entities[0].body == body);
    CHECK(payload->tail[0].entities[0].body.size() == 232u);
}

TEST_CASE("save payload: PNAME clamps declared length to 0x104") {
    std::vector<std::uint8_t> pnames;
    put_u32(pnames, 0x200);
    pnames.insert(pnames.end(), 0x104, static_cast<std::uint8_t>('A'));
    put_u32(pnames, 0);
    auto payload = gl::assets::parse_save_payload(
        make_save_with_blocks(make_blocks(pnames, make_rginf_body(), make_ents_body())));
    REQUIRE(payload.has_value());
    REQUIRE(payload->tail.size() == 1u);
    REQUIRE(payload->tail[0].person_names.size() == 1u);
    CHECK(payload->tail[0].person_names[0].size() == 0x104u);
}

TEST_CASE("save payload: tail errors identify the failing entry and block") {
    std::vector<std::uint8_t> ents;
    put_u32(ents, 9);
    put_u32(ents, 1);
    auto payload = gl::assets::parse_save_payload(
        make_save_with_blocks(make_blocks(make_pnames_body(), make_rginf_body(), ents)));
    REQUIRE_FALSE(payload.has_value());
    CHECK(payload.error().code == gl::ErrorCode::format);
    CHECK(payload.error().message.find("L93_4") != std::string::npos);
    CHECK(payload.error().message.find("BT_ENTS") != std::string::npos);
    CHECK(payload.error().message.find("9") != std::string::npos);
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

    SUBCASE("type 4 is rejected even with a zero body") {
        std::vector<std::uint8_t> ents;
        put_u32(ents, 4);
        put_u32(ents, 1);
        auto payload =
            parse_blocks(make_blocks(make_pnames_body(), make_rginf_body(), ents));
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("terminator followed by trailing bytes") {
        auto ents = make_ents_body();
        put_u32(ents, 0);
        auto payload =
            parse_blocks(make_blocks(make_pnames_body(), make_rginf_body(), ents));
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("duplicate tag in a section") {
        std::vector<std::uint8_t> blocks;
        put_block(blocks, "BT_PNAME", make_pnames_body());
        put_block(blocks, "BT_RGINF", make_rginf_body());
        put_block(blocks, "BT_RGINF", make_rginf_body());
        put_block(blocks, "BT_ENTS ", make_ents_body());
        auto payload = parse_blocks(blocks);
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

TEST_CASE("save payload: parses diary entries and sparse records") {
    std::vector<std::uint8_t> diary;
    put_u32(diary, 2);
    put_u32(diary, 1);
    diary.insert(diary.end(), 0x58, 0x11);
    put_u32(diary, 3);
    for (int block = 0; block < 3; ++block)
        for (int byte = 0; byte < 0x58; ++byte)
            diary.push_back(static_cast<std::uint8_t>(block * 0x10 + (byte & 0x0F)));
    put_u32(diary, 1);
    diary.insert(diary.end(), 0x58, 0x22);

    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), make_tribe_body(), diary,
                            make_empty_history()),
                  make_default_tail("L93_4")));
    REQUIRE(payload.has_value());
    REQUIRE(payload->diary.size() == 2u);
    CHECK(payload->diary[0].blocks.size() == 1u);
    CHECK(payload->diary[0].blocks[0][0] == 0x11);
    CHECK(payload->diary[1].blocks.size() == 3u);
    CHECK(payload->diary[1].blocks[2][0x10] == 0x20);
    REQUIRE(payload->diary_sparse.size() == 1u);
    CHECK(payload->diary_sparse[0][0] == 0x22);
}

TEST_CASE("save payload: parses history slots") {
    std::vector<std::uint8_t> history;
    put_u32(history, 7);
    for (int slot = 0; slot < 40; ++slot) {
        if (slot == 0)
            put_string(history, "Привет");
        else if (slot == 1)
            put_string(history, "");
        else
            put_u32(history, 0);
        put_u32(history, static_cast<std::uint32_t>(slot));
    }
    put_u32(history, 2);

    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), make_tribe_body(),
                            make_empty_diary(), history),
                  make_default_tail("L93_4")));
    REQUIRE(payload.has_value());
    CHECK(payload->history.head == 7u);
    CHECK(payload->history.tail == 2u);
    CHECK(payload->history.slots[0].text == "Привет");
    CHECK(payload->history.slots[0].id == 0u);
    CHECK(payload->history.slots[1].text.empty());
    CHECK(payload->history.slots[39].id == 39u);
}

TEST_CASE("save payload: rejects malformed diary and history framing") {
    std::vector<std::uint8_t> zero_blocks;
    put_u32(zero_blocks, 1);
    put_u32(zero_blocks, 0);
    auto diary_error = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), make_tribe_body(), zero_blocks,
                            make_empty_history()),
                  make_default_tail("L93_4")));
    REQUIRE_FALSE(diary_error.has_value());
    CHECK(diary_error.error().code == gl::ErrorCode::format);

    std::vector<std::uint8_t> short_history;
    put_u32(short_history, 1);
    for (int slot = 0; slot < 39; ++slot) {
        put_u32(short_history, 0);
        put_u32(short_history, 0);
    }
    put_u32(short_history, 1);
    auto history_error = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), make_tribe_body(),
                            make_empty_diary(), short_history),
                  make_default_tail("L93_4")));
    REQUIRE_FALSE(history_error.has_value());
    CHECK(history_error.error().code == gl::ErrorCode::format);
}

TEST_CASE("save payload: rejects malformed diary and history subcases") {
    const auto parse = [](const std::vector<std::uint8_t> &diary,
                          const std::vector<std::uint8_t> &history) {
        return gl::assets::parse_save_payload(
            make_save(make_body("L93_4", make_vars(), make_tribe_body(), diary, history),
                      make_default_tail("L93_4")));
    };

    SUBCASE("diary truncated multi-block body") {
        std::vector<std::uint8_t> diary;
        put_u32(diary, 1);
        put_u32(diary, 2);
        diary.insert(diary.end(), 0x58, 0x11);
        auto payload = parse(diary, make_empty_history());
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("diary truncated sparse records") {
        std::vector<std::uint8_t> diary;
        put_u32(diary, 0);
        put_u32(diary, 1);
        diary.push_back(0x22);
        auto payload = parse(diary, make_empty_history());
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("diary trailing byte") {
        auto diary = make_empty_diary();
        diary.push_back(0x00);
        auto payload = parse(diary, make_empty_history());
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("history truncated head") {
        auto payload = parse(make_empty_diary(), {});
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("history truncated slot text") {
        std::vector<std::uint8_t> history;
        put_u32(history, 0);
        put_u32(history, 5);
        history.insert(history.end(), 2, static_cast<std::uint8_t>('x'));
        auto payload = parse(make_empty_diary(), history);
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("history truncated slot id") {
        std::vector<std::uint8_t> history;
        put_u32(history, 0);
        put_u32(history, 0);
        auto payload = parse(make_empty_diary(), history);
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("history truncated tail") {
        auto history = make_empty_history();
        history.resize(history.size() - 4);
        auto payload = parse(make_empty_diary(), history);
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }

    SUBCASE("history trailing byte") {
        auto history = make_empty_history();
        history.push_back(0x00);
        auto payload = parse(make_empty_diary(), history);
        REQUIRE_FALSE(payload.has_value());
        CHECK(payload.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("encode_save_payload round-trips a parsed payload") {
    const auto file = make_save(make_body("L93_4", make_vars()),
                                make_default_tail("L93_4"));
    const auto payload = gl::assets::parse_save_payload(file);
    REQUIRE(payload.has_value());

    const auto encoded = gl::assets::encode_save_payload(payload.value());
    REQUIRE(encoded.has_value());
    const auto again = gl::assets::parse_save_payload(encoded.value());
    REQUIRE(again.has_value());

    CHECK(again->header_raw == payload->header_raw);
    CHECK(again->decompressed == payload->decompressed);
    REQUIRE(again->blocks.size() == payload->blocks.size());
    for (std::size_t index = 0; index < payload->blocks.size(); ++index) {
        CHECK(again->blocks[index].tag == payload->blocks[index].tag);
        CHECK(again->blocks[index].offset == payload->blocks[index].offset);
        CHECK(again->blocks[index].size == payload->blocks[index].size);
    }
    REQUIRE(again->tail.size() == payload->tail.size());
    for (std::size_t index = 0; index < payload->tail.size(); ++index) {
        CHECK(again->tail[index].name == payload->tail[index].name);
        CHECK(again->tail[index].raw_len == payload->tail[index].raw_len);
        CHECK(again->tail[index].section == payload->tail[index].section);
    }
    CHECK(again->info.level_id == payload->info.level_id);
    CHECK(again->info.party_level == payload->info.party_level);
    CHECK(again->info.time == payload->info.time);
    CHECK(again->vars.size() == payload->vars.size());
    CHECK(again->diary.size() == payload->diary.size());
    REQUIRE(again->history.slots.size() == payload->history.slots.size());
    for (std::size_t index = 0; index < payload->history.slots.size(); ++index) {
        CHECK(again->history.slots[index].text == payload->history.slots[index].text);
        CHECK(again->history.slots[index].id == payload->history.slots[index].id);
    }

    const auto encoded_twice = gl::assets::encode_save_payload(payload.value());
    REQUIRE(encoded_twice.has_value());
    CHECK(encoded.value() == encoded_twice.value());
}

TEST_CASE("encode_save_payload rejects an invalid payload") {
    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars()), make_default_tail("L93_4")));
    REQUIRE(payload.has_value());

    auto missing_block = payload.value();
    missing_block.blocks.pop_back();
    auto missing = gl::assets::encode_save_payload(missing_block);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code == gl::ErrorCode::format);

    auto wrong_tag = payload.value();
    wrong_tag.blocks[1].tag = "BT_X";
    auto tag_error = gl::assets::encode_save_payload(wrong_tag);
    REQUIRE_FALSE(tag_error.has_value());
    CHECK(tag_error.error().code == gl::ErrorCode::format);

    auto bad_range = payload.value();
    bad_range.blocks[0].size = static_cast<std::uint32_t>(bad_range.decompressed.size() + 1);
    auto range_error = gl::assets::encode_save_payload(bad_range);
    REQUIRE_FALSE(range_error.has_value());
    CHECK(range_error.error().code == gl::ErrorCode::format);

    auto bad_section = payload.value();
    REQUIRE_FALSE(bad_section.tail.empty());
    bad_section.tail[0].raw_len += 1;
    auto section_error = gl::assets::encode_save_payload(bad_section);
    REQUIRE_FALSE(section_error.has_value());
    CHECK(section_error.error().code == gl::ErrorCode::format);
}

TEST_CASE("encode_vars_body writes typed variables") {
    std::vector<gl::assets::SaveVar> vars;
    gl::assets::SaveVar integer;
    integer.type = 0;
    integer.name = "i";
    const double ten = 10.0;
    std::memcpy(integer.value.data(), &ten, 8);
    vars.push_back(integer);

    gl::assets::SaveVar word;
    word.type = 1;
    word.name = "d";
    std::memcpy(word.value.data(), &ten, 8);
    vars.push_back(word);

    gl::assets::SaveVar flt;
    flt.type = 2;
    flt.name = "f";
    const double two_and_a_half = 2.5;
    std::memcpy(flt.value.data(), &two_and_a_half, 8);
    vars.push_back(flt);

    gl::assets::SaveVar text;
    text.type = 3;
    text.name = "s";
    text.text_value = "x";
    vars.push_back(text);

    const auto body = gl::assets::encode_vars_body(vars);
    REQUIRE(body.has_value());

    std::vector<std::uint8_t> expected;
    put_u32(expected, 4);
    put_u32(expected, 0);
    put_string(expected, "i");
    expected.insert(expected.end(), integer.value.begin(), integer.value.end());
    put_u32(expected, 1);
    put_string(expected, "d");
    expected.insert(expected.end(), word.value.begin(), word.value.end());
    put_u32(expected, 2);
    put_string(expected, "f");
    expected.insert(expected.end(), flt.value.begin(), flt.value.end());
    put_u32(expected, 3);
    put_string(expected, "s");
    put_string(expected, "x");
    CHECK(body.value() == expected);

    const auto empty =
        gl::assets::encode_vars_body(std::span<const gl::assets::SaveVar>());
    REQUIRE(empty.has_value());
    CHECK(empty.value() == std::vector<std::uint8_t>{0, 0, 0, 0});

    auto bad = vars;
    bad[0].type = 4;
    const auto error = gl::assets::encode_vars_body(bad);
    REQUIRE_FALSE(error.has_value());
    CHECK(error.error().code == gl::ErrorCode::format);
}

TEST_CASE("encode_vars_body round-trips a non-ASCII name and text") {
    gl::assets::SaveVar text;
    text.type = 3;
    text.name = "Привет";
    text.text_value = "Привет";

    const std::vector<gl::assets::SaveVar> vars{text};
    const auto body = gl::assets::encode_vars_body(vars);
    REQUIRE(body.has_value());

    const std::string encoded_name = gl::assets::utf8_to_cp1251("Привет");
    std::vector<std::uint8_t> expected;
    put_u32(expected, 1);
    put_u32(expected, 3);
    put_u32(expected, static_cast<std::uint32_t>(encoded_name.size()));
    expected.insert(expected.end(), encoded_name.begin(), encoded_name.end());
    put_u32(expected, static_cast<std::uint32_t>(encoded_name.size()));
    expected.insert(expected.end(), encoded_name.begin(), encoded_name.end());
    CHECK(body.value() == expected);
}

TEST_CASE("encode_tribe_body writes entries and the matrix") {
    gl::assets::SaveTribeTable table;
    table.entries.push_back(gl::assets::SaveTribeEntry{7, "a"});
    table.entries.push_back(gl::assets::SaveTribeEntry{1, "b"});
    table.entries.push_back(gl::assets::SaveTribeEntry{4, "c"});
    table.matrix = {3, 5, 6, 7, 3, 8, 9, 10, 3};

    std::vector<std::uint8_t> expected;
    put_u32(expected, 3);
    put_u32(expected, 7);
    put_string(expected, "a");
    put_u32(expected, 1);
    put_string(expected, "b");
    put_u32(expected, 4);
    put_string(expected, "c");
    expected.insert(expected.end(), table.matrix.begin(), table.matrix.end());

    const auto body = gl::assets::encode_tribe_body(table);
    REQUIRE(body.has_value());
    CHECK(body.value() == expected);

    const auto empty = gl::assets::encode_tribe_body({});
    REQUIRE(empty.has_value());
    CHECK(empty.value() == std::vector<std::uint8_t>{0, 0, 0, 0});

    table.matrix.pop_back();
    const auto error = gl::assets::encode_tribe_body(table);
    REQUIRE_FALSE(error.has_value());
    CHECK(error.error().code == gl::ErrorCode::format);
}
