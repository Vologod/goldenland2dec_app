#include "gl/assets/pak.hpp"
#include "gl/assets/cp1251.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <istream>
#include <span>
#include <string_view>
#include <utility>
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
}

std::string pak_entry_key(std::string_view name) { return normalize_virtual_path(name); }

const PakEntry* PakArchive::find(std::string_view name) const {
    const std::string key = pak_entry_key(name);
    for (const auto& e : entries_) if (pak_entry_key(e.name) == key) return &e;
    return nullptr;
}

Result<PakArchive> PakArchive::parse_memory(ByteReader& reader, std::uint64_t file_size) {
    std::span<const std::uint8_t> magic;
    if (!reader.read_bytes(8, magic)) return Error{ErrorCode::format, "truncated header"};
    if (std::memcmp(magic.data(), "PAK \0\0\0\0", 8) != 0)
        return Error{ErrorCode::format, "bad PAK magic"};
    std::uint32_t count = 0;
    if (!reader.read_u32le(count)) return Error{ErrorCode::format, "missing file count"};
    if (count > 1'000'000) return Error{ErrorCode::format, "unreasonable file count"};
    PakArchive archive;
    archive.file_size_ = file_size;
    archive.entries_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t name_len = 0;
        if (!reader.read_u32le(name_len) || name_len == 0 || name_len > 4096)
            return Error{ErrorCode::format, "bad entry name length"};
        std::span<const std::uint8_t> name;
        if (!reader.read_bytes(name_len, name))
            return Error{ErrorCode::format, "truncated entry name"};
        std::uint32_t size = 0, offset = 0;
        if (!reader.read_u32le(size) || !reader.read_u32le(offset))
            return Error{ErrorCode::format, "truncated entry header"};
        if (offset >= file_size || size > kMaxEntrySize)
            return Error{ErrorCode::format, "entry outside archive"};
        archive.entries_.push_back({std::string(reinterpret_cast<const char*>(name.data()), name.size()),
                                    size, offset});
    }
    return archive;
}

Result<PakArchive> PakArchive::open_from_memory(Bytes data) {
    ByteReader reader(data);
    auto parsed = parse_memory(reader, data.size());
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
    char magic[8];
    if (!in.read(magic, 8) || std::memcmp(magic, "PAK \0\0\0\0", 8) != 0)
        return Error{ErrorCode::format, "bad PAK magic"};
    std::uint32_t count = 0;
    if (!read_stream_u32(in, count) || count > 1'000'000)
        return Error{ErrorCode::format, "bad PAK file count"};
    PakArchive archive;
    archive.path_ = path;
    archive.file_size_ = file_size;
    archive.entries_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t name_len = 0;
        if (!read_stream_u32(in, name_len) || name_len == 0 || name_len > 4096)
            return Error{ErrorCode::format, "bad entry name length"};
        std::string name(name_len, '\0');
        if (!in.read(name.data(), name_len))
            return Error{ErrorCode::format, "truncated entry name"};
        std::uint32_t size = 0, offset = 0;
        if (!read_stream_u32(in, size) || !read_stream_u32(in, offset))
            return Error{ErrorCode::format, "truncated entry header"};
        if (offset >= file_size || size > kMaxEntrySize)
            return Error{ErrorCode::format, "entry outside archive"};
        archive.entries_.push_back({std::move(name), size, offset});
    }
    return archive;
}

Result<Bytes> PakArchive::read(const PakEntry& entry) const {
    if (entry.size > kMaxEntrySize) return Error{ErrorCode::format, "entry too large"};
    if (in_memory_) {
        const std::size_t offset = entry.offset;
        if (offset > memory_.size() || memory_.size() - offset < 4)
            return Error{ErrorCode::format, "truncated entry payload"};
        ByteReader r(std::span<const std::uint8_t>(memory_).subspan(offset));
        std::uint32_t compressed = 0;
        if (!r.read_u32le(compressed)) return Error{ErrorCode::format, "missing compression flag"};
        if (compressed == 0) {
            if (r.remaining().size() < entry.size)
                return Error{ErrorCode::format, "truncated raw entry"};
            return Bytes(r.remaining().begin(), r.remaining().begin() + entry.size);
        }
        std::uint32_t stored = 0;
        if (!r.read_u32le(stored)) return Error{ErrorCode::format, "missing stored size"};
        if (stored > kMaxEntrySize) return Error{ErrorCode::format, "entry too large"};
        if (r.remaining().size() < stored)
            return Error{ErrorCode::format, "truncated compressed entry"};
        return inflate_zlib(r.remaining().first(stored), entry.size);
    }

    std::ifstream in(path_, std::ios::binary);
    if (!in) return Error{ErrorCode::io, "cannot open PAK: " + path_.string()};
    in.seekg(static_cast<std::streamoff>(entry.offset));
    if (!in) return Error{ErrorCode::io, "cannot seek PAK: " + path_.string()};
    std::uint32_t compressed = 0;
    if (!read_stream_u32(in, compressed))
        return Error{ErrorCode::format, "truncated entry payload"};
    if (compressed == 0) {
        Bytes out(entry.size);
        if (!out.empty() &&
            !in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size())))
            return Error{ErrorCode::format, "truncated entry payload"};
        return out;
    }
    std::uint32_t stored = 0;
    if (!read_stream_u32(in, stored)) return Error{ErrorCode::format, "truncated entry payload"};
    if (stored > kMaxEntrySize) return Error{ErrorCode::format, "entry too large"};
    Bytes payload(stored);
    if (!payload.empty() &&
        !in.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size())))
        return Error{ErrorCode::format, "truncated entry payload"};
    return inflate_zlib(std::span<const std::uint8_t>(payload), entry.size);
}
}
