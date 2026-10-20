#include "gl/assets/csx.hpp"
#include "gl/assets/lao.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include <array>
#include <cstdlib>
#include <doctest/doctest.h>
#include <string>
#include <utility>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
} // namespace

TEST_CASE("level loader: l1_1 loads background, statics and animations" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());

    CHECK(level->name == "l1_1");
    CHECK(level->pack == "l1_1");
    CHECK(level->width == 2760);
    CHECK(level->height == 2016);
    CHECK(level->background.width == 2760);
    CHECK(level->background.height == 2016);
    REQUIRE(level->statics.size() == 2);
    CHECK(level->statics[0].x == 905);
    CHECK(level->statics[0].y == 368);
    CHECK(level->statics[1].x == 2163);
    CHECK(level->statics[1].y == 61);
    REQUIRE(level->animations.size() == 2);
    CHECK(level->animations[0].x == 287);
    CHECK(level->animations[0].y == 150);
    CHECK(level->animations[1].x == 51);
    CHECK(level->animations[1].y == 126);

    auto lao_data = vfs->read("levels/pack/l1_1/data/animated/l1_1.lao");
    REQUIRE(lao_data.has_value());
    auto lao = parse_lao(lao_data.value());
    REQUIRE(lao.has_value());
    REQUIRE(lao->size() == level->animations.size());

    for (std::size_t i = 0; i < level->animations.size(); ++i) {
        const auto &animation = level->animations[i];
        REQUIRE_FALSE(animation.frames.empty());
        CHECK(animation.delay_ms == lao->at(i).delay);

        auto csx_data =
            vfs->read("levels/pack/l1_1/bitmaps/animated/anim_" + std::to_string(i) + ".csx");
        REQUIRE(csx_data.has_value());
        auto csx = decode_csx(csx_data.value());
        REQUIRE(csx.has_value());
        CHECK(animation.frames.front().width == csx->width);
        REQUIRE(lao->at(i).height > 0);
        CHECK(animation.frames.size() ==
              (static_cast<std::size_t>(csx->height) + lao->at(i).height - 1) / lao->at(i).height);

        for (std::size_t frame = 0; frame < animation.frames.size(); ++frame) {
            if (frame + 1 < animation.frames.size())
                CHECK(animation.frames[frame].height == lao->at(i).height);
            else
                CHECK(animation.frames[frame].height <= lao->at(i).height);
        }
    }
}

TEST_CASE("level loader: statics are loaded in number order" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto level = gl::game::load_level(vfs.value(), "l6_2_2");
    REQUIRE(level.has_value());

    const std::array<std::pair<int, int>, 7> expected = {{{762, 1000},
                                                          {1112, 1215},
                                                          {1517, 596},
                                                          {1878, 800},
                                                          {1853, 480},
                                                          {2203, 677},
                                                          {1098, 964}}};
    REQUIRE(level->statics.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(level->statics[i].x == expected[i].first);
        CHECK(level->statics[i].y == expected[i].second);
    }
}

TEST_CASE("level loader: missing level reports an error" * doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto level = gl::game::load_level(vfs.value(), "no_such_level");
    REQUIRE_FALSE(level.has_value());
    CHECK((level.error().code == gl::ErrorCode::io || level.error().code == gl::ErrorCode::format));
}
