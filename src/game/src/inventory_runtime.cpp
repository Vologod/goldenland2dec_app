#include "gl/game/inventory_runtime.hpp"

#include <cstdint>
#include <string_view>

namespace gl::game {
namespace {
constexpr std::uint32_t kMoneyType = 18;
} // namespace

InventoryRuntime InventoryRuntime::create(const gl::assets::HeroInventory &hero,
                                          const gl::assets::ItemNameIndex &names) {
    InventoryRuntime runtime;
    runtime.names_ = names;
    for (const gl::assets::InventoryItem &item : hero.items) {
        runtime.counts_[item.item_id] += static_cast<std::int32_t>(item.count);
        if (item.type == kMoneyType)
            runtime.money_ += static_cast<std::int32_t>(item.count);
    }
    return runtime;
}

std::int32_t InventoryRuntime::count(std::string_view person,
                                     std::string_view tech_name) const {
    if (person != "Hero")
        return 0;
    const auto item_id = names_.find(tech_name);
    if (!item_id)
        return 0;
    const auto it = counts_.find(*item_id);
    return it == counts_.end() ? 0 : it->second;
}

std::int32_t InventoryRuntime::money() const { return money_; }

gl::script::InventoryAccess InventoryRuntime::access() {
    gl::script::InventoryAccess access;
    access.item_count = [this](std::string_view person, std::string_view tech_name) {
        return count(person, tech_name);
    };
    access.money = [this] { return money(); };
    return access;
}
} // namespace gl::game
