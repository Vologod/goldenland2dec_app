#include "gl/assets/save_payload.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/save_person.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) /
                                       "Saves" / ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

std::string container_text(const gl::game::SaveContainer &container) {
    std::string text = container.present ? "1:" : "0:";
    bool first = true;
    for (const gl::assets::SaveItemRecord &item : container.items) {
        if (!first)
            text += ',';
        first = false;
        text += std::to_string(item.item_id) + "*" + std::to_string(item.count);
    }
    return text;
}

struct ContainerPin {
    const char *name;
    std::size_t occurrence;
    const char *c0;
    const char *c1;
    const char *c2;
};

std::vector<gl::game::SavePersonRecord> decode_section(const gl::assets::SaveTailEntry &tail) {
    std::vector<gl::game::SavePersonRecord> records;
    for (const gl::assets::SaveEntity &entity : tail.entities) {
        if (entity.type != 2)
            continue;
        const auto decoded = gl::game::decode_save_person(entity);
        REQUIRE(decoded.has_value());
        records.push_back(decoded.value());
    }
    return records;
}

std::string legacy_combined(const gl::game::SavePersonRecord &record) {
    return record.name + "\t" + std::to_string(record.current_tile_x) + "\t" +
           std::to_string(record.current_tile_y) + "\t" +
           std::to_string(record.anchor_tile_x) + "\t" +
           std::to_string(record.anchor_tile_y) + "\t" +
           std::to_string(record.direction) + "\t" +
           std::to_string(record.route_type) + "\t" + record.route_name + "\t" +
           container_text(record.containers[0]) + "\t" +
           container_text(record.containers[1]) + "\t" +
           container_text(record.containers[2]);
}

std::string join_lines(const std::vector<std::string> &lines) {
    std::string text;
    for (const std::string &line : lines)
        text += line + '\n';
    return text;
}

const std::vector<ContainerPin> kSlot0ContainerPins{
    {"L93_2.P292_Chernobogs_Guardian2", 1, "1:812*597,420*1", "1:812*289,421*2", "1:1784*1"},
    {"L93_2.P292_Chernobogs_Guardian2", 2,
     "1:812*688,919*50,920*50,921*50,317*1,318*1", "1:812*561", "1:1784*1"},
    {"L93_4.P284_Chernobog", 1, "0:", "1:", "1:1661*1"},
};

const std::vector<ContainerPin> kSlot21ContainerPins{
    {"L1_1.P387_Poison_Plotnik", 1, "1:975*1", "1:975*1", "1:1630*1"},
    {"L0.P678_citizen4", 1, "1:812*93,421*3", "1:812*72", "1:1787*1"},
    {"L0.P7_Kolbasens_Rodstvennik1", 1, "1:812*82,815*2",
     "1:812*76,445*1,446*1,447*1,926*1", "1:1781*1"},
    {"L1_1.P389_Crying_Guardian", 1, "1:812*129,294*1,328*1",
     "1:812*100,262*1,338*1,371*1", "1:1654*1"},
    {"L1_2.P404_Sister_Of_Mercy2", 1,
     "1:812*110,19*1,209*1,102*1,120*1,769*1,1515*1,1528*1", "1:", "1:1787*1"},
    {"L0.P785_witch3", 1, "1:812*87,124*1,1466*1,1515*1,1528*1,1556*1",
     "1:812*77", "1:1787*1"},
    {"L1_2.P403_Sister_Of_Mercy1", 1, "1:812*137", "1:812*68", "1:1786*1"},
    {"L0.P693_citizen6", 1, "1:418*2", "1:940*1", "1:1786*1"},
    {"L0.P711_citizen8", 1, "1:812*77", "1:812*84", "1:1780*1"},
    {"L0.P692_citizen5", 1, "1:812*81", "1:812*117", "1:1787*1"},
    {"L0.P744_oficer1", 1, "1:812*83,263*1", "1:812*92", "1:1639*1"},
    {"L0.P744_oficer1", 2, "1:812*103,534*1", "1:812*112,289*1,295*1", "1:1639*1"},
    {"L0.P640_children2", 1, "1:812*87,926*1", "1:812*112,442*1", "1:1750*1"},
    {"L1.P621_Svet_3CircusWorkerV", 1, "1:812*89,407*3,411*2", "1:812*84,411*1",
     "1:1780*1"},
    {"L0.P733_artist1", 1, "1:812*128,407*3,415*3", "1:812*82", "1:1780*1"},
    {"L1.P620_Svet_3CircusWorker2", 1, "1:812*114", "1:", "1:1787*1"},
};

