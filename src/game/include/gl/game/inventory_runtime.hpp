#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/game/trade_price.hpp"
#include "gl/script/native_registry.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gl::game {
struct ItemTemplate {
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t style = 0;
    std::uint32_t max_stack = 0;
    std::uint32_t mod_count = 0;
};

enum class TradeStatus {
    ok,
    missing_person,
    missing_template,
    insufficient_stock,
    insufficient_money,
    unsupported_price,
    not_available,
};

class InventoryRuntime {
  public:
    using ItemTemplateLookup = std::function<std::optional<ItemTemplate>(std::uint32_t)>;

    static InventoryRuntime create(const gl::assets::HeroInventory &hero,
                                   const gl::assets::ItemNameIndex &names,
                                   ItemTemplateLookup item_template = {});
    bool add_item(std::string_view person, std::string_view tech_name, std::int32_t count);
    bool remove_item(std::string_view person, std::string_view tech_name, std::int32_t count);
    bool transfer_item(std::string_view from, std::string_view to, std::string_view tech_name,
                       std::int32_t count);
    bool transfer_all(std::string_view from, std::string_view to);
    // One call moves the whole selected source stack (count comes from the
    // source record; there is no count parameter). `price` must be the
    // whole-stack price for that record.
    TradeStatus buy_trade(std::string_view npc, std::string_view hero, std::string_view tech_name,
                          const TradePrice &price);
    TradeStatus sell_trade(std::string_view npc, std::string_view hero, std::string_view tech_name,
                           const TradePrice &price);
    // Stages are settled in one step: each quote identifies a whole source
    // record (`item_id`, `count`). Buy quotes move NPC trade -> Hero main,
    // sell quotes move Hero main -> NPC trade, then the net money difference
    // is settled. Any error leaves every container untouched. Quotes are
    // trusted caller data: identity and count are validated, but the price
    // is not recomputed here (pricing and execution are coupled only through
    // the caller's provider, D-058).
    TradeStatus exchange_trade(std::string_view npc, std::string_view hero,
                               const std::vector<TradePrice> &buy_items,
                               const std::vector<TradePrice> &sell_items);
    std::int32_t count(std::string_view person, std::string_view tech_name) const;
    std::int32_t money() const;
    const gl::assets::HeroInventory &hero_inventory() const;
    const std::vector<gl::assets::InventoryItem> &person_items(std::string_view person) const;
    void set_person_items(std::string name, std::vector<gl::assets::InventoryItem> items);
    const std::vector<gl::assets::InventoryItem> &trade_items(std::string_view person) const;
    // Trade containers are keyed independently of main containers. Hero is
    // allowed here (unlike set_person_items) so an explicit trade list can be
    // supplied without touching Hero main; it stays empty unless set.
    void set_person_trade_items(std::string name, std::vector<gl::assets::InventoryItem> items);
    std::int32_t trade_money(std::string_view person) const;
    void clear_persons();
    gl::script::InventoryAccess access();

  private:
    std::vector<gl::assets::InventoryItem> *mutable_items(std::string_view person);
    TradeStatus execute_trade(std::vector<gl::assets::InventoryItem> &item_source,
                              std::vector<gl::assets::InventoryItem> &item_dest,
                              std::vector<gl::assets::InventoryItem> &money_source,
                              std::vector<gl::assets::InventoryItem> &money_dest,
                              std::uint32_t item_id, const TradePrice &price, std::int64_t total);

    gl::assets::HeroInventory hero_inventory_;
    std::map<std::string, std::vector<gl::assets::InventoryItem>, std::less<>> persons_;
    std::map<std::string, std::vector<gl::assets::InventoryItem>, std::less<>> trades_;
    gl::assets::ItemNameIndex names_;
    ItemTemplateLookup item_template_;
};
} // namespace gl::game
