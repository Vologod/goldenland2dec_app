# Phase 1b2b2b — core NPC `.inv` loot (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Реализовать парсер `.inv`, генератор базового NPC-лута и NPC-контейнеры; specials — вне scope (documented RNG-desync).

**Architecture:** `gl_assets::InvTable`; `gl_game::NpcLootGenerator`; `InventoryRuntime` с Hero+NPC; генерация при загрузке уровня.

**Tech Stack:** C++20, CMake + Ninja, doctest.

## Global Constraints

- Server.dll SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Поведение — из research `p3b2b2-npc-loot-server.md` §3–§8; без догадок.
- Слои вниз; `gl_game` без SDL; docs UTF-8 без BOM/NUL.
- RNG — D-053 (`MsvcRandom`, truncation/float), production seed/`--seed`.
- Коммиты разрешены.

---

### Task 1: Research gate — level source and generation prerequisites

**Files:**
- Modify: `docs/research/p3b2b2-npc-loot-server.md`
- Modify: `docs/STATUS.md` (verdict/blockers)

- [ ] **Step 1: `+0x26c4`**

Prove what BT_INFO field `+0x26c4` is (`FUN_1403210c`, `decompiled.c:35638`): save layout/offset, semantics (player level? world level?), and how to obtain it from `Saves/*.sav` read-only. If not provable, verdict `blocked` and stop Tasks 2–5.

- [ ] **Step 2: Prototype/merge/early exit**

Confirm from ITM: max_stack/type/flags/style; merge-path `+1` behavior; early-exit `>=4` stacks and restock metadata; initial R1 `coin(regenerate_chance)`.

- [ ] **Step 3: RNG order**

Record the order of RNG consumers before/at level spawn (movement init vs loot generation) as far as provable; if not, state the deterministic port order and the documented divergence.

- [ ] **Step 4: Verdict/commit**

State `implementable` or `blocked`; commit `docs(research): pin NPC loot level and order`.

---

### Task 2: `gl_assets::parse_inv` / `load_inv`

**Files:**
- Create: `src/assets/include/gl/assets/inv_table.hpp`
- Create: `src/assets/src/inv_table.cpp`
- Modify: root `CMakeLists.txt` (`gl_assets`)
- Create: `tests/assets/test_inv_table.cpp`
- Modify: `tests/assets/CMakeLists.txt`

- [ ] **Step 1: Failing tests**

Synthetic: header fields, 5-number item, 4-number skip, comments, quotes,
`special` ignore, malformed header. Real: all 238 `.inv` load; counts match
research (e.g. `inv001_swordsman.inv`).

- [ ] **Step 2: Implement** `parse_inv`/`load_inv` (Sdb not needed; raw text).
4-number `item` → skip + one warning per file. `special` → ignore.

- [ ] **Step 3: Run `gl_assets_tests` + targeted real; commit**
`feat(assets): parse .inv loot tables`.

---

### Task 3: `gl::game::NpcLootGenerator`

**Files:**
- Create: `src/game/include/gl/game/npc_loot.hpp`
- Create: `src/game/src/npc_loot.cpp`
- Modify: `src/game/CMakeLists.txt` (or root if that is the actual list)
- Create: `tests/game/test_npc_loot.cpp`
- Modify: `tests/game/CMakeLists.txt`

- [ ] **Step 1: Failing tests (reference values)**

Use a fixed seed/level and an `InvTable` built synthetically; hardcode
expected items/counts derived from the research pseudocode (not from the
implementation). Cover: window filtering, chance miss, qty range, split by
max_stack, merge, early exit at 4 stacks, unknown ITM, empty table,
R1 consumption.

- [ ] **Step 2: Implement** the generator per research; specials not drawn;
RNG calls only R1/R2/R3 (document the divergence).

- [ ] **Step 3: Run `gl_game_tests --test-case=*npc_loot*`; commit**
`feat(game): generate core NPC loot`.

---

### Task 4: NPC containers in `InventoryRuntime` + level-load generation

**Files:**
- Modify: `src/game/include/gl/game/inventory_runtime.hpp`
- Modify: `src/game/src/inventory_runtime.cpp`
- Modify: `src/app/src/level_screen.cpp` (or the actual level-load path)
- Modify: `tools/glgame/main.cpp` if runtime construction lives there

- [ ] **Step 1: Extend runtime**

Add per-person containers (`Hero` + NPC names). Hero from save; NPC from
`NpcLootGenerator`. `count`/`money` use the person's container; `Hero`
behavior unchanged.

- [ ] **Step 2: Generate at level load**

For each `PersonInstance` with non-empty `inventory`, in SEF order:
load `scripts/inventory/<name>.inv`, generate with the shared
`MsvcRandom` (production seed/`--seed`) and level from Task 1; store in
runtime. Keep a stable RNG source with `LevelSimulation` (do not reseed
per NPC).

- [ ] **Step 3: Tests**

Real `l1_1`: at least one NPC has non-empty container; counts match the
Task 1 oracle; Hero still from save. Smoke `glgame --level l1_1
--click-npc 13 --screenshot` exit 0.

- [ ] **Step 4: Commit** `feat(game): generate NPC inventories on level load`.

---

### Task 5: Docs, parity, full regression

**Files:**
- Modify: `docs/DECISIONS.md` (D-055)
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`

- [ ] **Step 1: D-055** — core loot implemented; specials skipped
(RNG-desync documented); level source; initial spawn only; NPC add/remove
still no-op; restock/trade out.

- [ ] **Step 2: parity-matrix** — NPC loot rows: core verified/partial,
specials blocked; recompute counts from the actual table.

- [ ] **Step 3: STATUS/PROJECT** — done section, exact test output, next.

- [ ] **Step 4: Full ctest** with `GL_GAME_DIR`; record counts/times/exit.

- [ ] **Step 5: Encoding check; commit** `docs: record core NPC loot`.
