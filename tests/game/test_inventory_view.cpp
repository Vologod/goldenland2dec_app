#include "gl/game/inventory_view.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

using namespace gl::game;

namespace {
gl::assets::HeroInventory make_inventory(const std::vector<std::uint32_t> &types) {
    gl::assets::HeroInventory inventory;
    for (const std::uint32_t type : types) {
        gl::assets::InventoryItem item;
        item.type = type;
        inventory.items.push_back(item);
    }
    return inventory;
}

std::vector<std::size_t> visible_types(const std::vector<std::uint32_t> &types,
                                       std::uint32_t mask) {
    const auto inventory = make_inventory(types);
    return visible_inventory(inventory, mask);
}
} // namespace

TEST_CASE("visible_inventory returns container order for mask 0") {
    const std::vector<std::uint32_t> types{0, 7, 8, 10, 11, 13, 14, 15, 18, 21, 22, 25, 26, 28, 99};
    const auto visible = visible_types(types, 0);
    REQUIRE(visible.size() == types.size());
    for (std::size_t i = 0; i < visible.size(); ++i)
        CHECK(visible[i] == i);
}

TEST_CASE("visible_inventory filter 0x01 keeps weapon types") {
    const std::vector<std::uint32_t> types{0, 1, 7, 8, 11, 18, 21, 25, 28, 29};
    CHECK(visible_types(types, 0x01) == std::vector<std::size_t>{0, 1, 2, 5, 8});
}

TEST_CASE("visible_inventory filter 0x02 keeps armor types") {
    const std::vector<std::uint32_t> types{10, 11, 12, 13, 14, 15, 18, 21};
    CHECK(visible_types(types, 0x02) == std::vector<std::size_t>{1, 2, 3, 4, 6});
}

TEST_CASE("visible_inventory filter 0x04 keeps jewelry types") {
    const std::vector<std::uint32_t> types{14, 15, 16, 17, 18, 19, 25};
    CHECK(visible_types(types, 0x04) == std::vector<std::size_t>{1, 2, 3, 4});
}

TEST_CASE("visible_inventory filter 0x08 keeps book and scroll types") {
    const std::vector<std::uint32_t> types{18, 20, 21, 22, 23, 25};
    CHECK(visible_types(types, 0x08) == std::vector<std::size_t>{0, 2, 3});
}

TEST_CASE("visible_inventory filter 0x10 keeps consumable and material types") {
    const std::vector<std::uint32_t> types{7, 8, 9, 10, 11, 18, 19, 20, 23, 24, 25, 26, 27, 28};
    CHECK(visible_types(types, 0x10) == std::vector<std::size_t>{1, 2, 3, 5, 6, 7, 8, 9, 11, 12});
}

TEST_CASE("visible_inventory filter 0x20 keeps quest items") {
    const std::vector<std::uint32_t> types{18, 24, 25, 26};
    CHECK(visible_types(types, 0x20) == std::vector<std::size_t>{0, 2});
}

TEST_CASE("visible_inventory always keeps money for every filter") {
    const std::vector<std::uint32_t> types{18};
    for (const std::uint32_t mask : {0u, 0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u})
        CHECK(visible_types(types, mask) == std::vector<std::size_t>{0});
}

TEST_CASE("visible_inventory combines multiple mask bits") {
    const std::vector<std::uint32_t> types{3, 12, 16, 21, 25, 18};
    CHECK(visible_types(types, 0x01 | 0x08) == std::vector<std::size_t>{0, 3, 5});
}

TEST_CASE("visible_inventory handles an empty container") {
    CHECK(visible_types({}, 0).empty());
    CHECK(visible_types({}, 0x01).empty());
}

TEST_CASE("clamp_page keeps pages inside the 12 slot window") {
    CHECK(clamp_page(0, 0) == 0);
    CHECK(clamp_page(5, 0) == 0);
    CHECK(clamp_page(0, 12) == 0);
    CHECK(clamp_page(3, 12) == 0);
    CHECK(clamp_page(0, 13) == 0);
    CHECK(clamp_page(1, 13) == 1);
    CHECK(clamp_page(2, 13) == 1);
    CHECK(clamp_page(4, 140) == 4);
    CHECK(clamp_page(200, 140) == 128);
}

TEST_CASE("inventory view state defaults to unfiltered first page") {
    const InventoryViewState state;
    CHECK(state.filter_mask == 0);
    CHECK(state.page == 0);
    CHECK_FALSE(state.selected.has_value());
}
