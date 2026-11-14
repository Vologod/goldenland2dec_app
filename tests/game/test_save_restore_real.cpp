#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include "gl/game/save_restore.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

struct PinnedPerson {
    const char *name;
    std::int32_t current_x;
    std::int32_t current_y;
    std::int32_t anchor_x;
    std::int32_t anchor_y;
    std::uint8_t direction;
    std::int32_t route_type;
    const char *route;
    std::int32_t radius;
    std::int32_t delay_min;
    std::int32_t delay_max;
    const char *tribe;
    const char *dialog;
    const char *inventory;
    std::int32_t hp;
    std::int32_t energy;
    std::int32_t route_index;
};

const std::vector<PinnedPerson> kSlot0Pins{
    {"L93_2.P292_Chernobogs_Guardian2", 131, 115, 131, 115, 4, 0, "", 10, 0, 9000, "chernobog",
     "scen2\\37_1_soul.age", "inv012_civil", 136, 73, 0},
    {"L93_2.P292_Chernobogs_Guardian2", 103, 122, 103, 122, 5, 0, "", 10, 0, 9000, "chernobog",
     "scen2\\37_1_soul.age", "inv005_rich_bowman", 136, 73, 0},
    {"L93_4.P284_Chernobog", 120, 127, 120, 127, 5, 0, "", 10, 0, 9000, "chernobog",
     "scen2\\l93_4.p284_chernobog.d284.age", "", 826, 136, 0},
};

const std::vector<PinnedPerson> kSlot21Pins{
    {"L1_1.P387_Poison_Plotnik", 133, 12, 133, 12, 5, 0, "", 15, 0, 9000, "L1_1_citizen",
     "l1_1\\l1_1.p387_poison_plotnik.d387.age", "p387", 51, 39, 0},
    {"L0.P678_citizen4", 57, 51, 59, 49, 1, 3, "track4", 15, 0, 15000, "L1_2_citizen",
     "l1_1_nap\\l1_svet_malepeasant1.age", "inv012_rich_civil", 54, 23, 2},
    {"L0.P7_Kolbasens_Rodstvennik1", 196, 56, 192, 49, 5, 4, "", 15, 0, 9000, "L1_2_citizen",
     "l1_1_nap\\l1_svet_malepeasant1.age", "inv018_garbage_reagent", 74, 39, 0},
    {"L1_1.P389_Crying_Guardian", 38, 51, 38, 51, 5, 0, "", 15, 0, 9000, "L1_1_guard",
     "l1_1\\l1_1.p389_crying_guardian.d389.age", "inv001_swordsman", 59, 30, 0},
    {"L1_2.P404_Sister_Of_Mercy2", 127, 87, 127, 87, 3, 0, "", 15, 0, 9000, "L1_3_citizen",
     "l1_1\\l1_2.p403_sister_of_mercy1.d403.age", "inv011_caster", 17, 71, 0},
    {"L0.P785_witch3", 129, 91, 129, 91, 3, 0, "", 15, 0, 9000, "L1_3_citizen",
     "l1_1\\l1_2.p402_sister_of_mercy_leader.d402.age", "inv011_caster", 51, 110, 0},
    {"L1_2.P403_Sister_Of_Mercy1", 134, 91, 134, 91, 3, 0, "", 15, 0, 9000, "L1_3_citizen",
     "l1_1\\l1_2.p403_sister_of_mercy1.d403.age", "inv014_money", 15, 70, 0},
    {"L0.P693_citizen6", 99, 121, 99, 121, 5, 0, "", 15, 0, 9000, "L1_1_citizen",
     "l1_1_nap\\l1_svet_femalepeasant1.age", "inv018_food_potion", 21, 76, 0},
    {"L0.P711_citizen8", 102, 124, 102, 124, 1, 0, "", 15, 0, 9000, "L1_1_citizen",
     "l1_1_nap\\l1_svet_malepeasant3.age", "inv014_money", 54, 21, 0},
    {"L0.P692_citizen5", 157, 128, 159, 124, 3, 3, "track1", 15, 0, 9000, "L1_1_citizen",
     "l1_1_nap\\l1_svet_femalepeasant2.age", "inv012_civil", 17, 71, 0},
    {"L0.P744_oficer1", 66, 136, 66, 136, 7, 0, "", 15, 0, 4000, "L1_1_guard",
     "l81.posilatel.d244.age", "inv001_swordsman", 74, 39, 0},
    {"L0.P744_oficer1", 186, 147, 190, 148, 1, 2, "track3", 15, 0, 10000, "L1_1_guard",
     "l81.posilatel.d244.age", "inv001_swordsman", 74, 39, 1},
    {"L0.P640_children2", 177, 186, 71, 172, 6, 3, "track2", 15, 0, 4000, "kids", "inter_kid1.age",
     "inv018_garbage_reagent", 55, 30, 8},
    {"L1.P621_Svet_3CircusWorkerV", 187, 185, 187, 185, 5, 0, "", 15, 0, 9000, "L1_2_citizen",
     "l1_1\\l1.p621_svet_3circusworkerv.d621.age", "inv012_civil", 44, 30, 0},
    {"L0.P733_artist1", 191, 188, 191, 188, 1, 0, "", 15, 0, 9000, "L1_2_citizen",
     "l81.posilatel.d244.age", "inv012_civil", 44, 30, 0},
    {"L1.P620_Svet_3CircusWorker2", 168, 199, 173, 189, 3, 4, "", 15, 0, 9000, "L1_2_citizen",
     "l1_1\\l1.p619_svet_3circusworker1.d619.age", "inv012_civil", 44, 32, 0},
};

