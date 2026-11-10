#include "gl/assets/specials.hpp"
#include "gl/assets/cp1251.hpp"
#include "gl/assets/vfs.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gl::assets {
namespace {
struct Token {
    std::string text;
    bool quoted = false;
};

constexpr std::array<std::string_view, 78> kIdSpecNames = {"IDSPEC_POISON_DMG",
                                                           "IDSPEC_COLD_DMG",
                                                           "IDSPEC_FIRE_DMG",
                                                           "IDSPEC_POISON_RES",
                                                           "IDSPEC_COLD_RES",
                                                           "IDSPEC_FIRE_RES",
                                                           "IDSPEC_STRENGTH",
                                                           "IDSPEC_CONSTITUTION",
                                                           "IDSPEC_PERCEPTION",
                                                           "IDSPEC_DEXTERITY",
                                                           "IDSPEC_INTELLIGENCE",
                                                           "IDSPEC_WISDOM",
                                                           "IDSPEC_LUCK",
                                                           "IDSPEC_GODS_MAGIC_RES",
                                                           "IDSPEC_ELEMENTS_MAGIC_RES",
                                                           "IDSPEC_LIGHTNESS_MAGIC_RES",
                                                           "IDSPEC_DARKNESS_MAGIC_RES",
                                                           "IDSPEC_SHADOWS_MAGIC_RES",
                                                           "IDSPEC_NATURE_MAGIC_RES",
                                                           "IDSPEC_HACKING_RES",
                                                           "IDSPEC_CRUSHING_RES",
                                                           "IDSPEC_PRICKING_RES",
                                                           "IDSPEC_MAX_HEALTH",
                                                           "IDSPEC_MAX_ENERGY",
                                                           "IDSPEC_INITIATIVE",
                                                           "IDSPEC_HEALTH_REGENERATE_TIME",
                                                           "IDSPEC_ENERGY_REGENERATE_TIME",
                                                           "IDSPEC_ACTION_POINTS",
                                                           "IDSPEC_REPUTATION",
                                                           "IDSPEC_ARMOR_CLASS",
                                                           "IDSPEC_MAX_WEIGHT_CARRIED",
                                                           "IDSPEC_TRANSFER_HEALTH",
                                                           "IDSPEC_TRANSFER_ENERGY",
                                                           "IDSPEC_HACKING_DMG",
                                                           "IDSPEC_CRUSHING_DMG",
                                                           "IDSPEC_PRICKING_DMG",
                                                           "IDSPEC_GODS_MAGIC_DMG",
                                                           "IDSPEC_ELEMENTS_MAGIC_DMG",
                                                           "IDSPEC_LIGHTNESS_MAGIC_DMG",
                                                           "IDSPEC_DARKNESS_MAGIC_DMG",
                                                           "IDSPEC_SHADOWS_MAGIC_DMG",
                                                           "IDSPEC_NATURE_MAGIC_DMG",
                                                           "IDSPEC_GODS_MAGIC_IMMUN",
                                                           "IDSPEC_ELEMENTS_MAGIC_IMMUN",
                                                           "IDSPEC_LIGHTNESS_MAGIC_IMMUN",
                                                           "IDSPEC_DARKNESS_MAGIC_IMMUN",
                                                           "IDSPEC_SHADOWS_MAGIC_IMMUN",
                                                           "IDSPEC_NATURE_MAGIC_IMMUN",
                                                           "IDSPEC_HEALTH_CURRENT",
                                                           "IDSPEC_ENERGY_CURRENT",
                                                           "IDSPEC_CHT_HIT",
                                                           "IDSPEC_CHT_CRITICAL_HIT",
                                                           "IDSPEC_MOD_CRITICAL_HIT",
                                                           "IDSPEC_CHT_CRITICAL_MISS",
                                                           "IDSPEC_CHT_DIALOG_LEVEL",
                                                           "IDSPEC_CHT_RUN_FROM_EVIL",
                                                           "IDSPEC_CHT_HIT_THROWING",
                                                           "IDSPEC_DMG_THROWING",
                                                           "IDSPEC_BROKEN_LEFT_FOOT",
                                                           "IDSPEC_BROKEN_RIGHT_FOOT",
                                                           "IDSPEC_BROKEN_LEFT_HAND",
                                                           "IDSPEC_BROKEN_RIGHT_HAND",
                                                           "IDSPEC_BLINDNESS",
                                                           "IDSPEC_STONECURSE",
                                                           "IDSPEC_FREEZE",
                                                           "IDSPEC_OGLUSHENIE",
                                                           "IDSPEC_RAGE",
                                                           "IDSPEC_SANCTITY",
                                                           "IDSPEC_DEATH_MIRROR",
                                                           "IDSPEC_IMBECILITY",
                                                           "IDSPEC_ELEMENTAL_SHIELD",
                                                           "IDSPEC_DISPELL",
                                                           "IDSPEC_MAP_WALKER",
                                                           "IDSPEC_SILENCE",
                                                           "IDSPEC_SUMMON_DEMISHADOW",
                                                           "IDSPEC_SUMMON_ICE_BEAST",
                                                           "IDSPEC_SUMMON_WINGED_DEMON",
                                                           "IDSPEC_SUMMON_VIOLIA"};

constexpr std::array<std::pair<std::string_view, SpecialFlags>, 10> kSpecFlags = {{
    {"SPECF_TIME_PERMANENT", 1},
    {"SPECF_TIME_DAY", 2},
    {"SPECF_TIME_NIGHT", 4},
    {"SPECF_TIME_INTERVAL", 8},
    {"SPECF_TIME_IMMEDIATE", 0x10},
    {"SPECF_TIME_DELAYED", 0x20},
    {"SPECF_VALUE_CHANGE_ABSOLUTE", 0x40},
    {"SPECF_VALUE_CHANGE_RELATIVE", 0x80},
    {"SPECF_VALUE_REPLACE", 0x100},
    {"SPECF_ANTI_MONSTER_SPECIFIED", 0x200},
}};

constexpr std::array<std::pair<std::string_view, ItemType>, 29> kItemTypes = {{
    {"T_SWORD", ItemType::sword},
    {"T_AXE", ItemType::axe},
    {"T_SPEAR", ItemType::spear},
    {"T_BOW", ItemType::bow},
    {"T_CROSSBOW", ItemType::crossbow},
    {"T_HANDGUNS", ItemType::rifle},
    {"T_CROSIER", ItemType::staff},
    {"T_MACE", ItemType::blunt},
    {"T_AMMO", ItemType::bullets},
    {"T_BOLTS", ItemType::bolts},
    {"T_ARROWS", ItemType::arrows},
    {"T_HELM", ItemType::helmet},
    {"T_ARMOUR", ItemType::cuirass},
    {"T_SHIELD", ItemType::shield},
    {"T_ELBARMS", ItemType::leggings},
    {"T_BRACELET", ItemType::bracers},
    {"T_AMULET", ItemType::amulet},
    {"T_RING", ItemType::ring},
    {"T_MONEY", ItemType::money},
    {"T_POTIONS", ItemType::potion},
    {"T_FOOD", ItemType::food},
    {"T_BOOK", ItemType::book},
    {"T_ROLLS", ItemType::scroll},
    {"T_MATERIAL", ItemType::smith_material},
    {"T_REAGENT", ItemType::alchemy_ingredient},
    {"T_QUEST_ITEM", ItemType::quest_item},
    {"T_RECIPE", ItemType::recipe},
    {"T_GARBAGE", ItemType::junk},
    {"T_STONE", ItemType::stone},
}};

char lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z')
        return static_cast<char>(c - 'A' + 'a');
    return c;
}

