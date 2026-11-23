#include "gl/game/diary_view.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <tuple>

namespace gl::game {
namespace {
std::uint32_t word_at(const gl::assets::SaveBlock58 &block, std::size_t index) {
    const std::size_t offset = index * 4;
    return static_cast<std::uint32_t>(block[offset]) |
           (static_cast<std::uint32_t>(block[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(block[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(block[offset + 3]) << 24);
}

auto date_key(const GameDatetime &value) {
    return std::tie(value.year, value.month, value.day, value.hour, value.minute,
                    value.second, value.millisecond);
}

std::string text_for(const gl::assets::Sdb &texts, std::uint32_t id) {
    const std::string *text = texts.find(id);
    return text != nullptr ? *text : std::string{};
}
} // namespace

std::vector<DiaryRow> build_diary_rows(std::span<const DiaryEntry> entries,
                                       const gl::assets::Sdb &texts) {
    std::vector<DiaryRow> rows;
    rows.reserve(entries.size());
    for (const DiaryEntry &entry : entries) {
        if (entry.status != 1) continue;
        DiaryRow row;
        row.key = entry.key;
        row.text_id = entry.text_id;
        row.status = entry.status;
        row.start = entry.start;
        row.end = entry.end;
        const std::string *title = texts.find(entry.key);
        row.title = title != nullptr && !title->empty()
                        ? *title
                        : "#" + std::to_string(entry.key);

        for (std::size_t block = 1; block < entry.blocks.size(); ++block) {
            const std::uint32_t stage_id = word_at(entry.blocks[block], 1);
            if (stage_id == 0 || stage_id == 0xFFFFFFFFu) continue;
            std::string text = text_for(texts, stage_id);
            if (text.empty()) continue;
            row.paragraphs.push_back(
                DiaryParagraph{std::move(text), word_at(entry.blocks[block], 2) != 0});
        }
        if (row.paragraphs.empty())
            row.paragraphs.push_back(
                DiaryParagraph{"#" + std::to_string(entry.text_id), false});
        for (std::size_t i = 0; i < row.paragraphs.size(); ++i) {
            if (i != 0) row.text += "\n\n";
            row.text += row.paragraphs[i].text;
        }
        rows.push_back(std::move(row));
    }
    std::stable_sort(rows.begin(), rows.end(), [](const DiaryRow &left, const DiaryRow &right) {
        if (left.end.has_value() != right.end.has_value()) return !left.end.has_value();
        return date_key(left.start) > date_key(right.start);
    });
    return rows;
}

std::string format_diary_date(const GameDatetime &value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02u.%02u.%04u %02u:%02u", value.day,
                  value.month, value.year, value.hour, value.minute);
    return std::string(buffer);
}

std::string format_diary_day(const GameDatetime &value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02u.%02u.%04u", value.day, value.month,
                  value.year);
    return std::string(buffer);
}
} // namespace gl::game
