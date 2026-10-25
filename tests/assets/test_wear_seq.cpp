#include "gl/assets/wear_seq.hpp"
#include <cstdint>
#include <doctest/doctest.h>
#include <initializer_list>
#include <vector>

TEST_CASE("parse_wear_seq reads stride, rows and order") {
    std::vector<std::uint8_t> data{12};
    for (int i = 0; i < 24; ++i)
        data.push_back(static_cast<std::uint8_t>(2 + (i % 2)));
    auto seq = gl::assets::parse_wear_seq(data);
    REQUIRE(seq.has_value());
    CHECK(seq->stride == 12);
    CHECK(seq->rows == 2);
    REQUIRE(seq->order.size() == 24);
    CHECK(seq->order[0] == 2);
    CHECK(seq->order[1] == 3);
    CHECK(seq->order[23] == 3);
}

TEST_CASE("parse_wear_seq rejects bad data") {
    CHECK_FALSE(gl::assets::parse_wear_seq({}).has_value());
    std::vector<std::uint8_t> bad_stride{0, 1};
    CHECK_FALSE(gl::assets::parse_wear_seq(bad_stride).has_value());
    std::vector<std::uint8_t> too_big{2, 5, 1};
    CHECK_FALSE(gl::assets::parse_wear_seq(too_big).has_value());
}
