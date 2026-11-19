# P2-15c3g1 — Game calendar model (plan)

Spec: `docs/specs/2026-11-19-p2-15c3g1-calendar-model-design.md`
Status: implemented (2026-11-19)

## Task 1 — Date/time math (TDD)

1. `tests/game/test_calendar.cpp`:
   - `days_from_civil` for known dates (1970-01-01, 2000-02-29, 1900-03-01,
     year-end) and round-trip via a local `civil_from_days`;
   - `add_milliseconds`: 1000/60k/3.6M/86.4M; month/year/leap carries;
     negative? (not required) — reject or floor.
2. Implement `days_from_civil`, `civil_from_days` (internal), and
   `add_milliseconds` in `gl/game/calendar.hpp`/`src/game/calendar.cpp`.
3. Acceptance: unit cases pass.

## Task 2 — `GameCalendar` (TDD)

1. Tests:
   - default/first `advance` no-op; second +1 s = +1 game minute;
   - 24 real minutes = +1 game day; paused advance does not change time;
   - speed 0.5 / 2.0 scaling; uint32 wrap sample;
   - weekday day-carry wrap.
2. Implement `GameCalendar`.
3. Add to `src/game/CMakeLists.txt` and tests CMake.
4. Acceptance: unit cases pass.

## Task 3 — Verification and docs

1. Focused: `gl_game_tests --test-case="*calendar*"`.
2. Full `ctest`.
3. Update `STATUS`, `PROJECT`, `DECISIONS` (D-089); residuals (weekday
   epoch, c3g2 wiring).
4. Reviewer; UTF-8 no BOM/NUL; `git diff --check`; commit.
