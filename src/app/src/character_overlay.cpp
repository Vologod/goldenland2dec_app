#include "gl/app/character_overlay.hpp"
#include "gl/assets/image.hpp"
#include "gl/assets/vfs.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <utility>

namespace gl::app {
namespace {
constexpr float kCanvasWidth = 1024.0f;
constexpr float kCanvasHeight = 768.0f;
constexpr std::string_view kBackground = "engineres/newinv/inventory_skills.bmp";
constexpr std::array<float, 5> kTabX{3.0f, 62.0f, 121.0f, 179.0f, 239.0f};
constexpr float kTabY = 4.0f;
constexpr float kTabW = 56.0f;
constexpr float kTabH = 25.0f;
constexpr std::array<float, 7> kPrimaryY{60.0f, 88.0f, 117.0f, 144.0f,
                                         172.0f, 199.0f, 226.0f};
constexpr std::array<float, 15> kCharacteristicsY{
    109.0f, 140.0f, 168.0f, 218.0f, 244.0f, 271.0f, 354.0f, 381.0f,
    408.0f, 435.0f, 462.0f, 489.0f, 516.0f, 543.0f, 570.0f};
constexpr std::array<float, 27> kSkillX{
    295.0f, 295.0f, 295.0f, 544.0f, 544.0f, 544.0f, 790.0f, 790.0f, 790.0f,
    295.0f, 295.0f, 295.0f, 544.0f, 544.0f, 544.0f, 790.0f, 790.0f, 790.0f,
    295.0f, 295.0f, 295.0f, 544.0f, 544.0f, 544.0f, 790.0f, 790.0f, 790.0f};
constexpr std::array<float, 27> kSkillY{
    130.0f, 162.0f, 194.0f, 130.0f, 162.0f, 194.0f, 130.0f, 162.0f, 194.0f,
    315.0f, 348.0f, 381.0f, 315.0f, 348.0f, 381.0f, 315.0f, 348.0f, 381.0f,
    500.0f, 532.0f, 564.0f, 500.0f, 532.0f, 564.0f, 500.0f, 532.0f, 564.0f};
constexpr gl::render::Color kTextColor{20, 18, 16, 255};

bool contains(float rx, float ry, float rw, float rh, float x, float y) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}
} // namespace

bool CharacterOverlay::open(const gl::assets::GameVfs &vfs,
                            gl::game::CharacterSheet sheet) {
    vfs_ = &vfs;
    sheet_ = std::move(sheet);
    tab_ = gl::game::CharacterTab::skills;
    visible_ = true;
    scale_ = 1.0f;
    offset_x_ = 0.0f;
    offset_y_ = 0.0f;
    update_transform(1024, 768);
    reset_texture_cache(nullptr);
    return true;
}

void CharacterOverlay::close() { visible_ = false; }

void CharacterOverlay::update_transform(int screen_w, int screen_h) {
    scale_ = std::min(static_cast<float>(screen_w) / kCanvasWidth,
                      static_cast<float>(screen_h) / kCanvasHeight);
    if (scale_ <= 0.0f) scale_ = 1.0f;
    offset_x_ = (static_cast<float>(screen_w) - kCanvasWidth * scale_) * 0.5f;
    offset_y_ = (static_cast<float>(screen_h) - kCanvasHeight * scale_) * 0.5f;
}

void CharacterOverlay::reset_texture_cache(SDL_Renderer *renderer) {
    textures_.clear();
    failed_textures_.clear();
    texture_renderer_ = renderer;
}

gl::render::Texture *CharacterOverlay::load_texture(std::string_view path) {
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

void CharacterOverlay::render(SDL_Renderer *renderer, int screen_w, int screen_h,
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
    SDL_Rect old_clip{};
    const SDL_bool old_clipping = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &old_clip);
    SDL_BlendMode old_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);
    const SDL_Rect viewport{
        static_cast<int>(std::lround(offset_x_)),
        static_cast<int>(std::lround(offset_y_)),
        std::max(1, static_cast<int>(std::lround(kCanvasWidth * scale_))),
        std::max(1, static_cast<int>(std::lround(kCanvasHeight * scale_)))};
    SDL_RenderSetViewport(renderer, &viewport);
    SDL_RenderSetScale(renderer, scale_, scale_);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    draw_background(renderer);
    draw_tabs(renderer);
    draw_content(renderer);
    draw_close(renderer);

    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    SDL_RenderSetScale(renderer, old_scale_x, old_scale_y);
    SDL_RenderSetClipRect(renderer, old_clipping ? &old_clip : nullptr);
    SDL_RenderSetViewport(renderer, &old_viewport);
}

