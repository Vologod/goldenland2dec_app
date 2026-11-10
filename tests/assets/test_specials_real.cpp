#include "gl/assets/specials.hpp"
#include "gl/assets/vfs.hpp"

#include <array>
#include <cstdlib>
#include <doctest/doctest.h>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::vector<SpecialId> id_list(std::initializer_list<int> ids) {
    std::vector<SpecialId> out;
    out.reserve(ids.size());
    for (int id : ids)
        out.push_back(static_cast<SpecialId>(id));
    return out;
}
} // namespace

TEST_CASE("specials real: parses retail database" * doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto db = load_specials_scr(vfs.value());
    REQUIRE(db.has_value());
    CHECK(db->defs.size() == 58);
    CHECK(db->item_classes.size() == 15);

    std::size_t random_sign = 0;
    for (const SpecialDef &def : db->defs) {
        CHECK(def.id < 58);
        REQUIRE(db->find(def.id) != nullptr);
        if (def.random_sign)
            ++random_sign;
    }
    CHECK(random_sign == 29);
    CHECK(db->find(58) == nullptr);
    CHECK(db->find(77) == nullptr);

    const auto &swords = db->item_classes.at(ItemType::sword);
    const auto &axes = db->item_classes.at(ItemType::axe);
    const auto &spears = db->item_classes.at(ItemType::spear);
    const auto &bows = db->item_classes.at(ItemType::bow);
    const auto &crossbows = db->item_classes.at(ItemType::crossbow);
    const auto &handguns = db->item_classes.at(ItemType::rifle);
    const auto &crosiers = db->item_classes.at(ItemType::staff);
    const auto &maces = db->item_classes.at(ItemType::blunt);
    const auto &helms = db->item_classes.at(ItemType::helmet);
    const auto &armours = db->item_classes.at(ItemType::cuirass);
    const auto &shields = db->item_classes.at(ItemType::shield);
    const auto &elbarms = db->item_classes.at(ItemType::leggings);
    const auto &bracelets = db->item_classes.at(ItemType::bracers);
    const auto &amulets = db->item_classes.at(ItemType::amulet);
    const auto &rings = db->item_classes.at(ItemType::ring);

    CHECK(swords.size() == 30);
    CHECK(axes.size() == 29);
    CHECK(spears.size() == 28);
    CHECK(bows.size() == 32);
    CHECK(crossbows.size() == 32);
    CHECK(handguns.size() == 32);
    CHECK(crosiers.size() == 43);
    CHECK(maces.size() == 28);
    CHECK(helms.size() == 22);
    CHECK(armours.size() == 21);
    CHECK(shields.size() == 18);
    CHECK(elbarms.size() == 21);
    CHECK(bracelets.size() == 33);
    CHECK(amulets.size() == 33);
    CHECK(rings.size() == 33);

    CHECK(swords == id_list({0,  1,  2,  33, 34, 35, 36, 37, 38, 39, 40, 41, 31, 32, 50,
                             53, 51, 52, 6,  7,  8,  9,  10, 11, 12, 24, 27, 28, 54, 55}));
    CHECK(axes == id_list({0,  1,  2, 33, 34, 36, 37, 38, 39, 40, 41, 31, 32, 50, 53,
                           51, 52, 6, 7,  8,  9,  10, 11, 12, 24, 27, 28, 54, 55}));
    CHECK(spears == id_list({0,  1,  2, 35, 36, 37, 38, 39, 40, 41, 31, 32, 50, 53,
                             51, 52, 6, 7,  8,  9,  10, 11, 12, 24, 27, 28, 54, 55}));
    CHECK(crosiers == id_list({0,  1,  2,  34, 36, 37, 38, 39, 40, 41, 31, 32, 50, 53, 51,
                               52, 25, 26, 13, 14, 15, 16, 17, 18, 42, 43, 44, 45, 46, 47,
                               6,  7,  8,  9,  10, 11, 12, 24, 27, 28, 30, 54, 55}));
    CHECK(helms == id_list({6,  7,  8,  9,  10, 11, 12, 22, 23, 25, 26,
                            19, 20, 21, 28, 29, 54, 55, 50, 56, 53, 52}));
    CHECK(armours == id_list({6,  7,  8,  9,  10, 11, 12, 19, 20, 21, 22,
                              23, 25, 26, 24, 28, 29, 30, 53, 54, 55}));
    CHECK(rings == id_list({3, 4, 5,  13, 14, 15, 16, 17, 18, 42, 43, 44, 45, 46, 47, 6, 7,
                            8, 9, 10, 11, 12, 22, 23, 25, 26, 24, 28, 50, 52, 53, 54, 55}));

    for (int type = 8; type <= 10; ++type)
        CHECK_FALSE(db->item_classes.contains(static_cast<ItemType>(type)));
    for (int type = 18; type <= 28; ++type)
        CHECK_FALSE(db->item_classes.contains(static_cast<ItemType>(type)));
}

