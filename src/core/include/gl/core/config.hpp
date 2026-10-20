#pragma once
#include "gl/assets/result.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gl::core {
class Config {
public:
    Config() = default;

    static gl::Result<Config> load(const std::filesystem::path& path);
    gl::Result<void> save(const std::filesystem::path& path) const;

    std::optional<std::string> get(std::string_view key) const;
    std::optional<int> get_int(std::string_view key) const;
    std::optional<double> get_float(std::string_view key) const;
    bool get_bool(std::string_view key, bool fallback) const;

    void set(std::string_view key, std::string_view value);
    void set_int(std::string_view key, int value);

    const std::vector<std::string>& raw_lines() const;

private:
    enum class LineKind { blank, comment, bind, setting };

    struct Line {
        LineKind kind = LineKind::blank;
        std::string raw;
        std::string raw_utf8;
        std::string eol;
        std::string key;
        std::string prefix;
        std::string value;
        std::string suffix;
    };

    static void parse_line(Line& line);
    void parse_lines(std::string_view bytes);

    std::vector<Line> lines_;
    std::vector<std::string> raw_lines_;
    std::string append_eol_ = "\r\n";
};
}
