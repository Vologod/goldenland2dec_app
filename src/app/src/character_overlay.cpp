#include "gl/app/character_overlay.hpp"
#include "gl/assets/cp1251.hpp"
#include "gl/assets/csx.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/image.hpp"
#include "gl/assets/item_catalog.hpp"
#include "gl/assets/vfs.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace gl::app {
namespace {
constexpr float kCanvasWidth = 1024.0f;
constexpr float kCanvasHeight = 768.0f;
constexpr std::string_view kBackgroundCharacteristics =
    "engineres/newinv/inventory_secondary.bmp";
constexpr std::string_view kBackgroundSkills =
    "engineres/newinv/inventory_skills.bmp";
constexpr std::string_view kFallbackIcon = "engineres/interface/bag.csx";
struct TextRect {
    float x;
    float y;
    float w;
    float h;
};

constexpr float kPrimaryLabelX = 30.0f;
constexpr std::array<float, 7> kPrimaryY{61.0f, 87.0f, 114.0f, 142.0f,
                                         169.0f, 197.0f, 224.0f};
constexpr float kPrimaryValueX = 197.0f;
constexpr float kPrimaryValueW = 30.0f;
constexpr std::array<float, 7> kPrimaryValueY{60.0f, 87.0f, 115.0f, 143.0f,
                                              171.0f, 198.0f, 225.0f};
constexpr float kExperienceLabelX = 30.0f;
constexpr float kExperienceY = 261.0f;
constexpr float kExperienceValueX = 216.0f;
constexpr float kExperienceValueW = 38.0f;
constexpr float kRowValueH = 18.0f;
constexpr float kCenterLabelX = 308.0f;
constexpr std::array<float, 6> kStateY{103.0f, 134.0f, 162.0f, 212.0f,
                                       238.0f, 265.0f};
constexpr std::array<TextRect, 6> kStateValueRect{{
    {588.0f, 103.0f, 41.0f, 18.0f}, {471.0f, 133.0f, 159.0f, 18.0f},
    {588.0f, 161.0f, 41.0f, 18.0f}, {480.0f, 212.0f, 150.0f, 16.0f},
    {480.0f, 238.0f, 150.0f, 16.0f}, {572.0f, 265.0f, 56.0f, 16.0f}}};
constexpr std::array<float, 10> kCombatY{348.0f, 375.0f, 402.0f, 429.0f,
                                         456.0f, 483.0f, 510.0f, 537.0f,
                                         564.0f, 598.0f};
constexpr std::array<TextRect, 10> kCombatValueRect{{
    {582.0f, 348.0f, 46.0f, 15.0f}, {582.0f, 375.0f, 46.0f, 15.0f},
    {582.0f, 402.0f, 46.0f, 15.0f}, {582.0f, 430.0f, 46.0f, 15.0f},
    {582.0f, 456.0f, 46.0f, 15.0f}, {582.0f, 483.0f, 46.0f, 15.0f},
    {582.0f, 510.0f, 46.0f, 15.0f}, {582.0f, 537.0f, 46.0f, 15.0f},
    {582.0f, 565.0f, 46.0f, 15.0f}, {497.0f, 597.0f, 130.0f, 18.0f}}};
constexpr float kWeightLabelX = 300.0f;
constexpr float kResistanceLabelX = 673.0f;
constexpr std::array<float, 12> kResistanceY{104.0f, 136.0f, 168.0f, 221.0f,
                                             251.0f, 282.0f, 312.0f, 343.0f,
                                             373.0f, 430.0f, 455.0f, 482.0f};
constexpr std::array<float, 12> kResistanceValueY{103.0f, 136.0f, 168.0f,
                                                  220.0f, 251.0f, 282.0f,
                                                  312.0f, 343.0f, 373.0f,
                                                  430.0f, 456.0f, 482.0f};
constexpr float kResistanceValueX = 950.0f;
constexpr float kResistanceValueW = 47.0f;
constexpr float kResistanceValueH = 17.0f;
constexpr float kImmunityLeftLabelX = 673.0f;
constexpr float kImmunityRightLabelX = 850.0f;
constexpr std::array<float, 3> kImmunityY{570.0f, 596.0f, 622.0f};
constexpr float kImmunityLeftValueX = 774.0f;
constexpr float kImmunityRightValueX = 950.0f;
constexpr float kImmunityValueW = 43.0f;
constexpr float kImmunityValueH = 16.0f;
constexpr std::array<float, 3> kSkillColumnX{295.0f, 544.0f, 790.0f};
constexpr std::array<float, 9> kSkillRowY{124.0f, 156.0f, 188.0f, 309.0f,
                                          342.0f, 375.0f, 494.0f, 526.0f,
                                          558.0f};
constexpr std::array<float, 3> kSkillValueX{460.0f, 706.0f, 952.0f};
constexpr std::array<float, 9> kSkillValueY{123.0f, 156.0f, 188.0f, 310.0f,
                                            342.0f, 374.0f, 494.0f, 527.0f,
                                            559.0f};
constexpr float kSkillValueW = 31.0f;
constexpr float kSkillValueH = 18.0f;
constexpr std::array<float, 3> kSkillGroupY{78.0f, 264.0f, 449.0f};
constexpr float kSkillPointsLabelX = 526.0f;
constexpr float kSkillPointsY = 626.0f;
constexpr float kSkillPointsValueX = 739.0f;
constexpr float kSkillPointsValueY = 623.0f;
constexpr float kSkillPointsValueW = 45.0f;
constexpr float kSkillPointsValueH = 23.0f;
constexpr gl::render::Color kTextColor{20, 18, 16, 255};
constexpr gl::render::Color kActiveTab{20, 18, 16, 255};
constexpr gl::render::Color kInactiveTab{70, 70, 70, 255};
constexpr gl::render::Color kNameColor{20, 18, 16, 255};

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

constexpr std::array<std::uint32_t, 6> kDollLayerMasks{
    0x200000u, 0x100000u, 0x0F8000u, 0x004000u, 0xC00000u, 0x003000u};
constexpr float kDollLeft = 5.0f;
constexpr float kDollBottom = 622.0f;
constexpr int kDollHalfWidth = 135;
constexpr int kDollHalfSlice = 30;

std::uint32_t equippable_mask(std::uint32_t flags) { return flags & 0x00FFF000u; }

bool doll_layer_skipped(std::uint32_t mask, bool use_b_set) {
    if (use_b_set) return mask == 0x001000u || mask == 0x400000u;
    return mask == 0x002000u || mask == 0x800000u;
}

bool doll_half_canvas(std::uint32_t mask) { return (mask & 0x000F0000u) != 0; }

bool doll_left_half(std::uint32_t mask) { return (mask & 0x00050000u) != 0; }

std::string doll_puppet_path(std::string_view puppet_name) {
    std::string path =
        "items/res/" + gl::assets::normalize_virtual_path(puppet_name);
    const bool has_extension =
        path.size() >= 4 && path.compare(path.size() - 4, 4, ".bmp") == 0;
    if (!has_extension) path += ".bmp";
    return path;
}
constexpr std::array<PuppetSpec, 12> kPuppetSlots{{
    {0x004000u, 67, 307},  {0x008000u, 168, 308}, {0x010000u, 0, 354},
    {0x020000u, 233, 354}, {0x040000u, 4, 402},   {0x080000u, 229, 402},
    {0x001000u, 22, 472},  {0x400000u, 216, 472}, {0x002000u, 16, 466},
    {0x800000u, 211, 466}, {0x100000u, 16, 547},  {0x200000u, 216, 547},
}};

bool contains(float rx, float ry, float rw, float rh, float x, float y) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

void apply_magenta_key(gl::assets::Image &image) {
    for (gl::assets::Rgba &pixel : image.rgba) {
        if (pixel.r == 255 && pixel.g == 0 && pixel.b == 255) pixel.a = 0;
    }
}

const gl::assets::InventoryItem *find_equipped(
    const gl::assets::HeroInventory &inventory, std::uint32_t mask) {
    for (const gl::assets::InventoryItem &candidate : inventory.equipped) {
        if ((candidate.flags & 0x00FFF000u) == mask) return &candidate;
    }
    return nullptr;
}

bool puppet_visible(const gl::assets::HeroInventory &inventory,
                    const PuppetSpec &slot, bool use_b_set) {
    const bool has_weapon_a = find_equipped(inventory, 0x001000u) != nullptr;
    const bool has_weapon_b = find_equipped(inventory, 0x002000u) != nullptr;
    const bool has_shield_a = find_equipped(inventory, 0x400000u) != nullptr;
    const bool has_shield_b = find_equipped(inventory, 0x800000u) != nullptr;
    if (slot.mask == 0x001000u) return use_b_set ? !has_weapon_b : true;
    if (slot.mask == 0x002000u) return use_b_set ? has_weapon_b : !has_weapon_a;
    if (slot.mask == 0x400000u) return use_b_set ? !has_shield_b : true;
    if (slot.mask == 0x800000u) return use_b_set ? has_shield_b : !has_shield_a;
    return true;
}

const gl::assets::InventoryItem *find_hot(
    const gl::assets::HeroInventory &inventory, std::size_t slot) {
    for (const gl::assets::InventoryItem &candidate : inventory.hot) {
        if (((candidate.flags & 0x0000F000u) >> 12) == slot) return &candidate;
    }
    return nullptr;
}
} // namespace

