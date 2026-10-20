#include "gl/core/config.hpp"
#include "gl/assets/cp1251.hpp"
#include "gl/assets/log.hpp"
#include <charconv>
#include <cstdint>
#include <fstream>
#include <span>
#include <system_error>
#include <utility>

namespace gl::core {
namespace {
std::string to_utf8(std::string_view bytes) {
    return gl::assets::cp1251_to_utf8(
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(bytes.data()),
                                      bytes.size()));
}

}

void Config::parse_line(Line& line) {
    line.kind = LineKind::blank;
    line.raw_utf8 = to_utf8(line.raw);
    line.key.clear();
    line.prefix.clear();
    line.value.clear();
    line.suffix.clear();

    const std::string_view raw(line.raw);
    const std::size_t first = raw.find_first_not_of(" \t");
    if (first == std::string_view::npos) return;
    if (raw.substr(first, 2) == "//") {
        line.kind = LineKind::comment;
        return;
    }

    const std::size_t key_end = raw.find_first_of(" \t", first);
    const std::string_view token =
        key_end == std::string_view::npos ? raw.substr(first) : raw.substr(first, key_end - first);
    if (token == "bind") {
        line.kind = LineKind::bind;
        return;
    }

    line.kind = LineKind::setting;
    line.key = to_utf8(token);
    if (key_end == std::string_view::npos) {
        line.prefix = line.raw;
        return;
    }

    const std::size_t value_start = raw.find_first_not_of(" \t", key_end);
    if (value_start == std::string_view::npos) {
        line.prefix = line.raw;
        return;
    }

    line.prefix = line.raw.substr(0, value_start);
    std::size_t comment = value_start;
    while ((comment = raw.find("//", comment)) != std::string_view::npos) {
        if (comment == 0 || raw[comment - 1] == ' ' || raw[comment - 1] == '\t') break;
        comment += 2;
    }
    std::size_t value_end = comment == std::string_view::npos ? raw.size() : comment;
    while (value_end > value_start && (raw[value_end - 1] == ' ' || raw[value_end - 1] == '\t')) {
        --value_end;
    }
    line.value = line.raw.substr(value_start, value_end - value_start);
    line.suffix = line.raw.substr(value_end);
}

gl::Result<Config> Config::load(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (!exists && !ec) {
            gl::log(gl::LogLevel::warning,
                    "config not found, using defaults: " + path.string());
            return Config{};
        }
        return Error{ErrorCode::io, "cannot open config: " + path.string()};
    }

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) return Error{ErrorCode::io, "cannot size config: " + path.string()};
    in.seekg(0, std::ios::beg);
    if (!in) return Error{ErrorCode::io, "cannot seek config: " + path.string()};

    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!bytes.empty()) {
        in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    if (!in) return Error{ErrorCode::io, "cannot read config: " + path.string()};

    Config config;
    config.parse_lines(bytes);
    return config;
}

gl::Result<void> Config::save(const std::filesystem::path& path) const {
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) return Error{ErrorCode::io, "cannot create config directory: " + parent.string()};
    }

    std::filesystem::path temp_path = path;
    temp_path += ".tmp";
    std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return Error{ErrorCode::io, "cannot open config for writing: " + temp_path.string()};
    }
    for (const Line& line : lines_) {
        out.write(line.raw.data(), static_cast<std::streamsize>(line.raw.size()));
        out.write(line.eol.data(), static_cast<std::streamsize>(line.eol.size()));
    }
    out.close();
    if (!out) {
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        return Error{ErrorCode::io, "cannot write config: " + temp_path.string()};
    }

    std::error_code rename_error;
    std::filesystem::rename(temp_path, path, rename_error);
    if (rename_error) {
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        return Error{ErrorCode::io, "cannot replace config: " + path.string()};
    }
    return gl::Result<void>{};
}

std::optional<std::string> Config::get(std::string_view key) const {
    for (const Line& line : lines_) {
        if (line.kind == LineKind::setting && line.key == key) return to_utf8(line.value);
    }
    return std::nullopt;
}

std::optional<int> Config::get_int(std::string_view key) const {
    const std::optional<std::string> value = get(key);
    if (!value || value->empty()) return std::nullopt;
    int out = 0;
    const char* begin = value->data();
    const char* end = begin + value->size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    if (ec != std::errc{} || ptr != end) return std::nullopt;
    return out;
}

std::optional<double> Config::get_float(std::string_view key) const {
    const std::optional<std::string> value = get(key);
    if (!value || value->empty()) return std::nullopt;
    double out = 0.0;
    const char* begin = value->data();
    const char* end = begin + value->size();
    const auto [ptr, ec] = std::from_chars(begin, end, out, std::chars_format::general);
    if (ec != std::errc{} || ptr != end) return std::nullopt;
    return out;
}

bool Config::get_bool(std::string_view key, bool fallback) const {
    const std::optional<std::string> value = get(key);
    if (!value) return fallback;
    if (*value == "1") return true;
    if (*value == "0") return false;
    return fallback;
}

void Config::set(std::string_view key, std::string_view value) {
    if (value.find_first_of("\r\n") != std::string_view::npos) {
        gl::log(gl::LogLevel::warning,
                "config: ignoring value with newline for key '" + std::string(key) + "'");
        return;
    }
    const std::string cp_value = gl::assets::utf8_to_cp1251(value);
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        Line& line = lines_[i];
        if (line.kind != LineKind::setting || line.key != key) continue;
        if (!line.prefix.empty() && line.prefix.back() != ' ' && line.prefix.back() != '\t') {
            line.prefix.push_back(' ');
        }
        line.value = cp_value;
        line.raw = line.prefix + line.value + line.suffix;
        line.raw_utf8 = to_utf8(line.raw);
        raw_lines_[i] = line.raw_utf8;
        return;
    }

    if (!lines_.empty() && lines_.back().eol.empty()) lines_.back().eol = append_eol_;

    Line line;
    line.kind = LineKind::setting;
    line.key = std::string(key);
    line.prefix = gl::assets::utf8_to_cp1251(key) + " ";
    line.value = cp_value;
    line.raw = line.prefix + line.value;
    line.eol = append_eol_;
    line.raw_utf8 = to_utf8(line.raw);
    raw_lines_.push_back(line.raw_utf8);
    lines_.push_back(std::move(line));
}

void Config::set_int(std::string_view key, int value) { set(key, std::to_string(value)); }

const std::vector<std::string>& Config::raw_lines() const { return raw_lines_; }

void Config::parse_lines(std::string_view bytes) {
    lines_.clear();
    raw_lines_.clear();
    std::size_t start = 0;
    while (start < bytes.size()) {
        Line line;
        const std::size_t newline = bytes.find('\n', start);
        if (newline == std::string_view::npos) {
            line.raw = std::string(bytes.substr(start));
            start = bytes.size();
        } else {
            std::size_t end = newline;
            if (end > start && bytes[end - 1] == '\r') {
                line.eol = "\r\n";
                --end;
            } else {
                line.eol = "\n";
            }
            line.raw = std::string(bytes.substr(start, end - start));
            start = newline + 1;
        }
        parse_line(line);
        lines_.push_back(std::move(line));
    }

    std::size_t crlf_count = 0;
    std::size_t lf_count = 0;
    for (const Line& line : lines_) {
        if (line.eol == "\r\n") {
            ++crlf_count;
        } else if (line.eol == "\n") {
            ++lf_count;
        }
        raw_lines_.push_back(line.raw_utf8);
    }
    append_eol_ = lf_count > crlf_count ? "\n" : "\r\n";
}
}
