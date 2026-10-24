#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/click_actions.hpp>
#include <gl/app/input_gate.hpp>
#include <gl/app/level_screen.hpp>
#include <gl/app/pending_talk.hpp>
#include <gl/app/person_render.hpp>
#include <gl/game/hero.hpp>
#include <gl/game/walk_grid.hpp>
#include <gl/render/camera.hpp>

#include <array>
#include <optional>
#include <vector>

int main(int argc, char** argv) {
    doctest::Context context(argc, argv);
    return context.run();
}

namespace {

gl::assets::PadAnimation make_hit_animation() {
    gl::assets::PadAnimation animation;
    animation.type = gl::assets::PadAnimationType::rt_stay;
    animation.anchor_x = 20;
    animation.anchor_y = 77;
    animation.frame_width = 40;
    animation.frame_height = 60;
    animation.frames_per_row = 12;
    animation.crops.assign(5, {});
    return animation;
}

gl::game::PersonInstance make_hit_person() {
    gl::game::PersonInstance person;
    person.stay_type = gl::assets::PadAnimationType::rt_stay;
    person.pad.push_back(make_hit_animation());
    return person;
}

gl::game::WalkGrid make_grid(std::int32_t width, std::int32_t height) {
    gl::game::WalkGrid grid;
    grid.width = width;
    grid.height = height;
    grid.walkable.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                         1);
    return grid;
}

}

TEST_CASE("pending talk waits while the hero is still pathing") {
    CHECK(gl::app::resolve_pending_talk(false, true) ==
          gl::app::PendingTalkDecision::wait);
}

TEST_CASE("pending talk opens when interaction range is reached") {
    CHECK(gl::app::resolve_pending_talk(true, true) ==
          gl::app::PendingTalkDecision::open);
    CHECK(gl::app::resolve_pending_talk(true, false) ==
          gl::app::PendingTalkDecision::open);
}

TEST_CASE("pending talk is abandoned when the path ends out of range") {
    CHECK(gl::app::resolve_pending_talk(false, false) ==
          gl::app::PendingTalkDecision::abandon);
}

TEST_CASE("pending talk retargets when the npc leaves its clicked tile") {
    CHECK(gl::app::resolve_pending_talk(false, true, true) ==
          gl::app::PendingTalkDecision::retarget);
    CHECK(gl::app::resolve_pending_talk(false, false, true) ==
          gl::app::PendingTalkDecision::retarget);
    CHECK(gl::app::resolve_pending_talk(true, false, true) ==
          gl::app::PendingTalkDecision::open);
}

TEST_CASE("pending talk targets the current simulation tile") {
    gl::game::PersonInstance person;
    person.tile_x = 20;
    person.tile_y = 20;
    gl::game::PersonMovement movement;
    movement.x = gl::game::tile_center_x(3);
    movement.y = gl::game::tile_center_y(4);
    CHECK(gl::app::current_person_tile(movement) == gl::game::Tile{3, 4});
}

TEST_CASE("non-talkable npc clicks are ignored instead of starting a path") {
    CHECK(gl::app::resolve_person_click(false, false) ==
          gl::app::PersonClickAction::ignore);
    CHECK(gl::app::resolve_person_click(false, true) ==
          gl::app::PersonClickAction::ignore);
    CHECK(gl::app::resolve_person_click(true, true) ==
          gl::app::PersonClickAction::open);
    CHECK(gl::app::resolve_person_click(true, false) ==
          gl::app::PersonClickAction::approach);
}

