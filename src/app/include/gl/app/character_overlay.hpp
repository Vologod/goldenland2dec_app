#pragma once
#include <gl/game/character_sheet.hpp>
#include <gl/render/font.hpp>
#include <gl/render/texture.hpp>
#include <SDL.h>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace gl::assets {
class GameVfs;
class ItemCatalog;
struct HeroInventory;
struct ItemCatalogEntry;
} // namespace gl::assets

namespace gl::app {
enum class CharacterRegion { none, tab, close };

class CharacterOverlay {
  public:
    bool open(SDL_Renderer *renderer, const gl::assets::GameVfs &vfs,
              gl::game::CharacterSheet sheet,
              const gl::assets::HeroInventory *inventory,
              const gl::assets::ItemCatalog *catalog, bool use_b_set = false);
    void close();
    bool visible() const { return visible_; }
    gl::game::CharacterTab tab() const { return tab_; }
    void set_tab(gl::game::CharacterTab tab) {
        const int value = static_cast<int>(tab);
        tab_ = static_cast<gl::game::CharacterTab>(value < 0 ? 0 : (value > 1 ? 1 : value));
    }
    void set_use_b_set(bool use_b_set) { use_b_set_ = use_b_set; }
    bool take_weapon_set_toggle();
    void set_font(const gl::render::Font *font) { font_ = font; }
    void set_title_font(const gl::render::Font *font) { title_font_ = font; }
    void set_fonts(const gl::render::Font *title, const gl::render::Font *rows) {
        set_title_font(title);
        set_font(rows);
    }
    void set_top_font(const gl::render::Font *font) { top_font_ = font; }
    void render(SDL_Renderer *renderer, int screen_w, int screen_h,
                const gl::render::Font *font = nullptr);
    bool handle_mouse(int x, int y, bool click);
    bool handle_key(SDL_Keycode key);
    CharacterRegion hit_test(int x, int y) const;

  private:
    const gl::render::Font *title_font() const {
        return title_font_ != nullptr ? title_font_ : font_;
    }
    const gl::render::Font *top_font() const {
        return top_font_ != nullptr ? top_font_ : title_font();
    }
    void update_transform(int screen_w, int screen_h);
    void reset_texture_cache(SDL_Renderer *renderer);
    gl::render::Texture *load_texture(std::string_view path, bool magenta_key = false);
    void draw_background(SDL_Renderer *renderer);
    void draw_tabs(SDL_Renderer *renderer);
    void draw_close(SDL_Renderer *renderer);
    void draw_text_rect(SDL_Renderer *renderer, const gl::render::Font *font,
                        std::string_view text, float x, float y, float w, float h,
                        gl::render::Color color, bool bold = false);
    void draw_item(SDL_Renderer *renderer, const gl::assets::ItemCatalogEntry *entry,
                   float x, float y, float w, float h);
    void draw_doll_overlays(SDL_Renderer *renderer);
    void draw_inventory_items(SDL_Renderer *renderer);
    void draw_content(SDL_Renderer *renderer);

    const gl::assets::GameVfs *vfs_ = nullptr;
    const gl::assets::HeroInventory *inventory_ = nullptr;
    const gl::assets::ItemCatalog *catalog_ = nullptr;
    const gl::render::Font *font_ = nullptr;
    const gl::render::Font *title_font_ = nullptr;
    const gl::render::Font *top_font_ = nullptr;
    gl::game::CharacterSheet sheet_;
    gl::game::CharacterTab tab_ = gl::game::CharacterTab::characteristics;
    bool visible_ = false;
    bool hover_close_ = false;
    bool use_b_set_ = false;
    bool weapon_set_toggle_requested_ = false;
    float scale_ = 1.0f;
    float offset_x_ = 0.0f;
    float offset_y_ = 0.0f;
    SDL_Renderer *texture_renderer_ = nullptr;
    gl::render::Texture *background_ = nullptr;
    std::map<std::string, gl::render::Texture> textures_;
    std::set<std::string> failed_textures_;
};
} // namespace gl::app
