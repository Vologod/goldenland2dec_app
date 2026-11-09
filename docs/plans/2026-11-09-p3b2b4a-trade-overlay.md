# Phase 1b2b4a — read-only trade overlay (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Открыть read-only торговую панель по `RS_PassToTradePanel` с NPC/Hero items, деньгами и фильтрами.

**Architecture:** `TradeOverlay` в `gl_app`; `NativeServices::trade_panel`; данные из `InventoryRuntime`/`ItemCatalog`; layout из research.

**Tech Stack:** C++20, CMake + Ninja, doctest, SDL2.

## Global Constraints

- Server/Client DLL — источник поведения (D-029); без догадок.
- Слои вниз; SDL только в `platform/app/render/ui`; `gl_game` без SDL.
- Docs UTF-8 без BOM/NUL; коммиты разрешены.
- Read-only: никаких покупок/продаж/мутаций.

---

### Task 1: Research trade panel

**Files:**
- Create: `docs/research/p3b2b4a-trade-panel.md`

- [x] **Step 1: Assets/layout**

Record `engineres/trade/*.bmp` dimensions/positions and the GUI script
(`trade_panel`/`inv_gui.scr`/analog). Identify list rects, money, filters,
exit.

- [x] **Step 2: Client functions**

Find the trade panel open/close/list/price functions in `Client.dll`
(RVA/file:line); record the data source: NPC trade `entity+0xC`
(server/native proven; client `FUN_1206EC98` invocation unverified),
hero main `+0x10`, filters, prices (`ITM price`, `SKILL_TRADE`), two
money values.

- [x] **Step 3: Native semantics**

Record `RS_PassToTradePanel`/`FUN_14028404`: close flag, return, what the
client opens.

- [x] **Step 4: Real trader oracle**

Choose a real trader dialog; record expected NPC trade items/`npc_money`,
hero items/`hero_money` (counts/money + first 12 hero main), prices for
slot0/slot2; node ids.

- [x] **Step 5: Verdict/commit**

`implementable`; UTF-8 no BOM/nul=0; commit
`docs(research): record trade panel layout`. Gate: cleared.

---

### Task 2: Trade layout + overlay core

**Files:**
- Create: `src/app/include/gl/app/trade_overlay.hpp`
- Create: `src/app/src/trade_overlay.cpp`
- Modify: `src/app/CMakeLists.txt` (or actual list)
- Create: `tests/app/test_trade_overlay.cpp` (or `tests/...` per repo)
- Modify: tests CMake
- Modify: `src/game/include/gl/game/inventory_runtime.hpp`
- Modify: `src/game/src/inventory_runtime.cpp`
- Modify: `src/game/src/npc_loot.cpp`
- Create/modify: `tests/game/test_inventory_runtime*.cpp`

**Interfaces:**
```cpp
struct TradeItemView;
class TradeOverlay {
  public:
    void open(gl::assets::GameVfs&, const gl::assets::ItemCatalog&,
              std::string npc_name, const std::vector<InventoryItem>& npc_items,
              std::int32_t npc_money, const std::vector<InventoryItem>& hero_items,
              std::int32_t hero_money);
    void close();
    bool visible() const;
    void render(SDL_Renderer*, const TradeLayout&);
};
```

- [ ] **Step 0: retain `loot.hot` + NPC trade accessor.** Store pass 1
  (`hot`) per person in `InventoryRuntime`; add `trade_items(name)` and
  `trade_money(name)` (count `MON_1_0_1`); main/hero RNG unchanged.
  Unit tests: pass-1 retained, accessors, no change to main counts.
- [ ] **Step 1: Layout constants/model** from research; unit tests.
- [ ] **Step 2: Render/asset loading**; 1024×768 screenshot; two money
  values (`npc_money`, `hero_money`).
- [ ] **Step 3: Hit-test/filters/scroll**; unit tests.
- [ ] **Step 4: Commit** `feat(app): add read-only trade overlay`.

---

### Task 3: Native/service wiring

**Files:**
- Modify: `src/script/include/gl/script/native_registry.hpp`
- Modify: `src/script/src/native_registry.cpp`
- Modify: `src/game/include/gl/game/game_script_host.hpp` if needed
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/script/test_native_registry.cpp`
- Create: `tests/script/test_trade_native.cpp`

- [ ] **Step 1: `NativeServices::trade_panel` + handler 0x700000D**
(always invoked? callback; return 0.0; empty → log+0).
- [ ] **Step 2: Registry supported 16/65; tests.**
- [ ] **Step 3: glgame**: callback closes dialog and opens overlay with
  speaker's `trade_items`/`trade_money` and hero main items/`hero_money`;
  input blocking; Esc/exit.
- [ ] **Step 4: Smoke/commit** `feat(tools): open trade overlay from scripts`.

---

### Task 4: Real trader E2E + snapshot

**Files:**
- Modify: `tests/app/test_glgame_screenshot.cpp`
- Create: real test if applicable

- [ ] **Step 1: Real trader dialog** from research → overlay visible,
  expected NPC trade items/`npc_money` and hero items/`hero_money`
  (slot0/slot2; counts/money + first 12 hero main).
- [ ] **Step 2: Screenshot 1024×768** with real assets.
- [ ] **Step 3: `glgame --level l1_1 --click-npc 13`** no regression.
- [ ] **Step 4: Commit** `test(trade): cover real trade overlay`.

---

### Task 5: Docs, parity, regression

**Files:**
- Modify: `docs/DECISIONS.md` (D-057)
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`

- [ ] **Step 1: D-057** — read-only trade overlay, NPC trade `entity+0xC`
  (pass 1) source, prices, two money, no purchase/sale/trade mutations.
- [ ] **Step 2: parity** — trade rows verified/partial; recompute counts.
- [ ] **Step 3: STATUS/PROJECT** — done, exact tests, next (1b2b4b buy/sell).
- [ ] **Step 4: Full ctest** with `GL_GAME_DIR`; encoding.
- [ ] **Step 5: Commit** `docs: record read-only trade overlay`.
