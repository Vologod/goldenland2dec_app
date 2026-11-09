#pragma once

#include <gl/assets/hero_equipment.hpp>
#include <gl/render/font.hpp>
#include <gl/render/texture.hpp>

#include <SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gl::assets {
class GameVfs;
class ItemCatalog;
struct Image;
struct ItemCatalogEntry;
} // namespace gl::assets

namespace gl::app {

struct TradeRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool contains(float px, float py) const;
};

struct TradeFilterSpec {
    TradeRect rect;
    std::uint32_t mask = 0;
    int sprite = 0;
};

enum class TradeRegion {
    none,
    npc_slot,
    npc_filter,
    npc_scroll_left,
    npc_scroll_right,
    hero_slot,
    hero_filter,
    hero_scroll_left,
    hero_scroll_right,
    exit,
    drop,
};

enum class TradeSelection {
    none,
    npc,
    hero,
};

struct TradeItemView {
    const gl::assets::InventoryItem *item = nullptr;
    const gl::assets::ItemCatalogEntry *entry = nullptr;
    std::size_t source_index = 0;
};

struct TradeLayout {
    static constexpr float kCanvasWidth = 1024.0f;
    static constexpr float kCanvasHeight = 768.0f;
    static constexpr int kIconSize = 55;

    static constexpr int kNpcY = 26;
    static constexpr std::array<int, 10> kNpcX{347, 407, 467, 528, 589, 650, 711, 773, 834, 895};
    static constexpr int kHeroY = 703;
    static constexpr std::array<int, 12> kHeroX{140, 201, 261, 321, 382, 443,
                                                504, 565, 627, 688, 749, 811};

    static constexpr float kNpcMoneyX = 562.0f;
    static constexpr int kNpcMoneyY = 126;
    static constexpr float kHeroMoneyX = 562.0f;
    static constexpr int kHeroMoneyY = 612;

    static constexpr std::array<TradeFilterSpec, 7> kNpcFilters{{
        TradeFilterSpec{{350.0f, 94.0f, 52.0f, 20.0f}, 0x01u, 1},
        TradeFilterSpec{{439.0f, 94.0f, 52.0f, 20.0f}, 0x02u, 2},
        TradeFilterSpec{{531.0f, 94.0f, 52.0f, 20.0f}, 0x04u, 3},
        TradeFilterSpec{{621.0f, 94.0f, 52.0f, 20.0f}, 0x08u, 4},
        TradeFilterSpec{{713.0f, 94.0f, 52.0f, 20.0f}, 0x10u, 5},
        TradeFilterSpec{{806.0f, 94.0f, 52.0f, 20.0f}, 0x20u, 6},
        TradeFilterSpec{{893.0f, 94.0f, 52.0f, 20.0f}, 0x00u, 7},
    }};

    static constexpr std::array<TradeFilterSpec, 7> kHeroFilters{{
        TradeFilterSpec{{201.0f, 672.0f, 56.0f, 23.0f}, 0x01u, 1},
        TradeFilterSpec{{290.0f, 673.0f, 56.0f, 23.0f}, 0x02u, 2},
        TradeFilterSpec{{383.0f, 673.0f, 56.0f, 23.0f}, 0x04u, 3},
        TradeFilterSpec{{474.0f, 673.0f, 56.0f, 23.0f}, 0x08u, 4},
        TradeFilterSpec{{565.0f, 673.0f, 56.0f, 23.0f}, 0x10u, 5},
        TradeFilterSpec{{658.0f, 673.0f, 56.0f, 23.0f}, 0x20u, 6},
        TradeFilterSpec{{745.0f, 673.0f, 56.0f, 23.0f}, 0x00u, 7},
    }};

    static constexpr TradeRect kNpcScrollLeft{312.0f, 28.0f, 27.0f, 55.0f};
    static constexpr TradeRect kNpcScrollRight{954.0f, 26.0f, 27.0f, 55.0f};
    static constexpr TradeRect kHeroScrollLeft{110.0f, 702.0f, 27.0f, 55.0f};
    static constexpr TradeRect kHeroScrollRight{870.0f, 703.0f, 26.0f, 54.0f};
    static constexpr TradeRect kExit{920.0f, 694.0f, 75.0f, 68.0f};
    static constexpr TradeRect kDrop{19.0f, 690.0f, 81.0f, 75.0f};

