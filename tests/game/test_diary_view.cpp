#include "gl/assets/save_payload.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/diary_view.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <doctest/doctest.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
std::vector<std::uint8_t> sdb_entry(std::uint32_t id, std::string_view text) {
    std::vector<std::uint8_t> out;
    const auto put = [&](std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8)
            out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
    };
    put(id);
    put(static_cast<std::uint32_t>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
    return out;
}

gl::assets::SaveBlock58 stage_block(std::uint32_t text_id, bool completed = false) {
    gl::assets::SaveBlock58 block{};
    block[4] = static_cast<std::uint8_t>(text_id & 0xFF);
    block[5] = static_cast<std::uint8_t>((text_id >> 8) & 0xFF);
    block[6] = static_cast<std::uint8_t>((text_id >> 16) & 0xFF);
    block[7] = static_cast<std::uint8_t>((text_id >> 24) & 0xFF);
    block[8] = completed ? 1 : 0;
    return block;
}

gl::assets::Sdb make_texts() {
    std::vector<std::uint8_t> data{'S', 'D', 'B', ' '};
    for (const auto &entry : {sdb_entry(5, "title five"), sdb_entry(6, "title six"),
                              sdb_entry(7, "first"), sdb_entry(9, "second")}) {
        data.insert(data.end(), entry.begin(), entry.end());
    }
    auto sdb = gl::assets::Sdb::parse(data);
    REQUIRE(sdb.has_value());
    return std::move(sdb.value());
}

gl::game::GameDatetime datetime(std::uint32_t year, std::uint32_t month, std::uint32_t day,
                                std::uint32_t hour, std::uint32_t minute) {
    gl::game::GameDatetime value;
    value.year = year;
    value.month = month;
    value.day = day;
    value.hour = hour;
    value.minute = minute;
    return value;
}
} // namespace

TEST_CASE("diary rows map text ids and keep save order") {
    const gl::assets::Sdb texts = make_texts();
    gl::game::DiaryEntry first;
    first.key = 5;
    first.text_id = 7;
    first.status = 1;
    first.start = datetime(2024, 3, 5, 7, 8);
    first.blocks = {gl::assets::SaveBlock58{}, stage_block(7)};
    gl::game::DiaryEntry second;
    second.key = 6;
    second.text_id = 9;
    second.status = 1;
    second.start = datetime(2024, 4, 6, 9, 10);
    second.blocks = {gl::assets::SaveBlock58{}, stage_block(9)};
    gl::game::DiaryEntry third;
    third.key = 8;
    third.text_id = 12;
    third.status = 1;
    const std::vector<gl::game::DiaryEntry> entries{first, second, third};

    const auto rows = gl::game::build_diary_rows(entries, texts);
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].key == 6);
    CHECK(rows[0].text == "second");
    CHECK(rows[0].title == "title six");
    CHECK(rows[1].key == 5);
    CHECK(rows[1].text == "first");
    CHECK(rows[1].title == "title five");
    CHECK(rows[2].text == "#12");
    CHECK(rows[2].title == "#8");
    CHECK(rows[1].status == 1);
    CHECK(gl::game::format_diary_date(rows[1].start) == "05.03.2024 07:08");
    CHECK(gl::game::format_diary_day(rows[1].start) == "05.03.2024");
}

TEST_CASE("diary view handles empty input") {
    const gl::assets::Sdb texts = make_texts();
    const auto rows = gl::game::build_diary_rows({}, texts);
    CHECK(rows.empty());
}

TEST_CASE("diary rows put active quests before completed and mark stages") {
    const gl::assets::Sdb texts = make_texts();
    gl::game::DiaryEntry completed;
    completed.key = 6;
    completed.text_id = 9;
    completed.status = 1;
    completed.start = datetime(2024, 5, 1, 12, 0);
    completed.end = datetime(2024, 5, 2, 12, 0);
    completed.blocks = {gl::assets::SaveBlock58{}, stage_block(9, true)};
    gl::game::DiaryEntry active;
    active.key = 5;
    active.text_id = 7;
    active.status = 1;
    active.start = datetime(2024, 4, 1, 12, 0);
    active.blocks = {gl::assets::SaveBlock58{}, stage_block(7, false)};
    const std::vector<gl::game::DiaryEntry> entries{completed, active};

    const auto rows = gl::game::build_diary_rows(entries, texts);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].key == 5);
    CHECK_FALSE(rows[0].end.has_value());
    CHECK(rows[1].key == 6);
    CHECK(rows[1].end.has_value());
    REQUIRE(rows[0].paragraphs.size() == 1);
    CHECK_FALSE(rows[0].paragraphs[0].completed);
    REQUIRE(rows[1].paragraphs.size() == 1);
    CHECK(rows[1].paragraphs[0].completed);
}

TEST_CASE("real slot0 diary maps lit quest texts" *
          doctest::skip(std::getenv("GL_GAME_DIR") == nullptr)) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    const std::filesystem::path save = std::filesystem::path(dir) / "Saves" / "slot0.sav";
    std::ifstream in(save, std::ios::binary);
    REQUIRE(in);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    auto payload = gl::assets::parse_save_payload(bytes);
    REQUIRE(payload.has_value());
    auto entries = gl::game::decode_diary_entries(payload.value());
    REQUIRE(entries.has_value());
    CHECK(entries->size() == 76);

    auto sdb_data = vfs->read("sdb/diary/single_quest_lit.sdb");
    REQUIRE(sdb_data.has_value());
    auto texts = gl::assets::Sdb::parse(sdb_data.value());
    REQUIRE(texts.has_value());
    const auto rows = gl::game::build_diary_rows(entries.value(), texts.value());
    const auto storyline = std::count_if(entries->begin(), entries->end(),
                                         [](const gl::game::DiaryEntry &entry) {
                                             return entry.status == 1;
                                         });
    CHECK(storyline == 20);
    REQUIRE(rows.size() == static_cast<std::size_t>(storyline));
    std::size_t fallbacks = 0;
    std::size_t title_fallbacks = 0;
    for (const auto &row : rows) {
        if (row.text.starts_with("#")) ++fallbacks;
        if (row.title.starts_with("#")) ++title_fallbacks;
    }
    CHECK(fallbacks == 0);
    CHECK(title_fallbacks == 0);
}
