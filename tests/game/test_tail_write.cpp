#include "gl/game/tail_write.hpp"

#include "gl/assets/save_item_convert.hpp"
#include "gl/game/effect.hpp"
#include "gl/game/rginf.hpp"
#include "gl/game/save_person.hpp"

#include <array>
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {
gl::assets::SaveItemRecord make_item(std::uint32_t id, std::uint32_t count) {
    gl::assets::SaveItemRecord item;
    item.item_id = id;
    item.count = count;
    item.durability = 50;
    item.type = 18;
    item.flags = 0xC0000000u;
    item.style = 7;
    item.mod_count = 1;
    item.modifiers.push_back({{{3, 1, 80, 0x00001E00u, 0xFFFFFFFFu}}});
    return item;
}

gl::game::SavePersonRecord make_person(std::string name, std::int32_t x, std::int32_t y,
                                       std::uint8_t direction) {
    gl::game::SavePersonRecord record;
    record.name = std::move(name);
    record.current_tile_x = x;
    record.current_tile_y = y;
    record.anchor_tile_x = x;
    record.anchor_tile_y = y;
    record.direction = direction;
    record.route_type = 0;
    record.route_name = "r1";
    record.radius = 13;
    record.delay_min = 100;
    record.delay_max = 200;
    record.tribe_id = 5;
    record.dialog = "d1.cs";
    record.inventory = "inv1.inv";
    record.field_6cc = 1;
    record.hp = 100;
    record.energy = 50;
    record.ap = 10;
    record.containers[0].flag = 1;
    record.containers[0].present = true;
    record.containers[0].items.push_back(make_item(812, 2));
    record.containers[1].flag = 1;
    record.containers[1].present = true;
    record.containers[1].items.push_back(make_item(913, 3));
    record.containers[2].flag = 1;
    record.containers[2].present = true;
    record.containers[2].raw_items = {0, 0, 0, 0};
    record.extra.push_back({});
    record.tail_count = 1;
    return record;
}

gl::assets::SaveEntity person_entity(const gl::game::SavePersonRecord &record) {
    const auto encoded = gl::game::encode_save_person_body(record);
    REQUIRE(encoded.has_value());
    return gl::assets::SaveEntity{2, encoded.value()};
}

gl::game::Level make_level(std::vector<gl::game::PersonInstance> persons) {
    gl::game::Level level;
    level.name = "test";
    level.width = 240 * 12;
    level.height = 240 * 9;
    level.persons = std::move(persons);
    return level;
}

gl::game::PersonInstance make_instance(std::string name, std::int32_t x, std::int32_t y,
                                       std::uint8_t direction) {
    gl::game::PersonInstance person;
    person.name = std::move(name);
    person.tile_x = x;
    person.tile_y = y;
    person.direction = direction;
    person.route_type = "STAY";
    person.radius = 13;
    person.delay_min = 100;
    person.delay_max = 200;
    person.tribe = "TribeFive";
    person.dialog = "d1.cs";
    person.inventory = "inv1.inv";
    person.saved_life = 100;
    person.saved_energy = 50;
    return person;
}

gl::game::RestoreResult make_restore(std::size_t entity_index, std::size_t level_index) {
    gl::game::RestoreResult restore;
    gl::game::RestoredPerson person;
    person.level_index = level_index;
    person.entity_index = entity_index;
    restore.persons.push_back(person);
    return restore;
}

gl::game::InventoryRuntime make_inventory(const gl::game::SavePersonRecord &record) {
    auto inventory = gl::game::InventoryRuntime::create({}, {}, {});
    std::vector<gl::assets::InventoryItem> main;
    for (const auto &item : record.containers[0].items)
        main.push_back(gl::assets::inventory_item_from_save(item));
    std::vector<gl::assets::InventoryItem> trade;
    for (const auto &item : record.containers[1].items)
        trade.push_back(gl::assets::inventory_item_from_save(item));
    inventory.replace_person_items(record.name, std::move(main));
    inventory.replace_person_trade_items(record.name, std::move(trade));
    return inventory;
}
} // namespace

