#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/level_screen.hpp>
#include <gl/app/person_render.hpp>
#include <gl/render/camera.hpp>

#include <array>
#include <vector>

int main(int argc, char** argv) {
    doctest::Context context(argc, argv);
    return context.run();
}

TEST_CASE("screen_to_tile maps camera and zoom") {
    gl::render::Camera camera;
    camera.x = 120.0f;
    camera.y = 90.0f;
    camera.zoom = 1.0f;
    const gl::game::Tile t = gl::app::screen_to_tile(camera, 0, 0);
    CHECK(t.x == 10);
    CHECK(t.y == 10);
}

TEST_CASE("screen_to_tile applies camera zoom") {
    gl::render::Camera camera;
    camera.x = -24.0f;
    camera.y = -18.0f;
    camera.zoom = 2.0f;
    const gl::game::Tile t = gl::app::screen_to_tile(camera, 240, 180);
    CHECK(t.x == 8);
    CHECK(t.y == 8);
}

TEST_CASE("hero marker key sorts between persons by baseline") {
    const std::array<gl::app::PersonDrawKey, 3> keys{{
        {30.0f, 100.0f},
        {30.0f, 300.0f},
        {30.0f, 200.0f},
    }};
    const std::vector<std::size_t> order = gl::app::sort_person_indices(keys);
    CHECK(order == std::vector<std::size_t>{0, 2, 1});
}

TEST_CASE("hit_test_persons returns the front-most overlapping target") {
    const std::vector<gl::app::PersonHitTarget> targets{
        {3, SDL_FRect{0.0f, 0.0f, 50.0f, 100.0f}, 100.0f, 25.0f},
        {7, SDL_FRect{10.0f, 0.0f, 50.0f, 120.0f}, 120.0f, 35.0f},
    };
    const std::optional<std::size_t> hit = gl::app::hit_test_persons(targets, 20, 40);
    REQUIRE(hit.has_value());
    CHECK(*hit == 7);
}

TEST_CASE("hit_test_persons prefers the closer center for equal baselines") {
    const std::vector<gl::app::PersonHitTarget> targets{
        {2, SDL_FRect{0.0f, 0.0f, 40.0f, 40.0f}, 50.0f, 20.0f},
        {5, SDL_FRect{20.0f, 0.0f, 40.0f, 40.0f}, 50.0f, 40.0f},
    };
    const std::optional<std::size_t> nearer_first =
        gl::app::hit_test_persons(targets, 15, 20);
    REQUIRE(nearer_first.has_value());
    CHECK(*nearer_first == 2);

    const std::optional<std::size_t> nearer_second =
        gl::app::hit_test_persons(targets, 45, 20);
    REQUIRE(nearer_second.has_value());
    CHECK(*nearer_second == 5);
}

TEST_CASE("hit_test_persons ignores points outside every frame") {
    const std::vector<gl::app::PersonHitTarget> targets{
        {0, SDL_FRect{0.0f, 0.0f, 10.0f, 10.0f}, 10.0f, 5.0f},
    };
    CHECK_FALSE(gl::app::hit_test_persons(targets, 20, 20).has_value());
}
