#include "gl/assets/cs.hpp"
#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"
#include <bit>
#include <cstddef>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace gl::assets {
namespace {
constexpr std::size_t kHeaderSize = 4;
constexpr std::size_t kMaxFunctionArgs = 9;
constexpr std::uint8_t kLegacyXorSeed = 0x55;

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

bool read_i32(ByteReader& reader, std::int32_t& out) {
    std::uint32_t raw = 0;
    if (!reader.read_u32le(raw)) return false;
    out = static_cast<std::int32_t>(raw);
    return true;
}

bool read_f64(ByteReader& reader, double& out) {
    std::uint64_t raw = 0;
    if (!reader.read_u64le(raw)) return false;
    out = std::bit_cast<double>(raw);
    return true;
}

bool read_cstring(ByteReader& reader, std::string& out) {
    const std::span<const std::uint8_t> bytes = reader.remaining();
    std::size_t length = 0;
    while (length < bytes.size() && bytes[length] != 0) ++length;
    if (length == bytes.size()) return false;
    out = cp1251_to_utf8(bytes.first(length));
    reader.skip(length + 1);
    return true;
}

Result<std::vector<CsNode>> parse_nodes(std::span<const std::uint8_t> data) {
    ByteReader reader(data);
    if (!reader.skip(kHeaderSize)) return format_error("truncated cs size field");

    std::vector<CsNode> nodes;
    while (!reader.at_end()) {
        CsNode node;
        if (!read_i32(reader, node.opcode)) return format_error("truncated cs opcode");
        if (!cs_opcode_valid(node.opcode))
            return format_error("invalid cs opcode " + std::to_string(node.opcode));

        if ((node.opcode >= 0 && node.opcode <= 20) || node.opcode == 50) {
            if (!read_i32(reader, node.a) || !read_i32(reader, node.b) ||
                !read_i32(reader, node.c) || !read_i32(reader, node.d))
                return format_error("truncated cs four-integer payload");
        } else if (node.opcode == 21 || node.opcode == 24) {
            if (!read_f64(reader, node.value))
                return format_error("truncated cs number payload");
        } else if (node.opcode == 22 || node.opcode == 23) {
            if (!read_cstring(reader, node.text))
                return format_error("unterminated or truncated cs string payload");
        } else if (node.opcode == 48) {
            if (!read_i32(reader, node.c) || !read_i32(reader, node.d) ||
                !read_f64(reader, node.value))
                return format_error("truncated cs function payload");

            bool terminated = false;
            for (std::size_t i = 0; i < kMaxFunctionArgs; ++i) {
                if (!read_i32(reader, node.args[i]))
                    return format_error("truncated cs function arguments");
                if (node.args[i] == -1) {
                    terminated = true;
                    break;
                }
            }
            if (!terminated)
                return format_error("cs function arguments are not terminated by -1");
        } else {
            if (!read_i32(reader, node.c) || !read_i32(reader, node.d))
                return format_error("truncated cs jump payload");
        }

        nodes.push_back(std::move(node));
    }

    return nodes;
}
} // namespace

Result<std::vector<CsNode>> parse_cs(std::span<const std::uint8_t> data) {
    try {
        if (data.size() < kHeaderSize)
            return format_error("cs data is shorter than the size field");

        ByteReader header(data);
        std::uint32_t file_size = 0;
        if (!header.read_u32le(file_size)) return format_error("truncated cs size field");
        if (static_cast<std::size_t>(file_size) != data.size())
            return format_error("cs size field does not match the data size");

        auto plain = parse_nodes(data);
        if (plain) return plain;

        bool first_opcode_invalid = false;
        if (data.size() >= kHeaderSize + 4) {
            std::int32_t first_opcode = 0;
            ByteReader body(data.subspan(kHeaderSize));
            first_opcode_invalid =
                read_i32(body, first_opcode) && !cs_opcode_valid(first_opcode);
        }
        if (!first_opcode_invalid) return plain;

        std::vector<std::uint8_t> decoded(data.begin(), data.end());
        for (std::size_t i = kHeaderSize; i < decoded.size(); ++i)
            decoded[i] ^= static_cast<std::uint8_t>(i + kLegacyXorSeed);
        auto legacy = parse_nodes(decoded);
        if (legacy) return legacy;

        return format_error("cs parse failed: raw: " + plain.error().message +
                            "; legacy xor: " + legacy.error().message);
    } catch (const std::bad_alloc&) {
        return format_error("cs allocation failed");
    }
}

