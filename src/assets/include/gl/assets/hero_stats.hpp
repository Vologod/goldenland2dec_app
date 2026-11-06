#pragma once
#include "gl/assets/result.hpp"

#include <cstdint>
#include <span>

namespace gl::assets {
struct HeroStats {
    std::int32_t strength = 0;
    std::int32_t constitution = 0;
    std::int32_t dexterity = 0;
    std::int32_t perception = 0;
    std::int32_t intelligence = 0;
    std::int32_t wisdom = 0;
    std::int32_t luck = 0;
    std::int32_t reputation = 0;
    std::int32_t experience = 0;
    std::int32_t life = 0;
    std::int32_t energy = 0;
    std::int32_t extra = 0;
};

Result<HeroStats> parse_hero_stats(std::span<const std::uint8_t> body);
} // namespace gl::assets
