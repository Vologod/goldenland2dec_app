#include "gl/assets/pak.hpp"
#include "gl/assets/cp1251.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <istream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <miniz.h>

namespace gl::assets {
namespace {
bool read_stream_u32(std::istream& in, std::uint32_t& out) {
    unsigned char b[4];
    if (!in.read(reinterpret_cast<char*>(b), 4)) return false;
    out = std::uint32_t(b[0]) | (std::uint32_t(b[1]) << 8) |
          (std::uint32_t(b[2]) << 16) | (std::uint32_t(b[3]) << 24);
    return true;
}

std::string archive_label(std::string_view path) {
    return path.empty() ? std::string("<memory>") : std::string(path);
}

Error format_error(std::string_view archive, std::string message) {
    return Error{ErrorCode::format, std::string(archive) + ": " + std::move(message)};
}

Result<Bytes> inflate_zlib(std::span<const std::uint8_t> data, std::uint32_t unpacked_size) {
    if (unpacked_size > kMaxEntrySize) return Error{ErrorCode::format, "entry too large"};
    if (data.empty()) return Error{ErrorCode::format, "empty zlib stream"};
    Bytes out(unpacked_size > 0 ? unpacked_size : 1);
    mz_ulong out_len = unpacked_size;
    const int status = mz_uncompress(out.data(), &out_len, data.data(), data.size());
    if (status != MZ_OK || out_len != unpacked_size)
        return Error{ErrorCode::format, "zlib decompression failed"};
    out.resize(unpacked_size);
    return out;
}

struct EntryExtent {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    std::string name;
};

template <typename ReadU32>
VoidResult validate_entries(const std::vector<PakEntry>& entries, std::uint64_t file_size,
                            std::string_view archive, ReadU32&& read_u32) {
    std::unordered_set<std::string> names;
    names.reserve(entries.size());
    std::vector<EntryExtent> extents;
    extents.reserve(entries.size());
    for (const auto& entry : entries) {
        const std::string key = pak_entry_key(entry.name);
        if (!names.insert(key).second)
            return format_error(archive, "duplicate entry name '" + entry.name + "'");
        if (entry.offset > file_size || file_size - entry.offset < 4)
            return format_error(archive, "entry '" + entry.name + "' at offset " +
                                             std::to_string(entry.offset) +
                                             " has no payload header");
        std::uint32_t mode = 0;
        if (!read_u32(entry.offset, mode))
            return format_error(archive, "cannot read payload mode for '" + entry.name +
                                             "' at offset " + std::to_string(entry.offset));
        std::uint64_t end = 0;
        if (mode == 0) {
            end = static_cast<std::uint64_t>(entry.offset) + 4 + entry.size;
        } else {
            if (entry.offset > file_size || file_size - entry.offset < 8)
                return format_error(archive, "entry '" + entry.name + "' at offset " +
                                                 std::to_string(entry.offset) +
                                                 " has a truncated stored size");
            std::uint32_t stored = 0;
            if (!read_u32(static_cast<std::uint64_t>(entry.offset) + 4, stored))
                return format_error(archive, "cannot read stored size for '" + entry.name +
                                                 "' at offset " + std::to_string(entry.offset));
            if (stored > kMaxEntrySize)
                return format_error(archive, "entry '" + entry.name + "' at offset " +
                                                 std::to_string(entry.offset) +
                                                 " has an oversized stored payload");
            end = static_cast<std::uint64_t>(entry.offset) + 8 + stored;
        }
        if (end > file_size)
            return format_error(archive, "entry '" + entry.name + "' at offset " +
                                             std::to_string(entry.offset) + " ends at " +
                                             std::to_string(end) + " beyond archive size " +
                                             std::to_string(file_size));
        extents.push_back({entry.offset, end, entry.name});
    }
    std::sort(extents.begin(), extents.end(), [](const EntryExtent& a, const EntryExtent& b) {
        if (a.begin != b.begin) return a.begin < b.begin;
        return a.end < b.end;
    });
    for (std::size_t i = 1; i < extents.size(); ++i) {
        if (extents[i].begin < extents[i - 1].end ||
            extents[i].begin == extents[i - 1].begin)
            return format_error(archive, "overlapping entries '" + extents[i - 1].name +
                                             "' and '" + extents[i].name + "'");
    }
    return VoidResult{std::monostate{}};
}
}

std::string pak_entry_key(std::string_view name) { return normalize_virtual_path(name); }

const PakEntry* PakArchive::find(std::string_view name) const {
    const std::string key = pak_entry_key(name);
    for (const auto& e : entries_) if (pak_entry_key(e.name) == key) return &e;
    return nullptr;
}

std::string PakArchive::describe_entry(const PakEntry& entry) const {
    return archive_label(path_.string()) + ": entry '" + entry.name + "' at offset " +
           std::to_string(entry.offset);
}

Result<PakArchive> PakArchive::parse_memory(std::span<const std::uint8_t> data) {
    ByteReader reader(data);
    const std::uint64_t file_size = data.size();
    constexpr std::string_view kMemorySource = "<memory>";
    std::span<const std::uint8_t> magic;
    if (!reader.read_bytes(8, magic)) return format_error(kMemorySource, "truncated header");
    if (std::memcmp(magic.data(), "PAK \0\0\0\0", 8) != 0)
        return format_error(kMemorySource, "bad PAK magic");
    std::uint32_t count = 0;
    if (!reader.read_u32le(count)) return format_error(kMemorySource, "missing file count");
    if (count > 1'000'000) return format_error(kMemorySource, "unreasonable file count");
    PakArchive archive;
    archive.file_size_ = file_size;
    archive.entries_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t name_len = 0;
        if (!reader.read_u32le(name_len) || name_len == 0 || name_len > 4096)
            return format_error(kMemorySource, "bad entry name length at entry #" +
                                                    std::to_string(i));
        std::span<const std::uint8_t> name;
        if (!reader.read_bytes(name_len, name))
            return format_error(kMemorySource, "truncated entry name at entry #" +
                                                    std::to_string(i));
        std::uint32_t size = 0, offset = 0;
        if (!reader.read_u32le(size) || !reader.read_u32le(offset))
            return format_error(kMemorySource, "truncated entry header at entry #" +
                                                    std::to_string(i));
        if (size > kMaxEntrySize)
            return format_error(kMemorySource, "entry too large at entry #" +
                                                    std::to_string(i));
        archive.entries_.push_back(
            {std::string(reinterpret_cast<const char*>(name.data()), name.size()), size, offset});
    }
    auto read_u32 = [data](std::uint64_t pos, std::uint32_t& out) {
        if (pos > data.size() || data.size() - static_cast<std::size_t>(pos) < 4) return false;
        const std::size_t p = static_cast<std::size_t>(pos);
        out = std::uint32_t(data[p]) | (std::uint32_t(data[p + 1]) << 8) |
              (std::uint32_t(data[p + 2]) << 16) | (std::uint32_t(data[p + 3]) << 24);
        return true;
    };
    auto valid = validate_entries(archive.entries_, file_size, kMemorySource, read_u32);
    if (!valid) return valid.error();
    return archive;
}

