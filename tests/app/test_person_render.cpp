#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/mask_render.hpp>
#include <gl/app/person_render.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/game/level.hpp>

#include <algorithm>
#include <cstdlib>

int main(int argc, char** argv) {
    doctest::Context context(argc, argv);
    return context.run();
}

namespace {

gl::assets::PadAnimation make_animation(gl::assets::PadAnimationType type,
                                        std::int32_t anchor_x = 20,
                                        std::int32_t anchor_y = 77) {
    gl::assets::PadAnimation animation;
    animation.type = type;
    animation.anchor_x = anchor_x;
    animation.anchor_y = anchor_y;
    animation.shadow_anchor_x = 31;
    animation.shadow_anchor_y = 16;
    animation.frames_per_row = 12;
    return animation;
}

}

TEST_CASE("px placement uses float position") {
    gl::assets::PadAnimation animation;
    animation.type = gl::assets::PadAnimationType::rt_go;
    animation.anchor_x = 20;
    animation.anchor_y = 77;
    auto placement = gl::app::place_person_px(animation, 12, 1610.0f, 1147.0f);
    CHECK(placement.draw_x == doctest::Approx(1590.0f));
    CHECK(placement.draw_y == doctest::Approx(1070.0f));
    CHECK(placement.row == 4);
    CHECK(placement.mirror);
}

TEST_CASE("standing person placement uses the cell-center simulation position") {
    const gl::assets::PadAnimation animation =
        make_animation(gl::assets::PadAnimationType::rt_stay);
    const gl::app::PersonPlacement placement =
        gl::app::place_person_px(animation, 0, 38.0f * 12.0f + 6.0f, 51.0f * 9.0f + 4.0f);
    CHECK(placement.draw_x == doctest::Approx(38.0f * 12.0f + 6.0f - 20.0f));
    CHECK(placement.draw_y == doctest::Approx(51.0f * 9.0f + 4.0f - 77.0f));
    CHECK(placement.row == 0);
    CHECK_FALSE(placement.mirror);
}

TEST_CASE("shadow px placement uses float position and identity rows") {
    const gl::assets::PadAnimation animation =
        make_animation(gl::assets::PadAnimationType::rt_go);
    const gl::app::PersonShadowPlacement placement =
        gl::app::place_person_shadow_px(animation, 12, 1610.0f, 1147.0f);
    CHECK(placement.draw_x == doctest::Approx(1610.0f - 31.0f));
    CHECK(placement.draw_y == doctest::Approx(1147.0f - 16.0f));
    CHECK(placement.row == 12);
}

TEST_CASE("person draw order sorts by Y then X") {
    const std::array<gl::app::PersonDrawKey, 5> keys{{
        {10.0f, 20.0f},
        {5.0f, 20.0f},
        {5.0f, 19.0f},
        {5.0f, 19.0f},
        {50.0f, 10.0f},
    }};
    const std::vector<std::size_t> order = gl::app::sort_person_indices(keys);
    CHECK(order == std::vector<std::size_t>{4, 2, 3, 1, 0});
}

TEST_CASE("person draw order is stable for equal keys") {
    const std::array<gl::app::PersonDrawKey, 3> keys{{
        {7.0f, 42.0f},
        {7.0f, 42.0f},
        {7.0f, 42.0f},
    }};
    const std::vector<std::size_t> order = gl::app::sort_person_indices(keys);
    CHECK(order == std::vector<std::size_t>{0, 1, 2});
}

TEST_CASE("same baseline sorts by X") {
    const std::array<gl::app::PersonDrawKey, 2> keys{{{120.0f, 300.0f}, {84.0f, 300.0f}}};
    const std::vector<std::size_t> order = gl::app::sort_person_indices(keys);
    CHECK(order == std::vector<std::size_t>{1, 0});
}

TEST_CASE("mask foreground copies background through alpha zero silhouette") {
    gl::assets::Image background;
    background.width = 4;
    background.height = 4;
    background.rgba.assign(16, gl::assets::Rgba{10, 20, 30, 255});

    gl::assets::Image mask;
    mask.width = 2;
    mask.height = 2;
    mask.rgba.assign(4, gl::assets::Rgba{0, 0, 0, 255});
    mask.rgba[0].a = 0;
    mask.rgba[3].a = 0;

    const gl::assets::Image out = gl::app::compose_mask_foreground(background, mask, 1, 1);
    REQUIRE(out.width == 2);
    REQUIRE(out.height == 2);
    REQUIRE(out.rgba.size() == 4);
    CHECK(out.rgba[0].r == 10);
    CHECK(out.rgba[0].g == 20);
    CHECK(out.rgba[0].b == 30);
    CHECK(out.rgba[0].a == 255);
    CHECK(out.rgba[1].a == 0);
    CHECK(out.rgba[2].a == 0);
    CHECK(out.rgba[3].r == 10);
    CHECK(out.rgba[3].a == 255);
}

TEST_CASE("mask foreground skips out of bounds") {
    gl::assets::Image background;
    background.width = 1;
    background.height = 1;
    background.rgba.assign(1, gl::assets::Rgba{1, 2, 3, 255});

    gl::assets::Image mask;
    mask.width = 2;
    mask.height = 1;
    mask.rgba.assign(2, gl::assets::Rgba{0, 0, 0, 0});

    const gl::assets::Image out = gl::app::compose_mask_foreground(background, mask, 0, 0);
    REQUIRE(out.rgba.size() == 2);
    CHECK(out.rgba[0].a == 255);
    CHECK(out.rgba[1].a == 0);
}

TEST_CASE("mask foreground rejects empty images") {
    gl::assets::Image background;
    background.width = 2;
    background.height = 2;
    background.rgba.assign(4, gl::assets::Rgba{1, 2, 3, 255});
    gl::assets::Image mask;
    const gl::assets::Image out = gl::app::compose_mask_foreground(background, mask, 0, 0);
    CHECK(out.rgba.empty());
}

