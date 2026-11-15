#include "gl/assets/save_item.hpp"

#include <new>
#include <string>
#include <utility>

namespace gl::assets {
namespace {
constexpr std::size_t kFixedSize = 0xC4;
constexpr std::size_t kSubrecordSize = 0x14;
constexpr std::size_t kTypeOffset = 0x08;
constexpr std::size_t kFlagsOffset = 0x0C;
constexpr std::size_t kStyleOffset = 0x20;
constexpr std::size_t kModCountOffset = 0xC0;
constexpr std::size_t kItemIdOffset = 0xA8;
constexpr std::size_t kCountOffset = 0xB0;
constexpr std::size_t kDurabilityOffset = 0xB4;

std::uint32_t read_u32(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
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
        out.record.mod_count = subcount;
        out.record.modifiers.resize(subcount);
        for (std::uint32_t index = 0; index < subcount; ++index) {
            const std::size_t base =
                kFixedSize + static_cast<std::size_t>(index) * kSubrecordSize;
            for (std::size_t word = 0; word < 5; ++word)
                out.record.modifiers[index].words[word] = read_u32(data, base + word * 4);
        }
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("item record allocation failed");
    }
}
} // namespace gl::assets
