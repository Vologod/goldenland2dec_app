#include "gl/core/server.hpp"
#include <doctest/doctest.h>
#include <string>
#include <vector>

using gl::core::ClientCore;
using gl::core::Event;
using gl::core::EventType;
using gl::core::ServerCore;

namespace {
bool contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}
} // namespace

TEST_CASE("ServerCore: gv_pause toggles only on tick") {
    ServerCore server;
    CHECK_FALSE(server.paused());

    server.submit("gv_pause");
    CHECK_FALSE(server.paused());
    CHECK(server.drain_events().empty());

    server.tick(1.0 / 60.0);
    CHECK(server.paused());

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::paused_changed);
    CHECK(events[0].flag);

    server.submit("gv_pause");
    server.tick(1.0 / 60.0);
    CHECK_FALSE(server.paused());

    const std::vector<Event> resumed = server.drain_events();
    REQUIRE(resumed.size() == 1);
    CHECK(resumed[0].type == EventType::paused_changed);
    CHECK_FALSE(resumed[0].flag);
}

TEST_CASE("ServerCore: gv_pause explicit set and unchanged value emits nothing") {
    ServerCore server;

    server.submit("gv_pause 1");
    server.tick(0.016);
    CHECK(server.paused());

    std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::paused_changed);
    CHECK(events[0].flag);

    server.submit("gv_pause 1");
    server.tick(0.016);
    CHECK(server.paused());
    CHECK(server.drain_events().empty());

    server.submit("gv_pause 0");
    server.tick(0.016);
    CHECK_FALSE(server.paused());
    events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::paused_changed);
    CHECK_FALSE(events[0].flag);
}

TEST_CASE("ServerCore: gv_free_camera toggles a bool") {
    ServerCore server;
    CHECK_FALSE(server.free_camera());

    server.submit("gv_free_camera");
    server.tick(0.016);
    CHECK(server.free_camera());

    server.submit("gv_free_camera");
    server.tick(0.016);
    CHECK_FALSE(server.free_camera());
}

TEST_CASE("ServerCore: config setters emit config_changed") {
    ServerCore server;

    server.submit("cl_username Vasya");
    server.submit("r_windowed 1");
    server.tick(0.016);

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == EventType::config_changed);
    CHECK(contains(events[0].text, "cl_username"));
    CHECK(contains(events[0].text, "Vasya"));

    CHECK(events[1].type == EventType::config_changed);
    CHECK(contains(events[1].text, "r_windowed"));
    CHECK(events[1].flag);
}

TEST_CASE("ServerCore: unknown command emits a log event") {
    ServerCore server;

    server.submit("does_not_exist arg");
    server.tick(0.016);

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::log);
    CHECK(contains(events[0].text, "does_not_exist"));
}

TEST_CASE("ServerCore: drain_events returns the queue once") {
    ServerCore server;

    server.submit("gv_pause 1");
    server.tick(0.016);
    CHECK(server.drain_events().size() == 1);
    CHECK(server.drain_events().empty());

    server.submit("nope");
    server.tick(0.016);
    CHECK(server.drain_events().size() == 1);
    CHECK(server.drain_events().empty());
}

TEST_CASE("ServerCore: empty input is a no-op") {
    ServerCore server;

    server.submit("");
    server.submit("   ");
    server.tick(0.016);

    CHECK(server.drain_events().empty());
    CHECK_FALSE(server.paused());
}

TEST_CASE("ClientCore: stores events until cleared") {
    ClientCore client;
    CHECK(client.events().empty());

    Event paused;
    paused.type = EventType::paused_changed;
    paused.flag = true;
    client.on_event(paused);

    Event log;
    log.type = EventType::log;
    log.text = "hello";
    client.on_event(log);

    REQUIRE(client.events().size() == 2);
    CHECK(client.events()[0].type == EventType::paused_changed);
    CHECK(client.events()[0].flag);
    CHECK(client.events()[1].type == EventType::log);
    CHECK(client.events()[1].text == "hello");

    client.clear_events();
    CHECK(client.events().empty());
}