TEST_CASE("level input is disabled while a dialog or save overlay owns input") {
    CHECK(gl::app::level_input_enabled(false, false, false));
    CHECK_FALSE(gl::app::level_input_enabled(true, false, false));
    CHECK_FALSE(gl::app::level_input_enabled(false, true, false));
    CHECK_FALSE(gl::app::level_input_enabled(true, true, false));
    CHECK_FALSE(gl::app::level_input_enabled(false, false, true));
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

TEST_CASE("level screen defaults draw the hero marker") {
    CHECK(gl::app::LevelScreenOptions{}.draw_hero);
}

TEST_CASE("hero_draw_key uses the hero world baseline") {
    gl::game::Hero hero;
    hero.x = 30.0f;
    hero.y = 200.0f;
    const gl::app::PersonDrawKey key = gl::app::hero_draw_key(hero);
    CHECK(key.x == 30.0f);
    CHECK(key.y == 200.0f);
}

TEST_CASE("hero key participates in the body draw order") {
    const std::array<gl::app::PersonDrawKey, 2> keys{{
        {30.0f, 100.0f},
        {30.0f, 300.0f},
    }};
    const std::vector<std::size_t> between =
        gl::app::sort_body_keys(keys, gl::app::PersonDrawKey{30.0f, 200.0f});
    CHECK(between == std::vector<std::size_t>{0, 2, 1});

    const std::array<gl::app::PersonDrawKey, 2> tied{{
        {30.0f, 100.0f},
        {30.0f, 100.0f},
    }};
    const std::vector<std::size_t> stable_tie =
        gl::app::sort_body_keys(tied, gl::app::PersonDrawKey{30.0f, 100.0f});
    CHECK(stable_tie == std::vector<std::size_t>{0, 1, 2});

    const std::vector<std::size_t> without_hero =
        gl::app::sort_body_keys(keys, std::nullopt);
    CHECK(without_hero == std::vector<std::size_t>{0, 1});
}

TEST_CASE("hero_marker_rect is 20 px and follows camera zoom") {
    gl::game::Hero hero;
    hero.x = 100.0f;
    hero.y = 50.0f;

    gl::render::Camera camera;
    camera.x = 10.0f;
    camera.y = 20.0f;
    camera.zoom = 1.0f;
    const gl::app::Rect normal = gl::app::hero_marker_rect(hero, camera);
    CHECK(normal.x == doctest::Approx(80.0f));
    CHECK(normal.y == doctest::Approx(20.0f));
    CHECK(normal.w == doctest::Approx(20.0f));
    CHECK(normal.h == doctest::Approx(20.0f));

    camera.zoom = 2.0f;
    const gl::app::Rect zoomed = gl::app::hero_marker_rect(hero, camera);
    CHECK(zoomed.x == doctest::Approx(160.0f));
    CHECK(zoomed.y == doctest::Approx(40.0f));
    CHECK(zoomed.w == doctest::Approx(40.0f));
    CHECK(zoomed.h == doctest::Approx(40.0f));
}

TEST_CASE("person_hit_rect applies PAD anchors and camera zoom") {
    const gl::game::PersonInstance person = make_hit_person();
    const gl::assets::PadAnimation& animation = person.pad.front();
    gl::game::PersonMovement movement;
    movement.x = 100.0f;
    movement.y = 100.0f;
    movement.direction8 = 0;

    gl::render::Camera camera;
    camera.x = 10.0f;
    camera.y = 20.0f;
    camera.zoom = 2.0f;
    const gl::app::PersonHitRect hit =
        gl::app::person_hit_rect(person, movement, animation, camera);
    REQUIRE(hit.valid);
    CHECK(hit.row == 0);
    CHECK(hit.rect.x == doctest::Approx(140.0f));
    CHECK(hit.rect.y == doctest::Approx(6.0f));
    CHECK(hit.rect.w == doctest::Approx(80.0f));
    CHECK(hit.rect.h == doctest::Approx(120.0f));
}

TEST_CASE("person_hit_rect selects the PAD row from the direction") {
    const gl::game::PersonInstance person = make_hit_person();
    const gl::assets::PadAnimation& animation = person.pad.front();
    gl::game::PersonMovement movement;
    movement.x = 50.0f;
    movement.y = 50.0f;
    movement.direction8 = 4;
    gl::render::Camera camera;

    const gl::app::PersonHitRect hit =
        gl::app::person_hit_rect(person, movement, animation, camera);
    REQUIRE(hit.valid);
    CHECK(hit.row == 4);
}

TEST_CASE("person_hit_rect rejects zero frames and missing rows") {
    const gl::game::PersonInstance person = make_hit_person();
    gl::game::PersonMovement movement;
    gl::render::Camera camera;

    gl::assets::PadAnimation zero = person.pad.front();
    zero.frame_width = 0;
    CHECK_FALSE(gl::app::person_hit_rect(person, movement, zero, camera).valid);

    gl::assets::PadAnimation no_crops = person.pad.front();
    no_crops.crops.clear();
    CHECK_FALSE(gl::app::person_hit_rect(person, movement, no_crops, camera).valid);

    gl::assets::PadAnimation short_rows = person.pad.front();
    short_rows.crops.resize(3);
    movement.direction8 = 4;
    CHECK_FALSE(
        gl::app::person_hit_rect(person, movement, short_rows, camera).valid);

    gl::game::PersonInstance no_pad = person;
    no_pad.pad.clear();
    CHECK_FALSE(
        gl::app::person_hit_rect(no_pad, movement, person.pad.front(), camera).valid);
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

TEST_CASE("hit_test_persons prefers the closer center inside an overlap") {
    const std::vector<gl::app::PersonHitTarget> targets{
        {2, SDL_FRect{0.0f, 0.0f, 40.0f, 40.0f}, 50.0f, 20.0f},
        {5, SDL_FRect{20.0f, 0.0f, 40.0f, 40.0f}, 50.0f, 40.0f},
    };
    const std::optional<std::size_t> nearer_first =
        gl::app::hit_test_persons(targets, 25, 20);
    REQUIRE(nearer_first.has_value());
    CHECK(*nearer_first == 2);

    const std::optional<std::size_t> nearer_second =
        gl::app::hit_test_persons(targets, 35, 20);
    REQUIRE(nearer_second.has_value());
    CHECK(*nearer_second == 5);
}

TEST_CASE("hit_test_persons breaks equal center ties by instance index") {
    const std::vector<gl::app::PersonHitTarget> targets{
        {9, SDL_FRect{0.0f, 0.0f, 40.0f, 40.0f}, 50.0f, 20.0f},
        {4, SDL_FRect{0.0f, 0.0f, 40.0f, 40.0f}, 50.0f, 20.0f},
    };
    const std::optional<std::size_t> hit = gl::app::hit_test_persons(targets, 20, 20);
    REQUIRE(hit.has_value());
    CHECK(*hit == 4);
}

TEST_CASE("hit_test_persons ignores points outside every frame") {
    const std::vector<gl::app::PersonHitTarget> targets{
        {0, SDL_FRect{0.0f, 0.0f, 10.0f, 10.0f}, 10.0f, 5.0f},
    };
    CHECK_FALSE(gl::app::hit_test_persons(targets, 20, 20).has_value());
}

TEST_CASE("world_to_tile rejects tiles outside the walk grid") {
    const gl::game::WalkGrid grid = make_grid(4, 3);
    gl::render::Camera camera;
    gl::game::Tile tile{};

    CHECK(gl::app::world_to_tile(camera, grid, 6, 4, tile));
    CHECK(tile == gl::game::Tile{0, 0});

    CHECK(gl::app::world_to_tile(camera, grid, 42, 22, tile));
    CHECK(tile == gl::game::Tile{3, 2});

    CHECK_FALSE(gl::app::world_to_tile(camera, grid, 0, 0, tile));
    CHECK_FALSE(gl::app::world_to_tile(camera, grid, 54, 4, tile));

    const gl::game::WalkGrid empty;
    CHECK(gl::app::world_to_tile(camera, empty, 6, 4, tile));
    CHECK(tile == gl::game::Tile{0, 0});
}

TEST_CASE("click_target combines a person hit with the underlying tile") {
    const gl::game::WalkGrid grid = make_grid(10, 10);
    gl::render::Camera camera;
    const std::vector<gl::app::PersonHitTarget> targets{
        {7, SDL_FRect{0.0f, 0.0f, 20.0f, 20.0f}, 10.0f, 10.0f},
    };

    const gl::app::ClickTarget hit =
        gl::app::click_target(targets, camera, grid, 10, 10);
    REQUIRE(hit.person.has_value());
    CHECK(*hit.person == 7);
    REQUIRE(hit.tile.has_value());
    CHECK(*hit.tile == gl::game::Tile{0, 1});

    const gl::app::ClickTarget background =
        gl::app::click_target(targets, camera, grid, 46, 22);
    CHECK_FALSE(background.person.has_value());
    REQUIRE(background.tile.has_value());
    CHECK(*background.tile == gl::game::Tile{3, 2});
}
