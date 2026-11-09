# Phase 1b2b4b — trade transactions (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Интерактивные buy/sell в trade overlay с ценами оригинала и обновлением денег/стоков.

**Architecture:** `InventoryRuntime` transaction methods + `TradePrice`; `TradeOverlay` callbacks; glgame wiring.

**Tech Stack:** C++20, CMake + Ninja, doctest, SDL2.

## Global Constraints

- Server/Client DLL — источник (D-029); без догадок.
- Слои вниз; SDL только app; gl_game без SDL.
- Docs UTF-8 без BOM/NUL; коммиты разрешены.
- Save не пишется; specials/trade natives вне scope.

---

### Task 1: Research buy/sell and prices

**Files:**
- Create: `docs/research/p3b2b4b-trade-transactions.md`

- [x] **Step 1: Transactions**

Read Client/Server buy/sell functions; record RVA/file:line: item transfer
order, money movement, count, confirm, errors (no money/stock/slot),
rounding.

- [x] **Step 2: Prices**

Confirm price formula/inputs (ITM price, durability, SKILL_TRADE), min 1,
buy/sell reciprocal, count multiply; record exact truncation.

- [x] **Step 3: Money/containers**

Record NPC/Hero money item/container and mutations.

- [x] **Step 4: Real oracle**

Boat trader slot0/slot2: selected item buy/sell prices, stock/money
before/after; node path/answers.

- [x] **Step 5: Verdict/commit**

`implementable`/`blocked`; UTF-8; commit `docs(research): record trade transactions`.

---

### Task 2: `TradePrice` + transaction methods

**Interfaces:** `buy_trade(npc, hero, tech, price)` /
`sell_trade(npc, hero, tech, price)` — one call moves the **whole
selected stack** (count from the runtime record; no `count` parameter).
`TradePrice` computes only under x87 PC=53/double and returns
`unsupported` for `mod_count != 0 && (flags&0x80)==0` (buy and sell) or
if the live FPU CW is not PC=53.

**Files:**
- Create: `src/game/include/gl/game/trade_price.hpp`
- Create: `src/game/src/trade_price.cpp`
- Modify: `src/game/include/gl/game/inventory_runtime.hpp`
- Modify: `src/game/src/inventory_runtime.cpp`
- Create/modify: `tests/game/test_trade_price.cpp`, `test_inventory_runtime.cpp`

- [x] **Step 1: Failing price tests** from research (PC=53/double; min 1;
buy/sell reciprocal; modifier guard → unsupported); commit.
- [x] **Step 2: Failing whole-stack buy/sell tests** (success/insufficient
money/stock/merge/min1/no-mutation/invariants; no `count` parameter;
conditional single-item path `count>1`+query `10|0xB` is a known case, not
implemented/tested yet).
- [x] **Step 3: Implement** whole-stack price + transactions + guards;
register CMake.
- [x] **Step 4: Run `gl_game_tests`; commit** `feat(game): add trade transactions`.

---

### Task 3: Staging + Exchange (overlay + runtime)

**Files:**
- Modify: `src/game/include/gl/game/inventory_runtime.hpp`
- Modify: `src/game/src/inventory_runtime.cpp`
- Modify: `src/app/include/gl/app/trade_overlay.hpp`
- Modify: `src/app/src/trade_overlay.cpp`
- Modify: `tests/game/test_inventory_runtime.cpp`
- Modify: `tests/app/test_trade_overlay.cpp`

- [x] **Step 1: `exchange_trade(npc, hero, buy_items, sell_items)`**
(whole-stack, net money per research §2.3-2.5; error-atomic; tests for
buy>sell/sell>buy/equal/insufficient NPC/hero money/unsupported).
- [x] **Step 2: Staging UI** — middle row: click NPC item stages buy,
click Hero item stages sell, click staged returns; totals y=386; prices
y=126/386/612; Exchange button obj 44 (`lbig2`, 579,320); no separate
confirm.
- [x] **Step 3: Hit-test/input/totals tests**; screenshots; read-only
state remains for un-staged lists.
- [x] **Step 4: Commit** `feat(app): add trade staging and exchange`.

---

### Task 4: glgame integration + real buy/sell E2E

**Files:**
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp`

- [x] **Step 1: Wire callbacks** to runtime; refresh overlay data after action.
- [x] **Step 2: E2E** boat trader slot0/slot2 whole-stack buy/sell via
production path; assert money/stock/prices; errors. Before-state is
real/validated; after-state oracle is analytic (computed under PC=53,
not observed) — record that caveat.
- [x] **Step 3: Smoke** `l1_1 --click-npc 13`; screenshots.
- [x] **Step 4: Commit** `feat(tools): wire trade transactions`.

---

### Task 5: Docs, parity, regression

**Files:**
- Modify: `docs/DECISIONS.md` (D-058)
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`

- [x] **Step 1: D-058** — transaction/price semantics (PC=53/double),
whole-stack scope, modifier guard, errors, runtime-only, deviations.
- [x] **Step 2: parity** — trade rows; recompute counts.
- [x] **Step 3: STATUS/PROJECT** — done, exact tests, next.
- [x] **Step 4: Full ctest; encoding.**
- [x] **Step 5: Commit** `docs: record trade transactions`.
