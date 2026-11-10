#include "gl/game/npc_loot.hpp"

#include "gl/assets/inv_table.hpp"
#include "gl/assets/item_catalog.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/specials.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include "gl/game/msvc_random.hpp"
#include "gl/game/person.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using gl::assets::SpecialDatabase;
using gl::assets::SpecialMod;
using gl::game::ItemTemplate;
using gl::game::Level;
using gl::game::LootItem;
using gl::game::MsvcRandom;
using gl::game::NpcLoot;
using gl::game::NpcLootGenerator;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::uint32_t count_to_state(std::uint32_t target) {
    MsvcRandom rng(1);
    std::uint32_t draws = 0;
    while (rng.state() != target && draws <= 100000) {
        rng.next();
        ++draws;
    }
    return draws;
}

struct PersonLoot {
    std::string name;
    NpcLoot loot;
    std::uint32_t state_before = 1;
    std::uint32_t state_after = 1;
};

struct RunResult {
    std::vector<PersonLoot> persons;
    std::uint32_t final_state = 1;
};

gl::game::InventoryRuntime::ItemTemplateLookup make_lookup(const gl::assets::ItemCatalog &catalog) {
    return [&catalog](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        const gl::assets::ItemCatalogEntry *entry = catalog.find(item_id);
        if (entry == nullptr)
            return std::nullopt;
        return ItemTemplate{static_cast<std::uint32_t>(entry->item.type), entry->item.flags,
                            entry->item.unknown_b[1], entry->item.max_stack,
                            static_cast<std::uint32_t>(entry->item.effects.size())};
    };
}

RunResult run_level(const Level &level, gl::assets::GameVfs &vfs,
                    const gl::assets::ItemNameIndex &names,
                    const gl::game::InventoryRuntime::ItemTemplateLookup &lookup,
                    const SpecialDatabase *specials) {
    RunResult result;
    MsvcRandom rng(1);
    std::map<std::string, std::optional<gl::assets::InvTable>, std::less<>> cache;
    for (const gl::game::PersonInstance &person : level.persons) {
        if (person.inventory.empty() || person.name == "Hero")
            continue;
        auto cached = cache.find(person.inventory);
        if (cached == cache.end()) {
            auto table = gl::assets::load_inv(vfs, person.inventory);
            if (!table)
                cached = cache.emplace(person.inventory, std::nullopt).first;
            else
                cached = cache.emplace(person.inventory, std::move(table.value())).first;
        }
        if (!cached->second)
            continue;
        const gl::assets::InvTable &table = *cached->second;
        PersonLoot generated;
        generated.name = person.name;
        generated.state_before = rng.state();
        generated.loot = NpcLootGenerator::generate(table, 1, names, lookup, rng, specials);
        generated.state_after = rng.state();
        result.persons.push_back(std::move(generated));
    }
    result.final_state = rng.state();
    return result;
}

const PersonLoot *find_person(const RunResult &run, std::string_view name) {
    const auto found = std::find_if(run.persons.begin(), run.persons.end(),
                                    [&](const PersonLoot &person) { return person.name == name; });
    return found == run.persons.end() ? nullptr : &*found;
}

bool same_mods(const std::vector<SpecialMod> &left, const std::vector<SpecialMod> &right) {
    if (left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].id != right[index].id || left[index].value != right[index].value ||
            left[index].flags != right[index].flags)
            return false;
    }
    return true;
}

bool same_item(const LootItem &left, const LootItem &right) {
    return left.item_id == right.item_id && left.count == right.count && left.type == right.type &&
           left.flags == right.flags && left.style == right.style &&
           same_mods(left.mods, right.mods);
}

bool same_items(const std::vector<LootItem> &left, const std::vector<LootItem> &right) {
    if (left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (!same_item(left[index], right[index]))
            return false;
    }
    return true;
}

bool same_loot(const NpcLoot &left, const NpcLoot &right) {
    return same_items(left.hot, right.hot) && same_items(left.main, right.main);
}

std::int32_t count_item(const std::vector<LootItem> &items, std::uint32_t item_id) {
    std::int32_t total = 0;
    for (const LootItem &item : items) {
        if (item.item_id == item_id)
            total += item.count;
    }
    return total;
}

std::size_t count_mods(const NpcLoot &loot) {
    std::size_t total = 0;
    for (const LootItem &item : loot.hot)
        total += item.mods.size();
    for (const LootItem &item : loot.main)
        total += item.mods.size();
    return total;
}

