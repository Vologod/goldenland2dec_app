#include "gl/assets/pad.hpp"
#include "gl/assets/byte_reader.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <utility>

namespace gl::assets {
namespace {
constexpr std::size_t kHeaderSize = 12;
constexpr std::size_t kAnimationFixedSize = 4 + 32 + 4 + 16 + 4;
constexpr std::size_t kMaxTableSize = 64U * 1024U * 1024U;
constexpr std::size_t kMaxRows = 65536;
constexpr std::size_t kMaxFramesPerRow = 65536;
constexpr std::uint32_t kKnownTypeMask = 0x000F05FFU;

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

bool is_known_type(std::uint32_t type) {
    switch (type) {
    case 0x00000001:
    case 0x00000002:
    case 0x00000004:
    case 0x00000008:
    case 0x00000010:
    case 0x00000020:
    case 0x00000040:
    case 0x00000080:
    case 0x00000100:
    case 0x00000400:
    case 0x00010000:
    case 0x00020000:
    case 0x00040000:
    case 0x00080000:
        return true;
    default:
        return false;
    }
}

bool read_i16(ByteReader &reader, std::int16_t &out) {
    std::uint16_t raw = 0;
    if (!reader.read_u16le(raw))
        return false;
    out = std::bit_cast<std::int16_t>(raw);
    return true;
}

bool read_i32(ByteReader &reader, std::int32_t &out) {
    std::uint32_t raw = 0;
    if (!reader.read_u32le(raw))
        return false;
    out = std::bit_cast<std::int32_t>(raw);
    return true;
}

bool read_f32(ByteReader &reader, float &out) {
    std::uint32_t raw = 0;
    if (!reader.read_u32le(raw))
        return false;
    out = std::bit_cast<float>(raw);
    return true;
}

VoidResult read_crop_table(ByteReader &reader, std::size_t rows, std::size_t frames,
                           std::size_t animation_end, std::vector<std::vector<PadCrop>> &crops) {
    const std::size_t table_size = rows * frames * 8;
    if (reader.position() > animation_end || animation_end - reader.position() < table_size)
        return format_error("pad crop table exceeds the animation payload");

    crops.reserve(rows);
    for (std::size_t row = 0; row < rows; ++row) {
        std::vector<PadCrop> frames_row;
        frames_row.reserve(frames);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            PadCrop crop;
            if (!read_i16(reader, crop.x) || !read_i16(reader, crop.y) ||
                !read_i16(reader, crop.w) || !read_i16(reader, crop.h))
                return format_error("truncated pad crop entry");
            frames_row.push_back(crop);
        }
        crops.push_back(std::move(frames_row));
    }
    return VoidResult{std::monostate{}};
}

Result<std::size_t> crop_rows(std::uint32_t table_size, std::int32_t frames_per_row,
                              std::size_t animation_end, std::size_t position) {
    if (table_size > kMaxTableSize)
        return format_error("pad crop table is too large");
    if (position > animation_end || table_size > animation_end - position)
        return format_error("pad crop table exceeds the animation payload");
    if (frames_per_row <= 0 || static_cast<std::size_t>(frames_per_row) > kMaxFramesPerRow)
        return format_error("invalid pad frames per row");

    const std::size_t row_size = 8U * static_cast<std::size_t>(frames_per_row);
    if (table_size % row_size != 0)
        return format_error("pad crop table size is not a multiple of the row size");
    const std::size_t rows = table_size / row_size;
    if (rows > kMaxRows)
        return format_error("pad crop table has too many rows");
    return rows;
}
} // namespace

