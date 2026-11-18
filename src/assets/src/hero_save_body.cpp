#include "gl/assets/hero_save_body.hpp"

#include <algorithm>
#include <new>
#include <string>
#include <utility>

namespace gl::assets {
namespace {
constexpr std::size_t kHeaderSize = 12;
constexpr std::size_t kStatSize = 0x11C;
constexpr std::size_t kHpOffset = 0x128;
constexpr std::size_t kBufferOffset = 0x134;
constexpr std::size_t kBufferSize = 0x600;
constexpr std::size_t kContainersOffset = 0x734;
constexpr std::size_t kTail80Size = 0x80;
constexpr std::size_t kExtraSize = 0x14;
constexpr std::size_t kMaxExtra = 100000;

std::uint32_t read_u32(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

void write_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

void append(std::vector<std::uint8_t> &out, std::span<const std::uint8_t> bytes) {
    out.insert(out.end(), bytes.begin(), bytes.end());
}

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }
} // namespace

Result<HeroSaveBody> parse_hero_save_body(std::span<const std::uint8_t> data) {
    if (data.size() < kContainersOffset)
        return format_error("hero save body is too short");
    try {
        HeroSaveBody body;
        body.tile_x = read_u32(data, 0);
        body.tile_y = read_u32(data, 4);
        body.direction = read_u32(data, 8);
        std::copy_n(data.begin() + kHeaderSize, kStatSize, body.stat_block.begin());
        body.hp = read_u32(data, kHpOffset);
        body.ep = read_u32(data, kHpOffset + 4);
        body.ap = read_u32(data, kHpOffset + 8);
        std::copy_n(data.begin() + kBufferOffset, kBufferSize, body.buffer600.begin());

        std::size_t cursor = kContainersOffset;
        for (HeroContainerState &container : body.containers) {
            if (cursor + 4 > data.size())
                return format_error("hero container flag is truncated");
            container.flag = read_u32(data, cursor);
            cursor += 4;
            if (container.flag > 1)
                return format_error("hero container flag is not 0/1");
            if (container.flag == 0)
                continue;
            if (cursor + 4 > data.size())
                return format_error("hero container count is truncated");
            const std::uint32_t count = read_u32(data, cursor);
            cursor += 4;
            if (count > 100000)
                return format_error("hero container count is unreasonable");
            container.items.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index) {
                auto parsed = parse_item_record(data.subspan(cursor));
                if (!parsed)
                    return parsed.error();
                cursor += parsed->size;
                container.items.push_back(std::move(parsed->record));
            }
        }
        if (cursor + 8 > data.size())
            return format_error("hero post-container fields are truncated");
        body.field_2bb4 = read_u32(data, cursor);
        body.field_2bb8 = read_u32(data, cursor + 4);
        cursor += 8;
        if (cursor + kTail80Size > data.size())
            return format_error("hero 0x80 block is truncated");
        std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(cursor), kTail80Size,
                    body.tail80.begin());
        cursor += kTail80Size;
        if (cursor + 4 > data.size())
            return format_error("hero extra count is truncated");
        const std::uint32_t extra_count = read_u32(data, cursor);
        cursor += 4;
        if (extra_count > kMaxExtra)
            return format_error("hero extra count is unreasonable");
        body.extra.resize(extra_count);
        for (auto &record : body.extra) {
            if (cursor + kExtraSize > data.size())
                return format_error("hero extra record is truncated");
            std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(cursor), kExtraSize,
                        record.begin());
            cursor += kExtraSize;
        }
        if (cursor + 4 > data.size())
            return format_error("hero attached count is truncated");
        body.attached_count = read_u32(data, cursor);
        cursor += 4;
        body.attached_raw.assign(data.begin() + static_cast<std::ptrdiff_t>(cursor), data.end());
        return body;
    } catch (const std::bad_alloc &) {
        return format_error("hero save body allocation failed");
    } catch (const std::length_error &) {
        return format_error("hero save body allocation failed");
    }
}

Result<std::vector<std::uint8_t>> encode_hero_save_body(const HeroSaveBody &body) {
    try {
        std::vector<std::uint8_t> out;
        out.reserve(kContainersOffset + 0x400);
        write_u32(out, body.tile_x);
        write_u32(out, body.tile_y);
        write_u32(out, body.direction);
        append(out, body.stat_block);
        write_u32(out, body.hp);
        write_u32(out, body.ep);
        write_u32(out, body.ap);
        append(out, body.buffer600);
        for (const HeroContainerState &container : body.containers) {
            if (container.flag > 1)
                return format_error("hero container flag is not 0/1");
            write_u32(out, container.flag);
            if (container.flag == 0)
                continue;
            if (container.items.size() > 100000)
                return format_error("hero container count is unreasonable");
            write_u32(out, static_cast<std::uint32_t>(container.items.size()));
            for (const SaveItemRecord &item : container.items) {
                auto encoded = encode_item_record(item);
                if (!encoded)
                    return encoded.error();
                append(out, encoded.value());
            }
        }
        write_u32(out, body.field_2bb4);
        write_u32(out, body.field_2bb8);
        append(out, body.tail80);
        if (body.extra.size() > kMaxExtra)
            return format_error("hero extra count is unreasonable");
        write_u32(out, static_cast<std::uint32_t>(body.extra.size()));
        for (const auto &record : body.extra)
            append(out, record);
        write_u32(out, body.attached_count);
        append(out, body.attached_raw);
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("hero save body allocation failed");
    } catch (const std::length_error &) {
        return format_error("hero save body allocation failed");
    }
}
} // namespace gl::assets
