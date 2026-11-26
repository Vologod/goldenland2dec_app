#include "gl/game/tail_write.hpp"

#include "gl/assets/save_item_convert.hpp"
#include "gl/game/effect.hpp"
#include "gl/game/person_movement.hpp"
#include "gl/game/save_person.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace gl::game {
namespace {
constexpr std::uint8_t kAliveFlag = 1;

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

std::optional<std::int32_t> route_type_number(std::string_view type) {
    if (type == "STAY")
        return 0;
    if (type == "STAY_ROTATE")
        return 1;
    if (type == "MOVED")
        return 2;
    if (type == "MOVED_FLIP")
        return 3;
    if (type == "RANDOM_RADIUS")
        return 4;
    if (type == "RANDOM")
        return 5;
    return std::nullopt;
}

Result<gl::assets::SaveEntity> person_entity(SavePersonRecord record) {
    auto encoded = encode_save_person_body(record);
    if (!encoded)
        return encoded.error();
    return gl::assets::SaveEntity{2, std::move(encoded.value())};
}

std::vector<gl::assets::SaveItemRecord>
live_items(const InventoryRuntime &inventory, std::string_view person, bool trade) {
    const auto &items = trade ? inventory.trade_items(person) : inventory.person_items(person);
    std::vector<gl::assets::SaveItemRecord> records;
    records.reserve(items.size());
    for (const gl::assets::InventoryItem &item : items)
        records.push_back(gl::assets::save_item_from_inventory(item));
    return records;
}

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

void apply_live_containers(SavePersonRecord &record, const InventoryRuntime &inventory,
                           std::string_view person) {
    const auto &items = inventory.person_items(person);
    const auto &trade = inventory.trade_items(person);
    if (record.containers[0].flag != 0 || !items.empty()) {
        record.containers[0].flag = 1;
        record.containers[0].present = true;
        record.containers[0].items = live_items(inventory, person, false);
        record.containers[0].raw_items.clear();
    }
    if (record.containers[1].flag != 0 || !trade.empty()) {
        record.containers[1].flag = 1;
        record.containers[1].present = true;
        record.containers[1].items = live_items(inventory, person, true);
        record.containers[1].raw_items.clear();
    }
}

Result<SavePersonRecord> make_new_person(const PersonInstance &person,
                                         const PersonMovement &movement,
                                         const InventoryRuntime &inventory,
                                         const gl::assets::SaveTribeTable *tribes,
                                         bool allow_live_containers) {
    const auto route_type = route_type_number(person.route_type);
    if (!route_type)
        return format_error("tail write: unknown route type '" + person.route_type + "'");
    SavePersonRecord record;
    record.name = person.name;
    const Tile tile = tile_at(movement.x, movement.y);
    record.current_tile_x = tile.x;
    record.current_tile_y = tile.y;
    record.anchor_tile_x = person.tile_x;
    record.anchor_tile_y = person.tile_y;
    record.direction = movement.direction8;
    record.route_type = *route_type;
    record.route_name = person.route;
    record.radius = person.radius;
    record.delay_min = person.delay_min;
    record.delay_max = person.delay_max;
    record.dialog = person.dialog;
    record.inventory = person.inventory;
    record.field_6cc = kAliveFlag;
    record.hp = person.saved_life.value_or(0);
    record.energy = person.saved_energy.value_or(0);
    if (!person.tribe.empty()) {
        if (tribes == nullptr)
            return format_error("tail write: missing tribe table for '" + person.tribe + "'");
        const auto *entry = tribes->find(person.tribe);
        if (entry == nullptr)
            return format_error("tail write: unknown tribe '" + person.tribe + "'");
        record.tribe_id = static_cast<std::int32_t>(entry->id);
    }
    if (allow_live_containers)
        apply_live_containers(record, inventory, person.name);
    return record;
}
} // namespace

