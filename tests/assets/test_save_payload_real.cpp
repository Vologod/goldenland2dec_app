#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/game/save_person.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <span>
#include <string>
#include <utility>
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

struct SlotPin {
    int slot;
    std::array<std::size_t, 5> histogram;
    std::array<std::uint8_t, 16> row;
};

constexpr std::array<SlotPin, 3> pins{{
    {0, {103, 232, 54186, 234, 1},
     {3, 2, 2, 2, 2, 2, 2, 0, 2, 0, 0, 2, 0, 0, 2, 0}},
    {2, {82, 232, 54207, 234, 1},
     {3, 2, 2, 2, 2, 2, 2, 0, 2, 0, 2, 2, 0, 0, 2, 0}},
    {21, {22, 3, 54497, 234, 0},
     {3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}},
}};

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

TEST_CASE("real data: tribe relation matrix" * doctest::skip(!game_dir_available())) {
    std::size_t total_values = 0;
    std::array<std::size_t, 5> histogram{};
    std::size_t relation_mismatches = 0;
    std::size_t duplicate_ids = 0;

    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        const auto &table = payload->tribes;
        CHECK(table.count() == 234u);
        REQUIRE(table.matrix.size() == 54756u);

        std::set<std::uint32_t> ids;
        for (const auto &entry : table.entries) {
            if (!ids.insert(entry.id).second)
                ++duplicate_ids;
        }
        for (std::size_t i = 0; i < table.count(); ++i) {
            CHECK(table.relation(table.entries[i].id, table.entries[i].id) == 3);
            for (std::size_t j = 0; j < table.count(); ++j) {
                if (table.relation(table.entries[i].id, table.entries[j].id) !=
                    table.matrix[i * table.count() + j])
                    ++relation_mismatches;
            }
        }
        CHECK(table.relation(0xFFFFFFFFu, table.entries[0].id) == 2);
        CHECK(table.relation(table.entries[0].id, 0xFFFFFFFFu) == 2);
        for (const SlotPin &pin : pins) {
            if (pin.slot != slot)
                continue;
            for (std::size_t value = 0; value < pin.histogram.size(); ++value) {
                std::size_t count = 0;
                for (std::uint8_t byte : table.matrix)
                    if (byte == value)
                        ++count;
                CHECK(count == pin.histogram[value]);
            }
            REQUIRE(table.row(0).size() == 234u);
            for (std::size_t index = 0; index < pin.row.size(); ++index)
                CHECK(table.row(0)[index] == pin.row[index]);
        }
        for (std::uint8_t byte : table.matrix) {
            CHECK(byte <= 4);
            if (byte < histogram.size())
                ++histogram[byte];
            ++total_values;
        }
        if (slot == 0) {
            const std::vector<std::pair<std::uint32_t, std::string>> first{
                {106, "L12_3_citizen"},
                {16, "snake"},
                {97, "L11_3_citizen"},
                {105, "L12_2_citizen"},
                {96, "L11_2_citizen"},
                {39, "citizen"},
            };
            REQUIRE(table.entries.size() >= first.size());
            for (std::size_t index = 0; index < first.size(); ++index) {
                CHECK(table.entries[index].id == first[index].first);
                CHECK(table.entries[index].name == first[index].second);
            }
        }
    }
    CHECK(duplicate_ids == 0);
    CHECK(relation_mismatches == 0);
    CHECK(total_values == 1533168u);
    CHECK(histogram[0] == 1257u);
    CHECK(histogram[1] == 6096u);
    CHECK(histogram[2] == 1519260u);
    CHECK(histogram[3] == 6552u);
    CHECK(histogram[4] == 3u);
}

