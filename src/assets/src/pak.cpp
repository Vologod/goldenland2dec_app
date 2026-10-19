#include "gl/assets/pak.hpp"
#include "gl/assets/cp1251.hpp"
#include <cstring>
#include <fstream>
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
Result<Bytes> read_payload(std::span<const std::uint8_t> payload, std::uint64_t entry_size) {
    if (entry_size < 4) return Error{ErrorCode::format, "entry too small"};
    ByteReader r(payload);
    std::uint32_t compressed = 0;
    if (!r.read_u32le(compressed)) return Error{ErrorCode::format, "missing compression flag"};
    if (compressed == 0) {
        std::uint32_t unpacked = 0;
        if (!r.read_u32le(unpacked)) return Error{ErrorCode::format, "missing unpacked size"};
        if (unpacked > kMaxEntrySize) return Error{ErrorCode::format, "entry too large"};
        Bytes out(unpacked);
        mz_ulong out_len = unpacked;
        int status = mz_uncompress(out.data(), &out_len, r.remaining().data(), r.remaining().size());
        if (status != MZ_OK || out_len != unpacked)
            return Error{ErrorCode::format, "zlib decompression failed"};
        return out;
    }
    Bytes out(r.remaining().begin(), r.remaining().end());
    if (out.size() > kMaxEntrySize) return Error{ErrorCode::format, "entry too large"};
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
        if (size < 4 || std::uint64_t(offset) + size > file_size)
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
        if (size < 4 || std::uint64_t(offset) + size > file_size)
            return Error{ErrorCode::format, "entry outside archive"};
        archive.entries_.push_back({std::move(name), size, offset});
    }
    return archive;
}

Result<Bytes> PakArchive::read(const PakEntry& entry) const {
    if (entry.size < 4 || entry.size > kMaxEntrySize) return Error{ErrorCode::format, "bad entry size"};
    Bytes payload(entry.size);
    if (in_memory_) {
        if (std::uint64_t(entry.offset) + entry.size > memory_.size())
            return Error{ErrorCode::format, "entry outside archive"};
        std::memcpy(payload.data(), memory_.data() + entry.offset, entry.size);
    } else {
        std::ifstream in(path_, std::ios::binary);
        if (!in) return Error{ErrorCode::io, "cannot open PAK: " + path_.string()};
        in.seekg(static_cast<std::streamoff>(entry.offset));
        if (!in.read(reinterpret_cast<char*>(payload.data()), entry.size))
            return Error{ErrorCode::format, "truncated entry payload"};
    }
    return read_payload(payload, entry.size);
}
}
