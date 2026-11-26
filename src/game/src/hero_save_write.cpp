#include "gl/game/hero_save_write.hpp"

#include "gl/assets/save_item_convert.hpp"

#include <algorithm>
#include <array>

namespace gl::game {
namespace {
std::array<std::uint8_t, 0x14> pack_effect(const PersonEffect &effect) {
    std::array<std::uint8_t, 0x14> out{};
    for (std::size_t i = 0; i < effect.words.size(); ++i) {
        const auto value = static_cast<std::uint32_t>(effect.words[i]);
        out[4 * i] = static_cast<std::uint8_t>(value & 0xFF);
        out[4 * i + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        out[4 * i + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
        out[4 * i + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    }
    return out;
}
} // namespace
gl::assets::HeroSaveBody make_hero_save_body(const gl::assets::HeroSaveBody &loaded,
                                             const HeroLiveState &live) {
    gl::assets::HeroSaveBody body = loaded;
    body.tile_x = live.tile_x;
    body.tile_y = live.tile_y;
    body.direction = live.direction;
    body.hp = live.hp;
    body.ep = live.ep;
    body.ap = live.ap;
    for (std::size_t index = 0; index < body.containers.size(); ++index) {
        body.containers[index].flag = live.present[index] ? 1u : 0u;
        body.containers[index].items.clear();
        if (!live.present[index])
            continue;
        body.containers[index].items.reserve(live.containers[index].size());
        for (const gl::assets::InventoryItem &item : live.containers[index])
            body.containers[index].items.push_back(gl::assets::save_item_from_inventory(item));
    }
    if (live.has_effects) {
        body.extra.clear();
        body.extra.reserve(live.effects.size());
        for (const PersonEffect &effect : live.effects)
            body.extra.push_back(pack_effect(effect));
    }
    if (live.effect_clock) {
        const auto bytes = live.effect_clock->to_time_bytes();
        std::copy(bytes.begin(), bytes.end(), body.tail80.begin() + 0x40);
    }
    return body;
}
} // namespace gl::game
