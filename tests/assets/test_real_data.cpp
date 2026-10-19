#include "gl/assets/itm.hpp"
#include "gl/assets/pak.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"
#include <cstddef>
#include <cstdlib>
#include <doctest/doctest.h>
#include <string_view>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
} // namespace

TEST_CASE("real data: archive counts and samples" * doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto count_for = [&](std::string_view name) -> std::size_t {
        for (const auto &a : vfs->archives())
            if (a.name == name)
                return a.entry_count;
        return 0;
    };
    CHECK(count_for("scripts.pak") == 4485);
    CHECK(count_for("sdb.pak") == 45);
    CHECK(count_for("items.pak") == 3786);
    CHECK(count_for("engineres.pak") == 1041);

    auto hero = vfs->read("sdb/hero.sdb");
    REQUIRE(hero.has_value());
    auto sdb = Sdb::parse(hero.value());
    CHECK(sdb.has_value());

    auto item = vfs->read("items/data/0.itm");
    REQUIRE(item.has_value());
    auto parsed = parse_item(item.value());
    CHECK(parsed.has_value());
}
