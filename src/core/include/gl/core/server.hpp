#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace gl::core {
enum class EventType { paused_changed, config_changed, log };

struct Event {
    EventType type;
    bool flag = false;
    std::string text;
};

class ServerCore {
  public:
    void submit(std::string_view command_line);
    void tick(double dt);
    bool paused() const;
    bool free_camera() const;
    std::vector<Event> drain_events();

  private:
    void process(std::vector<std::string> tokens);

    struct PendingCommand {
        std::vector<std::string> tokens;
    };

    std::vector<PendingCommand> pending_;
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
