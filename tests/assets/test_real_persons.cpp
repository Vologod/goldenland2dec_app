#include "gl/assets/pad.hpp"
#include "gl/assets/pak.hpp"
#include "gl/assets/person_script.hpp"
#include "gl/assets/sef.hpp"

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::string ascii_lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

bool has_pad_extension(std::string_view name) {
    if (name.size() < 4)
        return false;
    const std::string_view ext = name.substr(name.size() - 4);
    return ext[0] == '.' && std::tolower(static_cast<unsigned char>(ext[1])) == 'p' &&
           std::tolower(static_cast<unsigned char>(ext[2])) == 'a' &&
           std::tolower(static_cast<unsigned char>(ext[3])) == 'd';
}
} // namespace

TEST_CASE("real data: every .pad entry in persons.pak parses" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);

    const std::filesystem::path pak_path = std::filesystem::path(dir) / "Data" / "persons.pak";
    auto archive = PakArchive::open_from_file(pak_path);
    REQUIRE(archive.has_value());

    std::size_t pad_entries = 0;
    std::size_t parsed = 0;
    std::size_t failures = 0;
    std::size_t animations = 0;

    for (const auto &entry : archive->entries()) {
        if (!has_pad_extension(entry.name))
            continue;
        ++pad_entries;

        auto data = archive->read(entry);
        if (!data) {
            ++failures;
            std::cout << "read failure: " << entry.name << ": " << data.error().message << '\n';
            continue;
        }
        auto result = parse_pad(data.value());
        if (!result) {
            ++failures;
            std::cout << "parse failure: " << entry.name << ": " << result.error().message << '\n';
            continue;
        }
        ++parsed;
        animations += result->size();
    }

    std::cout << "real data .pad: entries=" << pad_entries << " parsed=" << parsed
              << " failures=" << failures << " animations=" << animations << '\n';
    CHECK(pad_entries == 276);
    CHECK(failures == 0);
    CHECK(parsed == pad_entries);
    CHECK(animations > 0);
}

TEST_CASE("real data: l1_1 SEF persons and their scripts" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    const std::filesystem::path data_dir = std::filesystem::path(dir) / "Data";

    auto levels = PakArchive::open_from_file(data_dir / "levels.pak");
    REQUIRE(levels.has_value());
    const PakEntry *sef_entry = levels->find("levels/single/l1_1/l1_1.sef");
    REQUIRE(sef_entry != nullptr);
    auto sef_data = levels->read(*sef_entry);
    REQUIRE(sef_data.has_value());
    auto sef = parse_sef(sef_data.value());
    REQUIRE(sef.has_value());

    std::cout << "real data l1_1 persons=" << sef->persons.size() << '\n';
    REQUIRE(sef->persons.size() == 16);

    const SefPerson& first = sef->persons.front();
    CHECK(first.name == "L1_1.P387_Poison_Plotnik");
    CHECK(first.x == 133);
    CHECK(first.y == 12);
    CHECK(first.direction == "DOWN_RIGHT");
    CHECK(first.route_type == "STAY");
    CHECK(first.route.empty());
    CHECK(first.radius == 15);
    CHECK(first.delay_min == 0);
    CHECK(first.delay_max == 9000);
    CHECK(first.tribe == "L1_1_citizen");
    CHECK(first.dialog == "l1_1\\l1_1.p387_poison_plotnik.d387.age");
    CHECK(first.inventory == "p387");

    const SefPerson& last = sef->persons.back();
    CHECK(last.name == "L1.P620_Svet_3CircusWorker2");
    CHECK(last.x == 173);
    CHECK(last.y == 189);
    CHECK(last.direction == "DOWN_LEFT");
    CHECK(last.route_type == "RANDOM_RADIUS");
    CHECK(last.radius == 15);
    CHECK(last.delay_max == 9000);
    CHECK(last.tribe == "L1_2_citizen");
    CHECK(last.inventory == "inv012_civil");

    std::set<std::string> scripts;
    for (const SefPerson& person : sef->persons)
        scripts.insert("scripts/persons/" + ascii_lower(person.name) + ".scr");
    CHECK(scripts.size() == 15);

    auto archive = PakArchive::open_from_file(data_dir / "scripts.pak");
    REQUIRE(archive.has_value());

    std::size_t parsed = 0;
    for (const std::string& script : scripts) {
        const PakEntry *entry = archive->find(script);
        CAPTURE(script);
        REQUIRE(entry != nullptr);
        auto data = archive->read(*entry);
        REQUIRE(data.has_value());
        auto person = parse_person_script(data.value());
        REQUIRE(person.has_value());
        CHECK_FALSE(person->res_name.empty());
        if (script == "scripts/persons/l1_1.p387_poison_plotnik.scr")
            CHECK(person->res_name == "L0.P1131_Inhabitans03");
        if (script == "scripts/persons/l1.p620_svet_3circusworker2.scr")
            CHECK(person->res_name == "L0.P1158_Clowns_Male01");
        ++parsed;
    }
    CHECK(parsed == scripts.size());
    CHECK(parsed == 15);
}
