#include "gl/assets/hero_stats.hpp"

#include <cstddef>
#include <string>

namespace gl::assets {
namespace {
constexpr std::size_t kPersonRecordOffset = 0x00C;
constexpr std::size_t kLifeOffset = 0x128;
constexpr std::size_t kEnergyOffset = 0x12C;
constexpr std::size_t kExtraOffset = 0x130;
constexpr std::size_t kMinimumBodySize = 0x134;

std::int32_t read_i32(std::span<const std::uint8_t> data, std::size_t offset) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(data[offset + i]) << (i * 8);
    return static_cast<std::int32_t>(value);
}
} // namespace

Result<HeroStats> parse_hero_stats(std::span<const std::uint8_t> body) {
    if (body.size() < kMinimumBodySize)
        return Error{ErrorCode::format, "hero stat block is too short"};

    HeroStats stats;
    stats.strength = read_i32(body, kPersonRecordOffset + 0x00);
    stats.constitution = read_i32(body, kPersonRecordOffset + 0x04);
    stats.dexterity = read_i32(body, kPersonRecordOffset + 0x08);
    stats.perception = read_i32(body, kPersonRecordOffset + 0x0C);
    stats.intelligence = read_i32(body, kPersonRecordOffset + 0x10);
    stats.wisdom = read_i32(body, kPersonRecordOffset + 0x14);
    stats.luck = read_i32(body, kPersonRecordOffset + 0x18);
    stats.reputation = read_i32(body, kPersonRecordOffset + 0x1C);
    stats.experience = read_i32(body, kPersonRecordOffset + 0x20);
    stats.life = read_i32(body, kLifeOffset);
    stats.energy = read_i32(body, kEnergyOffset);
    stats.extra = read_i32(body, kExtraOffset);
    return stats;
}
} // namespace gl::assets
