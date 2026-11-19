# P2-15c3g — Game calendar research (plan)

Spec: `docs/specs/2026-11-18-p2-15c3g-game-calendar-research-design.md`
Status: implemented (2026-11-19); research-only.

## Task 1 — Static clock contract

1. Re-read `Server.dll` calendar object `DAT_14088e14`:
   - vtable `0x14092b98`: `+0` add `0x5b38c`, `+8` compare `0x5b490`,
     `+0x18` get `0x5bb84`, `+0x1c` set `0x5bbc4`, `+0x20` advance `0x5bc84`
     (verify slot indices from the vtable dump).
   - helpers `FUN_1405b140` (serial day), `FUN_1405ad58` (date from serial),
     `FUN_1405b504`, constants `0x92b20..0x92b88`.
2. Record: field offsets, internal 0-based vs saved 1-based month/day, Gregorian
   month lengths and leap years, 24h / 60m / 60s / 1000ms; 30/360 only for
   duration/timer math.
3. Record the advance: delta source (`timeGetTime`), speed field `+8`
   (default `0x3f800000`), factor 60, duration year-ms constant; no clock-level gate.
4. Verify the writer/reader paths (35389/35633, 35812-35846).

## Task 2 — Corpus scan

1. Python read-only script under `.local/research/c3g/`:
   - parse `BT_INFO` time (28 saves) and every tail `BT_RGINF` datetime;
   - check ranges (month 1..12, day 1..31, weekday 0..6, hour 0..23,
     minute/second 0..59, ms 0..999), distinct values, and
     `RGINF <= BT_INFO` within a save;
   - compare with the header SYSTEMTIME (should differ: wall clock vs game
     time).
2. Record the script SHA-256 and summary counts.

## Task 3 — Verdict and decomposition

1. Write `docs/research/p2-15c3g-game-calendar.md`: layout, contract,
   advance model, interval/due-check, corpus table, unknowns, verdict.
2. Decomposition: port `GameCalendar` (layout/advance/interval), live
   `BT_INFO`/`RGINF` time wiring, live rate/cadence parity.
3. Update `STATUS`, `PROJECT`, `DECISIONS` (D-088); docs UTF-8 no BOM/NUL;
   `git diff --check`; reviewer.
