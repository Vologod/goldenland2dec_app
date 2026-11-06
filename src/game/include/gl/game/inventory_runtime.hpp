#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/script/native_registry.hpp"

#include <cstdint>
#include <map>
#include <string_view>

namespace gl::game {
class InventoryRuntime {
  public:
    static InventoryRuntime create(const gl::assets::HeroInventory &hero,
                                   const gl::assets::ItemNameIndex &names);
    std::int32_t count(std::string_view person, std::string_view tech_name) const;
    std::int32_t money() const;
    gl::script::InventoryAccess access();

  private:
    std::map<std::uint32_t, std::int32_t> counts_;
    std::int32_t money_ = 0;
    gl::assets::ItemNameIndex names_;
};
} // namespace gl::game
