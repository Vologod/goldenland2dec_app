#include "gl/app/diary_overlay.hpp"
#include "gl/assets/image.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/render/text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace gl::app {
namespace {
constexpr std::string_view kBackground = "engineres/diary/main.bmp";
constexpr std::string_view kPageStrip = "engineres/diary/page1.bmp";
constexpr std::string_view kClosePath = "engineres/diary/but/close_l.bmp";
constexpr std::string_view kClosePressedPath = "engineres/diary/but/close_p.bmp";
constexpr std::array<std::string_view, 4> kArrowPressed{
    "engineres/diary/but/1.bmp", "engineres/diary/but/2.bmp",
    "engineres/diary/but/3.bmp", "engineres/diary/but/4.bmp"};
constexpr std::array<std::string_view, 5> kTabLabels{
    "Сюжетные", "Несюжетные", "Биография", "Создания", "История"};
constexpr std::array<float, 5> kTabX{32.0f, 220.0f, 410.0f, 610.0f, 791.0f};
constexpr float kTabWidth = 178.0f;
constexpr float kTabHeight = 44.0f;
constexpr std::size_t kWheelLines = 3;
constexpr std::size_t kPageLines = 8;
} // namespace

bool DiaryRect::contains(float px, float py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
}

bool DiaryOverlay::open(const gl::assets::GameVfs &vfs,
                        std::span<const gl::game::DiaryRow> rows) {
    vfs_ = &vfs;
    rows_.assign(rows.begin(), rows.end());
    selected_ = 0;
    list_scroll_ = 0;
    detail_scroll_ = 0;
    close_hover_ = false;
    pressed_ = Pressed::none;
    visible_ = true;
    update_transform(1024, 768);
    reset_texture_cache(nullptr);
    return true;
}

void DiaryOverlay::close() {
    visible_ = false;
    pressed_ = Pressed::none;
}

void DiaryOverlay::release() { pressed_ = Pressed::none; }

void DiaryOverlay::update_transform(int screen_w, int screen_h) {
    if (screen_w <= 0 || screen_h <= 0) {
        scale_ = 1.0f;
        offset_x_ = 0.0f;
        offset_y_ = 0.0f;
        screen_w_ = 1024;
        screen_h_ = 768;
        return;
    }
    screen_w_ = screen_w;
    screen_h_ = screen_h;
    scale_ = std::min(static_cast<float>(screen_w) / kCanvasWidth,
                      static_cast<float>(screen_h) / kCanvasHeight);
    if (scale_ <= 0.0f) scale_ = 1.0f;
    offset_x_ = (static_cast<float>(screen_w) - kCanvasWidth * scale_) * 0.5f;
    offset_y_ = (static_cast<float>(screen_h) - kCanvasHeight * scale_) * 0.5f;
}

void DiaryOverlay::reset_texture_cache(SDL_Renderer *renderer) {
    textures_.clear();
    failed_textures_.clear();
    texture_renderer_ = renderer;
}

gl::render::Texture *DiaryOverlay::load_texture(std::string_view path) {
    if (vfs_ == nullptr || texture_renderer_ == nullptr) return nullptr;
    const std::string key(path);
    const auto cached = textures_.find(key);
    if (cached != textures_.end()) return &cached->second;
    if (failed_textures_.count(key) != 0) return nullptr;
    auto data = vfs_->read(key);
    if (!data) {
        failed_textures_.insert(key);
        return nullptr;
    }
    auto decoded = gl::assets::decode_image(data.value());
    if (!decoded) {
        failed_textures_.insert(key);
        return nullptr;
    }
    auto created = gl::render::Texture::from_image(texture_renderer_, decoded.value());
    if (!created) {
        failed_textures_.insert(key);
        return nullptr;
    }
    return &textures_.emplace(key, std::move(created.value())).first->second;
}

void DiaryOverlay::draw_texture(SDL_Renderer *renderer,
                                const gl::render::Texture &texture,
                                const DiaryRect &rect) const {
    const SDL_FRect destination{rect.x, rect.y, rect.w, rect.h};
    SDL_RenderCopyF(renderer, texture.get(), nullptr, &destination);
}

std::vector<std::string> DiaryOverlay::wrap(std::string_view text, float width) const {
    if (font_ != nullptr) return font_->wrap(text, width);
    return {std::string(text)};
}

std::string DiaryOverlay::ellipsize(std::string value, float width) const {
    if (font_ == nullptr) return value;
    if (font_->measure(value).x <= width) return value;
    constexpr std::string_view kDots = "...";
    while (!value.empty() && font_->measure(value + std::string(kDots)).x > width) {
        std::size_t cut = value.size() - 1;
        while (cut > 0 && (static_cast<std::uint8_t>(value[cut]) & 0xC0) == 0x80) --cut;
        value.resize(cut);
    }
    value += kDots;
    return value;
}

