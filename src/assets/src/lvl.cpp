#include "gl/assets/lvl.hpp"
#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"
#include <new>
#include <string_view>

namespace gl::assets {
namespace {
constexpr std::array<std::string_view, 12> kBlockNames = {
    "BLK_LVER", "BLK_MPSZ", "BLK_MHDR", "BLK_MDSC", "BLK_SDSC", "BLK_ADSC",
    "BLK_TDSC", "BLK_CGRP", "BLK_SENV", "BLK_WTHR", "BLK_DOOR", "BLK_LFLS"};

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

bool read_lvl_string(ByteReader& reader, std::string& out) {
    std::uint32_t length = 0;
    if (!reader.read_u32le(length)) return false;
    std::span<const std::uint8_t> bytes;
    if (!reader.read_bytes(length, bytes)) return false;
    out = cp1251_to_utf8(bytes);
    return true;
}

Result<std::span<const std::uint8_t>> read_block(ByteReader& reader, std::string_view expected) {
    const std::size_t offset = reader.position();
    std::span<const std::uint8_t> name;
    if (!reader.read_bytes(8, name))
        return format_error("truncated LVL block header at offset " + std::to_string(offset));
    const std::string_view actual(reinterpret_cast<const char*>(name.data()), name.size());
    if (actual != expected)
        return format_error("unexpected LVL block at offset " + std::to_string(offset) +
                            ": expected " + std::string(expected) + ", got " +
                            std::string(actual));
    std::uint32_t size = 0;
    if (!reader.read_u32le(size))
        return format_error("truncated LVL block size at offset " + std::to_string(offset));
    std::span<const std::uint8_t> payload;
    if (!reader.read_bytes(size, payload))
        return format_error("LVL block " + std::string(expected) + " payload exceeds file at offset " +
                            std::to_string(offset));
    return payload;
}

Result<std::vector<LvlMask>> parse_masks(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    std::uint32_t count = 0;
    if (!reader.read_u32le(count))
        return format_error("truncated BLK_MDSC count at offset 0");
    if (payload.size() < 4 || (payload.size() - 4) % 16 != 0 ||
        count != (payload.size() - 4) / 16)
        return format_error("BLK_MDSC count does not match payload size");
    std::vector<LvlMask> masks;
    masks.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        LvlMask mask;
        std::uint32_t padding = 0;
        const std::size_t offset = reader.position();
        if (!reader.read_u32le(padding) || !reader.read_u32le(mask.number) ||
            !reader.read_u32le(mask.x) || !reader.read_u32le(mask.y))
            return format_error("truncated BLK_MDSC entry at offset " + std::to_string(offset));
        (void)padding;
        masks.push_back(mask);
    }
    return masks;
}

Result<std::vector<LvlDescription>> parse_descriptions(std::span<const std::uint8_t> payload,
                                                       std::string_view block_name) {
    ByteReader reader(payload);
    std::uint32_t count = 0;
    if (!reader.read_u32le(count))
        return format_error("truncated " + std::string(block_name) + " count at offset 0");
    if (payload.size() < 4 || count > (payload.size() - 4) / 20)
        return format_error(std::string(block_name) + " count exceeds payload");
    std::vector<LvlDescription> descriptions;
    descriptions.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        LvlDescription description;
        std::uint32_t raw_x = 0, raw_y = 0;
        const std::size_t entry_offset = reader.position();
        if (!reader.read_u16le(description.param1) || !reader.read_u16le(description.param2) ||
            !reader.read_u32le(description.number) || !reader.read_u32le(raw_x) ||
            !reader.read_u32le(raw_y))
            return format_error("truncated " + std::string(block_name) + " entry at offset " +
                                std::to_string(entry_offset));
        description.x = static_cast<std::int32_t>(raw_x);
        description.y = static_cast<std::int32_t>(raw_y);
        const std::size_t string_offset = reader.position();
        if (!read_lvl_string(reader, description.name))
            return format_error("truncated " + std::string(block_name) + " string at offset " +
                                std::to_string(string_offset));
        descriptions.push_back(std::move(description));
    }
    if (!reader.at_end())
        return format_error(std::string(block_name) + " has trailing bytes at offset " +
                            std::to_string(reader.position()));
    return descriptions;
}

