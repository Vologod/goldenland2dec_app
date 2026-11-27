#include <doctest/doctest.h>

#include "gl/game/person_runtime.hpp"

TEST_CASE("PersonRuntime appends effects and writes raw parameters") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);

    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x40, 0, -1};
    runtime.add_effect("Hero", effect);
    REQUIRE(runtime.effects("Hero").size() == 1);
    CHECK(runtime.effects("Hero")[0].words[0] == 9);
    CHECK(runtime.effects("Hero")[0].words[1] == -3);

    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::reputation, 40.0);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::reputation) ==
          doctest::Approx(40.0));

    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::strength, 12.0);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::strength) ==
          doctest::Approx(12.0));

    runtime.add_effect("missing", effect);
    runtime.set_raw_parameter("missing", gl::script::PersonParameter::strength, 5.0);
}
