#include "gl/app/inventory_overlay.hpp"

#include <gl/assets/csx.hpp>
#include <gl/assets/image.hpp>
#include <gl/assets/item_catalog.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/render/text.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gl::app {
namespace {

constexpr float kCanvasWidth = 1024.0f;
constexpr float kCanvasHeight = 768.0f;
constexpr int kBottomY = 703;
constexpr int kHotY = 624;
constexpr std::array<int, 12> kBottomX{140, 201, 261, 321, 382, 443,
                                       504, 565, 627, 688, 749, 811};
constexpr std::array<int, 10> kHotX{17, 14, 43, 72, 101, 130, 159, 188, 217, 246};

struct PuppetSpec {
    std::uint32_t mask;
    int x;
    int y;
};

constexpr std::array<PuppetSpec, 12> kPuppetSlots{{
    {0x004000u, 67, 307},  {0x008000u, 168, 308}, {0x010000u, 0, 354},
    {0x020000u, 233, 354}, {0x040000u, 4, 402},   {0x080000u, 229, 402},
    {0x001000u, 22, 472},  {0x400000u, 216, 472}, {0x002000u, 16, 466},
    {0x800000u, 211, 466}, {0x100000u, 16, 547},  {0x200000u, 216, 547},
}};

struct FilterSpec {
    int x;
    int y;
    std::uint32_t mask;
};

constexpr std::array<FilterSpec, 7> kFilters{{
    {201, 672, 0x01u}, {290, 673, 0x02u}, {383, 673, 0x04u}, {474, 673, 0x08u},
    {565, 673, 0x10u}, {658, 673, 0x20u}, {745, 673, 0x00u},
}};

constexpr float kFilterWidth = 56.0f;
constexpr float kFilterHeight = 23.0f;

constexpr std::string_view kBackground = "engineres/newinv/inventory_secondary.bmp";
constexpr std::string_view kFallbackIcon = "engineres/interface/bag.csx";
constexpr float kPanelNameLine = 18.0f;
constexpr float kPanelStatLine = 18.0f;
constexpr float kPanelDescLine = 16.0f;

bool contains(float rx, float ry, float rw, float rh, float x, float y) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

bool lower_suffix(std::string_view path, std::string_view suffix) {
    if (path.size() < suffix.size())
        return false;
    const std::size_t offset = path.size() - suffix.size();
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        char c = path[offset + i];
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if (c != suffix[i])
            return false;
    }
    return true;
}

struct PanelLines {
    std::vector<std::string> name;
    std::array<std::string, 3> stats;
    std::vector<std::string> description;
    float content_height = 0.0f;
    bool show_scroll = false;
};

std::string format_weight(std::uint32_t weight_x10) {
    return std::to_string(weight_x10 / 10) + "." + std::to_string(weight_x10 % 10);
}

void erase_last_codepoint(std::string &text) {
    if (text.empty())
        return;
    std::size_t index = text.size() - 1;
    while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0u) == 0x80u)
        --index;
    text.erase(index);
}

std::string ellipsize(const gl::render::Font &font, std::string text, float width) {
    constexpr std::string_view kDots = "...";
    while (!text.empty() && font.measure(text + std::string(kDots)).x > width)
        erase_last_codepoint(text);
    text.append(kDots);
    return text;
}

PanelLines build_panel_lines(const gl::assets::ItemCatalogEntry &entry,
                             const gl::render::Font &font) {
    constexpr float kContentWidth = 340.0f;
    constexpr float kScrollWidth = 332.0f;
    constexpr float kContentHeight = 500.0f;
    constexpr float kNameLine = 18.0f;
    constexpr float kStatLine = 18.0f;
    constexpr float kDescLine = 16.0f;

    PanelLines lines;
    lines.name = font.wrap(entry.name, kContentWidth);
    if (lines.name.size() > 2) {
        lines.name.resize(2);
        lines.name[1] = ellipsize(font, lines.name[1], kContentWidth);
    }
    lines.stats = {
        "Type: " + std::to_string(static_cast<std::uint32_t>(entry.item.type)),
        "Price: " + std::to_string(entry.item.price),
        "Weight: " + format_weight(entry.item.weight_x10),
    };
    lines.description = font.wrap(entry.description, kContentWidth);

    const auto content_height = [&]() {
        return static_cast<float>(lines.name.size()) * kNameLine + 3.0f * kStatLine +
               static_cast<float>(lines.description.size()) * kDescLine;
    };
    lines.content_height = content_height();
    lines.show_scroll = lines.content_height > kContentHeight;
    if (lines.show_scroll) {
        lines.description = font.wrap(entry.description, kScrollWidth);
        lines.content_height = content_height();
    }
    return lines;
}

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

} // namespace

