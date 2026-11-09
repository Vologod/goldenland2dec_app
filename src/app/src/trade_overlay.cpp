#include "gl/app/trade_overlay.hpp"

#include <gl/assets/csx.hpp>
#include <gl/assets/image.hpp>
#include <gl/assets/item_catalog.hpp>
#include <gl/assets/log.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/game/inventory_view.hpp>
#include <gl/render/text.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gl::app {
namespace {

constexpr std::string_view kBackground = "engineres/trade/trade.bmp";
constexpr std::string_view kFallbackIcon = "engineres/interface/bag.csx";
constexpr float kMoneyScale = 1.5f;
constexpr gl::render::Color kMoneyColor{255, 232, 160, 255};
constexpr gl::render::Color kNameColor{255, 232, 160, 255};
constexpr gl::render::Color kErrorColor{255, 120, 120, 255};
constexpr std::size_t kMaxStaged = 16;

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

void draw_stack_count(SDL_Renderer *renderer, std::uint32_t count, float x, float y, float w,
                      float h) {
    if (count <= 1)
        return;
    const std::string text = std::to_string(count);
    float scale = 1.5f;
    const float max_width = w - 4.0f;
    const float max_height = h - 4.0f;
    const float natural_width = gl::render::text_width(text, scale);
    if (natural_width > max_width && natural_width > 0.0f)
        scale *= max_width / natural_width;
    const float natural_height = gl::render::text_height(scale);
    if (natural_height > max_height && natural_height > 0.0f)
        scale *= max_height / natural_height;
    const float width = gl::render::text_width(text, scale);
    const float height = gl::render::text_height(scale);
    gl::render::draw_text(renderer, text, x + w - 2.0f - width, y + h - 2.0f - height, scale,
                          gl::render::Color{255, 255, 255, 255});
}

void build_visible_hero(const std::vector<gl::assets::InventoryItem> &items, std::uint32_t mask,
                        std::vector<std::size_t> &out) {
    out.clear();
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].type == 18 || items[i].type == 25)
            continue;
        if (gl::game::item_matches_filter(items[i].type, mask))
            out.push_back(i);
    }
}

} // namespace

bool TradeRect::contains(float px, float py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
}

std::size_t TradeLayout::max_scroll(std::size_t item_count, std::size_t row_size) {
    return item_count > row_size ? item_count - row_size : 0;
}

std::optional<std::size_t> TradeLayout::npc_slot_at(float x, float y) const {
    for (std::size_t i = 0; i < kNpcX.size(); ++i) {
        const TradeRect rect{static_cast<float>(kNpcX[i]), static_cast<float>(kNpcY),
                             static_cast<float>(kIconSize), static_cast<float>(kIconSize)};
        if (rect.contains(x, y))
            return i;
    }
    return std::nullopt;
}

std::optional<std::size_t> TradeLayout::buy_staging_slot_at(float x, float y) const {
    for (std::size_t i = 0; i < kNpcX.size(); ++i) {
        const TradeRect rect{static_cast<float>(kNpcX[i]), static_cast<float>(kStagingY),
                             static_cast<float>(kIconSize), static_cast<float>(kIconSize)};
        if (rect.contains(x, y))
            return i;
    }
    return std::nullopt;
}

std::optional<std::size_t> TradeLayout::sell_staging_slot_at(float x, float y) const {
    for (std::size_t i = 0; i < kNpcX.size(); ++i) {
        const TradeRect rect{static_cast<float>(kNpcX[i]), static_cast<float>(kSellStagingY),
                             static_cast<float>(kIconSize), static_cast<float>(kIconSize)};
        if (rect.contains(x, y))
            return i;
    }
    return std::nullopt;
}

std::optional<std::size_t> TradeLayout::hero_slot_at(float x, float y) const {
    for (std::size_t i = 0; i < kHeroX.size(); ++i) {
        const TradeRect rect{static_cast<float>(kHeroX[i]), static_cast<float>(kHeroY),
                             static_cast<float>(kIconSize), static_cast<float>(kIconSize)};
        if (rect.contains(x, y))
            return i;
    }
    return std::nullopt;
}

std::optional<std::size_t> TradeLayout::npc_filter_at(float x, float y) const {
    for (std::size_t i = 0; i < kNpcFilters.size(); ++i) {
        if (kNpcFilters[i].rect.contains(x, y))
            return i;
    }
    return std::nullopt;
}

std::optional<std::size_t> TradeLayout::hero_filter_at(float x, float y) const {
    for (std::size_t i = 0; i < kHeroFilters.size(); ++i) {
        if (kHeroFilters[i].rect.contains(x, y))
            return i;
    }
    return std::nullopt;
}

