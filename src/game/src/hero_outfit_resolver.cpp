#include "gl/game/hero_outfit_resolver.hpp"

#include "gl/assets/log.hpp"

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace gl::game {
namespace {
constexpr std::uint32_t kMaskMask = 0xFFF000;
constexpr std::uint32_t kWeaponA = 0x001000;
constexpr std::uint32_t kShieldA = 0x400000;
constexpr std::uint32_t kHelmet = 0x004000;
constexpr std::uint32_t kArmor = 0x100000;
constexpr std::uint32_t kElbarms = 0x200000;

std::string lower_ascii(std::string value) {
    for (char& c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

void warn(const std::string& message) {
    gl::log(gl::LogLevel::warning, "hero outfit: " + message);
}

const gl::assets::HeroWearEntry* lookup(const gl::assets::Sdb& tech_names,
                                        const gl::assets::HeroWearCatalog& wear,
                                        std::uint32_t item_id,
                                        std::string_view expected_category) {
    const std::string* tech = tech_names.find(item_id);
    if (tech == nullptr) {
        warn("item " + std::to_string(item_id) + " has no TECH");
        return nullptr;
    }
    const gl::assets::HeroWearEntry* entry = wear.find(*tech);
    if (entry == nullptr) {
        warn("TECH '" + *tech + "' (item " + std::to_string(item_id) +
             ") has no wear group");
        return nullptr;
    }
    if (entry->category != expected_category) {
        warn("TECH '" + *tech + "' (item " + std::to_string(item_id) + ") category '" +
             entry->category + "' does not match '" + std::string(expected_category) + "'");
        return nullptr;
    }
    return entry;
}
} // namespace

Result<HeroOutfit> resolve_hero_outfit(const gl::assets::HeroEquipment& equipment,
                                       const gl::assets::Sdb& tech_names,
                                       const gl::assets::HeroWearCatalog& wear) {
    HeroOutfit outfit;
    outfit.wear_set = "noweapon_thrw";
    outfit.weapon.clear();
    outfit.shield.clear();
    outfit.elbarms.clear();
    outfit.helmet.clear();
    outfit.armor.clear();

    for (const gl::assets::EquippedItem& item : equipment.items) {
        const std::uint32_t mask = item.flags & kMaskMask;
        if (mask == kWeaponA) {
            const gl::assets::HeroWearEntry* entry =
                lookup(tech_names, wear, item.item_id, "weapon");
            if (entry == nullptr)
                continue;
            outfit.weapon = lower_ascii(entry->group);
            if (!entry->base_set.empty())
                outfit.wear_set = entry->base_set;
        } else if (mask == kHelmet) {
            const gl::assets::HeroWearEntry* entry =
                lookup(tech_names, wear, item.item_id, "helmet");
            if (entry != nullptr)
                outfit.helmet = lower_ascii(entry->group);
        } else if (mask == kArmor) {
            const gl::assets::HeroWearEntry* entry =
                lookup(tech_names, wear, item.item_id, "armor");
            if (entry != nullptr)
                outfit.armor = lower_ascii(entry->group);
        } else if (mask == kElbarms) {
            const gl::assets::HeroWearEntry* entry =
                lookup(tech_names, wear, item.item_id, "elbarms");
            if (entry != nullptr)
                outfit.elbarms = lower_ascii(entry->group);
        } else if (mask == kShieldA && item.type == 13) {
            const gl::assets::HeroWearEntry* entry =
                lookup(tech_names, wear, item.item_id, "shield");
            if (entry != nullptr)
                outfit.shield = lower_ascii(entry->group);
        }
    }
    return outfit;
}
} // namespace gl::game