bool CharacterOverlay::open(SDL_Renderer *renderer, const gl::assets::GameVfs &vfs,
                            gl::game::CharacterSheet sheet,
                            const gl::assets::HeroInventory *inventory,
                            const gl::assets::ItemCatalog *catalog, bool use_b_set) {
    vfs_ = &vfs;
    inventory_ = inventory;
    catalog_ = catalog;
    use_b_set_ = use_b_set;
    sheet_ = std::move(sheet);
    tab_ = gl::game::CharacterTab::characteristics;
    visible_ = true;
    scale_ = 1.0f;
    offset_x_ = 0.0f;
    offset_y_ = 0.0f;
    if (renderer != nullptr) reset_texture_cache(renderer);
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

gl::render::Texture *CharacterOverlay::load_texture(std::string_view path,
                                                    bool magenta_key) {
    if (vfs_ == nullptr || texture_renderer_ == nullptr) return nullptr;
    const std::string key = std::string(path) + (magenta_key ? "#magenta" : "");
    const auto cached = textures_.find(key);
    if (cached != textures_.end()) return &cached->second;
    if (failed_textures_.count(key) != 0) return nullptr;
    auto data = vfs_->read(path);
    if (!data) {
        failed_textures_.insert(key);
        return nullptr;
    }
    gl::assets::Image image;
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".csx") {
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
    }
    if (magenta_key) apply_magenta_key(image);
    auto created = gl::render::Texture::from_image(texture_renderer_, image);
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
    std::uint8_t old_color_r = 0;
    std::uint8_t old_color_g = 0;
    std::uint8_t old_color_b = 0;
    std::uint8_t old_color_a = 255;
    SDL_GetRenderDrawColor(renderer, &old_color_r, &old_color_g, &old_color_b,
                           &old_color_a);
    const SDL_Rect viewport{
        static_cast<int>(std::lround(offset_x_)),
        static_cast<int>(std::lround(offset_y_)),
        std::max(1, static_cast<int>(std::lround(kCanvasWidth * scale_))),
        std::max(1, static_cast<int>(std::lround(kCanvasHeight * scale_)))};
    SDL_RenderSetViewport(renderer, &viewport);
    SDL_RenderSetScale(renderer, scale_, scale_);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    draw_background(renderer);
    draw_doll_overlays(renderer);
    draw_tabs(renderer);
    draw_content(renderer);
    draw_inventory_items(renderer);
    draw_close(renderer);

    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    SDL_SetRenderDrawColor(renderer, old_color_r, old_color_g, old_color_b,
                           old_color_a);
    SDL_RenderSetScale(renderer, old_scale_x, old_scale_y);
    SDL_RenderSetClipRect(renderer, old_clipping ? &old_clip : nullptr);
    SDL_RenderSetViewport(renderer, &old_viewport);
}

