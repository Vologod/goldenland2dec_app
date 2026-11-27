#include "gl/game/item_use.hpp"

#include <algorithm>
#include <optional>

namespace gl::game {
namespace {
constexpr std::uint32_t kDirectFlag = 0x41;
constexpr std::int32_t kPrimaryMin = 1;
constexpr std::int32_t kPrimaryMax = 30;
constexpr std::int32_t kReputationMin = -50;
constexpr std::int32_t kReputationMax = 50;

std::optional<gl::script::PersonParameter> direct_parameter(std::uint32_t id) {
    using P = gl::script::PersonParameter;
    switch (id) {
    case 6: return P::strength;
    case 7: return P::constitution;
    case 8: return P::perception;
    case 9: return P::dexterity;
    case 10: return P::intelligence;
    case 11: return P::wisdom;
    case 12: return P::luck;
    case 28: return P::reputation;
    default: return std::nullopt;
    }
}

std::int32_t &primary_field(gl::assets::HeroStats &stats, gl::script::PersonParameter parameter) {
    switch (parameter) {
    case gl::script::PersonParameter::strength: return stats.strength;
    case gl::script::PersonParameter::constitution: return stats.constitution;
    case gl::script::PersonParameter::perception: return stats.perception;
    case gl::script::PersonParameter::dexterity: return stats.dexterity;
    case gl::script::PersonParameter::intelligence: return stats.intelligence;
    case gl::script::PersonParameter::wisdom: return stats.wisdom;
    case gl::script::PersonParameter::luck: return stats.luck;
    default: return stats.reputation;
    }
}

void apply_direct(gl::assets::HeroStats &stats, PersonRuntime &runtime,
                  gl::script::PersonParameter parameter, std::int32_t value) {
    std::int32_t &field = primary_field(stats, parameter);
    std::int32_t next = field + value;
    if (parameter == gl::script::PersonParameter::reputation)
        next = std::clamp(next, kReputationMin, kReputationMax);
    else
        next = std::clamp(next, kPrimaryMin, kPrimaryMax);
    field = next;
    runtime.set_raw_parameter("Hero", parameter, static_cast<double>(next));
}

void apply_heal(PersonRuntime &runtime, gl::assets::HeroStats &stats,
                gl::script::PersonParameter parameter, std::uint32_t amount, double maximum) {
    double next = runtime.get_parameter("Hero", parameter) + static_cast<double>(amount);
    if (next > maximum)
        next = maximum;
    if (next < 0.0)
        next = 0.0;
    runtime.set_raw_parameter("Hero", parameter, next);
    if (parameter == gl::script::PersonParameter::life)
        stats.life = static_cast<std::int32_t>(next);
    else
        stats.energy = static_cast<std::int32_t>(next);
}

std::uint32_t pack_word3(const gl::assets::ItemEffect &effect) {
    return static_cast<std::uint32_t>(effect.duration.hi) |
           (static_cast<std::uint32_t>(effect.duration.lo) << 8) |
           (static_cast<std::uint32_t>(effect.delay.hi) << 16) |
           (static_cast<std::uint32_t>(effect.delay.lo) << 24);
}
} // namespace

bool is_consumable(gl::assets::ItemType type) {
    return type == gl::assets::ItemType::potion || type == gl::assets::ItemType::food;
}

HeroUseResult use_hero_item(const gl::assets::Item &item, HeroItemContainer container,
                            std::size_t index, gl::assets::HeroStats &stats,
                            PersonRuntime &runtime, InventoryRuntime &inventory) {
    HeroUseResult result;
    if (!is_consumable(item.type) || !inventory.has_hero_item(container, index))
        return result;
    for (const gl::assets::ItemEffect &effect : item.effects) {
        const auto value = static_cast<std::int32_t>(effect.amount);
        if ((effect.flags & kDirectFlag) == kDirectFlag) {
            if (const auto parameter = direct_parameter(effect.type)) {
                apply_direct(stats, runtime, *parameter, value);
                continue;
            }
        }
        PersonEffect record;
        record.words[0] = static_cast<std::int32_t>(effect.type);
        record.words[1] = value;
        record.words[2] = static_cast<std::int32_t>(effect.flags);
        record.words[3] = static_cast<std::int32_t>(pack_word3(effect));
        record.words[4] = -1;
        runtime.add_effect("Hero", record);
        ++result.effects_added;
    }
    const double max_life =
        runtime.get_parameter("Hero", gl::script::PersonParameter::maximum_life);
    apply_heal(runtime, stats, gl::script::PersonParameter::life, item.healing_health, max_life);
    apply_heal(runtime, stats, gl::script::PersonParameter::energy, item.healing_mana,
               runtime.maximum_energy("Hero"));
    result.consumed = inventory.consume_hero_item(container, index);
    result.used = result.consumed;
    return result;
}
} // namespace gl::game
