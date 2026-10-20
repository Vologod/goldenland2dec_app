#include "gl/assets/csx.hpp"
#include "gl/assets/lao.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include <cstdlib>
#include <doctest/doctest.h>
#include <string>

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
    REQUIRE(level->animations.size() == 2);

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

        for (std::size_t frame = 0; frame < animation.frames.size(); ++frame) {
            if (frame + 1 < animation.frames.size())
                CHECK(animation.frames[frame].height == lao->at(i).height);
            else
                CHECK(animation.frames[frame].height <= lao->at(i).height);
        }
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