Result<std::vector<LvlCellGroup>> parse_cell_groups(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    std::uint32_t count = 0;
    if (!reader.read_u32le(count))
        return format_error("truncated BLK_CGRP count at offset 0");
    if (payload.size() < 4 || count > (payload.size() - 4) / 8)
        return format_error("BLK_CGRP count exceeds payload");
    std::vector<LvlCellGroup> groups;
    groups.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        LvlCellGroup group;
        const std::size_t name_offset = reader.position();
        if (!read_lvl_string(reader, group.name))
            return format_error("truncated BLK_CGRP name at offset " +
                                std::to_string(name_offset));
        std::uint32_t cells = 0;
        if (!reader.read_u32le(cells))
            return format_error("truncated BLK_CGRP cell count at offset " +
                                std::to_string(reader.position()));
        if (cells > reader.remaining().size() / 4)
            return format_error("BLK_CGRP cell count exceeds payload at offset " +
                                std::to_string(reader.position()));
        group.cells.reserve(cells);
        for (std::uint32_t j = 0; j < cells; ++j) {
            std::uint16_t x = 0, y = 0;
            const std::size_t cell_offset = reader.position();
            if (!reader.read_u16le(x) || !reader.read_u16le(y))
                return format_error("truncated BLK_CGRP cell at offset " +
                                    std::to_string(cell_offset));
            group.cells.emplace_back(x, y);
        }
        groups.push_back(std::move(group));
    }
    if (!reader.at_end())
        return format_error("BLK_CGRP has trailing bytes at offset " +
                            std::to_string(reader.position()));
    return groups;
}

VoidResult validate_environment(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    if (!reader.skip(16))
        return format_error("truncated BLK_SENV header at offset 0");
    std::uint32_t sound_count = 0;
    if (!reader.read_u32le(sound_count))
        return format_error("truncated BLK_SENV sound count at offset " +
                            std::to_string(reader.position()));
    std::string ignored;
    for (int i = 0; i < 3; ++i) {
        const std::size_t offset = reader.position();
        if (!read_lvl_string(reader, ignored))
            return format_error("truncated BLK_SENV string at offset " +
                                std::to_string(offset));
    }
    if (sound_count > reader.remaining().size() / 52)
        return format_error("BLK_SENV sound count exceeds payload at offset " +
                            std::to_string(reader.position()));
    for (std::uint32_t i = 0; i < sound_count; ++i) {
        const std::size_t path_offset = reader.position();
        if (!read_lvl_string(reader, ignored))
            return format_error("truncated BLK_SENV sound path at offset " +
                                std::to_string(path_offset));
        const std::size_t record_offset = reader.position();
        if (!reader.skip(48))
            return format_error("truncated BLK_SENV sound record at offset " +
                                std::to_string(record_offset));
    }
    if (!reader.at_end())
        return format_error("BLK_SENV has trailing bytes at offset " +
                            std::to_string(reader.position()));
    return std::monostate{};
}

VoidResult validate_doors(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    std::uint32_t count = 0;
    if (!reader.read_u32le(count))
        return format_error("truncated BLK_DOOR count at offset 0");
    if (count > (payload.size() - 4) / 24)
        return format_error("BLK_DOOR count exceeds payload");
    std::string ignored;
    for (std::uint32_t i = 0; i < count; ++i) {
        for (int j = 0; j < 6; ++j) {
            const std::size_t offset = reader.position();
            if (!read_lvl_string(reader, ignored))
                return format_error("truncated BLK_DOOR string at offset " +
                                    std::to_string(offset));
        }
    }
    if (!reader.at_end())
        return format_error("BLK_DOOR has trailing bytes at offset " +
                            std::to_string(reader.position()));
    return std::monostate{};
}
} // namespace

