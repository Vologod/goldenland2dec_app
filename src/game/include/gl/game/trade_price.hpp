#pragma once
#include <cstdint>
#include <optional>

namespace gl::game {

// The port evaluates the price formula with C++ `double` (IEEE-754 binary64,
// 53-bit significand, SSE2 on x86-64), which is equivalent to x87 precision
// control PC=53. Extended (80-bit) arithmetic must not be used, and the
// translation unit is compiled with FP contraction disabled
// (`-ffp-contract=off`, MSVC `/fp:precise`) so no FMA can widen an
// intermediate.
//
// All inputs are required; there are no default values. A price is
// unsupported unless:
// - `assume_pc53` is explicitly true (caller verified/assumes PC=53);
// - `item_price > 0`, `durability <= 100`, `count > 0`;
// - the modifier block is absent or gated by `(flags & 0x80) != 0`
//   (magic modifier tables are not decoded);
// - the intermediate/total values fit in int64/uint32.
//
// `gv_addon` and perk modifiers (0x10/0x400) are not modelled.
struct TradePriceInput {
    std::uint32_t item_id;
    std::uint32_t item_price;
    std::uint32_t durability;
    std::uint32_t flags;
    std::uint32_t mod_count;
    std::uint32_t skill_trade;
    std::uint32_t count;
    bool assume_pc53;
};

// Whole-stack totals for one record (`count` is the record's count).
// `item_id` identifies the quoted record; transaction methods reject a quote
// for a different item id.
struct TradePrice {
    std::uint32_t item_id = 0;
    std::int64_t buy = 0;
    std::int64_t sell = 0;
    std::uint32_t count = 0;
};

std::optional<TradePrice> compute_trade_price(const TradePriceInput &input);

} // namespace gl::game
