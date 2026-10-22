#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/person_render.hpp>

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
