# P2-15c3g — Game calendar research

Status: complete (2026-11-19)
Phase: P2-15c3g (research-only; follow-up to P2-15c3f, D-087)
Scope: `docs/`; scripts in `.local/research/c3g/` (not committed).

## 1. Provenance

| Artifact | SHA-256 |
|---|---|
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| `.local/research/c3g/calendar_scan.py` | `9621C06A78CE1CED5BFABAABCB54A6F29D4F2C6CE3E4144226E96CD7457C570C` |
| 28-save manifest | `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |

Corpus: `Saves/slot0.sav`…`slot27.sav` (28 files); every tail
`BT_RGINF` section (2,599 datetimes).

## 2. `BT_INFO` / `BT_RGINF` datetime layout

Both use the same eight little-endian `u32` words:

| index | field | range | evidence |
|---:|---|---|---|
| 0 | year | 1124..1126 in corpus | comparator/addition order |
| 1 | month | 1..12 in saves | get/set ±1 conversion (below) |
| 2 | day | 1..31 in saves | get/set ±1 conversion |
| 3 | weekday | 0..6 | comparator skips index 3 |
| 4 | hour | 0..23 | advance carry `>= 24` |
| 5 | minute | 0..59 | advance carry `>= 60` |
| 6 | second | 0..59 | advance carry `>= 60` |
| 7 | millisecond | 0..999 | advance carry `>= 1000` |

**Internal vs saved representation.** The clock object stores month/day
**0-based** at `+0x14`/`+0x18`; vtable `+0x18` (get) copies the eight words and
adds `1` to month and day (`0x1405bb84`), while `+0x1c` (set) copies the save
words and decrements both (`0x1405bbc4`). Saved files are therefore 1-based;
the corpus confirms valid ranges.

Calendar: **Gregorian**, with real month lengths (31/30/28/29) and the
div-4 / not-100 / or-400 leap rule. Evidence: `FUN_1405b140` (serial day)
uses 0x1f/0x1e/0x1c/0x1d and the leap test, base `0x16d` (365);
`FUN_1405ad58` splits a serial day back with the same rule. The add
`FUN_1405b38c` carries 60/60/24 and then normalizes through those helpers.

`0x1e` (30) appears only in **duration/timer** conversions:
`FUN_1405b504` maps a datetime through `(year*12 + month)*30 + day`, and the
regeneration interval is split as `month = interval / 0x1e`,
`day = interval % 0x1e`. The `31,104,000,000` constant is a duration year
(360 days) used by the timer math, **not** the date calendar.

## 3. Clock object and contract

`DAT_14088e14` is a `0x48`-byte object allocated by `FUN_1405bfdc`, vtable
`0x14092b98`:

| slot | VA | contract |
|---|---|---|
| `+0x00` | `0x1405b38c` | add datetime + duration (carry 1000 ms / 60 s / 60 min / 24 h, then Gregorian date normalization) |
| `+0x08` | `0x1405b490` | compare (year, month, day, hour, minute, second, ms; weekday skipped) |
| `+0x18` | `0x1405bb84` | get current datetime (copies +0x10..+0x2c, month/day +1) |
| `+0x1c` | `0x1405bbc4` | set datetime (copies, month/day -1, recomputes weekday) |
| `+0x20` | `0x1405bc84` | advance from real time (see §4) |
| `+0x28` | `0x1405c040` | timer-list update (unrelated to date fields) |

Fields: `+0x08` speed `float` (default `1.0`), `+0x0c` last `timeGetTime`,
`+0x10..0x2c` year/month/day/weekday/hour/min/sec/ms, `+0x30/+0x34`
timer-list pointer/count (0x24-byte entries), `+0x40/+0x44` real/game ms
accumulators.

The writer/reader use `+0x18`/`+0x1c` (`FUN_14031b64` 35389-35390,
`FUN_1403210c` 35633-35634); `BT_RGINF` reads/writes the same layout
(35812-35846, 36040-36042).

## 4. Advance model

`FUN_1405bc84` (slot `+0x20`):

1. If `+0x04` is zero, return (clock frozen/paused).
2. `delta = timeGetTime() - +0x0c`; store new sample.
3. `+0x40 += delta`, `+0x44 += delta`.
4. Convert with `speed = +0x08` (default 1.0), factor `60.0`, duration
   year-ms `31104000000` and `1/year-ms`; add the result to the date fields
   with the 60/60/24 carries and Gregorian normalization (weekday recomputed
   from the serial day).
5. No time gate inside `FUN_1405bc84`; it advances on each call. The
   `0.0002`/≥5 s formula belongs to `FUN_1405c0c4`, a separate timer-list
   object (`+0x54/+0x58/+0x5c`), and is **not** part of the clock.
6. Wrapper `0x5bfb8` (vtable `+0x24`) calls `+0x20`, then clears `+0x04` and
   sets `+0x44 = gv_... + 1`; this is a day/timer-list tick, not the
   real-time advance itself.

**Inferred:** with the 60.0 factor, 1 real second = 1 game minute, 24 real
minutes = 1 game day, 6 real days = 1 duration-year (360 days). This is a
static inference from the constants; a live before/after sample is required
for a parity claim. No clock-level time gate was found.

## 5. Item regeneration

- `BT_RGINF[0]` = latch `+0x26c0`; `BT_RGINF[1..7]` = last-regeneration
  datetime `+0x26a0..+0x26bc`.
- Due-check when latch is 0: `saved + interval days <= now`; on due, latch = 1
  and the last-regeneration datetime = now (35817-35842).
- `gv_items_regenerate_interval` = 7 days single-player, 5 multiplayer
  (D-074; config `DAT_1407fbcc+0x8c`).

## 6. Corpus scan

`calendar_scan.py` read-only over 28 saves / 2,599 `BT_RGINF` sections.

| metric | result |
|---|---|
| saves / BT_INFO times | 28 / 28 |
| RGINF datetimes | 2,599 |
| BT_INFO year range | 1124..1126 |
| RGINF year range | 1124..1126 |
| BT_INFO months | 1,2,3,5,6,7,9,10,11,12 |
| BT_INFO days | 4..31 |
| BT_INFO weekday | 0,2,3,4,5,6 (1 absent in this corpus) |
| BT_INFO hour/min/sec/ms | valid ranges 0..23 / 0..59 / 0..59 / 0..999 |
| RGINF months / weekdays | all 1..12 / all 0..6 |
| RGINF datetime > BT_INFO | **0 / 2,599** |
| header date == BT_INFO date | **0 / 28** (wall clock vs game time) |

The last two rows are the key boundary evidence: game time is independent of
the save-header SYSTEMTIME, and the last-regeneration timestamp never exceeds
the current game time.

## 7. Verdict

**`implementable (scoped)`.**

- Proven: layout, 1-based save representation, Gregorian 365/366 calendar
  with real month lengths and leap years, get/set/add/compare contracts,
  interval/latch semantics, corpus ranges and `RGINF <= BT_INFO`.
- Inferred (needs live confirmation): the 60× real→game rate.
- Blocked: none for the format; only a live rate/cadence parity check.

## 8. Decomposition

| Task | Scope | Acceptance |
|---|---|---|
| `P2-15c3g1` `GameCalendar` model | eight-field datetime, 1-based save conversion, Gregorian add/compare, weekday from serial day, advance(delta_ms, speed) with 60× | unit: leap-year/month-length carries, get/set round-trip, corpus decode 28/28 |
| `P2-15c3g2` live time wiring | `apply_live_save` uses the calendar for `BT_INFO` time and `BT_RGINF` datetime instead of raw | save→parse round-trip; RGINF latch/datetime semantics; c3f tests |
| `P2-15c3g3` live rate/cadence check | user-assisted read-only: two BT_INFO samples N real seconds apart | measured game-minute/second ratio 0/N vs 60× |

## 9. Non-claims

- No live sample yet: the 60× rate is inferred.
- No day/night rendering or UI clock.
- Multiplayer interval (5 days) is recorded from D-074 but not exercised.
- Timer-list `+0x30/+0x34` (0x24-byte entries) is unrelated to the calendar
  and remains unimplemented; `+0x28` slot is out of scope.

## 10. Reproduce

```powershell
py -3 .local\research\c3g\calendar_scan.py
Get-FileHash -Algorithm SHA256 .local\research\c3g\calendar_scan.py
& .local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin\llvm-objdump.exe -d `
  --start-address=0x1405b38c --stop-address=0x1405b504 "I:\Games\Златогорье-2\Server.dll"
& .local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin\llvm-objdump.exe -d `
  --start-address=0x1405bb84 --stop-address=0x1405bfc0 "I:\Games\Златогорье-2\Server.dll"
```
