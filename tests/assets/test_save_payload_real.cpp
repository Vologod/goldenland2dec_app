#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/save_payload.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
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

struct TypeHistogram {
    std::size_t by_type[4] = {0, 0, 0, 0};
};

TypeHistogram histogram(const gl::assets::SavePayload &payload) {
    TypeHistogram hist;
    for (const auto &var : payload.vars) {
        REQUIRE(var.type < 4);
        ++hist.by_type[var.type];
    }
    return hist;
}

std::span<const std::uint8_t> hero_body(const gl::assets::SavePayload &payload) {
    const gl::assets::SaveBlockView &block = payload.blocks[6];
    return std::span<const std::uint8_t>(payload.decompressed).subspan(block.offset, block.size);
}

struct Expected {
    int slot;
    const char *level;
    TypeHistogram hist;
    std::size_t tail;
    std::size_t selected;
    std::size_t rginf;
    std::size_t ents;
};

constexpr std::size_t entity_type_slot(std::uint32_t type) {
    switch (type) {
    case 2:
        return 0;
    case 8:
        return 1;
    case 16:
        return 2;
    case 32:
        return 3;
    default:
        return 4;
    }
}
} // namespace

TEST_CASE("save payload real: 28 slots decode" * doctest::skip(!game_dir_available())) {
    std::size_t decoded = 0;
    for (int slot = 0; slot < 28; ++slot) {
        CAPTURE(slot);
        const auto file = read_save(slot);
        REQUIRE(!file.empty());
        auto payload = gl::assets::parse_save_payload(file);
        REQUIRE(payload.has_value());
        CHECK(payload->blocks.size() == 7u);
        CHECK(!payload->info.level_id.empty());
        REQUIRE(payload->level_tail_index.has_value());
        CHECK(payload->blocks[0].tag == "BT_SCRN ");
        CHECK(payload->blocks[1].tag == "BT_INFO ");
        CHECK(payload->blocks[6].tag == "BT_HERO ");
        CHECK_FALSE(payload->rginf.empty());
        CHECK_FALSE(payload->ents.empty());
        ++decoded;
    }
    std::cout << "save payload real: decoded=" << decoded << "/28\n";
    CHECK(decoded == 28);
}

TEST_CASE("save payload real: slot0 and slot2 pinned values" * doctest::skip(!game_dir_available())) {
    const Expected expected[] = {
        {0, "L93_4", {{905, 94, 3, 0}}, 213, 8551, 36, 8381},
        {2, "L6_3", {{907, 94, 3, 0}}, 202, 3110, 36, 3034},
    };

    for (const Expected &slot : expected) {
        CAPTURE(slot.slot);
        auto payload = gl::assets::parse_save_payload(read_save(slot.slot));
        REQUIRE(payload.has_value());
        CHECK(payload->info.level_id == slot.level);
        CHECK(payload->vars.size() ==
              slot.hist.by_type[0] + slot.hist.by_type[1] + slot.hist.by_type[2] +
                  slot.hist.by_type[3]);
        const TypeHistogram hist = histogram(payload.value());
        for (int type = 0; type < 4; ++type) {
            CAPTURE(type);
            CHECK(hist.by_type[type] == slot.hist.by_type[type]);
        }
        CHECK(payload->tail.size() == slot.tail);
        REQUIRE(payload->level_tail_index.has_value());
        CHECK(payload->tail[*payload->level_tail_index].name == slot.level);
        CHECK(payload->tail[*payload->level_tail_index].section.size() == slot.selected);
        CHECK(payload->rginf.size() == slot.rginf);
        CHECK(payload->ents.size() == slot.ents);
    }
}

TEST_CASE("save payload real: slot0 hero counts" * doctest::skip(!game_dir_available())) {
    auto payload = gl::assets::parse_save_payload(read_save(0));
    REQUIRE(payload.has_value());
    auto inventory = gl::assets::parse_hero_inventory(hero_body(payload.value()));
    REQUIRE(inventory.has_value());
    CHECK(inventory->hot.size() == 8);
    CHECK(inventory->items.size() == 117);
    CHECK(inventory->equipped.size() == 11);
}

