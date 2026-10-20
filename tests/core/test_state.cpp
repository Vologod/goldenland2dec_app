#include "gl/core/state.hpp"
#include <doctest/doctest.h>

using gl::core::AppState;
using gl::core::StateMachine;

TEST_CASE("StateMachine: initial state is boot") {
    const StateMachine machine;
    CHECK(machine.state() == AppState::boot);
}

TEST_CASE("StateMachine: allowed transitions") {
    StateMachine machine;

    CHECK(machine.transition(AppState::menu));
    CHECK(machine.state() == AppState::menu);

    CHECK(machine.transition(AppState::level));
    CHECK(machine.state() == AppState::level);

    CHECK(machine.transition(AppState::level));
    CHECK(machine.state() == AppState::level);

    CHECK(machine.transition(AppState::menu));
    CHECK(machine.state() == AppState::menu);

    CHECK(machine.transition(AppState::exit));
    CHECK(machine.state() == AppState::exit);
}

TEST_CASE("StateMachine: forbidden transitions keep the state") {
    StateMachine machine;

    CHECK_FALSE(machine.transition(AppState::level));
    CHECK(machine.state() == AppState::boot);

    CHECK_FALSE(machine.transition(AppState::boot));
    CHECK(machine.state() == AppState::boot);

    REQUIRE(machine.transition(AppState::menu));
    CHECK_FALSE(machine.transition(AppState::boot));
    CHECK(machine.state() == AppState::menu);
    CHECK_FALSE(machine.transition(AppState::menu));
    CHECK(machine.state() == AppState::menu);
}

TEST_CASE("StateMachine: any state can exit and exit is final") {
    StateMachine from_boot;
    CHECK(from_boot.transition(AppState::exit));
    CHECK(from_boot.state() == AppState::exit);

    StateMachine from_menu;
    REQUIRE(from_menu.transition(AppState::menu));
    CHECK(from_menu.transition(AppState::exit));

    StateMachine from_level;
    REQUIRE(from_level.transition(AppState::menu));
    REQUIRE(from_level.transition(AppState::level));
    CHECK(from_level.transition(AppState::exit));

    CHECK_FALSE(from_boot.transition(AppState::menu));
    CHECK_FALSE(from_boot.transition(AppState::level));
    CHECK_FALSE(from_boot.transition(AppState::boot));
    CHECK_FALSE(from_boot.transition(AppState::exit));
    CHECK(from_boot.state() == AppState::exit);
}
