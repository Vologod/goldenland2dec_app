#include "gl/assets/person_script.hpp"
#include "gl/assets/cp1251.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace gl::assets {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f' ||
           c == '\0';
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && is_space(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && is_space(text.back()))
        text.remove_suffix(1);
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

void brace_counts(std::string_view line, int &opens, int &closes) {
    bool quoted = false;
    for (char c : line) {
        if (c == '"') {
            quoted = !quoted;
        } else if (!quoted && c == '{') {
            ++opens;
        } else if (!quoted && c == '}') {
            ++closes;
        }
    }
}

bool parse_level(std::string_view value, std::int32_t &out) {
    value = trim(value);
    if (value.empty())
        return false;
    std::int32_t parsed = 0;
    const auto [end, error] =
        std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size())
        return false;
    out = parsed;
    return true;
}

VoidResult parse_line(std::string_view line, PersonDef &person) {
    std::size_t key_end = 0;
    while (key_end < line.size() && !is_space(line[key_end]) && line[key_end] != ':' &&
           line[key_end] != '{' && line[key_end] != '}')
        ++key_end;
    if (key_end == 0)
        return VoidResult{std::monostate{}};

    const std::string_view key = line.substr(0, key_end);
    std::size_t position = key_end;
    while (position < line.size() && is_space(line[position]))
        ++position;
    if (position < line.size() && line[position] == ':')
        ++position;
    while (position < line.size() && is_space(line[position]))
        ++position;

    std::string_view value = trim(line.substr(position));
    if (value.empty())
        return VoidResult{std::monostate{}};

    if (value.front() == '"') {
        const std::size_t close = value.find('"', 1);
        if (close == std::string_view::npos)
            return format_error("person script has an unterminated string");
        if (!trim(value.substr(close + 1)).empty())
            return format_error("person script string has trailing characters");
        value = value.substr(1, close - 1);
    }

    if (key == "res_name") {
        if (value.empty())
            return format_error("person script has an empty res_name");
        person.res_name = std::string(value);
    } else if (key == "level") {
        if (!parse_level(value, person.level))
            return format_error("person script has an invalid level");
    } else {
        person.values[std::string(key)] = std::string(value);
    }
    return VoidResult{std::monostate{}};
}
} // namespace

Result<PersonDef> parse_person_script(std::span<const std::uint8_t> data) {
    try {
        PersonDef person;
        std::string text = cp1251_to_utf8(data);
        std::string_view view = text;
        if (view.starts_with("\xEF\xBB\xBF"))
            view.remove_prefix(3);

        int depth = 0;
        std::size_t position = 0;
        while (position <= view.size()) {
            const std::size_t newline = view.find('\n', position);
            std::string_view line =
                view.substr(position, newline == std::string_view::npos ? std::string_view::npos
                                                                        : newline - position);
            if (newline != std::string_view::npos) {
                position = newline + 1;
            } else {
                position = view.size() + 1;
            }

            line = trim(strip_comment(line));
            if (line.empty())
                continue;

            int opens = 0;
            int closes = 0;
            brace_counts(line, opens, closes);

            if (depth > 0 || opens != 0 || closes != 0) {
                depth += opens - closes;
                if (depth < 0)
                    depth = 0;
                continue;
            }

            auto status = parse_line(line, person);
            if (!status)
                return status.error();
        }

        if (person.res_name.empty())
            return format_error("person script is missing res_name");
        return person;
    } catch (const std::bad_alloc &) {
        return format_error("person script allocation failed");
    }
}
} // namespace gl::assets
