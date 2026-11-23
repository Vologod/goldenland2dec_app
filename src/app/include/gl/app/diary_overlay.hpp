#pragma once
#include "gl/game/diary_view.hpp"
#include "gl/render/font.hpp"
#include "gl/render/texture.hpp"

#include <SDL.h>

#include <cstddef>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gl::assets {
class GameVfs;
struct Image;
} // namespace gl::assets

namespace gl::app {
struct DiaryRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool contains(float px, float py) const;
};

class DiaryOverlay {
  public:
    static constexpr float kCanvasWidth = 1024.0f;
    static constexpr float kCanvasHeight = 768.0f;
    static constexpr DiaryRect kClose{909.0f, 687.0f, 71.0f, 65.0f};
    static constexpr DiaryRect kListPrev{38.0f, 696.0f, 41.0f, 61.0f};
    static constexpr DiaryRect kListNext{98.0f, 696.0f, 41.0f, 61.0f};
    static constexpr DiaryRect kDetailPrev{418.0f, 696.0f, 41.0f, 61.0f};
    static constexpr DiaryRect kDetailNext{478.0f, 696.0f, 41.0f, 61.0f};
    static constexpr DiaryRect kList{70.0f, 110.0f, 340.0f, 540.0f};
    static constexpr DiaryRect kDetail{455.0f, 110.0f, 490.0f, 540.0f};
    static constexpr float kRowHeight = 24.0f;

    bool open(const gl::assets::GameVfs &vfs, std::span<const gl::game::DiaryRow> rows);
    void close();
    bool visible() const { return visible_; }
    void set_font(const gl::render::Font *font) { font_ = font; }

    void render(SDL_Renderer *renderer, int screen_w, int screen_h,
                const gl::render::Font *font = nullptr);
    bool handle_key(SDL_Keycode key);
    bool handle_wheel(int delta);
    bool handle_mouse(int x, int y, bool click);
    void release();

    std::size_t selected() const { return selected_; }
    std::size_t list_scroll() const { return list_scroll_; }
    std::size_t detail_scroll() const { return detail_scroll_; }
    std::size_t detail_max_scroll() const { return detail_max_scroll_; }
    std::size_t row_count() const { return rows_.size(); }

  private:
    void update_transform(int screen_w, int screen_h);
    void reset_texture_cache(SDL_Renderer *renderer);
    gl::render::Texture *load_texture(std::string_view path);
    void draw_texture(SDL_Renderer *renderer, const gl::render::Texture &texture,
                      const DiaryRect &rect) const;
    void ensure_visible();
    void select_row(std::size_t row);
    float detail_body_top() const;
    void rebuild_detail_lines();
    void clamp_detail_scroll();
    std::vector<std::string> wrap(std::string_view text, float width) const;
    std::string ellipsize(std::string value, float width) const;

    const gl::assets::GameVfs *vfs_ = nullptr;
    const gl::render::Font *font_ = nullptr;
    std::vector<gl::game::DiaryRow> rows_;
    std::vector<std::pair<std::string, bool>> detail_lines_;
    std::size_t selected_ = 0;
    std::size_t list_scroll_ = 0;
    enum class Pressed { none, close, list_prev, list_next, detail_prev, detail_next };
    std::size_t detail_scroll_ = 0;
    std::size_t detail_max_scroll_ = 0;
    bool visible_ = false;
    bool close_hover_ = false;
    Pressed pressed_ = Pressed::none;

    float scale_ = 1.0f;
    float offset_x_ = 0.0f;
    float offset_y_ = 0.0f;
    int screen_w_ = 1024;
    int screen_h_ = 768;

    SDL_Renderer *texture_renderer_ = nullptr;
    std::map<std::string, gl::render::Texture> textures_;
    std::set<std::string> failed_textures_;
};
} // namespace gl::app
