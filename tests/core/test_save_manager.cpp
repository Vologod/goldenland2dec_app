#include "gl/core/save_manager.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr const char kMagic[] = "Goldenland II save ";

std::filesystem::path unique_temp_dir() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("gl_save_manager_test_" + std::to_string(stamp) + "_" +
            std::to_string(counter.fetch_add(1)));
}

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFF);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
    }
}

std::string make_save(std::string_view title, bool bad_magic = false) {
    std::vector<std::uint8_t> bytes(128, 0);
    for (std::size_t i = 0; i < sizeof(kMagic); ++i) {
        bytes[i] = static_cast<std::uint8_t>(kMagic[i]);
    }
    if (bad_magic) bytes[0] = 'X';
    put_u32(bytes, 20, 17);
    put_u16(bytes, 24, 2022);
    put_u16(bytes, 26, 11);
    put_u16(bytes, 30, 7);
    put_u16(bytes, 32, 10);
    put_u16(bytes, 34, 20);
    put_u16(bytes, 36, 30);
    for (std::size_t i = 0; i < title.size() && i < 64; ++i) {
        bytes[40 + i] = static_cast<std::uint8_t>(title[i]);
    }
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void write_bytes(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.is_open());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(out.good());
}
}

TEST_CASE("SaveManager: scans slots, sorts by index and retains broken saves") {
    const auto dir = unique_temp_dir();
    write_bytes(dir / "slot3.sav", make_save("Third"));
    write_bytes(dir / "slot0.sav", make_save("Quick save"));
    write_bytes(dir / "Slot10.SAV", make_save("Tenth"));
    write_bytes(dir / "broken.sav", make_save("Broken", true));
    write_bytes(dir / "other.txt", "not a save");

    auto scanned = gl::core::SaveManager::scan(dir);
    REQUIRE(scanned.has_value());
    const std::vector<gl::core::SaveSlotInfo>& slots = scanned->slots();

    REQUIRE(slots.size() == 4);
    CHECK(slots[0].slot == -1);
    CHECK_FALSE(slots[0].valid);
    CHECK(slots[1].slot == 0);
    CHECK(slots[1].valid);
    CHECK(slots[1].header.title == "Quick save");
    CHECK(slots[1].file_size == 128);
    CHECK(slots[2].slot == 3);
    CHECK(slots[2].valid);
    CHECK(slots[3].slot == 10);
    CHECK(slots[3].valid);
    CHECK(slots[3].header.title == "Tenth");
    CHECK(slots[3].path.filename() == "Slot10.SAV");

    REQUIRE(scanned->find(0) != nullptr);
    CHECK(scanned->find(0)->header.year == 2022);
    CHECK(scanned->find(3) != nullptr);
    CHECK(scanned->find(10) != nullptr);
    CHECK(scanned->find(4) == nullptr);
    CHECK(scanned->find(999) == nullptr);
    CHECK(scanned->has_valid_slots());

    std::filesystem::remove_all(dir);
}

TEST_CASE("SaveManager: missing directory and non-directory yield empty success") {
    const auto base = unique_temp_dir();
    const auto missing = base / "missing";

    auto missing_result = gl::core::SaveManager::scan(missing);
    REQUIRE(missing_result.has_value());
    CHECK(missing_result->slots().empty());
    CHECK_FALSE(missing_result->has_valid_slots());

    const auto file = base / "Saves";
    write_bytes(file, "not a directory");
    auto file_result = gl::core::SaveManager::scan(file);
    REQUIRE(file_result.has_value());
    CHECK(file_result->slots().empty());
    CHECK(file_result->find(0) == nullptr);

    std::filesystem::remove_all(base);
}

TEST_CASE("save_slot_from_filename: parses valid slot names and rejects others") {
    CHECK(gl::core::save_slot_from_filename("slot0.sav") == std::optional<int>{0});
    CHECK(gl::core::save_slot_from_filename("SLOT12.SAV") == std::optional<int>{12});
    CHECK(gl::core::save_slot_from_filename("slot007.sav") == std::optional<int>{7});
    CHECK_FALSE(gl::core::save_slot_from_filename("slot.sav").has_value());
    CHECK_FALSE(gl::core::save_slot_from_filename("slot1x.sav").has_value());
    CHECK_FALSE(gl::core::save_slot_from_filename("xslot1.sav").has_value());
    CHECK_FALSE(gl::core::save_slot_from_filename("slot1.txt").has_value());
    CHECK_FALSE(gl::core::save_slot_from_filename("slot2147483648.sav").has_value());
    CHECK_FALSE(gl::core::save_slot_from_filename("").has_value());
}
