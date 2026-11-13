# P2-9 — 35 Hz tick implementation Plan (combined P2-9a/P2-9b)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Перевести `ServerCore` и `LevelSimulation` на оригинальную модель: 28 ms gate, один тик за host-кадр, один шаг симуляции на тик; обновить оракулы.

**Architecture:** `core::ServerCore` — integer-ms clock + 28 ms gate без catch-up/clamp; `game::LevelSimulation::advance` — ровно один шаг с фактическим delta; `glgame`/`glview` headless — declared 16 ms frame schedule; все ожидания пересчитываются аналитически.

**Tech Stack:** C++20, CMake/Ninja, doctest, SDL2 (только app/tools).

## Global Constraints

- Fixed defaults: `sv_fps=35`, `sv_game_speed=0`; config/console plumbing — вне scope.
- Gate: `tick_interval_ms(35)=28`; максимум один тик за `tick()`; без accumulator/catch-up/clamp; `simulation_time_ms` = latch clock (не `tick_index*28`).
- `LevelSimulation::advance(dt)` — один шаг; `time_ms_ += llround(dt*1000)`; движение `speed * delta_ms/1000`.
- Оракулы только аналитически/независимо (A/R/L); никаких «подогнать под порт».
- Pause: port choice «no backlog» (clock advances, `last_ms=clock`), документируется; оригинальная pause-семантика unverified.
- Документы UTF-8 без BOM/NUL; код без лишних комментариев.
- Коммиты после approve; полный `ctest` с `GL_GAME_DIR` в конце.

Toolchain:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

---

### Task 0: STATUS entry

**Files:** `docs/STATUS.md`

- [ ] **Step 1:** Insert after `Обновлено: 2026-11-13` (или текущая дата раздела):

```markdown
## P2-9a/b — 35 Hz tick implementation (in progress 2026-11-13)

- **Задача:** ServerCore 28 ms gate без catch-up/clamp; LevelSimulation один
  шаг на тик; headless 16 ms schedule; oracle refresh.
- **Spec:** `docs/specs/2026-11-13-p2-9-35hz-implementation-design.md`.
- **Plan:** `docs/plans/2026-11-13-p2-9-35hz-implementation.md`.
- **Критерии:** gate/step semantics; оракулы A/R/L; полный `ctest`.
- **Статус:** `in progress`.
```

- [ ] **Step 2:** encoding check; commit `docs(status): start 35hz implementation`.

---

### Task 1: `ServerCore` 28 ms gate

**Files:**
- Modify: `src/core/include/gl/core/server.hpp`
- Modify: `src/core/src/server.cpp`
- Test: `tests/core/test_server.cpp`

**Interfaces:**
- Consumes: ничего.
- Produces:
  `kDefaultSvFps=35`, `kDefaultSvGameSpeed=0`, `kTickIntervalMs=28`,
  `kNominalTickHz=35`, `std::uint32_t tick_interval_ms(std::int32_t sv_fps)`,
  `ServerCore::clock_ms()`.

- [ ] **Step 1: Update tests first**

In `tests/core/test_server.cpp` replace 60 Hz expectations with the gate:
- `tick_interval_ms(35)==28`, `(0)==0`, `(-1)==0`, `(1000)==1`, `(1001)==0`, `(60)==16`.
- `tick(0.016)` → 0 ticks; next `tick(0.016)` → 1 tick, `simulation_time_ms()==32`.
- `tick(0.028)` → 1 tick, `simulation_time_ms()==28`.
- `tick(0.250)` → 1 tick (no catch-up), `simulation_time_ms()==250`.
- two `tick(0.028)` → 2 ticks.
- `tick(1.0)` → 1 tick, `simulation_time_ms()==1000`.
- pause: `tick(0.016)` while paused no tick; `last_ms` advances; after `gv_pause 0`, first 16 ms no tick, second ticks (waits 28 ms).
- existing command tests: use `tick(0.016)`; commands still apply without a tick.
- remove `kFixedDt`/`kTickHz` uses; use `kTickIntervalMs`/`kNominalTickHz`.

