#include "gl/assets/cp1251.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/game/person_movement.hpp"
#include "gl/game/person_runtime.hpp"
#include "gl/game/save_person.hpp"
#include "gl/game/save_restore.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <doctest/doctest.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
void put_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}

void put_string(std::vector<std::uint8_t> &out, std::string_view text) {
    const std::string encoded = gl::assets::utf8_to_cp1251(text);
    put_u32(out, static_cast<std::uint32_t>(encoded.size()));
    out.insert(out.end(), encoded.begin(), encoded.end());
}

struct PersonValues {
    std::string name = "L1_1.P387_Poison_Plotnik";
    std::uint32_t field_40 = 77;
    std::uint32_t field_44 = 2;
    std::uint32_t current_x = 133;
    std::uint32_t current_y = 12;
    std::uint32_t anchor_x = 129;
    std::uint32_t anchor_y = 9;
    std::uint32_t field_654 = 0;
    std::array<std::uint8_t, 0x20> field_658{};
    std::uint32_t direction = 5;
    std::uint32_t route_type = 2;
    std::string route = "track4";
    std::uint32_t radius = 13;
    std::uint32_t delay_min = 1000;
    std::uint32_t delay_max = 2000;
    std::uint32_t tribe = 7;
    std::string dialog = "l1_1/p387.poison.plotnik.d1.age";
    std::string inventory = "l1_1/p387.poison.plotnik";
    std::uint32_t field_6cc = 1;
    std::array<std::uint8_t, 0x11c> field_ff0{};
    std::uint32_t skipped = 0xDEADBEEF;
    std::uint32_t hp = 123;
    std::uint32_t energy = 45;
    std::uint32_t ap = 60;
    std::int32_t route_index = -1;
};

std::vector<std::uint8_t> person_prefix(const PersonValues &values) {
    std::vector<std::uint8_t> out;
    put_string(out, values.name);
    put_u32(out, values.field_40);
    put_u32(out, values.field_44);
    put_u32(out, values.current_x);
    put_u32(out, values.current_y);
    put_u32(out, values.anchor_x);
    put_u32(out, values.anchor_y);
    put_u32(out, values.field_654);
    out.insert(out.end(), values.field_658.begin(), values.field_658.end());
    put_u32(out, values.direction);
    put_u32(out, values.route_type);
    put_string(out, values.route);
    put_u32(out, values.radius);
    put_u32(out, values.delay_min);
    put_u32(out, values.delay_max);
    put_u32(out, values.tribe);
    put_string(out, values.dialog);
    put_string(out, values.inventory);
    put_u32(out, values.field_6cc);
    out.insert(out.end(), values.field_ff0.begin(), values.field_ff0.end());
    put_u32(out, values.skipped);
    put_u32(out, values.hp);
    put_u32(out, values.energy);
    put_u32(out, values.ap);
    put_u32(out, std::bit_cast<std::uint32_t>(values.route_index));
    return out;
}

void put_u32_at(std::vector<std::uint8_t> &out, std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out[offset + shift / 8] = static_cast<std::uint8_t>((value >> shift) & 0xFF);
}

struct ItemSpec {
    std::uint32_t id = 0;
    std::uint32_t count = 1;
    std::uint32_t flags = 0;
    std::uint32_t type = 0;
    std::uint32_t style = 0;
    std::uint32_t durability = 100;
    std::uint32_t mods = 0;
};

void put_item_record(std::vector<std::uint8_t> &out, const ItemSpec &spec) {
    const std::size_t start = out.size();
    out.resize(start + 0xC4, 0);
    put_u32_at(out, start + 0x00, 0x14092248u);
    put_u32_at(out, start + 0x08, spec.type);
    put_u32_at(out, start + 0x0C, spec.flags);
    put_u32_at(out, start + 0x20, spec.style);
    put_u32_at(out, start + 0xA8, spec.id);
    put_u32_at(out, start + 0xB0, spec.count);
    put_u32_at(out, start + 0xB4, spec.durability);
    put_u32_at(out, start + 0xC0, spec.mods);
    for (std::uint32_t index = 0; index < spec.mods; ++index)
        for (std::uint32_t word = 0; word < 5; ++word)
            put_u32(out, index * 10 + word);
}

void put_container(std::vector<std::uint8_t> &out,
                   const std::optional<std::vector<ItemSpec>> &items) {
    if (!items.has_value()) {
        put_u32(out, 0);
        return;
    }
    put_u32(out, 1);
    put_u32(out, static_cast<std::uint32_t>(items->size()));
    for (const ItemSpec &item : *items)
        put_item_record(out, item);
}