TEST_CASE("real data: diary framing" * doctest::skip(!game_dir_available())) {
    std::size_t entries = 0;
    std::size_t blocks = 0;
    std::size_t records = 0;
    std::size_t sparse = 0;
    std::size_t count_mismatches = 0;
    std::array<std::size_t, 28> per_slot_blocks{};
    std::array<std::size_t, 28> per_slot_entries{};

    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        entries += payload->diary.size();
        sparse += payload->diary_sparse.size();
        for (const auto &entry : payload->diary) {
            blocks += entry.blocks.size();
            records += entry.blocks.empty() ? 0 : entry.blocks.size() - 1;
            per_slot_blocks[slot] += entry.blocks.size();
            ++per_slot_entries[slot];
            if (entry.declared_count != entry.blocks.size() - 1)
                ++count_mismatches;
        }
    }
    CHECK(entries == 1013u);
    CHECK(blocks == 4855u);
    CHECK(records == 3842u);
    CHECK(sparse == 0u);
    CHECK(count_mismatches == 0u);
    CHECK(per_slot_blocks[0] == 371u);
    CHECK(per_slot_blocks[2] == 377u);
    CHECK(per_slot_blocks[21] == 2u);
    CHECK(per_slot_entries[0] == 76u);
    CHECK(per_slot_entries[2] == 75u);
    CHECK(per_slot_entries[21] == 1u);

    auto slot0 = gl::assets::parse_save_payload(read_save(0));
    REQUIRE(slot0.has_value());
    REQUIRE(slot0->diary.size() == 76u);
    CHECK(slot0->diary[0].key == 0u);
    CHECK(slot0->diary[0].declared_count == 4u);
    CHECK(slot0->diary[0].blocks.size() == 5u);

    auto slot21 = gl::assets::parse_save_payload(read_save(21));
    REQUIRE(slot21.has_value());
    REQUIRE(slot21->diary.size() == 1u);
    CHECK(slot21->diary[0].key == 482u);
    CHECK(slot21->diary[0].declared_count == 1u);
    CHECK(slot21->diary[0].blocks.size() == 2u);
}

TEST_CASE("real data: history framing" * doctest::skip(!game_dir_available())) {
    std::size_t ids_zero = 0;
    std::size_t ids_total = 0;
    std::size_t tail_one = 0;
    std::size_t tail_two = 0;
    std::uint32_t head_min = 1000;
    std::uint32_t head_max = 0;

    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        CHECK(payload->history.slots.size() == 40u);
        head_min = std::min(head_min, payload->history.head);
        head_max = std::max(head_max, payload->history.head);
        if (payload->history.tail == 1)
            ++tail_one;
        if (payload->history.tail == 2)
            ++tail_two;
        for (const auto &slot_value : payload->history.slots) {
            ++ids_total;
            if (slot_value.id == 0)
                ++ids_zero;
        }
    }
    CHECK(ids_total == 1120u);
    CHECK(ids_zero == 1120u);
    CHECK(tail_one == 26u);
    CHECK(tail_two == 2u);
    CHECK(head_min == 1u);
    CHECK(head_max == 37u);

    auto slot0 = gl::assets::parse_save_payload(read_save(0));
    REQUIRE(slot0.has_value());
    CHECK(slot0->history.head == 5u);
    CHECK(slot0->history.tail == 1u);
    std::size_t non_empty = 0;
    for (const auto &slot_value : slot0->history.slots)
        if (!slot_value.text.empty())
            ++non_empty;
    CHECK(non_empty == 40u);
}