void CharacterOverlay::draw_background(SDL_Renderer *renderer) {
    if (gl::render::Texture *background = load_texture(kBackground); background != nullptr) {
        const SDL_FRect rect{0.0f, 0.0f, kCanvasWidth, kCanvasHeight};
        SDL_RenderCopyF(renderer, background->get(), nullptr, &rect);
        return;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    const SDL_FRect rect{0.0f, 0.0f, kCanvasWidth, kCanvasHeight};
    SDL_RenderFillRectF(renderer, &rect);
}

void CharacterOverlay::draw_tabs(SDL_Renderer *renderer) {
    const std::size_t active = static_cast<std::size_t>(tab_);
    SDL_SetRenderDrawColor(renderer, 216, 200, 154, 220);
    const SDL_FRect rect{kTabX[active], kTabY, kTabW, kTabH};
    SDL_RenderDrawRectF(renderer, &rect);
}

void CharacterOverlay::draw_close(SDL_Renderer *renderer) {
    const std::string_view path =
        hover_close_ ? "engineres/newinv/exitl.bmp" : "engineres/newinv/exit.bmp";
    if (gl::render::Texture *texture = load_texture(path); texture != nullptr) {
        const SDL_FRect rect{920.0f, 694.0f, 75.0f, 68.0f};
        SDL_RenderCopyF(renderer, texture->get(), nullptr, &rect);
    }
}

void CharacterOverlay::draw_rows(SDL_Renderer *renderer,
                                 const std::vector<gl::game::CharacterRow> &rows,
                                 const int *y_positions, int count, float x, float w,
                                 float height, bool right_align) {
    if (font_ == nullptr) return;
    for (int index = 0; index < count && index < static_cast<int>(rows.size()); ++index) {
        draw_row(renderer, rows[static_cast<std::size_t>(index)], x,
                 static_cast<float>(y_positions[index]), w, height, right_align);
    }
}

void CharacterOverlay::draw_row(SDL_Renderer *renderer,
                                const gl::game::CharacterRow &row, float x, float y,
                                float w, float height, bool right_align) {
    if (font_ == nullptr) return;
    const SDL_Rect clip{static_cast<int>(x), static_cast<int>(y),
                        static_cast<int>(w), static_cast<int>(height)};
    SDL_RenderSetClipRect(renderer, &clip);
    font_->draw_text(renderer, row.label, x + 4.0f,
                     y + (height - font_->line_height()) * 0.5f, kTextColor);
    if (right_align) {
        const auto measured = font_->measure(row.value);
        font_->draw_text(renderer, row.value, x + w - 4.0f - measured.x,
                         y + (height - font_->line_height()) * 0.5f, kTextColor);
    } else {
        font_->draw_text(renderer, row.value, x + 4.0f,
                         y + (height - font_->line_height()) * 0.5f, kTextColor);
    }
    SDL_RenderSetClipRect(renderer, nullptr);
}

void CharacterOverlay::draw_content(SDL_Renderer *renderer) {
    if (font_ == nullptr) return;
    std::array<int, 7> primary_y{};
    for (std::size_t index = 0; index < primary_y.size(); ++index)
        primary_y[index] = static_cast<int>(kPrimaryY[index]);
    draw_rows(renderer, sheet_.primary, primary_y.data(), 7, 20.0f, 130.0f, 23.0f,
              true);
    if (sheet_.primary.size() == 8) {
        const auto &reputation = sheet_.primary[7];
        font_->draw_text(renderer, reputation.label, 24.0f, 264.0f, kTextColor);
        const auto measured = font_->measure(reputation.value);
        font_->draw_text(renderer, reputation.value, 181.0f - 4.0f - measured.x + 20.0f,
                         264.0f, kTextColor);
    }
    font_->draw_text(renderer, std::to_string(sheet_.skill_points), 526.0f, 634.0f,
                     kTextColor);

    switch (tab_) {
    case gl::game::CharacterTab::personality: {
        static constexpr std::array<int, 4> kY{102, 134, 163, 214};
        static constexpr std::array<float, 4> kW{272.0f, 155.0f, 272.0f, 164.0f};
        static constexpr std::array<float, 4> kH{24.0f, 23.0f, 24.0f, 20.0f};
        for (std::size_t index = 0;
             index < sheet_.personality.size() && index < kY.size(); ++index)
            draw_row(renderer, sheet_.personality[index], 296.0f,
                     static_cast<float>(kY[index]), kW[index], kH[index], true);
        break;
    }
    case gl::game::CharacterTab::characteristics: {
        std::array<int, 15> y{};
        for (std::size_t index = 0; index < y.size(); ++index)
            y[index] = static_cast<int>(kCharacteristicsY[index]);
        draw_rows(renderer, sheet_.characteristics, y.data(), 15, 296.0f, 155.0f, 20.0f,
                  true);
        break;
    }
    case gl::game::CharacterTab::skills: {
        for (std::size_t index = 0; index < sheet_.skills.size() && index < kSkillX.size();
             ++index) {
            const float x = kSkillX[index];
            const float y = kSkillY[index];
            const auto &row = sheet_.skills[index];
            const SDL_Rect clip{static_cast<int>(x), static_cast<int>(y), 140, 20};
            SDL_RenderSetClipRect(renderer, &clip);
            font_->draw_text(renderer, row.label, x + 4.0f, y + 2.0f, kTextColor);
            const auto measured = font_->measure(row.value);
            font_->draw_text(renderer, row.value, x + 140.0f - 4.0f - measured.x, y + 2.0f,
                             kTextColor);
            SDL_RenderSetClipRect(renderer, nullptr);
        }
        break;
    }
    case gl::game::CharacterTab::state:
    case gl::game::CharacterTab::resistance: {
        constexpr std::string_view kNoData = "Нет данных";
        const auto measured = font_->measure(kNoData);
        font_->draw_text(renderer, kNoData, 296.0f + (650.0f - measured.x) * 0.5f, 300.0f,
                         kTextColor);
        break;
    }
    }
}

bool CharacterOverlay::handle_mouse(int x, int y, bool click) {
    if (!visible_) return false;
    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;
    const bool on_close =
        contains(920.0f, 694.0f, 75.0f, 68.0f, logical_x, logical_y);
    hover_close_ = on_close;
    if (!click) return on_close;
    for (std::size_t index = 0; index < kTabX.size(); ++index) {
        if (contains(kTabX[index], kTabY, kTabW, kTabH, logical_x, logical_y)) {
            tab_ = static_cast<gl::game::CharacterTab>(index);
            return true;
        }
    }
    if (on_close) {
        close();
        return true;
    }
    return false;
}

bool CharacterOverlay::handle_key(SDL_Keycode key) {
    if (!visible_) return false;
    if (key == SDLK_ESCAPE || key == SDLK_AC_BACK || key == SDLK_c) {
        close();
        return true;
    }
    if (key >= SDLK_1 && key <= SDLK_5) {
        tab_ = static_cast<gl::game::CharacterTab>(key - SDLK_1);
        return true;
    }
    return false;
}

CharacterRegion CharacterOverlay::hit_test(int x, int y) const {
    if (!visible_ || scale_ <= 0.0f) return CharacterRegion::none;
    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;
    for (const float tab_x : kTabX)
        if (contains(tab_x, kTabY, kTabW, kTabH, logical_x, logical_y))
            return CharacterRegion::tab;
    if (contains(920.0f, 694.0f, 75.0f, 68.0f, logical_x, logical_y))
        return CharacterRegion::close;
    return CharacterRegion::none;
}
} // namespace gl::app
