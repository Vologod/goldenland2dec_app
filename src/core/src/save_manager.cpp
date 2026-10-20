#include "gl/core/save_manager.hpp"
#include "gl/assets/log.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <utility>

namespace gl::core {
namespace {
constexpr std::size_t kReadSize = 128;

char ascii_lower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

bool ascii_iequals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (ascii_lower(lhs[i]) != ascii_lower(rhs[i])) return false;
    }
    return true;
}

bool has_save_extension(std::string_view filename) {
    return filename.size() > 4 && filename[filename.size() - 4] == '.' &&
           ascii_iequals(filename.substr(filename.size() - 3), "sav");
}

void load_slot(SaveSlotInfo& slot) {
    std::ifstream in(slot.path, std::ios::binary);
    if (!in) {
        gl::log(gl::LogLevel::warning, "save: cannot open '" + slot.path.string() + "'");
        return;
    }

    std::array<std::uint8_t, kReadSize> buffer{};
    in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = in.gcount();
    if (count <= 0) {
        gl::log(gl::LogLevel::warning, "save: cannot read header from '" + slot.path.string() + "'");
        return;
    }

    const auto data = std::span<const std::uint8_t>(buffer.data(), static_cast<std::size_t>(count));
    auto header = gl::assets::parse_save_header(data);
    if (!header.has_value()) {
        gl::log(gl::LogLevel::warning, "save: invalid header in '" + slot.path.string() +
                                           "': " + header.error().message);
        return;
    }

    slot.valid = true;
    slot.header = std::move(header.value());
}
}

std::optional<int> save_slot_from_filename(std::string_view filename) {
    const std::size_t separator = filename.find_last_of("/\\");
    if (separator != std::string_view::npos) filename.remove_prefix(separator + 1);
    if (!has_save_extension(filename)) return std::nullopt;

    const std::string_view stem = filename.substr(0, filename.size() - 4);
    if (stem.size() <= 4 || !ascii_iequals(stem.substr(0, 4), "slot")) return std::nullopt;

    int slot = 0;
    for (const char value : stem.substr(4)) {
        if (value < '0' || value > '9') return std::nullopt;
        const int digit = value - '0';
        if (slot > (std::numeric_limits<int>::max() - digit) / 10) return std::nullopt;
        slot = slot * 10 + digit;
    }
    return slot;
}

gl::Result<SaveManager> SaveManager::scan(const std::filesystem::path& save_dir) {
    SaveManager manager;

    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::status(save_dir, ec);
    if (ec || !std::filesystem::exists(status) || !std::filesystem::is_directory(status)) {
        return manager;
    }

    std::filesystem::directory_iterator iterator(save_dir, ec);
    if (ec) return Error{ErrorCode::io, "cannot scan saves: " + save_dir.string()};
    const std::filesystem::directory_iterator end;

    for (; iterator != end; iterator.increment(ec)) {
        if (ec) return Error{ErrorCode::io, "cannot scan saves: " + save_dir.string()};

        const std::filesystem::directory_entry& entry = *iterator;
        std::error_code file_ec;
        if (!entry.is_regular_file(file_ec) || file_ec) continue;

        const std::string filename = entry.path().filename().string();
        if (!has_save_extension(filename)) continue;

        SaveSlotInfo slot;
        slot.path = entry.path();
        const std::optional<int> index = save_slot_from_filename(filename);
        slot.slot = index.value_or(-1);

        const std::uintmax_t size = entry.file_size(file_ec);
        slot.file_size = file_ec ? 0 : static_cast<std::uint64_t>(size);

        const std::filesystem::file_time_type modified = entry.last_write_time(file_ec);
        if (!file_ec) slot.modified = modified;

        load_slot(slot);
        manager.slots_.push_back(std::move(slot));
    }

    std::stable_sort(manager.slots_.begin(), manager.slots_.end(),
                     [](const SaveSlotInfo& lhs, const SaveSlotInfo& rhs) {
                         return lhs.slot < rhs.slot;
                     });
    return manager;
}

const std::vector<SaveSlotInfo>& SaveManager::slots() const { return slots_; }

const SaveSlotInfo* SaveManager::find(int slot) const {
    for (const SaveSlotInfo& info : slots_) {
        if (info.slot == slot) return &info;
    }
    return nullptr;
}

bool SaveManager::has_valid_slots() const {
    return std::any_of(slots_.begin(), slots_.end(),
                       [](const SaveSlotInfo& info) { return info.valid; });
}
}