std::string_view route_type_name(std::int32_t route_type) {
    switch (route_type) {
    case 0:
        return "STAY";
    case 1:
        return "STAY_ROTATE";
    case 2:
        return "MOVED";
    case 3:
        return "MOVED_FLIP";
    case 4:
        return "RANDOM_RADIUS";
    case 5:
        return "RANDOM";
    default:
        return "";
    }
}

struct IndependentCounts {
    std::size_t matched = 0;
    std::size_t sef_only = 0;
    std::size_t type2_only = 0;
};

IndependentCounts independent_counts(const gl::game::Level &level,
                                     const gl::assets::SaveTailEntry &tail) {
    std::map<std::string, std::deque<std::size_t>, std::less<>> occurrences;
    for (std::size_t index = 0; index < level.persons.size(); ++index)
        occurrences[level.persons[index].name].push_back(index);

    IndependentCounts counts;
    for (const std::string &name : tail.person_names) {
        auto &queue = occurrences[name];
        if (queue.empty()) {
            ++counts.type2_only;
        } else {
            queue.pop_front();
            ++counts.matched;
        }
    }
    counts.sef_only = level.persons.size() - counts.matched;
    return counts;
}

std::size_t type2_entity_count(const gl::assets::SaveTailEntry &tail) {
    return static_cast<std::size_t>(
        std::count_if(tail.entities.begin(), tail.entities.end(),
                      [](const gl::assets::SaveEntity &entity) { return entity.type == 2; }));
}

struct SlotRestore {
    gl::assets::SavePayload payload;
    gl::game::Level level;
    gl::game::RestoreResult result;
};

SlotRestore restore_slot(gl::assets::GameVfs &vfs, int slot) {
    auto payload = gl::assets::parse_save_payload(read_save(slot));
    REQUIRE(payload.has_value());
    REQUIRE(payload->level_tail_index.has_value());
    const gl::assets::SaveTailEntry &tail = payload->tail[*payload->level_tail_index];

    auto level = gl::game::load_level(vfs, payload->info.level_id);
    REQUIRE(level.has_value());

    SlotRestore restored{std::move(payload.value()), std::move(level.value()), {}};
    restored.result =
        gl::game::restore_save_persons(restored.level, tail, &restored.payload.tribes);
    return restored;
}

