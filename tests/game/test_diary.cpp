#include "gl/game/diary.hpp"

#include <array>
#include <cstdint>
#include <doctest/doctest.h>
#include <vector>

using gl::assets::SaveBlock58;
using gl::assets::SaveDiaryEntry;
using gl::assets::SavePayload;
using gl::game::DiaryEntry;

namespace {
void set_u32(SaveBlock58 &block, std::size_t index, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        block[index * 4 + shift / 8] = static_cast<std::uint8_t>((value >> shift) & 0xFF);
}

SaveBlock58 make_header(std::uint32_t key, std::uint32_t count, std::uint32_t flag,
                        std::uint32_t status, std::uint32_t text_id) {
    SaveBlock58 block{};
    set_u32(block, 0, key);
    set_u32(block, 1, count);
    set_u32(block, 2, flag);
    set_u32(block, 3, status);
    set_u32(block, 4, text_id);
    gl::game::GameDatetime start;
    start.year = 1125;
    start.month = 5;
    start.day = 17;
    start.weekday = 4;
    start.hour = 2;
    start.minute = 2;
    start.second = 26;
    start.millisecond = 900;
    const auto words = start.to_array();
    for (std::size_t index = 0; index < words.size(); ++index)
        set_u32(block, 5 + index, words[index]);
    if (flag == 1) {
        gl::game::GameDatetime end;
        end.year = 1125;
        end.month = 8;
        end.day = 17;
        end.weekday = 3;
        end.hour = 21;
        end.minute = 10;
        end.second = 30;
        end.millisecond = 480;
        const auto end_words = end.to_array();
        for (std::size_t index = 0; index < end_words.size(); ++index)
            set_u32(block, 13 + index, end_words[index]);
    }
    return block;
}

SavePayload make_payload(bool flag) {
    SavePayload payload;
    SaveDiaryEntry entry;
    entry.key = 6;
    entry.declared_count = 1;
    entry.blocks.push_back(make_header(6, 1, flag ? 1 : 0, 0, 511));
    entry.blocks.push_back(SaveBlock58{});
    payload.diary.push_back(entry);
    return payload;
}
} // namespace

TEST_CASE("decode_diary_entries maps header fields and keeps raw blocks") {
    const auto payload = make_payload(true);
    const auto entries = gl::game::decode_diary_entries(payload);
    REQUIRE(entries.has_value());
    REQUIRE(entries->size() == 1);
    const DiaryEntry &entry = entries->front();
    CHECK(entry.key == 6);
    CHECK(entry.record_count == 1);
    CHECK(entry.end_time_flag == 1);
    CHECK(entry.status == 0);
    CHECK(entry.text_id == 511);
    CHECK(entry.start.year == 1125);
    CHECK(entry.start.month == 5);
    CHECK(entry.start.day == 17);
    REQUIRE(entry.end.has_value());
    CHECK(entry.end->month == 8);
    CHECK(entry.end->hour == 21);
    REQUIRE(entry.blocks.size() == 2);
    CHECK(entry.blocks[0] == payload.diary[0].blocks[0]);
    CHECK(entry.blocks[1] == payload.diary[0].blocks[1]);
}

TEST_CASE("decode_diary_entries accepts a zero end datetime when the flag is clear") {
    const auto payload = make_payload(false);
    const auto entries = gl::game::decode_diary_entries(payload);
    REQUIRE(entries.has_value());
    CHECK_FALSE(entries->front().end.has_value());
}

TEST_CASE("decode_diary_entries rejects invalid headers") {
    SUBCASE("count mismatch") {
        auto payload = make_payload(true);
        set_u32(payload.diary[0].blocks[0], 1, 5);
        CHECK_FALSE(gl::game::decode_diary_entries(payload).has_value());
    }
    SUBCASE("key mismatch") {
        auto payload = make_payload(true);
        payload.diary[0].key = 7;
        CHECK_FALSE(gl::game::decode_diary_entries(payload).has_value());
    }
    SUBCASE("unknown end flag") {
        auto payload = make_payload(true);
        set_u32(payload.diary[0].blocks[0], 2, 2);
        CHECK_FALSE(gl::game::decode_diary_entries(payload).has_value());
    }
    SUBCASE("non-zero end with clear flag") {
        auto payload = make_payload(false);
        set_u32(payload.diary[0].blocks[0], 13, 1125);
        CHECK_FALSE(gl::game::decode_diary_entries(payload).has_value());
    }
    SUBCASE("invalid start") {
        auto payload = make_payload(true);
        set_u32(payload.diary[0].blocks[0], 6, 13);
        CHECK_FALSE(gl::game::decode_diary_entries(payload).has_value());
    }
    SUBCASE("invalid end") {
        auto payload = make_payload(true);
        set_u32(payload.diary[0].blocks[0], 15, 32);
        CHECK_FALSE(gl::game::decode_diary_entries(payload).has_value());
    }
    SUBCASE("no blocks") {
        auto payload = make_payload(true);
        payload.diary[0].blocks.clear();
        CHECK_FALSE(gl::game::decode_diary_entries(payload).has_value());
    }
}