bool InventoryOverlay::open(const gl::assets::GameVfs &vfs,
                            const gl::assets::ItemCatalog &catalog,
                            const gl::assets::HeroInventory &inventory) {
    vfs_ = &vfs;
    catalog_ = &catalog;
    inventory_ = inventory;
    view_ = {};
    visible_indices_.clear();
    selection_ = InventorySelection::none;
    panel_scroll_ = 0;
    panel_scroll_max_ = 0;
    visible_ = true;
    screen_w_ = 1024;
    screen_h_ = 768;
    update_transform(screen_w_, screen_h_);
    reset_texture_cache(nullptr);
    rebuild_visible();
    return true;
}

void InventoryOverlay::select(InventorySelection source, std::size_t index) {
    selection_ = source;
    view_.selected = index;
    panel_scroll_ = 0;
    panel_scroll_max_ = 0;
}

void InventoryOverlay::close() { visible_ = false; }

void InventoryOverlay::rebuild_visible() {
    visible_indices_ = gl::game::visible_inventory(inventory_, view_.filter_mask);
    view_.page = gl::game::clamp_page(view_.page, visible_indices_.size());
    if (view_.selected.has_value() && *view_.selected >= inventory_.items.size())
        view_.selected.reset();
}

void InventoryOverlay::update_transform(int screen_w, int screen_h) {
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
    if (scale_ <= 0.0f)
        scale_ = 1.0f;
    offset_x_ = (static_cast<float>(screen_w) - kCanvasWidth * scale_) * 0.5f;
    offset_y_ = (static_cast<float>(screen_h) - kCanvasHeight * scale_) * 0.5f;
}

void InventoryOverlay::reset_texture_cache(SDL_Renderer *renderer) {
    textures_.clear();
    failed_textures_.clear();
    texture_renderer_ = renderer;
}

gl::render::Texture *InventoryOverlay::load_texture(std::string_view path, bool magenta_key) {
    if (vfs_ == nullptr || texture_renderer_ == nullptr)
        return nullptr;
    const std::string key = std::string(path) + (magenta_key ? "#magenta" : "");
    const auto cached = textures_.find(key);
    if (cached != textures_.end())
        return &cached->second;
    if (failed_textures_.find(key) != failed_textures_.end())
        return nullptr;

    auto data = vfs_->read(path);
    if (!data) {
        failed_textures_.insert(key);
        return nullptr;
    }

    gl::assets::Image image;
    if (lower_suffix(path, ".csx")) {
        auto decoded = gl::assets::decode_csx(data.value());
        if (!decoded) {
            failed_textures_.insert(key);
            return nullptr;
        }
        image.width = decoded->width;
        image.height = decoded->height;
        image.rgba = std::move(decoded->rgba);
    } else {
        auto decoded = gl::assets::decode_image(data.value());
        if (!decoded) {
            failed_textures_.insert(key);
            return nullptr;
        }
        image = std::move(decoded.value());
        if (magenta_key)
            apply_magenta_key(image);
    }

    auto created = gl::render::Texture::from_image(texture_renderer_, image);
    if (!created) {
        failed_textures_.insert(key);
        return nullptr;
    }
    auto inserted = textures_.emplace(key, std::move(created.value()));
    return &inserted.first->second;
}

gl::render::Texture *InventoryOverlay::icon_texture(const gl::assets::ItemCatalogEntry *entry) {
    if (entry != nullptr) {
        if (lower_suffix(entry->icon_path, ".bmp") || lower_suffix(entry->icon_path, ".csx")) {
            if (gl::render::Texture *texture = load_texture(entry->icon_path, true);
                texture != nullptr)
                return texture;
        } else if (!entry->icon_path.empty()) {
            if (gl::render::Texture *texture = load_texture(entry->icon_path + ".bmp", true);
                texture != nullptr)
                return texture;
            if (gl::render::Texture *texture = load_texture(entry->icon_path + ".csx", true);
                texture != nullptr)
                return texture;
        }
    }
    return load_texture(kFallbackIcon, true);
}

