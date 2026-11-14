#include "gl/assets/save_payload.hpp"

#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"
#include "gl/assets/pak.hpp"

#include <miniz.h>

#include <array>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace gl::assets {
namespace {
constexpr std::array<std::string_view, 7> kBlockOrder = {
    "BT_SCRN ", "BT_INFO ", "BT_VARS ", "BT_TRIBE", "BT_DIARY", "BT_HSTRY", "BT_HERO "};

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

std::uint32_t u32_at(std::span<const std::uint8_t> data, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

bool read_string(ByteReader &reader, std::string &out) {
    std::uint32_t length = 0;
    if (!reader.read_u32le(length))
        return false;
    std::span<const std::uint8_t> bytes;
    if (!reader.read_bytes(length, bytes))
        return false;
    out = cp1251_to_utf8(bytes);
    return true;
}

bool skip_bytes(std::span<const std::uint8_t> data, std::size_t &cursor, std::size_t count) {
    if (cursor > data.size() || count > data.size() - cursor)
        return false;
    cursor += count;
    return true;
}

bool skip_u32(std::span<const std::uint8_t> data, std::size_t &cursor) {
    return skip_bytes(data, cursor, 4);
}

bool skip_stride(std::span<const std::uint8_t> data, std::size_t &cursor, std::uint32_t count,
                 std::size_t stride) {
    if (cursor > data.size())
        return false;
    const std::uint64_t bytes = static_cast<std::uint64_t>(count) * stride;
    if (bytes > data.size() - cursor)
        return false;
    cursor += static_cast<std::size_t>(bytes);
    return true;
}

bool skip_string(std::span<const std::uint8_t> data, std::size_t &cursor) {
    if (cursor > data.size() || data.size() - cursor < 4)
        return false;
    const std::uint32_t length = u32_at(data, cursor);
    cursor += 4;
    const std::size_t count = length < 0x104 ? length : 0x104;
    return skip_bytes(data, cursor, count);
}

bool skip_container(std::span<const std::uint8_t> data, std::size_t &cursor) {
    if (cursor > data.size() || data.size() - cursor < 4)
        return false;
    const std::uint32_t flag = u32_at(data, cursor);
    cursor += 4;
    if (flag == 0)
        return true;
    if (cursor > data.size() || data.size() - cursor < 4)
        return false;
    const std::uint32_t count = u32_at(data, cursor);
    cursor += 4;
    for (std::uint32_t index = 0; index < count; ++index) {
        if (!skip_bytes(data, cursor, 0xc0))
            return false;
        if (cursor > data.size() || data.size() - cursor < 4)
            return false;
        const std::uint32_t sub_count = u32_at(data, cursor);
        cursor += 4;
        if (!skip_stride(data, cursor, sub_count, 0x14))
            return false;
    }
    return true;
}

bool parse_rginf(std::span<const std::uint8_t> body, SaveRginf &out, std::string &detail) {
    if (body.size() != 36) {
        detail = "size " + std::to_string(body.size()) + " (expected 36)";
        return false;
    }
    out.latch = u32_at(body, 0);
    for (std::size_t index = 0; index < out.datetime.size(); ++index)
        out.datetime[index] = u32_at(body, 4 + 4 * index);
    return true;
}

bool parse_pnames(std::span<const std::uint8_t> body, std::vector<std::string> &out,
                  std::string &detail) {
    std::size_t cursor = 0;
    while (true) {
        if (cursor > body.size() || body.size() - cursor < 4) {
            detail = "length word at offset " + std::to_string(cursor) + " is truncated";
            return false;
        }
        const std::uint32_t length = u32_at(body, cursor);
        cursor += 4;
        if (length == 0) {
            if (cursor != body.size()) {
                detail = "terminator at offset " + std::to_string(cursor - 4) + " is not last (" +
                         std::to_string(body.size() - cursor) + " trailing bytes)";
                return false;
            }
            return true;
        }
        const std::size_t count = length < 0x104 ? length : 0x104;
        if (count > body.size() - cursor) {
            detail = "name length " + std::to_string(length) + " at offset " +
                     std::to_string(cursor - 4) + " overruns the body";
            return false;
        }
        out.push_back(cp1251_to_utf8(body.subspan(cursor, count)));
        cursor += count;
    }
}

bool skip_type2_tail(std::span<const std::uint8_t> data, std::size_t &cursor) {
    if (!skip_bytes(data, cursor, 0x80))
        return false;
    if (cursor > data.size() || data.size() - cursor < 4)
        return false;
    const std::uint32_t count = u32_at(data, cursor);
    cursor += 4;
    return skip_stride(data, cursor, count, 0x14);
}

bool parse_entities(std::span<const std::uint8_t> body, std::vector<SaveEntity> &out,
                    std::string &detail) {
    std::size_t cursor = 0;
    while (true) {
        if (cursor > body.size() || body.size() - cursor < 4) {
            detail = "type word at offset " + std::to_string(cursor) + " is truncated";
            return false;
        }
        const std::uint32_t type = u32_at(body, cursor);
        const std::size_t type_offset = cursor;
        cursor += 4;
        if (type == 1) {
            if (cursor != body.size()) {
                detail = "terminator at offset " + std::to_string(type_offset) + " is not last (" +
                         std::to_string(body.size() - cursor) + " trailing bytes)";
                return false;
            }
            return true;
        }
        const std::size_t start = cursor;
        bool ok = false;
        bool known = true;
        switch (type) {
        case 8:
            ok = skip_bytes(body, cursor, 8) && skip_container(body, cursor);
            break;
        case 0x10:
            ok = skip_string(body, cursor) && skip_u32(body, cursor) && skip_u32(body, cursor) &&
                 skip_string(body, cursor) && skip_string(body, cursor) && skip_u32(body, cursor) &&
                 skip_string(body, cursor) && skip_u32(body, cursor) &&
                 skip_container(body, cursor);
            break;
        case 0x20:
            ok = skip_string(body, cursor) && skip_u32(body, cursor) && skip_u32(body, cursor) &&
                 skip_string(body, cursor) && skip_string(body, cursor) && skip_u32(body, cursor) &&
                 skip_u32(body, cursor);
            break;
        case 2:
            ok = skip_string(body, cursor) && skip_u32(body, cursor) && skip_u32(body, cursor) &&
                 skip_bytes(body, cursor, 8) && skip_u32(body, cursor) && skip_u32(body, cursor) &&
                 skip_u32(body, cursor) && skip_bytes(body, cursor, 0x20) &&
                 skip_u32(body, cursor) && skip_u32(body, cursor) && skip_string(body, cursor) &&
                 skip_u32(body, cursor) && skip_u32(body, cursor) && skip_u32(body, cursor) &&
                 skip_u32(body, cursor) && skip_string(body, cursor) && skip_string(body, cursor) &&
                 skip_u32(body, cursor) && skip_bytes(body, cursor, 0x11c) &&
                 skip_bytes(body, cursor, 4) && skip_u32(body, cursor) && skip_u32(body, cursor) &&
                 skip_u32(body, cursor) && skip_u32(body, cursor) && skip_container(body, cursor) &&
                 skip_container(body, cursor) && skip_container(body, cursor) &&
                 skip_type2_tail(body, cursor);
            break;
        default:
            known = false;
            break;
        }
        if (!ok) {
            detail = "type " + std::to_string(type) + " at offset " + std::to_string(type_offset) +
                     (known ? " is truncated" : " is unknown");
            return false;
        }
        SaveEntity entity;
        entity.type = type;
        const auto bytes = body.subspan(start, cursor - start);
        entity.body.assign(bytes.begin(), bytes.end());
        out.push_back(std::move(entity));
    }
}

bool parse_tail_section(SaveTailEntry &entry, std::vector<std::uint8_t> *rginf_raw,
                        std::vector<std::uint8_t> *ents_raw, std::string &detail) {
    ByteReader snapshot(entry.section);
    bool have_rginf = false;
    bool have_pname = false;
    bool have_ents = false;
    while (!snapshot.at_end()) {
        const std::size_t tag_offset = snapshot.position();
        std::span<const std::uint8_t> tag;
        if (!snapshot.read_bytes(8, tag)) {
            detail = "tag at offset " + std::to_string(tag_offset) + " is truncated";
            return false;
        }
        std::uint32_t size = 0;
        if (!snapshot.read_u32le(size)) {
            detail = "tag at offset " + std::to_string(tag_offset) + " has a truncated size";
            return false;
        }
        std::span<const std::uint8_t> body;
        if (!snapshot.read_bytes(size, body)) {
            detail = "tag at offset " + std::to_string(tag_offset) + " has a truncated body";
            return false;
        }
        const std::string tag_text(reinterpret_cast<const char *>(tag.data()), tag.size());
        if (tag_text == "BT_RGINF") {
            if (have_rginf) {
                detail = "duplicate BT_RGINF at offset " + std::to_string(tag_offset);
                return false;
            }
            if (!parse_rginf(body, entry.rginf, detail)) {
                detail = "BT_RGINF " + detail;
                return false;
            }
            if (rginf_raw)
                rginf_raw->assign(body.begin(), body.end());
            have_rginf = true;
        } else if (tag_text == "BT_PNAME") {
            if (have_pname) {
                detail = "duplicate BT_PNAME at offset " + std::to_string(tag_offset);
                return false;
            }
            if (!parse_pnames(body, entry.person_names, detail)) {
                detail = "BT_PNAME " + detail;
                return false;
            }
            have_pname = true;
        } else if (tag_text == "BT_ENTS ") {
            if (have_ents) {
                detail = "duplicate BT_ENTS  at offset " + std::to_string(tag_offset);
                return false;
            }
            if (!parse_entities(body, entry.entities, detail)) {
                detail = "BT_ENTS " + detail;
                return false;
            }
            if (ents_raw)
                ents_raw->assign(body.begin(), body.end());
            have_ents = true;
        }
    }
    if (!have_rginf || !have_pname || !have_ents) {
        detail = "missing BT_RGINF/BT_PNAME/BT_ENTS ";
        return false;
    }
    return true;
}
} // namespace

Result<SavePayload> parse_save_payload(std::span<const std::uint8_t> file) {
    try {
        auto header = parse_save_header(file);
        if (!header)
            return header.error();
        if (file.size() < 0x70)
            return format_error("save payload is truncated");

        const std::uint32_t raw_len = u32_at(file, 0x68);
        const std::uint32_t compressed_len = u32_at(file, 0x6c);
        if (raw_len == 0 || compressed_len == 0 ||
            static_cast<std::size_t>(raw_len) > kMaxEntrySize ||
            static_cast<std::size_t>(compressed_len) > file.size() - 0x70)
            return format_error("save payload zlib length is invalid");

        SavePayload payload;
        payload.header = header.value();
        payload.decompressed.resize(raw_len);
        mz_ulong out_len = raw_len;
        if (mz_uncompress(payload.decompressed.data(), &out_len, file.data() + 0x70,
                          compressed_len) != MZ_OK)
            return format_error("save payload zlib decompression failed");
        if (out_len != raw_len)
            return format_error("save payload uncompressed length mismatch");

        ByteReader blocks(payload.decompressed);
        for (std::size_t index = 0; index < kBlockOrder.size(); ++index) {
            std::span<const std::uint8_t> tag;
            if (!blocks.read_bytes(8, tag))
                return format_error("save payload block tag is truncated");
            const std::string tag_text(reinterpret_cast<const char *>(tag.data()), tag.size());
            if (tag_text != kBlockOrder[index])
                return format_error("save payload block order mismatch");
            std::uint32_t size = 0;
            if (!blocks.read_u32le(size))
                return format_error("save payload block size is truncated");
            std::span<const std::uint8_t> body;
            if (!blocks.read_bytes(size, body))
                return format_error("save payload block body is truncated");
            payload.blocks.push_back(SaveBlockView{
                tag_text, static_cast<std::uint32_t>(body.data() - payload.decompressed.data()),
                size});
        }
        if (!blocks.at_end())
            return format_error("save payload has trailing bytes");

        const SaveBlockView &info_view = payload.blocks[1];
        ByteReader info(std::span<const std::uint8_t>(payload.decompressed)
                            .subspan(info_view.offset, info_view.size));
        std::string session;
        std::string description;
        if (!read_string(info, session) || !info.read_u32le(payload.info.field_218))
            return format_error("BT_INFO header is truncated");
        std::span<const std::uint8_t> time;
        if (!info.read_bytes(payload.info.time.size(), time))
            return format_error("BT_INFO time blob is truncated");
        std::memcpy(payload.info.time.data(), time.data(), time.size());
        if (!read_string(info, description) || !read_string(info, payload.info.level_id))
            return format_error("BT_INFO strings are truncated");
        if (payload.info.level_id.empty())
            return format_error("BT_INFO level id is empty");
        std::uint32_t party = 0;
        if (!info.read_u32le(party))
            return format_error("BT_INFO party level is truncated");
        payload.info.party_level = static_cast<std::int32_t>(party);
        std::span<const std::uint8_t> tail8;
        if (!info.read_bytes(payload.info.field_26c8.size(), tail8))
            return format_error("BT_INFO tail is truncated");
        std::memcpy(payload.info.field_26c8.data(), tail8.data(), tail8.size());
        if (!info.at_end())
            return format_error("BT_INFO has trailing bytes");

        const SaveBlockView &vars_view = payload.blocks[2];
        ByteReader vars(std::span<const std::uint8_t>(payload.decompressed)
                            .subspan(vars_view.offset, vars_view.size));
        std::uint32_t var_count = 0;
        if (!vars.read_u32le(var_count))
            return format_error("BT_VARS count is truncated");
        for (std::uint32_t index = 0; index < var_count; ++index) {
            SaveVar var;
            if (!vars.read_u32le(var.type))
                return format_error("BT_VARS type is truncated");
            if (var.type > 3)
                return format_error("BT_VARS type is unknown");
            if (!read_string(vars, var.name))
                return format_error("BT_VARS name is truncated");
            if (var.type < 3) {
                std::span<const std::uint8_t> value;
                if (!vars.read_bytes(var.value.size(), value))
                    return format_error("BT_VARS value is truncated");
                std::memcpy(var.value.data(), value.data(), value.size());
            } else if (!read_string(vars, var.text_value)) {
                return format_error("BT_VARS text value is truncated");
            }
            payload.vars.push_back(std::move(var));
        }
        if (!vars.at_end())
            return format_error("BT_VARS has trailing bytes");

        const std::size_t tail_offset = 0x70 + static_cast<std::size_t>(compressed_len);
        ByteReader tail(file.subspan(tail_offset));
        std::uint32_t tail_count = 0;
        if (!tail.read_u32le(tail_count))
            return format_error("save payload tail count is truncated");
        for (std::uint32_t index = 0; index < tail_count; ++index) {
            SaveTailEntry entry;
            if (!read_string(tail, entry.name))
                return format_error("save payload tail name is truncated");
            std::uint32_t section_len = 0;
            if (!tail.read_u32le(section_len))
                return format_error("save payload tail section length is truncated");
            std::span<const std::uint8_t> section_bytes;
            if (!tail.read_bytes(section_len, section_bytes))
                return format_error("save payload tail section is truncated");
            if (section_bytes.size() < 8)
                return format_error("save payload tail section header is truncated");
            const std::uint32_t section_raw_len = u32_at(section_bytes, 0);
            const std::uint32_t section_zlib_len = u32_at(section_bytes, 4);
            if (section_raw_len == 0 || section_raw_len > kMaxEntrySize ||
                static_cast<std::size_t>(section_zlib_len) != section_bytes.size() - 8)
                return format_error("save payload tail section length is invalid");
            entry.raw_len = section_raw_len;
            entry.section.resize(section_raw_len);
            mz_ulong section_out_len = section_raw_len;
            if (mz_uncompress(entry.section.data(), &section_out_len, section_bytes.data() + 8,
                              section_zlib_len) != MZ_OK)
                return format_error("save payload tail section zlib decompression failed");
            if (section_out_len != section_raw_len)
                return format_error("save payload tail section length mismatch");
            payload.tail.push_back(std::move(entry));
        }
        if (!tail.at_end())
            return format_error("save payload tail has trailing bytes");

        for (std::size_t index = 0; index < payload.tail.size(); ++index) {
            if (payload.tail[index].name == payload.info.level_id) {
                payload.level_tail_index = index;
                break;
            }
        }
        if (!payload.level_tail_index)
            return format_error("save payload tail is missing the selected level");

        for (std::size_t index = 0; index < payload.tail.size(); ++index) {
            const bool selected = index == *payload.level_tail_index;
            std::string detail;
            if (!parse_tail_section(payload.tail[index], selected ? &payload.rginf : nullptr,
                                    selected ? &payload.ents : nullptr, detail))
                return format_error("save payload tail '" + payload.tail[index].name +
                                    "': " + detail);
        }
        return payload;
    } catch (const std::bad_alloc &) {
        return format_error("save payload allocation failed");
    } catch (const std::length_error &) {
        return format_error("save payload allocation failed");
    }
}
} // namespace gl::assets
