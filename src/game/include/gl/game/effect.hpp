#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/game/rginf.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace gl::game {
struct PersonEffect {
    std::array<std::int32_t, 5> words{};
};

constexpr std::uint32_t kEffectDuration = 0x10;
constexpr std::uint32_t kEffectDelayed = 0x20;
constexpr std::uint32_t kEffectDelayMask = 0xffff;

bool effect_delay_pending(const PersonEffect &effect);
void age_effects(std::vector<PersonEffect> &effects, std::uint32_t elapsed_minutes);
std::int64_t total_minutes(const GameDatetime &value);
std::uint32_t minutes_between(const GameDatetime &previous, const GameDatetime &now);
std::vector<gl::assets::SpecialMod> active_effect_mods(const std::vector<PersonEffect> &effects);
} // namespace gl::game
