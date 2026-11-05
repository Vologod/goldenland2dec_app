#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/byte_reader.hpp"
#include "gl/assets/save.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <miniz.h>

namespace gl::assets {
namespace {
constexpr std::size_t kSaveHeaderSize = 112;
constexpr std::size_t kMaxBodySize = 32 * 1024 * 1024;
constexpr std::size_t kHeroFixedPrefix = 0x734;
constexpr std::size_t kItemRecordFixed = 0xC4;
constexpr std::size_t kItemSubrecordSize = 0x14;
constexpr std::size_t kItemTypeOffset = 0x08;
constexpr std::size_t kItemFlagsOffset = 0x0C;
constexpr std::size_t kItemStyleOffset = 0x20;
constexpr std::size_t kItemIdOffset = 0xA8;
constexpr std::size_t kItemCountOffset = 0xB0;
constexpr std::size_t kItemSubcountOffset = 0xC0;
constexpr std::uint32_t kAbsentContainer = 0xFFFFFFFFu;
constexpr std::array<std::uint8_t, 8> kHeroTag = {'B', 'T', '_', 'H', 'E', 'R', 'O', ' '};

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

std::uint32_t read_u32(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}
} // namespace

Result<std::vector<std::uint8_t>> decode_save_body(std::span<const std::uint8_t> file) {
    try {
        auto header = parse_save_header(file);
        if (!header.has_value())
            return header.error();
        if (file.size() <= kSaveHeaderSize)
            return format_error("save file has no zlib payload");

        std::vector<std::uint8_t> decompressed(kMaxBodySize);
        mz_ulong out_len = static_cast<mz_ulong>(decompressed.size());
        const auto *compressed = file.data() + kSaveHeaderSize;
        const auto compressed_size = static_cast<mz_ulong>(file.size() - kSaveHeaderSize);
        const int status =
            mz_uncompress(decompressed.data(), &out_len, compressed, compressed_size);
        if (status != MZ_OK)
            return format_error("save body zlib decompression failed");
        decompressed.resize(static_cast<std::size_t>(out_len));

        const auto tag =
            std::search(decompressed.begin(), decompressed.end(), kHeroTag.begin(), kHeroTag.end());
        if (tag == decompressed.end())
            return format_error("BT_HERO block not found");
        const auto tag_offset = static_cast<std::size_t>(std::distance(decompressed.begin(), tag));
        if (decompressed.size() - tag_offset < 12)
            return format_error("truncated BT_HERO header");
        const std::uint32_t body_size = read_u32(decompressed, tag_offset + 8);
        const std::size_t body_offset = tag_offset + 12;
        if (body_size > decompressed.size() - body_offset)
            return format_error("BT_HERO body extends past the save payload");

        const auto begin = decompressed.begin() + static_cast<std::ptrdiff_t>(body_offset);
        const auto end = begin + static_cast<std::ptrdiff_t>(body_size);
        return std::vector<std::uint8_t>(begin, end);
    } catch (const std::bad_alloc &) {
        return format_error("save body allocation failed");
    }
}

Result<HeroInventory> parse_hero_inventory(std::span<const std::uint8_t> body) {
    try {
        if (body.size() < kHeroFixedPrefix)
            return format_error("BT_HERO body is shorter than its fixed prefix");

        ByteReader reader(body);
        if (!reader.skip(kHeroFixedPrefix))
            return format_error("truncated BT_HERO fixed prefix");

        HeroInventory inventory;
        for (int container = 0; container < 3; ++container) {
            std::uint32_t id = 0;
            if (!reader.read_u32le(id))
                return format_error("truncated BT_HERO container id");
            if (id == kAbsentContainer)
                continue;

            std::uint32_t count = 0;
            if (!reader.read_u32le(count))
                return format_error("truncated BT_HERO container count");

            std::vector<InventoryItem> *target = &inventory.hot;
            if (container == 1)
                target = &inventory.items;
            else if (container == 2)
                target = &inventory.equipped;

            for (std::uint32_t index = 0; index < count; ++index) {
                const auto remaining = reader.remaining();
                if (remaining.size() < kItemRecordFixed)
                    return format_error("truncated BT_HERO item record");

                const std::uint32_t subcount = read_u32(remaining, kItemSubcountOffset);
                const std::uint64_t record_size64 =
                    static_cast<std::uint64_t>(kItemRecordFixed) +
                    static_cast<std::uint64_t>(subcount) * kItemSubrecordSize;
                if (record_size64 > remaining.size())
                    return format_error("truncated BT_HERO item subrecords");
                const std::size_t record_size = static_cast<std::size_t>(record_size64);

                InventoryItem item;
                item.item_id = read_u32(remaining, kItemIdOffset);
                item.type = read_u32(remaining, kItemTypeOffset);
                item.flags = read_u32(remaining, kItemFlagsOffset);
                item.style = read_u32(remaining, kItemStyleOffset);
                item.count = container == 2 ? 1u : read_u32(remaining, kItemCountOffset);
                target->push_back(item);

                if (!reader.skip(record_size))
                    return format_error("truncated BT_HERO item record");
            }
        }
        return inventory;
    } catch (const std::bad_alloc &) {
        return format_error("BT_HERO inventory allocation failed");
    }
}

Result<HeroEquipment> parse_hero_equipment(std::span<const std::uint8_t> body) {
    auto inventory = parse_hero_inventory(body);
    if (!inventory.has_value())
        return inventory.error();

    HeroEquipment equipment;
    equipment.items.reserve(inventory->equipped.size());
    for (const auto &item : inventory->equipped)
        equipment.items.push_back(EquippedItem{item.item_id, item.type, item.flags, item.style});
    return equipment;
}
} // namespace gl::assets
