#include "gl/assets/sef.hpp"
#include <charconv>
#include <cstddef>
#include <new>
#include <string>
#include <string_view>
#include <system_error>

namespace gl::assets {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && is_space(text.front())) text.remove_prefix(1);
    while (!text.empty() && is_space(text.back())) text.remove_suffix(1);
    return text;
}

std::string_view strip_comment(std::string_view line) {
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '"') {
            quoted = !quoted;
        } else if (!quoted && line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            return line.substr(0, i);
        }
    }
    return line;
}

std::string ascii_lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

bool parse_int(std::string_view value, int& out) {
    value = trim(value);
    if (value.empty()) return false;
    int parsed = 0;
    const auto [end, error] =
        std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size()) return false;
    out = parsed;
    return true;
}

Result<int> read_quoted(std::string_view value) {
    if (value.size() < 2 || value.front() != '"')
        return format_error("SEF pack value is not a quoted string");
    const std::size_t close = value.find('"', 1);
    if (close == std::string_view::npos)
        return format_error("SEF pack value has no closing quote");
    if (close + 1 != value.size())
        return format_error("SEF pack value has trailing characters");
    return static_cast<int>(close);
}
}

Result<SefData> parse_sef(std::span<const std::uint8_t> data) {
    try {
        SefData sef;
        bool found_pack = false;
        std::string_view text(reinterpret_cast<const char*>(data.data()), data.size());
        if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);

        std::size_t position = 0;
        std::size_t line_number = 0;
        while (position <= text.size()) {
            ++line_number;
            const std::size_t newline = text.find('\n', position);
            std::string_view line =
                text.substr(position, newline == std::string_view::npos ? std::string_view::npos
                                                                       : newline - position);
            if (newline != std::string_view::npos) {
                position = newline + 1;
            } else {
                position = text.size() + 1;
            }

            line = trim(strip_comment(line));
            if (line.empty()) continue;

            std::size_t key_end = 0;
            while (key_end < line.size() && line[key_end] != ':' && !is_space(line[key_end]))
                ++key_end;
            std::string_view key = line.substr(0, key_end);
            std::size_t colon = key_end;
            while (colon < line.size() && is_space(line[colon])) ++colon;
            if (colon >= line.size() || line[colon] != ':') continue;
            const std::string_view value = trim(line.substr(colon + 1));

            if (key == "pack") {
                auto close = read_quoted(value);
                if (!close.has_value())
                    return format_error(close.error().message + " on line " +
                                        std::to_string(line_number));
                sef.pack = ascii_lower(value.substr(1, static_cast<std::size_t>(close.value() - 1)));
                found_pack = true;
            } else if (key == "internal_location" || key == "exit_to_global_map" ||
                       key == "exit_to_globalmap") {
                int number = 0;
                if (!parse_int(value, number))
                    return format_error("bad SEF " + std::string(key) + " on line " +
                                        std::to_string(line_number));
                if (key == "internal_location")
                    sef.internal_location = number != 0;
                else
                    sef.exit_to_global_map = number != 0;
            } else if (key == "weather") {
                int number = 0;
                if (!parse_int(value, number))
                    return format_error("bad SEF weather on line " +
                                        std::to_string(line_number));
                sef.weather = number;
            }
        }

        if (!found_pack) return format_error("SEF is missing the pack field");
        return sef;
    } catch (const std::bad_alloc&) {
        return format_error("SEF allocation failed");
    }
}
}
