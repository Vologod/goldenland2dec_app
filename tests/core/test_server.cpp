#include "gl/core/server.hpp"
#include <doctest/doctest.h>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using gl::core::ClientCore;
using gl::core::Command;
using gl::core::Event;
using gl::core::EventType;
using gl::core::kDefaultSvFps;
using gl::core::kDefaultSvGameSpeed;
using gl::core::kNominalTickHz;
using gl::core::kTickIntervalMs;
using gl::core::ServerCore;
using gl::core::tick_interval_ms;
using gl::core::ServerSnapshot;

namespace {
bool contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}
} // namespace

TEST_CASE("Event: defaults to a log event with empty payload") {
    const Event event;
    CHECK(event.type == EventType::log);
    CHECK_FALSE(event.flag);
    CHECK(event.key.empty());
    CHECK(event.value.empty());
    CHECK(event.text.empty());
}

TEST_CASE("ServerCore: gv_pause toggles only on tick") {
    ServerCore server;
    CHECK_FALSE(server.paused());

    server.submit("gv_pause");
    CHECK_FALSE(server.paused());
    CHECK(server.drain_events().empty());

    server.tick(0.016);
    CHECK(server.paused());

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::paused_changed);
    CHECK(events[0].flag);
    CHECK(events[0].key.empty());
    CHECK(events[0].value.empty());

    server.submit("gv_pause");
    server.tick(0.016);
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

TEST_CASE("ServerCore: submit(Command) applies without parsing") {
    ServerCore server;

    server.submit(Command{"gv_pause", {"1"}});
    server.submit(Command{"cl_username", {"Ivan"}});
    CHECK_FALSE(server.paused());
    CHECK(server.drain_events().empty());

    server.tick(0.016);
    CHECK(server.paused());

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == EventType::paused_changed);
    CHECK(events[0].flag);
    CHECK(events[1].type == EventType::config_changed);
    CHECK(events[1].key == "cl_username");
    CHECK(events[1].value == "Ivan");
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

TEST_CASE("ServerCore: config setters emit config_changed with key and value") {
    ServerCore server;

    server.submit("cl_username Vasya");
    server.submit("r_windowed 1");
    server.tick(0.016);

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 2);

    CHECK(events[0].type == EventType::config_changed);
    CHECK(events[0].key == "cl_username");
    CHECK(events[0].value == "Vasya");
    CHECK(events[0].text == "Vasya");
    CHECK_FALSE(events[0].flag);

    CHECK(events[1].type == EventType::config_changed);
    CHECK(events[1].key == "r_windowed");
    CHECK(events[1].value == "1");
    CHECK(events[1].text == "1");
    CHECK(events[1].flag);

    server.submit("r_windowed 0");
    server.tick(0.016);
    const std::vector<Event> windowed = server.drain_events();
    REQUIRE(windowed.size() == 1);
    CHECK(windowed[0].key == "r_windowed");
    CHECK(windowed[0].value == "0");
    CHECK_FALSE(windowed[0].flag);
}

TEST_CASE("ServerCore: cl_username keeps the full quoted name as UTF-8") {
    ServerCore server;

    server.submit("cl_username \"Ivan Petrov\"");
    server.tick(0.016);

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::config_changed);
    CHECK(events[0].key == "cl_username");
    CHECK(events[0].value == "Ivan Petrov");
}

TEST_CASE("ServerCore: invalid gv_pause argument logs and changes nothing") {
    ServerCore server;

    server.submit("gv_pause yes");
    server.tick(0.016);

    CHECK_FALSE(server.paused());
    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::log);
    CHECK(contains(events[0].text, "gv_pause"));
    CHECK(contains(events[0].text, "yes"));
}

TEST_CASE("ServerCore: extra arguments log usage and change nothing") {
    ServerCore server;

    server.submit("gv_pause 1 extra");
    server.submit("r_windowed 1 extra");
    server.submit("cl_username Ivan Petrov");
    server.tick(0.016);

    CHECK_FALSE(server.paused());
    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 3);
    for (const Event &event : events) {
        CHECK(event.type == EventType::log);
        CHECK(contains(event.text, "usage:"));
    }
}

TEST_CASE("ServerCore: r_windowed requires a valid 0/1 argument") {
    ServerCore server;

    server.submit("r_windowed");
    server.tick(0.016);
    std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::log);
    CHECK(contains(events[0].text, "usage: r_windowed"));

    server.submit("r_windowed 2");
    server.tick(0.016);
    events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::log);
    CHECK(contains(events[0].text, "invalid argument for r_windowed"));
}

