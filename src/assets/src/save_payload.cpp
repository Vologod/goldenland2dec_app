#include "gl/assets/save_payload.hpp"

#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"
#include "gl/assets/pak.hpp"

#include <miniz.h>

#include <array>
#include <bit>
#include <cstring>
#include <limits>
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

void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}

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

bool parse_tribe(std::span<const std::uint8_t> body, SaveTribeTable &out, std::string &detail) {
    ByteReader reader(body);
    std::uint32_t count = 0;
    if (!reader.read_u32le(count)) {
        detail = "count is truncated";
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        SaveTribeEntry entry;
        if (!reader.read_u32le(entry.id)) {
            detail = "entry " + std::to_string(index) + " id is truncated";
            return false;
        }
        if (!read_string(reader, entry.name)) {
            detail = "entry " + std::to_string(index) + " name is truncated";
            return false;
        }
        out.entries.push_back(std::move(entry));
    }
    const std::uint64_t matrix = static_cast<std::uint64_t>(count) * count;
    if (matrix != reader.remaining().size()) {
        detail = matrix > reader.remaining().size()
                     ? "relation matrix is truncated (" + std::to_string(matrix) +
                           " bytes expected)"
                     : "relation matrix has trailing bytes";
        return false;
    }
    out.matrix.assign(reader.remaining().begin(), reader.remaining().end());
    return true;
}

bool parse_diary(std::span<const std::uint8_t> body,
                 std::vector<SaveDiaryEntry> &entries,
                 std::vector<SaveBlock58> &sparse, std::string &detail) {
    ByteReader reader(body);
    std::uint32_t count = 0;
    if (!reader.read_u32le(count)) {
        detail = "entry count is truncated";
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t blocks = 0;
        if (!reader.read_u32le(blocks)) {
            detail = "entry " + std::to_string(index) + " block count is truncated";
            return false;
        }
        if (blocks == 0) {
            detail = "entry " + std::to_string(index) + " has no header";
            return false;
        }
        const std::uint64_t size = static_cast<std::uint64_t>(blocks) * 0x58;
        if (size > reader.remaining().size()) {
            detail = "entry " + std::to_string(index) + " is truncated";
            return false;
        }
        std::span<const std::uint8_t> bytes;
        if (!reader.read_bytes(static_cast<std::size_t>(size), bytes)) {
            detail = "entry " + std::to_string(index) + " is truncated";
            return false;
        }
        SaveDiaryEntry entry;
        entry.key = u32_at(bytes, 0);
        entry.declared_count = u32_at(bytes, 4);
        entry.blocks.resize(blocks);
        for (std::uint32_t block = 0; block < blocks; ++block)
            std::memcpy(entry.blocks[block].data(),
                        bytes.data() + static_cast<std::size_t>(block) * 0x58, 0x58);
        entries.push_back(std::move(entry));
    }
    std::uint32_t sparse_count = 0;
    if (!reader.read_u32le(sparse_count)) {
        detail = "sparse count is truncated";
        return false;
    }
    const std::uint64_t size = static_cast<std::uint64_t>(sparse_count) * 0x58;
    if (size > reader.remaining().size()) {
        detail = "sparse records are truncated";
        return false;
    }
    std::span<const std::uint8_t> bytes;
    if (!reader.read_bytes(static_cast<std::size_t>(size), bytes)) {
        detail = "sparse records are truncated";
        return false;
    }
    sparse.resize(sparse_count);
    for (std::uint32_t block = 0; block < sparse_count; ++block)
        std::memcpy(sparse[block].data(),
                    bytes.data() + static_cast<std::size_t>(block) * 0x58, 0x58);
    if (!reader.at_end()) {
        detail = "has trailing bytes";
        return false;
    }
    return true;
}

