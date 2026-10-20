#include "gl/core/console.hpp"
#include <string_view>
#include <utility>

namespace gl::core {
namespace {
bool is_blank(std::string_view text) {
    for (const char c : text) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
    }
    return true;
}
}

void ConsoleBuffer::push(std::string line) {
    lines_.push_back(std::move(line));
    while (lines_.size() > max_lines)
        lines_.pop_front();
}

const std::deque<std::string>& ConsoleBuffer::lines() const { return lines_; }

void ConsoleBuffer::add_history(std::string line) {
    if (is_blank(line)) return;
    history_.push_back(std::move(line));
    while (history_.size() > max_history)
        history_.pop_front();
    history_cursor_ = history_.size();
}

std::string ConsoleBuffer::history_prev() {
    if (history_.empty() || history_cursor_ == 0)
        return {};
    if (history_cursor_ > history_.size())
        history_cursor_ = history_.size();
    --history_cursor_;
    return history_[history_cursor_];
}

std::string ConsoleBuffer::history_next() {
    if (history_.empty() || history_cursor_ >= history_.size()) {
        history_cursor_ = history_.size();
        return {};
    }
    ++history_cursor_;
    if (history_cursor_ >= history_.size()) {
        history_cursor_ = history_.size();
        return {};
    }
    return history_[history_cursor_];
}

void ConsoleBuffer::clear_history_navigation() { history_cursor_ = history_.size(); }
}