- [ ] **Step 2: RED** — build; expected failures.

- [ ] **Step 3: Implement header**

Replace constants:
```cpp
inline constexpr std::int32_t kDefaultSvFps = 35;
inline constexpr std::int32_t kDefaultSvGameSpeed = 0;
inline constexpr std::uint32_t kTickIntervalMs = 28;
inline constexpr std::int32_t kNominalTickHz = 35;

std::uint32_t tick_interval_ms(std::int32_t sv_fps);
```
Remove `kFixedDt`, `kMaxFrameDt`, `kMaxStepsPerTick`. `ServerCore` fields:
`std::uint64_t clock_ms_=0; std::uint64_t last_ms_=0;` (remove `accumulator_`).
Add `std::uint64_t clock_ms() const`.

- [ ] **Step 4: Implement tick**

```cpp
void ServerCore::tick(double dt) {
    std::vector<Command> pending;
    pending.swap(pending_);
    for (const Command &command : pending)
        process(command);

    if (!std::isfinite(dt) || dt <= 0.0)
        dt = 0.0;
    clock_ms_ += static_cast<std::uint64_t>(std::llround(dt * 1000.0));

    if (paused_) {
        last_ms_ = clock_ms_;
        return;
    }

    const std::uint32_t interval = tick_interval_ms(kDefaultSvFps);
    if (interval == 0 || clock_ms_ - last_ms_ >= interval) {
        last_ms_ = clock_ms_;
        ++tick_index_;
        simulation_time_ms_ = clock_ms_;
    }
}
```

`tick_interval_ms`:
```cpp
std::uint32_t tick_interval_ms(std::int32_t sv_fps) {
    if (sv_fps <= 0 || sv_fps > 1000)
        return 0;
    return static_cast<std::uint32_t>(1000 / sv_fps);
}
```

- [ ] **Step 5: GREEN** — `gl_core_tests` focused/full; commit
  `feat(core): gate ticks at 28ms`.

---

### Task 2: `LevelSimulation` one step per call

**Files:**
- Modify: `src/game/include/gl/game/level_simulation.hpp`
- Modify: `src/game/src/level_simulation.cpp`
- Test: `tests/game/test_level_simulation.cpp`, `tests/game/test_stability.cpp`
  (and any other compile/test users)

**Interfaces:**
- `advance(double)` semantics: one step; `time_ms()` integer; no accumulator.
- Movement delta = `llround(dt*1000) / 1000`.

- [ ] **Step 1: Update test cadence**

Replace every `advance(1.0/60.0)` with `advance(0.028)`; replace
`60*10` loops with the equivalent 28 ms loop count; update exact positions:
- non-arrival displacement = old displacement × `0.028 / (1/60) = 1.68`;
- arrival/clamp cases use the research impact matrix (4 px arrival: snaps on
  tick 17, 476 ms; STAY_ROTATE: 56/112/…/280, checks at t=0..280);
- long waits: `advance(0.5)` is one step to 500 ms, not 30 steps (update
  expectations accordingly).
No test may simply assert the new port output.

- [ ] **Step 2: RED**.

- [ ] **Step 3: Implement**

- Remove `constexpr double kFixedStep` and `accumulator_`/`time_ms_exact_`.
- `advance`:
```cpp
void LevelSimulation::advance(double dt_seconds) {
    if (!std::isfinite(dt_seconds) || dt_seconds <= 0.0)
        return;
    const std::uint64_t delta_ms =
        static_cast<std::uint64_t>(std::llround(dt_seconds * 1000.0));
    if (delta_ms == 0)
        return;
    time_ms_ += delta_ms;
    // one pass of the existing per-tick body
    ...
    const float step = speed * (static_cast<float>(delta_ms) / 1000.0f);
    ...
}
```
- All existing timers use `time_ms_`; no loop.

- [ ] **Step 4: GREEN** — `gl_game_tests` focused; commit
  `feat(game): step simulation once per tick`.

---

### Task 3: headless loops + app/render oracles

**Files:**
- Modify: `tools/glgame/main.cpp` (remove `delta_ms > 250` clamp; headless
  `--sim-ms` schedule)