std::vector<std::uint8_t>
person_body_with_containers(const PersonValues &values,
                            const std::optional<std::vector<ItemSpec>> &c0,
                            const std::optional<std::vector<ItemSpec>> &c1,
                            const std::optional<std::vector<ItemSpec>> &c2) {
    std::vector<std::uint8_t> out = person_prefix(values);
    put_container(out, c0);
    put_container(out, c1);
    put_container(out, c2);
    out.insert(out.end(), 0x80, 0);
    put_u32(out, 0);
    return out;
}

void append_empty_containers_and_tail(std::vector<std::uint8_t> &out) {
    for (int index = 0; index < 3; ++index)
        put_u32(out, 0);
    out.insert(out.end(), 0x80, 0);
    put_u32(out, 0);
}

std::vector<std::uint8_t> person_body(const PersonValues &values) {
    std::vector<std::uint8_t> out = person_prefix(values);
    append_empty_containers_and_tail(out);
    return out;
}

gl::assets::SaveEntity person_entity(std::vector<std::uint8_t> body) {
    gl::assets::SaveEntity entity;
    entity.type = 2;
    entity.body = std::move(body);
    return entity;
}

gl::game::PersonInstance sef_person(std::string name, std::int32_t tile_x, std::int32_t tile_y,
                                    std::string route) {
    gl::game::PersonInstance person;
    person.name = std::move(name);
    person.tile_x = tile_x;
    person.tile_y = tile_y;
    person.direction = 0;
    person.route_type = "MOVED";
    person.route = std::move(route);
    person.route_cells = {{tile_x, tile_y}, {tile_x + 1, tile_y}};
    person.radius = 1;
    person.delay_min = 1;
    person.delay_max = 2;
    person.tribe = "SEF_" + person.name;
    person.dialog = "sef_dialog";
    person.inventory = "sef_inventory";
    return person;
}

struct RestoreFixture {
    gl::game::Level level;
    gl::assets::SaveTailEntry tail;
    gl::assets::SaveTribeTable tribes;
};

RestoreFixture make_restore_fixture() {
    RestoreFixture fixture;
    fixture.level.name = "test";
    fixture.level.persons.push_back(sef_person("dup", 1, 1, "r1"));
    fixture.level.persons.push_back(sef_person("dup", 2, 2, "r2"));
    fixture.level.persons.push_back(sef_person("solo", 3, 3, "r3"));

    PersonValues first;
    first.name = "dup";
    first.anchor_x = 10;
    first.anchor_y = 11;
    first.current_x = 100;
    first.current_y = 101;
    first.direction = 4;
    first.route_type = 2;
    first.route = "r1";
    first.radius = 7;
    first.delay_min = 100;
    first.delay_max = 200;
    first.tribe = 5;
    first.dialog = "dialog_one";
    first.inventory = "inventory_one";
    first.hp = 33;
    first.energy = 22;
    first.route_index = 1;

    PersonValues second;
    second.name = "dup";
    second.anchor_x = 20;
    second.anchor_y = 21;
    second.current_x = 200;
    second.current_y = 201;
    second.direction = 7;
    second.route_type = 5;
    second.route = "r2";
    second.radius = 9;
    second.delay_min = 300;
    second.delay_max = 400;
    second.tribe = 6;
    second.dialog = "dialog_two";
    second.inventory = "inventory_two";
    second.hp = 44;
    second.energy = 55;
    second.route_index = 3;

    PersonValues unmatched;
    unmatched.name = "ghost";
    unmatched.anchor_x = 30;
    unmatched.anchor_y = 31;

    fixture.tail.entities.push_back(person_entity(person_body(first)));
    fixture.tail.entities.push_back(person_entity(person_body(second)));
    fixture.tail.entities.push_back(person_entity(person_body(unmatched)));
    fixture.tribes.entries.push_back(gl::assets::SaveTribeEntry{5, "TribeFive"});
    fixture.tribes.entries.push_back(gl::assets::SaveTribeEntry{6, "TribeSix"});
    return fixture;
}

const std::vector<std::pair<std::int32_t, std::int32_t>> kDupOneCells{{1, 1}, {2, 1}};
const std::vector<std::pair<std::int32_t, std::int32_t>> kDupTwoCells{{2, 2}, {3, 2}};

gl::game::PersonInstance runtime_person(std::string name, std::optional<std::int32_t> saved_life,
                                        std::optional<std::int32_t> saved_energy) {
    gl::game::PersonInstance person;
    person.name = std::move(name);
    person.def.values["strength"] = "10";
    person.def.values["constitution"] = "8";
    person.def.values["intelligence"] = "5";
    person.def.values["wisdom"] = "4";
    person.def.values["experience_value"] = "0";
    person.def.values["life"] = "5";
    person.def.values["energy"] = "2";
    person.saved_life = saved_life;
    person.saved_energy = saved_energy;
    return person;
}
} // namespace

