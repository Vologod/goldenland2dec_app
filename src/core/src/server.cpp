#include "gl/core/server.hpp"
#include "gl/core/command.hpp"
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace gl::core {
namespace {
std::optional<bool> parse_bool(std::string_view value) {
    if (value == "0")
        return false;
    if (value == "1")
        return true;
    return std::nullopt;
}

std::optional<int> parse_positive_int(std::string_view value) {
    if (value.empty())
        return std::nullopt;
    int parsed = 0;
    const char* const end = value.data() + value.size();
    const auto [ptr, error] = std::from_chars(value.data(), end, parsed);
    if (error != std::errc{} || ptr != end || parsed <= 0)
        return std::nullopt;
    return parsed;
}

Event make_log(std::string message) {
    Event event;
    event.type = EventType::log;
    event.text = std::move(message);
    return event;
}

Event make_config_change(std::string key, std::string value, bool flag) {
    Event event;
    event.type = EventType::config_changed;
    event.key = std::move(key);
    event.value = std::move(value);
    event.text = event.value;
    event.flag = flag;
    return event;
}
} // namespace

void ServerCore::submit(Command command) {
    if (command.name.empty())
        return;
    pending_.push_back(std::move(command));
}

void ServerCore::submit(std::string_view command_line) {
    std::vector<std::string> tokens = split_command_line(command_line);
    if (tokens.empty())
        return;

    Command command;
    command.name = std::move(tokens.front());
    tokens.erase(tokens.begin());
    command.args = std::move(tokens);
    submit(std::move(command));
}

void ServerCore::tick(double dt) {
    std::vector<Command> pending;
    pending.swap(pending_);
    for (const Command &command : pending)
        process(command);

    if (paused_) {
        accumulator_ = 0.0;
        return;
    }

    if (dt < 0.0)
        dt = 0.0;
    if (dt > kMaxFrameDt)
        dt = kMaxFrameDt;
    accumulator_ += dt;

    int steps = 0;
    while (accumulator_ >= kFixedDt && steps < kMaxStepsPerTick) {
        ++tick_index_;
        accumulator_ -= kFixedDt;
        ++steps;
    }
    if (steps == kMaxStepsPerTick)
        accumulator_ = 0.0;

    simulation_time_ms_ = tick_index_ * 1000 / static_cast<std::uint64_t>(kTickHz);
}

bool ServerCore::paused() const { return paused_; }

bool ServerCore::free_camera() const { return free_camera_; }

std::uint64_t ServerCore::tick_index() const { return tick_index_; }

std::uint64_t ServerCore::simulation_time_ms() const { return simulation_time_ms_; }

ServerSnapshot ServerCore::snapshot() const {
    ServerSnapshot snap;
    snap.paused = paused_;
    snap.free_camera = free_camera_;
    snap.tick_index = tick_index_;
    snap.simulation_time_ms = simulation_time_ms_;
    return snap;
}

std::vector<Event> ServerCore::drain_events() {
    std::vector<Event> events;
    events.swap(events_);
    return events;
}

void ServerCore::process(const Command &command) {
    const std::string &name = command.name;
    const std::vector<std::string> &args = command.args;

    if (name == "gv_pause") {
        if (args.size() > 1) {
            events_.push_back(make_log("usage: gv_pause [0|1]"));
            return;
        }

        bool value = !paused_;
        if (!args.empty()) {
            const std::optional<bool> parsed = parse_bool(args[0]);
            if (!parsed) {
                events_.push_back(make_log("invalid argument for gv_pause: " + args[0]));
                return;
            }
            value = *parsed;
        }

        if (value != paused_) {
            paused_ = value;
            Event event;
            event.type = EventType::paused_changed;
            event.flag = value;
            events_.push_back(std::move(event));
        }
        return;
    }

    if (name == "gv_free_camera") {
        if (args.empty()) {
            free_camera_ = !free_camera_;
            return;
        }
        const std::optional<bool> parsed = parse_bool(args[0]);
        if (!parsed) {
            events_.push_back(make_log("invalid argument for gv_free_camera: " + args[0]));
            return;
        }
        free_camera_ = *parsed;
        return;
    }

    if (name == "cl_username") {
        if (args.size() != 1) {
            events_.push_back(make_log("usage: cl_username <name>"));
            return;
        }
        events_.push_back(make_config_change("cl_username", args[0], false));
        return;
    }

    if (name == "r_windowed") {
        if (args.size() != 1) {
            events_.push_back(make_log("usage: r_windowed <0/1>"));
            return;
        }
        const std::optional<bool> value = parse_bool(args[0]);
        if (!value) {
            events_.push_back(make_log("invalid argument for r_windowed: " + args[0]));
            return;
        }
        events_.push_back(make_config_change("r_windowed", *value ? "1" : "0", *value));
        return;
    }

    if (name == "r_resolution") {
        if (args.size() != 2) {
            events_.push_back(make_log("usage: r_resolution <width> <height>"));
            return;
        }
        const std::optional<int> width = parse_positive_int(args[0]);
        const std::optional<int> height = parse_positive_int(args[1]);
        if (!width || !height) {
            events_.push_back(make_log("invalid argument for r_resolution: " + args[0] + " " +
                                       args[1]));
            return;
        }
        events_.push_back(make_config_change("r_resolution",
                                             std::to_string(*width) + " " +
                                                 std::to_string(*height),
                                             false));
        return;
    }

    events_.push_back(make_log("unknown command: " + name));
}

void ClientCore::on_event(const Event &event) { events_.push_back(event); }

const std::vector<Event> &ClientCore::events() const { return events_; }

void ClientCore::clear_events() { events_.clear(); }
} // namespace gl::core
