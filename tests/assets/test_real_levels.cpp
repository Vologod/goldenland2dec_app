#include "gl/assets/lao.hpp"
#include "gl/assets/lvl.hpp"
#include "gl/assets/pak.hpp"
#include "gl/assets/sef.hpp"
#include "gl/assets/vfs.hpp"
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <doctest/doctest.h>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::string normalized_entry_name(std::string_view name) {
    std::string path(name);
    for (char& c : path) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (c == '\\') c = '/';
    }
    return path;
}

bool is_level_entry(std::string_view name) {
    const std::string path = normalized_entry_name(name);
    constexpr std::string_view prefix = "levels/lvl/";
    constexpr std::string_view suffix = ".lvl";
    return path.size() > prefix.size() + suffix.size() &&
           path.compare(0, prefix.size(), prefix) == 0 &&
           path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool is_lao_entry(std::string_view name) {
    const std::string path = normalized_entry_name(name);
    constexpr std::string_view prefix = "levels/pack/";
    constexpr std::string_view marker = "/data/animated/";
    constexpr std::string_view suffix = ".lao";
    return path.size() > prefix.size() + marker.size() + suffix.size() &&
           path.compare(0, prefix.size(), prefix) == 0 &&
           path.find(marker, prefix.size()) != std::string::npos &&
           path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string lao_pack(std::string_view name) {
    const std::string path = normalized_entry_name(name);
    constexpr std::string_view prefix = "levels/pack/";
    if (!path.starts_with(prefix)) return {};
    const std::size_t begin = prefix.size();
    const std::size_t end = path.find('/', begin);
    if (end == std::string::npos) return {};
    return path.substr(begin, end - begin);
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

TEST_CASE("real data: every LAO in levels.pak parses" *
          doctest::skip(!game_dir_available())) {
    const char* dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    const PakArchive* levels = vfs->archive("levels.pak");
    REQUIRE(levels != nullptr);

    std::size_t total = 0;
    std::size_t parsed = 0;
    std::size_t failures = 0;
    std::set<std::string> packs;
    for (const auto& entry : levels->entries()) {
        if (!is_lao_entry(entry.name)) continue;
        ++total;
        packs.insert(lao_pack(entry.name));
        auto data = levels->read(entry);
        if (!data) {
            ++failures;
            std::cout << "read failure: " << entry.name << ": " << data.error().message << '\n';
            continue;
        }
        auto lao = parse_lao(data.value());
        if (!lao) {
            ++failures;
            std::cout << "parse failure: " << entry.name << ": " << lao.error().message << '\n';
            continue;
        }
        ++parsed;
    }

    std::cout << "real data parsed LAO: " << parsed << '/' << total << " across " << packs.size()
              << " packs\n";
    CHECK(failures == 0);
    CHECK(total == 33);
    CHECK(packs.size() == 33);
    CHECK(parsed == total);
}

TEST_CASE("real data: l1_1 SEF parses" * doctest::skip(!game_dir_available())) {
    const char* dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto data = vfs->read("levels/single/l1_1/l1_1.sef");
    REQUIRE(data.has_value());
    auto sef = parse_sef(data.value());
    REQUIRE(sef.has_value());
    CHECK(sef->pack == "l1_1");
    CHECK_FALSE(sef->internal_location);
    CHECK(sef->exit_to_global_map);
    CHECK(sef->weather == 1);
}