TEST_CASE("decode save person maps type-2 fields") {
    PersonValues values;
    values.field_658.fill(0xAB);
    values.field_ff0.fill(0xCD);

    const auto record = gl::game::decode_save_person(person_entity(person_body(values)));
    REQUIRE(record.has_value());
    CHECK(record->name == values.name);
    CHECK(record->current_tile_x == 133);
    CHECK(record->current_tile_y == 12);
    CHECK(record->anchor_tile_x == 129);
    CHECK(record->anchor_tile_y == 9);
    CHECK(record->direction == 5);
    CHECK(record->route_type == 2);
    CHECK(record->route_name == "track4");
    CHECK(record->radius == 13);
    CHECK(record->delay_min == 1000);
    CHECK(record->delay_max == 2000);
    CHECK(record->tribe_id == 7);
    CHECK(record->dialog == values.dialog);
    CHECK(record->inventory == values.inventory);
    CHECK(record->hp == 123);
    CHECK(record->energy == 45);
    CHECK(record->route_index == -1);
}

TEST_CASE("decode save person rejects a non-person entity") {
    const auto body = person_body(PersonValues{});
    gl::assets::SaveEntity entity;
    entity.type = 8;
    entity.body = body;
    const auto record = gl::game::decode_save_person(entity);
    REQUIRE_FALSE(record.has_value());
    CHECK(record.error().code == gl::ErrorCode::format);
}

