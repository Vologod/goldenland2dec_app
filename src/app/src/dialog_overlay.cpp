#include "gl/app/dialog_overlay.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gl::app {
namespace {

constexpr gl::render::Color kSpeakerColor{255, 232, 160, 255};
constexpr gl::render::Color kTextColor{232, 232, 238, 255};
constexpr gl::render::Color kAnswerColor{210, 214, 222, 255};
constexpr gl::render::Color kSelectedColor{255, 232, 160, 255};
constexpr gl::render::Color kHintColor{110, 112, 122, 255};
constexpr gl::render::Color kPanelColor{12, 14, 20, 230};
constexpr gl::render::Color kSelectedFill{60, 52, 24, 210};
constexpr gl::render::Color kBorderColor{232, 201, 112, 255};

std::vector<std::string> wrap_fallback(std::string_view text, float max_width, float scale) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::string_view paragraph =
            end == std::string_view::npos ? text.substr(start) : text.substr(start, end - start);
        std::string line;
        std::size_t index = 0;
        while (index < paragraph.size()) {
            while (index < paragraph.size() && paragraph[index] == ' ')
                ++index;
            const std::size_t word_start = index;
            while (index < paragraph.size() && paragraph[index] != ' ')
                ++index;
            if (word_start == index)
                continue;
            std::string candidate = line;
            if (!candidate.empty())
                candidate.push_back(' ');
            candidate.append(paragraph.substr(word_start, index - word_start));
            if (!line.empty() && gl::render::text_width(candidate, scale) > max_width) {
                lines.push_back(line);
                line.assign(paragraph.substr(word_start, index - word_start));
            } else {
                line = std::move(candidate);
            }
        }
        lines.push_back(line);
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    return lines;
}

std::vector<std::string> wrap_text(std::string_view text, float max_width, float scale,
                                   const gl::render::Font *font) {
    if (font != nullptr)
        return font->wrap(text, max_width);
    return wrap_fallback(text, max_width, scale);
}

void draw_line(SDL_Renderer *renderer, const gl::render::Font *font, float scale,
               std::string_view text, float x, float y, gl::render::Color color) {
    if (font != nullptr) {
        font->draw_text(renderer, text, x, y, color);
    } else {
        gl::render::draw_text(renderer, text, x, y, scale, color);
    }
}

} // namespace

void DialogOverlay::set_content(std::string speaker, std::string text,
                                std::vector<std::string> answers) {
    speaker_ = std::move(speaker);
    text_ = std::move(text);
    answers_ = std::move(answers);
    selected_ = 0;
    active_ = true;
}

void DialogOverlay::clear() {
    speaker_.clear();
    text_.clear();
    answers_.clear();
    selected_ = 0;
    active_ = false;
}

float DialogOverlay::panel_height() const {
    const float height = static_cast<float>(style_.viewport_height);
    const float requested = height * 0.55f;
    return std::min(std::max(requested, 220.0f), std::max(height - 40.0f, 220.0f));
}

float DialogOverlay::panel_y() const {
    return static_cast<float>(style_.viewport_height) - panel_height();
}

float DialogOverlay::content_bottom() const {
    return std::max(static_cast<float>(style_.viewport_height) - 40.0f, 0.0f);
}

float DialogOverlay::text_width() const {
    return std::max(static_cast<float>(style_.viewport_width) - 2.0f * style_.margin, 40.0f);
}

float DialogOverlay::answers_top() const {
    const float row = std::max(style_.answer_row_height, 1.0f);
    const float bottom = static_cast<float>(style_.viewport_height) - style_.bottom_margin;
    const float top = bottom - static_cast<float>(answers_.size()) * row;
    const float minimum = panel_y() + style_.margin + style_.line_height + style_.line_gap;
    return std::max(top, minimum);
}

SDL_FRect DialogOverlay::answer_rect(std::size_t index) const {
    if (index >= answers_.size())
        return SDL_FRect{};
    const float row = std::max(style_.answer_row_height, 1.0f);
    const float top = answers_top() + static_cast<float>(index) * row;
    const float bottom = std::min(top + row - 2.0f, content_bottom());
    return SDL_FRect{style_.margin, top, text_width(), std::max(bottom - top, 0.0f)};
}