gl::render::Texture *InventoryOverlay::background_texture() { return load_texture(kBackground); }

const gl::assets::InventoryItem *InventoryOverlay::selected_item() const {
    if (!view_.selected.has_value())
        return nullptr;
    const std::size_t index = *view_.selected;
    switch (selection_) {
    case InventorySelection::item:
        return index < inventory_.items.size() ? &inventory_.items[index] : nullptr;
    case InventorySelection::hot:
        return index < inventory_.hot.size() ? &inventory_.hot[index] : nullptr;
    case InventorySelection::equipped:
        return index < inventory_.equipped.size() ? &inventory_.equipped[index] : nullptr;
    case InventorySelection::none:
        break;
    }
    return nullptr;
}

const gl::assets::ItemCatalogEntry *InventoryOverlay::selected_entry() const {
    const gl::assets::InventoryItem *item = selected_item();
    if (catalog_ == nullptr || item == nullptr)
        return nullptr;
    return catalog_->find(item->item_id);
}

std::string InventoryOverlay::selected_name() const {
    const gl::assets::ItemCatalogEntry *entry = selected_entry();
    return entry != nullptr ? entry->name : std::string{};
}

std::vector<std::string> InventoryOverlay::wrap_text(std::string_view text, float width,
                                                     const gl::render::Font *font) const {
    if (font != nullptr)
        return font->wrap(text, width);
    return wrap_fallback(text, width, 1.0f);
}

void InventoryOverlay::draw_native(SDL_Renderer *renderer, const gl::render::Texture &texture,
                                   float x, float y, float target_w, float target_h) const {
    const float texture_w = static_cast<float>(texture.width());
    const float texture_h = static_cast<float>(texture.height());
    const float off_x = native_icon_offset(target_w, texture_w);
    const float off_y = native_icon_offset(target_h, texture_h);
    const SDL_FRect destination{x + off_x, y + off_y, texture_w, texture_h};
    SDL_RenderCopyF(renderer, texture.get(), nullptr, &destination);
}

void InventoryOverlay::draw_rect(SDL_Renderer *renderer, const gl::render::Texture &texture,
                                 const Rect &rect) const {
    const SDL_FRect destination{rect.x, rect.y, rect.w, rect.h};
    SDL_RenderCopyF(renderer, texture.get(), nullptr, &destination);
}

