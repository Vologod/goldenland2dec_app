#include "gl/assets/hero_stats.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/hero_save_write.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/game/live_save.hpp"
#include "gl/game/save_person.hpp"
#include "gl/game/save_restore.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::vector<std::uint8_t> read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

std::vector<std::uint8_t> block_body(const gl::assets::SavePayload &payload,
                                     std::size_t index) {
    const auto &block = payload.blocks[index];
    return std::vector<std::uint8_t>(
        payload.decompressed.begin() + static_cast<std::ptrdiff_t>(block.offset),
        payload.decompressed.begin() +
            static_cast<std::ptrdiff_t>(block.offset + block.size));
}

gl::game::HeroLiveState make_hero_state(const gl::assets::HeroSaveBody &body,
                                        const gl::assets::HeroInventory &inventory) {
    gl::game::HeroLiveState hero;
    hero.tile_x = body.tile_x;
    hero.tile_y = body.tile_y;
    hero.direction = body.direction;
    hero.hp = body.hp;
    hero.ep = body.ep;
    hero.ap = body.ap;
    hero.containers[0] = inventory.hot;
    hero.containers[1] = inventory.items;
    hero.containers[2] = inventory.equipped;
    for (std::size_t index = 0; index < 3; ++index)
        hero.present[index] = body.containers[index].flag != 0;
    return hero;
}

std::vector<std::string> type2_names(const gl::assets::SaveTailEntry &entry) {
    std::vector<std::string> names;
    for (const auto &entity : entry.entities) {
        if (entity.type != 2)
            continue;
        const auto decoded = gl::game::decode_save_person(entity);
        REQUIRE(decoded.has_value());
        names.push_back(decoded->name);
    }
    return names;
}
} // namespace

