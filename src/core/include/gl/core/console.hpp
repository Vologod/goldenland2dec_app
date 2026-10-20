#pragma once
#include <cstddef>
#include <deque>
#include <string>

namespace gl::core {
class ConsoleBuffer {
  public:
    static constexpr std::size_t max_lines = 256;
    static constexpr std::size_t max_history = 64;

    void push(std::string line);
    const std::deque<std::string>& lines() const;
    void add_history(std::string line);
    std::string history_prev();
    std::string history_next();
    void clear_history_navigation();

  private:
    std::deque<std::string> lines_;
    std::deque<std::string> history_;
    std::size_t history_cursor_ = 0;
};
}