void InventoryOverlay::draw_panel(SDL_Renderer *renderer, const gl::render::Font *font) {
    const gl::assets::ItemCatalogEntry *entry = selected_entry();
    panel_scroll_max_ = 0;
    if (entry == nullptr)
        return;

    const Rect outer{654.0f, 44.0f, 360.0f, 520.0f};
    const Rect content{664.0f, 54.0f, 340.0f, 500.0f};

    SDL_SetRenderDrawColor(renderer, 14, 16, 22, 235);
    const SDL_FRect outer_rect{outer.x, outer.y, outer.w, outer.h};
    SDL_RenderFillRectF(renderer, &outer_rect);
    SDL_SetRenderDrawColor(renderer, 198, 168, 96, 255);
    SDL_RenderDrawRectF(renderer, &outer_rect);

    if (font == nullptr)
        return;

    const PanelLines lines = build_panel_lines(*entry, *font);
    if (lines.content_height > content.h) {
        const float max_scroll = lines.content_height - content.h;
        panel_scroll_max_ = static_cast<std::size_t>(std::ceil(max_scroll));
    }
    if (panel_scroll_ > panel_scroll_max_)
        panel_scroll_ = panel_scroll_max_;

    SDL_Rect old_clip{};
    SDL_RenderGetClipRect(renderer, &old_clip);
    const bool had_clip = SDL_RenderIsClipEnabled(renderer) == SDL_TRUE;
    const SDL_Rect clip{
        static_cast<int>(std::lround(content.x)),
        static_cast<int>(std::lround(content.y)),
        static_cast<int>(std::lround(content.w)),
        static_cast<int>(std::lround(content.h)),
    };
    SDL_RenderSetClipRect(renderer, &clip);

    const gl::render::Color name_color{255, 232, 160, 255};
    const gl::render::Color stat_color{200, 205, 215, 255};
    const gl::render::Color desc_color{214, 214, 222, 255};
    float y = content.y - static_cast<float>(panel_scroll_);

    const auto draw_lines = [&](const auto &text_lines, float line_height,
                                gl::render::Color color) {
        for (const std::string &line : text_lines) {
            if (y + line_height > content.y && y < content.y + content.h)
                font->draw_text(renderer, line, content.x, y, color);
            y += line_height;
        }
    };
    draw_lines(lines.name, kPanelNameLine, name_color);
    draw_lines(lines.stats, kPanelStatLine, stat_color);
    draw_lines(lines.description, kPanelDescLine, desc_color);

    SDL_RenderSetClipRect(renderer, had_clip ? &old_clip : nullptr);

    if (lines.show_scroll) {
        const float track_x = content.x + content.w - 8.0f;
        SDL_SetRenderDrawColor(renderer, 54, 56, 66, 255);
        const SDL_FRect track{track_x, content.y, 8.0f, content.h};
        SDL_RenderFillRectF(renderer, &track);

        float thumb_height = content.h * (content.h / lines.content_height);
        if (thumb_height < 20.0f)
            thumb_height = 20.0f;
        const float travel = content.h - thumb_height;
        const float ratio =
            panel_scroll_max_ > 0
                ? static_cast<float>(panel_scroll_) / static_cast<float>(panel_scroll_max_)
                : 0.0f;
        SDL_SetRenderDrawColor(renderer, 150, 130, 80, 255);
        const SDL_FRect thumb{track_x, content.y + travel * ratio, 8.0f, thumb_height};
        SDL_RenderFillRectF(renderer, &thumb);
    }
}

std::size_t InventoryOverlay::panel_max_scroll(const gl::render::Font &font) const {
    const gl::assets::ItemCatalogEntry *entry = selected_entry();
    if (entry == nullptr)
        return 0;
    const PanelLines lines = build_panel_lines(*entry, font);
    const float max_scroll = lines.content_height - 500.0f;
    return max_scroll > 0.0f ? static_cast<std::size_t>(std::ceil(max_scroll)) : 0;
}

float InventoryOverlay::native_icon_offset(float target, float texture_size) {
    return std::max(0.0f, (target - texture_size) * 0.5f);
}

void InventoryOverlay::apply_magenta_key(gl::assets::Image &image) {
    for (gl::assets::Rgba &pixel : image.rgba) {
        if (pixel.r == 255 && pixel.g == 0 && pixel.b == 255)
            pixel.a = 0;
    }
}

