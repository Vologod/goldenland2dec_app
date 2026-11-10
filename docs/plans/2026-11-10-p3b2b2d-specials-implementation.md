# Phase 1b2b2d — NPC loot specials implementation (plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Реализовать specials NPC-лута (`specials.scr`, `FUN_14047284`, R4–R10) с обязательными live-гейтами до full parity.

**Architecture:** `gl_assets::SpecialDatabase`; `gl_game::SpecialEngine`; интеграция в `NpcLootGenerator`; `InventoryItem.mods`.

**Tech Stack:** C++20, CMake + Ninja, doctest, Python reference.

## Global Constraints

- Server.dll SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Поведение — из research `p3b2b2c`, без догадок.
- Слои вниз; gl_game без SDL; docs UTF-8 без BOM/NUL; коммиты разрешены.
- Full parity только после live FPU-CW + NPC-mod oracle; иначе `partial`.

---

### Task 1: `specials.scr` parser + enums

**Files:**
- Create: `src/assets/include/gl/assets/specials.hpp`
- Create: `src/assets/src/specials.cpp`
- Modify: root `CMakeLists.txt` (gl_assets)
- Create: `tests/assets/test_specials.cpp`
- Modify: `tests/assets/CMakeLists.txt`

- [x] **Step 1: Failing tests** — 58 `special_desc`, 9 slots, 15
  itemclasses, pool sizes §2.4, enum maps, malformed.
- [x] **Step 2: Implement** cp1251 parser + hardcoded enum maps.
- [x] **Step 3: Run `gl_assets_tests` + real; commit**
  `feat(assets): parse specials.scr`.

---

### Task 2: SpecialEngine (R4–R10)

**Files:**
- Create: `src/game/include/gl/game/special_engine.hpp`
- Create: `src/game/src/special_engine.cpp`
- Modify: `src/assets/include/gl/assets/hero_equipment.hpp` (`SpecialMod`, `mods`)
- Modify: `src/game/CMakeLists.txt`
- Create: `tests/game/test_special_engine.cpp`

- [x] **Step 1: Failing vectors** — pool ascending/nspec; R4/R5 limits;
  R6 bounds; R7 MAX_HEALTH/ENERGY; R8 n2=1; R9/R10; flags/0x200;
  fixed-seed values.
- [x] **Step 2: Implement** `random_special_count` + `apply`.
- [x] **Step 3: Run `gl_game_tests`; commit**
  `feat(game): implement specials engine`.

---

### Task 3: NpcLootGenerator integration

**Files:**
- Modify: `src/game/src/npc_loot.cpp`
- Modify: `tests/game/test_npc_loot.cpp`
- Modify: `tests/game/test_inventory_runtime_real.cpp` (oracle modes)

- [x] **Step 1: Integrate** — chunk==1, flags&4, mod_count==0;
  R4/R5 before R6–R10; mods/flags/0x200; per-person nesting.
- [x] **Step 2: Deterministic oracle** — 18/9/12, 1329 draws,
  `0x48CAA370`; first divergence; `Kolbasens` regression.
- [x] **Step 3: no-specials mode** — keep 174/218/323.
- [x] **Step 4: Run; commit** `feat(game): generate NPC specials`.

---

### Task 4: Live gates (FPU-CW + NPC-mod oracle)

**Files:**
- Create: `tests/game/test_special_gates.cpp` (or `tools/` capture hook)
- Modify: `docs/STATUS.md` (gate results)

- [x] **Step 1: FPU-CW** — capture before `FUN_14047284` and
  `FUN_1206E8D0`; expect `0x027F`; record test/hook.
- [x] **Step 2: NPC-mod oracle** — user-assisted live capture of
  `+0x10` mods for a level; 0/N compare with port.
- [x] **Step 3: Verdict** — pass → full parity; fail/unavailable →
  `partial` + Open Questions, no threshold weakening.
- [x] **Step 4: Commit** `test(game): add specials live gates`.

---

### Task 5: Trade/price integration + docs

**Files:**
- Modify: `src/game/inventory_runtime.cpp` (mod guard already)
- Modify: `docs/DECISIONS.md` (D-060)
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`

- [x] **Step 1: Mod guard** — items with generated mods remain
  `unsupported` for price/trade; add test.
- [x] **Step 2: D-060** — implementation, DB/algorithm, live gates,
  remaining unverified (random_sign/time-slots/flag0x40).
- [x] **Step 3: parity/STATUS/PROJECT** — specials implemented/partial;
  recompute counts.
- [x] **Step 4: Full ctest** with `GL_GAME_DIR`; encoding.
- [x] **Step 5: Commit** `docs: record specials implementation`.
