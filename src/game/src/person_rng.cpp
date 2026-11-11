#include "gl/game/person_rng.hpp"

#include "gl/assets/log.hpp"

#include <string>
#include <vector>

namespace gl::game {
namespace {
constexpr std::int32_t kSpawnDelayLow = 3000;
constexpr std::int32_t kSpawnDelayHigh = 10000;

std::vector<std::size_t> eligible_weapons(const gl::assets::PersonDef &person) {
    std::vector<std::size_t> eligible;
    for (std::size_t index = 0; index < person.weapons.size(); ++index) {
        const gl::assets::PersonWeapon &weapon = person.weapons[index];
        if (weapon.min_level + person.weapon_level_offset <= person.level &&
            person.level <= weapon.max_level + person.weapon_level_offset)
            eligible.push_back(index);
    }
    return eligible;
}
} // namespace

PersonInitRolls consume_person_init_rng(const gl::assets::PersonDef &person, MsvcRandom &rng) {
    PersonInitRolls rolls;
    if (person.has_weapon) {
        const std::vector<std::size_t> eligible = eligible_weapons(person);
        if (eligible.empty()) {
            gl::log(gl::LogLevel::warning,
                    "person rng: '" + person.res_name +
                        "' has a weapon block without eligible entries; no weapon draw");
        } else {
            std::size_t index = 0;
            while (!rng.coin(0.5)) {
                index = (index + 1) % eligible.size();
            }
            rolls.weapon_index = eligible[index];
        }
    }
    rolls.spawn_roll = rng.range(kSpawnDelayLow, kSpawnDelayHigh);
    return rolls;
}
} // namespace gl::game