bool ci_equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (lower_ascii(a[i]) != lower_ascii(b[i]))
            return false;
    }
    return true;
}

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

Result<std::vector<Token>> lex(std::span<const std::uint8_t> data) {
    std::vector<Token> tokens;
    const std::size_t size = data.size();
    std::size_t i = 0;
    while (i < size) {
        const unsigned char c = data[i];
        if (c <= 0x20) {
            ++i;
            continue;
        }
        if (c == '/' && i + 1 < size && data[i + 1] == '/') {
            while (i < size && data[i] != '\n')
                ++i;
            continue;
        }
        if (c == '"') {
            ++i;
            std::string text;
            bool closed = false;
            while (i < size) {
                const char ch = static_cast<char>(data[i++]);
                if (ch == '"') {
                    closed = true;
                    break;
                }
                text.push_back(ch);
            }
            if (!closed)
                return format_error("specials: unterminated quoted string");
            tokens.push_back(Token{std::move(text), true});
            continue;
        }

        std::string text;
        while (i < size) {
            const unsigned char b = data[i];
            if (b <= 0x20)
                break;
            if (b == '/' && i + 1 < size && data[i + 1] == '/')
                break;
            text.push_back(static_cast<char>(b));
            ++i;
        }
        tokens.push_back(Token{std::move(text), false});
    }
    return tokens;
}

bool parse_float_token(const Token &token, float &out) {
    if (token.quoted || token.text.empty())
        return false;
    const char *begin = token.text.c_str();
    char *end = nullptr;
    const float value = std::strtof(begin, &end);
    if (end == begin || static_cast<std::size_t>(end - begin) != token.text.size())
        return false;
    out = value;
    return true;
}

bool is_symbol(const Token &token, std::string_view symbol) {
    return !token.quoted && token.text == symbol;
}

std::string token_text(const std::string &text) {
    return cp1251_to_utf8(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t *>(text.data()), text.size()));
}

template <std::size_t N>
void add_special_slot(std::array<SpecialSlot, N> &slots, const SpecialSlot &slot) {
    for (const SpecialSlot &existing : slots) {
        if (existing.tag == slot.tag)
            return;
    }
    for (SpecialSlot &target : slots) {
        if (target.tag == 0) {
            target = slot;
            return;
        }
    }
}
} // namespace

std::optional<SpecialId> special_id_from_name(std::string_view name) {
    for (std::size_t i = 0; i < kIdSpecNames.size(); ++i) {
        if (ci_equal(name, kIdSpecNames[i]))
            return static_cast<SpecialId>(i);
    }
    return std::nullopt;
}

