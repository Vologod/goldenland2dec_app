#include <doctest/doctest.h>
#include "gl/assets/result.hpp"

TEST_CASE("Result holds a value") {
    gl::Result<int> r(42);
    REQUIRE(r.has_value());
    CHECK(r.value() == 42);
}

TEST_CASE("Result holds an error") {
    gl::Result<int> r(gl::Error{gl::ErrorCode::format, "bad"});
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code == gl::ErrorCode::format);
    CHECK(r.error().message == "bad");
}