TradeRegion TradeLayout::hit_test(float x, float y) const {
    if (kExit.contains(x, y))
        return TradeRegion::exit;
    if (kDrop.contains(x, y))
        return TradeRegion::drop;
    if (kExchange.contains(x, y))
        return TradeRegion::exchange;
    if (kNpcScrollLeft.contains(x, y))
        return TradeRegion::npc_scroll_left;
    if (kNpcScrollRight.contains(x, y))
        return TradeRegion::npc_scroll_right;
    if (kStagingScrollLeft.contains(x, y))
        return TradeRegion::buy_staging_scroll_left;
    if (kStagingScrollRight.contains(x, y))
        return TradeRegion::buy_staging_scroll_right;
    if (kSellStagingScrollLeft.contains(x, y))
        return TradeRegion::sell_staging_scroll_left;
    if (kSellStagingScrollRight.contains(x, y))
        return TradeRegion::sell_staging_scroll_right;
    if (kHeroScrollLeft.contains(x, y))
        return TradeRegion::hero_scroll_left;
    if (kHeroScrollRight.contains(x, y))
        return TradeRegion::hero_scroll_right;
    if (npc_filter_at(x, y).has_value())
        return TradeRegion::npc_filter;
    if (hero_filter_at(x, y).has_value())
        return TradeRegion::hero_filter;
    if (npc_slot_at(x, y).has_value())
        return TradeRegion::npc_slot;
    if (buy_staging_slot_at(x, y).has_value())
        return TradeRegion::buy_staging_slot;
    if (sell_staging_slot_at(x, y).has_value())
        return TradeRegion::sell_staging_slot;
    if (hero_slot_at(x, y).has_value())
        return TradeRegion::hero_slot;
    return TradeRegion::none;
}

bool TradeOverlay::open(const gl::assets::GameVfs &vfs, const gl::assets::ItemCatalog &catalog,
                        std::string npc_name,
                        const std::vector<gl::assets::InventoryItem> &npc_items,
                        std::int32_t npc_money,
                        const std::vector<gl::assets::InventoryItem> &hero_items,
                        std::int32_t hero_money) {
    vfs_ = &vfs;
    catalog_ = &catalog;
    npc_name_ = std::move(npc_name);
    npc_items_ = npc_items;
    hero_items_ = hero_items;
    npc_money_ = npc_money;
    hero_money_ = hero_money;
    npc_filter_ = 0;
    hero_filter_ = 0;
    npc_filter_index_.reset();
    hero_filter_index_.reset();
    npc_scroll_ = 0;
    hero_scroll_ = 0;
    buy_staging_scroll_ = 0;
    sell_staging_scroll_ = 0;
    buy_staged_.clear();
    sell_staged_.clear();
    last_error_.clear();
    info_item_.reset();
    info_price_.reset();
    exchange_pressed_ = false;
    clear_selection();
    visible_ = true;
    screen_w_ = 1024;
    screen_h_ = 768;
    update_transform(screen_w_, screen_h_);
    reset_texture_cache(nullptr);
    rebuild_npc_visible();
    rebuild_hero_visible();
    return true;
}

void TradeOverlay::close() { visible_ = false; }

void TradeOverlay::set_price_provider(TradePriceProvider provider) {
    price_provider_ = std::move(provider);
}

void TradeOverlay::set_exchange_handler(TradeExchangeHandler handler) {
    exchange_handler_ = std::move(handler);
}

bool TradeOverlay::stage_item_by_id(TradeOrigin origin, std::uint32_t item_id) {
    const std::vector<gl::assets::InventoryItem> &items =
        origin == TradeOrigin::npc ? npc_items_ : hero_items_;
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (items[index].item_id != item_id)
            continue;
        return stage_item(origin, index);
    }
    last_error_ = "item is not in the trade list";
    gl::log(gl::LogLevel::warning, "trade: item " + std::to_string(item_id) + " is not in the " +
                                       (origin == TradeOrigin::npc ? "npc" : "hero") +
                                       " trade list");
    return false;
}

gl::game::TradeStatus TradeOverlay::run_exchange() {
    if (buy_staged_.empty() && sell_staged_.empty()) {
        last_error_ = "nothing to exchange";
        gl::log(gl::LogLevel::warning, "trade: nothing to exchange");
        return gl::game::TradeStatus::not_available;
    }
    return exchange();
}

std::int64_t TradeOverlay::buy_total() const {
    std::int64_t total = 0;
    for (const StagedTradeItem &staged : buy_staged_)
        total += staged.price.buy;
    return total;
}

std::int64_t TradeOverlay::sell_total() const {
    std::int64_t total = 0;
    for (const StagedTradeItem &staged : sell_staged_)
        total += staged.price.sell;
    return total;
}

std::size_t TradeOverlay::buy_staging_max_scroll() const {
    return TradeLayout::max_scroll(buy_staged_.size(), TradeLayout::npc_row_size());
}

std::size_t TradeOverlay::sell_staging_max_scroll() const {
    return TradeLayout::max_scroll(sell_staged_.size(), TradeLayout::npc_row_size());
}

