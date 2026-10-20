# P2b — Deterministic Tick Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development or executing-plans. Steps use `- [ ]`.

**Goal:** Make `ServerCore` advance a deterministic 60 Hz simulation clock, expose snapshots to `ClientCore`, and make `glgame` render animations from server simulation time with pause semantics.

**Architecture:** Extend the SDL-free `gl_core` (fixed timestep, tick index, simulation time, snapshot). The shell (`glgame`) drives the clock with frame delta and stops using its own `animation_ms_`.

**Tech Stack:** C++20, existing `gl_core`/`gl_app`/SDL2, doctest.

## Global Constraints

- C++20; `-Wall -Wextra -Werror`.
- `gl_core`/`gl_game`/`gl_assets` SDL-free; SDL only in platform/render/app/tools.
- Fixed step **60 Hz**; `kMaxFrameDt=0.25`; `kMaxStepsPerTick=15`.
- Commands are applied at the start of `tick`, before the pause check; `gv_pause 0` must resume in the same tick.
- No game assets committed. Every task ends with a commit.

## File Structure

```
src/core/include/gl/core/server.hpp   (extend: snapshot/time/constants)
src/core/src/server.cpp               (fixed-step accumulator)
tests/core/test_server.cpp            (determinism/pause/limits)
tools/glgame/main.cpp                 (use server time; remove animation_ms_)
docs/...                              (STATUS/PROJECT/DECISIONS/research)
```

---

### Task 1: ServerCore fixed-step clock + snapshot

**Interfaces:**
```cpp
namespace gl::core {
inline constexpr int kTickHz = 60;
inline constexpr double kFixedDt = 1.0 / kTickHz;
inline constexpr double kMaxFrameDt = 0.25;
inline constexpr int kMaxStepsPerTick = 15;

struct ServerSnapshot {
    bool paused = false;
    bool free_camera = false;
    std::uint64_t tick_index = 0;
    std::uint64_t simulation_time_ms = 0;
};

class ServerCore {
    // existing...
    std::uint64_t tick_index() const;
    std::uint64_t simulation_time_ms() const;
    ServerSnapshot snapshot() const;
};
}
```
`tick(double dt)`:
1. process pending commands (including pause);
2. if `paused_`: `accumulator_ = 0`; return;
3. clamp `dt` to `[0, kMaxFrameDt]`; `accumulator_ += dt`;
4. loop up to `kMaxStepsPerTick`: while `accumulator_ >= kFixedDt`, `++tick_index_`, `accumulator_ -= kFixedDt`; if the step cap is hit, `accumulator_ = 0`;
5. `simulation_time_ms = tick_index_ * 1000 / kTickHz`.

Tests:
- `tick(1.0)` → 60 ticks, 1000 ms (exact within double accumulation: 1.0 / (1/60) may yield 60; assert 60).
- `tick(0.5)` → 30 ticks.
- `0.01 × 60` → 60 ticks.
- pause: `tick` does not advance; `gv_pause 0` submitted while paused applies on the next tick and advances.
- `tick(10.0)` → ≤15 ticks and accumulator reset (next `tick(0)` no advance).
- submit before tick does not apply until tick; events once.
- determinism: same `dt` sequence → same tick/time; different sequences to the same total time → same totals (with tolerance where needed).

- [ ] TDD, implement, commit.

---

### Task 2: ClientCore snapshot

**Interfaces:**
```cpp
class ClientCore {
    void on_snapshot(const ServerSnapshot&);
    const ServerSnapshot& snapshot() const;
};
```
Tests: mirror updates; default snapshot; snapshot independent of later server changes.

- [ ] TDD, implement, commit.

---

### Task 3: glgame integration

- Loop: `delta_ms` (cap 250) → `server_.tick(delta_seconds)`; `client_.on_snapshot(server_.snapshot())`; process `client_.events()` as before.
- Render level with `client_.snapshot().simulation_time_ms`; remove `animation_ms_` and the manual pause accumulation.
- Pause: `server_.paused()` drives both the PAUSED label and frozen animations (time stops).
- `--screenshot`: keep deterministic `frames * 16` ms; do not advance the server clock for screenshots.
- Keep input timing (`delta_seconds` for pan) independent of the simulation clock.
- Verify existing screenshot/menu/level tests pass; add a test that `simulation_time_ms` stays constant while paused and advances after unpause (unit in `gl_core`, already Task 1) and that glgame screenshots are unchanged.

- [ ] Implement, run tests, commit.

---

### Task 4: Acceptance + docs

- Full `ctest` with `GL_GAME_DIR`.
- Run `glgame --game-dir I:\Games\Златогорье-2`; animations run; `gv_pause 1/0` freezes/resumes (manual).
- `docs/STATUS.md`: P2b accepted (automated; manual note), next P2c.
- `docs/PROJECT.md`: P2 progress.
- `docs/DECISIONS.md`: D-021 fixed 60 Hz, max frame dt 0.25, max 15 steps.
- `docs/research/`: tick/time facts.
- Commit.

## Self-Review

- Covers fixed-step determinism, pause/commands, snapshot boundary, shell integration, tests, docs.
- Deferred: interpolation/networks/game systems/save-load.
