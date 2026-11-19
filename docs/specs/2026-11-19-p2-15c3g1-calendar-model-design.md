# P2-15c3g1 — Game calendar model (design)

Status: implemented (2026-11-19)
Task: `P2-15c3g1` (follow-up to P2-15c3g, D-088)
Scope: `src/game`, `tests/`; no save wiring (c3g2).

## 1. Context

D-088 proved the save datetime layout, the Gregorian date rule
(`FUN_1405b140`/`FUN_1405ad58`), and the clock advance (`0x5bc84`: real ms ×
speed × 60, Gregorian carries). `GameDatetime` already stores the 8 fields
and `add_days` is Gregorian. Missing: an advanceable clock and sub-day
addition.

## 2. API

`gl/game/calendar.hpp`:

```cpp
std::int64_t days_from_civil(std::int32_t year, std::uint32_t month,
                             std::uint32_t day);
GameDatetime add_milliseconds(const GameDatetime &value, std::int64_t ms);

class GameCalendar {
  public:
    GameCalendar() = default;
    explicit GameCalendar(GameDatetime value);
    const GameDatetime &now() const;
    void set(GameDatetime value);
    void set_paused(bool paused);
    bool paused() const;
    void set_speed(double speed);       // default 1.0
    double speed() const;
    void advance(std::uint64_t now_ms); // timeGetTime-style sample
};
```

`GameDatetime` month/day are the **save representation** (1-based), as in
`rginf.hpp`; no 0-based conversion is exposed.

## 3. Semantics

- `days_from_civil` = proleptic Gregorian serial day (Howard Hinnant
  algorithm); used for date carry.
- `add_milliseconds`: `total = time_of_day_ms + ms`; `day_carry =
  floor_div(total, 86400000)`; time = floor_mod; date = civil_from_days(serial
  + day_carry); **weekday += day_carry mod 7** (weekday is stored; the
  original recomputation epoch is unverified — non-claim).
- `advance(now_ms)`:
  - if paused: update `last_ms_` only (paused real time is excluded);
  - else `delta = (uint32_t)(now_ms - last_ms_)` (timeGetTime wrap),
    `game_ms = round(delta * 60.0 * speed)`, `value_ =
    add_milliseconds(value_, game_ms)`;
  - first call initializes `last_ms_` without advancing.
- Speed accepts only finite non-negative values; invalid values become `0`.
  The default is `1.0`; no parity claim beyond 60×.

## 4. Acceptance

1. `add_milliseconds`: 1000 ms = +1 s; 60 s/60 min/24 h carries; month-end,
   year-end, Feb 28/29 and century leap rules.
2. `advance`: 1 real second = +1 game minute at speed 1; 24 real minutes =
   1 game day; first call no-op; paused call no-op; wrap-safe delta.
3. Weekday advances by day carry mod 7 and wraps 0..6.
4. GameDatetime save array/time-bytes round-trip unchanged.
5. Full `ctest`; no save wiring.

## 5. Non-goals

- `BT_INFO`/`BT_RGINF` wiring (c3g2).
- Exact original weekday epoch/formula (blocked; weekday preserved + day
  carry).
- Day/night UI, timer-list `+0x30/+0x34`, multiplayer interval.
