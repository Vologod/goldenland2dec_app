#pragma once
#include <gl/app/hero_portrait.hpp>
#include <gl/game/character_sheet.hpp>
#include <gl/render/font.hpp>
#include <gl/render/texture.hpp>
#include <SDL.h>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gl::assets { class GameVfs; }

namespace gl::app {
enum class CharacterRegion { none, tab, close };

class CharacterOverlay {
  public:
    bool open(SDL_Renderer *renderer, const gl::assets::GameVfs &vfs,
              gl::game::CharacterSheet sheet, const gl::game::HeroOutfit *outfit);
    void close();
    bool visible() const { return visible_; }
    gl::game::CharacterTab tab() const { return tab_; }
    void set_tab(gl::game::CharacterTab tab) { tab_ = tab; }
    void set_font(const gl::render::Font *font) { font_ = font; }
    void render(SDL_Renderer *renderer, int screen_w, int screen_h,
                const gl::render::Font *font = nullptr);
    bool handle_mouse(int x, int y, bool click);
    bool handle_key(SDL_Keycode key);
    CharacterRegion hit_test(int x, int y) const;

  private:
    void update_transform(int screen_w, int screen_h);
    void reset_texture_cache(SDL_Renderer *renderer);
    gl::render::Texture *load_texture(std::string_view path);
    void draw_background(SDL_Renderer *renderer);
    void draw_tabs(SDL_Renderer *renderer);
    void draw_close(SDL_Renderer *renderer);
    void draw_rows(SDL_Renderer *renderer, const std::vector<gl::game::CharacterRow> &rows,
                   const int *y_positions, int count, float x, float w, float height,
                   bool right_align);
    void draw_row(SDL_Renderer *renderer, const gl::game::CharacterRow &row, float x,
                  float y, float w, float height, bool right_align);
    void draw_content(SDL_Renderer *renderer);

    const gl::assets::GameVfs *vfs_ = nullptr;
    const gl::render::Font *font_ = nullptr;
    gl::game::CharacterSheet sheet_;
    gl::game::CharacterTab tab_ = gl::game::CharacterTab::skills;
    bool visible_ = false;
    bool hover_close_ = false;
    float scale_ = 1.0f;
    float offset_x_ = 0.0f;
    float offset_y_ = 0.0f;
    SDL_Renderer *texture_renderer_ = nullptr;
    std::map<std::string, gl::render::Texture> textures_;
    std::set<std::string> failed_textures_;
    HeroPortrait portrait_;
};
} // namespace gl::app
