# P6a3 — Trigger state runtime (plan)

Spec: `docs/specs/2026-11-19-p6a3-trigger-state-design.md`
Status: approved (2026-11-19)

## Task 1 — Runtime (TDD)

1. `tests/game/test_trigger_runtime.cpp`: reset/known/unknown/toggle/clear.
2. `trigger_runtime.hpp/cpp`; add to `src/game/CMakeLists.txt`.

## Task 2 — Native and wiring

1. `TriggerAccess`; handler `0x7000009` (string,int -> 0.0).
2. glgame: reset from `level_->level().triggers` after load; attach service.
3. Registry counts 25/65; tests.

## Task 3 — Real and full verification

1. Real `l1_1` triggers: count/name toggle (`GL_GAME_DIR`).
2. Screenshot; full `ctest`.
3. `STATUS`, `PROJECT`, `DECISIONS` (D-097), parity matrix; reviewer; commit.
