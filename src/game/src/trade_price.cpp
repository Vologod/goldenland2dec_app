#include "gl/game/trade_price.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

#if defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace gl::game {

std::optional<TradePrice> compute_trade_price(const TradePriceInput &input) {
    if (!input.assume_x87_single)
        return std::nullopt;
    if (input.item_price == 0 || input.durability > 100 || input.count == 0)
        return std::nullopt;
    if (input.mod_count != 0 && (input.flags & 0x80u) == 0)
        return std::nullopt;

    const float durability_scaled = static_cast<float>(
        static_cast<double>(input.durability) * 0.0025);
    const float durability_factor = static_cast<float>(
        static_cast<double>(durability_scaled) + 0.75);
    const float base_scaled = static_cast<float>(
        static_cast<double>(durability_factor) * static_cast<double>(input.item_price));
    const std::int64_t base0 = static_cast<std::int64_t>(base_scaled);

    std::int64_t base = base0;
    if (base < base0 / 3)
        base = base0 / 3;

    const float skill_scaled = static_cast<float>(
        static_cast<double>(input.skill_trade) * -0.1);
    float factor = static_cast<float>(static_cast<double>(skill_scaled) + 2.5);
    if (factor < 1.0f)
        factor = 1.0f;

    const float buy_factor = factor;
    const float sell_factor = static_cast<float>(1.0 / static_cast<double>(factor));
    std::int64_t buy_unit = static_cast<std::int64_t>(
        static_cast<float>(static_cast<double>(buy_factor) * static_cast<double>(base)));
    std::int64_t sell_unit = static_cast<std::int64_t>(
        static_cast<float>(static_cast<double>(sell_factor) * static_cast<double>(base)));
    if (buy_unit < 1)
        buy_unit = 1;
    if (sell_unit < 1)
        sell_unit = 1;

    const std::int64_t max_total = std::numeric_limits<std::int64_t>::max();
    if (buy_unit > max_total / static_cast<std::int64_t>(input.count))
        return std::nullopt;
    if (sell_unit > max_total / static_cast<std::int64_t>(input.count))
        return std::nullopt;

    TradePrice price;
    price.item_id = input.item_id;
    price.buy = buy_unit * static_cast<std::int64_t>(input.count);
    price.sell = sell_unit * static_cast<std::int64_t>(input.count);
    price.count = input.count;
    assert(price.buy >= 0 && price.sell >= 0);
    return price;
}

} // namespace gl::game