std::optional<std::size_t> DialogOverlay::hit_test(int sx, int sy) const {
    if (!active_)
        return std::nullopt;
    const float x = static_cast<float>(sx);
    const float y = static_cast<float>(sy);
    for (std::size_t index = 0; index < answers_.size(); ++index) {
        const SDL_FRect rect = answer_rect(index);
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h) {
            return index;
        }
    }
    return std::nullopt;
}

void DialogOverlay::move_selection(int delta) {
    if (answers_.empty())
        return;
    const long long count = static_cast<long long>(answers_.size());
    const long long current = static_cast<long long>(selected_);
    const long long next = ((current + delta) % count + count) % count;
    selected_ = static_cast<std::size_t>(next);
}

void DialogOverlay::render(SDL_Renderer *renderer, const gl::render::Font *font) const {
    if (renderer == nullptr || !active_)
        return;

    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    SDL_BlendMode blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &blend);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, kPanelColor.r, kPanelColor.g, kPanelColor.b, kPanelColor.a);
    const SDL_FRect panel{0.0f, panel_y(), static_cast<float>(style_.viewport_width),
                          panel_height()};
    SDL_RenderFillRectF(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, kBorderColor.r, kBorderColor.g, kBorderColor.b,
                           kBorderColor.a);
    SDL_RenderDrawLineF(renderer, panel.x, panel.y, panel.x + panel.w, panel.y);

    const float line_height = std::max(style_.line_height, 1.0f);
    const float text_width_limit = text_width();
    const float content_limit = content_bottom();

    float y = panel_y() + style_.margin;
    if (!speaker_.empty()) {
        draw_line(renderer, font, style_.fallback_text_scale, speaker_, style_.margin, y,
                  kSpeakerColor);
        y += line_height + style_.line_gap;
    }
    if (!text_.empty()) {
        const std::vector<std::string> lines =
            wrap_text(text_, text_width_limit, style_.fallback_text_scale, font);
        for (const std::string &line : lines) {
            if (y + line_height > content_limit)
                break;
            draw_line(renderer, font, style_.fallback_text_scale, line, style_.margin, y,
                      kTextColor);
            y += line_height + style_.line_gap;
        }
    }

    if (answers_.empty()) {
        const float message_min = panel_y() + style_.margin;
        const float message_max = std::max(content_limit - line_height, message_min);
        const float message_y = std::min(std::max(y, message_min), message_max);
        draw_line(renderer, font, style_.fallback_text_scale, "No answers.", style_.margin,
                  message_y, kSelectedColor);
    } else {
        for (std::size_t index = 0; index < answers_.size(); ++index) {
            const SDL_FRect rect = answer_rect(index);
            if (rect.y >= content_limit)
                break;
            const bool selected = selected_ == index;
            if (selected) {
                SDL_SetRenderDrawColor(renderer, kSelectedFill.r, kSelectedFill.g, kSelectedFill.b,
                                       kSelectedFill.a);
                SDL_RenderFillRectF(renderer, &rect);
                SDL_SetRenderDrawColor(renderer, kBorderColor.r, kBorderColor.g, kBorderColor.b,
                                       kBorderColor.a);
                SDL_RenderDrawRectF(renderer, &rect);
            }

            const std::string label = std::to_string(index + 1) + ". " + answers_[index];
            const std::vector<std::string> lines =
                wrap_text(label, std::max(rect.w - 16.0f, 1.0f), style_.fallback_text_scale, font);
            float text_y = rect.y + 2.0f;
            for (const std::string &line : lines) {
                if (text_y + line_height > content_limit)
                    break;
                draw_line(renderer, font, style_.fallback_text_scale, line, rect.x + 8.0f, text_y,
                          selected ? kSelectedColor : kAnswerColor);
                text_y += line_height + 1.0f;
            }
        }
    }

    const std::string_view hint = answers_.empty()
                                      ? "Esc to close"
                                      : "Up/Down or mouse to select, Enter to choose, Esc to close";
    draw_line(renderer, font, style_.fallback_text_scale, hint, style_.margin,
              static_cast<float>(style_.viewport_height) - 28.0f, kHintColor);

    SDL_SetRenderDrawBlendMode(renderer, blend);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

} // namespace gl::app
