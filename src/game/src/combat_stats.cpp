#include "gl/game/combat_stats.hpp"
#include "gl/game/person_runtime.hpp"

#include <cmath>
#include <cstdint>

namespace gl::game {
namespace {
constexpr std::uint32_t kArmorStatId = 0x1d;
constexpr std::uint32_t kWeightStatId = 0x1e;
constexpr std::uint32_t kActionStatId = 0x1b;
constexpr std::uint32_t kCritStatId = 0x33;
constexpr std::uint32_t kStrengthStatId = 6;
constexpr std::uint32_t kConstitutionStatId = 7;
constexpr std::uint32_t kPerceptionStatId = 8;
constexpr std::uint32_t kDexterityStatId = 9;
constexpr std::uint32_t kLuckStatId = 0xc;
constexpr std::uint32_t kWisdomStatId = 0x0b;
constexpr std::uint32_t kIntelligenceStatId = 10;
constexpr std::uint32_t kDayCondition = 0x2u;
constexpr std::uint32_t kNightCondition = 0x4u;
constexpr std::uint32_t kTimeIntervalCondition = 0x8u;
constexpr std::uint32_t kAlwaysRejectCondition = 0x200u;
constexpr std::uint32_t kShieldMaskA = 0x400000u;
constexpr std::uint32_t kWeaponMaskA = 0x001000u;
constexpr std::uint32_t kBluntStatId = 0x22;
constexpr std::uint32_t kSlashStatId = 0x21;
constexpr std::uint32_t kPierceStatId = 0x23;
constexpr double kActionSkillFactor = 0.6666667;
constexpr double kActionConstant = 5.0;
constexpr double kCritLuckFactor = 0.334;
constexpr double kCritSkillFactor = 0.667;
constexpr double kCritScale = 0.5;
constexpr std::int32_t kMaxPrimaryValue = 30; // ch_max_primary_value default (ini absent)

bool modifier_applies(std::uint32_t flags, const StatCondition &condition) {
    if ((flags & kAlwaysRejectCondition) != 0)
        return false;
    if ((flags & kTimeIntervalCondition) != 0)
        return false;
    if ((flags & kDayCondition) != 0 &&
        (!condition.day_night_known || condition.day_night != 1.0))
        return false;
    if ((flags & kNightCondition) != 0 &&
        (!condition.day_night_known || condition.day_night != 0.0))
        return false;
    return true;
}

double apply_modifiers(double base, std::uint32_t stat_id,
                       const std::vector<EquippedStatItem> &equipped,
                       const StatCondition &condition) {
    double override_value = 0.0;
    bool have_override = false;
    double total = 0.0;
    for (const EquippedStatItem &item : equipped) {
        if ((item.flags & 0x80u) != 0)
            continue;
        for (const gl::assets::SpecialMod &entry : item.mods) {
            if (entry.id != stat_id)
                continue;
            if (!modifier_applies(entry.flags, condition))
                continue;
            const double value = static_cast<double>(entry.value);
            const bool is_override = (entry.flags & 0x40u) == 0 && (entry.flags & 0x80u) == 0;
            if (have_override || is_override) {
                if (!have_override) {
                    have_override = true;
                    override_value = value;
                } else if (std::fabs(override_value) < std::fabs(value)) {
                    override_value = value;
                }
            } else if ((entry.flags & 0x40u) != 0) {
                total += value;
            } else {
                total += value * 0.01 * base;
            }
        }
    }
    return have_override ? override_value : base + total;
}

std::int32_t attribute_value(std::int32_t raw, std::uint32_t stat_id,
                             const std::vector<EquippedStatItem> &equipped,
                             const StatCondition &condition) {
    const long value = std::lround(apply_modifiers(raw, stat_id, equipped, condition));
    if (value < 2)
        return 1;
    if (value >= kMaxPrimaryValue)
        return kMaxPrimaryValue;
    return static_cast<std::int32_t>(value);
}

std::int32_t weapon_skill(const EquippedStatItem &weapon,
                          const std::array<std::int32_t, 27> &skills) {
    std::size_t id = 0;
    const std::uint32_t type = weapon.type;
    if (type == 3 || type == 4 || type == 5)
        id = 9;
    else if ((weapon.flags & 0x10u) != 0)
        id = 0xb;
    else if (type == 0)
        id = 0xc;
    else if (type == 1)
        id = 0xd;
    else if (type == 2)
        id = 10;
    else if (type == 6)
        id = 8;
    else if (type == 7)
        id = 7;
    else
        return 0;
    return skills[id];
}

DamageRange physical_damage(std::uint32_t damage_min, std::uint32_t damage_max, double factor,
                            std::int32_t weapon_skill_value, std::uint32_t stat_id,
                            const std::vector<EquippedStatItem> &equipped,
                            const StatCondition &condition) {
    DamageRange out;
    const double skill = damage_min == 0 ? 0.0 : static_cast<double>(weapon_skill_value);
    const double low = static_cast<double>(damage_min) * factor + skill;
    const double high = static_cast<double>(damage_max) * factor + skill;
    out.minimum =
        static_cast<std::int32_t>(std::lround(apply_modifiers(low, stat_id, equipped, condition)));
    out.maximum =
        static_cast<std::int32_t>(std::lround(apply_modifiers(high, stat_id, equipped, condition)));
    return out;
}

DamageRange elemental_damage(const EquippedStatItem &weapon, std::uint32_t mod_id,
                             std::int32_t skill_value) {
    std::int64_t sum = 0;
    for (const gl::assets::SpecialMod &entry : weapon.mods) {
        if (entry.id == mod_id)
            sum += entry.value;
    }
    if (skill_value > 4)
        sum += 5;
    if (sum == 0)
        return DamageRange{};
    return DamageRange{1, static_cast<std::int32_t>(sum)};
}

std::string format_x10(std::int32_t value) {
    const std::int32_t whole = value / 10;
    const std::int32_t fraction = value % 10;
    std::string out = std::to_string(whole);
    if (fraction != 0)
        out += "." + std::to_string(fraction < 0 ? -fraction : fraction);
    return out;
}
} // namespace

ArmorWeight compute_armor_weight(const StatInputs &inputs) {
    ArmorWeight out;

    std::vector<EquippedStatItem> equipped = inputs.equipped;
    if (!inputs.effect_mods.empty()) {
        EquippedStatItem effects_item;
        effects_item.mods = inputs.effect_mods;
        equipped.push_back(effects_item);
    }

    const EquippedStatItem *shield = nullptr;
    for (const EquippedStatItem &item : equipped) {
        if ((item.flags & kShieldMaskA) != 0) {
            shield = &item;
            break;
        }
    }
    const std::int32_t dexterity =
        attribute_value(inputs.dexterity, kDexterityStatId, equipped, inputs.condition);
    const std::int32_t perception =
        attribute_value(inputs.perception, kPerceptionStatId, equipped, inputs.condition);
    const std::int32_t strength =
        attribute_value(inputs.strength, kStrengthStatId, equipped, inputs.condition);
    const std::int32_t constitution =
        attribute_value(inputs.constitution, kConstitutionStatId, equipped, inputs.condition);
    const std::int32_t luck =
        attribute_value(inputs.luck, kLuckStatId, equipped, inputs.condition);
    const std::int32_t intelligence =
        attribute_value(inputs.intelligence, kIntelligenceStatId, equipped, inputs.condition);
    float armor = static_cast<float>(dexterity + inputs.skills[6]) * 0.334f;
    if (shield != nullptr) {
        const float durability = static_cast<float>(shield->durability);
        armor += static_cast<float>(shield->unknown_b[3]) * (durability * 0.005f + 0.5f);
    }
    long armor_class =
        std::lround(apply_modifiers(armor, kArmorStatId, equipped, inputs.condition));
    if (armor_class < 0)
        armor_class = 0;
    else if (armor_class > 50)
        armor_class = 50;
    out.armor_class = static_cast<std::int32_t>(armor_class);

    std::int64_t carried = 0;
    for (const CarriedItem &item : inputs.carried)
        carried += static_cast<std::int64_t>(item.count) * item.weight_x10;
    out.weight_cur_x10 = static_cast<std::int32_t>(carried);

    const std::int32_t level = hero_level_from_experience(static_cast<double>(inputs.experience));
    const double base = static_cast<double>(strength * 20 + constitution * 20 + level);
    double maximum = apply_modifiers(base, kWeightStatId, equipped, inputs.condition);
    if (inputs.skills[6] >= 5)
        maximum *= 1.5;
    if ((inputs.person_flags & 0x2000u) != 0)
        maximum *= 1.1;
    long weight_max = std::lround(maximum);
    if (weight_max < 1)
        weight_max = 1;
    else if (weight_max > 9999)
        weight_max = 9999;
    out.weight_max_x10 = static_cast<std::int32_t>(weight_max);

    std::int64_t overload = (static_cast<std::int64_t>(out.weight_cur_x10) -
                             static_cast<std::int64_t>(out.weight_max_x10)) /
                            10;
    if (overload < 1)
        overload = 0;
    double action = static_cast<double>(inputs.skills[26]) * kActionSkillFactor +
                    static_cast<double>(dexterity + (perception >> 1)) +
                    (kActionConstant - static_cast<double>(overload));
    if (inputs.skills[6] > 14)
        action += kActionConstant;
    long action_points =
        std::lround(apply_modifiers(action, kActionStatId, equipped, inputs.condition));
    if (action_points < 6)
        action_points = 5;
    else if (action_points > 59)
        action_points = 60;
    out.action_points = static_cast<std::int32_t>(action_points);

    long crit = 0;
    if (inputs.skills[24] != 0) {
        const double base = (static_cast<double>(luck) * kCritLuckFactor +
                             static_cast<double>(inputs.skills[24]) * kCritSkillFactor) *
                            kCritScale;
        crit = std::lround(apply_modifiers(base, kCritStatId, equipped, inputs.condition));
        if (crit < 1)
            crit = 0;
        else if (crit > 9)
            crit = 10;
    }
    out.crit_chance = static_cast<std::int32_t>(crit);

    const EquippedStatItem *weapon = nullptr;
    for (const EquippedStatItem &item : equipped) {
        if ((item.flags & kWeaponMaskA) != 0) {
            weapon = &item;
            break;
        }
    }

    double accuracy =
        (static_cast<double>(perception) * 1.5 + static_cast<double>(inputs.skills[24]) * 2.0 +
         static_cast<double>(luck >> 1)) *
            0.334 +
        50.0;
    if (weapon != nullptr) {
        accuracy = (static_cast<double>(weapon->unknown_b[4]) + accuracy) * 0.6;
        const std::uint32_t type = weapon->type;
        std::int32_t skill_id = -1;
        std::uint32_t stat_id = 0x32;
        if (type == 3 || type == 4 || type == 5) {
            skill_id = 9;
            stat_id = 0x38;
        } else if ((weapon->flags & 0x10u) != 0) {
            skill_id = 0xb;
        } else if (type == 0) {
            skill_id = 0xc;
        } else if (type == 1) {
            skill_id = 0xd;
        } else if (type == 2) {
            skill_id = 10;
        } else if (type == 6) {
            skill_id = 8;
        } else if (type == 7) {
            skill_id = 7;
        }
        if (skill_id >= 0 && inputs.skills[skill_id] > 9)
            accuracy += 15.0;
        long value =
            std::lround(apply_modifiers(accuracy, stat_id, equipped, inputs.condition));
        if (value < 1)
            value = 0;
        else if (value > 89)
            value = 90;
        out.accuracy = static_cast<std::int32_t>(value);
    } else {
        if (inputs.skills[14] > 9)
            accuracy += 20.0;
        long value = std::lround(apply_modifiers(accuracy, 0x32, equipped, inputs.condition));
        if (value < 1)
            value = 0;
        else if (value > 89)
            value = 90;
        out.accuracy = static_cast<std::int32_t>(value);
    }

    std::int32_t wisdom_attr =
        attribute_value(inputs.wisdom, kWisdomStatId, equipped, inputs.condition);
    if (inputs.skills[21] > 4)
        wisdom_attr += 1;
    out.primary = {strength, constitution, dexterity, perception, wisdom_attr, intelligence, luck};
    const std::int32_t skill17 = inputs.skills[17];
    auto resistance_value = [&](std::int32_t base, std::int32_t skill, std::uint32_t stat_id) {
        long value = std::lround(apply_modifiers(
            static_cast<double>(base + inputs.skills[skill] + skill17), stat_id, equipped,
            inputs.condition));
        if (value < 1)
            value = 0;
        else if (value > 89)
            value = 90;
        return static_cast<std::int32_t>(value);
    };
    out.magic_resistance[0] = resistance_value(wisdom_attr, 4, 0x11);
    out.magic_resistance[1] = resistance_value(wisdom_attr, 5, 0x12);
    out.magic_resistance[2] = resistance_value(wisdom_attr, 0, 0x0d);
    out.magic_resistance[3] = resistance_value(wisdom_attr, 1, 0x0e);
    out.magic_resistance[4] = resistance_value(wisdom_attr, 2, 0x0f);
    out.magic_resistance[5] = resistance_value(wisdom_attr, 3, 0x10);
    out.immunity[0] = resistance_value(perception, 4, 0x2e);
    out.immunity[1] = resistance_value(perception, 5, 0x2f);
    out.immunity[2] = resistance_value(perception, 0, 0x2a);
    out.immunity[3] = resistance_value(perception, 1, 0x2b);
    out.immunity[4] = resistance_value(perception, 2, 0x2c);
    out.immunity[5] = resistance_value(perception, 3, 0x2d);

    auto elemental_value = [&](std::int32_t skill, std::uint32_t stat_id) {
        long value = std::lround(apply_modifiers(static_cast<double>(inputs.skills[skill]), stat_id,
                                                 equipped, inputs.condition));
        if (inputs.skills[17] > 4)
            value += 10;
        if (inputs.skills[1] > 4)
            value += 10;
        if (inputs.skills[20] > 4)
            value += 10;
        if (value < 1)
            value = 0;
        else if (value > 89)
            value = 90;
        return static_cast<std::int32_t>(value);
    };
    out.elemental_resistance[0] = elemental_value(22, 5);
    out.elemental_resistance[1] = elemental_value(25, 4);
    out.elemental_resistance[2] = elemental_value(20, 3);

    auto armor_piece_sum = [&](std::size_t damage_index) {
        double sum = 0.0;
        for (const EquippedStatItem &item : equipped) {
            if ((item.flags & 0x304000u) == 0)
                continue;
            sum += static_cast<double>(item.damage[damage_index]) *
                   (static_cast<double>(item.durability) * 0.005 + 0.5);
        }
        return sum;
    };
    auto physical_value = [&](double sum, double multiplier, std::int32_t attribute) {
        const double value = sum * multiplier + static_cast<double>(inputs.skills[6]) +
                             static_cast<double>(attribute) * 0.334;
        long result = std::lround(apply_modifiers(value, 0x13, equipped, inputs.condition));
        if (result < 1)
            result = 0;
        else if (result > 89)
            result = 90;
        return static_cast<std::int32_t>(result);
    };
    out.physical_resistance[0] = physical_value(armor_piece_sum(0), 0.334, constitution);
    out.physical_resistance[1] = physical_value(armor_piece_sum(4), 0.334, dexterity);
    out.physical_resistance[2] = physical_value(armor_piece_sum(2), 0.333, perception);

    if (weapon != nullptr) {
        const double attack = 1.0 + static_cast<double>(strength) * 0.01;
        const double factor = (static_cast<double>(weapon->durability) * 0.005 + 0.5) * attack;
        const std::int32_t skill = weapon_skill(*weapon, inputs.skills);
        out.damage[0] = physical_damage(weapon->damage[0], weapon->damage[1], factor, skill,
                                        kBluntStatId, equipped, inputs.condition);
        out.damage[1] = physical_damage(weapon->damage[4], weapon->damage[5], factor, skill,
                                        kPierceStatId, equipped, inputs.condition);
        out.damage[2] = physical_damage(weapon->damage[2], weapon->damage[3], factor, skill,
                                        kSlashStatId, equipped, inputs.condition);
        out.damage[3] = elemental_damage(*weapon, 2, inputs.skills[0]);
        out.damage[4] = elemental_damage(*weapon, 1, inputs.skills[4]);
        out.damage[5] = elemental_damage(*weapon, 0, inputs.skills[5]);
    }
    return out;
}

std::string format_armor(std::int32_t armor_class) { return std::to_string(armor_class); }

std::string format_weight(std::int32_t cur_x10, std::int32_t max_x10) {
    return format_x10(cur_x10) + "/" + format_x10(max_x10);
}

std::string format_damage(const DamageRange &damage) {
    if (damage.minimum == 0 && damage.maximum == 0)
        return "0";
    return std::to_string(damage.minimum) + "-" + std::to_string(damage.maximum);
}
} // namespace gl::game
