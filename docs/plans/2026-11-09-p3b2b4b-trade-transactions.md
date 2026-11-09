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

- [ ] **Step 1: Transactions**

Read Client/Server buy/sell functions; record RVA/file:line: item transfer
order, money movement, count, confirm, errors (no money/stock/slot),
rounding.

- [ ] **Step 2: Prices**

Confirm price formula/inputs (ITM price, durability, SKILL_TRADE), min 1,
buy/sell reciprocal, count multiply; record exact truncation.

- [ ] **Step 3: Money/containers**

Record NPC/Hero money item/container and mutations.

- [ ] **Step 4: Real oracle**

Boat trader slot0/slot2: selected item buy/sell prices, stock/money
before/after; node path/answers.

- [ ] **Step 5: Verdict/commit**

`implementable`/`blocked`; UTF-8; commit `docs(research): record trade transactions`.

---

### Task 2: `TradePrice` + transaction methods

**Files:**
- Create: `src/game/include/gl/game/trade_price.hpp`
- Create: `src/game/src/trade_price.cpp`
- Modify: `src/game/include/gl/game/inventory_runtime.hpp`
- Modify: `src/game/src/inventory_runtime.cpp`
- Create/modify: `tests/game/test_trade_price.cpp`, `test_inventory_runtime.cpp`

- [ ] **Step 1: Failing price tests** from research; commit.
- [ ] **Step 2: Failing buy/sell tests** (success/insufficient/merge/min1/count/invariants).
- [ ] **Step 3: Implement** price + transactions; register CMake.
- [ ] **Step 4: Run `gl_game_tests`; commit** `feat(game): add trade transactions`.

---

### Task 3: Interactive trade overlay

**Files:**
- Modify: `src/app/include/gl/app/trade_overlay.hpp`
- Modify: `src/app/src/trade_overlay.cpp`
- Modify: `tests/app/test_trade_overlay.cpp`

- [ ] **Step 1: Add price display/buy/sell controls** per research layout;
read-only selection remains.
- [ ] **Step 2: Callbacks** `buy`/`sell`/`price`; update snapshot after action;
error message/log; no direct mutation.
- [ ] **Step 3: Hit-test/input tests**; screenshots.
- [ ] **Step 4: Commit** `feat(app): add trade buy/sell controls`.

---

### Task 4: glgame integration + real buy/sell E2E

**Files:**
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp`

- [ ] **Step 1: Wire callbacks** to runtime; refresh overlay data after action.
- [ ] **Step 2: E2E** boat trader slot0/slot2 buy/sell via production path;
assert money/stock/prices; errors.
- [ ] **Step 3: Smoke** `l1_1 --click-npc 13`; screenshots.
- [ ] **Step 4: Commit** `feat(tools): wire trade transactions`.

---

### Task 5: Docs, parity, regression

**Files:**
- Modify: `docs/DECISIONS.md` (D-058)
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`

- [ ] **Step 1: D-058** — transaction/price semantics, errors, runtime-only,
quantity scope, deviations.
- [ ] **Step 2: parity** — trade rows; recompute counts.
- [ ] **Step 3: STATUS/PROJECT** — done, exact tests, next.
- [ ] **Step 4: Full ctest; encoding.**
- [ ] **Step 5: Commit** `docs: record trade transactions`.