std::optional<SpecialFlags> special_flag_from_name(std::string_view name) {
    for (const auto &[flag_name, tag] : kSpecFlags) {
        if (ci_equal(name, flag_name))
            return tag;
    }
    return std::nullopt;
}

std::optional<ItemType> special_item_type_from_name(std::string_view name) {
    for (const auto &[type_name, type] : kItemTypes) {
        if (ci_equal(name, type_name))
            return type;
    }
    return std::nullopt;
}

std::size_t special_id_name_count() { return kIdSpecNames.size(); }
std::size_t special_flag_name_count() { return kSpecFlags.size(); }
std::size_t special_item_type_name_count() { return kItemTypes.size(); }

const SpecialDef *SpecialDatabase::find(SpecialId id) const {
    for (const SpecialDef &def : defs) {
        if (def.id == id)
            return &def;
    }
    return nullptr;
}

Result<SpecialDatabase> parse_specials_scr(std::span<const std::uint8_t> data) {
    auto lexed = lex(data);
    if (!lexed)
        return lexed.error();
    const std::vector<Token> &tokens = lexed.value();

    SpecialDatabase db;
    std::optional<ItemType> current_class;
    std::size_t i = 0;
    while (i < tokens.size()) {
        const Token &token = tokens[i];
        if (!token.quoted && ci_equal(token.text, "special_desc")) {
            current_class.reset();
            ++i;
            if (i >= tokens.size() || tokens[i].quoted)
                return format_error("specials: expected special id after 'special_desc'");
            auto id = special_id_from_name(tokens[i].text);
            if (!id)
                return format_error("specials: unknown special id '" + token_text(tokens[i].text) +
                                    "'");
            ++i;

            bool random_sign = false;
            if (i < tokens.size() && !tokens[i].quoted && ci_equal(tokens[i].text, "random_sign")) {
                random_sign = true;
                ++i;
            }

            if (i >= tokens.size() || !is_symbol(tokens[i], "{"))
                return format_error("specials: expected '{' after special header");
            ++i;

            SpecialDef def;
            def.id = *id;
            def.random_sign = random_sign;
            while (i < tokens.size()) {
                if (is_symbol(tokens[i], "}")) {
                    ++i;
                    break;
                }
                if (tokens[i].quoted)
                    return format_error("specials: expected slot flag");
                const std::string tag_text = tokens[i].text;
                auto tag = special_flag_from_name(tag_text);
                if (!tag)
                    return format_error("specials: unknown slot flag '" + token_text(tag_text) +
                                        "'");
                ++i;

                if (i >= tokens.size() || tokens[i].quoted)
                    return format_error("specials: expected slot access");
                const std::uint8_t access = tokens[i].text == "+" ? 1 : 0;
                ++i;

                float value = 0.0f;
                if (i >= tokens.size() || !parse_float_token(tokens[i], value))
                    return format_error("specials: invalid slot value");
                ++i;

                float price = 0.0f;
                if (i >= tokens.size() || !parse_float_token(tokens[i], price))
                    return format_error("specials: invalid slot price");
                ++i;

                const SpecialSlot slot{*tag, access, value, price};
                if (is_special_value_tag(*tag)) {
                    add_special_slot(def.value_slots, slot);
                } else if (is_special_time_tag(*tag)) {
                    add_special_slot(def.time_slots, slot);
                } else {
                    return format_error("specials: unsupported slot flag '" + token_text(tag_text) +
                                        "'");
                }
            }
            db.defs.push_back(def);
        } else if (!token.quoted && ci_equal(token.text, "itemclass")) {
            current_class.reset();
            ++i;
            if (i >= tokens.size() || tokens[i].quoted)
                return format_error("specials: expected item class name");
            auto type = special_item_type_from_name(tokens[i].text);
            if (!type)
                return format_error("specials: unknown item class '" + token_text(tokens[i].text) +
                                    "'");
            current_class = *type;
            db.item_classes[*type];
            ++i;
        } else if (!token.quoted && ci_equal(token.text, "special")) {
            if (!current_class)
                return format_error("specials: 'special' outside itemclass");
            ++i;
            if (i >= tokens.size() || tokens[i].quoted)
                return format_error("specials: expected special id after 'special'");
            auto id = special_id_from_name(tokens[i].text);
            if (!id)
                return format_error("specials: unknown special id '" + token_text(tokens[i].text) +
                                    "'");
            std::vector<SpecialId> &pool = db.item_classes[*current_class];
            if (std::find(pool.begin(), pool.end(), *id) == pool.end())
                pool.push_back(*id);
            ++i;
        } else {
            return format_error("specials: unexpected token '" + token_text(token.text) + "'");
        }
    }
    return db;
}

Result<SpecialDatabase> load_specials_scr(GameVfs &vfs) {
    const std::string path = normalize_virtual_path("scripts/item_class_specials/specials.scr");
    auto bytes = vfs.read(path);
    if (!bytes)
        return Error{bytes.error().code,
                     "specials: cannot read '" + path + "': " + bytes.error().message};
    return parse_specials_scr(bytes.value());
}
} // namespace gl::assets
