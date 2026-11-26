#include "gl/game/effect.hpp"

#include "gl/game/calendar.hpp"

#include <cstddef>
#include <limits>

namespace gl::game {

bool effect_delay_pending(const PersonEffect &effect) {
    const auto flags = static_cast<std::uint32_t>(effect.words[2]);
    const auto cond = static_cast<std::uint32_t>(effect.words[3]);
    return (flags & kEffectDelayed) != 0 && (cond & kEffectDelayMask) != 0;
}

void age_effects(std::vector<PersonEffect> &effects, std::uint32_t elapsed_minutes) {
    if (elapsed_minutes == 0)
        return;
    std::size_t index = 0;
    while (index < effects.size()) {
        PersonEffect &effect = effects[index];
        const auto flags = static_cast<std::uint32_t>(effect.words[2]);
        std::uint32_t cond = static_cast<std::uint32_t>(effect.words[3]);
        bool remove = false;
        if ((flags & kEffectDuration) != 0) {
            const std::uint32_t duration = cond & 0xffffu;
            if (elapsed_minutes >= duration)
                remove = true;
            else
                cond = duration - elapsed_minutes;
        }
        if (!remove && (flags & kEffectDelayed) != 0) {
            if ((cond & kEffectDelayMask) != 0) {
                const std::uint32_t delay = cond & kEffectDelayMask;
                if (elapsed_minutes >= delay)
                    cond &= 0xffff0000u;
                else
                    cond = (cond & 0xffff0000u) | (delay - elapsed_minutes);
            } else {
                const std::uint32_t duration = cond >> 16;
                if (elapsed_minutes >= duration)
                    remove = true;
                else
                    cond = (duration - elapsed_minutes) << 16;
            }
        }
        if (remove) {
            effects.erase(effects.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        effect.words[3] = static_cast<std::int32_t>(cond);
        ++index;
    }
}

std::int64_t total_minutes(const GameDatetime &value) {
    if (value.year == 0)
        return 0;
    return days_from_civil(static_cast<std::int32_t>(value.year), value.month, value.day) * 1440 +
           static_cast<std::int64_t>(value.hour) * 60 + static_cast<std::int64_t>(value.minute);
}

std::uint32_t minutes_between(const GameDatetime &previous, const GameDatetime &now) {
    const std::int64_t before = total_minutes(previous);
    const std::int64_t after = total_minutes(now);
    if (before == 0 || after <= before)
        return 0;
    const std::uint64_t delta = static_cast<std::uint64_t>(after - before);
    return delta > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(delta);
}

std::vector<gl::assets::SpecialMod> active_effect_mods(const std::vector<PersonEffect> &effects) {
    std::vector<gl::assets::SpecialMod> mods;
    mods.reserve(effects.size());
    for (const PersonEffect &effect : effects) {
        if (effect_delay_pending(effect))
            continue;
        gl::assets::SpecialMod mod;
        mod.id = static_cast<std::uint32_t>(effect.words[0]);
        mod.value = effect.words[1];
        mod.flags = static_cast<std::uint32_t>(effect.words[2]);
        mod.word3 = static_cast<std::uint32_t>(effect.words[3]);
        mod.word4 = static_cast<std::uint32_t>(effect.words[4]);
        mods.push_back(mod);
    }
    return mods;
}
} // namespace gl::game