Result<PakArchive> PakArchive::open_from_memory(Bytes data) {
    auto parsed = parse_memory(data);
    if (!parsed) return parsed;
    parsed->memory_ = std::move(data);
    parsed->in_memory_ = true;
    return parsed;
}

Result<PakArchive> PakArchive::open_from_file(const std::filesystem::path& path) {
    std::error_code ec;
    const std::uint64_t file_size = std::filesystem::file_size(path, ec);
    if (ec) return Error{ErrorCode::io, "cannot stat PAK: " + path.string()};
    std::ifstream in(path, std::ios::binary);
    if (!in) return Error{ErrorCode::io, "cannot open PAK: " + path.string()};
    const std::string source = path.string();
    char magic[8];
    if (!in.read(magic, 8) || std::memcmp(magic, "PAK \0\0\0\0", 8) != 0)
        return format_error(source, "bad PAK magic");
    std::uint32_t count = 0;
    if (!read_stream_u32(in, count) || count > 1'000'000)
        return format_error(source, "bad PAK file count");
    PakArchive archive;
    archive.path_ = path;
    archive.file_size_ = file_size;
    archive.entries_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t name_len = 0;
        if (!read_stream_u32(in, name_len) || name_len == 0 || name_len > 4096)
            return format_error(source, "bad entry name length at entry #" + std::to_string(i));
        std::string name(name_len, '\0');
        if (!in.read(name.data(), name_len))
            return format_error(source, "truncated entry name at entry #" + std::to_string(i));
        std::uint32_t size = 0, offset = 0;
        if (!read_stream_u32(in, size) || !read_stream_u32(in, offset))
            return format_error(source, "truncated entry header at entry #" + std::to_string(i));
        if (size > kMaxEntrySize)
            return format_error(source, "entry too large at entry #" + std::to_string(i));
        archive.entries_.push_back({std::move(name), size, offset});
    }
    auto read_u32 = [&in](std::uint64_t pos, std::uint32_t& out) {
        in.clear();
        in.seekg(static_cast<std::streamoff>(pos));
        if (!in) return false;
        return read_stream_u32(in, out);
    };
    auto valid = validate_entries(archive.entries_, file_size, source, read_u32);
    if (!valid) return valid.error();
    return archive;
}