TEST_CASE("ServerCore: r_resolution emits a config change for two positive ints") {
    ServerCore server;

    server.submit("r_resolution 800 600");
    server.tick(0.016);

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::config_changed);
    CHECK(events[0].key == "r_resolution");
    CHECK(events[0].value == "800 600");
    CHECK(events[0].text == "800 600");
    CHECK_FALSE(events[0].flag);
}

TEST_CASE("ServerCore: r_resolution rejects missing, extra and invalid dimensions") {
    ServerCore server;

    server.submit("r_resolution");
    server.submit("r_resolution 800");
    server.submit("r_resolution 800 600 extra");
    server.submit("r_resolution 0 600");
    server.submit("r_resolution 800 -1");
    server.submit("r_resolution wide 600");
    server.tick(0.016);

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 6);
    for (const Event &event : events) {
        CHECK(event.type == EventType::log);
        CHECK(contains(event.text, "r_resolution"));
    }
}

TEST_CASE("ServerCore: cl_username requires a name") {
    ServerCore server;

    server.submit("cl_username");
    server.tick(0.016);

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::log);
    CHECK(contains(events[0].text, "usage: cl_username"));
}

TEST_CASE("ServerCore: unknown command emits a log event") {
    ServerCore server;

    server.submit("does_not_exist arg");
    server.submit(Command{"also_missing", {}});
    server.tick(0.016);

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == EventType::log);
    CHECK(contains(events[0].text, "does_not_exist"));
    CHECK(events[1].type == EventType::log);
    CHECK(contains(events[1].text, "also_missing"));
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
    server.submit(Command{});
    server.tick(0.016);

    CHECK(server.drain_events().empty());
    CHECK_FALSE(server.paused());
}

TEST_CASE("ServerCore: tick interval truncates 1000/sv_fps to whole ms") {
    CHECK(kDefaultSvFps == 35);
    CHECK(kDefaultSvGameSpeed == 0);
    CHECK(kTickIntervalMs == 28);
    CHECK(kNominalTickHz == 35);
    CHECK(tick_interval_ms(35) == 28);
    CHECK(tick_interval_ms(0) == 0);
    CHECK(tick_interval_ms(-1) == 0);
    CHECK(tick_interval_ms(1000) == 1);
    CHECK(tick_interval_ms(1001) == 0);
    CHECK(tick_interval_ms(60) == 16);
}

TEST_CASE("ServerCore: two 16 ms frames cross the 28 ms gate once") {
    ServerCore server;
    CHECK(server.clock_ms() == 0);

    server.tick(0.016);
    CHECK(server.tick_index() == 0);
    CHECK(server.simulation_time_ms() == 0);
    CHECK(server.clock_ms() == 16);

    server.tick(0.016);
    CHECK(server.tick_index() == 1);
    CHECK(server.simulation_time_ms() == 32);
    CHECK(server.clock_ms() == 32);
}

TEST_CASE("ServerCore: a 28 ms frame ticks immediately") {
    ServerCore server;

    server.tick(0.028);
    CHECK(server.tick_index() == 1);
    CHECK(server.simulation_time_ms() == 28);
    CHECK(server.clock_ms() == 28);
}

TEST_CASE("ServerCore: a long frame advances only one tick") {
    ServerCore server;

    server.tick(0.250);
    CHECK(server.tick_index() == 1);
    CHECK(server.simulation_time_ms() == 250);
    CHECK(server.clock_ms() == 250);
}

TEST_CASE("ServerCore: two 28 ms frames advance two ticks") {
    ServerCore server;

    server.tick(0.028);
    server.tick(0.028);
    CHECK(server.tick_index() == 2);
    CHECK(server.simulation_time_ms() == 56);
}

TEST_CASE("ServerCore: a one second frame ticks once and latches the clock") {
    ServerCore server;

    server.tick(1.0);
    CHECK(server.tick_index() == 1);
    CHECK(server.simulation_time_ms() == 1000);
    CHECK(server.clock_ms() == 1000);
}

TEST_CASE("ServerCore: negative and non-finite dt do not advance the clock") {
    ServerCore server;

    server.tick(0.25);
    REQUIRE(server.tick_index() == 1);
    REQUIRE(server.simulation_time_ms() == 250);

    server.tick(-1.0);
    server.tick(std::numeric_limits<double>::quiet_NaN());
    server.tick(std::numeric_limits<double>::infinity());
    server.tick(0.0);

    CHECK(server.tick_index() == 1);
    CHECK(server.simulation_time_ms() == 250);
    CHECK(server.clock_ms() == 250);
}

