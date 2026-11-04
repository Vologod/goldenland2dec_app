#include "gl/assets/herowear.hpp"
#include "gl/assets/cp1251.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

bool parse_quoted(std::string_view& text, std::string& out) {
    text = trim(text);
    if (text.empty() || text.front() != '"')
        return false;
    const std::size_t close = text.find('"', 1);
    if (close == std::string_view::npos)
        return false;
    out = std::string(text.substr(1, close - 1));
    text = trim(text.substr(close + 1));
    return true;
}

bool parse_quoted_list(std::string_view text, std::vector<std::string>& out) {
    while (!text.empty()) {
        std::string value;
        if (!parse_quoted(text, value))
            return false;
        out.push_back(std::move(value));
    }
    return true;
}
} // namespace

const HeroWearEntry* HeroWearCatalog::find(std::string_view tech) const {
    for (const HeroWearEntry& entry : entries)
        if (entry.tech == tech)
            return &entry;
    return nullptr;
}

Result<HeroWearCatalog> parse_herowear(std::span<const std::uint8_t> data) {
    HeroWearCatalog catalog;
    std::string text = cp1251_to_utf8(data);
    std::string_view view = text;
    if (view.starts_with("\xEF\xBB\xBF"))
        view.remove_prefix(3);

    std::string category;
    std::string group;
    std::string base_set;
    std::map<std::pair<std::string, std::string>, std::string> groups;

    std::size_t position = 0;
    while (position <= view.size()) {
        const std::size_t newline = view.find('\n', position);
        std::string_view line =
            view.substr(position, newline == std::string_view::npos ? std::string_view::npos
                                                                    : newline - position);
        if (newline == std::string_view::npos)
            position = view.size() + 1;
        else
            position = newline + 1;

        line = trim(strip_comment(line));
        if (line.empty())
            continue;

        std::size_t key_end = 0;
        while (key_end < line.size() && !is_space(line[key_end]))
            ++key_end;
        const std::string_view key = line.substr(0, key_end);
        const std::string_view value = trim(line.substr(key_end));

        if (key == "category") {
            std::string_view rest = value;
            std::size_t name_end = 0;
            while (name_end < rest.size() && !is_space(rest[name_end]))
                ++name_end;
            if (name_end == 0)
                return format_error("herowear category is empty");
            category = std::string(rest.substr(0, name_end));
            group.clear();
            base_set.clear();
        } else if (key == "group") {
            if (category.empty())
                return format_error("herowear group before category");
            std::vector<std::string> args;
            if (!parse_quoted_list(value, args) || args.empty() || args.size() > 2)
                return format_error("herowear group has invalid arguments");
            group = std::move(args[0]);
            const std::string requested_base =
                category == "weapon" && args.size() == 2 ? args[1] : std::string();
            auto found = groups.find({category, group});
            if (found == groups.end()) {
                base_set = requested_base;
                groups.emplace(std::make_pair(category, group), base_set);
            } else {
                base_set = found->second;
            }
        } else if (key == "item") {
            if (category.empty() || group.empty())
                return format_error("herowear item outside a group");
            std::vector<std::string> args;
            if (!parse_quoted_list(value, args) || args.size() != 1)
                return format_error("herowear item has invalid arguments");
            catalog.entries.push_back(
                HeroWearEntry{std::move(args[0]), category, group, base_set});
        } else {
            return format_error("herowear has an unknown directive: " + std::string(key));
        }
    }
    return catalog;
}
} // namespace gl::assets
