# Phase 1b2b1 — Hero inventory mutations (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Реализовать `RS_PersonAddItem` и `RS_PersonRemoveItem` для Hero на mutable `InventoryRuntime` с семантикой `Server.dll`.

**Architecture:** `InventoryRuntime` становится mutable-контейнером Hero; `InventoryAccess` получает add/remove callbacks; handlers в `gl_script`; overlay читает live inventory.

**Tech Stack:** C++20, CMake + Ninja, doctest, Ghidra/llvm-objdump (research), PowerShell 5.1.

## Global Constraints

- `Server.dll` SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Поведение — только из `Server.dll` (D-029); без догадок.
- Слои только вниз: `game → script → assets`; `gl_script` без `gl_game`/SDL.
- Документы UTF-8 без BOM, `nul=0`.
- Коммиты по задачам разрешены (2026-11-06).
- Перед каждым шагом сверять `Get-Date`; в 21:00–05:00 — checkpoint.

---

### Task 1: Research add/remove semantics

**Files:**
- Create: `docs/research/p3b2b1-inventory-mutations-server.md`

- [x] **Step 1: Natives**

Record `0x5000004`/`0x5000005` arity 3, types `3,3,0`, return `0.0`,
`count != 0` guard, container resolution and `FUN_140482e8`.

- [x] **Step 2: Add semantics**

Read `FUN_1404cd28` and record: stacking by item id, count addition, max
count, new record type/flags/style, behavior for unknown id, zero/negative
count.

- [x] **Step 3: Remove semantics**

Read `FUN_1404cae8` and record: partial removal, full removal, count >
available, missing item, zero/negative count, record deletion.

- [x] **Step 4: State/notify**

Record container `+0x18` meaning and `FUN_140482e8`; decide what the port
must model (runtime state only) and what is safely omitted.

- [x] **Step 5: Real oracle**

For slot0/slot2 record: add `MON_1_0_1` (e.g. +100), remove `MON_1_0_1`
(e.g. −100), add/remove a non-money item; expected counts/money.

- [x] **Step 6: Real dialog**

Find a real `.cs` with Add/Remove and an observable result (count/money/
branch); record path, node ids, expected values for slot0/slot2.

- [x] **Step 7: Verify and commit**

UTF-8 no BOM/nul=0. Commit `docs(research): record inventory mutation natives`.

---

### Task 2: Mutable `InventoryRuntime`

**Files:**
- Modify: `src/game/include/gl/game/inventory_runtime.hpp`
- Modify: `src/game/src/inventory_runtime.cpp`
- Modify: `tests/game/test_inventory_runtime.cpp`
- Modify: `tests/game/test_inventory_runtime_real.cpp`

**Interfaces:**
```cpp
struct ItemTemplate {
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t style = 0;
    std::uint32_t max_stack = 0;
};
static InventoryRuntime create(
    const gl::assets::HeroInventory& hero,
    const gl::assets::ItemNameIndex& names,
    std::function<std::optional<ItemTemplate>(std::uint32_t)> item_template = {});
bool add_item(std::string_view person, std::string_view tech, std::int32_t count);
bool remove_item(std::string_view person, std::string_view tech, std::int32_t count);
const gl::assets::HeroInventory& hero_inventory() const;
```

`item_template` must supply `type`, `flags`, `style`, `max_stack` from
ITM/ItemCatalog; a missing template or `max_stack` behavior follows
research. Negative count is a no-op (D-052).

- [x] **Step 1: Failing tests**

Add synthetic cases for new item, stacking, money add, partial/full remove,
count > available, count 0/negative, unknown tech/person, and
`hero_inventory()` reflecting mutations.

- [x] **Step 2: Implement mutable vector**

Store `std::vector<InventoryItem> hero_items_`; add/remove per research;
recompute `count`/`money` from the vector (or maintain consistently);
type from `item_type` callback (default 0); money type 18.

- [x] **Step 3: Update real tests**

