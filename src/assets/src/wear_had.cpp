#include "gl/assets/wear_had.hpp"
#include "gl/assets/byte_reader.hpp"

#include <bit>
#include <cstring>
#include <string>

namespace gl::assets {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

bool known_mask(std::uint32_t mask) {
    switch (mask) {
    case 0x1:
    case 0x2:
    case 0x4:
    case 0x8:
    case 0x10:
    case 0x20:
    case 0x40:
    case 0x80:
    case 0x100:
    case 0x200:
    case 0x400:
    case 0x10000:
    case 0x20000:
    case 0x40000:
    case 0x80000:
        return true;
    default:
        return false;
    }
}

std::uint32_t rows_for_mask(std::uint32_t mask) {
    return (mask == 0x10 || mask == 0x20 || mask == 0x200) ? 9 : 5;
}

std::int32_t to_i32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
} // namespace

Result<std::vector<WearHadAnimation>> parse_wear_had(std::span<const std::uint8_t> data) {
    if (data.size() < 0x10)
        return format_error("wear had is shorter than its header");
    if (std::memcmp(data.data(), "HAD ", 4) != 0)
        return format_error("bad wear had magic");

    std::uint32_t end = 0;
    std::memcpy(&end, data.data() + 4, sizeof(end));
    if (end < 0xC || end > data.size())
        return format_error("invalid wear had data end");

    std::vector<WearHadAnimation> out;
    std::size_t offset = 0xC;
    while (offset + 8 <= end) {
        std::uint32_t mask = 0, size = 0;
        std::memcpy(&mask, data.data() + offset, sizeof(mask));
        std::memcpy(&size, data.data() + offset + 4, sizeof(size));
        if (!known_mask(mask))
            return format_error("unknown wear had animation mask");
        const std::uint64_t record_end =
            static_cast<std::uint64_t>(offset) + 4 + static_cast<std::uint64_t>(size);
        if (size < 4 + 44 + 28 || (size % 4) != 0 || record_end > end)
            return format_error("invalid wear had record size");

        ByteReader r(data.subspan(offset + 8, size - 4));
        WearHadAnimation a;
        a.type_mask = mask;
        a.rows = rows_for_mask(mask);
        std::uint32_t raw = 0;
        if (!r.read_u32le(a.delay_ms) || !r.read_u32le(a.columns) || !r.read_u32le(raw))
            return format_error("truncated wear had fields");
        a.layer_offset_x = to_i32(raw);
        if (!r.read_u32le(raw))
            return format_error("truncated wear had fields");
        a.layer_offset_y = to_i32(raw);
        if (!r.read_u32le(a.frame_width) || !r.read_u32le(a.frame_height) || !r.read_u32le(raw))
            return format_error("truncated wear had fields");
        a.anchor_x = to_i32(raw);
        if (!r.read_u32le(raw))
            return format_error("truncated wear had fields");
        a.anchor_y = to_i32(raw);
        if (!r.read_u32le(raw))
            return format_error("truncated wear had fields");
        a.movement_x = std::bit_cast<float>(raw);
        if (!r.read_u32le(raw))
            return format_error("truncated wear had fields");
        a.movement_y = std::bit_cast<float>(raw);

        std::uint32_t crops_bytes = 0;
        if (!r.read_u32le(crops_bytes) || (crops_bytes % 8) != 0)
            return format_error("invalid wear had body crop table");
        if (!r.skip(crops_bytes))
            return format_error("truncated wear had body crop table");

        if (!r.read_u32le(raw))
            return format_error("truncated wear had shadow");
        a.shadow_offset_x = to_i32(raw);
        if (!r.read_u32le(raw))
            return format_error("truncated wear had shadow");
        a.shadow_offset_y = to_i32(raw);
        if (!r.read_u32le(a.shadow_width) || !r.read_u32le(a.shadow_height) || !r.read_u32le(raw))
            return format_error("truncated wear had shadow");
        a.shadow_anchor_x = to_i32(raw);
        if (!r.read_u32le(raw))
            return format_error("truncated wear had shadow");
        a.shadow_anchor_y = to_i32(raw);
        std::uint32_t shadow_bytes = 0;
        if (!r.read_u32le(shadow_bytes) || (shadow_bytes % 8) != 0)
            return format_error("invalid wear had shadow crop table");
        if (!r.skip(shadow_bytes))
            return format_error("truncated wear had shadow crop table");
        if (!r.at_end())
            return format_error("wear had record has trailing bytes");

        if (a.columns == 0 || a.frame_width == 0 || a.frame_height == 0)
            return format_error("wear had animation has empty frame");
        out.push_back(a);
        offset = static_cast<std::size_t>(record_end);
    }
    if (offset != end)
        return format_error("wear had records do not fill the data");
    if (out.empty())
        return format_error("wear had contains no animations");
    return out;
}
} // namespace gl::assets