bool parse_history(std::span<const std::uint8_t> body, SaveHistory &history,
                   std::string &detail) {
    ByteReader reader(body);
    if (!reader.read_u32le(history.head)) {
        detail = "head is truncated";
        return false;
    }
    for (auto &slot : history.slots) {
        if (!read_string(reader, slot.text)) {
            detail = "slot text is truncated";
            return false;
        }
        if (!reader.read_u32le(slot.id)) {
            detail = "slot id is truncated";
            return false;
        }
    }
    if (!reader.read_u32le(history.tail)) {
        detail = "tail is truncated";
        return false;
    }
    if (!reader.at_end()) {
        detail = "has trailing bytes";
        return false;
    }
    return true;
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

std::optional<std::size_t> SaveTribeTable::index_of(std::uint32_t id) const {
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (entries[index].id == id)
            return index;
    }
    return std::nullopt;
}

const SaveTribeEntry *SaveTribeTable::find(std::uint32_t id) const {
    const auto index = index_of(id);
    return index ? &entries[*index] : nullptr;
}

const SaveTribeEntry *SaveTribeTable::find(std::string_view name) const {
    for (const SaveTribeEntry &entry : entries) {
        if (entry.name == name)
            return &entry;
    }
    return nullptr;
}

std::int32_t SaveTribeTable::relation(std::uint32_t id_a, std::uint32_t id_b) const {
    const auto a = index_of(id_a);
    const auto b = index_of(id_b);
    if (!a || !b || matrix.size() != entries.size() * entries.size())
        return 2;
    return matrix[*a * entries.size() + *b];
}

std::span<const std::uint8_t> SaveTribeTable::row(std::size_t index) const {
    if (index >= entries.size() || matrix.size() != entries.size() * entries.size())
        return {};
    return std::span<const std::uint8_t>(matrix).subspan(index * entries.size(),
                                                         entries.size());
}

std::optional<double> save_var_number(const SaveVar &var) {
    if (var.type >= 3)
        return std::nullopt;
    ByteReader reader(var.value);
    std::uint64_t bits = 0;
    if (!reader.read_u64le(bits))
        return std::nullopt;
    return std::bit_cast<double>(bits);
}

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
        std::memcpy(payload.header_raw.data(), file.data(), payload.header_raw.size());
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
        if (!read_string(info, payload.info.session) ||
            !info.read_u32le(payload.info.field_218))
            return format_error("BT_INFO header is truncated");
        std::span<const std::uint8_t> time;
        if (!info.read_bytes(payload.info.time.size(), time))
            return format_error("BT_INFO time blob is truncated");
        std::memcpy(payload.info.time.data(), time.data(), time.size());
        if (!read_string(info, payload.info.description) ||
            !read_string(info, payload.info.level_id))
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

        const SaveBlockView &tribe_view = payload.blocks[3];
        std::string tribe_detail;
        if (!parse_tribe(std::span<const std::uint8_t>(payload.decompressed)
                             .subspan(tribe_view.offset, tribe_view.size),
                         payload.tribes, tribe_detail))
            return format_error("BT_TRIBE " + tribe_detail);

        const SaveBlockView &diary_view = payload.blocks[4];
        std::string diary_detail;
        if (!parse_diary(std::span<const std::uint8_t>(payload.decompressed)
                             .subspan(diary_view.offset, diary_view.size),
                         payload.diary, payload.diary_sparse, diary_detail))
            return format_error("BT_DIARY " + diary_detail);

        const SaveBlockView &history_view = payload.blocks[5];
        std::string history_detail;
        if (!parse_history(std::span<const std::uint8_t>(payload.decompressed)
                               .subspan(history_view.offset, history_view.size),
                           payload.history, history_detail))
            return format_error("BT_HSTRY " + history_detail);

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

