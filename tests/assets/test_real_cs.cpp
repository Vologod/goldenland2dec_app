#include "gl/assets/cs.hpp"
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

bool has_cs_extension(std::string_view name) {
    if (name.size() < 3) return false;
    const std::string_view ext = name.substr(name.size() - 3);
    return (ext[0] == '.') && std::tolower(static_cast<unsigned char>(ext[1])) == 'c' &&
           std::tolower(static_cast<unsigned char>(ext[2])) == 's';
}
} // namespace

TEST_CASE("real data: every .cs entry in scripts.pak parses" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    const PakArchive* archive = vfs->archive("scripts.pak");
    REQUIRE(archive != nullptr);

    std::size_t parsed = 0;
    std::size_t failures = 0;
    std::size_t nodes = 0;

    for (const auto& entry : archive->entries()) {
        if (!has_cs_extension(entry.name)) continue;

        auto data = archive->read(entry);
        if (!data) {
            ++failures;
            std::cout << "read failure: " << entry.name << ": " << data.error().message << '\n';
            continue;
        }
        auto result = parse_cs(data.value());
        if (!result) {
            ++failures;
            std::cout << "parse failure: " << entry.name << ": " << result.error().message << '\n';
            continue;
        }
        ++parsed;
        nodes += result->size();
    }

    std::cout << "real data .cs: parsed=" << parsed << " failures=" << failures
              << " nodes=" << nodes << '\n';
    CHECK(parsed + failures == 555);
    CHECK(failures == 0);
    CHECK(parsed == 555);
    CHECK(nodes > 0);
}