Result<LvlData> parse_lvl(std::span<const std::uint8_t> data) {
    try {
        ByteReader reader(data);
        LvlData lvl;
        for (std::size_t block = 0; block < kBlockNames.size(); ++block) {
            auto payload_result = read_block(reader, kBlockNames[block]);
            if (!payload_result.has_value()) return payload_result.error();
            const std::span<const std::uint8_t> payload = payload_result.value();
            ByteReader block_reader(payload);

            switch (block) {
                case 0: {
                    if (payload.size() != 4) return format_error("bad BLK_LVER size");
                    if (!block_reader.read_u16le(lvl.version.major) ||
                        !block_reader.read_u16le(lvl.version.minor))
                        return format_error("truncated BLK_LVER at offset " +
                                            std::to_string(block_reader.position()));
                    break;
                }
                case 1: {
                    if (payload.size() != 8) return format_error("bad BLK_MPSZ size");
                    if (!block_reader.read_u32le(lvl.map_size.pixel_width) ||
                        !block_reader.read_u32le(lvl.map_size.pixel_height))
                        return format_error("truncated BLK_MPSZ at offset " +
                                            std::to_string(block_reader.position()));
                    break;
                }
                case 2: {
                    if (payload.size() < 8) return format_error("BLK_MHDR too short");
                    if (!block_reader.read_u32le(lvl.chunk_width) ||
                        !block_reader.read_u32le(lvl.chunk_height))
                        return format_error("truncated BLK_MHDR header at offset " +
                                            std::to_string(block_reader.position()));
                    const std::uint64_t chunk_count =
                        static_cast<std::uint64_t>(lvl.chunk_width) * lvl.chunk_height;
                    const std::size_t chunk_bytes = payload.size() - 8;
                    if (chunk_bytes % 24 != 0 || chunk_count != chunk_bytes / 24)
                        return format_error("BLK_MHDR size does not match chunk dimensions at "
                                            "offset " +
                                            std::to_string(block_reader.position()));
                    lvl.chunks.resize(static_cast<std::size_t>(chunk_count));
                    for (auto& chunk : lvl.chunks) {
                        for (auto& tile : chunk) {
                            const std::size_t offset = block_reader.position();
                            if (!block_reader.read_u16le(tile.relief) ||
                                !block_reader.read_u16le(tile.sound) ||
                                !block_reader.read_u16le(tile.mask))
                                return format_error("truncated BLK_MHDR tile at offset " +
                                                    std::to_string(offset));
                        }
                    }
                    break;
                }
                case 3: {
                    auto masks = parse_masks(payload);
                    if (!masks.has_value()) return masks.error();
                    lvl.masks = std::move(masks.value());
                    break;
                }
                case 4: {
                    auto descriptions = parse_descriptions(payload, "BLK_SDSC");
                    if (!descriptions.has_value()) return descriptions.error();
                    lvl.statics = std::move(descriptions.value());
                    break;
                }
                case 5: {
                    auto descriptions = parse_descriptions(payload, "BLK_ADSC");
                    if (!descriptions.has_value()) return descriptions.error();
                    lvl.animations = std::move(descriptions.value());
                    break;
                }
                case 6: {
                    auto descriptions = parse_descriptions(payload, "BLK_TDSC");
                    if (!descriptions.has_value()) return descriptions.error();
                    lvl.triggers = std::move(descriptions.value());
                    break;
                }
                case 7: {
                    auto groups = parse_cell_groups(payload);
                    if (!groups.has_value()) return groups.error();
                    lvl.cell_groups = std::move(groups.value());
                    break;
                }
                case 8: {
                    auto environment = validate_environment(payload);
                    if (!environment.has_value()) return environment.error();
                    break;
                }
                case 9: {
                    if (payload.size() != 4) return format_error("bad BLK_WTHR size");
                    break;
                }
                case 10: {
                    auto doors = validate_doors(payload);
                    if (!doors.has_value()) return doors.error();
                    break;
                }
                case 11: {
                    if (payload.size() != 4) return format_error("bad BLK_LFLS size");
                    if (!block_reader.read_u32le(lvl.level_floors))
                        return format_error("truncated BLK_LFLS at offset " +
                                            std::to_string(block_reader.position()));
                    break;
                }
                default:
                    break;
            }
        }
        if (!reader.at_end()) return format_error("trailing data after BLK_LFLS");
        return std::move(lvl);
    } catch (const std::bad_alloc&) {
        return format_error("LVL allocation failed");
    }
}
}