Result<std::vector<std::uint8_t>> encode_tail_section(const SaveRginf &rginf,
                                                      std::span<const std::string> names,
                                                      std::span<const SaveEntity> entities) {
    try {
        std::vector<std::uint8_t> out;
        const auto append_tag = [&out](std::string_view tag, std::uint32_t size) {
            out.insert(out.end(), tag.begin(), tag.end());
            append_u32(out, size);
        };

        std::vector<std::uint8_t> pnames;
        for (const std::string &name : names) {
            if (name.empty())
                return format_error("tail person name is empty");
            if (name.find('\0') != std::string::npos)
                return format_error("tail person name contains a NUL");
            const std::string encoded = utf8_to_cp1251(name);
            if (encoded.size() > std::numeric_limits<std::uint32_t>::max())
                return format_error("tail person name is too long");
            append_u32(pnames, static_cast<std::uint32_t>(encoded.size()));
            pnames.insert(pnames.end(), encoded.begin(), encoded.end());
        }
        append_u32(pnames, 0);
        append_tag("BT_PNAME", static_cast<std::uint32_t>(pnames.size()));
        out.insert(out.end(), pnames.begin(), pnames.end());

        append_tag("BT_RGINF", 36);
        append_u32(out, rginf.latch);
        for (std::uint32_t word : rginf.datetime)
            append_u32(out, word);

        std::vector<std::uint8_t> ents;
        for (const SaveEntity &entity : entities) {
            if (entity.type == 1)
                return format_error("tail entity type 1 is a terminator");
            if (entity.body.size() > std::numeric_limits<std::uint32_t>::max())
                return format_error("tail entity body is too large");
            append_u32(ents, entity.type);
            ents.insert(ents.end(), entity.body.begin(), entity.body.end());
        }
        append_u32(ents, 1);
        append_tag("BT_ENTS ", static_cast<std::uint32_t>(ents.size()));
        out.insert(out.end(), ents.begin(), ents.end());
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("tail section allocation failed");
    } catch (const std::length_error &) {
        return format_error("tail section allocation failed");
    }
}

Result<std::vector<std::uint8_t>> encode_info_body(const SaveInfo &info) {
    try {
        if (info.level_id.empty())
            return format_error("BT_INFO level id is empty");
        std::vector<std::uint8_t> out;
        const auto put_string = [&out](const std::string &text) -> bool {
            const std::string encoded = utf8_to_cp1251(text);
            if (encoded.size() > std::numeric_limits<std::uint32_t>::max())
                return false;
            append_u32(out, static_cast<std::uint32_t>(encoded.size()));
            out.insert(out.end(), encoded.begin(), encoded.end());
            return true;
        };
        if (!put_string(info.session))
            return format_error("BT_INFO session is too long");
        append_u32(out, info.field_218);
        out.insert(out.end(), info.time.begin(), info.time.end());
        if (!put_string(info.description))
            return format_error("BT_INFO description is too long");
        if (!put_string(info.level_id))
            return format_error("BT_INFO level id is too long");
        append_u32(out, static_cast<std::uint32_t>(info.party_level));
        out.insert(out.end(), info.field_26c8.begin(), info.field_26c8.end());
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("BT_INFO allocation failed");
    } catch (const std::length_error &) {
        return format_error("BT_INFO allocation failed");
    }
}

VoidResult update_info_block(SavePayload &payload) {
    auto body = encode_info_body(payload.info);
    if (!body)
        return body.error();
    return replace_block_body(payload, 1, body.value());
}

Result<SaveTailEntry> parse_tail_section_entry(std::string_view name,
                                               std::span<const std::uint8_t> section) {
    if (name.empty())
        return format_error("tail section name is empty");
    if (section.empty())
        return format_error("tail section is empty");
    if (section.size() > std::numeric_limits<std::uint32_t>::max())
        return format_error("tail section is too large");
    try {
        SaveTailEntry entry;
        entry.name = std::string(name);
        entry.raw_len = static_cast<std::uint32_t>(section.size());
        entry.section.assign(section.begin(), section.end());
        std::string detail;
        if (!parse_tail_section(entry, nullptr, nullptr, detail))
            return format_error("tail section '" + entry.name + "': " + detail);
        return entry;
    } catch (const std::bad_alloc &) {
        return format_error("tail section allocation failed");
    } catch (const std::length_error &) {
        return format_error("tail section allocation failed");
    }
}