    TradeRegion hit_test(float x, float y) const;
    std::optional<std::size_t> npc_slot_at(float x, float y) const;
    std::optional<std::size_t> hero_slot_at(float x, float y) const;
    std::optional<std::size_t> npc_filter_at(float x, float y) const;
    std::optional<std::size_t> hero_filter_at(float x, float y) const;
    static constexpr std::size_t npc_row_size() { return kNpcX.size(); }
    static constexpr std::size_t hero_row_size() { return kHeroX.size(); }
    static std::size_t max_scroll(std::size_t item_count, std::size_t row_size);
};

class TradeOverlay {
  public:
    bool open(const gl::assets::GameVfs &vfs, const gl::assets::ItemCatalog &catalog,
              std::string npc_name, const std::vector<gl::assets::InventoryItem> &npc_items,
              std::int32_t npc_money, const std::vector<gl::assets::InventoryItem> &hero_items,
              std::int32_t hero_money);
    void close();
    bool visible() const { return visible_; }

    void render(SDL_Renderer *renderer, int screen_w, int screen_h,
                const gl::render::Font *font = nullptr);
    void set_font(const gl::render::Font *font) { font_ = font; }

    bool handle_mouse(int x, int y, bool click);
    bool handle_key(SDL_Keycode key);
    bool handle_wheel(int delta);
    bool handle_wheel(int delta, int x, int y);

    TradeRegion hit_test(int x, int y) const;
    const TradeLayout &layout() const { return layout_; }
    const std::string &npc_name() const { return npc_name_; }
    const std::vector<gl::assets::InventoryItem> &npc_items() const { return npc_items_; }
    const std::vector<gl::assets::InventoryItem> &hero_items() const { return hero_items_; }
    std::int32_t npc_money() const { return npc_money_; }
    std::int32_t hero_money() const { return hero_money_; }
    std::uint32_t npc_filter() const { return npc_filter_; }
    std::uint32_t hero_filter() const { return hero_filter_; }
    std::size_t npc_scroll() const { return npc_scroll_; }
    std::size_t hero_scroll() const { return hero_scroll_; }
    std::size_t npc_max_scroll() const;
    std::size_t hero_max_scroll() const;
    TradeSelection selection() const { return selection_; }
    const std::optional<std::size_t> &selected_npc() const { return selected_npc_; }
    const std::optional<std::size_t> &selected_hero() const { return selected_hero_; }
    std::string selected_name() const;
    const std::vector<std::size_t> &visible_hero_indices() const { return hero_visible_; }

    static float native_icon_offset(float target, float texture_size);
    static void apply_magenta_key(gl::assets::Image &image);

  private:
    void rebuild_hero_visible();
    void clear_selection();
    void update_transform(int screen_w, int screen_h);
    void reset_texture_cache(SDL_Renderer *renderer);
    gl::render::Texture *load_texture(std::string_view path, bool magenta_key = false);
    gl::render::Texture *icon_texture(const gl::assets::ItemCatalogEntry *entry);
    gl::render::Texture *background_texture();
    const gl::assets::InventoryItem *selected_item() const;
    const gl::assets::ItemCatalogEntry *selected_entry() const;
    void draw_native(SDL_Renderer *renderer, const gl::render::Texture &texture, float x, float y,
                     float target_w, float target_h) const;
    void draw_rect(SDL_Renderer *renderer, const gl::render::Texture &texture,
                   const TradeRect &rect) const;
    void draw_row(SDL_Renderer *renderer, const std::vector<gl::assets::InventoryItem> &items,
                  const std::vector<std::size_t> *visible, const int *xs, int y, std::size_t scroll,
                  int slots);
    void draw_filters(SDL_Renderer *renderer);
    void draw_money(SDL_Renderer *renderer);
    void draw_selected_name(SDL_Renderer *renderer);

    const gl::assets::GameVfs *vfs_ = nullptr;
    const gl::assets::ItemCatalog *catalog_ = nullptr;
    const gl::render::Font *font_ = nullptr;
    TradeLayout layout_;

    std::string npc_name_;
    std::vector<gl::assets::InventoryItem> npc_items_;
    std::vector<gl::assets::InventoryItem> hero_items_;
    std::vector<std::size_t> hero_visible_;
    std::int32_t npc_money_ = 0;
    std::int32_t hero_money_ = 0;

    std::uint32_t npc_filter_ = 0;
    std::uint32_t hero_filter_ = 0;
    std::size_t npc_scroll_ = 0;
    std::size_t hero_scroll_ = 0;

    TradeSelection selection_ = TradeSelection::none;
    std::optional<std::size_t> selected_npc_;
    std::optional<std::size_t> selected_hero_;

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