TEST_CASE("frame rect is the full PAD cell") {
    const gl::app::FrameRect rect = gl::app::frame_rect(3, 2, 40, 85);
    CHECK(rect.x == 120);
    CHECK(rect.y == 170);
    CHECK(rect.w == 40);
    CHECK(rect.h == 85);
}

TEST_CASE("frame rect ignores PAD crops") {
    gl::assets::PadAnimation animation;
    animation.frame_width = 32;
    animation.frame_height = 64;
    animation.frames_per_row = 12;
    animation.crops = {{{1, 2, 3, 4}}};
    const gl::app::FrameRect rect = gl::app::frame_rect(2, 0, animation.frame_width,
                                                        animation.frame_height);
    CHECK(rect.x == 64);
    CHECK(rect.y == 0);
    CHECK(rect.w == 32);
    CHECK(rect.h == 64);
}

TEST_CASE("person frame index wraps by frames per row") {
    CHECK(gl::app::person_frame_index(0, 89, 12) == 0);
    CHECK(gl::app::person_frame_index(88, 89, 12) == 0);
    CHECK(gl::app::person_frame_index(89, 89, 12) == 1);
    CHECK(gl::app::person_frame_index(89 * 11, 89, 12) == 11);
    CHECK(gl::app::person_frame_index(89 * 12, 89, 12) == 0);
    CHECK(gl::app::person_frame_index(1000, 89, 0) == 0);
}

TEST_CASE("person frame index treats non-positive delays as one") {
    CHECK(gl::app::person_frame_index(7, 0, 5) == 2);
    CHECK(gl::app::person_frame_index(7, -50, 5) == 2);
    CHECK(gl::app::person_frame_index(4, 0, 5) == 4);
}

TEST_CASE("person placement subtracts PAD anchors from tile pixels") {
    const gl::assets::PadAnimation animation =
        make_animation(gl::assets::PadAnimationType::rt_stay);
    const gl::app::PersonPlacement placement =
        gl::app::place_person(animation, 0, 133, 12);
    CHECK(placement.draw_x == doctest::Approx(133.0f * 12.0f - 20.0f));
    CHECK(placement.draw_y == doctest::Approx(12.0f * 9.0f - 77.0f));
    CHECK(placement.row == 0);
    CHECK_FALSE(placement.mirror);

    const gl::app::PersonPlacement negative =
        gl::app::place_person(animation, 0, 1, 0);
    CHECK(negative.draw_x == doctest::Approx(-8.0f));
    CHECK(negative.draw_y == doctest::Approx(-77.0f));
}

TEST_CASE("person directions map to PAD rows and mirror flags") {
    const gl::assets::PadAnimation animation =
        make_animation(gl::assets::PadAnimationType::rt_stay);
    constexpr std::uint8_t kRows[8] = {0, 1, 2, 3, 4, 3, 2, 1};
    constexpr bool kMirror[8] = {false, false, false, false, false, true, true, true};
    for (std::uint8_t direction = 0; direction < 8; ++direction) {
        const gl::app::PersonPlacement placement =
            gl::app::place_person(animation, direction, 0, 0);
        CAPTURE(direction);
        CHECK(placement.row == kRows[direction]);
        CHECK(placement.mirror == kMirror[direction]);
    }
}

TEST_CASE("go animations use the 16 direction row table") {
    const gl::assets::PadAnimation animation =
        make_animation(gl::assets::PadAnimationType::tb_go);
    const gl::app::PersonPlacement forward = gl::app::place_person(animation, 8, 0, 0);
    CHECK(forward.row == 8);
    CHECK_FALSE(forward.mirror);

    const gl::app::PersonPlacement mirrored = gl::app::place_person(animation, 9, 0, 0);
    CHECK(mirrored.row == 7);
    CHECK(mirrored.mirror);

    const gl::app::PersonPlacement last = gl::app::place_person(animation, 15, 0, 0);
    CHECK(last.row == 1);
    CHECK(last.mirror);
}

TEST_CASE("shadow placement uses identity rows and shadow anchors") {
    const gl::assets::PadAnimation animation =
        make_animation(gl::assets::PadAnimationType::rt_stay);
    const gl::app::PersonShadowPlacement placement =
        gl::app::place_person_shadow(animation, 5, 10, 20);
    CHECK(placement.draw_x == doctest::Approx(10.0f * 12.0f - 31.0f));
    CHECK(placement.draw_y == doctest::Approx(20.0f * 9.0f - 16.0f));
    CHECK(placement.row == 5);

    const gl::assets::PadAnimation go =
        make_animation(gl::assets::PadAnimationType::rt_go);
    const gl::app::PersonShadowPlacement go_placement =
        gl::app::place_person_shadow(go, 15, 0, 0);
    CHECK(go_placement.row == 15);
}

TEST_CASE("real data: l1_1 mask 45 foreground silhouette" *
          doctest::skip(std::getenv("GL_GAME_DIR") == nullptr)) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    const auto it = std::find_if(level->masks.begin(), level->masks.end(),
                                 [](const gl::game::LevelMask& mask) {
                                     return mask.number == 45;
                                 });
    REQUIRE(it != level->masks.end());
    const gl::assets::Image foreground =
        gl::app::compose_mask_foreground(level->background, it->image, it->x, it->y);
    REQUIRE(foreground.width == 687);
    REQUIRE(foreground.height == 741);
    std::size_t opaque = 0;
    for (const gl::assets::Rgba& pixel : foreground.rgba) {
        if (pixel.a != 0) ++opaque;
    }
    CHECK(opaque == 182090);
}
