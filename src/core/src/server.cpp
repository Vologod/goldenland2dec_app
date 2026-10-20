#include "gl/core/server.hpp"
#include "gl/core/command.hpp"
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

std::string join_from(const std::vector<std::string> &tokens, std::size_t first) {
    std::string out;
    for (std::size_t i = first; i < tokens.size(); ++i) {
        if (!out.empty())
            out.push_back(' ');
        out += tokens[i];
    }
    return out;
}
} // namespace

void ServerCore::submit(std::string_view command_line) {
    std::vector<std::string> tokens = split_command_line(command_line);
    if (tokens.empty())
        return;
    pending_.push_back(PendingCommand{std::move(tokens)});
}

void ServerCore::tick(double dt) {
    (void)dt;
    std::vector<PendingCommand> pending;
    pending.swap(pending_);
    for (PendingCommand &command : pending)
        process(std::move(command.tokens));
}

bool ServerCore::paused() const { return paused_; }

bool ServerCore::free_camera() const { return free_camera_; }

std::vector<Event> ServerCore::drain_events() {
    std::vector<Event> events;
    events.swap(events_);
    return events;
}

void ServerCore::process(std::vector<std::string> tokens) {
    if (tokens.empty())
        return;

    const std::string &name = tokens.front();
    const bool has_arg = tokens.size() > 1;

    if (name == "gv_pause") {
        bool value = !paused_;
        if (has_arg) {
            const std::optional<bool> parsed = parse_bool(tokens[1]);
            if (!parsed) {
                events_.push_back(
                    Event{EventType::log, false, "invalid argument for gv_pause: " + tokens[1]});
                return;
            }
            value = *parsed;
        }
        if (value != paused_) {
            paused_ = value;
            events_.push_back(Event{EventType::paused_changed, value, {}});
        }
        return;
    }

    if (name == "gv_free_camera") {
        if (!has_arg) {
            free_camera_ = !free_camera_;
            return;
        }
        const std::optional<bool> parsed = parse_bool(tokens[1]);
        if (!parsed) {
            events_.push_back(
                Event{EventType::log, false, "invalid argument for gv_free_camera: " + tokens[1]});
            return;
        }
        free_camera_ = *parsed;
        return;
    }

    if (name == "cl_username") {
        if (!has_arg) {
            events_.push_back(Event{EventType::log, false, "usage: cl_username <name>"});
            return;
        }
        events_.push_back(
            Event{EventType::config_changed, false, "cl_username " + join_from(tokens, 1)});
        return;
    }

    if (name == "r_windowed") {
        if (!has_arg) {
            events_.push_back(Event{EventType::log, false, "usage: r_windowed <0/1>"});
            return;
        }
        const std::optional<bool> value = parse_bool(tokens[1]);
        if (!value) {
            events_.push_back(
                Event{EventType::log, false, "invalid argument for r_windowed: " + tokens[1]});
            return;
        }
        events_.push_back(
            Event{EventType::config_changed, *value, *value ? "r_windowed 1" : "r_windowed 0"});
        return;
    }

    events_.push_back(Event{EventType::log, false, "unknown command: " + name});
}

void ClientCore::on_event(const Event &event) { events_.push_back(event); }

const std::vector<Event> &ClientCore::events() const { return events_; }

void ClientCore::clear_events() { events_.clear(); }
} // namespace gl::core