const char *kSlot0LegacyCombined =
    "L93_2.P292_Chernobogs_Guardian2\t131\t115\t131\t115\t4\t0\t\t1:812*597,420*1\t"
    "1:812*289,421*2\t1:1784*1\n"
    "L93_2.P292_Chernobogs_Guardian2\t103\t122\t103\t122\t5\t0\t\t"
    "1:812*688,919*50,920*50,921*50,317*1,318*1\t1:812*561\t1:1784*1\n"
    "L93_4.P284_Chernobog\t120\t127\t120\t127\t5\t0\t\t0:\t1:\t1:1661*1\n";

const char *kSlot21LegacyCombined =
    "L1_1.P387_Poison_Plotnik\t133\t12\t133\t12\t5\t0\t\t1:975*1\t1:975*1\t1:1630*1\n"
    "L0.P678_citizen4\t57\t51\t59\t49\t1\t3\ttrack4\t1:812*93,421*3\t1:812*72\t1:1787*1\n"
    "L0.P7_Kolbasens_Rodstvennik1\t196\t56\t192\t49\t5\t4\t\t1:812*82,815*2\t"
    "1:812*76,445*1,446*1,447*1,926*1\t1:1781*1\n"
    "L1_1.P389_Crying_Guardian\t38\t51\t38\t51\t5\t0\t\t1:812*129,294*1,328*1\t"
    "1:812*100,262*1,338*1,371*1\t1:1654*1\n"
    "L1_2.P404_Sister_Of_Mercy2\t127\t87\t127\t87\t3\t0\t\t"
    "1:812*110,19*1,209*1,102*1,120*1,769*1,1515*1,1528*1\t1:\t1:1787*1\n"
    "L0.P785_witch3\t129\t91\t129\t91\t3\t0\t\t"
    "1:812*87,124*1,1466*1,1515*1,1528*1,1556*1\t1:812*77\t1:1787*1\n"
    "L1_2.P403_Sister_Of_Mercy1\t134\t91\t134\t91\t3\t0\t\t1:812*137\t1:812*68\t1:1786*1\n"
    "L0.P693_citizen6\t99\t121\t99\t121\t5\t0\t\t1:418*2\t1:940*1\t1:1786*1\n"
    "L0.P711_citizen8\t102\t124\t102\t124\t1\t0\t\t1:812*77\t1:812*84\t1:1780*1\n"
    "L0.P692_citizen5\t157\t128\t159\t124\t3\t3\ttrack1\t1:812*81\t1:812*117\t1:1787*1\n"
    "L0.P744_oficer1\t66\t136\t66\t136\t7\t0\t\t1:812*83,263*1\t1:812*92\t1:1639*1\n"
    "L0.P744_oficer1\t186\t147\t190\t148\t1\t2\ttrack3\t1:812*103,534*1\t"
    "1:812*112,289*1,295*1\t1:1639*1\n"
    "L0.P640_children2\t177\t186\t71\t172\t6\t3\ttrack2\t1:812*87,926*1\t"
    "1:812*112,442*1\t1:1750*1\n"
    "L1.P621_Svet_3CircusWorkerV\t187\t185\t187\t185\t5\t0\t\t1:812*89,407*3,411*2\t"
    "1:812*84,411*1\t1:1780*1\n"
    "L0.P733_artist1\t191\t188\t191\t188\t1\t0\t\t1:812*128,407*3,415*3\t1:812*82\t1:1780*1\n"
    "L1.P620_Svet_3CircusWorker2\t168\t199\t173\t189\t3\t4\t\t1:812*114\t1:\t1:1787*1\n";

