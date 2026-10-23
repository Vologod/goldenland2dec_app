#include "gl/game/msvc_random.hpp"
#include <doctest/doctest.h>

using gl::game::MsvcRandom;

TEST_CASE("MsvcRandom next follows MSVC sequence from seed 1") {
    MsvcRandom rng(1);
    const std::uint32_t expected[] = {41,   18467, 6334,  26500, 19169,
                                      15724, 11478, 29358, 26962, 24464};
    for (std::uint32_t value : expected)
        CHECK(rng.next() == value);
}

TEST_CASE("MsvcRandom fraction is next over 32767") {
    MsvcRandom rng(1);
    CHECK(rng.fraction() == doctest::Approx(41.0 / 32767.0));
    CHECK(rng.next() == 18467u);
}

TEST_CASE("MsvcRandom range matches original formula") {
    MsvcRandom rng(1);
    CHECK(rng.range(-3, 3) == -3);
    CHECK(rng.range(-3, 3) == 1);
    CHECK(rng.range(-3, 3) == -2);
    CHECK(rng.range(-3, 3) == 3);
}

TEST_CASE("MsvcRandom range degenerate bounds do not consume rand") {
    MsvcRandom rng(1);
    CHECK(rng.range(5, 5) == 5);
    CHECK(rng.range(5, 4) == 5);
    CHECK(rng.next() == 41u);
}

TEST_CASE("MsvcRandom range can return max plus one for rand 32767") {
    MsvcRandom probe(4028364353u);
    CHECK(probe.next() == 32767u);
    MsvcRandom rng(4028364353u);
    CHECK(rng.range(0, 1) == 2);
}

TEST_CASE("MsvcRandom coin edges") {
    MsvcRandom rng(1);
    CHECK_FALSE(rng.coin(0.0));
    CHECK_FALSE(rng.coin(-0.5));
    CHECK(rng.coin(1.0));
    CHECK(rng.coin(1.5));
    CHECK(rng.next() == 18467u);

    MsvcRandom edge(4028364353u);
    CHECK_FALSE(edge.coin(1.0));
}
