#include "gl/assets/sdb.hpp"
#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"
#include <cstring>

namespace gl::assets {
Result<Sdb> Sdb::parse(std::span<const std::uint8_t> data) {
    ByteReader r(data);
    Sdb sdb;
    if (data.size() >= 4 && std::memcmp(data.data(), "SDB ", 4) == 0) {
        sdb.had_signature_ = true;
        r.skip(4);
    }
    while (!r.at_end()) {
        std::uint32_t id = 0, len = 0;
        if (!r.read_u32le(id) || !r.read_u32le(len))
            return Error{ErrorCode::format, "truncated SDB entry"};
        std::span<const std::uint8_t> bytes;
        if (!r.read_bytes(len, bytes))
            return Error{ErrorCode::format, "truncated SDB string"};
        std::vector<std::uint8_t> tmp(bytes.begin(), bytes.end());
        if (!sdb.had_signature_)
            for (auto& b : tmp) b ^= 0xAA;
        sdb.entries_.push_back({id, cp1251_to_utf8(tmp)});
    }
    return sdb;
}

const std::string* Sdb::find(std::uint32_t id) const {
    for (const auto& e : entries_) if (e.id == id) return &e.value;
    return nullptr;
}
}