TEST_CASE("live save applies HERO, tail and VARS from real slot0" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto payload = gl::assets::parse_save_payload(read_save(0));
    REQUIRE(payload.has_value());
    REQUIRE(payload->level_tail_index.has_value());
    const gl::assets::SaveTailEntry loaded_tail =
        payload->tail[*payload->level_tail_index];
    const auto hero_body = gl::assets::parse_hero_save_body(block_body(payload.value(), 6));
    REQUIRE(hero_body.has_value());
    const auto hero_inventory = gl::assets::parse_hero_inventory(block_body(payload.value(), 6));
    REQUIRE(hero_inventory.has_value());

    auto level = gl::game::load_level(vfs.value(), payload->info.level_id);
    REQUIRE(level.has_value());
    auto inventory = gl::game::InventoryRuntime::create({}, {}, {});
    const auto restore = gl::game::restore_save_persons(
        level.value(), loaded_tail, &payload->tribes, false, &inventory);
    auto simulation = gl::game::LevelSimulation::create(level.value(), 1);
    for (const auto &person : restore.persons)
        simulation.restore_person(person.level_index, person.current, person.direction,
                                  person.route_index);
    const auto hero_stats = gl::assets::parse_hero_stats(block_body(payload.value(), 6));
    REQUIRE(hero_stats.has_value());
    auto person_runtime = gl::game::PersonRuntime::create(
        level->persons, std::optional<gl::assets::HeroStats>(hero_stats.value()));

    gl::game::HeroLiveState hero = make_hero_state(hero_body.value(), hero_inventory.value());
    gl::game::LiveSaveInput input;
    input.level = &level.value();
    input.simulation = &simulation;
    input.inventory = &inventory;
    input.restore = &restore;
    input.persons = &person_runtime;
    input.hero = &hero;
    input.has_rginf_latch = true;
    input.rginf_latch = 7;
    input.vars = payload->vars;

    auto unchanged = gl::game::apply_live_save(payload.value(), input);
    REQUIRE(unchanged.has_value());
    const auto reparsed = gl::assets::parse_save_payload(
        gl::assets::encode_save_payload(unchanged.value()).value());
    REQUIRE(reparsed.has_value());
    REQUIRE(reparsed->level_tail_index.has_value());
    const auto saved_hero = gl::assets::parse_hero_save_body(block_body(reparsed.value(), 6));
    REQUIRE(saved_hero.has_value());
    CHECK(saved_hero->tile_x == hero_body->tile_x);
    CHECK(saved_hero->tile_y == hero_body->tile_y);
    CHECK(saved_hero->direction == hero_body->direction);
    CHECK(saved_hero->containers[0].items.size() == hero_body->containers[0].items.size());
    CHECK(saved_hero->containers[1].items.size() == hero_body->containers[1].items.size());
    CHECK(saved_hero->containers[2].items.size() == hero_body->containers[2].items.size());
    CHECK(reparsed->tail[*reparsed->level_tail_index].rginf.latch == 7);
    CHECK(reparsed->tail[*reparsed->level_tail_index].rginf.datetime ==
          loaded_tail.rginf.datetime);
    CHECK(type2_names(reparsed->tail[*reparsed->level_tail_index]) == type2_names(loaded_tail));

    std::map<std::string, std::size_t, std::less<>> name_counts;
    for (const auto &person : level->persons)
        ++name_counts[person.name];
    std::optional<std::size_t> unique_restore;
    for (std::size_t index = 0; index < restore.persons.size(); ++index) {
        if (name_counts[level->persons[restore.persons[index].level_index].name] == 1) {
            unique_restore = index;
            break;
        }
    }

    if (unique_restore.has_value()) {
        const auto &restored = restore.persons[*unique_restore];
        const std::string &name = level->persons[restored.level_index].name;
        person_runtime.set_parameter(name, gl::script::PersonParameter::life, 7.0);
        person_runtime.set_parameter(name, gl::script::PersonParameter::energy, 8.0);
        simulation.restore_person(restored.level_index, {12, 13}, 6, restored.route_index);
        hero.tile_x = 5;
        hero.tile_y = 6;
        hero.direction = 2;
        hero.containers[0].clear();
        input.rginf_latch = 9;
        auto moved = gl::game::apply_live_save(payload.value(), input);
        REQUIRE(moved.has_value());
        const auto moved_payload = gl::assets::parse_save_payload(
            gl::assets::encode_save_payload(moved.value()).value());
        REQUIRE(moved_payload.has_value());
        REQUIRE(moved_payload->level_tail_index.has_value());
        const auto &moved_tail = moved_payload->tail[*moved_payload->level_tail_index];
        CHECK(moved_tail.rginf.latch == 9);
        REQUIRE(restored.entity_index < moved_tail.entities.size());
        const auto person =
            gl::game::decode_save_person(moved_tail.entities[restored.entity_index]);
        REQUIRE(person.has_value());
        CHECK(person->name == name);
        CHECK(person->current_tile_x == 12);
        CHECK(person->current_tile_y == 13);
        CHECK(person->direction == 6);
        CHECK(person->hp == 7);
        CHECK(person->energy == 8);

        const auto moved_hero = gl::assets::parse_hero_save_body(block_body(moved_payload.value(), 6));
        REQUIRE(moved_hero.has_value());
        CHECK(moved_hero->tile_x == 5);
        CHECK(moved_hero->tile_y == 6);
        CHECK(moved_hero->direction == 2);
        CHECK(moved_hero->containers[0].items.empty());

        person_runtime.set_parameter(name, gl::script::PersonParameter::life, 0.0);
        auto dead = gl::game::apply_live_save(payload.value(), input);
        REQUIRE(dead.has_value());
        const auto dead_payload = gl::assets::parse_save_payload(
            gl::assets::encode_save_payload(dead.value()).value());
        REQUIRE(dead_payload.has_value());
        REQUIRE(dead_payload->level_tail_index.has_value());
        const auto dead_names =
            type2_names(dead_payload->tail[*dead_payload->level_tail_index]);
        CHECK(std::find(dead_names.begin(), dead_names.end(), name) == dead_names.end());
        CHECK(std::find(dead_payload->tail[*dead_payload->level_tail_index].person_names.begin(),
                        dead_payload->tail[*dead_payload->level_tail_index].person_names.end(),
                        name) ==
              dead_payload->tail[*dead_payload->level_tail_index].person_names.end());
    }

    if (!payload->vars.empty()) {
        auto changed = payload.value();
        auto var = gl::assets::save_var_number(changed.vars.front());
        REQUIRE(var.has_value());
        const double value = *var + 1.0;
        std::memcpy(changed.vars.front().value.data(), &value, sizeof(value));
        input.vars = changed.vars;
        auto vars_saved = gl::game::apply_live_save(payload.value(), input);
        REQUIRE(vars_saved.has_value());
        const auto vars_encoded = gl::assets::encode_save_payload(vars_saved.value());
        REQUIRE(vars_encoded.has_value());
        const auto vars_parsed = gl::assets::parse_save_payload(vars_encoded.value());
        REQUIRE(vars_parsed.has_value());
        CHECK(gl::assets::save_var_number(vars_parsed->vars.front()) ==
              doctest::Approx(value));
        REQUIRE(vars_parsed->vars.size() == changed.vars.size());
        for (std::size_t index = 0; index < changed.vars.size(); ++index)
            CHECK(vars_parsed->vars[index].name == changed.vars[index].name);
    }

    auto text_vars = payload->vars;
    gl::assets::SaveVar text;
    text.type = 3;
    text.name = "test_text";
    text.text_value = "abc";
    text_vars.push_back(text);
    input.vars = text_vars;
    auto text_saved = gl::game::apply_live_save(payload.value(), input);
    REQUIRE(text_saved.has_value());
    const auto text_encoded = gl::assets::encode_save_payload(text_saved.value());
    REQUIRE(text_encoded.has_value());
    const auto text_parsed = gl::assets::parse_save_payload(text_encoded.value());
    REQUIRE(text_parsed.has_value());
    auto found_text = std::find_if(text_parsed->vars.begin(), text_parsed->vars.end(),
                                   [](const auto &var) { return var.name == "test_text"; });
    REQUIRE(found_text != text_parsed->vars.end());
    CHECK(found_text->type == 3);
    CHECK(found_text->text_value == "abc");
}

TEST_CASE("live save rejects a malformed HERO body without mutating the payload" *
          doctest::skip(!game_dir_available())) {
    auto payload = gl::assets::parse_save_payload(read_save(0));
    REQUIRE(payload.has_value());
    const auto &block = payload->blocks[6];
    payload->decompressed[block.offset + 0x734] = 2;
    const auto before = payload->decompressed;
    gl::game::LiveSaveInput input;
    input.vars = payload->vars;
    const gl::game::HeroLiveState hero;
    input.hero = &hero;
    const auto applied = gl::game::apply_live_save(payload.value(), input);
    REQUIRE_FALSE(applied.has_value());
    CHECK(applied.error().code == gl::ErrorCode::format);
    CHECK(payload->decompressed == before);
}