void CharacterOverlay::draw_background(SDL_Renderer *renderer) {
    const std::string_view path = tab_ == gl::game::CharacterTab::skills
                                      ? kBackgroundSkills
                                      : kBackgroundCharacteristics;
    background_ = load_texture(path);
    if (background_ != nullptr) {
        const SDL_FRect rect{0.0f, 0.0f, kCanvasWidth, kCanvasHeight};
        SDL_RenderCopyF(renderer, background_->get(), nullptr, &rect);
        return;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    const SDL_FRect rect{0.0f, 0.0f, kCanvasWidth, kCanvasHeight};
    SDL_RenderFillRectF(renderer, &rect);
}

void CharacterOverlay::draw_text_rect(SDL_Renderer *renderer,
                                      const gl::render::Font *font,
                                      std::string_view text, float x, float y,
                                      float w, float h, gl::render::Color color,
                                      bool bold) {
    if (font == nullptr || text.empty()) return;
    if (w >= 0.0f) {
        const float text_w = font->measure(text).x;
        if (text_w < w) x += (w - text_w) * 0.5f;
    }
    if (h >= 0.0f) {
        const float cell_h = font->text_height();
        if (cell_h < h) y += (h - cell_h) * 0.5f;
    }
    y += font->line_height() - font->text_height();
    font->draw_text(renderer, text, x, y, color);
    if (bold) font->draw_text(renderer, text, x + 1.0f, y, color);
}

void CharacterOverlay::draw_tabs(SDL_Renderer *renderer) {
    const gl::render::Font *title = top_font();
    if (title == nullptr) return;
    const bool characteristics =
        tab_ == gl::game::CharacterTab::characteristics;
    draw_text_rect(renderer, title, sheet_.name, 15.0f, 10.0f, 250.0f, 26.0f,
                   kNameColor, true);
    draw_text_rect(renderer, title, sheet_.characteristics_title, 292.0f, 16.0f,
                   344.0f, -1.0f,
                   characteristics ? kActiveTab : kInactiveTab, true);
    draw_text_rect(renderer, title, sheet_.skills_title, 661.0f, 16.0f, 344.0f,
                   -1.0f, characteristics ? kInactiveTab : kActiveTab, true);
}

void CharacterOverlay::draw_close(SDL_Renderer *renderer) {
    const std::string_view path =
        hover_close_ ? "engineres/newinv/exitl.bmp" : "engineres/newinv/exit.bmp";
    if (gl::render::Texture *texture = load_texture(path); texture != nullptr) {
        const SDL_FRect rect{920.0f, 694.0f, 75.0f, 68.0f};
        SDL_RenderCopyF(renderer, texture->get(), nullptr, &rect);
    }
}

void CharacterOverlay::draw_item(SDL_Renderer *renderer,
                                 const gl::assets::ItemCatalogEntry *entry,
                                 float x, float y, float w, float h) {
    std::vector<std::string> candidates;
    if (entry != nullptr && !entry->icon_path.empty()) {
        const std::string &path = entry->icon_path;
        const bool has_extension =
            (path.size() >= 4 && path.substr(path.size() - 4) == ".bmp") ||
            (path.size() >= 4 && path.substr(path.size() - 4) == ".csx");
        if (has_extension) {
            candidates.push_back(path);
        } else {
            candidates.push_back(path + ".bmp");
            candidates.push_back(path + ".csx");
        }
    }
    candidates.emplace_back(kFallbackIcon);
    gl::render::Texture *texture = nullptr;
    for (const std::string &path : candidates) {
        texture = load_texture(path, true);
        if (texture != nullptr) break;
    }
    if (texture == nullptr) return;
    int width = 0;
    int height = 0;
    SDL_QueryTexture(texture->get(), nullptr, nullptr, &width, &height);
    SDL_FRect dest{x, y, w, h};
    if (width > 0 && height > 0 && width <= w && height <= h) {
        dest.w = static_cast<float>(width);
        dest.h = static_cast<float>(height);
        dest.x = x + (w - dest.w) * 0.5f;
        dest.y = y + (h - dest.h) * 0.5f;
    }
    SDL_RenderCopyF(renderer, texture->get(), nullptr, &dest);
}

void CharacterOverlay::draw_doll_overlays(SDL_Renderer *renderer) {
    if (inventory_ == nullptr || catalog_ == nullptr) return;
    for (const std::uint32_t group : kDollLayerMasks) {
        for (const gl::assets::InventoryItem &item : inventory_->equipped) {
            const std::uint32_t mask = equippable_mask(item.flags);
            if ((mask & group) == 0) continue;
            if (doll_layer_skipped(mask, use_b_set_)) continue;
            const gl::assets::ItemCatalogEntry *entry =
                catalog_->find(item.item_id);
            if (entry == nullptr || entry->item.puppet_name.empty()) continue;
            gl::render::Texture *texture =
                load_texture(doll_puppet_path(entry->item.puppet_name), true);
            if (texture == nullptr) continue;
            int width = 0;
            int height = 0;
            SDL_QueryTexture(texture->get(), nullptr, nullptr, &width, &height);
            if (width <= 0 || height <= 0) continue;
            SDL_Rect source{0, 0, width, height};
            float x = kDollLeft;
            if (doll_half_canvas(mask)) {
                if (doll_left_half(mask)) {
                    source.w = std::min(width, kDollHalfWidth);
                } else {
                    const int x0 = width < kDollHalfWidth
                                       ? std::max(0, width - kDollHalfSlice)
                                       : kDollHalfWidth;
                    source.x = x0;
                    source.w = width - x0;
                    x = static_cast<float>(x0) + kDollLeft;
                }
            }
            const SDL_FRect destination{
                x, kDollBottom - static_cast<float>(height),
                static_cast<float>(source.w), static_cast<float>(source.h)};
            SDL_RenderCopyF(renderer, texture->get(), &source, &destination);
        }
    }
}

void CharacterOverlay::draw_inventory_items(SDL_Renderer *renderer) {
    if (inventory_ == nullptr || catalog_ == nullptr) return;
    for (std::size_t slot = 0; slot < kBottomX.size(); ++slot) {
        if (slot >= inventory_->items.size()) break;
        const gl::assets::InventoryItem &item = inventory_->items[slot];
        draw_item(renderer, catalog_->find(item.item_id),
                  static_cast<float>(kBottomX[slot]), static_cast<float>(kBottomY),
                  55.0f, 55.0f);
    }
    for (std::size_t slot = 1; slot < kHotX.size(); ++slot) {
        const gl::assets::InventoryItem *item = find_hot(*inventory_, slot);
        if (item == nullptr) continue;
        draw_item(renderer, catalog_->find(item->item_id),
                  static_cast<float>(kHotX[slot]), static_cast<float>(kHotY), 27.0f,
                  27.0f);
    }
    for (const PuppetSpec &slot : kPuppetSlots) {
        if (!puppet_visible(*inventory_, slot, use_b_set_)) continue;
        const gl::assets::InventoryItem *item =
            find_equipped(*inventory_, slot.mask);
        if (item == nullptr) continue;
        draw_item(renderer, catalog_->find(item->item_id),
                  static_cast<float>(slot.x), static_cast<float>(slot.y), 55.0f,
                  55.0f);
    }
}

void CharacterOverlay::draw_content(SDL_Renderer *renderer) {
    if (font_ == nullptr) return;
    const gl::render::Font *title = title_font();
    for (std::size_t i = 0; i < sheet_.primary.size() && i < 7; ++i) {
        draw_text_rect(renderer, font_, sheet_.primary[i].label, kPrimaryLabelX,
                       kPrimaryY[i], -1.0f, -1.0f, kTextColor);
        draw_text_rect(renderer, font_, sheet_.primary[i].value, kPrimaryValueX,
                       kPrimaryValueY[i], kPrimaryValueW, kRowValueH, kTextColor);
    }
    draw_text_rect(renderer, font_, sheet_.experience_points.label,
                   kExperienceLabelX, kExperienceY, -1.0f, -1.0f, kTextColor);
    draw_text_rect(renderer, font_, sheet_.experience_points.value,
                   kExperienceValueX, kExperienceY, kExperienceValueW, kRowValueH,
                   kTextColor);

    if (tab_ == gl::game::CharacterTab::characteristics) {
        draw_text_rect(renderer, title, sheet_.state_title, 352.0f, 57.0f, 228.0f,
                       -1.0f, kTextColor);
        for (std::size_t i = 0; i < 6 && i < sheet_.state.size(); ++i) {
            draw_text_rect(renderer, font_, sheet_.state[i].label, kCenterLabelX,
                           kStateY[i], -1.0f, -1.0f, kTextColor);
            const TextRect &value = kStateValueRect[i];
            draw_text_rect(renderer, font_, sheet_.state[i].value, value.x,
                           value.y, value.w, value.h, kTextColor);
        }

        draw_text_rect(renderer, title, sheet_.combat_title, 352.0f, 307.0f,
                       228.0f, -1.0f, kTextColor);
        for (std::size_t i = 0; i < 10 && i < sheet_.combat.size(); ++i) {
            const float label_x = (i == 9) ? kWeightLabelX : kCenterLabelX;
            draw_text_rect(renderer, font_, sheet_.combat[i].label, label_x,
                           kCombatY[i], -1.0f, -1.0f, kTextColor);
            const TextRect &value = kCombatValueRect[i];
            draw_text_rect(renderer, font_, sheet_.combat[i].value, value.x,
                           value.y, value.w, value.h, kTextColor);
        }

        draw_text_rect(renderer, title, sheet_.resistance_title, 709.0f, 57.0f,
                       228.0f, -1.0f, kTextColor);
        for (std::size_t i = 0; i < 12 && i < sheet_.resistance.size(); ++i) {
            draw_text_rect(renderer, font_, sheet_.resistance[i].label,
                           kResistanceLabelX, kResistanceY[i], -1.0f, -1.0f,
                           kTextColor);
            draw_text_rect(renderer, font_, sheet_.resistance[i].value,
                           kResistanceValueX, kResistanceValueY[i],
                           kResistanceValueW, kResistanceValueH, kTextColor);
        }

        draw_text_rect(renderer, title, sheet_.immunity_title, 713.0f, 527.0f,
                       228.0f, -1.0f, kTextColor);
        for (std::size_t column = 0; column < 2; ++column) {
            const float label_x =
                column == 0 ? kImmunityLeftLabelX : kImmunityRightLabelX;
            const float value_x =
                column == 0 ? kImmunityLeftValueX : kImmunityRightValueX;
            for (std::size_t row = 0; row < 3; ++row) {
                const std::size_t index = column * 3 + row;
                if (index >= sheet_.immunity.size()) continue;
                draw_text_rect(renderer, font_, sheet_.immunity[index].label,
                               label_x, kImmunityY[row], -1.0f, -1.0f,
                               kTextColor);
                draw_text_rect(renderer, font_, sheet_.immunity[index].value,
                               value_x, kImmunityY[row], kImmunityValueW,
                               kImmunityValueH, kTextColor);
            }
        }
    } else {
        for (std::size_t group = 0;
             group < sheet_.skill_groups.size() && group < 3; ++group) {
            const auto &entry = sheet_.skill_groups[group];
            draw_text_rect(renderer, title, entry.title, 535.0f,
                           kSkillGroupY[group], 230.0f, -1.0f, kTextColor);
            for (std::size_t column = 0; column < 3; ++column) {
                for (std::size_t row = 0; row < 3; ++row) {
                    const std::size_t index = column * 3 + row;
                    if (index >= entry.rows.size()) break;
                    const auto &item = entry.rows[index];
                    draw_text_rect(renderer, font_, item.label,
                                   kSkillColumnX[column],
                                   kSkillRowY[group * 3 + row], -1.0f, -1.0f,
                                   kTextColor);
                    draw_text_rect(renderer, font_, item.value,
                                   kSkillValueX[column],
                                   kSkillValueY[group * 3 + row], kSkillValueW,
                                   kSkillValueH, kTextColor);
                }
            }
        }
        draw_text_rect(renderer, font_, sheet_.skill_points_label,
                       kSkillPointsLabelX, kSkillPointsY, -1.0f, -1.0f,
                       kTextColor);
        draw_text_rect(renderer, font_, std::to_string(sheet_.skill_points),
                       kSkillPointsValueX, kSkillPointsValueY, kSkillPointsValueW,
                       kSkillPointsValueH, kTextColor);
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
    if (contains(265.0f, 0.0f, 350.0f, 35.0f, logical_x, logical_y)) {
        tab_ = gl::game::CharacterTab::characteristics;
        return true;
    }
    if (contains(615.0f, 0.0f, 409.0f, 35.0f, logical_x, logical_y)) {
        tab_ = gl::game::CharacterTab::skills;
        return true;
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
    if (key == SDLK_1) {
        tab_ = gl::game::CharacterTab::characteristics;
        return true;
    }
    if (key == SDLK_2) {
        tab_ = gl::game::CharacterTab::skills;
        return true;
    }
    return false;
}

CharacterRegion CharacterOverlay::hit_test(int x, int y) const {
    if (!visible_ || scale_ <= 0.0f) return CharacterRegion::none;
    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;
    if (contains(265.0f, 0.0f, 350.0f, 35.0f, logical_x, logical_y) ||
        contains(615.0f, 0.0f, 409.0f, 35.0f, logical_x, logical_y))
        return CharacterRegion::tab;
    if (contains(920.0f, 694.0f, 75.0f, 68.0f, logical_x, logical_y))
        return CharacterRegion::close;
    return CharacterRegion::none;
}
} // namespace gl::app