TEST_CASE("save payload real: save entities corpus" * doctest::skip(!game_dir_available())) {
    std::array<std::size_t, 5> by_type{};
    std::size_t saves = 0;
    std::size_t sections = 0;
    std::size_t pname_names = 0;
    std::size_t person_entities = 0;
    std::size_t pname_eq_person = 0;
    std::size_t latch_zero = 0;
    std::size_t latch_one = 0;

    for (int slot = 0; slot < 28; ++slot) {
        CAPTURE(slot);
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        ++saves;
        for (const auto &entry : payload->tail) {
            CAPTURE(entry.name);
            ++sections;
            CHECK_FALSE(entry.entities.empty());

            std::size_t persons = 0;
            for (const auto &entity : entry.entities) {
                CAPTURE(entity.type);
                const std::size_t index = entity_type_slot(entity.type);
                CHECK(index < 4);
                CHECK(entity.type != 4);
                CHECK_FALSE(entity.body.empty());
                if (index < by_type.size())
                    ++by_type[index];
                if (entity.type == 2)
                    ++persons;
            }
            pname_names += entry.person_names.size();
            person_entities += persons;
            if (entry.person_names.size() == persons)
                ++pname_eq_person;

            const auto &dt = entry.rginf.datetime;
            CHECK(dt[0] >= 1124);
            CHECK(dt[0] <= 1126);
            CHECK(dt[1] >= 1);
            CHECK(dt[1] <= 12);
            CHECK(dt[2] >= 1);
            CHECK(dt[2] <= 31);
            CHECK(dt[3] <= 6);
            CHECK(dt[4] <= 23);
            CHECK(dt[5] <= 59);
            CHECK(dt[6] <= 59);
            CHECK(dt[7] <= 999);
            if (entry.rginf.latch == 0)
                ++latch_zero;
            else if (entry.rginf.latch == 1)
                ++latch_one;
            CHECK(entry.rginf.latch <= 1);
        }
    }

    CHECK(saves == 28);
    CHECK(sections == 2599);
    CHECK(by_type[entity_type_slot(2)] == 14507);
    CHECK(by_type[entity_type_slot(8)] == 354);
    CHECK(by_type[entity_type_slot(16)] == 12090);
    CHECK(by_type[entity_type_slot(32)] == 2591);
    CHECK(by_type[4] == 0);
    CHECK(person_entities == 14507);
    CHECK(pname_names == 14507);
    CHECK(pname_eq_person == 2599);
    CHECK(latch_zero == 1920);
    CHECK(latch_one == 679);
}

TEST_CASE("save payload real: save entities selected pins" * doctest::skip(!game_dir_available())) {
    struct PinnedSection {
        int slot;
        const char *level;
        std::size_t section;
        std::size_t pnames;
        std::size_t rginf;
        std::size_t ents;
        std::array<std::size_t, 4> types;
    };
    const PinnedSection expected[] = {
        {0, "L93_4", 8551, 3, 36, 8381, {3, 1, 0, 0}},
        {2, "L6_3", 3110, 0, 36, 3034, {0, 0, 11, 0}},
    };

    for (const PinnedSection &slot : expected) {
        CAPTURE(slot.slot);
        auto payload = gl::assets::parse_save_payload(read_save(slot.slot));
        REQUIRE(payload.has_value());
        REQUIRE(payload->level_tail_index.has_value());
        CHECK(payload->info.level_id == slot.level);
        const gl::assets::SaveTailEntry &entry = payload->tail[*payload->level_tail_index];
        CHECK(entry.name == slot.level);
        CHECK(entry.section.size() == slot.section);
        CHECK(entry.person_names.size() == slot.pnames);
        CHECK(payload->rginf.size() == slot.rginf);
        CHECK(payload->ents.size() == slot.ents);
        std::array<std::size_t, 4> by_type{};
        for (const auto &entity : entry.entities) {
            const std::size_t index = entity_type_slot(entity.type);
            REQUIRE(index < by_type.size());
            ++by_type[index];
        }
        for (std::size_t index = 0; index < by_type.size(); ++index) {
            CAPTURE(index);
            CHECK(by_type[index] == slot.types[index]);
        }
    }
}

TEST_CASE("save payload real: decode is deterministic" * doctest::skip(!game_dir_available())) {
    const auto file = read_save(0);
    auto first = gl::assets::parse_save_payload(file);
    auto second = gl::assets::parse_save_payload(file);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(first->decompressed == second->decompressed);
    CHECK(first->info.level_id == second->info.level_id);
    CHECK(first->vars.size() == second->vars.size());
    CHECK(first->tail.size() == second->tail.size());
    CHECK(first->rginf == second->rginf);
    CHECK(first->ents == second->ents);
}
