#pragma once
#include "gl/assets/save_payload.hpp"
#include "gl/game/rginf.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace gl::game {
struct DiaryEntry {
    std::uint32_t key = 0;
    std::uint32_t record_count = 0;
    std::uint32_t end_time_flag = 0;
    std::uint32_t status = 0;
    std::uint32_t text_id = 0;
    GameDatetime start;
    std::optional<GameDatetime> end;
    std::vector<gl::assets::SaveBlock58> blocks;
};

Result<std::vector<DiaryEntry>>
decode_diary_entries(const gl::assets::SavePayload &payload);
} // namespace gl::game