void InventoryOverlay::render(SDL_Renderer *renderer, int screen_w, int screen_h,
                              const gl::render::Font *font) {
    if (renderer == nullptr || !visible_ || screen_w <= 0 || screen_h <= 0)
        return;
    if (texture_renderer_ != renderer)
        reset_texture_cache(renderer);
    update_transform(screen_w, screen_h);

    float old_scale_x = 1.0f;
    float old_scale_y = 1.0f;
    SDL_RenderGetScale(renderer, &old_scale_x, &old_scale_y);
    SDL_Rect old_viewport{};
    SDL_RenderGetViewport(renderer, &old_viewport);
    Uint8 old_r = 0;
    Uint8 old_g = 0;
    Uint8 old_b = 0;
    Uint8 old_a = 0;
    SDL_GetRenderDrawColor(renderer, &old_r, &old_g, &old_b, &old_a);
    SDL_BlendMode old_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);

    const SDL_Rect viewport{
        static_cast<int>(std::lround(offset_x_)),
        static_cast<int>(std::lround(offset_y_)),
        std::max(1, static_cast<int>(std::lround(kCanvasWidth * scale_))),
        std::max(1, static_cast<int>(std::lround(kCanvasHeight * scale_))),
    };
    SDL_RenderSetViewport(renderer, &viewport);
    SDL_RenderSetScale(renderer, scale_, scale_);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (gl::render::Texture *background = background_texture(); background != nullptr) {
        draw_rect(renderer, *background, Rect{0.0f, 0.0f, kCanvasWidth, kCanvasHeight});
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        const SDL_FRect canvas{0.0f, 0.0f, kCanvasWidth, kCanvasHeight};
        SDL_RenderFillRectF(renderer, &canvas);
    }

    for (std::size_t slot = 0; slot < kBottomX.size(); ++slot) {
        const std::size_t index = view_.page + slot;
        const gl::render::Texture *icon = nullptr;
        const gl::assets::InventoryItem *item = nullptr;
        if (index < visible_indices_.size()) {
            item = &inventory_.items[visible_indices_[index]];
            const gl::assets::ItemCatalogEntry *entry =
                catalog_ != nullptr ? catalog_->find(item->item_id) : nullptr;
            icon = icon_texture(entry);
        }

        const float x = static_cast<float>(kBottomX[slot]);
        const float y = static_cast<float>(kBottomY);
        if (item != nullptr) {
            if (icon != nullptr) {
                draw_native(renderer, *icon, x, y, 55.0f, 55.0f);
            } else {
                SDL_SetRenderDrawColor(renderer, 22, 24, 30, 200);
                const SDL_FRect fill{x, y, 55.0f, 55.0f};
                SDL_RenderFillRectF(renderer, &fill);
            }
        }

        if (font != nullptr && item != nullptr && item->count > 1) {
            const std::string count = std::to_string(item->count);
            const gl::render::Vec2 size = font->measure(count);
            font->draw_text(renderer, count,
                            x + 55.0f - 2.0f - size.x,
                            y + 55.0f - 2.0f - size.y,
                            gl::render::Color{255, 255, 255, 255});
        }
    }

    for (std::size_t slot = 1; slot < kHotX.size(); ++slot) {
        const gl::assets::InventoryItem *item = nullptr;
        for (const gl::assets::InventoryItem &candidate : inventory_.hot) {
            if (((candidate.flags & 0x0000F000u) >> 12) == slot) {
                item = &candidate;
                break;
            }
        }
        if (item == nullptr)
            continue;
        const gl::assets::ItemCatalogEntry *entry =
            catalog_ != nullptr ? catalog_->find(item->item_id) : nullptr;
        const gl::render::Texture *icon = icon_texture(entry);
        const float x = static_cast<float>(kHotX[slot]);
        const float y = static_cast<float>(kHotY);
        if (icon != nullptr) {
            draw_native(renderer, *icon, x, y, 27.0f, 27.0f);
        } else {
            SDL_SetRenderDrawColor(renderer, 22, 24, 30, 200);
            const SDL_FRect fill{x, y, 27.0f, 27.0f};
            SDL_RenderFillRectF(renderer, &fill);
        }
        if (font != nullptr && item->count > 1) {
            const std::string count = std::to_string(item->count);
            const gl::render::Vec2 size = font->measure(count);
            font->draw_text(renderer, count,
                            x + 27.0f - 2.0f - size.x,
                            y + 27.0f - 2.0f - size.y,
                            gl::render::Color{255, 255, 255, 255});
        }
    }

    for (const PuppetSpec &slot : kPuppetSlots) {
        const gl::assets::InventoryItem *item = nullptr;
        for (const gl::assets::InventoryItem &candidate : inventory_.equipped) {
            if ((candidate.flags & 0x00FFF000u) == slot.mask) {
                item = &candidate;
                break;
            }
        }
        if (item == nullptr)
            continue;
        const gl::assets::ItemCatalogEntry *entry =
            catalog_ != nullptr ? catalog_->find(item->item_id) : nullptr;
        const gl::render::Texture *icon = icon_texture(entry);
        const float x = static_cast<float>(slot.x);
        const float y = static_cast<float>(slot.y);
        if (icon != nullptr) {
            draw_native(renderer, *icon, x, y, 55.0f, 55.0f);
        } else {
            SDL_SetRenderDrawColor(renderer, 22, 24, 30, 200);
            const SDL_FRect fill{x, y, 55.0f, 55.0f};
            SDL_RenderFillRectF(renderer, &fill);
        }
    }

    for (std::size_t i = 0; i < kFilters.size(); ++i) {
        const std::string path =
            "engineres/newinv/filter" + std::to_string(i + 1) + ".bmp";
        if (gl::render::Texture *texture = load_texture(path); texture != nullptr) {
            draw_rect(renderer, *texture,
                      Rect{static_cast<float>(kFilters[i].x), static_cast<float>(kFilters[i].y),
                           kFilterWidth, kFilterHeight});
        }
    }

    if (gl::render::Texture *left = load_texture("engineres/newinv/inv_left.bmp");
        left != nullptr) {
        draw_rect(renderer, *left, Rect{110.0f, 702.0f, 27.0f, 55.0f});
    }
    if (gl::render::Texture *right = load_texture("engineres/newinv/inv_right.bmp");
        right != nullptr) {
        draw_rect(renderer, *right, Rect{870.0f, 703.0f, 26.0f, 54.0f});
    }
    if (gl::render::Texture *exit = load_texture("engineres/newinv/exitl.bmp");
        exit != nullptr) {
        draw_rect(renderer, *exit, Rect{920.0f, 694.0f, 75.0f, 68.0f});
    }
    if (gl::render::Texture *drop = load_texture("engineres/newinv/drop.bmp");
        drop != nullptr) {
        draw_rect(renderer, *drop, Rect{19.0f, 690.0f, 81.0f, 75.0f});
    }

    draw_panel(renderer, font);

    SDL_RenderSetScale(renderer, old_scale_x, old_scale_y);
    SDL_RenderSetViewport(renderer, &old_viewport);
    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    SDL_SetRenderDrawColor(renderer, old_r, old_g, old_b, old_a);
}

