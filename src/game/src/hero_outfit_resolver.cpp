#include "gl/game/hero_outfit_resolver.hpp"

#include <cctype>
#include <string>

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

const gl::assets::HeroWearEntry* lookup(const gl::assets::Sdb& tech_names,
                                        const gl::assets::HeroWearCatalog& wear,
                                        std::uint32_t item_id) {
    const std::string* tech = tech_names.find(item_id);
    if (tech == nullptr)
        return nullptr;
    return wear.find(*tech);
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
                lookup(tech_names, wear, item.item_id);
            if (entry == nullptr)
                continue;
            outfit.weapon = lower_ascii(entry->group);
            if (!entry->base_set.empty())
                outfit.wear_set = entry->base_set;
        } else if (mask == kHelmet) {
            const gl::assets::HeroWearEntry* entry =
                lookup(tech_names, wear, item.item_id);
            if (entry != nullptr)
                outfit.helmet = lower_ascii(entry->group);
        } else if (mask == kArmor) {
            const gl::assets::HeroWearEntry* entry =
                lookup(tech_names, wear, item.item_id);
            if (entry != nullptr)
                outfit.armor = lower_ascii(entry->group);
        } else if (mask == kElbarms) {
            const gl::assets::HeroWearEntry* entry =
                lookup(tech_names, wear, item.item_id);
            if (entry != nullptr)
                outfit.elbarms = lower_ascii(entry->group);
        } else if (mask == kShieldA && item.type == 13) {
            const gl::assets::HeroWearEntry* entry =
                lookup(tech_names, wear, item.item_id);
            if (entry != nullptr)
                outfit.shield = lower_ascii(entry->group);
        }
    }
    return outfit;
}
} // namespace gl::game
