# P2-15c3g2 — Live time wiring (plan)

Spec: `docs/specs/2026-11-19-p2-15c3g2-live-time-wiring-design.md`
Status: implemented (2026-11-19)

## Task 1 — `to_time_bytes` + `LiveSaveInput`

1. Unit: `GameDatetime::to_time_bytes` round-trips `from_time_bytes`.
2. `LiveSaveInput.calendar`/`rginf_datetime`; `apply_live_save`:
   - if calendar → `payload.info.time = calendar->now().to_time_bytes()` and
     `update_info_block`;
   - tail RGINF datetime from `rginf_datetime` when set, else loaded.
3. Tests: calendar set/absent; RGINF override; parse round-trip; malformed
   calendar (invalid fields) → `format`.

## Task 2 — glgame wiring

1. Members + load/rollback + decision timestamp.
2. Advance calendar once per frame from a steady clock (only when a loaded
   save is active).
3. Pass calendar/timestamp into `LiveSaveInput`.
4. E2E headless: load slot0, write slot, parse: `BT_INFO` time unchanged
   (no frames); advanced-time is covered by the `apply_live_save` real test.

## Task 3 — Verification, docs, review

1. Focused:
   ```powershell
   build\bin\gl_game_tests.exe --test-case="*live save*,*calendar*"
   build\bin\glgame_screenshot_tests.exe --test-case="*save*"
   ```
2. Full `ctest`.
3. `STATUS`, `PROJECT`, `DECISIONS` (D-090); residuals (rate c3g3, weekday
   epoch, timer list).
4. Reviewer; fix Critical/Important; UTF-8 no BOM/NUL; `git diff --check`;
   commit.