bool InventoryOverlay::handle_mouse(int x, int y, bool click) {
    if (!visible_)
        return false;
    const InventoryRegion region = hit_test(x, y);
    if (region == InventoryRegion::none)
        return false;
    if (!click)
        return true;

    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;

    switch (region) {
    case InventoryRegion::exit:
        close();
        break;
    case InventoryRegion::scroll_left:
        if (view_.page > 0)
            --view_.page;
        break;
    case InventoryRegion::scroll_right:
        view_.page = gl::game::clamp_page(view_.page + 1, visible_indices_.size());
        break;
    case InventoryRegion::filter:
        for (const FilterSpec &filter : kFilters) {
            if (contains(static_cast<float>(filter.x), static_cast<float>(filter.y), kFilterWidth,
                         kFilterHeight, logical_x, logical_y)) {
                view_.filter_mask = filter.mask;
                view_.page = 0;
                rebuild_visible();
                break;
            }
        }
        break;
    case InventoryRegion::bottom_slot:
        for (std::size_t slot = 0; slot < kBottomX.size(); ++slot) {
            if (!contains(static_cast<float>(kBottomX[slot]), static_cast<float>(kBottomY), 55.0f,
                          55.0f, logical_x, logical_y))
                continue;
            const std::size_t index = view_.page + slot;
            if (index < visible_indices_.size()) {
                select(InventorySelection::item, visible_indices_[index]);
            } else {
                selection_ = InventorySelection::none;
                view_.selected.reset();
                panel_scroll_ = 0;
                panel_scroll_max_ = 0;
            }
            break;
        }
        break;
    case InventoryRegion::hot_slot:
        for (std::size_t slot = 1; slot < kHotX.size(); ++slot) {
            if (!contains(static_cast<float>(kHotX[slot]), static_cast<float>(kHotY), 27.0f, 27.0f,
                          logical_x, logical_y))
                continue;
            for (std::size_t index = 0; index < inventory_.hot.size(); ++index) {
                if (((inventory_.hot[index].flags & 0x0000F000u) >> 12) == slot) {
                    select(InventorySelection::hot, index);
                    break;
                }
            }
            break;
        }
        break;
    case InventoryRegion::puppet_slot:
        for (const PuppetSpec &puppet : kPuppetSlots) {
            if (!contains(static_cast<float>(puppet.x), static_cast<float>(puppet.y), 55.0f, 55.0f,
                          logical_x, logical_y))
                continue;
            for (std::size_t index = 0; index < inventory_.equipped.size(); ++index) {
                if ((inventory_.equipped[index].flags & 0x00FFF000u) == puppet.mask) {
                    select(InventorySelection::equipped, index);
                    break;
                }
            }
            break;
        }
        break;
    case InventoryRegion::none:
    case InventoryRegion::skills_tab:
    case InventoryRegion::drop:
    case InventoryRegion::panel:
        break;
    }
    return true;
}