void DiaryOverlay::ensure_visible() {
    if (rows_.empty()) {
        selected_ = 0;
        list_scroll_ = 0;
        return;
    }
    if (selected_ >= rows_.size()) selected_ = rows_.size() - 1;
    const std::size_t per_page =
        std::max<std::size_t>(1, static_cast<std::size_t>(kList.h / kRowHeight));
    if (selected_ < list_scroll_) list_scroll_ = selected_;
    if (selected_ >= list_scroll_ + per_page) list_scroll_ = selected_ - per_page + 1;
}

void DiaryOverlay::select_row(std::size_t row) {
    if (row >= rows_.size()) return;
    selected_ = row;
    detail_scroll_ = 0;
    ensure_visible();
}

float DiaryOverlay::detail_body_top() const {
    return kDetail.y + 4.0f;
}

void DiaryOverlay::rebuild_detail_lines() {
    detail_lines_.clear();
    if (font_ == nullptr || rows_.empty()) return;
    const gl::game::DiaryRow &row = rows_[selected_];
    if (row.paragraphs.empty()) {
        for (const std::string &line : font_->wrap(row.text, kDetail.w - 24.0f))
            detail_lines_.emplace_back(line, false);
        return;
    }
    for (std::size_t index = 0; index < row.paragraphs.size(); ++index) {
        const gl::game::DiaryParagraph &paragraph = row.paragraphs[index];
        for (const std::string &line : font_->wrap(paragraph.text, kDetail.w - 24.0f))
            detail_lines_.emplace_back(line, paragraph.completed);
        if (index + 1 < row.paragraphs.size()) detail_lines_.emplace_back("", false);
    }
}

void DiaryOverlay::clamp_detail_scroll() {
    rebuild_detail_lines();
    if (font_ == nullptr || detail_lines_.empty()) {
        detail_scroll_ = 0;
        detail_max_scroll_ = 0;
        return;
    }
    const float body_height = kDetail.y + kDetail.h - detail_body_top();
    const std::size_t visible = std::max<std::size_t>(
        1, static_cast<std::size_t>(body_height / font_->line_height()));
    detail_max_scroll_ =
        detail_lines_.size() > visible ? detail_lines_.size() - visible : 0;
    detail_scroll_ = std::min(detail_scroll_, detail_max_scroll_);
}