TEST_CASE("build_selected_tail is byte-identical when nothing changes") {
    const auto record = make_person("A", 10, 10, 0);
    gl::assets::SaveTailEntry loaded;
    loaded.name = "test";
    loaded.person_names = {"A"};
    loaded.rginf.latch = 1;
    loaded.entities.push_back(person_entity(record));
    const auto section =
        gl::assets::encode_tail_section(loaded.rginf, loaded.person_names, loaded.entities);
    REQUIRE(section.has_value());
    loaded.section = section.value();
    loaded.raw_len = static_cast<std::uint32_t>(loaded.section.size());

    const auto level = make_level({make_instance("A", 10, 10, 0)});
    const auto simulation = gl::game::LevelSimulation::create(level, 1);
    const auto inventory = make_inventory(record);
    const std::array<gl::game::TailPersonLive, 1> live{gl::game::TailPersonLive{}};
    const auto rebuilt = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(0, 0), live, {}, loaded.rginf, nullptr);
    REQUIRE(rebuilt.has_value());
    CHECK(rebuilt->section == loaded.section);
}

TEST_CASE("build_selected_tail patches live fields and containers") {
    const auto record = make_person("A", 10, 10, 0);
    gl::assets::SaveTailEntry loaded;
    loaded.name = "test";
    loaded.rginf.latch = 1;
    loaded.rginf.datetime.fill(9);
    loaded.entities.push_back(person_entity(record));
    auto level = make_level({make_instance("A", 10, 10, 0)});
    auto simulation = gl::game::LevelSimulation::create(level, 1);
    simulation.restore_person(0, {12, 9}, 5, 0);
    auto inventory = make_inventory(record);
    std::vector<gl::assets::InventoryItem> main;
    main.push_back(gl::assets::inventory_item_from_save(make_item(812, 5)));
    inventory.replace_person_items("A", std::move(main));
    std::array<gl::game::TailPersonLive, 1> live{};
    live[0].hp = 42;
    live[0].energy = 43;
    live[0].ap = 44;
    gl::assets::SaveRginf rginf;
    rginf.latch = 7;
    rginf.datetime.fill(9);

    const auto rebuilt = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(0, 0), live, {}, rginf, nullptr);
    REQUIRE(rebuilt.has_value());
    CHECK(rebuilt->rginf.latch == 7);
    CHECK(rebuilt->rginf.datetime[0] == 9);
    REQUIRE(rebuilt->entities.size() == 1);
    const auto decoded = gl::game::decode_save_person(rebuilt->entities[0]);
    REQUIRE(decoded.has_value());
    CHECK(decoded->current_tile_x == 12);
    CHECK(decoded->current_tile_y == 9);
    CHECK(decoded->direction == 5);
    CHECK(decoded->hp == 42);
    CHECK(decoded->energy == 43);
    CHECK(decoded->ap == 44);
    REQUIRE(decoded->containers[0].items.size() == 1);
    CHECK(decoded->containers[0].items[0].count == 5);
    REQUIRE(decoded->containers[1].items.size() == 1);
    CHECK(decoded->containers[1].items[0].item_id == 913);
    CHECK(decoded->containers[2].raw_items == record.containers[2].raw_items);
}

TEST_CASE("build_selected_tail drops dead and preserves non-person entities") {
    const auto record = make_person("A", 10, 10, 0);
    gl::assets::SaveTailEntry loaded;
    loaded.entities.push_back(gl::assets::SaveEntity{8, {1, 2, 3}});
    loaded.entities.push_back(person_entity(record));
    const auto level = make_level({make_instance("A", 10, 10, 0)});
    const auto simulation = gl::game::LevelSimulation::create(level, 1);
    const auto inventory = make_inventory(record);
    std::array<gl::game::TailPersonLive, 1> live{};
    live[0].alive = false;

    const auto rebuilt = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(1, 0), live, {}, loaded.rginf, nullptr);
    REQUIRE(rebuilt.has_value());
    REQUIRE(rebuilt->entities.size() == 1);
    CHECK(rebuilt->entities[0].type == 8);
    CHECK((rebuilt->entities[0].body == std::vector<std::uint8_t>{1, 2, 3}));
    CHECK(rebuilt->person_names.empty());
}

