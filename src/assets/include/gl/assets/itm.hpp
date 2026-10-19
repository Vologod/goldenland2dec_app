#pragma once
#include "gl/assets/result.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gl::assets {
enum class ItemType : std::uint32_t {
    sword = 0, axe, spear, bow, crossbow, rifle, staff, blunt,
    bullets, bolts, arrows, helmet, cuirass, shield, leggings, bracers,
    amulet, ring, money, potion, food, book, scroll, smith_material,
    alchemy_ingredient, quest_item, recipe, junk, stone
};

struct ItemTime {
    std::uint8_t hi = 0;
    std::uint8_t lo = 0;
    std::uint16_t turns() const { return static_cast<std::uint16_t>(hi * (hi < 2 ? 30 : 60) + lo); }
};

struct ItemEffect {
    std::uint32_t type = 0;
    std::uint32_t amount = 0;
    std::uint32_t flags = 0;
    ItemTime duration;
    ItemTime delay;
};

struct Item {
    std::uint32_t description_id = 0;
    ItemType type = ItemType::sword;
    std::uint32_t flags = 0;
    std::uint32_t max_stack = 0;
    std::string puppet_name;
    std::string icon_name;
    std::uint64_t unknown = 0;
    std::uint32_t price = 0;
    std::uint32_t weight_x10 = 0;
    std::uint32_t unknown_a = 0;
    std::array<std::uint32_t, 5> unknown_b{};
    std::uint32_t material = 0xFFFFFFFFu;
    std::string shell;
    std::uint32_t level = 0, strength = 0, wisdom = 0, endurance = 0;
    std::uint32_t intelligence = 0, perception = 0, agility = 0;
    std::array<std::uint32_t, 6> damage{};
    bool has_damage = false;
    std::uint32_t spell_id = 0;
    bool has_spell = false;
    std::uint32_t charge = 0;
    bool has_charge = false;
    std::uint32_t healing_health = 0, healing_mana = 0;
    bool has_healing = false;
    std::array<std::uint32_t, 3> recipe{};
    bool has_recipe = false;
    std::uint32_t damage_type = 0;
    bool has_damage_type = false;
    std::vector<ItemEffect> effects;
};

Result<Item> parse_item(std::span<const std::uint8_t> data);
}
