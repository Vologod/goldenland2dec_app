#include <doctest/doctest.h>

#include "gl/game/combat_stats.hpp"
#include "gl/game/weapon_set.hpp"

namespace {
gl::game::EquippedStatItem weapon(std::uint32_t mask, std::array<std::uint32_t, 6> damage) {
    gl::game::EquippedStatItem item;
    item.type = 0;
    item.flags = mask;
    item.durability = 100;
    item.damage = damage;
    item.unknown_b = {0, 0, 0, 0, 10};
    return item;
}

gl::game::EquippedStatItem shield(std::uint32_t mask, std::uint32_t armor) {
    gl::game::EquippedStatItem item;
    item.type = 13;
    item.flags = mask;
    item.durability = 100;
    item.unknown_b = {0, 0, 0, armor, 0};
    return item;
}
} // namespace

TEST_CASE("weapon_set_masks selects A and B pairs") {
    const auto a = gl::game::weapon_set_masks(gl::game::WeaponSet::a);
    CHECK(a.weapon == 0x001000u);
    CHECK(a.shield == 0x400000u);
    const auto b = gl::game::weapon_set_masks(gl::game::WeaponSet::b);
    CHECK(b.weapon == 0x002000u);
    CHECK(b.shield == 0x800000u);
}

TEST_CASE("compute_armor_weight uses the active weapon and shield set") {
    gl::game::StatInputs inputs;
    inputs.strength = 10;
    inputs.constitution = 10;
    inputs.dexterity = 10;
    inputs.perception = 10;
    inputs.luck = 10;
    inputs.wisdom = 10;
    inputs.intelligence = 10;
    inputs.experience = 1000;
    inputs.skills.fill(0);
    inputs.equipped = {
        weapon(0x001000u, {10, 20, 0, 0, 0, 0}),
        weapon(0x002000u, {0, 0, 30, 40, 0, 0}),
        shield(0x400000u, 10),
        shield(0x800000u, 20),
    };

    const auto a = gl::game::compute_armor_weight(inputs);
    inputs.use_b_set = true;
    const auto b = gl::game::compute_armor_weight(inputs);

    CHECK(b.armor_class == a.armor_class + 10);
    CHECK(a.damage[0].minimum > 0);
    CHECK(a.damage[1].minimum == 0);
    CHECK(b.damage[0].minimum == 0);
    CHECK(b.damage[2].minimum > 0);
}
