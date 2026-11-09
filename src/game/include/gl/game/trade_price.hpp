#pragma once
#include <cstdint>
#include <optional>

namespace gl::game {

// The port evaluates the price formula with C++ `double` (IEEE-754 binary64,
// 53-bit significand, SSE2 on x86-64), which is equivalent to x87 precision
// control PC=53. Extended (80-bit) arithmetic must not be used.
//
// `fpu_pc53` carries a live FPU control-word verification. When it is not
// PC=53 (or the verification is unavailable) the price is unsupported.
//
// Modifier prices are not decoded: a record with `mod_count != 0` and
// `(flags & 0x80) == 0` is unsupported for both buy and sell. `gv_addon` and
// perk modifiers (0x10/0x400) are not modelled.
struct TradePriceInput {
    std::uint32_t item_price = 0;
    std::uint32_t durability = 0;
    std::uint32_t flags = 0;
    std::uint32_t mod_count = 0;
    std::uint32_t skill_trade = 0;
    std::uint32_t count = 1;
    bool fpu_pc53 = true;
};

// Whole-stack totals for one record (`count` is the record's count).
struct TradePrice {
    std::int64_t buy = 0;
    std::int64_t sell = 0;
    std::uint32_t count = 0;
};

std::optional<TradePrice> compute_trade_price(const TradePriceInput &input);

} // namespace gl::game
