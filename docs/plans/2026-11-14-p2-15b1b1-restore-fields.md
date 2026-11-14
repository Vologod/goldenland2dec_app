# P2-15b1b1 — Restore person fields Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Применить saved type-2 persons к уровню: match/order/fields/HP/EP; SEF-only keep, type-2-only gated.

**Architecture:** `assets` парсит BT_TRIBE names; `game::SavePersonRecord` + `restore_save_persons`; `PersonInstance` saved HP/EP; `LevelSimulation::restore_person`; glgame применяет в `on_level_loaded` после NPC loot, до simulation create.

**Tech Stack:** C++20, CMake/Ninja, doctest.

## Global Constraints

- Containers/inventory — b1b2; NPC loot gate/order — b1b3; type-2-only synthesis gated; no save write.
- Match by name occurrence; matched order = BT_ENTS; SEF-only append unless gate.
- `route_cells` from SEF; if saved route name differs, keep SEF + log.
- Docs UTF-8 без BOM/NUL; no comments unless needed.
- Коммиты после approve; full ctest с `GL_GAME_DIR` в конце.

Toolchain:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

---

### Task 0: STATUS entry

**Files:** `docs/STATUS.md`

- [ ] **Step 1:** Insert after `Обновлено:`:

```markdown
## P2-15b1b1 — Restore person fields (in progress 2026-11-14)

- **Задача:** type-2 decoder + restore match/order/fields/HP/EP; BT_TRIBE
  names; glgame load hook; без containers/loot gate.
- **Spec:** `docs/specs/2026-11-14-p2-15b1b1-restore-fields-design.md`.
- **Plan:** `docs/plans/2026-11-14-p2-15b1b1-restore-fields.md`.
- **Критерии:** 496/43/3; person tuple 0/N; E2E; full ctest.
- **Статус:** `in progress`.
```

- [ ] **Step 2:** encoding; commit `docs(status): start restore person fields`.

---

### Task 1: BT_TRIBE names in `SavePayload`

**Files:** `src/assets/include/gl/assets/save_payload.hpp`,
`src/assets/src/save_payload.cpp`, `tests/assets/test_save_payload.cpp`

**Interfaces:**
- `SaveTribeEntry { u32 id; string name; }`, `SaveTribeTable { vector<SaveTribeEntry> entries; }`
  in `SavePayload.tribes`.

- [ ] **Step 1:** Test synthetic `BT_TRIBE` body (`u32 count`, per entry
  `u32 id + string`, `count*count` bytes) → `tribes.entries` id/name; malformed
  count/string/truncated → format.
- [ ] **Step 2:** RED.
- [ ] **Step 3:** Parse after `BT_VARS` in `parse_save_payload`; matrix bytes
  skipped/validated count².
- [ ] **Step 4:** GREEN; commit `feat(assets): parse save tribe names`.

---

### Task 2: `SavePersonRecord` decoder

**Files:**
- Create: `src/game/include/gl/game/save_person.hpp`
- Create: `src/game/src/save_person.cpp`
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_save_restore.cpp` (new; CMake)

**Interfaces:**
- `Result<SavePersonRecord> decode_save_person(const gl::assets::SaveEntity &entity);`

- [ ] **Step 1:** Test synthetic type-2 body with known values → fields.
  Malformed/truncated/wrong type → format.
- [ ] **Step 2:** RED.
- [ ] **Step 3:** Decoder reads type-2 prefix (offsets/order from research):
  name, +0x40, +0x44, current tile pair, +0x5f8, +0x5fc, +0x654, +0x658(0x20),
  skip 4, +0x151c, route string, +0x1520, +0x2668, +0x266c, +0x150c,
  dialog string, inventory string, +0x6cc, raw 0x11c, skip 4, HP, EP, AP,
  route index; stop before containers. Require reader bounds.
- [ ] **Step 4:** GREEN; commit `feat(game): decode saved person fields`.

---

### Task 3: Restore + runtime

**Files:**
- Create: `src/game/include/gl/game/save_restore.hpp`,
  `src/game/src/save_restore.cpp`
- Modify: `src/game/include/gl/game/person.hpp` (saved_life/energy optional)
- Modify: `src/game/src/person_runtime.cpp` (use saved values)
- Modify: `src/game/include/gl/game/level_simulation.hpp`,
  `src/game/src/level_simulation.cpp` (`restore_person`)
- Test: `tests/game/test_save_restore.cpp`

**Interfaces:**
- `RestoreResult restore_save_persons(Level&, const SaveTailEntry&,
  const SaveTribeTable*, bool drop_sef_only=false)` where `RestoreResult`
  carries `RestoreCounts` plus `RestoredPerson { level_index, current,
  direction, route_index }` for post-create runtime restore.
- `LevelSimulation::restore_person(index, Tile current, uint8 direction,
  int32 route_index);`

- [ ] **Step 1:** Synthetic Level 3 SEF + tail 2 matched + 1 type2-only:
  assert matched order/fields/SEF-only append/skip counts; duplicate name;
  drop gate variant; route-name mismatch keeps SEF cells + log; saved order
  different from SEF order; `RestoreResult` runtime fields.
- [ ] **Step 2:** RED.
- [ ] **Step 3:** Implement match queues, field apply (tribe id→name;
  keep SEF cells if route differs), reorder `level.persons`; optional
  saved_life/energy; PersonRuntime uses them (log HP/EP above maximum);
  return per-matched `RestoredPerson` runtime data; `restore_person` sets
  x/y/direction/route_index and clears path/goal/wait.
- [ ] **Step 4:** GREEN; full `gl_game_tests`; commit
  `feat(game): restore saved person fields`.

---

### Task 4: glgame hook + E2E

**Files:** `tools/glgame/main.cpp`, `tests/app/test_glgame_screenshot.cpp`

- [ ] **Step 1:** `load_save` stores pending tail pointer; `on_level_loaded`
  runs: generate NPC loot → `restore_save_persons` → return; keep the
  `RestoreResult`, log `save restore: matched=... sef_only=... type2_only=...`,
  then after `LevelSimulation::create` call
  `restore_person(level_index, current, direction, route_index)` for each
  `RestoreResult.persons` entry.
- [ ] **Step 2:** E2E `--load-save 0` asserts per-slot log counts (3/0/0) and
  both restored Guardian2 fields via `--dump-npc`/log; no change to loot
  assertions.
- [ ] **Step 3:** GREEN; commit `feat(tools): restore save persons on load`.

---

### Task 5: Real oracle + docs + ctest

**Files:** `tests/game/test_save_restore_real.cpp` (new; CMake),
docs/spec/plan/STATUS/PROJECT/parity/DECISIONS.

- [ ] **Step 1:** Real slot0/2/21: restore each selected section; assert the
  28-save corpus aggregate 496/43/3 plus per-slot counts, pinned person fields
  from research; 0/N; no containers.
- [ ] **Step 2:** full ctest with GL_GAME_DIR; D-072; docs; encoding.
- [ ] **Step 3:** commit `test(game): cover real save restore` +
  `docs: record restore person fields`.

---

## Self-review notes

- Spec §4/§5 → Tasks 1–4; acceptance §7 → Task 5.
- No container/loot scope; type-2-only and SEF-only gates explicit.
- HP/EP optional fields default nullopt for New Game.