struct ObservedEvent {
    std::string person;
    bool pass_two = false;
    std::uint32_t item_id = 0;
    std::vector<SpecialMod> mods;
};

std::vector<ObservedEvent> collect_events(const RunResult &run) {
    std::vector<ObservedEvent> events;
    for (const PersonLoot &person : run.persons) {
        for (const LootItem &item : person.loot.hot) {
            if (!item.mods.empty())
                events.push_back({person.name, false, item.item_id, item.mods});
        }
        for (const LootItem &item : person.loot.main) {
            if (!item.mods.empty())
                events.push_back({person.name, true, item.item_id, item.mods});
        }
    }
    return events;
}

std::uint32_t item_id_for(const gl::assets::ItemNameIndex &names, std::string_view tech_name) {
    return names.find(tech_name).value_or(0);
}
} // namespace

TEST_CASE("npc_loot specials: qualifying empty-pool item consumes R4 and marks 0x200") {
    gl::assets::InvTable table;
    table.regenerate_chance = 0.0;
    table.chance_decrease = 1.0;
    table.quantity_decrease = 1.0;
    gl::assets::InvItem item;
    item.tech_name = "POT";
    item.min_level = 0;
    item.max_level = 100;
    item.chance_percent = 0.0;
    item.qty_min = 1;
    item.qty_max = 1;
    table.items = {item};

    const std::vector<gl::assets::SdbEntry> entries = {{200, "POT"}};
    auto names = gl::assets::ItemNameIndex::from_entries(entries);
    REQUIRE(names.has_value());

    const auto lookup = [](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        if (item_id != 200)
            return std::nullopt;
        return ItemTemplate{20, 0x44u, 0, 10000, 0};
    };

    SpecialDatabase db;
    MsvcRandom rng(1);
    const NpcLoot loot = NpcLootGenerator::generate(table, 1, names.value(), lookup, rng, &db);

    CHECK(loot.hot.empty());
    REQUIRE(loot.main.size() == 1);
    CHECK(loot.main[0].item_id == 200);
    CHECK(loot.main[0].count == 1);
    CHECK(loot.main[0].flags == 0x244u);
    CHECK(loot.main[0].mods.empty());
    CHECK(rng.next() == 6334u);

    MsvcRandom null_rng(1);
    const NpcLoot null_loot =
        NpcLootGenerator::generate(table, 1, names.value(), lookup, null_rng, nullptr);
    REQUIRE(null_loot.main.size() == 1);
    CHECK(null_loot.main[0].flags == 0x44u);
    CHECK(null_loot.main[0].mods.empty());
    CHECK(null_rng.next() == 41u);
}

