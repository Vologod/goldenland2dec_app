#include "gl/game/save_restore.hpp"

#include "gl/assets/log.hpp"
#include "gl/assets/save_item_convert.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/save_person.hpp"

#include <array>
#include <cctype>
#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gl::game {
namespace {
const char *route_type_name(std::int32_t route_type) {
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
        return "STAY";
    }
}

bool same_route_name(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size())
        return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const auto left = static_cast<unsigned char>(lhs[i]);
        const auto right = static_cast<unsigned char>(rhs[i]);
        if (std::tolower(left) != std::tolower(right))
            return false;
    }
    return true;
}

const gl::assets::SaveTribeEntry *find_tribe(const gl::assets::SaveTribeTable &tribes,
                                             std::int32_t id) {
    const auto wanted = static_cast<std::uint32_t>(id);
    for (const gl::assets::SaveTribeEntry &entry : tribes.entries) {
        if (entry.id == wanted)
            return &entry;
    }
    return nullptr;
}

std::vector<gl::assets::InventoryItem> to_inventory_items(const SaveContainer &container) {
    std::vector<gl::assets::InventoryItem> items;
    items.reserve(container.items.size());
    for (const gl::assets::SaveItemRecord &record : container.items)
        items.push_back(gl::assets::inventory_item_from_save(record));
    return items;
}

void apply_record(PersonInstance &person, const SavePersonRecord &record,
                  const gl::assets::SaveTribeTable *tribes, bool &warned_missing_table) {
    person.tile_x = record.anchor_tile_x;
    person.tile_y = record.anchor_tile_y;
    person.radius = record.radius;
    person.delay_min = record.delay_min;
    person.delay_max = record.delay_max;
    if (record.route_type < 0 || record.route_type > 5) {
        gl::log(gl::LogLevel::warning, "save restore: person '" + person.name +
                                           "' has unknown route type " +
                                           std::to_string(record.route_type) + "; using STAY");
    }
    person.route_type = route_type_name(record.route_type);
    person.direction = record.direction;
    person.dialog = record.dialog;
    person.inventory = record.inventory;
    person.saved_life = record.hp;
    person.saved_energy = record.energy;

    if (tribes != nullptr) {
        const gl::assets::SaveTribeEntry *tribe = find_tribe(*tribes, record.tribe_id);
        if (tribe != nullptr) {
            person.tribe = tribe->name;
        } else {
            gl::log(gl::LogLevel::warning, "save restore: person '" + person.name + "' tribe id " +
                                               std::to_string(record.tribe_id) +
                                               " is missing; keeping SEF tribe '" + person.tribe +
                                               "'");
        }
    } else if (!warned_missing_table) {
        warned_missing_table = true;
        gl::log(gl::LogLevel::warning,
                "save restore: tribe table is missing; keeping SEF tribe names");
    }

    if (!same_route_name(person.route, record.route_name)) {
        gl::log(gl::LogLevel::warning, "save restore: person '" + person.name + "' route '" +
                                           record.route_name + "' differs from SEF route '" +
                                           person.route + "'; keeping SEF route cells");
    }
    person.route = record.route_name;
}
} // namespace

RestoreResult restore_save_persons(Level &level, const gl::assets::SaveTailEntry &tail,
                                   const gl::assets::SaveTribeTable *tribes, bool drop_sef_only,
                                   InventoryRuntime *inventory) {
    std::map<std::string, std::deque<std::size_t>, std::less<>> occurrences;
    for (std::size_t index = 0; index < level.persons.size(); ++index)
        occurrences[level.persons[index].name].push_back(index);

    std::vector<char> matched(level.persons.size(), 0);
    std::vector<std::size_t> matched_order;
    matched_order.reserve(level.persons.size());
    std::map<std::string, std::size_t, std::less<>> applied_names;
    bool warned_missing_table = false;

    RestoreResult result;
    for (const gl::assets::SaveEntity &entity : tail.entities) {
        if (entity.type != 2) {
            ++result.counts.skipped;
            continue;
        }
        const auto decoded = decode_save_person(entity);
        if (!decoded) {
            ++result.counts.skipped;
            gl::log(gl::LogLevel::warning,
                    "save restore: cannot decode saved person: " + decoded.error().message);
            continue;
        }
        auto &queue = occurrences[decoded->name];
        if (queue.empty()) {
            ++result.counts.type2_only;
            gl::log(gl::LogLevel::warning, "save restore: no SEF person for saved '" +
                                               decoded->name + "' (type-2-only, skipped)");
            continue;
        }
        const std::size_t index = queue.front();
        queue.pop_front();
        apply_record(level.persons[index], decoded.value(), tribes, warned_missing_table);
        if (inventory != nullptr) {
            const std::string &name = level.persons[index].name;
            const std::size_t occurrence = ++applied_names[name];
            if (occurrence > 1) {
                ++result.counts.duplicate_containers;
                gl::log(gl::LogLevel::warning, "save restore: duplicate containers for '" + name +
                                                   "' occurrence " + std::to_string(occurrence) +
                                                   "; last record wins");
            }
            inventory->replace_person_items(name, to_inventory_items(decoded->containers[0]));
            inventory->replace_person_trade_items(name, to_inventory_items(decoded->containers[1]));
            ++result.counts.containers_applied;
        }
        result.counts.c2_items += decoded->containers[2].items.size();
        result.persons.push_back(RestoredPerson{
            matched_order.size(),
            Tile{decoded->current_tile_x, decoded->current_tile_y},
            decoded->direction,
            decoded->route_index,
            decoded->containers[0].present && decoded->containers[1].present &&
                !decoded->inventory.empty(),
        });
        matched[index] = 1;
        matched_order.push_back(index);
        ++result.counts.matched;
    }

    std::vector<PersonInstance> restored;
    restored.reserve(matched_order.size() +
                     (drop_sef_only ? 0 : level.persons.size() - matched_order.size()));
    for (std::size_t index : matched_order)
        restored.push_back(std::move(level.persons[index]));
    for (std::size_t index = 0; index < level.persons.size(); ++index) {
        if (matched[index])
            continue;
        ++result.counts.sef_only;
        if (!drop_sef_only)
            restored.push_back(std::move(level.persons[index]));
    }
    level.persons = std::move(restored);
    return result;
}
} // namespace gl::game
