#include "gl/app/character_overlay.hpp"
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
constexpr std::array<float, 7> kPrimaryY{62.0f, 89.0f, 117.0f, 144.0f,
                                         172.0f, 199.0f, 227.0f};
constexpr std::array<int, 7> kPrimaryH{21, 21, 20, 21, 20, 21, 20};
constexpr float kExperienceY = 263.0f;
constexpr int kExperienceH = 21;
constexpr std::array<float, 6> kStateY{103.0f, 134.0f, 163.0f, 214.0f,
                                       240.0f, 267.0f};
constexpr std::array<int, 6> kStateH{23, 22, 22, 19, 19, 19};
constexpr std::array<float, 10> kCombatY{349.0f, 376.0f, 403.0f, 431.0f,
                                         458.0f, 485.0f, 512.0f, 539.0f,
                                         566.0f, 598.0f};
constexpr std::array<int, 10> kCombatH{19, 19, 19, 18, 18, 18, 18, 18, 18, 21};
constexpr std::array<float, 12> kResistanceY{105.0f, 137.0f, 169.0f, 222.0f,
                                             253.0f, 283.0f, 314.0f, 344.0f,
                                             375.0f, 431.0f, 457.0f, 484.0f};
constexpr std::array<int, 12> kResistanceH{20, 20, 20, 20, 19, 19, 19,
                                           19, 19, 19, 19, 19};
constexpr std::array<float, 3> kImmunityY{571.0f, 597.0f, 624.0f};
constexpr std::array<int, 3> kImmunityH{19, 19, 18};
constexpr std::array<float, 3> kSkillColumnX{291.0f, 537.0f, 783.0f};
constexpr float kSkillLabelW = 160.0f;
constexpr std::array<float, 3> kSkillValueRight{488.0f, 733.0f, 978.0f};
constexpr std::array<float, 9> kSkillRowY{126.0f, 159.0f, 191.0f, 312.0f,
                                          345.0f, 377.0f, 497.0f, 529.0f,
                                          561.0f};
constexpr std::array<int, 9> kSkillRowH{19, 18, 19, 19, 18, 19, 18, 19, 19};
constexpr std::array<float, 3> kSkillGroupY{74.0f, 260.0f, 445.0f};
constexpr std::array<int, 3> kSkillGroupH{29, 28, 28};
constexpr float kSkillPointsY = 628.0f;
constexpr int kSkillPointsH = 22;
constexpr float kSkillPointsLabelX = 517.0f;
constexpr float kSkillPointsLabelW = 194.0f;
constexpr float kSkillPointsValueRight = 779.0f;
constexpr gl::render::Color kTextColor{20, 18, 16, 255};
constexpr gl::render::Color kActiveTab{238, 232, 214, 255};
constexpr gl::render::Color kInactiveTab{68, 63, 52, 255};
constexpr gl::render::Color kNameColor{250, 248, 240, 255};

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

void CharacterOverlay::draw_centered(SDL_Renderer *renderer,
                                     const gl::render::Font *font,
                                     std::string_view text, float center_x,
                                     float box_y, float box_h,
                                     gl::render::Color color) {
    if (font == nullptr || text.empty()) return;
    const auto measured = font->measure(text);
    const float y = box_y + (box_h - font->text_height()) * 0.5f;
    font->draw_text(renderer, text, center_x - measured.x * 0.5f, y, color);
}

void CharacterOverlay::draw_tabs(SDL_Renderer *renderer) {
    const gl::render::Font *title = title_font();
    if (title == nullptr) return;
    draw_centered(renderer, title, sheet_.name, 132.0f, 0.0f, 35.0f, kNameColor);
    const bool characteristics =
        tab_ == gl::game::CharacterTab::characteristics;
    draw_centered(renderer, title, sheet_.characteristics_title, 440.0f, 0.0f, 35.0f,
                  characteristics ? kActiveTab : kInactiveTab);
    draw_centered(renderer, title, sheet_.skills_title, 819.0f, 0.0f, 35.0f,
                  characteristics ? kInactiveTab : kActiveTab);
    SDL_SetRenderDrawColor(renderer, 216, 200, 154, 220);
    const SDL_FRect underline{characteristics ? 265.0f : 615.0f, 32.0f,
                              characteristics ? 350.0f : 409.0f, 2.0f};
    SDL_RenderFillRectF(renderer, &underline);
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
                                 const gl::render::Font *font,
                                 const std::vector<gl::game::CharacterRow> &rows,
                                 const int *y_positions, const int *heights, int count,
                                 float label_x, float label_w, float value_right) {
    if (font == nullptr) return;
    for (int index = 0; index < count && index < static_cast<int>(rows.size()); ++index) {
        draw_row(renderer, font, rows[static_cast<std::size_t>(index)], label_x,
                 label_w, value_right, static_cast<float>(y_positions[index]),
                 static_cast<float>(heights[index]));
    }
}

