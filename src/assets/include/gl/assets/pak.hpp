#pragma once
#include "gl/assets/byte_reader.hpp"
#include "gl/assets/bytes.hpp"
#include "gl/assets/result.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace gl::assets {
constexpr std::uint64_t kMaxEntrySize = 512ull * 1024 * 1024;

struct PakEntry {
    std::string name;
    std::uint32_t size = 0;
    std::uint32_t offset = 0;
};

std::string pak_entry_key(std::string_view name);

class PakArchive {
public:
    static Result<PakArchive> open_from_file(const std::filesystem::path& path);
    static Result<PakArchive> open_from_memory(Bytes data);

    const std::vector<PakEntry>& entries() const { return entries_; }
    const PakEntry* find(std::string_view name) const;
    Result<Bytes> read(const PakEntry& entry) const;

private:
    PakArchive() = default;
    static Result<PakArchive> parse_memory(ByteReader& reader, std::uint64_t file_size);

    std::filesystem::path path_;
    Bytes memory_;
    bool in_memory_ = false;
    std::uint64_t file_size_ = 0;
    std::vector<PakEntry> entries_;
};
}
