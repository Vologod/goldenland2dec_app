#pragma once
#include "gl/assets/sdb.hpp"
#include "gl/game/diary.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace gl::game {
struct DiaryParagraph {
    std::string text;
    bool completed = false;
};

struct DiaryRow {
    std::uint32_t key = 0;
    std::uint32_t text_id = 0;
    std::uint32_t status = 0;
    GameDatetime start;
    std::optional<GameDatetime> end;
    std::string title;
    std::string text;
    std::vector<DiaryParagraph> paragraphs;
};

std::vector<DiaryRow> build_diary_rows(std::span<const DiaryEntry> entries,
                                       const gl::assets::Sdb &texts);
std::string format_diary_date(const GameDatetime &value);
std::string format_diary_day(const GameDatetime &value);
} // namespace gl::game