- Modify: `tools/glview/main.cpp` (16 ms frames already; verify)
- Test: `tests/app/test_glgame_screenshot.cpp`,
  `tests/app/test_level_click.cpp`, `tests/render/test_glview_screenshot.cpp`

**Interfaces:**
- Headless `--sim-ms`: 16 ms frames; glgame should render/tick each frame
  (or use a declared frame schedule) instead of one `render(0, sim_ms)` when
  simulation matters.
- glview already uses `(frame+1)*16`; keep and update expectations.

- [ ] **Step 1: glgame interactive**

Remove `if (delta_ms > 250) delta_ms = 250;`; pass actual delta to
`server_.tick`. Pause unchanged.

- [ ] **Step 2: glgame headless sim**

For `--sim-ms > 0` (screenshot/headless), step in declared 16 ms frames:
loop `frame_ms = 16` until `sim_ms`, calling `server_.tick(0.016)` and
`render(now, simulation_time_ms)`; final screenshot at `sim_ms` (or last
frame). Update tests' expectations to the 16 ms schedule:
20 s → 1250 frames, ticks every 32 ms → 625.

- [ ] **Step 3: Update app/render oracles**

Use A/R/L:
- `--list-saves`/save tests unaffected.
- click/dialog timing: update tick counts from schedule.
- glview NPC move 10 s: 16 ms grid → 625 frames/357 ticks? Use the research
  impact matrix row (F=16 → 312 if 10 s=10000/32; verify formula); recompute,
  do not copy output.
- screenshot `differing` thresholds: re-derive from declared schedule.

- [ ] **Step 4: GREEN** — `glgame_screenshot_tests`, `glview_screenshot_tests`,
  `level_click_tests`; commit `feat(tools): run headless sim on 16ms frames`.

---

### Task 4: full ctest + docs

**Files:** `docs/DECISIONS.md`, `docs/STATUS.md`, `docs/PROJECT.md`,
`docs/research/parity-matrix.md`, `docs/research/p2-9-35hz.md`, spec.

- [ ] **Step 1:** full build + `ctest` with `GL_GAME_DIR`; record N/time/exit.
  If failures: fix only oracle/implementation defects within this plan; do not
  weaken tests.
- [ ] **Step 2:** D-068:
```markdown
| D-068 | 2026-11-13 | P2-9a/b - 35 Hz tick implementation. `ServerCore` uses integer-ms clock and the original host gate: `tick_interval_ms(35)=28` (`1000/35` trunc), at most one tick per `tick(dt)`, no accumulator/catch-up/clamp; `simulation_time_ms` is the clock latch at the executed tick. `sv_fps=35`/`sv_game_speed=0` are fixed defaults (no config plumbing). `LevelSimulation::advance` performs exactly one step with `delta_ms=llround(dt*1000)`; movement step is `speed*delta_ms/1000`; the 60 Hz accumulator is removed. Headless glgame/glview use a declared 16 ms frame schedule. Pause keeps the port's no-backlog behavior (original pause path unverified). Oracle refresh uses analytic/independent schedules. | spec P2-9 impl; research P2-9 §Tick model/§Impact; D-021/D-067. |
```
- [ ] **Step 3:** STATUS done (real ctest, residual U1/pause/anim U-C1,
  config plumbing absent); PROJECT P2/P4 notes tick now 35 Hz gate;
  parity `P2-9` -> `verified` for gate (or `partial` if U1 remains a caveat,
  state explicitly); research update with implementation result.
- [ ] **Step 4:** encoding checks; commit docs including spec/plan
  `docs: record 35hz tick implementation`.

---

## Self-review notes

- Spec §3.1 → Task 1; §3.2 → Task 2; §3.3 → Task 3; §7 → Task 4.
- Oracle strategy is A/R/L; no “copy port”.
- U1 (host frame) and pause remain documented limitations; config plumbing
  out of scope.
- `50ms`/`250ms` single-call expectations are one tick by design (no
  catch-up); tests must assert that explicitly.