TEST_CASE("real data: save payload encode round-trips every slot" *
          doctest::skip(!game_dir_available())) {
    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);

        const auto encoded = gl::assets::encode_save_payload(payload.value());
        REQUIRE(encoded.has_value());
        const auto again = gl::assets::parse_save_payload(encoded.value());
        REQUIRE(again.has_value());

        CHECK(again->header_raw == payload->header_raw);
        CHECK(again->decompressed == payload->decompressed);
        REQUIRE(again->tail.size() == payload->tail.size());
        for (std::size_t index = 0; index < payload->tail.size(); ++index) {
            CHECK(again->tail[index].name == payload->tail[index].name);
            CHECK(again->tail[index].raw_len == payload->tail[index].raw_len);
            CHECK(again->tail[index].section == payload->tail[index].section);
        }
        CHECK(again->info.level_id == payload->info.level_id);
        CHECK(again->info.party_level == payload->info.party_level);
        CHECK(again->info.time == payload->info.time);
        CHECK(again->info.field_218 == payload->info.field_218);
        CHECK(again->info.field_26c8 == payload->info.field_26c8);
        REQUIRE(again->vars.size() == payload->vars.size());
        for (std::size_t index = 0; index < payload->vars.size(); ++index) {
            CHECK(again->vars[index].type == payload->vars[index].type);
            CHECK(again->vars[index].name == payload->vars[index].name);
            CHECK(again->vars[index].value == payload->vars[index].value);
            CHECK(again->vars[index].text_value == payload->vars[index].text_value);
        }
        REQUIRE(again->tribes.entries.size() == payload->tribes.entries.size());
        for (std::size_t index = 0; index < payload->tribes.entries.size(); ++index) {
            CHECK(again->tribes.entries[index].id == payload->tribes.entries[index].id);
            CHECK(again->tribes.entries[index].name == payload->tribes.entries[index].name);
        }
        CHECK(again->tribes.matrix == payload->tribes.matrix);
        CHECK(again->diary.size() == payload->diary.size());
        CHECK(again->diary_sparse.size() == payload->diary_sparse.size());
        CHECK(again->history.head == payload->history.head);
        CHECK(again->history.tail == payload->history.tail);
        REQUIRE(again->history.slots.size() == payload->history.slots.size());
        for (std::size_t index = 0; index < payload->history.slots.size(); ++index) {
            CHECK(again->history.slots[index].text == payload->history.slots[index].text);
            CHECK(again->history.slots[index].id == payload->history.slots[index].id);
        }
        CHECK(again->level_tail_index == payload->level_tail_index);

        const auto encoded_twice = gl::assets::encode_save_payload(payload.value());
        REQUIRE(encoded_twice.has_value());
        CHECK(encoded.value() == encoded_twice.value());
        CHECK(encoded->size() >= 0x70u);
    }
}

TEST_CASE("real data: BT_TRIBE re-encodes byte-identically" *
          doctest::skip(!game_dir_available())) {
    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        const auto body = gl::assets::encode_tribe_body(payload->tribes);
        REQUIRE(body.has_value());
        const auto &block = payload->blocks[3];
        const std::vector<std::uint8_t> expected(
            payload->decompressed.begin() + static_cast<std::ptrdiff_t>(block.offset),
            payload->decompressed.begin() +
                static_cast<std::ptrdiff_t>(block.offset + block.size));
        CHECK(body.value() == expected);
    }
}

TEST_CASE("real data: entity item records round-trip" * doctest::skip(!game_dir_available())) {
    std::size_t records = 0;
    for (int slot = 0; slot < 28; ++slot) {
        CAPTURE(slot);
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        for (const auto &entry : payload->tail) {
            for (const auto &entity : entry.entities) {
                if (entity.type != 2)
                    continue;
                const auto person = gl::game::decode_save_person(entity);
                REQUIRE(person.has_value());
                for (const auto &container : person->containers) {
                    for (const auto &record : container.items) {
                        const auto encoded = gl::assets::encode_item_record(record);
                        REQUIRE(encoded.has_value());
                        CHECK(encoded->size() == 0xC4 + record.modifiers.size() * 0x14);
                        const auto reparsed = gl::assets::parse_item_record(encoded.value());
                        REQUIRE(reparsed.has_value());
                        CHECK(reparsed->record.item_id == record.item_id);
                        CHECK(reparsed->record.count == record.count);
                        CHECK(reparsed->record.flags == record.flags);
                        CHECK(reparsed->record.type == record.type);
                        CHECK(reparsed->record.style == record.style);
                        CHECK(reparsed->record.durability == record.durability);
                        CHECK(reparsed->record.mod_count == record.mod_count);
                        CHECK(reparsed->record.raw == record.raw);
                        REQUIRE(reparsed->record.modifiers.size() == record.modifiers.size());
                        for (std::size_t index = 0; index < record.modifiers.size(); ++index)
                            CHECK(reparsed->record.modifiers[index].words ==
                                  record.modifiers[index].words);
                        ++records;
                    }
                }
            }
        }
    }
    CHECK(records == 115405);
}