Result<std::vector<PadAnimation>> parse_pad(std::span<const std::uint8_t> data) {
    try {
        if (data.size() < kHeaderSize)
            return format_error("pad data is shorter than its header");

        ByteReader header(data);
        std::span<const std::uint8_t> magic;
        if (!header.read_bytes(4, magic) || std::memcmp(magic.data(), "PAD ", 4) != 0)
            return format_error("bad pad magic");

        std::uint32_t data_size = 0;
        std::uint32_t animation_masks = 0;
        if (!header.read_u32le(data_size) || !header.read_u32le(animation_masks))
            return format_error("truncated pad header");
        if (data_size < kHeaderSize)
            return format_error("pad data size is smaller than its header");
        if (data_size > data.size())
            return format_error("pad data size exceeds the input size");
        if ((animation_masks & ~kKnownTypeMask) != 0)
            return format_error("pad header contains an unknown animation mask");

        std::vector<PadAnimation> animations;
        if (animation_masks == 0) {
            if (data_size != kHeaderSize)
                return format_error("pad has animation data but no animation masks");
            return animations;
        }

        ByteReader reader(data.first(data_size));
        reader.skip(kHeaderSize);
        std::uint32_t pending_masks = animation_masks;
        animations.reserve(static_cast<std::size_t>(std::popcount(animation_masks)));

        while (pending_masks != 0) {
            if (reader.remaining().size() < 8)
                return format_error("truncated pad animation header");

            std::uint32_t raw_type = 0;
            if (!reader.read_u32le(raw_type))
                return format_error("truncated pad animation type");
            if (!is_known_type(raw_type))
                return format_error("unknown pad animation type " + std::to_string(raw_type));
            if ((animation_masks & raw_type) == 0)
                return format_error("pad animation type is missing from the header mask");
            if ((pending_masks & raw_type) == 0)
                return format_error("duplicate pad animation type");

            const std::size_t animation_start = reader.position();
            std::int32_t animation_size = 0;
            if (!read_i32(reader, animation_size))
                return format_error("truncated pad animation size");
            if (animation_size < static_cast<std::int32_t>(kAnimationFixedSize))
                return format_error("pad animation size is too small");
            const std::uint64_t animation_end_wide = static_cast<std::uint64_t>(animation_start) +
                                                     static_cast<std::uint64_t>(animation_size);
            if (animation_end_wide > data_size)
                return format_error("pad animation exceeds the data size");
            const std::size_t animation_end = static_cast<std::size_t>(animation_end_wide);

            PadAnimation animation;
            animation.type = static_cast<PadAnimationType>(raw_type);
            if (!read_i32(reader, animation.delay) || !read_i32(reader, animation.frames_per_row) ||
                !read_i32(reader, animation.frame_width) ||
                !read_i32(reader, animation.frame_height) ||
                !read_i32(reader, animation.anchor_x) || !read_i32(reader, animation.anchor_y) ||
                !read_f32(reader, animation.movement_x) || !read_f32(reader, animation.movement_y))
                return format_error("truncated pad animation fields");

            std::uint32_t crops_table_size = 0;
            if (!reader.read_u32le(crops_table_size))
                return format_error("truncated pad crop table size");
            auto rows = crop_rows(crops_table_size, animation.frames_per_row, animation_end,
                                  reader.position());
            if (!rows)
                return rows.error();
            auto crops = read_crop_table(reader, rows.value(),
                                         static_cast<std::size_t>(animation.frames_per_row),
                                         animation_end, animation.crops);
            if (!crops)
                return crops.error();

            if (reader.position() > animation_end || animation_end - reader.position() < 16)
                return format_error("truncated pad shadow frame");
            if (!read_i32(reader, animation.shadow_width) ||
                !read_i32(reader, animation.shadow_height) ||
                !read_i32(reader, animation.shadow_anchor_x) ||
                !read_i32(reader, animation.shadow_anchor_y))
                return format_error("truncated pad shadow frame");

            std::uint32_t shadow_table_size = 0;
            if (!reader.read_u32le(shadow_table_size))
                return format_error("truncated pad shadow crop table size");
            auto shadow_rows = crop_rows(shadow_table_size, animation.frames_per_row, animation_end,
                                         reader.position());
            if (!shadow_rows)
                return shadow_rows.error();
            auto shadow_crops = read_crop_table(reader, shadow_rows.value(),
                                                static_cast<std::size_t>(animation.frames_per_row),
                                                animation_end, animation.shadow_crops);
            if (!shadow_crops)
                return shadow_crops.error();

            if (reader.position() != animation_end)
                return format_error("pad animation size does not match its payload");

            pending_masks &= ~raw_type;
            animations.push_back(std::move(animation));
        }

        return animations;
    } catch (const std::bad_alloc &) {
        return format_error("pad allocation failed");
    }
}

bool pad_is_go(PadAnimationType type) {
    return type == PadAnimationType::tb_go || type == PadAnimationType::rt_go;
}

PadDirectionRow pad_person_row(PadAnimationType type, std::uint8_t direction) {
    if (pad_is_go(type)) {
        const std::size_t index = direction % kPadPersonRows16.size();
        const std::uint8_t row = kPadPersonRows16[index];
        return {row, index != row};
    }
    const std::size_t index = direction % kPadPersonRows8.size();
    const std::uint8_t row = kPadPersonRows8[index];
    return {row, index != row};
}

PadDirectionRow pad_shadow_row(PadAnimationType type, std::uint8_t direction) {
    if (pad_is_go(type))
        return {kPadShadowRows16[direction % kPadShadowRows16.size()], false};
    return {kPadShadowRows8[direction % kPadShadowRows8.size()], false};
}
} // namespace gl::assets
