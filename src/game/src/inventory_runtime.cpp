#include "gl/game/inventory_runtime.hpp"

#include "gl/assets/log.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace gl::game {
namespace {
constexpr std::uint32_t kMoneyType = 18;

bool is_hero(std::string_view person) { return person == "Hero"; }

bool append_items(std::vector<gl::assets::InventoryItem> &items, std::uint32_t item_id,
                  const ItemTemplate &item_template, std::uint32_t count) {
    if (item_template.max_stack == 0)
        return false;
    std::uint32_t remaining = count;
    if (item_template.max_stack >= 2) {
        for (gl::assets::InventoryItem &item : items) {
            if (item.item_id != item_id || item.count >= item_template.max_stack)
                continue;
            const std::uint32_t added = std::min(remaining, item_template.max_stack - item.count);
            item.count += added;
            remaining -= added;
            if (remaining == 0)
                return true;
        }
    }
    while (remaining != 0) {
        const std::uint32_t added = std::min(remaining, item_template.max_stack);
        items.push_back(
            {item_id, added, item_template.flags, item_template.type, item_template.style});
        remaining -= added;
    }
    return true;
}
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
    if (!item_template)
        return false;
    return append_items(hero_inventory_.items, *item_id, *item_template,
                        static_cast<std::uint32_t>(count));
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

bool InventoryRuntime::transfer_item(std::string_view from, std::string_view to,
                                     std::string_view tech_name, std::int32_t count) {
    std::vector<gl::assets::InventoryItem> *source = mutable_items(from);
    std::vector<gl::assets::InventoryItem> *dest = mutable_items(to);
    if (source == nullptr || dest == nullptr)
        return false;
    if (from == to)
        return true;

    std::uint32_t remaining = count < 0 ? 1000000u : static_cast<std::uint32_t>(count);
    if (remaining == 0)
        return true;

    const auto item_id = names_.find(tech_name);
    if (!item_id || !item_template_)
        return count < 0;
    const auto item_template = item_template_(*item_id);
    if (!item_template || item_template->max_stack == 0)
        return count < 0;

    while (remaining != 0) {
        const auto it = std::find_if(
            source->begin(), source->end(),
            [&](const gl::assets::InventoryItem &item) { return item.item_id == *item_id; });
        if (it == source->end())
            return remaining != static_cast<std::uint32_t>(count);
        const std::uint32_t available = it->count;
        if (remaining < available) {
            it->count = available - remaining;
            append_items(*dest, *item_id, *item_template, remaining);
            return true;
        }
        append_items(*dest, *item_id, *item_template, available);
        remaining -= available;
        source->erase(it);
    }
    return true;
}

bool InventoryRuntime::transfer_all(std::string_view from, std::string_view to) {
    std::vector<gl::assets::InventoryItem> *source = mutable_items(from);
    std::vector<gl::assets::InventoryItem> *dest = mutable_items(to);
    if (source == nullptr || dest == nullptr)
        return false;
    if (from == to)
        return true;

    for (const gl::assets::InventoryItem &item : *source) {
        if (!item_template_)
            return false;
        const auto item_template = item_template_(item.item_id);
        if (!item_template || item_template->max_stack == 0)
            return false;
    }

    std::size_t index = 0;
    while (index < source->size()) {
        const gl::assets::InventoryItem item = (*source)[index];
        const auto item_template = item_template_(item.item_id);
        append_items(*dest, item.item_id, *item_template, item.count);
        source->erase(source->begin() + static_cast<std::ptrdiff_t>(index));
    }
    return true;
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
    const auto [it, inserted] = persons_.try_emplace(std::move(name), std::move(items));
    if (!inserted) {
        gl::log(gl::LogLevel::warning,
                "inventory runtime: duplicate person name '" + it->first + "' ignored");
    }
}

const std::vector<gl::assets::InventoryItem> &
InventoryRuntime::trade_items(std::string_view person) const {
    static const std::vector<gl::assets::InventoryItem> empty;
    const auto it = trades_.find(person);
    return it == trades_.end() ? empty : it->second;
}

void InventoryRuntime::set_person_trade_items(std::string name,
                                              std::vector<gl::assets::InventoryItem> items) {
    const auto [it, inserted] = trades_.try_emplace(std::move(name), std::move(items));
    if (!inserted) {
        gl::log(gl::LogLevel::warning,
                "inventory runtime: duplicate trade name '" + it->first + "' ignored");
    }
}

std::int32_t InventoryRuntime::trade_money(std::string_view person) const {
    std::int64_t total = 0;
    for (const gl::assets::InventoryItem &item : trade_items(person)) {
        if (item.type == kMoneyType)
            total += item.count;
    }
    return static_cast<std::int32_t>(total);
}

void InventoryRuntime::clear_persons() {
    persons_.clear();
    trades_.clear();
}

std::vector<gl::assets::InventoryItem> *InventoryRuntime::mutable_items(std::string_view person) {
    if (is_hero(person))
        return &hero_inventory_.items;
    const auto it = persons_.find(person);
    return it == persons_.end() ? nullptr : &it->second;
}

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
    access.transfer_item = [this](std::string_view from, std::string_view to,
                                  std::string_view tech_name, std::int32_t item_count) {
        return transfer_item(from, to, tech_name, item_count);
    };
    access.transfer_all = [this](std::string_view from, std::string_view to) {
        return transfer_all(from, to);
    };
    return access;
}
} // namespace gl::game
