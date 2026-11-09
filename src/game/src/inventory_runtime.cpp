#include "gl/game/inventory_runtime.hpp"

#include "gl/assets/log.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace gl::game {
namespace {
constexpr std::uint32_t kMoneyType = 18;
constexpr std::string_view kMoneyTech = "MON_1_0_1";

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

bool append_items_preserving(std::vector<gl::assets::InventoryItem> &items, std::uint32_t item_id,
                             const gl::assets::InventoryItem &source, std::uint32_t max_stack,
                             std::uint32_t count) {
    if (max_stack == 0)
        return false;
    std::uint32_t remaining = count;
    if (max_stack >= 2) {
        for (gl::assets::InventoryItem &item : items) {
            if (item.item_id != item_id || item.count >= max_stack)
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
        items.push_back({item_id, added, source.flags, source.type, source.style, source.durability,
                         source.mod_count});
        remaining -= added;
    }
    return true;
}

bool tradeable_record(const gl::assets::InventoryItem &item) {
    if (item.count == 0)
        return false;
    if (item.type == 18 || item.type == 25)
        return false;
    return item.mod_count == 0 || (item.flags & 0x80u) != 0;
}

std::uint64_t money_units(const std::vector<gl::assets::InventoryItem> &items,
                          std::uint32_t money_id) {
    std::uint64_t total = 0;
    for (const gl::assets::InventoryItem &item : items) {
        if (item.item_id == money_id)
            total += item.count;
    }
    return total;
}

bool remove_item_count(std::vector<gl::assets::InventoryItem> &items, std::uint32_t item_id,
                       std::uint64_t amount) {
    std::uint64_t remaining = amount;
    if (remaining == 0)
        return true;
    for (auto it = items.begin(); it != items.end();) {
        if (it->item_id != item_id) {
            ++it;
            continue;
        }
        if (remaining < it->count) {
            it->count -= static_cast<std::uint32_t>(remaining);
            return true;
        }
        remaining -= it->count;
        it = items.erase(it);
        if (remaining == 0)
            return true;
    }
    return false;
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
        const gl::assets::InventoryItem moved = *it;
        const std::uint32_t available = it->count;
        if (remaining < available) {
            append_items_preserving(*dest, *item_id, moved, item_template->max_stack, remaining);
            it->count = available - remaining;
            return true;
        }
        append_items_preserving(*dest, *item_id, moved, item_template->max_stack, available);
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
        append_items_preserving(*dest, item.item_id, item, item_template->max_stack, item.count);
        source->erase(source->begin() + static_cast<std::ptrdiff_t>(index));
    }
    return true;
}

TradeStatus InventoryRuntime::execute_trade(std::vector<gl::assets::InventoryItem> &item_source,
                                            std::vector<gl::assets::InventoryItem> &item_dest,
                                            std::vector<gl::assets::InventoryItem> &money_source,
                                            std::vector<gl::assets::InventoryItem> &money_dest,
                                            std::uint32_t item_id, const TradePrice &price,
                                            std::int64_t total) {
    // The original command carries a signed 32-bit money delta; a larger
    // amount is not representable and is rejected (D-058 note).
    if (total <= 0 || total > std::numeric_limits<std::int32_t>::max())
        return TradeStatus::unsupported_price;
    if (price.item_id != item_id || price.count == 0)
        return TradeStatus::unsupported_price;

    const auto source = std::find_if(
        item_source.begin(), item_source.end(),
        [item_id, &price](const gl::assets::InventoryItem &item) {
            return item.item_id == item_id && item.count == price.count;
        });
    if (source == item_source.end()) {
        const bool has_item = std::any_of(
            item_source.begin(), item_source.end(), [item_id](const gl::assets::InventoryItem &item) {
                return item.item_id == item_id && item.count != 0;
            });
        return has_item ? TradeStatus::unsupported_price : TradeStatus::insufficient_stock;
    }
    if (!tradeable_record(*source))
        return TradeStatus::unsupported_price;
    const std::uint32_t count = source->count;

    // Money is identified by MON_1_0_1 name (as FUN_140484C8 does), while
    // money()/trade_money() sum any type-18 record. The asymmetry is
    // intentional: only MON_1_0_1 can be moved by a trade.
    const auto money_id = names_.find(kMoneyTech);
    if (!money_id || !item_template_)
        return TradeStatus::missing_template;
    const auto item_template = item_template_(item_id);
    if (!item_template || item_template->max_stack == 0)
        return TradeStatus::missing_template;
    const auto money_template = item_template_(*money_id);
    if (!money_template || money_template->max_stack == 0)
        return TradeStatus::missing_template;

    std::uint64_t available = 0;
    for (const gl::assets::InventoryItem &item : money_source) {
        if (item.item_id == *money_id)
            available += item.count;
    }
    if (available < static_cast<std::uint64_t>(total))
        return TradeStatus::insufficient_money;

    // New destination records copy the moved record's instance metadata
    // (flags/type/style); the ITM template supplies only max_stack for
    // merging/splitting. Merging keeps the destination record's flags.
    const gl::assets::InventoryItem moved = *source;
    item_source.erase(source);
    append_items_preserving(item_dest, item_id, moved, item_template->max_stack, count);
    if (!remove_item_count(money_source, *money_id, static_cast<std::uint64_t>(total))) {
        assert(false && "money availability validated before trade");
        return TradeStatus::insufficient_money;
    }
    append_items(money_dest, *money_id, *money_template, static_cast<std::uint32_t>(total));
    return TradeStatus::ok;
}

TradeStatus InventoryRuntime::buy_trade(std::string_view npc, std::string_view hero,
                                        std::string_view tech_name, const TradePrice &price) {
    if (!is_hero(hero))
        return TradeStatus::missing_person;
    const auto trade = trades_.find(npc);
    if (trade == trades_.end())
        return TradeStatus::missing_person;
    const auto item_id = names_.find(tech_name);
    if (!item_id)
        return TradeStatus::missing_template;
    return execute_trade(trade->second, hero_inventory_.items, hero_inventory_.items, trade->second,
                         *item_id, price, price.buy);
}

TradeStatus InventoryRuntime::sell_trade(std::string_view npc, std::string_view hero,
                                         std::string_view tech_name, const TradePrice &price) {
    if (!is_hero(hero))
        return TradeStatus::missing_person;
    const auto trade = trades_.find(npc);
    if (trade == trades_.end())
        return TradeStatus::missing_person;
    const auto item_id = names_.find(tech_name);
    if (!item_id)
        return TradeStatus::missing_template;
    return execute_trade(hero_inventory_.items, trade->second, trade->second, hero_inventory_.items,
                         *item_id, price, price.sell);
}

TradeStatus InventoryRuntime::exchange_trade(std::string_view npc, std::string_view hero,
                                             const std::vector<TradePrice> &buy_items,
                                             const std::vector<TradePrice> &sell_items) {
    if (!is_hero(hero))
        return TradeStatus::missing_person;
    const auto trade = trades_.find(npc);
    if (trade == trades_.end())
        return TradeStatus::missing_person;
    if (buy_items.empty() && sell_items.empty())
        return TradeStatus::ok;

    std::vector<gl::assets::InventoryItem> npc_items = trade->second;
    std::vector<gl::assets::InventoryItem> hero_items = hero_inventory_.items;

    std::vector<gl::assets::InventoryItem> moved_buy;
    moved_buy.reserve(buy_items.size());
    std::int64_t buy_total = 0;
    for (const TradePrice &quote : buy_items) {
        if (quote.buy <= 0 ||
            static_cast<std::uint64_t>(quote.buy) > std::numeric_limits<std::int32_t>::max())
            return TradeStatus::unsupported_price;
        buy_total += quote.buy;
        if (buy_total > std::numeric_limits<std::int32_t>::max())
            return TradeStatus::unsupported_price;

        const auto record = std::find_if(
            npc_items.begin(), npc_items.end(), [&quote](const gl::assets::InventoryItem &item) {
                return item.item_id == quote.item_id && item.count == quote.count;
            });
        if (record == npc_items.end()) {
            const bool has_item = std::any_of(
                npc_items.begin(), npc_items.end(), [&quote](const gl::assets::InventoryItem &item) {
                    return item.item_id == quote.item_id && item.count != 0;
                });
            return has_item ? TradeStatus::unsupported_price : TradeStatus::insufficient_stock;
        }
        if (!tradeable_record(*record))
            return TradeStatus::unsupported_price;
        if (!item_template_)
            return TradeStatus::missing_template;
        const auto item_template = item_template_(record->item_id);
        if (!item_template || item_template->max_stack == 0)
            return TradeStatus::missing_template;
        moved_buy.push_back(*record);
        npc_items.erase(record);
    }

    std::vector<gl::assets::InventoryItem> moved_sell;
    moved_sell.reserve(sell_items.size());
    std::int64_t sell_total = 0;
    for (const TradePrice &quote : sell_items) {
        if (quote.sell <= 0 ||
            static_cast<std::uint64_t>(quote.sell) > std::numeric_limits<std::int32_t>::max())
            return TradeStatus::unsupported_price;
        sell_total += quote.sell;
        if (sell_total > std::numeric_limits<std::int32_t>::max())
            return TradeStatus::unsupported_price;

        const auto record = std::find_if(
            hero_items.begin(), hero_items.end(), [&quote](const gl::assets::InventoryItem &item) {
                return item.item_id == quote.item_id && item.count == quote.count;
            });
        if (record == hero_items.end()) {
            const bool has_item = std::any_of(
                hero_items.begin(), hero_items.end(), [&quote](const gl::assets::InventoryItem &item) {
                    return item.item_id == quote.item_id && item.count != 0;
                });
            return has_item ? TradeStatus::unsupported_price : TradeStatus::insufficient_stock;
        }
        if (!tradeable_record(*record))
            return TradeStatus::unsupported_price;
        if (!item_template_)
            return TradeStatus::missing_template;
        const auto item_template = item_template_(record->item_id);
        if (!item_template || item_template->max_stack == 0)
            return TradeStatus::missing_template;
        moved_sell.push_back(*record);
        hero_items.erase(record);
    }

    const std::int64_t net = sell_total - buy_total;
    const auto money_id = names_.find(kMoneyTech);
    const std::optional<ItemTemplate> money_template =
        (money_id && item_template_) ? item_template_(*money_id) : std::nullopt;
    if (net != 0) {
        if (!money_id || !money_template || money_template->max_stack == 0)
            return TradeStatus::missing_template;
        if (net > 0) {
            if (money_units(npc_items, *money_id) < static_cast<std::uint64_t>(net))
                return TradeStatus::insufficient_money;
        } else if (money_units(hero_items, *money_id) < static_cast<std::uint64_t>(-net)) {
            return TradeStatus::insufficient_money;
        }
    }

    for (const gl::assets::InventoryItem &moved : moved_buy) {
        const auto item_template = item_template_(moved.item_id);
        append_items_preserving(hero_items, moved.item_id, moved, item_template->max_stack,
                                moved.count);
    }
    for (const gl::assets::InventoryItem &moved : moved_sell) {
        const auto item_template = item_template_(moved.item_id);
        append_items_preserving(npc_items, moved.item_id, moved, item_template->max_stack,
                                moved.count);
    }

    if (net > 0) {
        if (!remove_item_count(npc_items, *money_id, static_cast<std::uint64_t>(net))) {
            assert(false && "money availability validated before exchange");
            return TradeStatus::insufficient_money;
        }
        append_items(hero_items, *money_id, *money_template, static_cast<std::uint32_t>(net));
    } else if (net < 0) {
        if (!remove_item_count(hero_items, *money_id, static_cast<std::uint64_t>(-net))) {
            assert(false && "money availability validated before exchange");
            return TradeStatus::insufficient_money;
        }
        append_items(npc_items, *money_id, *money_template, static_cast<std::uint32_t>(-net));
    }

    trade->second = std::move(npc_items);
    hero_inventory_.items = std::move(hero_items);
    return TradeStatus::ok;
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