TEST_CASE("specials real: representative defs" * doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto db = load_specials_scr(vfs.value());
    REQUIRE(db.has_value());

    const SpecialDef *poison = db->find(0);
    REQUIRE(poison != nullptr);
    CHECK_FALSE(poison->random_sign);
    const std::array<std::uint32_t, special_value_slot_count> value_tags = {0x40, 0x80, 0x100};
    const std::array<std::uint8_t, special_value_slot_count> value_access = {1, 0, 0};
    const std::array<float, special_value_slot_count> value_values = {1.0f, 1.0f, 1.0f};
    const std::array<float, special_value_slot_count> value_prices = {50.0f, 1.0f, 1.0f};
    for (std::size_t i = 0; i < special_value_slot_count; ++i) {
        CAPTURE(i);
        CHECK(poison->value_slots[i].tag == value_tags[i]);
        CHECK(poison->value_slots[i].access == value_access[i]);
        CHECK(poison->value_slots[i].value == doctest::Approx(value_values[i]));
        CHECK(poison->value_slots[i].price == doctest::Approx(value_prices[i]));
    }
    const std::array<std::uint32_t, special_time_slot_count> time_tags = {1, 2, 4, 8, 0x10, 0x20};
    const std::array<std::uint8_t, special_time_slot_count> time_access = {1, 0, 0, 0, 0, 0};
    const std::array<float, special_time_slot_count> time_values = {0.3f, 0.3f, 0.3f,
                                                                    1.0f, 1.0f, 1.0f};
    const std::array<float, special_time_slot_count> time_prices = {1.0f, 0.5f, 0.5f,
                                                                    1.0f, 1.0f, 1.0f};
    for (std::size_t i = 0; i < special_time_slot_count; ++i) {
        CAPTURE(i);
        CHECK(poison->time_slots[i].tag == time_tags[i]);
        CHECK(poison->time_slots[i].access == time_access[i]);
        CHECK(poison->time_slots[i].value == doctest::Approx(time_values[i]));
        CHECK(poison->time_slots[i].price == doctest::Approx(time_prices[i]));
    }

    const SpecialDef *poison_res = db->find(3);
    REQUIRE(poison_res != nullptr);
    CHECK(poison_res->random_sign);

    const SpecialDef *max_health = db->find(22);
    REQUIRE(max_health != nullptr);
    CHECK(max_health->value_slots[0].access == 1);
    CHECK(max_health->value_slots[0].value == doctest::Approx(-1.0f));
    CHECK(max_health->value_slots[1].access == 1);
    CHECK(max_health->value_slots[1].value == doctest::Approx(-0.6f));
    CHECK(max_health->value_slots[1].price == doctest::Approx(5.0f));

    const SpecialDef *health_current = db->find(48);
    REQUIRE(health_current != nullptr);
    CHECK(health_current->random_sign);
    CHECK(health_current->value_slots[0].access == 0);
    CHECK(health_current->time_slots[0].access == 0);
    CHECK(health_current->time_slots[5].access == 0);
}