void DiaryOverlay::render(SDL_Renderer *renderer, int screen_w, int screen_h,
                          const gl::render::Font *font) {
    if (renderer == nullptr || !visible_ || screen_w <= 0 || screen_h <= 0) return;
    if (texture_renderer_ != renderer) reset_texture_cache(renderer);
    if (font != nullptr) font_ = font;
    update_transform(screen_w, screen_h);

    float old_scale_x = 1.0f;
    float old_scale_y = 1.0f;
    SDL_RenderGetScale(renderer, &old_scale_x, &old_scale_y);
    SDL_Rect old_viewport{};
    SDL_RenderGetViewport(renderer, &old_viewport);
    Uint8 old_r = 0, old_g = 0, old_b = 0, old_a = 0;
    SDL_GetRenderDrawColor(renderer, &old_r, &old_g, &old_b, &old_a);
    SDL_BlendMode old_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);
    SDL_Rect old_clip{};
    SDL_RenderGetClipRect(renderer, &old_clip);
    const bool old_clip_enabled = SDL_RenderIsClipEnabled(renderer) == SDL_TRUE;

    const SDL_Rect viewport{
        static_cast<int>(std::lround(offset_x_)),
        static_cast<int>(std::lround(offset_y_)),
        std::max(1, static_cast<int>(std::lround(kCanvasWidth * scale_))),
        std::max(1, static_cast<int>(std::lround(kCanvasHeight * scale_))),
    };
    SDL_RenderSetViewport(renderer, &viewport);
    SDL_RenderSetScale(renderer, scale_, scale_);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (gl::render::Texture *background = load_texture(kBackground);
        background != nullptr) {
        draw_texture(renderer, *background, {0.0f, 0.0f, kCanvasWidth, kCanvasHeight});
    }
    if (gl::render::Texture *strip = load_texture(kPageStrip); strip != nullptr)
        draw_texture(renderer, *strip, {0.0f, 0.0f, kCanvasWidth, 80.0f});
    if (gl::render::Texture *close =
            load_texture(close_hover_ || pressed_ == Pressed::close ? kClosePressedPath
                                                                   : kClosePath);
        close != nullptr)
        draw_texture(renderer, *close, kClose);

    const auto pressed_arrow = [&](std::size_t index, const DiaryRect &rect) {
        if (gl::render::Texture *texture = load_texture(kArrowPressed[index]);
            texture != nullptr)
            draw_texture(renderer, *texture, rect);
    };
    switch (pressed_) {
        case Pressed::list_prev: pressed_arrow(0, kListPrev); break;
        case Pressed::list_next: pressed_arrow(1, kListNext); break;
        case Pressed::detail_prev: pressed_arrow(2, kDetailPrev); break;
        case Pressed::detail_next: pressed_arrow(3, kDetailNext); break;
        case Pressed::none:
        case Pressed::close:
            break;
    }

    if (font_ != nullptr) {
        const gl::render::Color header{48, 36, 18, 255};
        const float label_y = (kTabHeight - font_->line_height()) * 0.5f + 8.0f;
        for (std::size_t tab = 0; tab < kTabLabels.size(); ++tab) {
            const std::string label(kTabLabels[tab]);
            const gl::render::Vec2 measured = font_->measure(label);
            font_->draw_text(renderer, label,
                             kTabX[tab] + (kTabWidth - measured.x) * 0.5f, label_y,
                             header);
        }
    }

    ensure_visible();
    clamp_detail_scroll();

    const std::size_t per_page =
        std::max<std::size_t>(1, static_cast<std::size_t>(kList.h / kRowHeight));
    const std::size_t end = std::min(rows_.size(), list_scroll_ + per_page);
    SDL_Rect list_clip{static_cast<int>(kList.x), static_cast<int>(kList.y),
                       static_cast<int>(kList.w), static_cast<int>(kList.h)};
    SDL_RenderSetClipRect(renderer, &list_clip);
    for (std::size_t index = list_scroll_; index < end; ++index) {
        const float y = kList.y + static_cast<float>(index - list_scroll_) * kRowHeight;
        const DiaryRect row{kList.x, y, kList.w, kRowHeight};
        const gl::game::DiaryRow &item = rows_[index];
        const std::string line_text = gl::game::format_diary_day(item.start) + "  " +
                                      item.title;
        if (font_ != nullptr) {
            const std::string clipped = ellipsize(line_text, row.w - 28.0f);
            const gl::render::Color color =
                index == selected_ ? gl::render::Color{24, 18, 10, 255}
                                   : gl::render::Color{48, 38, 22, 255};
            font_->draw_text(renderer, clipped, row.x + 14.0f, row.y + 2.0f, color);
            if (item.end.has_value()) {
                SDL_SetRenderDrawColor(renderer, 48, 38, 22, 200);
                const SDL_FRect strike{row.x + 14.0f,
                                       row.y + 2.0f + font_->line_height() * 0.55f,
                                       font_->measure(clipped).x, 1.0f};
                SDL_RenderFillRectF(renderer, &strike);
            }

        } else {
            gl::render::draw_text(renderer, line_text, row.x + 14.0f, row.y + 6.0f, 1.0f,
                                  gl::render::Color{48, 38, 22, 255});
        }
    }

    if (rows_.empty()) {
        const std::string empty = "Нет записей";
        if (font_ != nullptr) {
            const gl::render::Vec2 measured = font_->measure(empty);
            font_->draw_text(renderer, empty, kList.x + (kList.w - measured.x) * 0.5f,
                             kList.y + kList.h * 0.5f, gl::render::Color{80, 62, 34, 255});
        } else {
            gl::render::draw_text(renderer, "No entries", kList.x + 120.0f,
                                  kList.y + 240.0f, 1.0f,
                                  gl::render::Color{80, 62, 34, 255});
        }
    }

    if (!rows_.empty() && font_ != nullptr) {
        SDL_Rect detail_clip{static_cast<int>(kDetail.x), static_cast<int>(kDetail.y),
                             static_cast<int>(kDetail.w), static_cast<int>(kDetail.h)};
        SDL_RenderSetClipRect(renderer, &detail_clip);
        const gl::render::Color body_color{34, 26, 16, 255};
        const float body_top = detail_body_top();
        const int clip_top = static_cast<int>(std::ceil(body_top));
        const int clip_height =
            std::max(0, static_cast<int>(kDetail.y + kDetail.h) - clip_top);
        if (clip_height > 0) {
            SDL_Rect clip{static_cast<int>(kDetail.x), clip_top, static_cast<int>(kDetail.w),
                          clip_height};
            SDL_RenderSetClipRect(renderer, &clip);
            const float line_height = font_->line_height();
            float text_y = body_top - static_cast<float>(detail_scroll_) * line_height;
            for (const auto &[line, completed] : detail_lines_) {
                font_->draw_text(renderer, line, kDetail.x + 12.0f, text_y, body_color);
                if (completed && !line.empty()) {
                    SDL_SetRenderDrawColor(renderer, 48, 38, 22, 200);
                    const SDL_FRect strike{kDetail.x + 12.0f,
                                           text_y + line_height * 0.55f,
                                           font_->measure(line).x, 1.0f};
                    SDL_RenderFillRectF(renderer, &strike);
                }
                text_y += line_height;
            }
        }
        SDL_RenderSetClipRect(renderer, old_clip_enabled ? &old_clip : nullptr);
    }

    SDL_RenderSetScale(renderer, old_scale_x, old_scale_y);
    SDL_RenderSetViewport(renderer, &old_viewport);
    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    SDL_SetRenderDrawColor(renderer, old_r, old_g, old_b, old_a);
    SDL_RenderSetClipRect(renderer, old_clip_enabled ? &old_clip : nullptr);
}