void TradeOverlay::clear_selection() {
    selection_ = TradeSelection::none;
    selected_npc_.reset();
    selected_hero_.reset();
}

void TradeOverlay::rebuild_npc_visible() {
    build_visible_hero(npc_items_, npc_filter_, npc_visible_);
    const std::size_t max_scroll = npc_max_scroll();
    if (npc_scroll_ > max_scroll)
        npc_scroll_ = max_scroll;
    if (selected_npc_.has_value() &&
        std::find(npc_visible_.begin(), npc_visible_.end(), *selected_npc_) == npc_visible_.end()) {
        clear_selection();
    }
}

void TradeOverlay::rebuild_hero_visible() {
    build_visible_hero(hero_items_, hero_filter_, hero_visible_);
    const std::size_t max_scroll = hero_max_scroll();
    if (hero_scroll_ > max_scroll)
        hero_scroll_ = max_scroll;
    if (selected_hero_.has_value() && std::find(hero_visible_.begin(), hero_visible_.end(),
                                                *selected_hero_) == hero_visible_.end()) {
        clear_selection();
    }
}

bool TradeOverlay::stage_item(TradeOrigin origin, std::size_t source_index) {
    std::vector<StagedTradeItem> &staged = origin == TradeOrigin::npc ? buy_staged_ : sell_staged_;
    if (staged.size() >= kMaxStaged) {
        last_error_ =
            origin == TradeOrigin::npc ? "buy staging row is full" : "sell staging row is full";
        gl::log(gl::LogLevel::warning, "trade: staging row is full (16 records)");
        return false;
    }
    std::vector<gl::assets::InventoryItem> &items =
        origin == TradeOrigin::npc ? npc_items_ : hero_items_;
    if (source_index >= items.size())
        return false;
    const gl::assets::InventoryItem item = items[source_index];
    if (item.type == 18 || item.type == 25) {
        last_error_ = "money and quest items cannot be traded";
        gl::log(gl::LogLevel::warning,
                "trade: item type " + std::to_string(item.type) + " cannot be staged");
        return false;
    }
    if (!price_provider_) {
        last_error_ = "price unavailable";
        gl::log(gl::LogLevel::warning, "trade: no price provider configured");
        return false;
    }
    const std::optional<gl::game::TradePrice> price = price_provider_(item);
    if (!price || price->item_id != item.item_id || price->count != item.count) {
        last_error_ = "price unavailable";
        gl::log(gl::LogLevel::warning,
                "trade: no price for item " + std::to_string(item.item_id) + " (modifier guard)");
        return false;
    }
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(source_index));
    staged.push_back(StagedTradeItem{item, *price, origin});
    info_item_ = item;
    info_price_ = *price;
    info_origin_ = origin;
    last_error_.clear();
    clear_selection();
    rebuild_npc_visible();
    rebuild_hero_visible();
    return true;
}

void TradeOverlay::unstage_item(TradeOrigin origin, std::size_t staged_index) {
    std::vector<StagedTradeItem> &staged = origin == TradeOrigin::npc ? buy_staged_ : sell_staged_;
    if (staged_index >= staged.size())
        return;
    const StagedTradeItem entry = staged[staged_index];
    staged.erase(staged.begin() + static_cast<std::ptrdiff_t>(staged_index));
    std::vector<gl::assets::InventoryItem> &items =
        entry.origin == TradeOrigin::npc ? npc_items_ : hero_items_;
    items.push_back(entry.item);
    info_item_.reset();
    info_price_.reset();
    last_error_.clear();
    clear_selection();
    rebuild_npc_visible();
    rebuild_hero_visible();
}

const char *TradeOverlay::status_message(gl::game::TradeStatus status) {
    switch (status) {
    case gl::game::TradeStatus::ok:
        return "";
    case gl::game::TradeStatus::missing_person:
        return "trade person is not available";
    case gl::game::TradeStatus::missing_template:
        return "item template is not available";
    case gl::game::TradeStatus::insufficient_stock:
        return "staged item is no longer available";
    case gl::game::TradeStatus::insufficient_money:
        return "not enough money for this exchange";
    case gl::game::TradeStatus::unsupported_price:
        return "price is unavailable for a staged item";
    case gl::game::TradeStatus::not_available:
        return "exchange did not return a result";
    }
    return "trade failed";
}