void CharacterOverlay::draw_row(SDL_Renderer *renderer,
                                const gl::render::Font *font,
                                const gl::game::CharacterRow &row, float label_x,
                                float label_w, float value_right, float y,
                                float height) {
    if (font == nullptr) return;
    const float text_y = y + (height - font->text_height()) * 0.5f;
    const SDL_Rect label_clip{static_cast<int>(label_x), static_cast<int>(y),
                              static_cast<int>(label_w), static_cast<int>(height)};
    SDL_RenderSetClipRect(renderer, &label_clip);
    font->draw_text(renderer, row.label, label_x + 4.0f, text_y, kTextColor);
    SDL_RenderSetClipRect(renderer, nullptr);
    const auto measured = font->measure(row.value);
    font->draw_text(renderer, row.value, value_right - measured.x, text_y, kTextColor);
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
    std::array<int, 7> primary_y{};
    for (std::size_t index = 0; index < primary_y.size(); ++index)
        primary_y[index] = static_cast<int>(kPrimaryY[index]);
    draw_rows(renderer, font_, sheet_.primary, primary_y.data(), kPrimaryH.data(), 7,
              20.0f, 132.0f, 221.0f);
    draw_row(renderer, font_, sheet_.experience_points, 20.0f, 182.0f, 251.0f,
             kExperienceY, static_cast<float>(kExperienceH));

    if (tab_ == gl::game::CharacterTab::characteristics) {
        draw_centered(renderer, title, sheet_.state_title, 466.0f, 55.0f, 24.0f,
                      kTextColor);
        std::array<int, 6> state_y{};
        for (std::size_t index = 0; index < state_y.size(); ++index)
            state_y[index] = static_cast<int>(kStateY[index]);
        draw_rows(renderer, font_, sheet_.state, state_y.data(), kStateH.data(), 6,
                  298.0f, 282.0f, 627.0f);

        draw_centered(renderer, title, sheet_.combat_title, 466.0f, 305.0f, 24.0f,
                      kTextColor);
        std::array<int, 10> combat_y{};
        for (std::size_t index = 0; index < combat_y.size(); ++index)
            combat_y[index] = static_cast<int>(kCombatY[index]);
        draw_rows(renderer, font_, sheet_.combat, combat_y.data(), kCombatH.data(), 10,
                  298.0f, 282.0f, 627.0f);

        draw_centered(renderer, title, sheet_.resistance_title, 822.0f, 55.0f, 25.0f,
                      kTextColor);
        std::array<int, 12> resistance_y{};
        for (std::size_t index = 0; index < resistance_y.size(); ++index)
            resistance_y[index] = static_cast<int>(kResistanceY[index]);
        draw_rows(renderer, font_, sheet_.resistance, resistance_y.data(),
                  kResistanceH.data(), 12, 665.0f, 278.0f, 995.0f);

        draw_centered(renderer, title, sheet_.immunity_title, 828.0f, 525.0f, 25.0f,
                      kTextColor);
        std::array<int, 3> immunity_y{};
        for (std::size_t index = 0; index < immunity_y.size(); ++index)
            immunity_y[index] = static_cast<int>(kImmunityY[index]);
        for (std::size_t column = 0; column < 2; ++column) {
            std::vector<gl::game::CharacterRow> rows;
            for (std::size_t row = 0; row < 3; ++row)
                rows.push_back(sheet_.immunity[column * 3 + row]);
            draw_rows(renderer, font_, rows, immunity_y.data(), kImmunityH.data(), 3,
                      column == 0 ? 667.0f : 842.0f, column == 0 ? 101.0f : 96.0f,
                      column == 0 ? 816.0f : 991.0f);
        }
    } else {
        for (std::size_t group = 0; group < sheet_.skill_groups.size() && group < 3;
             ++group) {
            const auto &entry = sheet_.skill_groups[group];
            draw_centered(renderer, title, entry.title, 652.0f, kSkillGroupY[group],
                          static_cast<float>(kSkillGroupH[group]), kTextColor);
            std::array<int, 3> row_y{};
            std::array<int, 3> row_h{};
            for (std::size_t row = 0; row < 3; ++row) {
                row_y[row] = static_cast<int>(kSkillRowY[group * 3 + row]);
                row_h[row] = kSkillRowH[group * 3 + row];
            }
            for (std::size_t column = 0; column < 3; ++column) {
                std::vector<gl::game::CharacterRow> rows;
                for (std::size_t row = 0; row < 3; ++row) {
                    const std::size_t index = column * 3 + row;
                    if (index >= entry.rows.size()) break;
                    rows.push_back(entry.rows[index]);
                }
                draw_rows(renderer, font_, rows, row_y.data(), row_h.data(),
                          static_cast<int>(rows.size()), kSkillColumnX[column],
                          kSkillLabelW, kSkillValueRight[column]);
            }
        }
        const gl::game::CharacterRow points{sheet_.skill_points_label,
                                            std::to_string(sheet_.skill_points),
                                            true};
        draw_centered(renderer, font_, points.label,
                      kSkillPointsLabelX + kSkillPointsLabelW * 0.5f, kSkillPointsY,
                      static_cast<float>(kSkillPointsH), kTextColor);
        const auto measured = font_->measure(points.value);
        font_->draw_text(renderer, points.value,
                         kSkillPointsValueRight - measured.x,
                         kSkillPointsY +
                             (kSkillPointsH - font_->text_height()) * 0.5f,
                         kTextColor);
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
