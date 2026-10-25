#include "gl/assets/iad.hpp"
#include "gl/assets/byte_reader.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace gl::assets {
namespace {
constexpr std::size_t kHeaderSize = 32;
constexpr std::uint32_t kMaxTableSize = 1U << 20U;

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

bool read_u32_at(const std::vector<std::uint8_t> &data, std::size_t offset, std::uint32_t &out) {
    if (offset + 4 > data.size())
        return false;
    std::memcpy(&out, data.data() + offset, sizeof(out));
    return true;
}

bool read_i16_at(const std::vector<std::uint8_t> &data, std::size_t offset, std::int16_t &out) {
    std::uint16_t raw = 0;
    if (offset + 2 > data.size())
        return false;
    std::memcpy(&raw, data.data() + offset, sizeof(raw));
    out = std::bit_cast<std::int16_t>(raw);
    return true;
}

std::int32_t to_i32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
} // namespace

Result<std::vector<IadBlock>> parse_iad(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 8)
        return format_error("iad data is shorter than its header");
    if (std::memcmp(bytes.data(), "IAD ", 4) != 0)
        return format_error("bad iad magic");

    const std::vector<std::uint8_t> data(bytes.begin(), bytes.end());
    std::uint32_t version = 0;
    if (!read_u32_at(data, 4, version) || version != 1) {
        return format_error("unsupported iad version");
    }

    std::vector<IadBlock> blocks;
    std::size_t offset = 4;
    while (offset + kHeaderSize <= data.size()) {
        IadBlock block;
        if (!read_u32_at(data, offset, block.type) || !read_u32_at(data, offset + 4, block.size)) {
            return format_error("truncated iad block header");
        }
        if (block.size < kHeaderSize)
            return format_error("invalid iad block size");

        const std::uint64_t block_end_wide = static_cast<std::uint64_t>(offset) + 4 + block.size;
        if (block_end_wide > data.size())
            return format_error("iad block exceeds the input");
        const std::size_t block_end = static_cast<std::size_t>(block_end_wide);

        std::uint32_t raw = 0;
        if (!read_u32_at(data, offset + 8, block.frames) ||
            !read_u32_at(data, offset + 12, block.frame_width) ||
            !read_u32_at(data, offset + 16, block.frame_height) ||
            !read_u32_at(data, offset + 20, raw)) {
            return format_error("truncated iad block fields");
        }
        block.layer_offset_x = to_i32(raw);
        if (!read_u32_at(data, offset + 24, raw)) {
            return format_error("truncated iad block fields");
        }
        block.layer_offset_y = to_i32(raw);

        std::uint32_t table_size = 0;
        if (!read_u32_at(data, offset + 28, table_size) || table_size > kMaxTableSize ||
            (table_size % 8) != 0) {
            return format_error("invalid iad crop table size");
        }
        if (offset + kHeaderSize + table_size > block_end) {
            return format_error("iad crop table exceeds the block");
        }

        const std::size_t crop_count = table_size / 8;
        block.crops.reserve(crop_count);
        for (std::size_t i = 0; i < crop_count; ++i) {
            IadCrop crop;
            const std::size_t entry = offset + kHeaderSize + i * 8;
            if (!read_i16_at(data, entry, crop.x1) || !read_i16_at(data, entry + 2, crop.y1) ||
                !read_i16_at(data, entry + 4, crop.x2) || !read_i16_at(data, entry + 6, crop.y2)) {
                return format_error("truncated iad crop entry");
            }
            block.crops.push_back(crop);
        }

        const std::size_t shadow_pos = offset + kHeaderSize + table_size;
        if (shadow_pos + 20 <= block_end) {
            std::uint32_t shadow_width = 0, shadow_height = 0, off_x = 0, off_y = 0;
            std::uint32_t shadow_size = 0;
            if (!read_u32_at(data, shadow_pos, shadow_width) ||
                !read_u32_at(data, shadow_pos + 4, shadow_height) ||
                !read_u32_at(data, shadow_pos + 8, off_x) ||
                !read_u32_at(data, shadow_pos + 12, off_y) ||
                !read_u32_at(data, shadow_pos + 16, shadow_size)) {
                return format_error("truncated iad shadow header");
            }
            block.shadow_frame_width = shadow_width;
            block.shadow_frame_height = shadow_height;
            block.shadow_layer_offset_x = to_i32(off_x);
            block.shadow_layer_offset_y = to_i32(off_y);
            if (shadow_size > kMaxTableSize || (shadow_size % 8) != 0 ||
                shadow_pos + 20 + shadow_size > block_end) {
                return format_error("invalid iad shadow crop table");
            }
            const std::size_t shadow_count = shadow_size / 8;
            block.shadow_crops.reserve(shadow_count);
            for (std::size_t i = 0; i < shadow_count; ++i) {
                IadCrop crop;
                const std::size_t entry = shadow_pos + 20 + i * 8;
                if (!read_i16_at(data, entry, crop.x1) || !read_i16_at(data, entry + 2, crop.y1) ||
                    !read_i16_at(data, entry + 4, crop.x2) ||
                    !read_i16_at(data, entry + 6, crop.y2)) {
                    return format_error("truncated iad shadow crop entry");
                }
                block.shadow_crops.push_back(crop);
            }
        } else if (shadow_pos != block_end) {
            return format_error("truncated iad shadow section");
        }

        blocks.push_back(std::move(block));

        if (block_end == data.size())
            break;
        offset = block_end;
    }
    if (blocks.empty())
        return format_error("iad contains no blocks");
    return blocks;
}
} // namespace gl::assets