const char* cs_opcode_name(std::int32_t opcode) {
    switch (opcode) {
    case 0: return "||";
    case 1: return "^^";
    case 2: return "&&";
    case 3: return "|";
    case 4: return "^";
    case 5: return "&";
    case 6: return "!=";
    case 7: return "==";
    case 8: return ">=";
    case 9: return "<=";
    case 10: return ">";
    case 11: return "<";
    case 12: return "<<";
    case 13: return ">>";
    case 14: return "+";
    case 15: return "-";
    case 16: return "*";
    case 17: return "/";
    case 18: return "%";
    case 19: return "~";
    case 20: return "!";
    case 21: return "num_var";
    case 22: return "str";
    case 23: return "str_var";
    case 24: return "num";
    case 48: return "func";
    case 49: return "jmp";
    case 50: return "assign";
    default: return "unknown";
    }
}

bool cs_opcode_valid(std::int32_t opcode) {
    return (opcode >= 0 && opcode <= 24) || opcode == 48 || opcode == 49 || opcode == 50;
}

bool cs_function_is_known(std::uint32_t id) {
    return std::string_view(cs_function_name(id)) != "unknown";
}

const char* cs_function_name(std::uint32_t id) {
    switch (id) {
    case 0x1000000: return "Exit";
    case 0x1000001: return "Signal";
    case 0x1000002: return "Console";
    case 0x1000003: return "Cmd";
    case 0x1000004: return "D_Say";
    case 0x1000005: return "D_CloseDialog";
    case 0x1000006: return "D_Answer";
    case 0x1000007: return "D_PlaySound";

    case 0x2000000: return "LE_CastEffect";
    case 0x2000001: return "LE_DelEffect";
    case 0x2000002: return "LE_CastMagic";

    case 0x3000000: return "WD_LoadArea";
    case 0x3000001: return "WD_SetCellsGroupFlag";
    case 0x3000002: return "RS_SetTribesRelation";
    case 0x3000003: return "RS_GetTribesRelation";
    case 0x3000004: return "RS_StartDialog";
    case 0x3000005: return "WD_SetVisible";
    case 0x3000006: return "C_FINISHED";
    case 0x3000007: return "WD_TitlesAndLoadArea";
    case 0x3000008: return "C_TitlesAndFINISHED";

    case 0x4000000: return "RS_GetPersonParameterI";
    case 0x4000001: return "RS_SetPersonParameterI";
    case 0x4000002: return "RS_AddPerson_1";
    case 0x4000003: return "RS_AddPerson_2";
    case 0x4000004: return "RS_IsPersonExistsI";
    case 0x4000005: return "RS_AddExp";
    case 0x4000006: return "RS_DelPerson";
    case 0x4000007: return "RS_AddToHeroPartyName";
    case 0x4000008: return "RS_RemoveFromHeroPartyName";
    case 0x4000009: return "RS_TestHeroHasPartyName";
    case 0x400000A: return "RS_AllyCmd";
    case 0x400000B: return "RS_ShowMessage";
    case 0x400000C: return "RS_GetPersonSkillI";

    case 0x5000000: return "RS_TestPersonHasItem";
    case 0x5000001: return "RS_PersonTransferItemI";
    case 0x5000002: return "RS_GetItemCountI";
    case 0x5000003: return "RS_PersonTransferAllItemsI";
    case 0x5000004: return "RS_PersonAddItem";
    case 0x5000005: return "RS_PersonRemoveItem";
    case 0x5000006: return "RS_PersonAddItemToTrade";
    case 0x5000007: return "RS_PersonRemoveItemToTrade";
    case 0x5000008: return "RS_GetMoney";

    case 0x6000000: return "RS_GetDayOrNight";
    case 0x6000001: return "RS_GetCurrentTimeOfDayI";
    case 0x6000002: return "RS_GetDaysFromBeginningI";
    case 0x6000003: return "RS_AddTime";

    case 0x7000000: return "RS_QuestComplete";
    case 0x7000001: return "RS_StageEnable";
    case 0x7000002: return "RS_QuestEnable";
    case 0x7000003: return "RS_StageComplete";
    case 0x7000004: return "RS_StorylineQuestEnable";
    case 0x7000005: return "RS_SetEvent";
    case 0x7000006: return "RS_GetEvent";
    case 0x7000007: return "RS_ClearEvent";
    case 0x7000008: return "RS_SetLocationAccess";
    case 0x7000009: return "RS_EnableTrigger";
    case 0x700000A: return "RS_GetRandMinMaxI";
    case 0x700000B: return "RS_SetWeather";
    case 0x700000C: return "RS_SetSpecialPerk";
    case 0x700000D: return "RS_PassToTradePanel";
    case 0x700000E: return "RS_GetDialogEnabled";
    case 0x700000F: return "RS_SetUndeadState";
    case 0x7000010: return "RS_GlobalMap";
    case 0x7000013: return "RS_SetInjured";
    case 0x7000014: return "RS_SetDoorState";

    default: return "unknown";
    }
}
}
