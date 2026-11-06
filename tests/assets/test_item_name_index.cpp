#include "gl/assets/item_name_index.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"

#include <doctest/doctest.h>

#include <cstdlib>
#include <string>
#include <vector>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
} // namespace

TEST_CASE("item_name index keeps the first duplicate tech name") {
    const std::vector<SdbEntry> entries = {
        {812, "MON_1_0_1"},
        {42, "MON_1_0_1"},
        {307, "ARM_1_08_2"},
    };

    auto index = ItemNameIndex::from_entries(entries);
    REQUIRE(index.has_value());
    CHECK(index->find("MON_1_0_1") == 812);
    CHECK(index->find("ARM_1_08_2") == 307);
    CHECK_FALSE(index->find("mon_1_0_1"));
    CHECK_FALSE(index->find("NOPE"));
}

TEST_CASE("item_name index resolves tech names exactly" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto index = ItemNameIndex::load(vfs.value());
    REQUIRE(index.has_value());
    CHECK(index->find("MON_1_0_1") == 812);
    CHECK(index->find("ARM_1_08_2") == 307);
    CHECK(index->find("QST_1_0_159") == 1101);
    CHECK_FALSE(index->find("mon_1_0_1"));
    CHECK_FALSE(index->find("NOPE"));
}