Result<Bytes> PakArchive::read(const PakEntry& entry) const {
    if (entry.size > kMaxEntrySize)
        return Error{ErrorCode::format, describe_entry(entry) + ": entry too large"};
    if (in_memory_) {
        const std::size_t offset = entry.offset;
        if (offset > memory_.size() || memory_.size() - offset < 4)
            return Error{ErrorCode::format, describe_entry(entry) + ": truncated entry payload"};
        ByteReader r(std::span<const std::uint8_t>(memory_).subspan(offset));
        std::uint32_t compressed = 0;
        if (!r.read_u32le(compressed))
            return Error{ErrorCode::format, describe_entry(entry) + ": missing compression flag"};
        if (compressed == 0) {
            if (r.remaining().size() < entry.size)
                return Error{ErrorCode::format, describe_entry(entry) + ": truncated raw entry"};
            return Bytes(r.remaining().begin(), r.remaining().begin() + entry.size);
        }
        std::uint32_t stored = 0;
        if (!r.read_u32le(stored))
            return Error{ErrorCode::format, describe_entry(entry) + ": missing stored size"};
        if (stored > kMaxEntrySize)
            return Error{ErrorCode::format, describe_entry(entry) + ": entry too large"};
        if (r.remaining().size() < stored)
            return Error{ErrorCode::format,
                         describe_entry(entry) + ": truncated compressed entry"};
        auto inflated = inflate_zlib(r.remaining().first(stored), entry.size);
        if (!inflated)
            return Error{inflated.error().code,
                         describe_entry(entry) + ": " + inflated.error().message};
        return inflated;
    }

    std::ifstream in(path_, std::ios::binary);
    if (!in) return Error{ErrorCode::io, describe_entry(entry) + ": cannot open PAK: " + path_.string()};
    in.seekg(static_cast<std::streamoff>(entry.offset));
    if (!in)
        return Error{ErrorCode::io, describe_entry(entry) + ": cannot seek PAK: " + path_.string()};
    std::uint32_t compressed = 0;
    if (!read_stream_u32(in, compressed))
        return Error{ErrorCode::format, describe_entry(entry) + ": truncated entry payload"};
    if (compressed == 0) {
        Bytes out(entry.size);
        if (!out.empty() &&
            !in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size())))
            return Error{ErrorCode::format, describe_entry(entry) + ": truncated entry payload"};
        return out;
    }
    std::uint32_t stored = 0;
    if (!read_stream_u32(in, stored))
        return Error{ErrorCode::format, describe_entry(entry) + ": truncated entry payload"};
    if (stored > kMaxEntrySize)
        return Error{ErrorCode::format, describe_entry(entry) + ": entry too large"};
    Bytes payload(stored);
    if (!payload.empty() &&
        !in.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size())))
        return Error{ErrorCode::format, describe_entry(entry) + ": truncated entry payload"};
    auto inflated = inflate_zlib(std::span<const std::uint8_t>(payload), entry.size);
    if (!inflated)
        return Error{inflated.error().code,
                     describe_entry(entry) + ": " + inflated.error().message};
    return inflated;
}
}
