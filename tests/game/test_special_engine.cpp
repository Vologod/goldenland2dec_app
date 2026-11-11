#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/specials.hpp"
#include "gl/game/msvc_random.hpp"
#include "gl/game/special_engine.hpp"

#include <doctest/doctest.h>
#include <initializer_list>
#include <utility>
#include <vector>

using gl::assets::ItemType;
using gl::assets::SpecialDatabase;
using gl::assets::SpecialDef;
using gl::assets::SpecialFlags;
using gl::assets::SpecialId;
using gl::assets::SpecialMod;
using gl::assets::SpecialSlot;
using gl::game::MsvcRandom;
using gl::game::random_special_count;
using gl::game::SpecialEngine;

namespace {
SpecialSlot value_slot(SpecialFlags tag, std::uint8_t access, float value) {
    return SpecialSlot{tag, access, value, 0.0f};
}

SpecialSlot empty_slot() { return SpecialSlot{}; }

SpecialDef make_def(SpecialId id, std::initializer_list<SpecialSlot> values,
                    std::initializer_list<SpecialSlot> times) {
    SpecialDef def;
    def.id = id;
    std::size_t index = 0;
    for (const SpecialSlot &slot : values) {
        def.value_slots[index++] = slot;
    }
    index = 0;
    for (const SpecialSlot &slot : times) {
        def.time_slots[index++] = slot;
    }
    return def;
}

SpecialDatabase make_db(std::vector<SpecialDef> defs, std::vector<SpecialId> pool) {
    SpecialDatabase db;
    db.defs = std::move(defs);
    db.item_classes[ItemType::sword] = std::move(pool);
    return db;
}

std::vector<SpecialId> mod_ids(const std::vector<SpecialMod> &mods) {
    std::vector<SpecialId> ids;
    ids.reserve(mods.size());
    for (const SpecialMod &mod : mods)
        ids.push_back(mod.id);
    return ids;
}

void check_mod(const SpecialMod &mod, SpecialId id, std::int32_t value, SpecialFlags flags) {
    CHECK(mod.id == id);
    CHECK(mod.value == value);
    CHECK(mod.flags == flags);
}

std::uint32_t next_after(std::uint32_t seed, int draws) {
    MsvcRandom rng(seed);
    for (int index = 0; index < draws; ++index)
        rng.next();
    return rng.next();
}
} // namespace

TEST_CASE("random_special_count normal branch fixed seeds") {
    struct Case {
        std::uint32_t flags;
        std::uint32_t seed;
        int count;
        int draws;
        std::uint32_t next;
    };
    const Case cases[] = {
        {0u, 1u, 1, 2, 6334u},     {0u, 3u, 2, 3, 9091u},    {0u, 21u, 3, 3, 16246u},
        {0u, 4504u, 0, 1, 20205u}, {0x44u, 1u, 1, 2, 6334u},
    };
    for (const Case &item : cases) {
        CAPTURE(item.flags);
        CAPTURE(item.seed);
        MsvcRandom rng(item.seed);
        CHECK(random_special_count(item.flags, rng) == item.count);
        CHECK(rng.next() == item.next);
        CHECK(next_after(item.seed, item.draws) == item.next);
    }
}

TEST_CASE("random_special_count flag 0x40 branch fixed seeds") {
    struct Case {
        std::uint32_t flags;
        std::uint32_t seed;
        int count;
        int draws;
        std::uint32_t next;
    };
    const Case cases[] = {
        {0x40u, 892u, 0, 1, 27042u}, {0x40u, 1u, 1, 2, 6334u},     {0x40u, 33u, 2, 3, 10094u},
        {0x40u, 30u, 3, 4, 23380u},  {0x40u, 20343u, 4, 5, 4879u}, {0x44u, 1u, 1, 2, 6334u},
    };
    for (const Case &item : cases) {
        CAPTURE(item.flags);
        CAPTURE(item.seed);
        MsvcRandom rng(item.seed);
        const int count = random_special_count(item.flags, rng);
        CHECK(count == item.count);
        CHECK(count <= 9);
        CHECK(rng.next() == item.next);
        CHECK(next_after(item.seed, item.draws) == item.next);
    }
}

TEST_CASE("special engine pool is identity intersection in ascending id order") {
    SpecialDatabase db = make_db({make_def(0, {}, {}), make_def(2, {}, {}), make_def(5, {}, {}),
                                  make_def(7, {}, {}), make_def(57, {}, {})},
                                 {2, 5, 57, 7, 0, 58, 60});

    MsvcRandom none(1);
    CHECK(SpecialEngine::apply(db, ItemType::sword, 0, 1, none).empty());
    CHECK(none.next() == 41u);

    MsvcRandom all_seed(1);
    const auto all = SpecialEngine::apply(db, ItemType::sword, 10, 1, all_seed);
    CHECK(mod_ids(all) == std::vector<SpecialId>{0, 2, 5, 7, 57});
    CHECK(all_seed.next() == 15724u);

    MsvcRandom no_class(1);
    CHECK(SpecialEngine::apply(db, ItemType::helmet, 3, 1, no_class).empty());
    CHECK(no_class.next() == 41u);
}

