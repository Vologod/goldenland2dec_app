#include "gl/assets/pad.hpp"
#include "gl/assets/pak.hpp"

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <iostream>
#include <string_view>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

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
    CHECK(pad_entries > 0);
    CHECK(failures == 0);
    CHECK(parsed == pad_entries);
    CHECK(animations > 0);
}
