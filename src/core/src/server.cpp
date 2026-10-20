#include "gl/core/server.hpp"
#include "gl/core/command.hpp"
#include <cstddef>
#include <optional>
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
    (void)dt;
    std::vector<Command> pending;
    pending.swap(pending_);
    for (const Command &command : pending)
        process(command);
}

bool ServerCore::paused() const { return paused_; }

bool ServerCore::free_camera() const { return free_camera_; }

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

    events_.push_back(make_log("unknown command: " + name));
}

void ClientCore::on_event(const Event &event) { events_.push_back(event); }

const std::vector<Event> &ClientCore::events() const { return events_; }

void ClientCore::clear_events() { events_.clear(); }
} // namespace gl::core
