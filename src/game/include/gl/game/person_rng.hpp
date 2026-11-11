#pragma once
#include "gl/assets/person_script.hpp"
#include "gl/game/msvc_random.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace gl::game {
struct PersonInitRolls {
    std::optional<std::size_t> weapon_index;
    std::int32_t spawn_roll = 0;
};

PersonInitRolls consume_person_init_rng(const gl::assets::PersonDef &person, MsvcRandom &rng);
} // namespace gl::game
