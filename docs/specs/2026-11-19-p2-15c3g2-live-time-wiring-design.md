# P2-15c3g2 — Live time wiring (design)

Status: implemented (2026-11-19)
Task: `P2-15c3g2` (follow-up to P2-15c3g1, D-089)
Scope: `src/game`, `tools/glgame`, `tests/`.

## 1. Context

`apply_live_save` still copies `BT_INFO` time and `BT_RGINF` datetime raw.
D-088/D-089 provide the layout and `GameCalendar`. This task writes the
current game time and the last-regeneration time.

## 2. API

- `GameDatetime::to_time_bytes()` → 0x20 bytes (LE, save layout).
- `LiveSaveInput` adds:
  - `const GameCalendar *calendar = nullptr;` — when set, `BT_INFO` time is
    `calendar->now().to_time_bytes()`;
  - `std::optional<GameDatetime> rginf_datetime;` — last-regeneration time
    for `BT_RGINF` (loaded value or the due decision timestamp).
- `apply_live_save` calls `update_info_block` after setting `info.time`.

## 3. glgame

- Members `GameCalendar calendar_; bool has_calendar_ = false;` and
  `std::optional<GameDatetime> runtime_rginf_timestamp_;`.
- On load: `calendar_ = GameCalendar(GameDatetime::from_time_bytes(info.time))`;
  `has_calendar_ = true`; `runtime_rginf_timestamp_ = decision.timestamp`.
- On level-load failure rollback: restore/clear both with the other restore
  state.
- Frame loop: `calendar_.advance(steady_clock_ms())` once per frame while a
  loaded save is active; save uses the current value.
- `write_loaded_save_to`: pass `calendar`/`rginf_datetime`; `BT_RGINF`
  datetime is the last-regeneration time (not necessarily now).
- New Game / no loaded save: calendar is absent; time stays raw.

## 4. Acceptance

1. Calendar wired: `BT_INFO` time advances by 1 game minute per real second
   (60×), wrapped through Gregorian carries; save→parse round-trip.
2. `BT_RGINF` datetime = loaded value when not due, `decision.timestamp`
   when due; latch still from the decision.
3. No calendar / no loaded save → `BT_INFO` time unchanged.
4. E2E headless: load→write→parse — `BT_INFO` time is unchanged because no
   frames run; the advanced-time path is covered by the `apply_live_save`
   real test. Interactive frame advance is a manual check.
5. Full `ctest`.

## 5. Non-goals

- Exact original weekday epoch (c3g1 non-claim), live rate parity (c3g3),
  day/night UI, timer-list `+0x30/+0x34`, level transitions.
