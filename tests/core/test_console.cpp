#include "gl/core/console.hpp"
#include <doctest/doctest.h>
#include <string>

TEST_CASE("ConsoleBuffer: output lines are capped") {
    gl::core::ConsoleBuffer buffer;
    for (int i = 0; i < 300; ++i)
        buffer.push("line " + std::to_string(i));

    REQUIRE(buffer.lines().size() == 256);
    CHECK(buffer.lines().front() == "line 44");
    CHECK(buffer.lines().back() == "line 299");
}

TEST_CASE("ConsoleBuffer: empty history returns empty strings") {
    gl::core::ConsoleBuffer buffer;
    CHECK(buffer.history_prev().empty());
    CHECK(buffer.history_next().empty());
}

TEST_CASE("ConsoleBuffer: history navigates from the end") {
    gl::core::ConsoleBuffer buffer;
    buffer.add_history("first");
    buffer.add_history("second");
    buffer.add_history("third");

    CHECK(buffer.history_prev() == "third");
    CHECK(buffer.history_prev() == "second");
    CHECK(buffer.history_prev() == "first");
    CHECK(buffer.history_prev().empty());

    CHECK(buffer.history_next() == "second");
    CHECK(buffer.history_next() == "third");
    CHECK(buffer.history_next().empty());
    CHECK(buffer.history_next().empty());
}

TEST_CASE("ConsoleBuffer: adding history resets navigation") {
    gl::core::ConsoleBuffer buffer;
    buffer.add_history("a");
    buffer.add_history("b");
    CHECK(buffer.history_prev() == "b");
    CHECK(buffer.history_prev() == "a");

    buffer.add_history("c");
    CHECK(buffer.history_prev() == "c");
    CHECK(buffer.history_prev() == "b");
    CHECK(buffer.history_prev() == "a");
}

TEST_CASE("ConsoleBuffer: empty and whitespace history is not stored") {
    gl::core::ConsoleBuffer buffer;
    buffer.add_history("");
    buffer.add_history("   ");
    buffer.add_history("\t\r\n");

    CHECK(buffer.history_prev().empty());

    buffer.add_history("real");
    CHECK(buffer.history_prev() == "real");
    CHECK(buffer.history_prev().empty());
}

TEST_CASE("ConsoleBuffer: history is capped at 64 entries") {
    gl::core::ConsoleBuffer buffer;
    for (int i = 0; i < 100; ++i)
        buffer.add_history("h" + std::to_string(i));

    CHECK(buffer.history_prev() == "h99");
    for (int i = 98; i >= 36; --i)
        CHECK(buffer.history_prev() == "h" + std::to_string(i));
    CHECK(buffer.history_prev().empty());

    CHECK(buffer.history_next() == "h37");
}

TEST_CASE("ConsoleBuffer: clear_history_navigation rewinds to the end") {
    gl::core::ConsoleBuffer buffer;
    buffer.add_history("one");
    buffer.add_history("two");
    CHECK(buffer.history_prev() == "two");
    CHECK(buffer.history_prev() == "one");

    buffer.clear_history_navigation();
    CHECK(buffer.history_next().empty());
    CHECK(buffer.history_prev() == "two");
}
