#include "gl/game/save_person.hpp"

#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"

#include <bit>
#include <cstddef>
#include <span>
#include <string>
#include <utility>

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
    if (!reader.read_u32le(value))
        return format_error("save person +0x44 is truncated");
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
    if (!reader.skip(0x20))
        return format_error("save person +0x658 is truncated");
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
    if (!reader.skip(0x11c))
        return format_error("save person +0xff0 is truncated");
    if (!reader.skip(4))
        return format_error("save person skipped field is truncated");
    if (!reader.read_u32le(value))
        return format_error("save person HP is truncated");
    record.hp = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person EP is truncated");
    record.energy = static_cast<std::int32_t>(value);
    if (!reader.read_u32le(value))
        return format_error("save person AP is truncated");
    if (!reader.read_u32le(value))
        return format_error("save person route index is truncated");
    record.route_index = std::bit_cast<std::int32_t>(value);

    return record;
}
} // namespace gl::game
