#include "gl/game/save_person.hpp"

#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gl::game {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

bool read_string(assets::ByteReader &reader, std::string &out) {
    std::uint32_t length = 0;
    if (!reader.read_u32le(length))
        return false;
    const std::size_t count = length < 0x104 ? length : 0x104;
    std::span<const std::uint8_t> bytes;
    if (!reader.read_bytes(count, bytes))
        return false;
    out = assets::cp1251_to_utf8(bytes);
    return true;
}
} // namespace

Result<SavePersonRecord> decode_save_person(const assets::SaveEntity &entity) {
    if (entity.type != 2)
        return format_error("save entity type " + std::to_string(entity.type) +
                            " is not a person (2)");

    assets::ByteReader reader(entity.body);
    SavePersonRecord record;
    std::uint32_t value = 0;

    if (!read_string(reader, record.name))
        return format_error("save person name is truncated");
    if (!reader.read_u32le(value))
        return format_error("save person +0x40 is truncated");
    record.field_40 = value;
    if (!reader.read_u32le(value))
        return format_error("save person +0x44 is truncated");
    record.field_44 = value;
    if (!reader.read_u32le(value))
        return format_error("save person current tile x is truncated");
    record.current_tile_x = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person current tile y is truncated");
    record.current_tile_y = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person anchor tile x is truncated");
    record.anchor_tile_x = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person anchor tile y is truncated");
    record.anchor_tile_y = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person +0x654 is truncated");
    record.field_654 = value;
    std::span<const std::uint8_t> field658;
    if (!reader.read_bytes(record.field_658.size(), field658))
        return format_error("save person +0x658 is truncated");
    std::copy(field658.begin(), field658.end(), record.field_658.begin());
    if (!reader.read_u32le(value))
        return format_error("save person direction is truncated");
    record.direction = static_cast<std::uint8_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person route type is truncated");
    record.route_type = static_cast<std::int32_t>(value);
    if (!read_string(reader, record.route_name))
        return format_error("save person route name is truncated");
    if (!reader.read_u32le(value))
        return format_error("save person radius is truncated");
    record.radius = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person delay min is truncated");
    record.delay_min = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person delay max is truncated");
    record.delay_max = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person tribe id is truncated");
    record.tribe_id = static_cast<std::int32_t>(value);
    if (!read_string(reader, record.dialog))
        return format_error("save person dialog is truncated");
    if (!read_string(reader, record.inventory))
        return format_error("save person inventory is truncated");
    if (!reader.read_u32le(value))
        return format_error("save person +0x6cc is truncated");
    record.field_6cc = value;
    std::span<const std::uint8_t> field_ff0;
    if (!reader.read_bytes(record.field_ff0.size(), field_ff0))
        return format_error("save person +0xff0 is truncated");
    std::copy(field_ff0.begin(), field_ff0.end(), record.field_ff0.begin());
    if (!reader.read_u32le(record.field_f50))
        return format_error("save person skipped field is truncated");
    if (!reader.read_u32le(value))
        return format_error("save person HP is truncated");
    record.hp = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person EP is truncated");
    record.energy = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person AP is truncated");
    record.ap = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person route index is truncated");
    record.route_index = std::bit_cast<std::int32_t>(value);

    for (SaveContainer &container : record.containers) {
        if (!reader.read_u32le(value))
            return format_error("save person container flag is truncated");
        container.flag = value;
        if (value == 0)
            continue;
        container.present = true;
        const std::size_t items_start = reader.position();
        if (!reader.read_u32le(value))
            return format_error("save person container count is truncated");
        if (value > 100000)
            return format_error("save person container count is unreasonable");
        for (std::uint32_t index = 0; index < value; ++index) {
            auto parsed = assets::parse_item_record(reader.remaining());
            if (!parsed)
                return format_error("save person item record is truncated");
            container.items.push_back(std::move(parsed->record));
            if (!reader.skip(parsed->size))
                return format_error("save person item record is truncated");
        }
        container.raw_items.assign(entity.body.begin() + items_start,
                                   entity.body.begin() + reader.position());
    }
    std::span<const std::uint8_t> tail80;
    if (!reader.read_bytes(record.tail80.size(), tail80))
        return format_error("save person tail prefix is truncated");
    std::copy(tail80.begin(), tail80.end(), record.tail80.begin());
    if (!reader.read_u32le(record.tail_count))
        return format_error("save person tail count is truncated");
    const std::uint64_t tail_bytes =
        static_cast<std::uint64_t>(record.tail_count) * 0x14;
    if (tail_bytes > reader.remaining().size())
        return format_error("save person tail entries are truncated");
    record.extra.resize(record.tail_count);
    for (auto &entry : record.extra) {
        std::span<const std::uint8_t> bytes;
        if (!reader.read_bytes(entry.size(), bytes))
            return format_error("save person tail entries are truncated");
        std::copy(bytes.begin(), bytes.end(), entry.begin());
    }
    if (!reader.remaining().empty())
        return format_error("save person body has trailing bytes");

    return record;
}