TEST_CASE("ServerCore: pause advances the clock without ticking or backlog") {
    ServerCore server;

    server.submit("gv_pause 1");
    server.tick(0.016);
    CHECK(server.paused());
    CHECK(server.tick_index() == 0);
    CHECK(server.simulation_time_ms() == 0);
    CHECK(server.clock_ms() == 16);

    server.tick(0.016);
    CHECK(server.tick_index() == 0);
    CHECK(server.simulation_time_ms() == 0);
    CHECK(server.clock_ms() == 32);

    server.submit("gv_pause 0");
    server.tick(0.016);
    CHECK_FALSE(server.paused());
    CHECK(server.tick_index() == 0);
    CHECK(server.simulation_time_ms() == 0);
    CHECK(server.clock_ms() == 48);

    server.tick(0.016);
    CHECK(server.tick_index() == 1);
    CHECK(server.simulation_time_ms() == 64);
    CHECK(server.clock_ms() == 64);
}

TEST_CASE("ServerCore: commands apply before the gate without a tick") {
    ServerCore server;

    server.submit("gv_pause 1");
    CHECK_FALSE(server.paused());
    CHECK(server.tick_index() == 0);

    server.tick(0.0);
    CHECK(server.paused());
    CHECK(server.tick_index() == 0);

    const std::vector<Event> events = server.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::paused_changed);
    CHECK(events[0].flag);
    CHECK(server.drain_events().empty());
}

TEST_CASE("ServerCore: identical dt sequences are deterministic") {
    const auto run = [] {
        ServerCore server;
        const double deltas[] = {0.016, 0.017, 0.033, 0.001, 0.100, 0.250, 0.008, 0.042};
        for (double dt : deltas)
            server.tick(dt);
        return std::pair<std::uint64_t, std::uint64_t>{server.tick_index(),
                                                       server.simulation_time_ms()};
    };

    const auto first = run();
    const auto second = run();
    CHECK(first.first == 5);
    CHECK(first.second == 467);
    CHECK(first.first == second.first);
    CHECK(first.second == second.second);
}

TEST_CASE("ServerCore: snapshot mirrors pause, free camera, tick and time") {
    ServerCore server;

    const ServerSnapshot initial = server.snapshot();
    CHECK_FALSE(initial.paused);
    CHECK_FALSE(initial.free_camera);
    CHECK(initial.tick_index == 0);
    CHECK(initial.simulation_time_ms == 0);

    server.submit("gv_free_camera 1");
    server.tick(0.25);
    server.submit("gv_pause 1");
    server.tick(0.25);

    const ServerSnapshot snap = server.snapshot();
    CHECK(snap.paused);
    CHECK(snap.free_camera);
    CHECK(snap.tick_index == 1);
    CHECK(snap.simulation_time_ms == 250);
    CHECK(snap.tick_index == server.tick_index());
    CHECK(snap.simulation_time_ms == server.simulation_time_ms());
    CHECK(snap.paused == server.paused());
    CHECK(snap.free_camera == server.free_camera());
}

TEST_CASE("ClientCore: default snapshot is zeroed and unpaused") {
    const ClientCore client;

    const ServerSnapshot snap = client.snapshot();
    CHECK_FALSE(snap.paused);
    CHECK_FALSE(snap.free_camera);
    CHECK(snap.tick_index == 0);
    CHECK(snap.simulation_time_ms == 0);
}

TEST_CASE("ClientCore: on_snapshot mirrors paused, free camera, tick and time") {
    ClientCore client;

    ServerSnapshot snap;
    snap.paused = true;
    snap.free_camera = true;
    snap.tick_index = 42;
    snap.simulation_time_ms = 700;
    client.on_snapshot(snap);

    CHECK(client.snapshot().paused);
    CHECK(client.snapshot().free_camera);
    CHECK(client.snapshot().tick_index == 42);
    CHECK(client.snapshot().simulation_time_ms == 700);

    snap.paused = false;
    snap.free_camera = false;
    snap.tick_index = 1;
    snap.simulation_time_ms = 16;
    client.on_snapshot(snap);

    CHECK_FALSE(client.snapshot().paused);
    CHECK_FALSE(client.snapshot().free_camera);
    CHECK(client.snapshot().tick_index == 1);
    CHECK(client.snapshot().simulation_time_ms == 16);
}

TEST_CASE("ClientCore: later server ticks leave the stored snapshot unchanged") {
    ServerCore server;
    ClientCore client;

    client.on_snapshot(server.snapshot());

    server.tick(0.25);
    REQUIRE(server.tick_index() == 1);

    const ServerSnapshot copy = client.snapshot();
    CHECK(copy.tick_index == 0);
    CHECK(copy.simulation_time_ms == 0);

    server.tick(0.25);
    CHECK(server.tick_index() == 2);
    CHECK(client.snapshot().tick_index == 0);
    CHECK(client.snapshot().simulation_time_ms == 0);
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
