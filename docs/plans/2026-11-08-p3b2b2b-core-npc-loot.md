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

> **Revision 2026-11-08 (Task 1 fix):** реальный SEF-путь NPC —
> `FUN_1404b1c0` (два прохода), не `FUN_140489ec`. Task 2 можно
> начинать сразу; Tasks 3–4 переписаны ниже и стартуют после этой
> ревизии spec/plan. Детали — research `p3b2b2-npc-loot-server.md` §15.

### Task 1: Research gate — level source and generation prerequisites

**Status:** done 2026-11-08 (commit `637f184`); findings revised by
`docs: revise NPC loot path to FUN_1404b1c0`.

**Files:**
- Modify: `docs/research/p3b2b2-npc-loot-server.md`
- Modify: `docs/STATUS.md` (verdict/blockers)

- [x] **Step 1: `+0x26c4`**

Prove what BT_INFO field `+0x26c4` is (`FUN_1403210c`, `decompiled.c:35638`): save layout/offset, semantics (player level? world level?), and how to obtain it from `Saves/*.sav` read-only. If not provable, verdict `blocked` and stop Tasks 2–5.

- [x] **Step 2: Prototype/merge/early exit**

Confirm from ITM: max_stack/type/flags/style; merge-path `+1` behavior; early-exit `>=4` stacks and restock metadata; initial R1 `coin(regenerate_chance)`.

- [x] **Step 3: RNG order**

Record the order of RNG consumers before/at level spawn (movement init vs loot generation) as far as provable; if not, state the deterministic port order and the documented divergence.

- [x] **Step 4: Verdict/commit**

State `implementable` or `blocked`; commit `docs(research): pin NPC loot level and order`.

---

### Task 2: `gl_assets::parse_inv` / `load_inv`

**Status:** may proceed now (independent of the generator path).

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

**Status:** starts after this spec/plan revision.

**Files:**
- Create: `src/game/include/gl/game/npc_loot.hpp`
- Create: `src/game/src/npc_loot.cpp`
- Modify: `src/game/CMakeLists.txt` (or root if that is the actual list)
- Create: `tests/game/test_npc_loot.cpp`
- Modify: `tests/game/CMakeLists.txt`

- [ ] **Step 1: Failing tests (reference values)**

Fixed seed/level, synthetic `InvTable`; hardcode expected items/counts
from research §15.3 (not from the implementation). Cover:
- R1 `coin(regenerate_chance)` consumed always;
- pass 1: window, `coin(0.01*n3)`, `range(n4,n5)`, split by
  `max_stack` (`FUN_1404e3a4(name,0,1)`, `this+4=1` only clears item
  flag `0x80`, direct `count=min(remaining,max)`); merge possible via
  shared helper (`unverified`); **no 4-stack exit** (`100000`
  threshold effectively unreachable);
- pass 2: `n3==0` → no chance coin; `n3>0` → `coin((0.01*n3)/
  chance_decrease)`; qty `n4/n5 * (1/quantity_decrease)` with
  trunc/`+1` → `range`; creation with `this+4=0`;
- two-pass RNG order and `NpcLoot{hot, main}`;
- unknown ITM, empty table.
- Merge `+1` is in shared `FUN_1404e3a4`; pass-1/2 merge is
  `unverified`. Only caller quirks (`qty -= chunk`, 4-stack exit) are
  scoped to `FUN_140489ec` (separate helper only if ever added).
- Division by `chance_decrease == 0` / `quantity_decrease == 0` is
  unguarded; real corpus has no zeros; behavior at 0 is
  `unverified`/D-055.

- [ ] **Step 2: Implement** `FUN_1404b1c0` initial spawn (two passes):
- R1; SEF call uses `param_4=1`, so generation always proceeds;
- pass 1 → `hot` (`+0xc`): creation + direct count set; merge
  possible/`unverified`; `this+4=1` only clears item flag `0x80`;
- pass 2 → `main` (`+0x10`): scaled chance/qty + creation; merge
  possible/`unverified`; division-by-0 unverified/D-055;
- specials R4/R5 not drawn; RNG desync documented (D-055, Task 5).

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

Add per-person NPC containers (`Hero` + NPC names). Hero from save.
For NPC store the **main** container (`+0x10`, pass 2) — it is what
`TestPersonHasItem`/`GetItemCountI` read; store `hot` (`+0xc`, pass 1)
too if the runtime supports two containers, otherwise discard hot after
consuming its RNG draws (documented divergence). `count`/`money` use the
person's main container; `Hero` behavior unchanged.

- [ ] **Step 2: Generate at level load**

For each `PersonInstance` with non-empty `inventory`, in SEF order:
load `scripts/inventory/<name>.inv`; generate both passes of
`FUN_1404b1c0` from one shared `MsvcRandom` (production seed/`--seed`)
with `loot_level = party_level + 1` (party level from Task 1: BT_INFO
`+0x26c4` if a save is the source, else runtime/0); store `main`
(and `hot` if supported). Run loot **before** `LevelSimulation` and pass
the same advanced RNG state to it (do not reseed per NPC).

- [ ] **Step 3: Tests**

Real `l1_1`: at least one NPC main container is non-empty; counts match
the revised Task 1 oracle (pass 2 / main); `TestPersonHasItem`/
`GetItemCountI` see NPC items; Hero still from save. Smoke
`glgame --level l1_1 --click-npc 13 --screenshot` exit 0.

- [ ] **Step 4: Commit** `feat(game): generate NPC inventories on level load`.

---

### Task 5: Docs, parity, full regression

**Files:**
- Modify: `docs/DECISIONS.md` (D-055)
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`

- [ ] **Step 1: D-055** — core loot implemented via `FUN_1404b1c0`
(two passes; main `+0x10`, hot `+0xc` policy); specials skipped
(RNG-desync documented); level source `+0x26c4 + 1`; initial spawn only;
no 4-stack exit; NPC add/remove still no-op; restock/trade out.

- [ ] **Step 2: parity-matrix** — NPC loot rows: core verified/partial,
specials blocked; recompute counts from the actual table.

- [ ] **Step 3: STATUS/PROJECT** — done section, exact test output, next.

- [ ] **Step 4: Full ctest** with `GL_GAME_DIR`; record counts/times/exit.

- [ ] **Step 5: Encoding check; commit** `docs: record core NPC loot`.