void check_container_pins(const std::vector<gl::game::SavePersonRecord> &records,
                          const std::vector<ContainerPin> &pins) {
    REQUIRE(records.size() == pins.size());
    for (std::size_t index = 0; index < pins.size(); ++index) {
        const ContainerPin &pin = pins[index];
        CAPTURE(index);
        CAPTURE(pin.name);
        CHECK(records[index].name == pin.name);
        CHECK(container_text(records[index].containers[0]) == pin.c0);
        CHECK(container_text(records[index].containers[1]) == pin.c1);
        CHECK(container_text(records[index].containers[2]) == pin.c2);
    }
}
} // namespace

TEST_CASE("real data: save container pins" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());

    {
        auto payload = gl::assets::parse_save_payload(read_save(0));
        REQUIRE(payload.has_value());
        REQUIRE(payload->level_tail_index.has_value());
        const auto records =
            decode_section(payload->tail[*payload->level_tail_index]);
        REQUIRE(records.size() == 3);
        check_container_pins(records, kSlot0ContainerPins);
        std::vector<std::string> legacy;
        for (const auto &record : records)
            legacy.push_back(legacy_combined(record));
        CHECK(join_lines(legacy) == kSlot0LegacyCombined);
    }
    {
        auto payload = gl::assets::parse_save_payload(read_save(2));
        REQUIRE(payload.has_value());
        REQUIRE(payload->level_tail_index.has_value());
        const auto records =
            decode_section(payload->tail[*payload->level_tail_index]);
        CHECK(records.empty());
    }
    {
        auto payload = gl::assets::parse_save_payload(read_save(21));
        REQUIRE(payload.has_value());
        REQUIRE(payload->level_tail_index.has_value());
        const auto records =
            decode_section(payload->tail[*payload->level_tail_index]);
        REQUIRE(records.size() == 16);
        check_container_pins(records, kSlot21ContainerPins);
        std::vector<std::string> legacy;
        for (const auto &record : records)
            legacy.push_back(legacy_combined(record));
        CHECK(join_lines(legacy) == kSlot21LegacyCombined);
    }
}

TEST_CASE("real data: save container corpus aggregate" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());

    auto sdb_bytes = vfs->read("sdb/items/tech_names.sdb");
    REQUIRE(sdb_bytes.has_value());
    auto sdb = gl::assets::Sdb::parse(sdb_bytes.value());
    REQUIRE(sdb.has_value());

    std::size_t type2_records = 0;
    std::size_t item_records = 0;
    std::size_t containers_present = 0;
    std::size_t containers_absent = 0;
    std::size_t zero_count_containers = 0;
    std::size_t c2_items = 0;
    std::size_t selected_records = 0;
    std::set<std::uint32_t> item_ids;

    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        REQUIRE(payload->level_tail_index.has_value());
        for (std::size_t index = 0; index < payload->tail.size(); ++index) {
            for (const gl::assets::SaveEntity &entity : payload->tail[index].entities) {
                if (entity.type != 2)
                    continue;
                const auto decoded = gl::game::decode_save_person(entity);
                REQUIRE(decoded.has_value());
                ++type2_records;
                if (index == *payload->level_tail_index)
                    ++selected_records;
                for (const gl::game::SaveContainer &container : decoded->containers) {
                    if (container.items.empty())
                        ++zero_count_containers;
                    if (!container.present) {
                        ++containers_absent;
                        continue;
                    }
                    ++containers_present;
                    item_records += container.items.size();
                    for (const gl::assets::SaveItemRecord &item : container.items)
                        item_ids.insert(item.item_id);
                }
                c2_items += decoded->containers[2].items.size();
            }
        }
    }

    CHECK(type2_records == 14507);
    CHECK(selected_records == 499);
    CHECK(item_records == 115405);
    CHECK(containers_present == 42192);
    CHECK(containers_absent == 1329);
    CHECK(zero_count_containers == 5150);
    CHECK(c2_items == 14156);

    std::size_t unresolved_names = 0;
    std::size_t missing_archives = 0;
    for (std::uint32_t id : item_ids) {
        if (sdb->find(id) == nullptr)
            ++unresolved_names;
        if (!vfs->exists("items/data/" + std::to_string(id) + ".itm"))
            ++missing_archives;
    }
    CHECK(unresolved_names == 0);
    CHECK(missing_archives == 0);
}