void check_pins(const SlotRestore &restored, const std::vector<PinnedPerson> &pins) {
    REQUIRE(restored.result.persons.size() == pins.size());
    REQUIRE(restored.level.persons.size() == pins.size());
    for (std::size_t index = 0; index < pins.size(); ++index) {
        const PinnedPerson &pin = pins[index];
        const gl::game::PersonInstance &person = restored.level.persons[index];
        const gl::game::RestoredPerson &runtime = restored.result.persons[index];
        CAPTURE(index);
        CAPTURE(pin.name);
        CHECK(person.name == pin.name);
        CHECK(person.tile_x == pin.anchor_x);
        CHECK(person.tile_y == pin.anchor_y);
        CHECK(person.direction == pin.direction);
        CHECK(person.route_type == route_type_name(pin.route_type));
        CHECK(person.route == pin.route);
        CHECK(person.radius == pin.radius);
        CHECK(person.delay_min == pin.delay_min);
        CHECK(person.delay_max == pin.delay_max);
        CHECK(person.tribe == pin.tribe);
        CHECK(person.dialog == pin.dialog);
        CHECK(person.inventory == pin.inventory);
        REQUIRE(person.saved_life.has_value());
        REQUIRE(person.saved_energy.has_value());
        CHECK(*person.saved_life == pin.hp);
        CHECK(*person.saved_energy == pin.energy);
        CHECK(runtime.current == (gl::game::Tile{pin.current_x, pin.current_y}));
        CHECK(runtime.direction == pin.direction);
        CHECK(runtime.route_index == pin.route_index);
    }
}
} // namespace

TEST_CASE("real data: save restore slot pins" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());

    {
        const SlotRestore restored = restore_slot(vfs.value(), 0);
        CHECK(restored.result.counts.matched == 3);
        CHECK(restored.result.counts.sef_only == 0);
        CHECK(restored.result.counts.type2_only == 0);
        check_pins(restored, kSlot0Pins);
    }
    {
        const SlotRestore restored = restore_slot(vfs.value(), 2);
        CHECK(restored.result.counts.matched == 0);
        CHECK(restored.result.counts.sef_only == 1);
        CHECK(restored.result.counts.type2_only == 0);
        REQUIRE(restored.level.persons.size() == 1);
        CHECK(restored.level.persons[0].name == "L6_4.P199_Shurba_Hal");
    }
    {
        const SlotRestore restored = restore_slot(vfs.value(), 21);
        CHECK(restored.result.counts.matched == 16);
        CHECK(restored.result.counts.sef_only == 0);
        CHECK(restored.result.counts.type2_only == 0);
        check_pins(restored, kSlot21Pins);
    }
}

TEST_CASE("real data: save restore corpus aggregate" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());

    IndependentCounts expected_total;
    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        REQUIRE(payload->level_tail_index.has_value());
        const gl::assets::SaveTailEntry &tail = payload->tail[*payload->level_tail_index];

        auto level = gl::game::load_level(vfs.value(), payload->info.level_id);
        REQUIRE(level.has_value());
        CHECK(type2_entity_count(tail) == tail.person_names.size());

        const IndependentCounts expected = independent_counts(level.value(), tail);
        const gl::game::RestoreResult actual =
            gl::game::restore_save_persons(level.value(), tail, &payload->tribes);

        CAPTURE(slot);
        CAPTURE(payload->info.level_id);
        CHECK(actual.counts.matched == expected.matched);
        CHECK(actual.counts.sef_only == expected.sef_only);
        CHECK(actual.counts.type2_only == expected.type2_only);

        expected_total.matched += expected.matched;
        expected_total.sef_only += expected.sef_only;
        expected_total.type2_only += expected.type2_only;
    }

    CHECK(expected_total.matched == 496);
    CHECK(expected_total.sef_only == 43);
    CHECK(expected_total.type2_only == 3);
}