bool DiaryOverlay::handle_key(SDL_Keycode key) {
    if (!visible_) return false;
    if (key == SDLK_ESCAPE || key == SDLK_j || key == static_cast<SDL_Keycode>('J') ||
        key == SDLK_AC_BACK) {
        close();
        return true;
    }
    if (rows_.empty()) return true;
    if (key == SDLK_UP) {
        if (selected_ > 0) select_row(selected_ - 1);
        return true;
    }
    if (key == SDLK_DOWN) {
        if (selected_ + 1 < rows_.size()) select_row(selected_ + 1);
        return true;
    }
    if (key == SDLK_HOME) {
        select_row(0);
        return true;
    }
    if (key == SDLK_END) {
        select_row(rows_.size() - 1);
        return true;
    }
    if (key == SDLK_PAGEUP || key == SDLK_PAGEDOWN) {
        if (key == SDLK_PAGEUP)
            detail_scroll_ = detail_scroll_ > kPageLines ? detail_scroll_ - kPageLines : 0;
        else
            detail_scroll_ += kPageLines;
        clamp_detail_scroll();
        return true;
    }
    return true;
}

bool DiaryOverlay::handle_wheel(int delta) {
    if (!visible_ || delta == 0) return false;
    if (delta > 0)
        detail_scroll_ = detail_scroll_ > kWheelLines ? detail_scroll_ - kWheelLines : 0;
    else
        detail_scroll_ += kWheelLines;
    clamp_detail_scroll();
    return true;
}

bool DiaryOverlay::handle_mouse(int x, int y, bool click) {
    if (!visible_) return false;
    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;
    close_hover_ = kClose.contains(logical_x, logical_y);
    if (click) pressed_ = Pressed::none;
    if (click && close_hover_) {
        pressed_ = Pressed::close;
        close();
        return true;
    }
    const std::size_t per_page =
        std::max<std::size_t>(1, static_cast<std::size_t>(kList.h / kRowHeight));
    if (click && kListPrev.contains(logical_x, logical_y)) {
        pressed_ = Pressed::list_prev;
        list_scroll_ = list_scroll_ > per_page ? list_scroll_ - per_page : 0;
        if (!rows_.empty()) select_row(list_scroll_);
        return true;
    }
    if (click && kListNext.contains(logical_x, logical_y)) {
        pressed_ = Pressed::list_next;
        if (!rows_.empty()) {
            const std::size_t maximum = rows_.size() > per_page ? rows_.size() - per_page : 0;
            list_scroll_ = std::min(list_scroll_ + per_page, maximum);
            select_row(list_scroll_);
        }
        return true;
    }
    if (click && kDetailPrev.contains(logical_x, logical_y)) {
        pressed_ = Pressed::detail_prev;
        detail_scroll_ = detail_scroll_ > kPageLines ? detail_scroll_ - kPageLines : 0;
        clamp_detail_scroll();
        return true;
    }
    if (click && kDetailNext.contains(logical_x, logical_y)) {
        pressed_ = Pressed::detail_next;
        detail_scroll_ += kPageLines;
        clamp_detail_scroll();
        return true;
    }
    if (click && kList.contains(logical_x, logical_y) && !rows_.empty()) {
        const std::size_t per_page =
            std::max<std::size_t>(1, static_cast<std::size_t>(kList.h / kRowHeight));
        const std::size_t row = std::min(
            per_page - 1,
            static_cast<std::size_t>((logical_y - kList.y) / kRowHeight));
        select_row(list_scroll_ + row);
    }
    return logical_x >= 0.0f && logical_y >= 0.0f && logical_x < kCanvasWidth &&
           logical_y < kCanvasHeight;
}
} // namespace gl::app
