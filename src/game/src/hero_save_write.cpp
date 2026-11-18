#include "gl/game/hero_save_write.hpp"

#include "gl/assets/save_item_convert.hpp"

namespace gl::game {
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
    return body;
}
} // namespace gl::game