TEST_CASE("decode save person rejects a truncated body") {
    SUBCASE("missing name length") {
        const auto record = gl::game::decode_save_person(person_entity({0x01, 0x02}));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
    SUBCASE("name overruns the body") {
        std::vector<std::uint8_t> body = {0x10, 0x00, 0x00, 0x00, 'a'};
        const auto record = gl::game::decode_save_person(person_entity(std::move(body)));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
    SUBCASE("prefix ends inside route index") {
        auto prefix = person_prefix(PersonValues{});
        prefix.resize(prefix.size() - 2);
        const auto record = gl::game::decode_save_person(person_entity(std::move(prefix)));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("save restore matches names by occurrence and applies fields") {
    RestoreFixture fixture = make_restore_fixture();
    const auto result =
        gl::game::restore_save_persons(fixture.level, fixture.tail, &fixture.tribes);
    const auto &counts = result.counts;

    REQUIRE(result.persons.size() == 2);
    CHECK(result.persons[0].level_index == 0);
    CHECK(result.persons[0].current == (gl::game::Tile{100, 101}));
    CHECK(result.persons[0].direction == 4);
    CHECK(result.persons[0].route_index == 1);
    CHECK(result.persons[1].level_index == 1);
    CHECK(result.persons[1].current == (gl::game::Tile{200, 201}));
    CHECK(result.persons[1].direction == 7);
    CHECK(result.persons[1].route_index == 3);

    CHECK(counts.matched == 2);
    CHECK(counts.sef_only == 1);
    CHECK(counts.type2_only == 1);
    CHECK(counts.skipped == 0);
    REQUIRE(fixture.level.persons.size() == 3);
    CHECK(fixture.level.persons[0].name == "dup");
    CHECK(fixture.level.persons[1].name == "dup");
    CHECK(fixture.level.persons[2].name == "solo");

    const gl::game::PersonInstance &first = fixture.level.persons[0];
    CHECK(first.tile_x == 10);
    CHECK(first.tile_y == 11);
    CHECK(first.direction == 4);
    CHECK(first.route_type == "MOVED");
    CHECK(first.route == "r1");
    CHECK(first.route_cells == kDupOneCells);
    CHECK(first.radius == 7);
    CHECK(first.delay_min == 100);
    CHECK(first.delay_max == 200);
    CHECK(first.tribe == "TribeFive");
    CHECK(first.dialog == "dialog_one");
    CHECK(first.inventory == "inventory_one");
    REQUIRE(first.saved_life.has_value());
    REQUIRE(first.saved_energy.has_value());
    CHECK(*first.saved_life == 33);
    CHECK(*first.saved_energy == 22);

    const gl::game::PersonInstance &second = fixture.level.persons[1];
    CHECK(second.tile_x == 20);
    CHECK(second.tile_y == 21);
    CHECK(second.direction == 7);
    CHECK(second.route_type == "RANDOM");
    CHECK(second.route == "r2");
    CHECK(second.route_cells == kDupTwoCells);
    CHECK(second.radius == 9);
    CHECK(second.delay_min == 300);
    CHECK(second.delay_max == 400);
    CHECK(second.tribe == "TribeSix");
    CHECK(second.dialog == "dialog_two");
    CHECK(second.inventory == "inventory_two");
    REQUIRE(second.saved_life.has_value());
    REQUIRE(second.saved_energy.has_value());
    CHECK(*second.saved_life == 44);
    CHECK(*second.saved_energy == 55);

    const gl::game::PersonInstance &solo = fixture.level.persons[2];
    CHECK(solo.tile_x == 3);
    CHECK(solo.tile_y == 3);
    CHECK(solo.route == "r3");
    CHECK(solo.tribe == "SEF_solo");
    CHECK_FALSE(solo.saved_life.has_value());
    CHECK_FALSE(solo.saved_energy.has_value());
}

TEST_CASE("save restore keeps BT_ENTS order when it differs from SEF order") {
    gl::game::Level level;
    level.name = "test";
    level.persons.push_back(sef_person("dup", 1, 1, "r1"));
    level.persons.push_back(sef_person("dup", 2, 2, "r2"));
    level.persons.push_back(sef_person("solo", 3, 3, "r3"));

    PersonValues solo;
    solo.name = "solo";
    solo.anchor_x = 30;
    solo.anchor_y = 31;
    solo.current_x = 40;
    solo.current_y = 41;
    solo.direction = 2;
    solo.route_type = 4;
    solo.route = "r3";
    solo.radius = 8;
    solo.delay_min = 10;
    solo.delay_max = 20;
    solo.tribe = 5;
    solo.hp = 12;
    solo.energy = 13;
    solo.route_index = 2;

    PersonValues dup;
    dup.name = "dup";
    dup.anchor_x = 10;
    dup.anchor_y = 11;
    dup.current_x = 50;
    dup.current_y = 51;
    dup.direction = 1;
    dup.route_type = 0;
    dup.route = "r1";
    dup.tribe = 5;
    dup.hp = 14;
    dup.energy = 15;
    dup.route_index = 0;

    gl::assets::SaveTailEntry tail;
    tail.entities.push_back(person_entity(person_body(solo)));
    tail.entities.push_back(person_entity(person_body(dup)));
    gl::assets::SaveTribeTable tribes;
    tribes.entries.push_back(gl::assets::SaveTribeEntry{5, "TribeFive"});

    const auto result = gl::game::restore_save_persons(level, tail, &tribes);

    CHECK(result.counts.matched == 2);
    CHECK(result.counts.sef_only == 1);
    CHECK(result.counts.type2_only == 0);
    REQUIRE(level.persons.size() == 3);
    CHECK(level.persons[0].name == "solo");
    CHECK(level.persons[1].name == "dup");
    CHECK(level.persons[2].name == "dup");
    REQUIRE(result.persons.size() == 2);
    CHECK(result.persons[0].level_index == 0);
    CHECK(result.persons[0].current == (gl::game::Tile{40, 41}));
    CHECK(result.persons[0].direction == 2);
    CHECK(result.persons[0].route_index == 2);
    CHECK(result.persons[1].level_index == 1);
    CHECK(result.persons[1].current == (gl::game::Tile{50, 51}));
    CHECK(result.persons[1].direction == 1);
    CHECK(result.persons[1].route_index == 0);
}

TEST_CASE("save restore maps an unknown route type to STAY") {
    gl::game::Level level;
    level.name = "test";
    level.persons.push_back(sef_person("m", 4, 4, "r"));

    PersonValues values;
    values.name = "m";
    values.route = "r";
    values.route_type = 9;

    gl::assets::SaveTailEntry tail;
    tail.entities.push_back(person_entity(person_body(values)));

    const auto result = gl::game::restore_save_persons(level, tail, nullptr);

    CHECK(result.counts.matched == 1);
    REQUIRE(level.persons.size() == 1);
    CHECK(level.persons[0].route_type == "STAY");
}

TEST_CASE("save restore keeps SEF route cells and tribe on mismatch") {
    gl::game::Level level;
    level.name = "test";
    level.persons.push_back(sef_person("m", 4, 4, "sef_route"));

    PersonValues values;
    values.name = "m";
    values.anchor_x = 12;
    values.anchor_y = 13;
    values.route = "saved_route";
    values.tribe = 99;

    gl::assets::SaveTailEntry tail;
    tail.entities.push_back(person_entity(person_body(values)));
    gl::assets::SaveTribeTable tribes;
    tribes.entries.push_back(gl::assets::SaveTribeEntry{5, "TribeFive"});

    const auto result = gl::game::restore_save_persons(level, tail, &tribes);
    const auto &counts = result.counts;

    CHECK(counts.matched == 1);
    CHECK(counts.sef_only == 0);
    CHECK(counts.type2_only == 0);
    REQUIRE(level.persons.size() == 1);
    CHECK(level.persons[0].tile_x == 12);
    CHECK(level.persons[0].tile_y == 13);
    CHECK(level.persons[0].route == "saved_route");
    CHECK(level.persons[0].route_cells ==
          (std::vector<std::pair<std::int32_t, std::int32_t>>{{4, 4}, {5, 4}}));
    CHECK(level.persons[0].tribe == "SEF_m");
}

TEST_CASE("save restore keeps SEF tribe without a tribe table") {
    gl::game::Level level;
    level.name = "test";
    level.persons.push_back(sef_person("m", 4, 4, "r"));

    PersonValues values;
    values.name = "m";
    values.route = "r";
    values.tribe = 7;

    gl::assets::SaveTailEntry tail;
    tail.entities.push_back(person_entity(person_body(values)));

    const auto result = gl::game::restore_save_persons(level, tail, nullptr);
    const auto &counts = result.counts;

    CHECK(counts.matched == 1);
    REQUIRE(level.persons.size() == 1);
    CHECK(level.persons[0].tribe == "SEF_m");
}

TEST_CASE("save restore drops SEF-only persons behind the gate") {
    RestoreFixture fixture = make_restore_fixture();
    const auto result =
        gl::game::restore_save_persons(fixture.level, fixture.tail, &fixture.tribes, true);
    const auto &counts = result.counts;

    REQUIRE(result.persons.size() == 2);
    CHECK(result.persons[0].level_index == 0);
    CHECK(result.persons[1].level_index == 1);
    CHECK(counts.matched == 2);
    CHECK(counts.sef_only == 1);
    CHECK(counts.type2_only == 1);
    REQUIRE(fixture.level.persons.size() == 2);
    CHECK(fixture.level.persons[0].name == "dup");
    CHECK(fixture.level.persons[1].name == "dup");
}

TEST_CASE("save restore skips non-person and malformed entities") {
    gl::game::Level level;
    level.name = "test";
    level.persons.push_back(sef_person("solo", 3, 3, "r"));

    gl::assets::SaveTailEntry tail;
    gl::assets::SaveEntity trigger;
    trigger.type = 16;
    tail.entities.push_back(std::move(trigger));
    tail.entities.push_back(person_entity({}));

    const auto result = gl::game::restore_save_persons(level, tail, nullptr);
    const auto &counts = result.counts;

    CHECK(counts.matched == 0);
    CHECK(counts.sef_only == 1);
    CHECK(counts.type2_only == 0);
    CHECK(counts.skipped == 2);
    CHECK(result.persons.empty());
    REQUIRE(level.persons.size() == 1);
    CHECK(level.persons[0].name == "solo");
}

TEST_CASE("save restore runtime uses saved NPC life and energy") {
    gl::game::PersonInstance saved = runtime_person("saved", 30, 10);
    gl::game::PersonInstance untouched = runtime_person("untouched", std::nullopt, std::nullopt);
    gl::game::PersonInstance over = runtime_person("over", 1000, 1000);

    auto runtime = gl::game::PersonRuntime::create({saved, untouched, over}, std::nullopt);

    CHECK(runtime.get_parameter("saved", gl::script::PersonParameter::life) ==
          doctest::Approx(30.0));
    CHECK(runtime.get_parameter("saved", gl::script::PersonParameter::energy) ==
          doctest::Approx(10.0));
    CHECK(runtime.get_parameter("untouched", gl::script::PersonParameter::life) ==
          doctest::Approx(5.0));
    CHECK(runtime.get_parameter("untouched", gl::script::PersonParameter::energy) ==
          doctest::Approx(2.0));
    CHECK(runtime.get_parameter("over", gl::script::PersonParameter::life) ==
          doctest::Approx(40.0));
    CHECK(runtime.get_parameter("over", gl::script::PersonParameter::energy) ==
          doctest::Approx(18.0));
}

TEST_CASE("save restore runtime applies current person state") {
    gl::game::Level level;
    level.name = "test";
    level.width = 240 * 12;
    level.height = 240 * 9;
    gl::game::PersonInstance walker = sef_person("walker", 10, 10, "r");
    walker.route_type = "MOVED";
    walker.route_cells = {{11, 10}, {14, 10}, {14, 13}};
    walker.delay_min = 0;
    walker.delay_max = 0;
    walker.go_type = gl::assets::PadAnimationType::rt_go;
    walker.go_move_x = 6.0f;
    walker.go_move_y = 5.0f;
    walker.go_delay_ms = 100;
    level.persons.push_back(std::move(walker));

    auto sim = gl::game::LevelSimulation::create(level, 1);
    sim.restore_person(0, gl::game::Tile{5, 6}, 3, 2);
    sim.restore_person(7, gl::game::Tile{0, 0}, 0, 0);

    REQUIRE(sim.persons().size() == 1);
    const gl::game::PersonMovement &restored = sim.persons()[0];
    CHECK(restored.x == doctest::Approx(5.0f * 12 + 6));
    CHECK(restored.y == doctest::Approx(6.0f * 9 + 4));
    CHECK(restored.target_x == doctest::Approx(5.0f * 12 + 6));
    CHECK(restored.target_y == doctest::Approx(6.0f * 9 + 4));
    CHECK(restored.direction8 == 3);
    CHECK(restored.direction16 == gl::game::direction8_to16(3));
    CHECK_FALSE(restored.moving);
    CHECK_FALSE(restored.waiting);
    CHECK(restored.path.empty());

    sim.advance(0.028);
    const gl::game::PersonMovement &planned = sim.persons()[0];
    REQUIRE_FALSE(planned.path.empty());
    CHECK(planned.path.back() == gl::game::Tile{14, 13});
    CHECK(planned.target_x == doctest::Approx(14.0f * 12 + 6));
    CHECK(planned.target_y == doctest::Approx(13.0f * 9 + 4));
}

TEST_CASE("decode save person reads containers and tail") {
    PersonValues values;
    values.name = "solo";
    const std::vector<ItemSpec> c0{{812, 597, 0xC0000000u, 18, 0, 93, 1},
                                   {812, 5, 0, 18, 0, 100, 0}};
    const std::vector<ItemSpec> c1{{421, 2, 0x8000000Cu, 19, 0, 100, 0}};
    const std::vector<ItemSpec> c2{{1784, 1, 0, 0, 0, 100, 0}};
    const auto record = gl::game::decode_save_person(
        person_entity(person_body_with_containers(values, c0, c1, c2)));
    REQUIRE(record.has_value());
    REQUIRE(record->containers[0].present);
    REQUIRE(record->containers[0].items.size() == 2);
    CHECK(record->containers[0].items[0].item_id == 812);
    CHECK(record->containers[0].items[0].count == 597);
    CHECK(record->containers[0].items[0].flags == 0xC0000000u);
    CHECK(record->containers[0].items[0].type == 18);
    CHECK(record->containers[0].items[0].durability == 93);
    CHECK(record->containers[0].items[0].mod_count == 1);
    REQUIRE(record->containers[0].items[0].modifiers.size() == 1);
    CHECK(record->containers[0].items[0].modifiers[0].words[0] == 0u);
    CHECK(record->containers[0].items[0].modifiers[0].words[4] == 4u);
    CHECK(record->containers[0].items[1].item_id == 812);
    CHECK(record->containers[0].items[1].count == 5);
    REQUIRE(record->containers[1].present);
    REQUIRE(record->containers[1].items.size() == 1);
    CHECK(record->containers[1].items[0].item_id == 421);
    REQUIRE(record->containers[2].present);
    REQUIRE(record->containers[2].items.size() == 1);
    CHECK(record->containers[2].items[0].item_id == 1784);
    CHECK(record->tail_count == 0);
}

TEST_CASE("decode save person distinguishes absent and empty containers") {
    PersonValues values;
    const auto record = gl::game::decode_save_person(person_entity(
        person_body_with_containers(values, std::nullopt, std::vector<ItemSpec>{}, std::nullopt)));
    REQUIRE(record.has_value());
    CHECK_FALSE(record->containers[0].present);
    CHECK(record->containers[0].items.empty());
    CHECK(record->containers[1].present);
    CHECK(record->containers[1].items.empty());
    CHECK_FALSE(record->containers[2].present);
}

TEST_CASE("decode save person rejects malformed containers and tail") {
    SUBCASE("truncated container flag") {
        auto body = person_prefix(PersonValues{});
        body.resize(body.size() + 2);
        const auto record = gl::game::decode_save_person(person_entity(std::move(body)));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
    SUBCASE("container count without item records") {
        std::vector<std::uint8_t> body = person_prefix(PersonValues{});
        put_u32(body, 1);
        put_u32(body, 3);
        const auto record = gl::game::decode_save_person(person_entity(std::move(body)));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
    SUBCASE("item subrecords overrun") {
        std::vector<std::uint8_t> body = person_prefix(PersonValues{});
        put_u32(body, 1);
        put_u32(body, 1);
        put_item_record(body, ItemSpec{812, 1, 0, 18, 0, 100, 2});
        body.resize(body.size() - 1);
        const auto record = gl::game::decode_save_person(person_entity(std::move(body)));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
    SUBCASE("truncated tail prefix") {
        std::vector<std::uint8_t> body = person_prefix(PersonValues{});
        put_u32(body, 0);
        put_u32(body, 0);
        put_u32(body, 0);
        body.insert(body.end(), 0x70, 0);
        const auto record = gl::game::decode_save_person(person_entity(std::move(body)));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
    SUBCASE("trailing bytes") {
        auto body = person_body(PersonValues{});
        body.push_back(0x7F);
        const auto record = gl::game::decode_save_person(person_entity(std::move(body)));
        REQUIRE_FALSE(record.has_value());
        CHECK(record.error().code == gl::ErrorCode::format);
    }
}

TEST_CASE("save restore applies c0 main and c1 trade containers") {
    gl::game::Level level;
    level.name = "test";
    level.persons.push_back(sef_person("solo", 3, 3, "r3"));

    PersonValues values;
    values.name = "solo";
    const std::vector<ItemSpec> c0{{812, 597, 0xC0000000u, 18, 0, 93, 0}};
    const std::vector<ItemSpec> c1{{421, 2, 0x8000000Cu, 19, 0, 100, 0}};
    const std::vector<ItemSpec> c2{{1784, 1, 0, 0, 0, 100, 0}};
    gl::assets::SaveTailEntry tail;
    tail.entities.push_back(person_entity(person_body_with_containers(values, c0, c1, c2)));

    auto runtime = gl::game::InventoryRuntime::create(gl::assets::HeroInventory{},
                                                      gl::assets::ItemNameIndex{});
    runtime.set_person_items("solo", {gl::assets::InventoryItem{999, 1, 0, 0, 0}});
    runtime.set_person_trade_items("solo", {gl::assets::InventoryItem{998, 1, 0, 0, 0}});

    const auto result = gl::game::restore_save_persons(level, tail, nullptr, false, &runtime);
    CHECK(result.counts.matched == 1);
    CHECK(result.counts.containers_applied == 1);
    CHECK(result.counts.duplicate_containers == 0);
    CHECK(result.counts.c2_items == 1);

    REQUIRE(runtime.person_items("solo").size() == 1);
    CHECK(runtime.person_items("solo")[0].item_id == 812);
    CHECK(runtime.person_items("solo")[0].count == 597);
    CHECK(runtime.person_items("solo")[0].flags == 0xC0000000u);
    CHECK(runtime.person_items("solo")[0].type == 18);
    CHECK(runtime.person_items("solo")[0].durability == 93);
    CHECK(runtime.person_items("solo")[0].mods.empty());

    REQUIRE(runtime.trade_items("solo").size() == 1);
    CHECK(runtime.trade_items("solo")[0].item_id == 421);
    CHECK(runtime.trade_items("solo")[0].count == 2);
}

TEST_CASE("save restore clears loot for absent saved containers") {
    gl::game::Level level;
    level.name = "test";
    level.persons.push_back(sef_person("solo", 3, 3, "r3"));

    PersonValues values;
    values.name = "solo";
    gl::assets::SaveTailEntry tail;
    tail.entities.push_back(person_entity(
        person_body_with_containers(values, std::nullopt, std::nullopt, std::nullopt)));

    auto runtime = gl::game::InventoryRuntime::create(gl::assets::HeroInventory{},
                                                      gl::assets::ItemNameIndex{});
    runtime.set_person_items("solo", {gl::assets::InventoryItem{999, 1, 0, 0, 0}});
    runtime.set_person_trade_items("solo", {gl::assets::InventoryItem{998, 1, 0, 0, 0}});

    const auto result = gl::game::restore_save_persons(level, tail, nullptr, false, &runtime);
    CHECK(result.counts.containers_applied == 1);
    CHECK(runtime.person_items("solo").empty());
    CHECK(runtime.trade_items("solo").empty());
}

TEST_CASE("save restore keeps the last containers for a duplicate name") {
    gl::game::Level level;
    level.name = "test";
    level.persons.push_back(sef_person("dup", 1, 1, "r1"));
    level.persons.push_back(sef_person("dup", 2, 2, "r2"));

    PersonValues first;
    first.name = "dup";
    PersonValues second;
    second.name = "dup";
    gl::assets::SaveTailEntry tail;
    tail.entities.push_back(person_entity(person_body_with_containers(
        first, std::vector<ItemSpec>{{111, 1, 0, 0, 0, 100, 0}}, std::nullopt, std::nullopt)));
    tail.entities.push_back(person_entity(person_body_with_containers(
        second, std::vector<ItemSpec>{{222, 1, 0, 0, 0, 100, 0}}, std::nullopt, std::nullopt)));

    auto runtime = gl::game::InventoryRuntime::create(gl::assets::HeroInventory{},
                                                      gl::assets::ItemNameIndex{});
    const auto result = gl::game::restore_save_persons(level, tail, nullptr, false, &runtime);
    CHECK(result.counts.matched == 2);
    CHECK(result.counts.containers_applied == 2);
    CHECK(result.counts.duplicate_containers == 1);
    REQUIRE(runtime.person_items("dup").size() == 1);
    CHECK(runtime.person_items("dup")[0].item_id == 222);
}

TEST_CASE("save restore without a runtime does not touch inventories") {
    RestoreFixture fixture = make_restore_fixture();
    auto runtime = gl::game::InventoryRuntime::create(gl::assets::HeroInventory{},
                                                      gl::assets::ItemNameIndex{});
    runtime.set_person_items("dup", {gl::assets::InventoryItem{999, 1, 0, 0, 0}});
    const auto result = gl::game::restore_save_persons(fixture.level, fixture.tail, &fixture.tribes,
                                                       false, nullptr);
    CHECK(result.counts.containers_applied == 0);
    CHECK(result.counts.duplicate_containers == 0);
    REQUIRE(runtime.person_items("dup").size() == 1);
    CHECK(runtime.person_items("dup")[0].item_id == 999);
}

TEST_CASE("decode save person accepts non-zero container flags") {
    std::vector<std::uint8_t> body = person_prefix(PersonValues{});
    for (int index = 0; index < 3; ++index) {
        put_u32(body, 2);
        put_u32(body, 0);
    }
    body.insert(body.end(), 0x80, 0);
    put_u32(body, 0);

    const auto record = gl::game::decode_save_person(person_entity(std::move(body)));
    REQUIRE(record.has_value());
    CHECK(record->containers[0].present);
    CHECK(record->containers[1].present);
    CHECK(record->containers[2].present);
    CHECK(record->containers[0].items.empty());
}

TEST_CASE("decode save person skips a positive tail") {
    std::vector<std::uint8_t> body = person_prefix(PersonValues{});
    put_u32(body, 0);
    put_u32(body, 0);
    put_u32(body, 0);
    body.insert(body.end(), 0x80, 0xAB);
    put_u32(body, 2);
    body.insert(body.end(), 0x28, 0xCD);

    const auto record = gl::game::decode_save_person(person_entity(std::move(body)));
    REQUIRE(record.has_value());
    CHECK(record->tail_count == 2);
}

TEST_CASE("decode save person rejects a tail overrun") {
    std::vector<std::uint8_t> body = person_prefix(PersonValues{});
    put_u32(body, 0);
    put_u32(body, 0);
    put_u32(body, 0);
    body.insert(body.end(), 0x80, 0);
    put_u32(body, 2);
    body.insert(body.end(), 0x14, 0);

    const auto record = gl::game::decode_save_person(person_entity(std::move(body)));
    REQUIRE_FALSE(record.has_value());
    CHECK(record.error().code == gl::ErrorCode::format);
}

TEST_CASE("save restore marks only valid inventory candidates for regeneration") {
    gl::game::Level level;
    level.name = "test";
    level.persons.push_back(sef_person("valid", 1, 1, "r1"));
    level.persons.push_back(sef_person("absent", 2, 2, "r2"));
    level.persons.push_back(sef_person("empty_inv", 3, 3, "r3"));

    PersonValues valid;
    valid.name = "valid";
    valid.inventory = "inv012_civil";
    PersonValues absent;
    absent.name = "absent";
    absent.inventory = "inv012_civil";
    PersonValues empty_inv;
    empty_inv.name = "empty_inv";
    empty_inv.inventory = "";

    gl::assets::SaveTailEntry tail;
    tail.entities.push_back(person_entity(person_body_with_containers(
        valid, std::vector<ItemSpec>{{111, 1, 0, 0, 0, 100, 0}},
        std::vector<ItemSpec>{{222, 1, 0, 0, 0, 100, 0}}, std::nullopt)));
    tail.entities.push_back(person_entity(person_body_with_containers(
        absent, std::nullopt, std::vector<ItemSpec>{{222, 1, 0, 0, 0, 100, 0}},
        std::nullopt)));
    tail.entities.push_back(person_entity(person_body_with_containers(
        empty_inv, std::vector<ItemSpec>{{111, 1, 0, 0, 0, 100, 0}},
        std::vector<ItemSpec>{{222, 1, 0, 0, 0, 100, 0}}, std::nullopt)));

    const auto result = gl::game::restore_save_persons(level, tail, nullptr);
    REQUIRE(result.persons.size() == 3);
    CHECK(result.persons[0].loot_regenerate);
    CHECK_FALSE(result.persons[1].loot_regenerate);
    CHECK_FALSE(result.persons[2].loot_regenerate);
}
