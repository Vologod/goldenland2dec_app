#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/herowear.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/game/hero_wear.hpp"

namespace gl::game {
Result<HeroOutfit> resolve_hero_outfit(const gl::assets::HeroEquipment& equipment,
                                       const gl::assets::Sdb& tech_names,
                                       const gl::assets::HeroWearCatalog& wear);
} // namespace gl::game
