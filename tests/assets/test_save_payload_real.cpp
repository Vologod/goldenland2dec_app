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
