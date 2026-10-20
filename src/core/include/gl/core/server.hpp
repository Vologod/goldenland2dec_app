#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace gl::core {
enum class EventType { paused_changed, config_changed, log };

// Event payload convention:
// - paused_changed: flag = new pause state, key/value/text empty.
// - config_changed: key = setting name, value = new value, text = value
//   (flag = bool value for r_windowed; r_resolution carries "<width> <height>";
//   false for other string settings).
// - log: text = message, key/value empty.
struct Event {
    EventType type = EventType::log;
    bool flag = false;
    std::string key;
    std::string value;
    std::string text;
};

struct Command {
    std::string name;
    std::vector<std::string> args;
};

class ServerCore {
  public:
    void submit(Command command);
    void submit(std::string_view command_line);
    void tick(double dt);
    bool paused() const;
    bool free_camera() const;
    std::vector<Event> drain_events();

  private:
    void process(const Command &command);

    std::vector<Command> pending_;
    std::vector<Event> events_;
    bool paused_ = false;
    bool free_camera_ = false;
};

class ClientCore {
  public:
    void on_event(const Event &event);
    const std::vector<Event> &events() const;
    void clear_events();

  private:
    std::vector<Event> events_;
};
} // namespace gl::core