Result<std::size_t> upsert_tail_entry(SavePayload &payload, SaveTailEntry entry) {
    if (entry.name.empty())
        return format_error("tail entry name is empty");
    if (entry.section.empty())
        return format_error("tail entry section is empty");
    if (entry.raw_len == 0)
        entry.raw_len = static_cast<std::uint32_t>(entry.section.size());
    if (entry.raw_len != entry.section.size())
        return format_error("tail entry raw length mismatch");
    for (std::size_t index = 0; index < payload.tail.size(); ++index) {
        if (payload.tail[index].name == entry.name) {
            payload.tail[index] = std::move(entry);
            payload.level_tail_index = index;
            return index;
        }
    }
    payload.tail.push_back(std::move(entry));
    payload.level_tail_index = payload.tail.size() - 1;
    return payload.tail.size() - 1;
}

Result<std::size_t> set_current_level(SavePayload &payload, std::string level_id) {
    if (level_id.empty())
        return format_error("BT_INFO level id is empty");
    for (std::size_t index = 0; index < payload.tail.size(); ++index) {
        if (payload.tail[index].name == level_id) {
            payload.info.level_id = std::move(level_id);
            payload.level_tail_index = index;
            return index;
        }
    }
    return format_error("BT_INFO level id has no tail entry");
}

Result<std::vector<std::uint8_t>> encode_save_payload(const SavePayload &payload) {
    try {
        if (payload.blocks.size() != kBlockOrder.size())
            return format_error("save payload must contain seven blocks");

        std::vector<std::uint8_t> raw;
        for (std::size_t index = 0; index < payload.blocks.size(); ++index) {
            const SaveBlockView &block = payload.blocks[index];
            if (block.tag != kBlockOrder[index])
                return format_error("save payload block order mismatch");
            const std::uint64_t end =
                static_cast<std::uint64_t>(block.offset) + block.size;
            if (end > payload.decompressed.size())
                return format_error("save payload block body is out of range");
            raw.insert(raw.end(), block.tag.begin(), block.tag.end());
            append_u32(raw, block.size);
            raw.insert(raw.end(),
                       payload.decompressed.begin() + static_cast<std::ptrdiff_t>(block.offset),
                       payload.decompressed.begin() + static_cast<std::ptrdiff_t>(end));
        }
        if (raw.size() > std::numeric_limits<std::uint32_t>::max())
            return format_error("save payload is too large");

        const auto zlib_compress = [](const std::vector<std::uint8_t> &data,
                                      std::vector<std::uint8_t> &out) {
            out.resize(mz_compressBound(static_cast<mz_ulong>(data.size())));
            mz_ulong length = static_cast<mz_ulong>(out.size());
            if (mz_compress(out.data(), &length, data.data(),
                            static_cast<mz_ulong>(data.size())) != MZ_OK)
                return false;
            out.resize(static_cast<std::size_t>(length));
            return true;
        };

        std::vector<std::uint8_t> compressed;
        if (!zlib_compress(raw, compressed))
            return format_error("save payload zlib compression failed");
        if (compressed.size() > std::numeric_limits<std::uint32_t>::max())
            return format_error("save payload is too large");

        std::vector<std::uint8_t> file(payload.header_raw.begin(), payload.header_raw.end());
        append_u32(file, static_cast<std::uint32_t>(raw.size()));
        append_u32(file, static_cast<std::uint32_t>(compressed.size()));
        file.insert(file.end(), compressed.begin(), compressed.end());

        append_u32(file, static_cast<std::uint32_t>(payload.tail.size()));
        for (const SaveTailEntry &entry : payload.tail) {
            if (entry.raw_len == 0 || entry.raw_len != entry.section.size())
                return format_error("save tail section length mismatch");
            const std::string name = utf8_to_cp1251(entry.name);
            append_u32(file, static_cast<std::uint32_t>(name.size()));
            file.insert(file.end(), name.begin(), name.end());

            std::vector<std::uint8_t> section_compressed;
            if (!zlib_compress(entry.section, section_compressed))
                return format_error("save tail section zlib compression failed");
            if (section_compressed.size() > std::numeric_limits<std::uint32_t>::max() - 8u)
                return format_error("save tail section is too large");
            append_u32(file, static_cast<std::uint32_t>(8 + section_compressed.size()));
            append_u32(file, entry.raw_len);
            append_u32(file, static_cast<std::uint32_t>(section_compressed.size()));
            file.insert(file.end(), section_compressed.begin(), section_compressed.end());
        }
        return file;
    } catch (const std::bad_alloc &) {
        return format_error("save payload allocation failed");
    } catch (const std::length_error &) {
        return format_error("save payload allocation failed");
    }
}

