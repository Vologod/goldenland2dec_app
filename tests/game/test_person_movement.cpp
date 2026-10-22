#include "gl/game/person_movement.hpp"
#include <doctest/doctest.h>

using namespace gl::game;

TEST_CASE("route type names") {
    CHECK(parse_route_type("STAY") == RouteType::stay);
    CHECK(parse_route_type("STAY_ROTATE") == RouteType::stay_rotate);
    CHECK(parse_route_type("MOVED") == RouteType::moved);
    CHECK(parse_route_type("MOVED_FLIP") == RouteType::moved_flip);
    CHECK(parse_route_type("RANDOM_RADIUS") == RouteType::random_radius);
    CHECK(parse_route_type("RANDOM") == RouteType::random);
    CHECK(parse_route_type("???") == RouteType::stay);
}

TEST_CASE("16-way direction from velocity") {
    CHECK(direction16_from_velocity(0.0f, 0.0f) == 0);
    CHECK(direction16_from_velocity(0.0f, -1.0f) == 0);
    CHECK(direction16_from_velocity(-0.40f, -0.90f) == 1);
    CHECK(direction16_from_velocity(-0.70f, -0.60f) == 2);
    CHECK(direction16_from_velocity(-0.95f, -0.30f) == 3);
    CHECK(direction16_from_velocity(-1.0f, 0.0f) == 4);
    CHECK(direction16_from_velocity(-0.95f, 0.30f) == 5);
    CHECK(direction16_from_velocity(-0.70f, 0.60f) == 6);
    CHECK(direction16_from_velocity(-0.40f, 0.90f) == 7);
    CHECK(direction16_from_velocity(0.0f, 1.0f) == 8);
    CHECK(direction16_from_velocity(0.40f, 0.90f) == 9);
    CHECK(direction16_from_velocity(0.70f, 0.60f) == 10);
    CHECK(direction16_from_velocity(0.95f, 0.30f) == 11);
    CHECK(direction16_from_velocity(1.0f, 0.0f) == 12);
    CHECK(direction16_from_velocity(0.95f, -0.30f) == 13);
    CHECK(direction16_from_velocity(0.70f, -0.60f) == 14);
    CHECK(direction16_from_velocity(0.40f, -0.90f) == 15);
}

TEST_CASE("16/8 direction mapping") {
    const std::uint8_t to8[16] = {0, 1, 1, 1, 2, 3, 3, 3, 4, 5, 5, 5, 6, 7, 7, 7};
    for (std::uint8_t i = 0; i < 16; ++i)
        CHECK(direction16_to8(i) == to8[i]);
    const std::uint8_t to16[8] = {0, 2, 4, 6, 8, 10, 12, 14};
    for (std::uint8_t i = 0; i < 8; ++i)
        CHECK(direction8_to16(i) == to16[i]);
}

TEST_CASE("isometric speed blend") {
    CHECK(iso_speed(0.0f, 0.0f, 6.0f, 5.0f) == doctest::Approx(5.0f));
    CHECK(iso_speed(1.0f, 0.0f, 6.0f, 5.0f) == doctest::Approx(6.0f));
    CHECK(iso_speed(0.0f, 1.0f, 6.0f, 5.0f) == doctest::Approx(5.0f));
    CHECK(iso_speed(1.0f, 1.0f, 6.0f, 5.0f) == doctest::Approx(5.5f));
}
