#include "gl/core/save_manager.hpp"
#include <chrono>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <string>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::chrono::year_month_day modified_date(std::filesystem::file_time_type modified) {
    const auto days = std::chrono::floor<std::chrono::days>(
        std::filesystem::file_time_type::clock::to_sys(modified));
    return std::chrono::year_month_day{days};
}
}

TEST_CASE("SaveManager: real saves expose 28 valid version 17 slots" *
          doctest::skip(!game_dir_available())) {
    const char* dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);

    auto scanned = gl::core::SaveManager::scan(std::filesystem::path(dir) / "Saves");
    REQUIRE(scanned.has_value());

    const auto& slots = scanned->slots();
    REQUIRE(slots.size() == 28);
    CHECK(scanned->has_valid_slots());

    for (const gl::core::SaveSlotInfo& slot : slots) {
        CHECK(slot.slot >= 0);
        CHECK(slot.valid);
        CHECK(slot.header.version == 17);
        CHECK_FALSE(slot.header.title.empty());

        const std::chrono::year_month_day date = modified_date(slot.modified);
        CHECK(static_cast<int>(date.year()) == slot.header.year);
        CHECK(static_cast<unsigned>(date.month()) == slot.header.month);
    }

    const gl::core::SaveSlotInfo* slot0 = scanned->find(0);
    REQUIRE(slot0 != nullptr);
    CHECK(slot0->header.title == "Quick save");
    CHECK(slot0->header.year == 2022);
    CHECK(slot0->header.month == 11);

    const gl::core::SaveSlotInfo* slot16 = scanned->find(16);
    REQUIRE(slot16 != nullptr);
    CHECK(slot16->header.title.find("Убмйство кукуя") != std::string::npos);
    CHECK(slot16->header.year == 2013);
    CHECK(slot16->header.month == 2);

    const gl::core::SaveSlotInfo* slot25 = scanned->find(25);
    REQUIRE(slot25 != nullptr);
    CHECK(slot25->header.title.find("Продаем артифакты") != std::string::npos);

    CHECK(scanned->find(999) == nullptr);
}
