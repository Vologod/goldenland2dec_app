#include "gl/game/trade_price.hpp"

#include <cmath>
#include <cstdint>

namespace gl::game {

std::optional<TradePrice> compute_trade_price(const TradePriceInput &input) {
    if (!input.fpu_pc53)
        return std::nullopt;
    if (input.count == 0)
        return std::nullopt;
    if (input.mod_count != 0 && (input.flags & 0x80u) == 0)
        return std::nullopt;

    const double durability_factor = static_cast<double>(input.durability) * 0.0025 + 0.75;
    const double base_scaled = durability_factor * static_cast<double>(input.item_price);
    const std::int64_t base0 = static_cast<std::int64_t>(std::trunc(base_scaled));

    std::int64_t base = base0;
    if (base < base0 / 3)
        base = base0 / 3;

    double factor = 2.5 - 0.1 * static_cast<double>(input.skill_trade);
    if (factor < 1.0)
        factor = 1.0;

    const double buy_factor = factor;
    const double sell_factor = 1.0 / factor;
    std::int64_t buy_unit =
        static_cast<std::int64_t>(std::trunc(buy_factor * static_cast<double>(base)));
    std::int64_t sell_unit =
        static_cast<std::int64_t>(std::trunc(sell_factor * static_cast<double>(base)));
    if (buy_unit < 1)
        buy_unit = 1;
    if (sell_unit < 1)
        sell_unit = 1;

    TradePrice price;
    price.buy = buy_unit * static_cast<std::int64_t>(input.count);
    price.sell = sell_unit * static_cast<std::int64_t>(input.count);
    price.count = input.count;
    return price;
}

} // namespace gl::game