TEST_CASE("npc_loot specials: inventories keep generated mods and mod count" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    REQUIRE(names.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    auto specials = gl::assets::load_specials_scr(vfs.value());
    REQUIRE(specials.has_value());

    const auto lookup = make_lookup(catalog.value());
    gl::game::InventoryRuntime runtime =
        gl::game::InventoryRuntime::create({}, names.value(), lookup);
    MsvcRandom rng(1);
    gl::game::generate_npc_inventories(runtime, level->persons, vfs.value(), names.value(), lookup,
                                       1, rng, &specials.value());

    CHECK(runtime.count("L1_1.P389_Crying_Guardian", "MON_1_0_1") == 287);
    const std::vector<gl::assets::InventoryItem> &trade =
        runtime.trade_items("L1_2.P404_Sister_Of_Mercy2");
    const auto brl =
        std::find_if(trade.begin(), trade.end(),
                     [](const gl::assets::InventoryItem &item) { return item.item_id == 8; });
    REQUIRE(brl != trade.end());
    CHECK(brl->mod_count == 1);
    REQUIRE(brl->mods.size() == 1);
    CHECK(brl->mods[0].id == 25);
    CHECK(brl->mods[0].value == -1);
    CHECK(brl->mods[0].flags == 0x81u);
    CHECK((brl->flags & 0x200u) != 0);
}

TEST_CASE("npc_loot specials: l1_1 seed 1 loot level 1 deterministic oracle" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    REQUIRE(names.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    auto specials = gl::assets::load_specials_scr(vfs.value());
    REQUIRE(specials.has_value());

    const auto lookup = make_lookup(catalog.value());
    const RunResult with_specials =
        run_level(level.value(), vfs.value(), names.value(), lookup, &specials.value());
    const RunResult no_specials =
        run_level(level.value(), vfs.value(), names.value(), lookup, nullptr);

    CHECK(with_specials.final_state == 0x48CAA370u);
    CHECK(count_to_state(with_specials.final_state) == 1329u);
    CHECK(count_to_state(no_specials.final_state) == 1057u);
    CHECK(no_specials.final_state == 0x4037FFE0u);

    std::size_t candidates = 0;
    std::size_t triggers = 0;
    std::size_t mods = 0;
    for (const PersonLoot &person : with_specials.persons) {
        mods += count_mods(person.loot);
        for (const LootItem &item : person.loot.hot) {
            const auto item_template = lookup(item.item_id);
            REQUIRE(item_template.has_value());
            if (item.count == 1 && (item.flags & 0x4u) != 0 && item_template->mod_count == 0)
                ++candidates;
            const std::uint32_t base_flags = item_template->flags & ~0x80u;
            if ((item.flags & 0x200u) != 0 && (base_flags & 0x200u) == 0)
                ++triggers;
        }
        for (const LootItem &item : person.loot.main) {
            const auto item_template = lookup(item.item_id);
            REQUIRE(item_template.has_value());
            if (item.count == 1 && (item.flags & 0x4u) != 0 && item_template->mod_count == 0)
                ++candidates;
            if ((item.flags & 0x200u) != 0 && (item_template->flags & 0x200u) == 0)
                ++triggers;
        }
    }
    CHECK(candidates == 18u);
    CHECK(triggers == 9u);
    CHECK(mods == 12u);

    const std::vector<ObservedEvent> events = collect_events(with_specials);
    REQUIRE(events.size() == 9u);
    CHECK(collect_events(no_specials).empty());

    const std::uint32_t mon = item_id_for(names.value(), "MON_1_0_1");
    REQUIRE(mon != 0);
    struct ExpectedEvent {
        std::string_view person;
        bool pass_two;
        std::string_view tech_name;
        std::vector<SpecialMod> mods;
    };
    const ExpectedEvent expected[] = {
        {"L1_2.P404_Sister_Of_Mercy2", false, "BRL_1_01_5", {{25, -1, 0x81u}}},
        {"L0.P785_witch3", false, "BRL_1_04_4", {{16, 2, 0x41u}, {43, 2, 0x41u}}},
        {"L0.P785_witch3", false, "AML_1_03_1", {{44, 1, 0x41u}, {46, 2, 0x41u}}},
        {"L0.P785_witch3", true, "RNG_1_03_1", {{18, 2, 0x41u}}},
        {"L0.P744_oficer1", false, "HLM_1_03_1", {{20, 2, 0x41u}}},
        {"L0.P744_oficer1", true, "SWR_1_04_2", {{10, 2, 0x41u}}},
        {"L0.P744_oficer1", false, "HLM_1_01_2", {{12, 2, 0x41u}, {25, -4, 0x81u}}},
        {"L0.P744_oficer1", false, "SHL_1_01_2", {{7, 2, 0x41u}}},
        {"L0.P744_oficer1", true, "ARM_1_04_2", {{12, 1, 0x41u}}},
    };
    std::size_t r5_draws = 0;
    std::size_t r6_draws = 0;
    for (const ExpectedEvent &expected_event : expected) {
        const std::uint32_t expected_id = item_id_for(names.value(), expected_event.tech_name);
        REQUIRE(expected_id != 0);
        const auto found =
            std::find_if(events.begin(), events.end(), [&](const ObservedEvent &event) {
                return event.person == expected_event.person &&
                       event.pass_two == expected_event.pass_two && event.item_id == expected_id;
            });
        INFO(expected_event.person << " " << expected_event.tech_name);
        REQUIRE(found != events.end());
        CHECK(same_mods(found->mods, expected_event.mods));

        const auto item_template = lookup(found->item_id);
        REQUIRE(item_template.has_value());
        const auto &class_ids =
            specials->item_classes.at(static_cast<gl::assets::ItemType>(item_template->type));
        const auto pool_size = static_cast<std::size_t>(std::count_if(
            class_ids.begin(), class_ids.end(), [](gl::assets::SpecialId id) { return id < 58; }));
        REQUIRE(found->mods.size() < pool_size);
        r6_draws += pool_size - found->mods.size();

        const std::size_t limit = (item_template->flags & 0x40u) != 0 ? 9u : 3u;
        r5_draws += found->mods.size() < limit ? found->mods.size() : limit - 1;
    }

    CHECK(r5_draws == 12u);
    CHECK(candidates + r5_draws == 30u);
    CHECK(r6_draws == 233u);
    CHECK(candidates + r5_draws + r6_draws + mods == 275u);
    CHECK(count_to_state(no_specials.final_state) + 275u - 3u ==
          count_to_state(with_specials.final_state));

    for (const std::string_view name :
         {"L1_1.P387_Poison_Plotnik", "L0.P678_citizen4", "L0.P7_Kolbasens_Rodstvennik1"}) {
        INFO(name);
        const PersonLoot *left = find_person(with_specials, name);
        const PersonLoot *right = find_person(no_specials, name);
        REQUIRE(left != nullptr);
        REQUIRE(right != nullptr);
        CHECK(same_loot(left->loot, right->loot));
    }
    const PersonLoot *kolbasens = find_person(with_specials, "L0.P7_Kolbasens_Rodstvennik1");
    REQUIRE(kolbasens != nullptr);
    std::vector<std::int32_t> kolbasens_mon;
    for (const LootItem &item : kolbasens->loot.main) {
        if (item.item_id == mon)
            kolbasens_mon.push_back(item.count);
    }
    CHECK(kolbasens_mon == std::vector<std::int32_t>{87, 87});

    REQUIRE(with_specials.persons.size() == no_specials.persons.size());
    for (std::size_t index = 0; index < with_specials.persons.size(); ++index) {
        const PersonLoot &left = with_specials.persons[index];
        const PersonLoot &right = no_specials.persons[index];
        if (same_loot(left.loot, right.loot))
            continue;
        CHECK(left.name == "L1_1.P389_Crying_Guardian");
        CHECK(same_items(left.loot.hot, right.loot.hot));
        CHECK(count_item(left.loot.main, mon) == 287);
        CHECK(count_item(right.loot.main, mon) == 218);
        break;
    }

    REQUIRE(with_specials.persons.size() == no_specials.persons.size());
    for (std::size_t index = 0; index < with_specials.persons.size(); ++index) {
        const PersonLoot &left = with_specials.persons[index];
        const PersonLoot &right = no_specials.persons[index];
        if (left.state_after == right.state_after)
            continue;
        CHECK(left.name == "L1_1.P389_Crying_Guardian");
        break;
    }

    const PersonLoot *crying = find_person(with_specials, "L1_1.P389_Crying_Guardian");
    REQUIRE(crying != nullptr);
    const auto crying_person = std::find_if(level->persons.begin(), level->persons.end(),
                                            [](const gl::game::PersonInstance &person) {
                                                return person.name == "L1_1.P389_Crying_Guardian";
                                            });
    REQUIRE(crying_person != level->persons.end());
    auto crying_table = gl::assets::load_inv(vfs.value(), crying_person->inventory);
    REQUIRE(crying_table.has_value());
    crying_table->chance_decrease = 0.0;
    MsvcRandom special_rng(crying->state_before);
    const NpcLoot crying_specials = NpcLootGenerator::generate(
        crying_table.value(), 1, names.value(), lookup, special_rng, &specials.value());
    MsvcRandom null_rng(crying->state_before);
    const NpcLoot crying_none = NpcLootGenerator::generate(crying_table.value(), 1, names.value(),
                                                           lookup, null_rng, nullptr);
    CHECK(same_items(crying_specials.hot, crying_none.hot));
    MsvcRandom extra(null_rng.state());
    extra.next();
    CHECK(extra.state() == special_rng.state());

    std::size_t crying_candidates = 0;
    const LootItem *crying_candidate = nullptr;
    for (const LootItem &item : crying_specials.hot) {
        const auto item_template = lookup(item.item_id);
        REQUIRE(item_template.has_value());
        if (item.count == 1 && (item.flags & 0x4u) != 0 && item_template->mod_count == 0) {
            ++crying_candidates;
            crying_candidate = &item;
        }
    }
    CHECK(crying_candidates == 1u);
    REQUIRE(crying_candidate != nullptr);
    const std::uint32_t elb = item_id_for(names.value(), "ELB_1_04_1");
    REQUIRE(elb != 0);
    CHECK(crying_candidate->item_id == elb);
    CHECK(crying_candidate->count == 1);
    CHECK((crying_candidate->flags & 0x200u) == 0);
}
