# P6a1 — Quest/event runtime (plan)

Spec: `docs/specs/2026-11-19-p6a1-quest-runtime-design.md`
Status: implemented (2026-11-19)

## Task 1 — Runtime (TDD)

1. `tests/game/test_quest_runtime.cpp`: quest/stage string names,
   storyline, idempotence, empty names, clear.
2. `quest_runtime.hpp/cpp`; add to `src/game/CMakeLists.txt`.

## Task 2 — Natives

1. `QuestAccess` string callbacks in `NativeServices`; 5 handlers
   (`0x7000000..4`) in `native_registry.cpp`, `0.0` return.
2. Tests: string dispatch, numeric rejection, missing access; event natives
   remain unsupported.
3. Registry counts 21/65.

## Task 3 — glgame wiring

1. `QuestRuntime quest_runtime_` in `Game`; attach to `DialogContext`
   services; `load_level` clears state.
2. Smoke: `glgame_screenshot_tests` green.

## Task 4 — Verification and docs

1. Focused `gl_game_tests` and `gl_script_tests`.
2. Full `ctest`.
3. `STATUS`, `PROJECT`, `DECISIONS` (D-094), parity matrix; reviewer; commit.
