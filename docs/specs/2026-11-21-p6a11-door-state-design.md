# P6a11 — Door state native (design)

Status: approved (2026-11-21)
Scope: SEF/LVL door data, level walk grid, `DoorRuntime`, native
`RS_SetDoorState` (0x7000014), glgame wiring, tests. One subproject.

## Goal

Implement `RS_SetDoorState(name, state)` with parity for door lookup,
stored state and runtime walkability, and mark it supported.

## Evidence

`docs/research/p6a11-door-state.md`; Server.dll SHA-256
`418E2560…CC837`, GoldenLand.exe `7854019E…109FF`.

- Native `0x41c74`: type gate string+int, returns `0.0`; calls
  `FUN_14029388(server, name)` then `FUN_14024228(door, state)`.
- `FUN_14029388` matches `(entity+0x14 & 0x3e) == 0x20` and exact
  `entity+0x4c` name.
- `FUN_14024228`: stores `state` at `+0x24`; state 0 sets `0x100/0x1000`
  on the door cell group, state != 0 clears them.
- `FUN_14023fb4` maps the SEF door name to a LVL `BLK_DOOR` record, takes
  its fourth string (`+0x31c`) and maps it to a `BLK_CGRP` group
  (`+0x368`), then applies the initial `+0x24`.
- SEF `doors:` reader `FUN_14024584`: `name`, `literary_name_open`,
  `literary_name_close`, `script_name`, `cells_name`, `is_opened`,
  `is_inversed`; base constructor zeroes `+0x24` (closed by default).

## Non-goals

- Sight flag `0x1000` (stay out of scope, as P4b2).
- Door rendering, interaction/event type `0x14`, scripts `bOpenDoor`.
- Save/load of door state (initial state is re-derived from SEF).
- Revalidating already planned NPC/hero paths after a door change; the
  original movement step checks occupancy only, so cached waypoints stay.
- Replicating the original null-dereference for an unknown door name.

## Assets

`gl::assets`:

- `SefDoor`: `name`, `literary_name_open`, `literary_name_close`,
  `script_name`, `cells_name`, `is_opened`, `is_inversed`; defaults 0.
  `SefData::doors` parsed from `doors:`; malformed known fields are a
  format error, unknown keys are skipped.
- `LvlDoor`: `name`, `cell_group`; `LvlData::doors` parsed from
  `BLK_DOOR`. All six strings are read and the payload must be consumed
  exactly; only fields 1 and 4 are retained.
- No SDL/OS dependency.

## Game

`gl::game`:

- `LevelDoor { std::string name; std::int32_t state;
  std::vector<std::pair<std::int32_t, std::int32_t>> cells; }`;
  `Level::doors`.
- `load_level` builds `Level.doors` after parsing SEF and LVL:
  - SEF door → LVL door by exact name → `LvlCellGroup` by `cell_group`.
  - A SEF door without an LVL record or group is skipped with a warning
    (no such door in the shipped data).
  - Initial state: `state = is_opened` (any non-zero is open).
  - After the static `WalkGrid` is built, every door is applied:
    state 0 blocks all group cells, state != 0 clears them.
- `DoorRuntime` (SDL-free):
  - `reset(std::span<const LevelDoor>)` copies the doors.
  - `bool set(std::string_view name, std::int32_t state, WalkGrid&)`
    updates the stored state and applies the same block/clear rule;
    exact match, unknown name returns false and does not touch the grid.
  - `const LevelDoor* find(std::string_view) const`,
    `std::size_t size() const`, `void clear()`.
- `LevelSimulation` gains `void refresh_walk_grid(const WalkGrid&)` which
  replaces its private grid copy; occupancy is unchanged.

## Script host

- `DoorAccess { std::function<bool(std::string_view, std::int32_t)>
  set; }`.
- `NativeServices::door`.
- Native `0x7000014`: arity 2, `supported`, string + numeric VM value
  gate (int32/uint32/double), returns `0.0`; calls `door.set` when present.

## App / glgame

- `LevelScreen` owns `DoorRuntime doors_`, initialized from
  `level_.doors`; exposes
  `bool set_door_state(std::string_view, std::int32_t)` which calls
  `doors_.set(name, state, level_.walkable)` and then
  `simulation_.refresh_walk_grid(level_.walkable)`.
- Move constructor moves `doors_`; pointers are not stored in it.
- glgame binds `services().door.set` to `level_->set_door_state`.
- New Game and save load create a fresh level, so door state resets from
  SEF automatically.

## Error handling

- Asset parsers return `ErrorCode::format` with block/section and line or
  offset on malformed input.
- Missing optional sections (`doors:`) yield an empty list.
- Unknown door names are a no-op with `false` (port safety deviation).

## Tests

- `tests/assets/test_sef.cpp`: door fields, defaults, unknown keys,
  malformed types; synthetic.
- `tests/assets/test_lvl.cpp`: synthetic `BLK_DOOR` (name + group,
  trailing-byte/count errors).
- `tests/assets/test_real_levels.cpp`: unchanged 175 LVL parse sweep;
  real door records are asserted in `tests/game/test_real_doors.cpp`.
- `tests/game/test_door_runtime.cpp`:
  - synthetic grid: closed blocks, open clears, repeated set is
    idempotent, unknown name no-op, exact/case-sensitive name;
  - native registry: `0x7000014` supported, arity 2, returns 0.0,
    string+numeric gate;
  - supported total 35/65 (update the two hard-coded counts).
- `tests/game/test_real_doors.cpp`: `l10_1` closed door group blocked at
  load, `door_storozhka` open → group walkable, close → blocked;
  `l10_1_2` initial `is_opened=1` leaves its group walkable.
- `gl_script_tests` 131/131 and screenshot suite unchanged; full `ctest`.

## Acceptance

- `0x7000014` supported (35/65).
- Real `l10_1` and `l10_1_2` door behaviour asserted.
- Full `ctest` green with `GL_GAME_DIR`.
- `docs/STATUS.md`, `docs/PROJECT.md`, `docs/research/formats.md`,
  `docs/DECISIONS.md` updated.