Result<std::vector<std::uint8_t>> encode_vars_body(std::span<const SaveVar> vars) {
    try {
        if (vars.size() > std::numeric_limits<std::uint32_t>::max())
            return format_error("BT_VARS has too many entries");
        std::vector<std::uint8_t> out;
        append_u32(out, static_cast<std::uint32_t>(vars.size()));
        for (const SaveVar &var : vars) {
            if (var.type > 3)
                return format_error("BT_VARS type is unknown");
            append_u32(out, var.type);
            const std::string name = utf8_to_cp1251(var.name);
            append_u32(out, static_cast<std::uint32_t>(name.size()));
            out.insert(out.end(), name.begin(), name.end());
            if (var.type < 3) {
                out.insert(out.end(), var.value.begin(), var.value.end());
            } else {
                const std::string text = utf8_to_cp1251(var.text_value);
                append_u32(out, static_cast<std::uint32_t>(text.size()));
                out.insert(out.end(), text.begin(), text.end());
            }
        }
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("BT_VARS allocation failed");
    } catch (const std::length_error &) {
        return format_error("BT_VARS allocation failed");
    }
}

Result<std::vector<std::uint8_t>> encode_tribe_body(const SaveTribeTable &table) {
    try {
        if (table.entries.size() > std::numeric_limits<std::uint32_t>::max())
            return format_error("BT_TRIBE has too many entries");
        const std::uint64_t matrix_size =
            static_cast<std::uint64_t>(table.entries.size()) * table.entries.size();
        if (matrix_size != table.matrix.size())
            return format_error("BT_TRIBE matrix size mismatch");
        std::vector<std::uint8_t> out;
        append_u32(out, static_cast<std::uint32_t>(table.entries.size()));
        for (const SaveTribeEntry &entry : table.entries) {
            append_u32(out, entry.id);
            const std::string name = utf8_to_cp1251(entry.name);
            append_u32(out, static_cast<std::uint32_t>(name.size()));
            out.insert(out.end(), name.begin(), name.end());
        }
        out.insert(out.end(), table.matrix.begin(), table.matrix.end());
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("BT_TRIBE allocation failed");
    } catch (const std::length_error &) {
        return format_error("BT_TRIBE allocation failed");
    }
}

