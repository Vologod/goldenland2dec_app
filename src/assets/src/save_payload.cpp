#include "gl/assets/save_payload.hpp"

#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"

#include <miniz.h>

#include <array>
#include <cstring>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace gl::assets {
namespace {
constexpr std::array<std::string_view, 7> kBlockOrder = {
    "BT_SCRN ", "BT_INFO ", "BT_VARS ", "BT_TRIBE", "BT_DIARY", "BT_HSTRY", "BT_HERO "};

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

std::uint32_t u32_at(std::span<const std::uint8_t> data, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

bool read_string(ByteReader &reader, std::string &out) {
    std::uint32_t length = 0;
    if (!reader.read_u32le(length))
        return false;
    std::span<const std::uint8_t> bytes;
    if (!reader.read_bytes(length, bytes))
        return false;
    out = cp1251_to_utf8(bytes);
    return true;
}
} // namespace

Result<SavePayload> parse_save_payload(std::span<const std::uint8_t> file) {
    try {
        auto header = parse_save_header(file);
        if (!header)
            return header.error();
        if (file.size() < 0x70)
            return format_error("save payload is truncated");

        const std::uint32_t raw_len = u32_at(file, 0x68);
        const std::uint32_t compressed_len = u32_at(file, 0x6c);
        if (raw_len == 0 || compressed_len == 0 ||
            static_cast<std::size_t>(compressed_len) > file.size() - 0x70)
            return format_error("save payload zlib length is invalid");

        SavePayload payload;
        payload.header = header.value();
        payload.decompressed.resize(raw_len);
        mz_ulong out_len = raw_len;
        if (mz_uncompress(payload.decompressed.data(), &out_len, file.data() + 0x70,
                          compressed_len) != MZ_OK)
            return format_error("save payload zlib decompression failed");
        if (out_len != raw_len)
            return format_error("save payload uncompressed length mismatch");

        ByteReader blocks(payload.decompressed);
        for (std::size_t index = 0; index < kBlockOrder.size(); ++index) {
            std::span<const std::uint8_t> tag;
            if (!blocks.read_bytes(8, tag))
                return format_error("save payload block tag is truncated");
            const std::string tag_text(reinterpret_cast<const char *>(tag.data()), tag.size());
            if (tag_text != kBlockOrder[index])
                return format_error("save payload block order mismatch");
            std::uint32_t size = 0;
            if (!blocks.read_u32le(size))
                return format_error("save payload block size is truncated");
            std::span<const std::uint8_t> body;
            if (!blocks.read_bytes(size, body))
                return format_error("save payload block body is truncated");
            payload.blocks.push_back(SaveBlockView{
                tag_text, static_cast<std::uint32_t>(body.data() - payload.decompressed.data()),
                size});
        }
        if (!blocks.at_end())
            return format_error("save payload has trailing bytes");

        const SaveBlockView &info_view = payload.blocks[1];
        ByteReader info(std::span<const std::uint8_t>(payload.decompressed)
                            .subspan(info_view.offset, info_view.size));
        std::string session;
        std::string description;
        if (!read_string(info, session) || !info.read_u32le(payload.info.field_218))
            return format_error("BT_INFO header is truncated");
        std::span<const std::uint8_t> time;
        if (!info.read_bytes(payload.info.time.size(), time))
            return format_error("BT_INFO time blob is truncated");
        std::memcpy(payload.info.time.data(), time.data(), time.size());
        if (!read_string(info, description) || !read_string(info, payload.info.level_id))
            return format_error("BT_INFO strings are truncated");
        if (payload.info.level_id.empty())
            return format_error("BT_INFO level id is empty");
        std::uint32_t party = 0;
        if (!info.read_u32le(party))
            return format_error("BT_INFO party level is truncated");
        payload.info.party_level = static_cast<std::int32_t>(party);
        std::span<const std::uint8_t> tail8;
        if (!info.read_bytes(payload.info.field_26c8.size(), tail8))
            return format_error("BT_INFO tail is truncated");
        std::memcpy(payload.info.field_26c8.data(), tail8.data(), tail8.size());
        if (!info.at_end())
            return format_error("BT_INFO has trailing bytes");
        return payload;
    } catch (const std::bad_alloc &) {
        return format_error("save payload allocation failed");
    }
}
} // namespace gl::assets
