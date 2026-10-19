#include "gl/assets/lvl.hpp"
#include "gl/assets/pak.hpp"
#include "gl/assets/vfs.hpp"
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <doctest/doctest.h>
#include <iostream>
#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

bool is_level_entry(std::string_view name) {
    std::string path(name);
    for (char& c : path) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (c == '\\') c = '/';
    }
    constexpr std::string_view prefix = "levels/lvl/";
    constexpr std::string_view suffix = ".lvl";
    return path.size() > prefix.size() + suffix.size() &&
           path.compare(0, prefix.size(), prefix) == 0 &&
           path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}
} // namespace

TEST_CASE("real data: every LVL in levels.pak parses" * doctest::skip(!game_dir_available())) {
    const char* dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    const PakArchive* levels = vfs->archive("levels.pak");
    REQUIRE(levels != nullptr);

    std::size_t total = 0;
    std::size_t parsed = 0;
    std::size_t failures = 0;
    for (const auto& entry : levels->entries()) {
        if (!is_level_entry(entry.name)) continue;
        ++total;
        auto data = levels->read(entry);
        if (!data) {
            ++failures;
            std::cout << "read failure: " << entry.name << ": " << data.error().message << '\n';
            continue;
        }
        auto lvl = parse_lvl(data.value());
        if (!lvl) {
            ++failures;
            std::cout << "parse failure: " << entry.name << ": " << lvl.error().message << '\n';
            continue;
        }
        if (lvl->chunks.empty()) {
            ++failures;
            std::cout << "empty level: " << entry.name << '\n';
            continue;
        }
        ++parsed;
    }

    std::cout << "real data parsed LVL: " << parsed << '/' << total << '\n';
    CHECK(failures == 0);
    CHECK(total == 175);
    CHECK(parsed == total);
}