TEST_CASE("build_selected_tail appends new persons") {
    const auto record = make_person("A", 10, 10, 0);
    gl::assets::SaveTailEntry loaded;
    loaded.entities.push_back(person_entity(record));
    const std::array<std::size_t, 1> new_persons{1};
    auto level = make_level({make_instance("A", 10, 10, 0), make_instance("B", 4, 5, 2)});
    const auto simulation = gl::game::LevelSimulation::create(level, 1);
    const auto inventory = make_inventory(record);
    const std::array<gl::game::TailPersonLive, 1> live{};
    gl::assets::SaveTribeTable tribes;
    tribes.entries.push_back(gl::assets::SaveTribeEntry{5, "TribeFive"});

    const auto rebuilt = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(0, 0), live, new_persons, loaded.rginf, &tribes);
    REQUIRE(rebuilt.has_value());
    REQUIRE(rebuilt->entities.size() == 2);
    const auto appended = gl::game::decode_save_person(rebuilt->entities[1]);
    REQUIRE(appended.has_value());
    CHECK(appended->name == "B");
    CHECK(appended->current_tile_x == 4);
    CHECK(appended->current_tile_y == 5);
    CHECK(appended->direction == 2);
    CHECK(appended->tribe_id == 5);
    REQUIRE(rebuilt->person_names.size() == 2);
    CHECK(rebuilt->person_names[0] == "A");
    CHECK(rebuilt->person_names[1] == "B");
}

TEST_CASE("build_selected_tail keeps an absent container absent until items exist") {
    auto record = make_person("A", 10, 10, 0);
    record.containers[0].flag = 0;
    record.containers[0].present = false;
    record.containers[0].items.clear();
    gl::assets::SaveTailEntry loaded;
    loaded.entities.push_back(person_entity(record));
    const auto level = make_level({make_instance("A", 10, 10, 0)});
    const auto simulation = gl::game::LevelSimulation::create(level, 1);
    auto inventory = gl::game::InventoryRuntime::create({}, {}, {});
    const std::array<gl::game::TailPersonLive, 1> live{};

    const auto absent = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(0, 0), live, {}, loaded.rginf,
        nullptr);
    REQUIRE(absent.has_value());
    const auto absent_person = gl::game::decode_save_person(absent->entities[0]);
    REQUIRE(absent_person.has_value());
    CHECK(absent_person->containers[0].flag == 0);

    inventory.replace_person_items("A", {gl::assets::inventory_item_from_save(make_item(812, 1))});
    const auto present = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(0, 0), live, {}, loaded.rginf,
        nullptr);
    REQUIRE(present.has_value());
    const auto present_person = gl::game::decode_save_person(present->entities[0]);
    REQUIRE(present_person.has_value());
    CHECK(present_person->containers[0].flag == 1);
    REQUIRE(present_person->containers[0].items.size() == 1);
    CHECK(present_person->containers[0].items[0].item_id == 812);
}

TEST_CASE("build_selected_tail rejects unknown new-person route and tribe") {
    const auto record = make_person("A", 10, 10, 0);
    gl::assets::SaveTailEntry loaded;
    loaded.entities.push_back(person_entity(record));
    auto level = make_level({make_instance("A", 10, 10, 0), make_instance("B", 4, 5, 2)});
    level.persons[1].route_type = "NOPE";
    const auto simulation = gl::game::LevelSimulation::create(level, 1);
    const auto inventory = make_inventory(record);
    const std::array<gl::game::TailPersonLive, 1> live{};
    const std::array<std::size_t, 1> new_persons{1};
    gl::assets::SaveTribeTable tribes;
    tribes.entries.push_back(gl::assets::SaveTribeEntry{5, "TribeFive"});

    const auto bad_route = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(0, 0), live, new_persons,
        loaded.rginf, &tribes);
    REQUIRE_FALSE(bad_route.has_value());
    CHECK(bad_route.error().code == gl::ErrorCode::format);

    level.persons[1].route_type = "STAY";
    level.persons[1].tribe = "Unknown";
    const auto bad_tribe = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(0, 0), live, new_persons,
        loaded.rginf, &tribes);
    REQUIRE_FALSE(bad_tribe.has_value());
    CHECK(bad_tribe.error().code == gl::ErrorCode::format);
}

