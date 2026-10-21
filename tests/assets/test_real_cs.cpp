#include "gl/assets/cs.hpp"
#include "gl/assets/pak.hpp"
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

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

    const std::filesystem::path pak_path =
        std::filesystem::path(dir) / "Data" / "scripts.pak";
    std::error_code ec;
    const std::uintmax_t file_size = std::filesystem::file_size(pak_path, ec);
    REQUIRE_FALSE(ec);

    Bytes payload(static_cast<std::size_t>(file_size));
    std::ifstream in(pak_path, std::ios::binary);
    REQUIRE(in);
    const bool read_ok =
        static_cast<bool>(in.read(reinterpret_cast<char*>(payload.data()),
                                  static_cast<std::streamsize>(payload.size())));
    REQUIRE(read_ok);

    auto archive = PakArchive::open_from_memory(std::move(payload));
    REQUIRE(archive.has_value());

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
