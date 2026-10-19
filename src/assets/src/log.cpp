#include "gl/assets/log.hpp"
#include <climits>
#include <cstddef>
#include <cstdio>
namespace gl {
static LogLevel g_level = LogLevel::info;
void set_log_level(LogLevel level) { g_level = level; }
LogLevel log_level() { return g_level; }
void log(LogLevel level, std::string_view message) {
    if (level < g_level) return;
    const char* name = level == LogLevel::debug ? "debug" :
                       level == LogLevel::info ? "info" :
                       level == LogLevel::warning ? "warning" : "error";
    if (message.data() == nullptr) {
        std::fprintf(stderr, "[%s] \n", name);
        return;
    }
    const std::size_t size = message.size();
    const int precision = size > static_cast<std::size_t>(INT_MAX)
                              ? INT_MAX
                              : static_cast<int>(size);
    std::fprintf(stderr, "[%s] %.*s\n", name, precision, message.data());
}
}
