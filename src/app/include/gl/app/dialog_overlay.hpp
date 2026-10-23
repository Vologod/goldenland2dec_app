#pragma once

#include <gl/render/font.hpp>
#include <gl/render/text.hpp>

#include <SDL.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gl::app {

struct DialogOverlayStyle {
    int viewport_width = 1280;
    int viewport_height = 720;
    float margin = 32.0f;
    float bottom_margin = 48.0f;
    float line_height = 24.0f;
    float line_gap = 2.0f;
    float answer_gap = 8.0f;
    float answer_row_height = 36.0f;
    float fallback_text_scale = 2.0f;
};

class DialogOverlay {
  public:
    void set_content(std::string speaker, std::string text, std::vector<std::string> answers);
    void clear();

    bool active() const { return active_; }

    void set_style(const DialogOverlayStyle &style) { style_ = style; }
    const DialogOverlayStyle &style() const { return style_; }

    std::optional<std::size_t> hit_test(int sx, int sy) const;
    SDL_FRect answer_rect(std::size_t index) const;

    void render(SDL_Renderer *renderer, const gl::render::Font *font = nullptr) const;

    std::size_t selected() const { return selected_; }
    void move_selection(int delta);

  private:
    float panel_height() const;
    float panel_y() const;
    float content_bottom() const;
    float answers_top() const;
    float text_width() const;

    DialogOverlayStyle style_{};
    std::string speaker_;
    std::string text_;
    std::vector<std::string> answers_;
    std::size_t selected_ = 0;
    bool active_ = false;
};

} // namespace gl::app
