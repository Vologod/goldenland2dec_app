#pragma once
#include "gl/assets/result.hpp"
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gl::core {
using Args = std::span<const std::string_view>;
using Handler = std::function<gl::Result<std::string>(Args)>;

struct CommandInfo {
    std::string name;
    std::string help;
    Handler handler;
};

class CommandRegistry {
  public:
    void add(std::string name, std::string help, Handler handler);
    gl::Result<std::string> execute(std::string_view line) const;
    std::string help() const;
    std::optional<std::string> complete(std::string_view prefix) const;
    const std::vector<CommandInfo>& commands() const;

  private:
    std::vector<CommandInfo> commands_;
};

std::vector<std::string> split_command_line(std::string_view line);
}