gl::game::TradeStatus TradeOverlay::exchange() {
    if (buy_staged_.empty() && sell_staged_.empty())
        return gl::game::TradeStatus::not_available;
    if (!exchange_handler_) {
        last_error_ = "exchange is not available";
        gl::log(gl::LogLevel::warning, "trade: no exchange handler configured");
        return gl::game::TradeStatus::not_available;
    }
    std::vector<gl::game::TradePrice> buy_items;
    std::vector<gl::game::TradePrice> sell_items;
    buy_items.reserve(buy_staged_.size());
    sell_items.reserve(sell_staged_.size());
    for (const StagedTradeItem &staged : buy_staged_)
        buy_items.push_back(staged.price);
    for (const StagedTradeItem &staged : sell_staged_)
        sell_items.push_back(staged.price);
    TradeExchangeResult result = exchange_handler_(buy_items, sell_items);
    if (result.status != gl::game::TradeStatus::ok) {
        last_error_ = status_message(result.status);
        gl::log(gl::LogLevel::warning,
                "trade: exchange failed: " + std::string(status_message(result.status)));
        return result.status;
    }
    npc_items_ = std::move(result.snapshot.npc_items);
    hero_items_ = std::move(result.snapshot.hero_items);
    npc_money_ = result.snapshot.npc_money;
    hero_money_ = result.snapshot.hero_money;
    buy_staged_.clear();
    sell_staged_.clear();
    buy_staging_scroll_ = 0;
    sell_staging_scroll_ = 0;
    info_item_.reset();
    info_price_.reset();
    last_error_.clear();
    clear_selection();
    rebuild_npc_visible();
    rebuild_hero_visible();
    return gl::game::TradeStatus::ok;
}

void TradeOverlay::update_transform(int screen_w, int screen_h) {
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
    scale_ = std::min(static_cast<float>(screen_w) / TradeLayout::kCanvasWidth,
                      static_cast<float>(screen_h) / TradeLayout::kCanvasHeight);
    if (scale_ <= 0.0f)
        scale_ = 1.0f;
    offset_x_ = (static_cast<float>(screen_w) - TradeLayout::kCanvasWidth * scale_) * 0.5f;
    offset_y_ = (static_cast<float>(screen_h) - TradeLayout::kCanvasHeight * scale_) * 0.5f;
}

void TradeOverlay::reset_texture_cache(SDL_Renderer *renderer) {
    textures_.clear();
    failed_textures_.clear();
    texture_renderer_ = renderer;
}

