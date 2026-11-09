#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_name_index.hpp"
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
    std::int32_t count(std::string_view person, std::string_view tech_name) const;
    std::int32_t money() const;
    const gl::assets::HeroInventory &hero_inventory() const;
    const std::vector<gl::assets::InventoryItem> &person_items(std::string_view person) const;
    void set_person_items(std::string name, std::vector<gl::assets::InventoryItem> items);
    const std::vector<gl::assets::InventoryItem> &trade_items(std::string_view person) const;
    void set_person_trade_items(std::string name, std::vector<gl::assets::InventoryItem> items);
    std::int32_t trade_money(std::string_view person) const;
    void clear_persons();
    gl::script::InventoryAccess access();

  private:
    std::vector<gl::assets::InventoryItem> *mutable_items(std::string_view person);

    gl::assets::HeroInventory hero_inventory_;
    std::map<std::string, std::vector<gl::assets::InventoryItem>, std::less<>> persons_;
    std::map<std::string, std::vector<gl::assets::InventoryItem>, std::less<>> trades_;
    gl::assets::ItemNameIndex names_;
    ItemTemplateLookup item_template_;
};
} // namespace gl::game
