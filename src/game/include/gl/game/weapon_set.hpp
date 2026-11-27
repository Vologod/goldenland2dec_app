#pragma once
#include <cstdint>

namespace gl::game {
enum class WeaponSet : std::uint8_t { a = 0, b = 1 };

inline constexpr std::uint32_t kWeaponMaskA = 0x001000u;
inline constexpr std::uint32_t kWeaponMaskB = 0x002000u;
inline constexpr std::uint32_t kShieldMaskA = 0x400000u;
inline constexpr std::uint32_t kShieldMaskB = 0x800000u;

struct WeaponSetMasks {
    std::uint32_t weapon = kWeaponMaskA;
    std::uint32_t shield = kShieldMaskA;
};

inline constexpr WeaponSetMasks weapon_set_masks(WeaponSet set) {
    return set == WeaponSet::b ? WeaponSetMasks{kWeaponMaskB, kShieldMaskB}
                               : WeaponSetMasks{kWeaponMaskA, kShieldMaskA};
}
} // namespace gl::game