Result<gl::assets::SaveTailEntry> build_selected_tail(
    const gl::assets::SaveTailEntry &loaded, const Level &level,
    const LevelSimulation &simulation, const InventoryRuntime &inventory,
    const RestoreResult &restore, std::span<const TailPersonLive> live,
    std::span<const std::size_t> new_persons, const gl::assets::SaveRginf &rginf,
    const gl::assets::SaveTribeTable *tribes) {
    if (restore.persons.size() != live.size())
        return format_error("tail write: live flags do not match restored persons");
    if (level.persons.size() != simulation.persons().size())
        return format_error("tail write: level and simulation person counts differ");

    std::map<std::string, std::size_t, std::less<>> name_counts;
    for (const PersonInstance &person : level.persons)
        ++name_counts[person.name];

    gl::assets::SaveTailEntry out;
    out.name = loaded.name;
    out.rginf.latch = rginf.latch;
    out.rginf.datetime = rginf.datetime;

    const std::size_t no_entity = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> restored_by_entity(loaded.entities.size(), no_entity);
    for (std::size_t index = 0; index < restore.persons.size(); ++index) {
        const std::size_t entity_index = restore.persons[index].entity_index;
        if (entity_index >= restored_by_entity.size())
            return format_error("tail write: restored entity index is out of range");
        if (restored_by_entity[entity_index] != no_entity)
            return format_error("tail write: duplicate restored entity index");
        restored_by_entity[entity_index] = index;
    }

    std::vector<std::string> names;
    std::vector<char> appended(level.persons.size(), 0);
    for (std::size_t entity_index = 0; entity_index < loaded.entities.size(); ++entity_index) {
        const gl::assets::SaveEntity &entity = loaded.entities[entity_index];
        if (entity.type != 2) {
            out.entities.push_back(entity);
            continue;
        }
        const std::size_t restore_index = restored_by_entity[entity_index];
        if (restore_index == no_entity) {
            auto decoded = decode_save_person(entity);
            if (!decoded)
                return decoded.error();
            names.push_back(decoded->name);
            out.entities.push_back(entity);
            continue;
        }
        const std::size_t level_index = restore.persons[restore_index].level_index;
        if (level_index >= level.persons.size())
            return format_error("tail write: restored level index is out of range");
        appended[level_index] = 1;
        if (!live[restore_index].alive)
            continue;
        auto decoded = decode_save_person(entity);
        if (!decoded)
            return decoded.error();
        SavePersonRecord record = std::move(decoded.value());
        const PersonMovement &movement = simulation.persons()[level_index];
        const Tile tile = tile_at(movement.x, movement.y);
        record.current_tile_x = tile.x;
        record.current_tile_y = tile.y;
        record.direction = movement.direction8;
        record.route_index = restore.persons[restore_index].route_index;
        if (live[restore_index].hp)
            record.hp = *live[restore_index].hp;
        if (live[restore_index].energy)
            record.energy = *live[restore_index].energy;
        if (live[restore_index].ap)
            record.ap = *live[restore_index].ap;
        if (live[restore_index].effects) {
            record.extra.clear();
            record.extra.reserve(live[restore_index].effects->size());
            for (const PersonEffect &effect : *live[restore_index].effects)
                record.extra.push_back(pack_effect(effect));
            record.tail_count = static_cast<std::uint32_t>(record.extra.size());
        }
        if (live[restore_index].effect_clock) {
            const auto bytes = live[restore_index].effect_clock->to_time_bytes();
            std::copy(bytes.begin(), bytes.end(), record.tail80.begin() + 0x40);
        }
        if (name_counts[level.persons[level_index].name] == 1)
            apply_live_containers(record, inventory, level.persons[level_index].name);
        names.push_back(record.name);
        auto entity_out = person_entity(std::move(record));
        if (!entity_out)
            return entity_out.error();
        out.entities.push_back(std::move(entity_out.value()));
    }

    for (const std::size_t level_index : new_persons) {
        if (level_index >= level.persons.size())
            return format_error("tail write: new person index is out of range");
        if (appended[level_index])
            return format_error("tail write: new person is already in the save");
        const PersonInstance &person = level.persons[level_index];
        auto record = make_new_person(person, simulation.persons()[level_index], inventory, tribes,
                                      name_counts[person.name] == 1);
        if (!record)
            return record.error();
        names.push_back(record->name);
        auto entity_out = person_entity(std::move(record.value()));
        if (!entity_out)
            return entity_out.error();
        out.entities.push_back(std::move(entity_out.value()));
    }

    auto section = gl::assets::encode_tail_section(out.rginf, names, out.entities);
    if (!section)
        return section.error();
    out.person_names = std::move(names);
    out.section = std::move(section.value());
    out.raw_len = static_cast<std::uint32_t>(out.section.size());
    return out;
}
} // namespace gl::game