TEST_CASE("build_selected_tail rejects mismatched live flags") {
    const auto record = make_person("A", 10, 10, 0);
    gl::assets::SaveTailEntry loaded;
    loaded.entities.push_back(person_entity(record));
    const auto level = make_level({make_instance("A", 10, 10, 0)});
    const auto simulation = gl::game::LevelSimulation::create(level, 1);
    const auto inventory = make_inventory(record);
    const std::array<gl::game::TailPersonLive, 0> live{};
    const auto rebuilt = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(0, 0), live, {}, loaded.rginf, nullptr);
    REQUIRE_FALSE(rebuilt.has_value());
    CHECK(rebuilt.error().code == gl::ErrorCode::format);
}

TEST_CASE("build_selected_tail writes live effects and clock") {
    const auto record = make_person("A", 10, 10, 0);
    gl::assets::SaveTailEntry loaded;
    loaded.name = "test";
    loaded.rginf.latch = 1;
    loaded.entities.push_back(person_entity(record));
    const auto level = make_level({make_instance("A", 10, 10, 0)});
    const auto simulation = gl::game::LevelSimulation::create(level, 1);
    const auto inventory = make_inventory(record);
    std::array<gl::game::TailPersonLive, 1> live{};
    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x60, 0x23000000, -1};
    live[0].effects = std::vector<gl::game::PersonEffect>{effect};
    gl::game::GameDatetime clock;
    clock.year = 1124;
    clock.month = 11;
    clock.day = 27;
    clock.hour = 4;
    clock.minute = 31;
    live[0].effect_clock = clock;

    const auto rebuilt = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(0, 0), live, {}, loaded.rginf,
        nullptr);
    REQUIRE(rebuilt.has_value());
    REQUIRE(rebuilt->entities.size() == 1);
    const auto decoded = gl::game::decode_save_person(rebuilt->entities[0]);
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->extra.size() == 1);
    CHECK(decoded->tail_count == 1);
    std::int32_t words[5];
    for (std::size_t i = 0; i < 5; ++i) {
        words[i] = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(decoded->extra[0][4 * i]) |
            (static_cast<std::uint32_t>(decoded->extra[0][4 * i + 1]) << 8) |
            (static_cast<std::uint32_t>(decoded->extra[0][4 * i + 2]) << 16) |
            (static_cast<std::uint32_t>(decoded->extra[0][4 * i + 3]) << 24));
    }
    CHECK(words[0] == 9);
    CHECK(words[1] == -3);
    CHECK(static_cast<std::uint32_t>(words[3]) == 0x23000000);
    const auto clock_back = gl::game::GameDatetime::from_time_bytes(
        std::array<std::uint8_t, 0x20>{decoded->tail80[0x40], decoded->tail80[0x41],
                                        decoded->tail80[0x42], decoded->tail80[0x43],
                                        decoded->tail80[0x44], decoded->tail80[0x45],
                                        decoded->tail80[0x46], decoded->tail80[0x47],
                                        decoded->tail80[0x48], decoded->tail80[0x49],
                                        decoded->tail80[0x4A], decoded->tail80[0x4B],
                                        decoded->tail80[0x4C], decoded->tail80[0x4D],
                                        decoded->tail80[0x4E], decoded->tail80[0x4F],
                                        decoded->tail80[0x50], decoded->tail80[0x51],
                                        decoded->tail80[0x52], decoded->tail80[0x53],
                                        decoded->tail80[0x54], decoded->tail80[0x55],
                                        decoded->tail80[0x56], decoded->tail80[0x57],
                                        decoded->tail80[0x58], decoded->tail80[0x59],
                                        decoded->tail80[0x5A], decoded->tail80[0x5B],
                                        decoded->tail80[0x5C], decoded->tail80[0x5D],
                                        decoded->tail80[0x5E], decoded->tail80[0x5F]});
    CHECK(clock_back.year == 1124);
    CHECK(clock_back.minute == 31);
}