Extend `test_inventory_runtime_real.cpp` with add/remove `MON_1_0_1` and a
non-money item for slot0/slot2, asserting the research oracle.

- [x] **Step 4: Run `gl_game_tests`**; report counts.

- [x] **Step 5: Commit** `feat(game): make hero inventory mutable`.

---

### Task 3: `gl_script` add/remove handlers

**Files:**
- Modify: `src/script/include/gl/script/native_registry.hpp`
- Modify: `src/script/src/native_registry.cpp`
- Create: `tests/script/test_inventory_mutations.cpp`
- Modify: `tests/script/CMakeLists.txt`
- Modify: `tests/script/test_native_registry.cpp`

**Interfaces:**
```cpp
struct InventoryAccess {
    // existing item_count, money
    std::function<void(std::string_view, std::string_view, std::int32_t)> add_item;
    std::function<void(std::string_view, std::string_view, std::int32_t)> remove_item;
};
```

- [x] **Step 1: Failing tests**

Handlers forward person/tech/count; return `double{0.0}`; wrong args warn;
empty callbacks no-op; registry arity 3/3 and supported 13/65.

- [x] **Step 2: Implement handlers + registry entries**

IDs `0x5000004`/`0x5000005`; type `3,3,0`; `as_int` for count.

- [x] **Step 3: Run `gl_script_tests`**; report counts.

- [x] **Step 4: Commit** `feat(script): implement inventory mutation natives`.

---

### Task 4: Wire mutations in `glgame`

**Files:**
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp` (optional overlay check)

- [x] **Step 1: Item type callback**

Pass `[this](std::uint32_t id) { auto* e = item_catalog_.find(id); return e ? e->item.type : 0; }`
to `InventoryRuntime::create` (use the existing catalog member; check its
name).

- [x] **Step 2: Wire services**

`context->host.services().inventory = inventory_runtime_.access();` now
includes add/remove.

- [x] **Step 3: Live overlay**

Make `open_inventory` use `inventory_runtime_.hero_inventory()` for the
Hero snapshot, so mutations are visible on the next open. Keep equipment
play-doll data unchanged.

- [x] **Step 4: Smoke**

`glgame --level l1_1 --click-npc 13 --screenshot` exit 0; inventory
overlay screenshot test still passes.

- [x] **Step 5: Commit** `feat(tools): wire inventory mutations into dialogs`.

---

### Task 5: Real dialog test

**Files:**
- Create: `tests/script/test_real_inventory_mutations.cpp`
- Modify: `tests/script/CMakeLists.txt`

- [x] **Step 1: Test**

Use the dialog from research Task 1. Seed runtime from slot0/slot2
`parse_hero_inventory`, run `ScriptVm → GameScriptHost → InventoryRuntime`,
and assert the mutated count/money/global matches the research oracle.

- [x] **Step 2: CLI smoke**

`glscript demon.d1.age.cs --run` unchanged; `glgame --level l1_1
--click-npc 13` exit 0.

- [x] **Step 3: Commit** `test(inventory): cover real mutation dialog`.

---

### Task 6: Regression, docs, status

**Files:**
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`, `docs/DECISIONS.md`.

- [x] **Step 1: Targeted tests**

`ctest -R "gl_script_tests|gl_script_real_tests|gl_game_tests|gl_assets_tests|gl_assets_real_tests|glgame_screenshot_tests"`.

- [x] **Step 2: Full ctest** with `GL_GAME_DIR`; record counts/times/exit.

- [x] **Step 3: Docs**

- `P3-9`: supported `11 → 13`; list IDs 0x5000004/0x5000005; note Hero-only.
- `STATUS`: completed 1b2b1 section.
- `PROJECT`: P3/P5 row update.
- `DECISIONS D-052`: mutable runtime, add/remove semantics (flags/style/
  max_stack from ITM, negative count → no-op), money via item, overlay live
  view, NPC/box no-op, no save writes.

- [x] **Step 4: Encoding check**.

- [x] **Step 5: Commit** `docs: record Phase 1b2b1 inventory mutations`.
