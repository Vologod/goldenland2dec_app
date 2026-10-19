#include "gl/assets/log.hpp"
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
    std::fprintf(stderr, "[%s] %.*s\n", name, static_cast<int>(message.size()), message.data());
}
}