TEST_CASE("special engine R6 removes pool_size-nspec entries with shrinking bounds") {
    SpecialDatabase db = make_db({make_def(0, {}, {}), make_def(2, {}, {}), make_def(5, {}, {}),
                                  make_def(7, {}, {}), make_def(57, {}, {})},
                                 {2, 5, 57, 7, 0, 58, 60});

    struct Case {
        std::uint32_t seed;
        int nspec;
        std::vector<SpecialId> ids;
        std::vector<std::int32_t> values;
    };
    const Case cases[] = {
        {1u, 1, {5}, {2}},
        {1u, 2, {5, 57}, {2, 2}},
        {1u, 3, {2, 5, 57}, {1, 2, 2}},
        {1u, 4, {2, 5, 7, 57}, {2, 1, 2, 2}},
        {7u, 1, {57}, {1}},
        {7u, 2, {2, 57}, {1, 1}},
        {7u, 3, {2, 5, 57}, {1, 1, 1}},
        {7u, 4, {2, 5, 7, 57}, {2, 1, 1, 1}},
    };
    for (const Case &item : cases) {
        CAPTURE(item.seed);
        CAPTURE(item.nspec);
        MsvcRandom rng(item.seed);
        const auto mods = SpecialEngine::apply(db, ItemType::sword, item.nspec, 1, rng);
        REQUIRE(mods.size() == item.ids.size());
        for (std::size_t index = 0; index < mods.size(); ++index) {
            check_mod(mods[index], item.ids[index], item.values[index], 0u);
        }
        CHECK(rng.next() == next_after(item.seed, 5));
    }
}

TEST_CASE("special engine R7 selects max health and max energy value slots") {
    SpecialDatabase db;
    db.defs = {
        make_def(
            22,
            {value_slot(0x40, 1, -1.0f), value_slot(0x80, 1, -0.6f), value_slot(0x100, 0, 1.0f)},
            {value_slot(1, 1, 0.5f), value_slot(2, 0, 1.0f), value_slot(4, 0, 1.0f)}),
        make_def(
            23,
            {value_slot(0x40, 1, -1.0f), value_slot(0x80, 1, -0.6f), value_slot(0x100, 0, 1.0f)},
            {value_slot(1, 1, 0.5f), value_slot(2, 0, 1.0f), value_slot(4, 0, 1.0f)}),
    };
    db.item_classes[ItemType::helmet] = {22, 23};

    struct Case {
        std::uint32_t seed;
        std::uint32_t id;
        std::int32_t value;
        SpecialFlags flags;
        std::uint32_t next;
    };
    const Case cases[] = {
        {5006u, 22u, -2, 0x41u, 8217u},
        {5007u, 22u, -2, 0x81u, 32281u},
        {3u, 23u, -1, 0x41u, 9091u},
        {1u, 23u, -1, 0x81u, 26500u},
    };
    for (const Case &item : cases) {
        CAPTURE(item.seed);
        MsvcRandom rng(item.seed);
        const auto mods = SpecialEngine::apply(db, ItemType::helmet, 1, 1, rng);
        REQUIRE(mods.size() == 1);
        check_mod(mods[0], item.id, item.value, item.flags);
        CHECK(rng.next() == item.next);
    }
}

TEST_CASE("special engine R8 reads only the first three time slots") {
    SpecialDatabase db;
    db.defs = {
        make_def(7, {value_slot(0x40, 1, 2.0f), empty_slot(), empty_slot()},
                 {value_slot(1, 1, 3.0f), value_slot(2, 1, 5.0f), value_slot(4, 1, 7.0f),
                  value_slot(8, 0, 1.0f), value_slot(0x10, 0, 1.0f), value_slot(0x20, 0, 1.0f)})};
    db.item_classes[ItemType::sword] = {7};

    struct Case {
        std::uint32_t seed;
        std::int32_t value;
        SpecialFlags flags;
        std::uint32_t next;
    };
    const Case cases[] = {
        {1u, 5, 0x41u, 6334u},
        {3338u, 5, 0x42u, 14082u},
        {6679u, 7, 0x44u, 27751u},
    };
    for (const Case &item : cases) {
        CAPTURE(item.seed);
        MsvcRandom rng(item.seed);
        const auto mods = SpecialEngine::apply(db, ItemType::sword, 1, 1, rng);
        REQUIRE(mods.size() == 1);
        check_mod(mods[0], 7u, item.value, item.flags);
        CHECK(rng.next() == item.next);
    }
}

