#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/script/native_registry.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

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
    std::int32_t count(std::string_view person, std::string_view tech_name) const;
    std::int32_t money() const;
    const gl::assets::HeroInventory &hero_inventory() const;
    gl::script::InventoryAccess access();

  private:
    gl::assets::HeroInventory hero_inventory_;
    gl::assets::ItemNameIndex names_;
    ItemTemplateLookup item_template_;
};
} // namespace gl::game
