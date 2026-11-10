#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/itm.hpp"
#include "gl/assets/specials.hpp"
#include "gl/game/msvc_random.hpp"

#include <cstdint>
#include <vector>

namespace gl::game {
int random_special_count(std::uint32_t flags, MsvcRandom &rng);

class SpecialEngine {
  public:
    static std::vector<gl::assets::SpecialMod> apply(const gl::assets::SpecialDatabase &db,
                                                     gl::assets::ItemType item_type, int nspec,
                                                     int loot_level, MsvcRandom &rng);
};
} // namespace gl::game
