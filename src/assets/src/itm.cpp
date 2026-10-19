#include "gl/assets/itm.hpp"
#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"
#include <utility>

namespace gl::assets {
namespace {
Result<std::string> read_string(ByteReader& r) {
    std::uint32_t len = 0;
    if (!r.read_u32le(len)) return Error{ErrorCode::format, "truncated string length"};
    std::span<const std::uint8_t> bytes;
    if (!r.read_bytes(len, bytes)) return Error{ErrorCode::format, "truncated string"};
    return cp1251_to_utf8(bytes);
}
}

Result<Item> parse_item(std::span<const std::uint8_t> data) {
    ByteReader r(data);
    std::uint32_t magic = 0;
    if (!r.read_u32le(magic) || magic != 4) return Error{ErrorCode::format, "bad ITM magic"};
    Item item;
    std::uint32_t type = 0;
    if (!r.read_u32le(item.description_id) || !r.read_u32le(type) ||
        !r.read_u32le(item.flags) || !r.read_u32le(item.max_stack))
        return Error{ErrorCode::format, "truncated ITM header"};
    // Item types outside 0..28 pass through; conditional blocks key off the raw value.
    item.type = static_cast<ItemType>(type);
    auto puppet = read_string(r); if (!puppet) return puppet.error();
    auto icon = read_string(r); if (!icon) return icon.error();
    item.puppet_name = std::move(puppet.value());
    item.icon_name = std::move(icon.value());
    if (!r.read_u64le(item.unknown) || !r.read_u32le(item.price) || !r.read_u32le(item.weight_x10) ||
        !r.read_u32le(item.unknown_a))
        return Error{ErrorCode::format, "truncated ITM core"};
    for (auto& v : item.unknown_b) {
        if (!r.read_u32le(v)) return Error{ErrorCode::format, "truncated ITM core"};
    }
    if (!r.read_u32le(item.material)) return Error{ErrorCode::format, "truncated ITM material"};
    if (item.flags & (1u << 4)) {
        auto shell = read_string(r); if (!shell) return shell.error();
        item.shell = std::move(shell.value());
    }
    if (!r.read_u32le(item.level) || !r.read_u32le(item.strength) || !r.read_u32le(item.wisdom) ||
        !r.read_u32le(item.endurance) || !r.read_u32le(item.intelligence) ||
        !r.read_u32le(item.perception) || !r.read_u32le(item.agility))
        return Error{ErrorCode::format, "truncated ITM requirements"};
    const std::uint32_t t = type;
    item.has_damage = (t <= 7) || (t >= 11 && t <= 14);
    if (item.has_damage) {
        for (auto& v : item.damage) {
            if (!r.read_u32le(v)) return Error{ErrorCode::format, "truncated ITM damage"};
        }
    }
    item.has_spell = (t == 6 || t == 21 || t == 22);
    if (item.has_spell) {
        std::uint32_t zero = 0;
        if (!r.read_u32le(item.spell_id) || !r.read_u32le(zero))
            return Error{ErrorCode::format, "truncated ITM spell"};
    }
    item.has_charge = (t == 6);
    if (item.has_charge && !r.read_u32le(item.charge)) return Error{ErrorCode::format, "truncated ITM charge"};
    item.has_healing = (t == 19 || t == 20);
    if (item.has_healing && (!r.read_u32le(item.healing_health) || !r.read_u32le(item.healing_mana)))
        return Error{ErrorCode::format, "truncated ITM healing"};
    item.has_recipe = (t == 26);
    if (item.has_recipe) {
        for (auto& v : item.recipe) {
            if (!r.read_u32le(v)) return Error{ErrorCode::format, "truncated ITM recipe"};
        }
    }
    item.has_damage_type = (t == 8 || t == 9 || t == 10);
    if (item.has_damage_type && !r.read_u32le(item.damage_type))
        return Error{ErrorCode::format, "truncated ITM damage type"};
    std::uint32_t count = 0;
    if (!r.read_u32le(count)) return Error{ErrorCode::format, "truncated ITM effects count"};
    if (count > 1000) return Error{ErrorCode::format, "unreasonable ITM effects count"};
    item.effects.resize(count);
    for (auto& e : item.effects) {
        if (!r.read_u32le(e.type) || !r.read_u32le(e.amount) || !r.read_u32le(e.flags) ||
            !r.read_u8(e.duration.hi) || !r.read_u8(e.duration.lo) ||
            !r.read_u8(e.delay.hi) || !r.read_u8(e.delay.lo))
            return Error{ErrorCode::format, "truncated ITM effect"};
    }
    if (!r.at_end()) return Error{ErrorCode::format, "trailing ITM data"};
    return item;
}
}