VoidResult replace_block_body(SavePayload &payload, std::size_t index,
                              std::span<const std::uint8_t> body) {
    try {
        if (index >= payload.blocks.size())
            return format_error("save block index is out of range");
        SaveBlockView &block = payload.blocks[index];
        if (block.tag != kBlockOrder[index])
            return format_error("save payload block order mismatch");
        const std::uint64_t offset = block.offset;
        const std::uint64_t old_size = block.size;
        if (offset < 12 || offset - 12 + 12 > payload.decompressed.size())
            return format_error("save block header is out of range");
        const std::size_t header = static_cast<std::size_t>(offset - 12);
        if (std::memcmp(payload.decompressed.data() + header, block.tag.data(), 8) != 0)
            return format_error("save block tag mismatch");
        if (u32_at(payload.decompressed, header + 8) != block.size)
            return format_error("save block size mismatch");
        if (body.size() > std::numeric_limits<std::uint32_t>::max())
            return format_error("save block body is too large");
        if (old_size > payload.decompressed.size() - static_cast<std::size_t>(offset))
            return format_error("save block body is out of range");

        const std::int64_t delta = static_cast<std::int64_t>(body.size()) -
                                   static_cast<std::int64_t>(old_size);
        const std::size_t suffix = static_cast<std::size_t>(offset + old_size);
        std::vector<std::uint8_t> rebuilt;
        rebuilt.insert(rebuilt.end(), payload.decompressed.begin(),
                       payload.decompressed.begin() + static_cast<std::ptrdiff_t>(header));
        rebuilt.insert(rebuilt.end(), block.tag.begin(), block.tag.end());
        append_u32(rebuilt, static_cast<std::uint32_t>(body.size()));
        rebuilt.insert(rebuilt.end(), body.begin(), body.end());
        rebuilt.insert(rebuilt.end(),
                       payload.decompressed.begin() + static_cast<std::ptrdiff_t>(suffix),
                       payload.decompressed.end());

        std::vector<std::uint32_t> moved_offsets;
        moved_offsets.reserve(payload.blocks.size() - index - 1);
        for (std::size_t later = index + 1; later < payload.blocks.size(); ++later) {
            const std::int64_t moved =
                static_cast<std::int64_t>(payload.blocks[later].offset) + delta;
            if (moved < 0 ||
                moved > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
                return format_error("save block offset is out of range");
            const std::uint64_t end =
                static_cast<std::uint64_t>(moved) + payload.blocks[later].size;
            if (end > rebuilt.size())
                return format_error("save block body is out of range");
            moved_offsets.push_back(static_cast<std::uint32_t>(moved));
        }

        for (std::size_t later = index + 1; later < payload.blocks.size(); ++later)
            payload.blocks[later].offset = moved_offsets[later - index - 1];
        block.size = static_cast<std::uint32_t>(body.size());
        payload.decompressed = std::move(rebuilt);
        return VoidResult{std::monostate{}};
    } catch (const std::bad_alloc &) {
        return format_error("save payload allocation failed");
    } catch (const std::length_error &) {
        return format_error("save payload allocation failed");
    }
}

VoidResult patch_save_header(SavePayload &payload, const SaveHeader &header) {
    const std::string title = utf8_to_cp1251(header.title);
    if (title.size() > 64)
        return Error{ErrorCode::format, "save title is longer than 64 bytes"};
    const auto put_u16 = [&payload](std::size_t offset, std::uint16_t value) {
        payload.header_raw[offset] = static_cast<std::uint8_t>(value & 0xFF);
        payload.header_raw[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    };
    payload.header_raw[0x14] = static_cast<std::uint8_t>(header.version & 0xFF);
    payload.header_raw[0x15] = static_cast<std::uint8_t>((header.version >> 8) & 0xFF);
    payload.header_raw[0x16] = static_cast<std::uint8_t>((header.version >> 16) & 0xFF);
    payload.header_raw[0x17] = static_cast<std::uint8_t>((header.version >> 24) & 0xFF);
    put_u16(0x18, header.year);
    put_u16(0x1a, header.month);
    put_u16(0x1c, header.weekday);
    put_u16(0x1e, header.day);
    put_u16(0x20, header.time_a);
    put_u16(0x22, header.time_b);
    put_u16(0x24, header.time_c);
    for (std::size_t index = 0; index < 64; ++index)
        payload.header_raw[0x28 + index] = 0;
    for (std::size_t index = 0; index < title.size(); ++index)
        payload.header_raw[0x28 + index] = static_cast<std::uint8_t>(title[index]);
    return VoidResult{std::monostate{}};
}
} // namespace gl::assets
