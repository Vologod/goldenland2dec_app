#include "gl/assets/save_payload.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {
gl::assets::SaveTailEntry make_entry(const std::string &name, std::uint32_t latch) {
    gl::assets::SaveRginf rginf;
    rginf.latch = latch;
    rginf.datetime.fill(latch + 1);
    const std::vector<std::string> names{"A"};
    const std::vector<gl::assets::SaveEntity> entities;
    auto section = gl::assets::encode_tail_section(rginf, names, entities);
    REQUIRE(section.has_value());
    auto parsed = gl::assets::parse_tail_section_entry(name, section.value());
    REQUIRE(parsed.has_value());
    return parsed.value();
}
} // namespace

TEST_CASE("parse_tail_section_entry fills the entry") {
    const auto entry = make_entry("L1_1", 5);
    CHECK(entry.name == "L1_1");
    CHECK(entry.raw_len == entry.section.size());
    CHECK(entry.rginf.latch == 5);
    CHECK(entry.rginf.datetime[0] == 6);
    CHECK(entry.person_names == std::vector<std::string>{"A"});
    CHECK(entry.entities.empty());
}

TEST_CASE("parse_tail_section_entry rejects malformed sections") {
    const std::vector<std::uint8_t> bad{1, 2, 3};
    const auto parsed = gl::assets::parse_tail_section_entry("bad", bad);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == gl::ErrorCode::format);

    const auto valid = make_entry("L1_1", 1);
    auto trailing = valid.section;
    trailing.push_back(0);
    const auto with_trailing = gl::assets::parse_tail_section_entry("L1_1", trailing);
    REQUIRE_FALSE(with_trailing.has_value());
    CHECK(with_trailing.error().code == gl::ErrorCode::format);
}

TEST_CASE("upsert_tail_entry rejects invalid entries") {
    gl::assets::SavePayload payload;
    const auto valid = make_entry("L1_1", 1);
    gl::assets::SaveTailEntry empty_name = valid;
    empty_name.name.clear();
    CHECK_FALSE(gl::assets::upsert_tail_entry(payload, std::move(empty_name)).has_value());
    gl::assets::SaveTailEntry empty_section = valid;
    empty_section.section.clear();
    empty_section.raw_len = 0;
    CHECK_FALSE(gl::assets::upsert_tail_entry(payload, std::move(empty_section)).has_value());
    gl::assets::SaveTailEntry bad_len = valid;
    bad_len.raw_len = static_cast<std::uint32_t>(bad_len.section.size() + 1);
    CHECK_FALSE(gl::assets::upsert_tail_entry(payload, std::move(bad_len)).has_value());
    CHECK(payload.tail.empty());
}

TEST_CASE("upsert_tail_entry replaces in place and appends") {
    gl::assets::SavePayload payload;
    payload.info.level_id = "L1_1";
    const auto first = make_entry("L1_1", 1);
    const auto first_index = gl::assets::upsert_tail_entry(payload, first);
    REQUIRE(first_index.has_value());
    CHECK(first_index.value() == 0);
    CHECK(payload.tail.size() == 1);
    CHECK(payload.level_tail_index == 0);

    const auto second = make_entry("L1_2", 2);
    const auto second_index = gl::assets::upsert_tail_entry(payload, second);
    REQUIRE(second_index.has_value());
    CHECK(second_index.value() == 1);
    CHECK(payload.tail.size() == 2);
    CHECK(payload.level_tail_index == 1);

    const auto replaced = make_entry("L1_1", 3);
    const auto replaced_index = gl::assets::upsert_tail_entry(payload, replaced);
    REQUIRE(replaced_index.has_value());
    CHECK(replaced_index.value() == 0);
    CHECK(payload.tail.size() == 2);
    CHECK(payload.tail[0].rginf.latch == 3);
    CHECK(payload.level_tail_index == 0);
}

TEST_CASE("set_current_level updates info and index") {
    gl::assets::SavePayload payload;
    payload.info.level_id = "L1_1";
    REQUIRE(gl::assets::upsert_tail_entry(payload, make_entry("L1_1", 1)).has_value());
    REQUIRE(gl::assets::upsert_tail_entry(payload, make_entry("L1_2", 2)).has_value());

    const auto index = gl::assets::set_current_level(payload, "L1_2");
    REQUIRE(index.has_value());
    CHECK(index.value() == 1);
    CHECK(payload.info.level_id == "L1_2");
    CHECK(payload.level_tail_index == 1);

    const auto missing = gl::assets::set_current_level(payload, "NOPE");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code == gl::ErrorCode::format);

    const auto empty = gl::assets::set_current_level(payload, "");
    REQUIRE_FALSE(empty.has_value());
    CHECK(empty.error().code == gl::ErrorCode::format);
}
