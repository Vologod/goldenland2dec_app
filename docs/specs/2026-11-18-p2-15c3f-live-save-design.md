# P2-15c3f — Live save integration (design)

Status: implemented (2026-11-18)
Task: `P2-15c3f` (follow-up to P2-15c3b/c/d/e)
Scope: `src/game`, `tools/glgame`, `tests/`; no level transitions.

## 1. Context

`write_loaded_save_to` currently re-encodes `BT_VARS`/`BT_TRIBE` from the
loaded values, patches the header and copies `BT_HERO`/tail/DIARY/HSTRY raw.
`P2-15c3b`…`e` provide the HERO body writer, the selected-tail writer, and
the `BT_INFO`/tail-index helpers. This task wires them to the live state:

- HERO: `HeroController` tile/direction, `InventoryRuntime` containers,
  hero HP/EP, loaded AP/stats/opaque blocks;
- selected tail: `build_selected_tail` with live simulation positions,
  restored mapping, live c0/c1, c2 raw, RGINF latch;
- VARS: `update_vars_from_globals` from the dialog VM when present and from
  the live snapshot captured on dialog close; new names are not added
  (D-080 policy);
- header date/title (existing);
- DIARY/HSTRY, type 8/16/32 raw, c2/opaque blocks unchanged.

## 2. Testable integration helper

New `game/live_save.hpp`:

```cpp
struct HeroLiveState { ... };          // already from c3b
struct LiveSaveInput {
    const Level* level;
    const LevelSimulation* simulation;
    const InventoryRuntime* inventory;
    const RestoreResult* restore;
    const PersonRuntime* persons;
    const HeroLiveState* hero;
    std::uint32_t rginf_latch;
    bool has_rginf_latch;
    std::vector<gl::assets::SaveVar> vars;
};
Result<gl::assets::SavePayload> apply_live_save(gl::assets::SavePayload payload,
                                                const LiveSaveInput& input);
```

It performs, in order:

1. HERO: parse block 6 with `parse_hero_save_body`; build
   `make_hero_save_body`; encode; `replace_block_body(6)`.
2. Tail: if `level_tail_index` and restore is available, build live persons
   (`alive = !(has life && life <= 0)`, HP/EP from `PersonRuntime` when
   present, AP from the loaded record), call `build_selected_tail` with
   `new_persons={}`, RGINF latch from input (else loaded), upsert the entry.
3. VARS: if `vars` non-empty, `encode_vars_body` and `replace_block_body(2)`.
4. TRIBE: unchanged (c2 already snapshots; no live relation model).
5. Header/`BT_INFO`: unchanged (no level transition; c3e helpers available).

Returns `format` on parse/encode errors; no filesystem I/O.

## 3. `PersonRuntime` presence

Add `bool has_parameter(std::string_view person, PersonParameter) const` so
the adapter can distinguish absent HP/EP from 0. `alive` uses the life
parameter only when present; otherwise true.

## 4. glgame wiring

- Add `live_save_vars_` (initialized from `payload.vars` on load, rollback on
  level-load failure) and `runtime_rginf_latch_` + `has_rginf_latch_` from the
  `decide_rginf` result during load.
- `reset_dialog()` updates `live_save_vars_ = update_vars_from_globals(...)`
  while the dialog VM is alive.
- `write_loaded_save_to` builds `LiveSaveInput`:
  - hero tile/direction from `level_->hero().hero()`, containers from
    `inventory_runtime_.hero_inventory()`, `present` flags from the loaded
    HERO body, HP/EP from `person_runtime_` (`Hero` life/energy) when
    present, AP from the loaded HERO record;
  - tail from `level_->level()`, `level_->simulation()`,
    `inventory_runtime_`, `restore_result_`, `person_runtime_`,
    `runtime_rginf_latch_`;
  - vars from `live_save_vars_` plus an active dialog VM if present.
- Call `apply_live_save`, patch the header, encode with `encode_save_payload`.
- Same path for `--write-save`, `--write-save-slot` and `game_save`.

## 5. Error handling / non-claims

- Missing loaded level/restore → keep the HERO/tail raw (no crash); log a
  warning. `--write-save` without a payload still exits 4.
- VARS names/order fixed to the loaded vector (D-080); new globals are not
  added; an empty `LiveSaveInput::vars` leaves `BT_VARS` untouched.
- Duplicate-name HP/EP/alive use the loaded values: `PersonRuntime` is
  name-keyed and does not distinguish occurrences (same limitation as the
  name-keyed containers).
- DIARY/HSTRY, c2, opaque type-2 blocks, appended persons (no spawn system),
  RGINF datetime/calendar, RNG/restock parity, level transitions remain
  out of scope / non-claims.
- Hero AP has no live model; loaded AP is preserved.

## 6. Acceptance

1. Unit `apply_live_save`:
   - HERO tile/direction and containers reflect a mutated `HeroLiveState`;
   - tail person tile/direction/HP/EP/containers reflect live simulation and
     inventory; dead person removed; PNAME matches;
   - RGINF latch patched, datetime raw;
   - VARS numeric/string values updated, names/order unchanged;
   - malformed HERO/tail → `format`, input payload unchanged on error.
2. E2E `glgame --load-save <slot> --write-save-slot <n> --save-dir <tmp>`:
   - written file parses, HERO/tail blocks are rebuilt (not the raw source
     bytes) and reloadable; source slots unchanged;
   - `game_save` uses the same path.
3. Full `ctest`; no game data committed.

## 7. Tests

- `tests/game/test_live_save_real.cpp` — load slot0 payload + level +
  restore, apply no-change, parse the result; HERO/tail person counts and
  tuples match (no byte claim).
- `tests/app/test_glgame_screenshot.cpp` — extend the existing write-slot
  test to parse the written file and assert HERO/tail blocks parse and match
  the expected person names/counts.

## 8. Non-goals

- No automatic level transitions, no DIARY/HSTRY, no c2 equipment runtime,
  no RNG parity, no new VARS names, no calendar update.
