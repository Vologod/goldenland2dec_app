#pragma once
#include "gl/assets/result.hpp"
#include "gl/assets/save.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace gl::core {
struct SaveSlotInfo {
    int slot = -1;
    std::filesystem::path path;
    std::uint64_t file_size = 0;
    std::filesystem::file_time_type modified;
    bool valid = false;
    gl::assets::SaveHeader header;
};

class SaveManager {
public:
    static gl::Result<SaveManager> scan(const std::filesystem::path& save_dir);

    const std::vector<SaveSlotInfo>& slots() const;
    const SaveSlotInfo* find(int slot) const;
    bool has_valid_slots() const;

private:
    std::vector<SaveSlotInfo> slots_;
};

std::optional<int> slot_index_from_filename(std::string_view filename);
}
