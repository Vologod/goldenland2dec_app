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
        return format_error("LVL block " + std::string(expected) + " payload exceeds file");
    return payload;
}

Result<std::vector<LvlMask>> parse_masks(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    std::uint32_t count = 0;
    if (!reader.read_u32le(count)) return format_error("truncated BLK_MDSC count");
    if (static_cast<std::uint64_t>(count) * 16 + 4 != payload.size())
        return format_error("BLK_MDSC count does not match payload size");
    std::vector<LvlMask> masks;
    masks.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        LvlMask mask;
        std::uint32_t padding = 0;
        if (!reader.read_u32le(padding) || !reader.read_u32le(mask.number) ||
            !reader.read_u32le(mask.x) || !reader.read_u32le(mask.y))
            return format_error("truncated BLK_MDSC entry");
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
        return format_error("truncated " + std::string(block_name) + " count");
    if (payload.size() < 4 || count > (payload.size() - 4) / 20)
        return format_error(std::string(block_name) + " count exceeds payload");
    std::vector<LvlDescription> descriptions;
    descriptions.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        LvlDescription description;
        std::uint32_t raw_x = 0, raw_y = 0;
        if (!reader.read_u16le(description.param1) || !reader.read_u16le(description.param2) ||
            !reader.read_u32le(description.number) || !reader.read_u32le(raw_x) ||
            !reader.read_u32le(raw_y))
            return format_error("truncated " + std::string(block_name) + " entry");
        description.x = static_cast<std::int32_t>(raw_x);
        description.y = static_cast<std::int32_t>(raw_y);
        if (!read_lvl_string(reader, description.name))
            return format_error("truncated " + std::string(block_name) + " string");
        descriptions.push_back(std::move(description));
    }
    if (!reader.at_end())
        return format_error(std::string(block_name) + " has trailing bytes");
    return descriptions;
}

Result<std::vector<LvlCellGroup>> parse_cell_groups(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    std::uint32_t count = 0;
    if (!reader.read_u32le(count)) return format_error("truncated BLK_CGRP count");
    if (payload.size() < 4 || count > (payload.size() - 4) / 8)
        return format_error("BLK_CGRP count exceeds payload");
    std::vector<LvlCellGroup> groups;
    groups.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        LvlCellGroup group;
        if (!read_lvl_string(reader, group.name))
            return format_error("truncated BLK_CGRP name");
        std::uint32_t cells = 0;
        if (!reader.read_u32le(cells))
            return format_error("truncated BLK_CGRP cell count");
        if (cells > reader.remaining().size() / 4)
            return format_error("BLK_CGRP cell count exceeds payload");
        group.cells.reserve(cells);
        for (std::uint32_t j = 0; j < cells; ++j) {
            std::uint16_t x = 0, y = 0;
            if (!reader.read_u16le(x) || !reader.read_u16le(y))
                return format_error("truncated BLK_CGRP cell");
            group.cells.emplace_back(x, y);
        }
        groups.push_back(std::move(group));
    }
    if (!reader.at_end()) return format_error("BLK_CGRP has trailing bytes");
    return groups;
}

bool validate_environment(std::span<const std::uint8_t> payload) {
    if (payload.size() < 32) return false;
    ByteReader reader(payload);
    if (!reader.skip(20)) return false;
    std::string ignored;
    for (int i = 0; i < 3; ++i)
        if (!read_lvl_string(reader, ignored)) return false;
    return true;
}

bool validate_doors(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    std::uint32_t count = 0;
    if (!reader.read_u32le(count)) return false;
    if (count > (payload.size() - 4) / 24) return false;
    std::string ignored;
    for (std::uint32_t i = 0; i < count; ++i)
        for (int j = 0; j < 6; ++j)
            if (!read_lvl_string(reader, ignored)) return false;
    return reader.at_end();
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
                        return format_error("truncated BLK_LVER");
                    break;
                }
                case 1: {
                    if (payload.size() != 8) return format_error("bad BLK_MPSZ size");
                    if (!block_reader.read_u32le(lvl.map_size.pixel_width) ||
                        !block_reader.read_u32le(lvl.map_size.pixel_height))
                        return format_error("truncated BLK_MPSZ");
                    break;
                }
                case 2: {
                    if (payload.size() < 8) return format_error("bad BLK_MHDR size");
                    if (!block_reader.read_u32le(lvl.chunk_width) ||
                        !block_reader.read_u32le(lvl.chunk_height))
                        return format_error("truncated BLK_MHDR header");
                    const std::uint64_t chunk_count =
                        static_cast<std::uint64_t>(lvl.chunk_width) * lvl.chunk_height;
                    if (chunk_count * 24 + 8 != payload.size())
                        return format_error("BLK_MHDR size does not match chunk dimensions");
                    lvl.chunks.resize(static_cast<std::size_t>(chunk_count));
                    for (auto& chunk : lvl.chunks) {
                        for (auto& tile : chunk) {
                            if (!block_reader.read_u16le(tile.relief) ||
                                !block_reader.read_u16le(tile.sound) ||
                                !block_reader.read_u16le(tile.mask))
                                return format_error("truncated BLK_MHDR tile");
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
                    if (!validate_environment(payload))
                        return format_error("bad BLK_SENV payload");
                    break;
                }
                case 9: {
                    if (payload.size() != 4) return format_error("bad BLK_WTHR size");
                    break;
                }
                case 10: {
                    if (!validate_doors(payload)) return format_error("bad BLK_DOOR payload");
                    break;
                }
                case 11: {
                    if (payload.size() != 4) return format_error("bad BLK_LFLS size");
                    if (!block_reader.read_u32le(lvl.level_floors))
                        return format_error("truncated BLK_LFLS");
                    break;
                }
                default:
                    break;
            }
        }
        if (!reader.at_end()) return format_error("trailing data after BLK_LFLS");
        return lvl;
    } catch (const std::bad_alloc&) {
        return format_error("LVL allocation failed");
    }
}
}
