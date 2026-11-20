# P6a2 — Time-event runtime (plan)

Spec: `docs/specs/2026-11-19-p6a2-time-event-runtime-design.md`
Status: approved (2026-11-19)

## Task 1 — Runtime (TDD)

1. `tests/game/test_time_event_runtime.cpp`: handles, due, clear, unknown,
   negative/mixed offsets, clear-all.
2. `time_event_runtime.hpp/cpp`; add to `src/game/CMakeLists.txt`.

## Task 2 — Natives

1. `TimeEventAccess` in `NativeServices`; handlers for `0x7000005..7`.
2. Tests: SetEvent returns handle, GetEvent 0→1, ClearEvent 0.0, missing
   access; registry counts 24/65.

## Task 3 — glgame

1. `TimeEventRuntime time_events_` with `now` provider from `calendar_`;
   attach to dialog services; clear on New Game/successful load and rollback
   on failed load (snapshot like quests).
2. Screenshot smoke.

## Task 4 — Verification and docs

1. Focused tests; full `ctest`.
2. `STATUS`, `PROJECT`, `DECISIONS` (D-096), parity matrix; reviewer; commit.
