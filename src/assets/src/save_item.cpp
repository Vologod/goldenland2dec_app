#include "gl/assets/save_item.hpp"

#include <algorithm>
#include <new>
#include <string>
#include <utility>

namespace gl::assets {
namespace {
constexpr std::size_t kFixedSize = 0xC4;
constexpr std::size_t kSubrecordSize = 0x14;
constexpr std::size_t kTypeOffset = 0x08;
constexpr std::size_t kFlagsOffset = 0x0C;
constexpr std::size_t kPriceOffset = 0x10;
constexpr std::size_t kWeightOffset = 0x14;
constexpr std::size_t kUnknownAOffset = 0x18;
constexpr std::size_t kStyleOffset = 0x20;
constexpr std::size_t kMaterialOffset = 0x30;
constexpr std::size_t kDuplicatedCountOffset = 0x78;
constexpr std::size_t kModCountOffset = 0xC0;
constexpr std::size_t kItemIdOffset = 0xA8;
constexpr std::size_t kCountOffset = 0xB0;
constexpr std::size_t kDurabilityOffset = 0xB4;
constexpr std::uint32_t kItemMarker = 0x14092248u;
constexpr std::uint32_t kMaxModifiers = 0xFFFFu;

std::uint32_t read_u32(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

void write_u32(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value) {
    data[offset] = static_cast<std::uint8_t>(value & 0xFF);
    data[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    data[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    data[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }
} // namespace

Result<ItemRecordRead> parse_item_record(std::span<const std::uint8_t> data) {
    if (data.size() < kFixedSize)
        return format_error("item record is shorter than 0xC4");
    const std::uint32_t subcount = read_u32(data, kModCountOffset);
    const std::uint64_t size64 =
        static_cast<std::uint64_t>(kFixedSize) + static_cast<std::uint64_t>(subcount) * kSubrecordSize;
    if (size64 > data.size())
        return format_error("item record subrecords are truncated");
    try {
        ItemRecordRead out;
        out.size = static_cast<std::size_t>(size64);
        out.record.item_id = read_u32(data, kItemIdOffset);
        out.record.count = read_u32(data, kCountOffset);
        out.record.durability = read_u32(data, kDurabilityOffset);
        out.record.type = read_u32(data, kTypeOffset);
        out.record.flags = read_u32(data, kFlagsOffset);
        out.record.style = read_u32(data, kStyleOffset);
        out.record.price = read_u32(data, kPriceOffset);
        out.record.weight_x10 = read_u32(data, kWeightOffset);
        out.record.unknown_a = read_u32(data, kUnknownAOffset);
        out.record.material = read_u32(data, kMaterialOffset);
        out.record.mod_count = subcount;
        out.record.modifiers.resize(subcount);
        for (std::uint32_t index = 0; index < subcount; ++index) {
            const std::size_t base =
                kFixedSize + static_cast<std::size_t>(index) * kSubrecordSize;
            for (std::size_t word = 0; word < 5; ++word)
                out.record.modifiers[index].words[word] = read_u32(data, base + word * 4);
        }
        std::copy_n(data.begin(), out.record.raw.size(), out.record.raw.begin());
        out.record.has_raw = true;
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("item record allocation failed");
    }
}

Result<std::vector<std::uint8_t>> encode_item_record(const SaveItemRecord &record) {
    if (record.mod_count != record.modifiers.size() || record.mod_count > kMaxModifiers)
        return format_error("item record modifier count mismatch");
    try {
        std::vector<std::uint8_t> out;
        if (record.has_raw)
            out.assign(record.raw.begin(), record.raw.end());
        else {
            out.assign(0xC0, 0);
            write_u32(out, 0x00, kItemMarker);
        }
        write_u32(out, kTypeOffset, record.type);
        write_u32(out, kFlagsOffset, record.flags);
        write_u32(out, kStyleOffset, record.style);
        write_u32(out, kPriceOffset, record.price);
        write_u32(out, kWeightOffset, record.weight_x10);
        write_u32(out, kUnknownAOffset, record.unknown_a);
        write_u32(out, kMaterialOffset, record.material);
        write_u32(out, kItemIdOffset, record.item_id);
        write_u32(out, kCountOffset, record.count);
        write_u32(out, kDurabilityOffset, record.durability);
        out.resize(out.size() + 4 + record.modifiers.size() * kSubrecordSize);
        write_u32(out, kDuplicatedCountOffset, record.mod_count);
        write_u32(out, kModCountOffset, record.mod_count);
        for (std::size_t index = 0; index < record.modifiers.size(); ++index) {
            const std::size_t base = kFixedSize + index * kSubrecordSize;
            for (std::size_t word = 0; word < 5; ++word)
                write_u32(out, base + word * 4, record.modifiers[index].words[word]);
        }
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("item record allocation failed");
    } catch (const std::length_error &) {
        return format_error("item record allocation failed");
    }
}
} // namespace gl::assets
