#include "gl/script/globals.hpp"
#include "gl/assets/cp1251.hpp"
#include "gl/assets/log.hpp"
#include <charconv>
#include <cstddef>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace gl::script {
namespace {
bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

std::string_view trim_left(std::string_view text) {
    while (!text.empty() && is_space(text.front())) text.remove_prefix(1);
    return text;
}

std::string_view trim(std::string_view text) {
    text = trim_left(text);
    while (!text.empty() && is_space(text.back())) text.remove_suffix(1);
    return text;
}

std::string_view strip_comment(std::string_view line) {
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            quoted = !quoted;
        } else if (!quoted && c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            return line.substr(0, i);
        }
    }
    return line;
}

bool next_token(std::string_view& rest, std::string_view& token) {
    rest = trim_left(rest);
    if (rest.empty()) return false;
    std::size_t end = 0;
    while (end < rest.size() && !is_space(rest[end])) ++end;
    token = rest.substr(0, end);
    rest = rest.substr(end);
    return true;
}

std::string decode(std::string_view text) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.data());
    return gl::assets::cp1251_to_utf8(
        std::span<const std::uint8_t>(bytes, text.size()));
}

std::optional<std::int32_t> parse_int(std::string_view text) {
    std::int32_t value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size()) return std::nullopt;
    return value;
}

std::optional<std::uint32_t> parse_uint(std::string_view text) {
    std::uint32_t value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size()) return std::nullopt;
    return value;
}

std::optional<double> parse_double(std::string_view text) {
    double value = 0.0;
    const auto [ptr, ec] =
        std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
    if (ec != std::errc{} || ptr != text.data() + text.size()) return std::nullopt;
    return value;
}

std::string_view first_token(std::string_view text) {
    std::size_t end = 0;
    while (end < text.size() && !is_space(text[end])) ++end;
    return text.substr(0, end);
}

std::string_view unquote(std::string_view text) {
    if (text.empty() || text.front() != '"') return text;
    text.remove_prefix(1);
    const std::size_t close = text.find('"');
    if (close != std::string_view::npos) text = text.substr(0, close);
    return text;
}

void warn_line(std::size_t line_number, std::string_view reason) {
    gl::log(gl::LogLevel::warning,
            "globals: line " + std::to_string(line_number) + ": " + std::string(reason));
}
} // namespace

gl::Result<std::vector<ScriptGlobal>> parse_globals(std::span<const std::uint8_t> data) {
    try {
        std::vector<ScriptGlobal> globals;
        const auto* raw = reinterpret_cast<const char*>(data.data());
        const std::string_view text(raw == nullptr ? "" : raw, data.size());

        std::size_t position = 0;
        if (text.size() >= 3 && static_cast<std::uint8_t>(text[0]) == 0xEF &&
            static_cast<std::uint8_t>(text[1]) == 0xBB &&
            static_cast<std::uint8_t>(text[2]) == 0xBF) {
            position = 3;
        }
        std::size_t line_number = 0;
        while (position <= text.size()) {
            ++line_number;
            std::size_t end = text.find('\n', position);
            if (end == std::string_view::npos) end = text.size();

            std::string_view line = strip_comment(text.substr(position, end - position));
            line = trim(line);

            if (!line.empty()) {
                std::string_view rest = line;
                std::string_view type;
                std::string_view name;
                if (!next_token(rest, type) || !next_token(rest, name)) {
                    warn_line(line_number, "expected 'type name value'");
                } else {
                    rest = trim(rest);
                    if (rest.empty()) {
                        warn_line(line_number, "missing value");
                    } else {
                        ScriptGlobal global;
                        global.type = std::string(type);
                        global.name = decode(name);

                        if (type == "int") {
                            auto value = parse_int(first_token(rest));
                            if (value) {
                                global.value = *value;
                                globals.push_back(std::move(global));
                            } else {
                                warn_line(line_number, "invalid int value");
                            }
                        } else if (type == "DWORD") {
                            auto value = parse_uint(first_token(rest));
                            if (value) {
                                global.value = *value;
                                globals.push_back(std::move(global));
                            } else {
                                warn_line(line_number, "invalid DWORD value");
                            }
                        } else if (type == "float") {
                            auto value = parse_double(first_token(rest));
                            if (value) {
                                global.value = *value;
                                globals.push_back(std::move(global));
                            } else {
                                warn_line(line_number, "invalid float value");
                            }
                        } else if (type == "string") {
                            if (!rest.empty() && rest.front() == '"' &&
                                rest.find('"', 1) == std::string_view::npos) {
                                warn_line(line_number, "unterminated quoted string");
                            }
                            global.value = decode(unquote(rest));
                            globals.push_back(std::move(global));
                        } else {
                            warn_line(line_number, "unknown variable type");
                        }
                    }
                }
            }

            if (end == text.size()) break;
            position = end + 1;
        }

        return globals;
    } catch (const std::bad_alloc&) {
        return gl::Error{gl::ErrorCode::format, "globals allocation failed"};
    }
}
} // namespace gl::script
