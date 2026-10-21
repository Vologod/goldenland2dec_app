#pragma once

#include "gl/assets/result.hpp"
#include "gl/render/text.hpp"

#include <SDL.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gl::render {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

class Font {
public:
    struct Impl;

    // pixel_height must be in [8, 256]; invalid values return ErrorCode::argument.
    static Result<Font> load_from_file(SDL_Renderer* renderer, const std::filesystem::path& path,
                                       float pixel_height);

    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    void draw_text(SDL_Renderer* renderer, std::string_view utf8, float x, float y,
                   Color color) const;
    Vec2 measure(std::string_view utf8) const;
    float line_height() const;
    bool has_glyph(char32_t codepoint) const;
    // Greedy wrap on spaces: leading/trailing spaces are dropped, space runs
    // collapse, tabs expand to four spaces, \r and other C0/C1 controls are
    // ignored, \n is preserved, and whitespace-only paragraphs become empty
    // lines. Overlong words are split by codepoint.
    std::vector<std::string> wrap(std::string_view utf8, float max_width) const;

private:
    explicit Font(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

std::optional<std::filesystem::path> find_system_font();

}
