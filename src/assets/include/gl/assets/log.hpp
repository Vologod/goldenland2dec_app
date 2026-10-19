#pragma once
#include <string_view>
namespace gl {
enum class LogLevel { debug, info, warning, error };
void set_log_level(LogLevel level);
LogLevel log_level();
void log(LogLevel level, std::string_view message);
}
