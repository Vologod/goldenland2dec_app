#pragma once

#include <gl/assets/hero_equipment.hpp>
#include <gl/game/inventory_view.hpp>
#include <gl/render/font.hpp>
#include <gl/render/texture.hpp>

#include <SDL.h>

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gl::assets {
class GameVfs;
class ItemCatalog;
struct ItemCatalogEntry;
} // namespace gl::assets

namespace gl::app {

enum class InventoryRegion {
    none,
    bottom_slot,
    hot_slot,
    puppet_slot,
    filter,
    scroll_left,
    scroll_right,
    exit,
    skills_tab,
    drop,
    panel,
};

enum class InventorySelection {
    none,
    item,
    hot,
    equipped,
};

class InventoryOverlay {
  public:
    bool open(const gl::assets::GameVfs &vfs, const gl::assets::ItemCatalog &catalog,
              const gl::assets::HeroInventory &inventory);
    void close();
    bool visible() const { return visible_; }

    void render(SDL_Renderer *renderer, int screen_w, int screen_h,
                const gl::render::Font *font = nullptr);
    bool handle_mouse(int x, int y, bool click);
    bool handle_key(SDL_Keycode key);
    bool handle_wheel(int delta);
    bool handle_wheel(int delta, int x, int y);

    const gl::game::InventoryViewState &view() const { return view_; }
    std::size_t visible_count() const { return visible_indices_.size(); }
    std::string selected_name() const;
    InventorySelection selection() const { return selection_; }
    std::size_t panel_scroll() const { return panel_scroll_; }
    std::size_t panel_max_scroll(const gl::render::Font &font) const;
    static float native_icon_offset(float target, float texture_size);

    InventoryRegion hit_test(int x, int y) const;

  private:
    struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
    };

    void rebuild_visible();
    void select(InventorySelection source, std::size_t index);
    void update_transform(int screen_w, int screen_h);
    void reset_texture_cache(SDL_Renderer *renderer);
    gl::render::Texture *load_texture(std::string_view path);
    gl::render::Texture *icon_texture(const gl::assets::ItemCatalogEntry *entry);
    gl::render::Texture *background_texture();
    const gl::assets::InventoryItem *selected_item() const;
    const gl::assets::ItemCatalogEntry *selected_entry() const;
    std::vector<std::string> wrap_text(std::string_view text, float width,
                                       const gl::render::Font *font) const;
    void draw_native(SDL_Renderer *renderer, const gl::render::Texture &texture, float x,
                     float y, float target_w, float target_h) const;
    void draw_rect(SDL_Renderer *renderer, const gl::render::Texture &texture,
                   const Rect &rect) const;
    void draw_panel(SDL_Renderer *renderer, const gl::render::Font *font);

    const gl::assets::GameVfs *vfs_ = nullptr;
    const gl::assets::ItemCatalog *catalog_ = nullptr;
    gl::assets::HeroInventory inventory_;
    gl::game::InventoryViewState view_;
    std::vector<std::size_t> visible_indices_;
    InventorySelection selection_ = InventorySelection::none;
    std::size_t panel_scroll_ = 0;
    std::size_t panel_scroll_max_ = 0;
    bool visible_ = false;

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
