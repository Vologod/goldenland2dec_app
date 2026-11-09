#include "gl/game/inventory_view.hpp"

#include <algorithm>

namespace gl::game {
namespace {

constexpr std::size_t kPageSize = 12;

} // namespace

bool item_matches_filter(std::uint32_t type, std::uint32_t mask) {
    if (mask == 0)
        return true;
    if (type == 18)
        return true;
    if ((mask & 0x01u) != 0 && (type <= 7 || type == 28))
        return true;
    if ((mask & 0x02u) != 0 && type >= 11 && type <= 14)
        return true;
    if ((mask & 0x04u) != 0 && type >= 15 && type <= 17)
        return true;
    if ((mask & 0x08u) != 0 && (type == 21 || type == 22))
        return true;
    if ((mask & 0x10u) != 0 && ((type >= 8 && type <= 10) || type == 19 || type == 20 ||
                                type == 23 || type == 24 || type == 26 || type == 27))
        return true;
    if ((mask & 0x20u) != 0 && type == 25)
        return true;
    return false;
}

std::vector<std::size_t> visible_inventory(const gl::assets::HeroInventory &inventory,
                                           std::uint32_t mask) {
    std::vector<std::size_t> visible;
    for (std::size_t i = 0; i < inventory.items.size(); ++i) {
        if (item_matches_filter(inventory.items[i].type, mask))
            visible.push_back(i);
    }
    return visible;
}

std::size_t clamp_page(std::size_t page, std::size_t count) {
    const std::size_t max_page = count > kPageSize ? count - kPageSize : 0;
    return std::min(page, max_page);
}

} // namespace gl::game