Result<std::vector<std::uint8_t>> encode_save_person_body(const SavePersonRecord &record) {
    try {
        std::vector<std::uint8_t> out;
        const auto put_u32 = [&out](std::uint32_t value) {
            out.push_back(static_cast<std::uint8_t>(value & 0xFF));
            out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
            out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
            out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
        };
        const auto put_string = [&](const std::string &text) -> bool {
            const std::string encoded = assets::utf8_to_cp1251(text);
            if (encoded.size() > 0xFFFFFFFFu)
                return false;
            put_u32(static_cast<std::uint32_t>(encoded.size()));
            out.insert(out.end(), encoded.begin(), encoded.end());
            return true;
        };
        const auto put_bytes = [&out](const auto &bytes) {
            out.insert(out.end(), bytes.begin(), bytes.end());
        };

        if (!put_string(record.name))
            return format_error("save person name is too long");
        put_u32(record.field_40);
        put_u32(record.field_44);
        put_u32(static_cast<std::uint32_t>(record.current_tile_x));
        put_u32(static_cast<std::uint32_t>(record.current_tile_y));
        put_u32(static_cast<std::uint32_t>(record.anchor_tile_x));
        put_u32(static_cast<std::uint32_t>(record.anchor_tile_y));
        put_u32(record.field_654);
        put_bytes(record.field_658);
        put_u32(record.direction);
        put_u32(static_cast<std::uint32_t>(record.route_type));
        if (!put_string(record.route_name))
            return format_error("save person route name is too long");
        put_u32(static_cast<std::uint32_t>(record.radius));
        put_u32(static_cast<std::uint32_t>(record.delay_min));
        put_u32(static_cast<std::uint32_t>(record.delay_max));
        put_u32(static_cast<std::uint32_t>(record.tribe_id));
        if (!put_string(record.dialog))
            return format_error("save person dialog is too long");
        if (!put_string(record.inventory))
            return format_error("save person inventory is too long");
        put_u32(record.field_6cc);
        put_bytes(record.field_ff0);
        put_u32(record.field_f50);
        put_u32(static_cast<std::uint32_t>(record.hp));
        put_u32(static_cast<std::uint32_t>(record.energy));
        put_u32(static_cast<std::uint32_t>(record.ap));
        put_u32(static_cast<std::uint32_t>(record.route_index));
        for (std::size_t index = 0; index < record.containers.size(); ++index) {
            const SaveContainer &container = record.containers[index];
            const std::uint32_t flag = container.flag != 0 ? 1u : 0u;
            put_u32(flag);
            if (flag == 0)
                continue;
            if (index == 2 && !container.raw_items.empty()) {
                out.insert(out.end(), container.raw_items.begin(), container.raw_items.end());
                continue;
            }
            if (container.items.size() > 100000)
                return format_error("save person container count is unreasonable");
            put_u32(static_cast<std::uint32_t>(container.items.size()));
            for (const assets::SaveItemRecord &item : container.items) {
                auto encoded = assets::encode_item_record(item);
                if (!encoded)
                    return encoded.error();
                out.insert(out.end(), encoded->begin(), encoded->end());
            }
        }
        put_bytes(record.tail80);
        if (record.tail_count != record.extra.size())
            return format_error("save person tail count mismatch");
        put_u32(record.tail_count);
        for (const auto &entry : record.extra)
            put_bytes(entry);
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("save person allocation failed");
    } catch (const std::length_error &) {
        return format_error("save person allocation failed");
    }
}
} // namespace gl::game
