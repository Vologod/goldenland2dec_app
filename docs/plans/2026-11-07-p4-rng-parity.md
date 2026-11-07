# P4 — RNG parity (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Привести `MsvcRandom` к `Server.dll` (truncation + float coin) и clock-based production seed.

**Architecture:** `gl_game` меняет `MsvcRandom`; `LevelScreen` передаёт `production_seed()`; тесты фиксируют seed и эталонную формулу.

**Tech Stack:** C++20, CMake + Ninja, doctest.

## Global Constraints

- Server.dll SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Поведение — из research `p3b2b2-npc-loot-server.md` §9 и D-053; без догадок.
- Слои только вниз; `gl_game` без SDL.
- Документы UTF-8 без BOM/NUL; коммиты разрешены.

---

### Task 1: `MsvcRandom` formulas + production seed

**Files:**
- Modify: `src/game/include/gl/game/msvc_random.hpp`
- Modify: `src/game/src/msvc_random.cpp`
- Modify: `tests/game/test_msvc_random.cpp`

**Interfaces:**
- `std::int32_t range(std::int32_t low, std::int32_t high) noexcept;`
- `bool coin(double probability) noexcept;`
- `static std::uint32_t production_seed() noexcept;`

- [ ] **Step 1: Failing tests**

Add tests:
```cpp
TEST_CASE("range truncates like Server.dll") {
    gl::game::MsvcRandom rng(1);
    // Reference formula with truncation, fixed rand sequence.
    // e.g. range(10,25) expected values for the first N draws.
}
TEST_CASE("coin uses float arithmetic") {
    gl::game::MsvcRandom rng(1);
    CHECK_FALSE(rng.coin(0.0));
    CHECK(rng.coin(1.5));
    // p == 1.0 degenerate case with rand()==32767
}
TEST_CASE("production seed is available") {
    CHECK(gl::game::MsvcRandom::production_seed() != 0xFFFFFFFFu);
}
```

Compute expected values with the exact formula, not `nearbyint`.

- [ ] **Step 2: Implement**

`range`:
```cpp
if (high <= low) return low;
const double span = static_cast<double>(high - low) + 0.99999;
const double scaled = span * static_cast<double>(next()) * (1.0 / 32767.0);
return low + static_cast<std::int32_t>(scaled);
```
`coin`:
```cpp
const float p = static_cast<float>(probability);
if (p <= 0.0f) return false;
if (p > 1.0f) return true;
return static_cast<float>(next()) * (1.0f / 32767.0f) < p;
```
`production_seed`:
```cpp
const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
return static_cast<std::uint32_t>(ms);
```

- [ ] **Step 3: Run `gl_game_tests --test-case=*msvc_random*`**.

- [ ] **Step 4: Commit** `feat(game): match Server.dll RNG formulas`.

---

### Task 2: Update movement/STAY_ROTATE expectations

**Files:**
- Modify: `tests/game/test_level_simulation.cpp`
- Modify: `tests/game/test_person_movement.cpp` (if exists/affected)
- Modify: `tests/game/test_stability.cpp` (if affected)

- [ ] **Step 1: Run affected tests**

`build\bin\gl_game_tests.exe --test-case=*movement*` and the full binary.
Record failures caused by truncation.

- [ ] **Step 2: Recompute expected values**

For every changed expectation, recompute with the reference formula
(`lo + trunc(((hi-lo)+0.99999)*rand/32767)`) and the same seed; do not
loosen assertions or skip cases. If a test's seed/stream order must change,
document why.

- [ ] **Step 3: Run full `gl_game_tests`**; report counts.

- [ ] **Step 4: Commit** `test(game): update RNG expectations for truncation`.

---

### Task 3: Production seed wiring

**Files:**
- Modify: `src/app/src/level_screen.cpp`
- Modify: `src/app/include/gl/app/level_screen.hpp`
- Modify: `tools/glgame/main.cpp`
- Modify: `tools/glview/main.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp`
- Modify: `tests/render/test_glview_screenshot.cpp`
- Modify: `tests/app/test_level_click.cpp`

- [ ] **Step 1: Replace seed 1**

`LevelScreen::load(..., std::optional<std::uint64_t> rng_seed = std::nullopt)`,
create with `rng_seed.value_or(MsvcRandom::production_seed())`. Production
without a seed stays clock-based.

- [ ] **Step 2: Deterministic test hook**

Add `--seed <n>` to `glgame` and `glview`; parse and pass to
`LevelScreen::load`. Test harnesses that compare screenshots/behavior across
independent processes pass `--seed 1` (or an explicit seed for direct
`LevelScreen::load`). `gl_game` tests keep their explicit seeds.

- [ ] **Step 3: Smoke**

`glgame --level l1_1 --click-npc 13 --screenshot` and `glview` smoke exit 0.

- [ ] **Step 4: Commit** `feat(app): seed NPC RNG from clock`.

---

### Task 4: Docs, parity, regression

**Files:**
- Modify: `docs/DECISIONS.md` (D-054 or amend D-053)
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`

- [ ] **Step 1: Targeted + full ctest** with `GL_GAME_DIR`; record counts/times/exit.

- [ ] **Step 2: D-054**

D-054 was opened in Task 2 for the port-side anti-livelock guard
(occupancy-abort resample + backoff; **not parity**). Extend that same entry:
truncation/float coin implemented; production seed = steady_clock ms; remaining
limitations: original `timeGetTime` non-deterministic, RNG stream not continuous
across levels, NPC loot specials still blocked; keep the anti-livelock guard
explicitly marked as a port robustness deviation.

- [ ] **Step 3: parity-matrix**

Update RNG rows (P4b-3/P4b-7/P4b-9/P4b-15): formula/seed status and
remaining NPC-loot blockers; keep D-053 reference.

- [ ] **Step 4: STATUS/PROJECT**

Record task, exact test output, next step (1b2b2b or Trade).

- [ ] **Step 5: Encoding check** (`bom=False nul=False`).

- [ ] **Step 6: Commit** `docs: record RNG parity fix`.
