#include "gl/render/animation.hpp"

#include <doctest.h>

TEST_CASE("animation_frame_index starts at frame 0 at time 0") {
    CHECK(gl::render::animation_frame_index(0, 100, 4) == 0);
}

TEST_CASE("animation_frame_index advances exactly at the delay boundary") {
    CHECK(gl::render::animation_frame_index(99, 100, 4) == 0);
    CHECK(gl::render::animation_frame_index(100, 100, 4) == 1);
    CHECK(gl::render::animation_frame_index(199, 100, 4) == 1);
    CHECK(gl::render::animation_frame_index(200, 100, 4) == 2);
}

TEST_CASE("animation_frame_index wraps around after the last frame") {
    CHECK(gl::render::animation_frame_index(299, 100, 3) == 2);
    CHECK(gl::render::animation_frame_index(300, 100, 3) == 0);
    CHECK(gl::render::animation_frame_index(400, 100, 3) == 1);
}

TEST_CASE("animation_frame_index returns 0 for a zero delay") {
    CHECK(gl::render::animation_frame_index(500, 0, 4) == 0);
}

TEST_CASE("animation_frame_index returns 0 for zero frames") {
    CHECK(gl::render::animation_frame_index(500, 100, 0) == 0);
}