bool InventoryOverlay::handle_key(SDL_Keycode key) {
    if (key == SDLK_ESCAPE) {
        if (!visible_)
            return false;
        close();
        return true;
    }
    if (key == SDLK_i || key == static_cast<SDL_Keycode>('I')) {
        if (catalog_ == nullptr)
            return false;
        visible_ = !visible_;
        return true;
    }
    if (key == SDLK_PAGEDOWN || key == SDLK_PAGEUP) {
        if (!visible_ || panel_scroll_max_ == 0)
            return false;
        constexpr std::size_t kPageStep = 90;
        if (key == SDLK_PAGEDOWN) {
            panel_scroll_ = std::min(panel_scroll_ + kPageStep, panel_scroll_max_);
        } else {
            panel_scroll_ = panel_scroll_ > kPageStep ? panel_scroll_ - kPageStep : 0;
        }
        return true;
    }
    return false;
}

bool InventoryOverlay::handle_wheel(int delta) {
    if (!visible_ || delta == 0)
        return false;
    const int step = delta > 0 ? 3 : -3;
    const long long next = static_cast<long long>(view_.page) + step;
    view_.page = gl::game::clamp_page(next < 0 ? 0 : static_cast<std::size_t>(next),
                                     visible_indices_.size());
    return true;
}

bool InventoryOverlay::handle_wheel(int delta, int x, int y) {
    if (!visible_ || delta == 0)
        return false;
    if (hit_test(x, y) != InventoryRegion::panel)
        return handle_wheel(delta);
    if (panel_scroll_max_ == 0)
        return true;
    constexpr std::size_t kWheelStep = 54;
    if (delta > 0)
        panel_scroll_ = std::min(panel_scroll_ + kWheelStep, panel_scroll_max_);
    else
        panel_scroll_ = panel_scroll_ > kWheelStep ? panel_scroll_ - kWheelStep : 0;
    return true;
}

InventoryRegion InventoryOverlay::hit_test(int x, int y) const {
    if (!visible_ || scale_ <= 0.0f)
        return InventoryRegion::none;
    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;
    if (logical_x < 0.0f || logical_y < 0.0f || logical_x >= kCanvasWidth ||
        logical_y >= kCanvasHeight)
        return InventoryRegion::none;

    if (contains(920.0f, 694.0f, 75.0f, 68.0f, logical_x, logical_y))
        return InventoryRegion::exit;
    if (contains(110.0f, 702.0f, 27.0f, 55.0f, logical_x, logical_y))
        return InventoryRegion::scroll_left;
    if (contains(870.0f, 703.0f, 26.0f, 54.0f, logical_x, logical_y))
        return InventoryRegion::scroll_right;

    for (const FilterSpec &filter : kFilters) {
        if (contains(static_cast<float>(filter.x), static_cast<float>(filter.y), kFilterWidth,
                     kFilterHeight, logical_x, logical_y))
            return InventoryRegion::filter;
    }

    for (const int slot_x : kBottomX) {
        if (contains(static_cast<float>(slot_x), static_cast<float>(kBottomY), 55.0f, 55.0f,
                     logical_x, logical_y))
            return InventoryRegion::bottom_slot;
    }

    for (std::size_t i = 1; i < kHotX.size(); ++i) {
        if (contains(static_cast<float>(kHotX[i]), static_cast<float>(kHotY), 27.0f, 27.0f,
                     logical_x, logical_y))
            return InventoryRegion::hot_slot;
    }

    for (const PuppetSpec &slot : kPuppetSlots) {
        if (contains(static_cast<float>(slot.x), static_cast<float>(slot.y), 55.0f, 55.0f,
                     logical_x, logical_y))
            return InventoryRegion::puppet_slot;
    }

    if (contains(19.0f, 690.0f, 81.0f, 75.0f, logical_x, logical_y))
        return InventoryRegion::drop;
    if (contains(280.0f, 4.0f, 368.0f, 40.0f, logical_x, logical_y))
        return InventoryRegion::skills_tab;
    if (contains(654.0f, 44.0f, 360.0f, 520.0f, logical_x, logical_y))
        return InventoryRegion::panel;

    return InventoryRegion::none;
}

} // namespace gl::app
