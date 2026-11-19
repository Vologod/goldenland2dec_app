#include "gl/game/diary.hpp"

#include <array>
#include <string>
#include <utility>

namespace gl::game {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

std::uint32_t u32_at(const gl::assets::SaveBlock58 &block, std::size_t index) {
    const std::size_t offset = index * 4;
    return static_cast<std::uint32_t>(block[offset]) |
           (static_cast<std::uint32_t>(block[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(block[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(block[offset + 3]) << 24);
}

std::array<std::uint32_t, 8> words_at(const gl::assets::SaveBlock58 &block,
                                      std::size_t first) {
    std::array<std::uint32_t, 8> out{};
    for (std::size_t index = 0; index < out.size(); ++index)
        out[index] = u32_at(block, first + index);
    return out;
}
} // namespace

Result<std::vector<DiaryEntry>>
decode_diary_entries(const gl::assets::SavePayload &payload) {
    try {
        std::vector<DiaryEntry> entries;
        entries.reserve(payload.diary.size());
        for (const gl::assets::SaveDiaryEntry &saved : payload.diary) {
            if (saved.blocks.empty())
                return format_error("diary entry has no header block");
            const gl::assets::SaveBlock58 &header = saved.blocks.front();
            DiaryEntry entry;
            entry.key = u32_at(header, 0);
            entry.record_count = u32_at(header, 1);
            entry.end_time_flag = u32_at(header, 2);
            entry.status = u32_at(header, 3);
            entry.text_id = u32_at(header, 4);
            entry.start = GameDatetime::from_array(words_at(header, 5));
            if (!valid_datetime(entry.start))
                return format_error("diary header start datetime is invalid");
            if (entry.key != saved.key)
                return format_error("diary header key does not match the entry");
            if (entry.record_count != saved.blocks.size() - 1)
                return format_error("diary record count does not match the blocks");
            if (entry.end_time_flag == 1) {
                const GameDatetime end = GameDatetime::from_array(words_at(header, 13));
                if (!valid_datetime(end))
                    return format_error("diary header end datetime is invalid");
                entry.end = end;
            } else if (entry.end_time_flag == 0) {
                for (std::size_t index = 13; index < 21; ++index)
                    if (u32_at(header, index) != 0)
                        return format_error("diary header end datetime is not zero");
            } else {
                return format_error("diary header end-time flag is unknown");
            }
            entry.blocks = saved.blocks;
            entries.push_back(std::move(entry));
        }
        return entries;
    } catch (const std::bad_alloc &) {
        return format_error("diary entry allocation failed");
    }
}
} // namespace gl::game
