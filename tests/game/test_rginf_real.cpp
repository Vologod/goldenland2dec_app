#include "gl/assets/bytes.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/game/rginf.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) /
                                       "Saves" / ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

gl::game::RginfDecision decide(const gl::assets::SavePayload &payload) {
    const auto &tail = payload.tail[*payload.level_tail_index];
    return gl::game::decide_rginf(
        tail.rginf, gl::game::GameDatetime::from_time_bytes(payload.info.time),
        gl::game::kSinglePlayerItemsRegenerateIntervalDays);
}
} // namespace

TEST_CASE("real data: rginf due decisions" * doctest::skip(!game_dir_available())) {
    const std::vector<std::pair<int, int>> expected{
        {0, 0}, {2, 0}, {3, 0}, {20, 1}, {21, 0}, {23, 1}};
    for (const auto &[slot, due] : expected) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        REQUIRE(payload->level_tail_index.has_value());
        CAPTURE(slot);
        const auto decision = decide(payload.value());
        CHECK(decision.due == (due != 0));
        CHECK(decision.runtime_latch == static_cast<std::uint32_t>(due));
    }
}

TEST_CASE("real data: rginf corpus decisions" * doctest::skip(!game_dir_available())) {
    std::vector<int> due_slots;
    std::size_t latch_one = 0;
    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        REQUIRE(payload->level_tail_index.has_value());
        CAPTURE(slot);
        if (payload->tail[*payload->level_tail_index].rginf.latch != 0)
            ++latch_one;
        if (decide(payload.value()).due)
            due_slots.push_back(slot);
    }
    CHECK(latch_one == 11);
    CHECK(due_slots == std::vector<int>{20, 23});
}