TEST_CASE("special engine R9 and R10 conditions") {
    struct Case {
        SpecialId id;
        SpecialFlags tag;
        float base;
        int level;
        std::uint32_t seed;
        std::int32_t value;
        SpecialFlags flags;
        int draws;
    };
    const Case cases[] = {
        {50u, 0x40u, 2.0f, 3, 1u, 3, 0x41u, 1},     {50u, 0x40u, 2.0f, 3, 5006u, 5, 0x41u, 1},
        {51u, 0x40u, 4.0f, 1, 1u, 2, 0x41u, 1},     {51u, 0x40u, 4.0f, 1, 6678u, 4, 0x41u, 1},
        {0u, 0x80u, 1.0f, 1, 1u, 1, 0x81u, 1},      {0u, 0x80u, 1.0f, 1, 7514u, 4, 0x81u, 1},
        {1u, 0x80u, -3.0f, 2, 1u, -3, 0x81u, 0},    {2u, 0x40u, -1.0f, 1, 1u, -1, 0x41u, 1},
        {2u, 0x40u, -1.0f, 1, 5006u, -2, 0x41u, 1}, {3u, 0x40u, -3.0f, 1, 1u, -2, 0x41u, 0},
        {4u, 0x80u, -1.0f, 1, 1u, -1, 0x81u, 1},    {4u, 0x80u, -1.0f, 1, 7514u, -4, 0x81u, 1},
        {5u, 0x80u, 4.0f, 1, 1u, 4, 0x81u, 2},      {5u, 0x80u, 4.0f, 1, 3u, 2, 0x81u, 2},
        {6u, 0x40u, 1.0f, 1, 1u, 1, 0x41u, 1},      {6u, 0x40u, 1.0f, 1, 5006u, 2, 0x41u, 1},
    };
    for (const Case &item : cases) {
        CAPTURE(item.id);
        CAPTURE(item.seed);
        SpecialDatabase db;
        db.defs = {make_def(item.id,
                            {value_slot(item.tag, 1, item.base), empty_slot(), empty_slot()},
                            {value_slot(1, 1, 1.0f)})};
        db.item_classes[ItemType::sword] = {item.id};

        MsvcRandom rng(item.seed);
        const auto mods = SpecialEngine::apply(db, ItemType::sword, 1, item.level, rng);
        REQUIRE(mods.size() == 1);
        check_mod(mods[0], item.id, item.value, item.flags);
        CHECK(rng.next() == next_after(item.seed, item.draws));
    }
}

TEST_CASE("special engine combines value and time tags and caller marks 0x200") {
    SpecialDatabase db;
    db.defs = {make_def(8, {value_slot(0x100, 1, 1.0f), empty_slot(), empty_slot()},
                        {value_slot(1, 0, 1.0f), value_slot(2, 1, 1.0f)})};
    db.item_classes[ItemType::sword] = {8};

    MsvcRandom rng(1);
    const auto mods = SpecialEngine::apply(db, ItemType::sword, 1, 1, rng);
    REQUIRE(mods.size() == 1);
    check_mod(mods[0], 8u, 1, 0x102u);
    CHECK(rng.next() == 18467u);

    gl::assets::InventoryItem item;
    item.flags = 0x104u;
    item.mods = mods;
    item.mod_count = static_cast<std::uint32_t>(item.mods.size());
    item.flags |= 0x200u;
    CHECK(item.flags == 0x304u);
    CHECK(item.mod_count == 1u);
}

TEST_CASE("special engine skips a pool id without a SpecialDef") {
    const SpecialDatabase db =
        make_db({make_def(5, {value_slot(0x40, 1, 1.0f), empty_slot(), empty_slot()},
                          {value_slot(1, 1, 1.0f)})},
                {5, 6});

    MsvcRandom rng(1);
    const auto mods = SpecialEngine::apply(db, ItemType::sword, 2, 1, rng);
    REQUIRE(mods.size() == 1);
    check_mod(mods[0], 5u, 1, 0x41u);
}

TEST_CASE("special engine uses x87 single-precision product") {
    SpecialDatabase db;
    SpecialDef def;
    def.id = 0;
    def.value_slots[0] = value_slot(0x40, 1, 0x1.4d29c2p+0f);
    def.time_slots[0] = value_slot(0, 1, 1.0f);
    db.defs = {def};
    db.item_classes[ItemType::sword] = {0};

    MsvcRandom rng(9969);
    const std::vector<SpecialMod> mods =
        SpecialEngine::apply(db, ItemType::sword, 1, 282, rng);
    REQUIRE(mods.size() == 1);
    CHECK(mods[0].id == 0);
    CHECK(mods[0].value == 367);
}