gl::render::Texture *TradeOverlay::load_texture(std::string_view path, bool magenta_key) {
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

gl::render::Texture *TradeOverlay::icon_texture(const gl::assets::ItemCatalogEntry *entry) {
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

gl::render::Texture *TradeOverlay::background_texture() { return load_texture(kBackground, true); }

const gl::assets::InventoryItem *TradeOverlay::selected_item() const {
    switch (selection_) {
    case TradeSelection::npc:
        if (selected_npc_.has_value() && *selected_npc_ < npc_items_.size())
            return &npc_items_[*selected_npc_];
        break;
    case TradeSelection::hero:
        if (selected_hero_.has_value() && *selected_hero_ < hero_items_.size())
            return &hero_items_[*selected_hero_];
        break;
    case TradeSelection::none:
        break;
    }
    return nullptr;
}

const gl::assets::ItemCatalogEntry *TradeOverlay::selected_entry() const {
    const gl::assets::InventoryItem *item = selected_item();
    if (catalog_ == nullptr || item == nullptr)
        return nullptr;
    return catalog_->find(item->item_id);
}

std::string TradeOverlay::selected_name() const {
    const gl::assets::ItemCatalogEntry *entry = selected_entry();
    return entry != nullptr ? entry->name : std::string{};
}

std::size_t TradeOverlay::npc_max_scroll() const {
    return TradeLayout::max_scroll(npc_visible_.size(), TradeLayout::npc_row_size());
}

std::size_t TradeOverlay::hero_max_scroll() const {
    return TradeLayout::max_scroll(hero_visible_.size(), TradeLayout::hero_row_size());
}

float TradeOverlay::native_icon_offset(float target, float texture_size) {
    return std::max(0.0f, (target - texture_size) * 0.5f);
}

void TradeOverlay::apply_magenta_key(gl::assets::Image &image) {
    for (gl::assets::Rgba &pixel : image.rgba) {
        if (pixel.r == 255 && pixel.g == 0 && pixel.b == 255)
            pixel.a = 0;
    }
}

void TradeOverlay::draw_native(SDL_Renderer *renderer, const gl::render::Texture &texture, float x,
                               float y, float target_w, float target_h) const {
    const float texture_w = static_cast<float>(texture.width());
    const float texture_h = static_cast<float>(texture.height());
    const float off_x = native_icon_offset(target_w, texture_w);
    const float off_y = native_icon_offset(target_h, texture_h);
    const SDL_FRect destination{x + off_x, y + off_y, texture_w, texture_h};
    SDL_RenderCopyF(renderer, texture.get(), nullptr, &destination);
}

void TradeOverlay::draw_rect(SDL_Renderer *renderer, const gl::render::Texture &texture,
                             const TradeRect &rect) const {
    const SDL_FRect destination{rect.x, rect.y, rect.w, rect.h};
    SDL_RenderCopyF(renderer, texture.get(), nullptr, &destination);
}

void TradeOverlay::draw_row(SDL_Renderer *renderer,
                            const std::vector<gl::assets::InventoryItem> &items,
                            const std::vector<std::size_t> *visible, const int *xs, int y,
                            std::size_t scroll, int slots) {
    for (int slot = 0; slot < slots; ++slot) {
        const std::size_t index = scroll + static_cast<std::size_t>(slot);
        std::size_t source = index;
        if (visible != nullptr) {
            if (index >= visible->size())
                continue;
            source = (*visible)[index];
        } else if (index >= items.size()) {
            continue;
        }
        if (source >= items.size())
            continue;
        const gl::assets::InventoryItem &item = items[source];
        const gl::assets::ItemCatalogEntry *entry =
            catalog_ != nullptr ? catalog_->find(item.item_id) : nullptr;
        const float x = static_cast<float>(xs[slot]);
        const float row_y = static_cast<float>(y);
        if (gl::render::Texture *icon = icon_texture(entry); icon != nullptr) {
            draw_native(renderer, *icon, x, row_y, static_cast<float>(TradeLayout::kIconSize),
                        static_cast<float>(TradeLayout::kIconSize));
        } else {
            SDL_SetRenderDrawColor(renderer, 22, 24, 30, 200);
            const SDL_FRect fill{x, row_y, static_cast<float>(TradeLayout::kIconSize),
                                 static_cast<float>(TradeLayout::kIconSize)};
            SDL_RenderFillRectF(renderer, &fill);
        }
        draw_stack_count(renderer, item.count, x, row_y, static_cast<float>(TradeLayout::kIconSize),
                         static_cast<float>(TradeLayout::kIconSize));
    }
}

void TradeOverlay::draw_filters(SDL_Renderer *renderer) {
    for (std::size_t i = 0; i < TradeLayout::kNpcFilters.size(); ++i) {
        const TradeFilterSpec &filter = TradeLayout::kNpcFilters[i];
        const std::string path = "engineres/trade/afilter" + std::to_string(filter.sprite) + ".bmp";
        if (gl::render::Texture *texture = load_texture(path); texture != nullptr)
            draw_rect(renderer, *texture, filter.rect);
        if (npc_filter_index_ == std::optional<std::size_t>{i}) {
            SDL_SetRenderDrawColor(renderer, 18, 16, 12, 110);
            const SDL_FRect pressed{filter.rect.x, filter.rect.y, filter.rect.w, filter.rect.h};
            SDL_RenderFillRectF(renderer, &pressed);
            SDL_SetRenderDrawColor(renderer, 232, 201, 112, 255);
            SDL_RenderDrawRectF(renderer, &pressed);
        }
    }
    for (std::size_t i = 0; i < TradeLayout::kHeroFilters.size(); ++i) {
        const TradeFilterSpec &filter = TradeLayout::kHeroFilters[i];
        const std::string path = "engineres/trade/filter" + std::to_string(filter.sprite) + ".bmp";
        if (gl::render::Texture *texture = load_texture(path); texture != nullptr)
            draw_rect(renderer, *texture, filter.rect);
        if (hero_filter_index_ == std::optional<std::size_t>{i}) {
            SDL_SetRenderDrawColor(renderer, 18, 16, 12, 110);
            const SDL_FRect pressed{filter.rect.x, filter.rect.y, filter.rect.w, filter.rect.h};
            SDL_RenderFillRectF(renderer, &pressed);
            SDL_SetRenderDrawColor(renderer, 232, 201, 112, 255);
            SDL_RenderDrawRectF(renderer, &pressed);
        }
    }
}

void TradeOverlay::draw_money(SDL_Renderer *renderer) {
    const std::string npc_text = std::to_string(npc_money_);
    const std::string hero_text = std::to_string(hero_money_);
    if (font_ != nullptr) {
        font_->draw_text(renderer, npc_text, TradeLayout::kNpcMoneyX,
                         static_cast<float>(TradeLayout::kNpcMoneyY), kMoneyColor);
        font_->draw_text(renderer, hero_text, TradeLayout::kHeroMoneyX,
                         static_cast<float>(TradeLayout::kHeroMoneyY), kMoneyColor);
        return;
    }
    gl::render::draw_text(renderer, npc_text, TradeLayout::kNpcMoneyX,
                          static_cast<float>(TradeLayout::kNpcMoneyY), kMoneyScale, kMoneyColor);
    gl::render::draw_text(renderer, hero_text, TradeLayout::kHeroMoneyX,
                          static_cast<float>(TradeLayout::kHeroMoneyY), kMoneyScale, kMoneyColor);
}

void TradeOverlay::draw_totals(SDL_Renderer *renderer) {
    const std::string text = std::to_string(net_total());
    if (font_ != nullptr) {
        font_->draw_text(renderer, text, TradeLayout::kTotalsX,
                         static_cast<float>(TradeLayout::kTotalsY), kMoneyColor);
        return;
    }
    gl::render::draw_text(renderer, text, TradeLayout::kTotalsX,
                          static_cast<float>(TradeLayout::kTotalsY), 1.5f, kMoneyColor);
}

void TradeOverlay::draw_staging_info(SDL_Renderer *renderer) {
    if (!info_item_ || !info_price_)
        return;
    const gl::assets::ItemCatalogEntry *entry =
        catalog_ != nullptr ? catalog_->find(info_item_->item_id) : nullptr;
    const std::string name = entry != nullptr && !entry->name.empty()
                                 ? entry->name
                                 : std::to_string(info_item_->item_id);
    const std::int64_t total =
        info_origin_ == TradeOrigin::npc ? info_price_->buy : info_price_->sell;
    const std::uint32_t count = info_price_->count;
    const std::int64_t unit = count != 0 ? total / static_cast<std::int64_t>(count) : total;
    const std::string label = std::string(info_origin_ == TradeOrigin::npc ? "buy " : "sell ") +
                              std::to_string(unit) + " / " + std::to_string(total);
    if (font_ != nullptr) {
        font_->draw_text(renderer, name, 340.0f, 150.0f, kNameColor);
        font_->draw_text(renderer, label, 340.0f, 172.0f, kNameColor);
        return;
    }
    gl::render::draw_text(renderer, name, 340.0f, 150.0f, 1.5f, kNameColor);
    gl::render::draw_text(renderer, label, 340.0f, 172.0f, 1.5f, kNameColor);
}

void TradeOverlay::draw_error(SDL_Renderer *renderer) {
    if (last_error_.empty())
        return;
    if (font_ != nullptr) {
        font_->draw_text(renderer, last_error_, 340.0f, 430.0f, kErrorColor);
        return;
    }
    gl::render::draw_text(renderer, last_error_, 340.0f, 430.0f, 1.5f, kErrorColor);
}

void TradeOverlay::draw_selected_name(SDL_Renderer *renderer) {
    if (font_ == nullptr || selection_ == TradeSelection::none)
        return;
    const gl::assets::ItemCatalogEntry *entry = selected_entry();
    if (entry == nullptr || entry->name.empty())
        return;
    font_->draw_text(renderer, entry->name, 340.0f, 150.0f, kNameColor);
}

void TradeOverlay::render(SDL_Renderer *renderer, int screen_w, int screen_h,
                          const gl::render::Font *font) {
    if (renderer == nullptr || !visible_ || screen_w <= 0 || screen_h <= 0)
        return;
    if (texture_renderer_ != renderer)
        reset_texture_cache(renderer);
    if (font != nullptr)
        font_ = font;
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
        std::max(1, static_cast<int>(std::lround(TradeLayout::kCanvasWidth * scale_))),
        std::max(1, static_cast<int>(std::lround(TradeLayout::kCanvasHeight * scale_))),
    };
    SDL_RenderSetViewport(renderer, &viewport);
    SDL_RenderSetScale(renderer, scale_, scale_);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (gl::render::Texture *background = background_texture(); background != nullptr) {
        draw_rect(renderer, *background,
                  TradeRect{0.0f, 0.0f, TradeLayout::kCanvasWidth, TradeLayout::kCanvasHeight});
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        const SDL_FRect canvas{0.0f, 0.0f, TradeLayout::kCanvasWidth, TradeLayout::kCanvasHeight};
        SDL_RenderFillRectF(renderer, &canvas);
    }

    npc_scroll_ = std::min(npc_scroll_, npc_max_scroll());
    hero_scroll_ = std::min(hero_scroll_, hero_max_scroll());
    buy_staging_scroll_ = std::min(buy_staging_scroll_, buy_staging_max_scroll());
    sell_staging_scroll_ = std::min(sell_staging_scroll_, sell_staging_max_scroll());

    draw_row(renderer, npc_items_, &npc_visible_, TradeLayout::kNpcX.data(), TradeLayout::kNpcY,
             npc_scroll_, static_cast<int>(TradeLayout::npc_row_size()));
    std::vector<gl::assets::InventoryItem> buy_staging_items;
    buy_staging_items.reserve(buy_staged_.size());
    for (const StagedTradeItem &staged : buy_staged_)
        buy_staging_items.push_back(staged.item);
    draw_row(renderer, buy_staging_items, nullptr, TradeLayout::kNpcX.data(),
             TradeLayout::kStagingY, buy_staging_scroll_,
             static_cast<int>(TradeLayout::npc_row_size()));
    std::vector<gl::assets::InventoryItem> sell_staging_items;
    sell_staging_items.reserve(sell_staged_.size());
    for (const StagedTradeItem &staged : sell_staged_)
        sell_staging_items.push_back(staged.item);
    draw_row(renderer, sell_staging_items, nullptr, TradeLayout::kNpcX.data(),
             TradeLayout::kSellStagingY, sell_staging_scroll_,
             static_cast<int>(TradeLayout::npc_row_size()));
    draw_row(renderer, hero_items_, &hero_visible_, TradeLayout::kHeroX.data(), TradeLayout::kHeroY,
             hero_scroll_, static_cast<int>(TradeLayout::hero_row_size()));

    draw_filters(renderer);

    if (gl::render::Texture *texture = load_texture("engineres/trade/left3.bmp");
        texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kNpcScrollLeft);
    if (gl::render::Texture *texture = load_texture("engineres/trade/right3.bmp");
        texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kNpcScrollRight);
    if (gl::render::Texture *texture = load_texture("engineres/trade/left2.bmp");
        texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kStagingScrollLeft);
    if (gl::render::Texture *texture = load_texture("engineres/trade/right2.bmp");
        texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kStagingScrollRight);
    if (gl::render::Texture *texture = load_texture("engineres/trade/left1.bmp");
        texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kSellStagingScrollLeft);
    if (gl::render::Texture *texture = load_texture("engineres/trade/right1.bmp");
        texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kSellStagingScrollRight);
    if (gl::render::Texture *texture = load_texture(
            exchange_pressed_ ? "engineres/trade/pbig2.bmp" : "engineres/trade/lbig2.bmp");
        texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kExchange);
    if (gl::render::Texture *texture = load_texture("engineres/trade/inv_left.bmp");
        texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kHeroScrollLeft);
    if (gl::render::Texture *texture = load_texture("engineres/trade/inv_right.bmp");
        texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kHeroScrollRight);
    if (gl::render::Texture *texture = load_texture("engineres/trade/exitl.bmp");
        texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kExit);
    if (gl::render::Texture *texture = load_texture("engineres/trade/drop.bmp"); texture != nullptr)
        draw_rect(renderer, *texture, TradeLayout::kDrop);

    draw_money(renderer);
    draw_totals(renderer);
    draw_staging_info(renderer);
    draw_error(renderer);
    draw_selected_name(renderer);

    SDL_RenderSetScale(renderer, old_scale_x, old_scale_y);
    SDL_RenderSetViewport(renderer, &old_viewport);
    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    SDL_SetRenderDrawColor(renderer, old_r, old_g, old_b, old_a);
}

bool TradeOverlay::handle_mouse(int x, int y, bool click) {
    if (!visible_)
        return false;
    const TradeRegion region = hit_test(x, y);
    if (region == TradeRegion::none) {
        exchange_pressed_ = false;
        return false;
    }
    exchange_pressed_ = !click && region == TradeRegion::exchange;
    if (!click)
        return true;
    exchange_pressed_ = false;

    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;

    switch (region) {
    case TradeRegion::exit:
        close();
        break;
    case TradeRegion::exchange:
        exchange();
        break;
    case TradeRegion::npc_scroll_left:
        if (npc_scroll_ > 0)
            --npc_scroll_;
        break;
    case TradeRegion::npc_scroll_right:
        npc_scroll_ = std::min(npc_scroll_ + 1, npc_max_scroll());
        break;
    case TradeRegion::buy_staging_scroll_left:
        if (buy_staging_scroll_ > 0)
            --buy_staging_scroll_;
        break;
    case TradeRegion::buy_staging_scroll_right:
        buy_staging_scroll_ = std::min(buy_staging_scroll_ + 1, buy_staging_max_scroll());
        break;
    case TradeRegion::sell_staging_scroll_left:
        if (sell_staging_scroll_ > 0)
            --sell_staging_scroll_;
        break;
    case TradeRegion::sell_staging_scroll_right:
        sell_staging_scroll_ = std::min(sell_staging_scroll_ + 1, sell_staging_max_scroll());
        break;
    case TradeRegion::hero_scroll_left:
        if (hero_scroll_ > 0)
            --hero_scroll_;
        break;
    case TradeRegion::hero_scroll_right:
        hero_scroll_ = std::min(hero_scroll_ + 1, hero_max_scroll());
        break;
    case TradeRegion::npc_filter:
        if (const auto index = layout_.npc_filter_at(logical_x, logical_y); index.has_value()) {
            npc_filter_index_ = index;
            npc_filter_ = TradeLayout::kNpcFilters[*index].mask;
            npc_scroll_ = 0;
            rebuild_npc_visible();
        }
        break;
    case TradeRegion::hero_filter:
        if (const auto index = layout_.hero_filter_at(logical_x, logical_y); index.has_value()) {
            hero_filter_index_ = index;
            hero_filter_ = TradeLayout::kHeroFilters[*index].mask;
            hero_scroll_ = 0;
            rebuild_hero_visible();
        }
        break;
    case TradeRegion::npc_slot:
        if (const auto slot = layout_.npc_slot_at(logical_x, logical_y); slot.has_value()) {
            const std::size_t index = npc_scroll_ + *slot;
            if (index < npc_visible_.size()) {
                if (price_provider_) {
                    if (!stage_item(TradeOrigin::npc, npc_visible_[index]))
                        clear_selection();
                } else {
                    selection_ = TradeSelection::npc;
                    selected_npc_ = npc_visible_[index];
                    selected_hero_.reset();
                }
            } else {
                clear_selection();
            }
        }
        break;
    case TradeRegion::buy_staging_slot:
        if (const auto slot = layout_.buy_staging_slot_at(logical_x, logical_y); slot.has_value()) {
            const std::size_t index = buy_staging_scroll_ + *slot;
            if (index < buy_staged_.size())
                unstage_item(TradeOrigin::npc, index);
        }
        break;
    case TradeRegion::sell_staging_slot:
        if (const auto slot = layout_.sell_staging_slot_at(logical_x, logical_y);
            slot.has_value()) {
            const std::size_t index = sell_staging_scroll_ + *slot;
            if (index < sell_staged_.size())
                unstage_item(TradeOrigin::hero, index);
        }
        break;
    case TradeRegion::hero_slot:
        if (const auto slot = layout_.hero_slot_at(logical_x, logical_y); slot.has_value()) {
            const std::size_t index = hero_scroll_ + *slot;
            if (index < hero_visible_.size()) {
                if (price_provider_) {
                    if (!stage_item(TradeOrigin::hero, hero_visible_[index]))
                        clear_selection();
                } else {
                    selection_ = TradeSelection::hero;
                    selected_hero_ = hero_visible_[index];
                    selected_npc_.reset();
                }
            } else {
                clear_selection();
            }
        }
        break;
    case TradeRegion::drop:
    case TradeRegion::none:
        break;
    }
    return true;
}

bool TradeOverlay::handle_key(SDL_Keycode key) {
    if (!visible_)
        return false;
    if (key == SDLK_ESCAPE || key == SDLK_AC_BACK) {
        close();
        return true;
    }
    return false;
}

bool TradeOverlay::handle_wheel(int delta) {
    if (!visible_ || delta == 0)
        return false;
    const std::size_t step = delta > 0 ? 3u : 0u;
    if (delta > 0)
        npc_scroll_ = std::min(npc_scroll_ + step, npc_max_scroll());
    else
        npc_scroll_ = npc_scroll_ > 3u ? npc_scroll_ - 3u : 0u;
    return true;
}

bool TradeOverlay::handle_wheel(int delta, int x, int y) {
    if (!visible_ || delta == 0)
        return false;
    const TradeRegion region = hit_test(x, y);
    if (region == TradeRegion::buy_staging_slot || region == TradeRegion::buy_staging_scroll_left ||
        region == TradeRegion::buy_staging_scroll_right) {
        if (delta > 0)
            buy_staging_scroll_ = std::min(buy_staging_scroll_ + 3u, buy_staging_max_scroll());
        else
            buy_staging_scroll_ = buy_staging_scroll_ > 3u ? buy_staging_scroll_ - 3u : 0u;
        return true;
    }
    if (region == TradeRegion::sell_staging_slot ||
        region == TradeRegion::sell_staging_scroll_left ||
        region == TradeRegion::sell_staging_scroll_right) {
        if (delta > 0)
            sell_staging_scroll_ = std::min(sell_staging_scroll_ + 3u, sell_staging_max_scroll());
        else
            sell_staging_scroll_ = sell_staging_scroll_ > 3u ? sell_staging_scroll_ - 3u : 0u;
        return true;
    }
    const bool over_hero = region == TradeRegion::hero_slot || region == TradeRegion::hero_filter ||
                           region == TradeRegion::hero_scroll_left ||
                           region == TradeRegion::hero_scroll_right;
    if (over_hero) {
        if (delta > 0)
            hero_scroll_ = std::min(hero_scroll_ + 3u, hero_max_scroll());
        else
            hero_scroll_ = hero_scroll_ > 3u ? hero_scroll_ - 3u : 0u;
    } else {
        if (delta > 0)
            npc_scroll_ = std::min(npc_scroll_ + 3u, npc_max_scroll());
        else
            npc_scroll_ = npc_scroll_ > 3u ? npc_scroll_ - 3u : 0u;
    }
    return true;
}

TradeRegion TradeOverlay::hit_test(int x, int y) const {
    if (!visible_ || scale_ <= 0.0f)
        return TradeRegion::none;
    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;
    if (logical_x < 0.0f || logical_y < 0.0f || logical_x >= TradeLayout::kCanvasWidth ||
        logical_y >= TradeLayout::kCanvasHeight)
        return TradeRegion::none;
    return layout_.hit_test(logical_x, logical_y);
}

} // namespace gl::app
