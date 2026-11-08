#include "gl/game/inventory_runtime.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>

namespace gl::game {
namespace {
constexpr std::uint32_t kMoneyType = 18;

bool is_hero(std::string_view person) { return person == "Hero"; }
} // namespace

InventoryRuntime InventoryRuntime::create(const gl::assets::HeroInventory &hero,
                                          const gl::assets::ItemNameIndex &names,
                                          ItemTemplateLookup item_template) {
    InventoryRuntime runtime;
    runtime.hero_inventory_ = hero;
    runtime.names_ = names;
    runtime.item_template_ = std::move(item_template);
    return runtime;
}

bool InventoryRuntime::add_item(std::string_view person, std::string_view tech_name,
                                std::int32_t count) {
    if (!is_hero(person) || count <= 0)
        return false;
    const auto item_id = names_.find(tech_name);
    if (!item_id || !item_template_)
        return false;
    const auto item_template = item_template_(*item_id);
    if (!item_template || item_template->max_stack == 0)
        return false;

    std::uint32_t remaining = static_cast<std::uint32_t>(count);
    const std::uint32_t max_stack = item_template->max_stack;
    if (max_stack >= 2) {
        for (gl::assets::InventoryItem &item : hero_inventory_.items) {
            if (item.item_id != *item_id || item.count >= max_stack)
                continue;
            const std::uint32_t added = std::min(remaining, max_stack - item.count);
            item.count += added;
            remaining -= added;
            if (remaining == 0)
                return true;
        }
    }
    while (remaining != 0) {
        const std::uint32_t added = std::min(remaining, max_stack);
        hero_inventory_.items.push_back(
            {*item_id, added, item_template->flags, item_template->type,
             item_template->style});
        remaining -= added;
    }
    return true;
}

bool InventoryRuntime::remove_item(std::string_view person, std::string_view tech_name,
                                   std::int32_t count) {
    if (!is_hero(person) || count <= 0)
        return false;
    const auto item_id = names_.find(tech_name);
    if (!item_id)
        return false;

    std::uint32_t remaining = static_cast<std::uint32_t>(count);
    bool changed = false;
    auto &items = hero_inventory_.items;
    for (auto it = items.begin(); it != items.end();) {
        if (it->item_id != *item_id) {
            ++it;
            continue;
        }
        if (remaining < it->count) {
            it->count -= remaining;
            return true;
        }
        remaining -= it->count;
        it = items.erase(it);
        changed = true;
        if (remaining == 0)
            return changed;
    }
    return changed;
}

std::int32_t InventoryRuntime::count(std::string_view person, std::string_view tech_name) const {
    const auto item_id = names_.find(tech_name);
    if (!item_id)
        return 0;
    const std::vector<gl::assets::InventoryItem> &items = person_items(person);
    std::int64_t total = 0;
    for (const gl::assets::InventoryItem &item : items) {
        if (item.item_id == *item_id)
            total += item.count;
    }
    return static_cast<std::int32_t>(total);
}

std::int32_t InventoryRuntime::money() const {
    std::int64_t total = 0;
    for (const gl::assets::InventoryItem &item : hero_inventory_.items) {
        if (item.type == kMoneyType)
            total += item.count;
    }
    return static_cast<std::int32_t>(total);
}

const gl::assets::HeroInventory &InventoryRuntime::hero_inventory() const {
    return hero_inventory_;
}

const std::vector<gl::assets::InventoryItem> &
InventoryRuntime::person_items(std::string_view person) const {
    static const std::vector<gl::assets::InventoryItem> empty;
    if (is_hero(person))
        return hero_inventory_.items;
    const auto it = persons_.find(person);
    return it == persons_.end() ? empty : it->second;
}

void InventoryRuntime::set_person_items(std::string name,
                                        std::vector<gl::assets::InventoryItem> items) {
    if (is_hero(name))
        return;
    persons_.insert_or_assign(std::move(name), std::move(items));
}

void InventoryRuntime::clear_persons() { persons_.clear(); }

gl::script::InventoryAccess InventoryRuntime::access() {
    gl::script::InventoryAccess access;
    access.item_count = [this](std::string_view person, std::string_view tech_name) {
        return count(person, tech_name);
    };
    access.money = [this] { return money(); };
    access.add_item = [this](std::string_view person, std::string_view tech_name,
                             std::int32_t item_count) { add_item(person, tech_name, item_count); };
    access.remove_item = [this](std::string_view person, std::string_view tech_name,
                                std::int32_t item_count) {
        remove_item(person, tech_name, item_count);
    };
    return access;
}
} // namespace gl::game
