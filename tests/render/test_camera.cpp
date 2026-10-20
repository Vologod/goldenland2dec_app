#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "gl/render/camera.hpp"

#include <doctest.h>

TEST_CASE("clamp_camera keeps a large map inside the view") {
    gl::render::Camera camera;
    camera.x = -120.0f;
    camera.y = -50.0f;
    gl::render::clamp_camera(camera, 2760.0f, 2016.0f, 1280, 720);
    CHECK(camera.x == doctest::Approx(0.0f));
    CHECK(camera.y == doctest::Approx(0.0f));

    camera.x = 5000.0f;
    camera.y = 5000.0f;
    gl::render::clamp_camera(camera, 2760.0f, 2016.0f, 1280, 720);
    CHECK(camera.x == doctest::Approx(1480.0f));
    CHECK(camera.y == doctest::Approx(1296.0f));
}

TEST_CASE("clamp_camera centers a map smaller than the window") {
    gl::render::Camera camera;
    camera.x = 500.0f;
    camera.y = -500.0f;
    gl::render::clamp_camera(camera, 100.0f, 100.0f, 1280, 720);
    CHECK(camera.x == doctest::Approx(-590.0f));
    CHECK(camera.y == doctest::Approx(-310.0f));
}

TEST_CASE("zoom_camera_at keeps the world point under the cursor") {
    gl::render::Camera camera;
    gl::render::zoom_camera_at(camera, 640.0f, 360.0f, 2.0f, 2760.0f, 2016.0f, 1280, 720,
                               0.25f, 4.0f);
    CHECK(camera.zoom == doctest::Approx(2.0f));
    CHECK(camera.x == doctest::Approx(320.0f));
    CHECK(camera.y == doctest::Approx(180.0f));

    gl::render::zoom_camera_at(camera, 0.0f, 0.0f, 2.0f, 2760.0f, 2016.0f, 1280, 720, 0.25f,
                               4.0f);
    CHECK(camera.zoom == doctest::Approx(4.0f));
    CHECK(camera.x == doctest::Approx(320.0f));
    CHECK(camera.y == doctest::Approx(180.0f));
}

TEST_CASE("zoom_camera_at respects the zoom limits") {
    gl::render::Camera camera;
    camera.zoom = 4.0f;
    gl::render::zoom_camera_at(camera, 640.0f, 360.0f, 2.0f, 10000.0f, 8000.0f, 1280, 720,
                               0.25f, 4.0f);
    CHECK(camera.zoom == doctest::Approx(4.0f));
    CHECK(camera.x == doctest::Approx(0.0f));
    CHECK(camera.y == doctest::Approx(0.0f));

    camera.zoom = 0.25f;
    camera.x = 100.0f;
    camera.y = 100.0f;
    gl::render::zoom_camera_at(camera, 640.0f, 360.0f, 0.5f, 10000.0f, 8000.0f, 1280, 720,
                               0.25f, 4.0f);
    CHECK(camera.zoom == doctest::Approx(0.25f));
    CHECK(camera.x == doctest::Approx(100.0f));
    CHECK(camera.y == doctest::Approx(100.0f));
}

TEST_CASE("clamp_camera accounts for zoom") {
    gl::render::Camera camera;
    camera.zoom = 4.0f;
    camera.x = 5000.0f;
    camera.y = 5000.0f;
    gl::render::clamp_camera(camera, 2760.0f, 2016.0f, 1280, 720);
    CHECK(camera.x == doctest::Approx(2440.0f));
    CHECK(camera.y == doctest::Approx(1836.0f));

    camera.zoom = 0.25f;
    camera.x = 1000.0f;
    camera.y = 1000.0f;
    gl::render::clamp_camera(camera, 2760.0f, 2016.0f, 1280, 720);
    CHECK(camera.x == doctest::Approx((2760.0f - 5120.0f) * 0.5f));
    CHECK(camera.y == doctest::Approx((2016.0f - 2880.0f) * 0.5f));
}
