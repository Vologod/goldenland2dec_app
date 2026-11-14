# P2-15b1b1 — Restore person fields from save Design

Status: draft for approval (2026-11-14)
Phase: P2-15b1b1 (first restore implementation; research-backed)
Layers: `assets` (BT_TRIBE names) + `game` (decoder/restore) + `tools/glgame`

## 1. Goal

Применить type-2 saved persons к загруженному уровню: match by
name-occurrence, порядок BT_ENTS, поля person/route/HP/EP; SEF-only persons
сохранить (drop gated), type-2-only 3 записи skip. Containers/inventory и
NPC-loot gate — b1b2/b1b3.

## 2. Evidence

`docs/research/p2-15b1b-restore.md`:

- Type-2 field map: name/+0x40/+0x44, current tile pair, anchor `+0x5f8/+0x5fc`,
  direction, route type `+0x151c` (0..5), route name `+0x1524`, radius
  `+0x1520`, delay min/max `+0x2668/+0x266c`, tribe id `+0x150c`, dialog
  `+0x267c`, inventory `+0x2aac`, HP/EP `+0x14fc/+0x1500`, route index
  `+0x1630`; containers `+0xc/+0x10/+0x109c` (b1b2), tail `+0x110c`.
- SEF identity: anchor/route/radius/delays/tribe/name 496/496; direction
  309/496; PNAME==type2 2599/2599.
- Restore: SEF persons are NOT read by the save reader; BT_ENTS order is
  authoritative. 496 matched, 43 SEF-only, 3 type-2-only (gated).
- BT_TRIBE: `u32 count, count×(u32 id + string), count² matrix`; id→name
  mapping corpus/SEF 496/496.
- HP/EP current values at `+0x14fc/+0x1500`; AP has no port parameter.

## 3. Scope

Входит:

- `game::SavePersonRecord` decoder for type-2 body (fields above).
- `assets::SavePayload` tribe names (`id→name`) from `BT_TRIBE`.
- `game::restore_save_persons(Level&, const SaveTailEntry&, const SaveTribeTable*)`
  with match/order/field apply; SEF-only keep (drop gate flag default false),
  type-2-only skip (gate false).
- `PersonInstance.saved_life/saved_energy` optional; `PersonRuntime::create`
  uses them for NPC `life`/`energy`.
- `LevelSimulation::restore_person(index, current_tile, direction, route_index)`
  to apply current runtime state after create without resetting anchor.
- `glgame load_save`: apply restore in `on_level_loaded` after
  `generate_npc_inventories` and before `LevelSimulation::create`; logs
  `save restore: matched=... sef_only=... type2_only=...`.
- Tests: synthetic + real slot0/2/21 + E2E.

Не входит:

- Containers/inventory restore (b1b2), NPC loot `+0x26c0` gate/order (b1b3),
  type-2-only synthesis (gated), `+0x40/+0x44/+0x654/+0x658/+0x6cc/+0xff0`
  opaque fields, AP, animation/UI, save write.

## 4. API

`src/game/include/gl/game/save_person.hpp`:

```cpp
struct SavePersonRecord {
    std::string name;
    std::int32_t current_tile_x = 0, current_tile_y = 0;
    std::int32_t anchor_tile_x = 0, anchor_tile_y = 0;
    std::uint8_t direction = 0;
    std::int32_t route_type = 0;       // 0..5
    std::string route_name;
    std::int32_t radius = 0;
    std::int32_t delay_min = 0, delay_max = 0;
    std::int32_t tribe_id = 0;
    std::string dialog;
    std::string inventory;
    std::int32_t hp = 0, energy = 0;
    std::int32_t route_index = 0;
};

Result<SavePersonRecord> decode_save_person(const gl::assets::SaveEntity &entity);

struct RestoreCounts { std::size_t matched, sef_only, type2_only, skipped; };
struct RestoredPerson {
    std::size_t level_index;   // index in restored Level.persons
    Tile current;
    std::uint8_t direction;
    std::int32_t route_index;
};
struct RestoreResult {
    RestoreCounts counts;
    std::vector<RestoredPerson> persons;
};
RestoreResult restore_save_persons(gl::game::Level &level,
                                   const gl::assets::SaveTailEntry &tail,
                                   const gl::assets::SaveTribeTable *tribes,
                                   bool drop_sef_only = false);
```

`RestoreResult.persons` retains per-matched current tile, direction, and route
index so `glgame` can call `LevelSimulation::restore_person` after
`LevelSimulation::create` without re-decoding the tail.

`src/assets/include/gl/assets/save_payload.hpp`:

```cpp
struct SaveTribeEntry { std::uint32_t id; std::string name; };
struct SaveTribeTable { std::vector<SaveTribeEntry> entries; };
```
Split `BT_TRIBE` parser (count/entries/matrix) from `parse_save_payload`:
store `SaveTribeTable tribes` in `SavePayload`; matrix ignored (b1c).

`PersonInstance`:
```cpp
std::optional<std::int32_t> saved_life;
std::optional<std::int32_t> saved_energy;
```
`PersonRuntime::create` uses `saved_life/energy` when present (else def
`life`/`energy`).

`LevelSimulation`:
```cpp
void restore_person(std::size_t index, Tile current, std::uint8_t direction,
                    std::int32_t route_index);
```
Sets `movement.x/y` from tile (`x*12+6`, `y*9+4`), direction8/16,
`state.route_index`, clears path/goal/waiting (restored state starts idle).

## 5. Restore algorithm

1. Collect type-2 entities in `tail.entities` order (decode; skip type 4/unknown
   — parser already rejects unknown).
2. Build SEF occurrence queues by name.
3. For each saved record in order: pop k-th SEF person with same name; if none
   → type2_only++, skip (gated).
4. Matched persons: set `PersonInstance`:
   name (already), anchor `tile_x/y`, radius, delay_min/max, route_type
   string, direction, dialog, inventory, `saved_life/energy`.
   Route name: set `person.route`; keep SEF `route_cells` when route name
   matches (57/57 observed); if it differs, keep SEF cells and log
   `unverified route_cells` (b1c).
5. `level.persons = matched (BT_ENTS order) + SEF-only (if !drop_sef_only)`.
6. After `LevelSimulation::create`: for each `RestoreResult.persons` entry, call
   `restore_person(level_index, current, direction, route_index)`.
7. Log counts; type-2-only and drop-gate are explicit.

Ordering in glgame: `on_level_loaded` runs `generate_npc_inventories` first
(unchanged loot RNG/SEF order), then restore persons, then
`LevelSimulation::create` uses restored order/fields.

## 6. Tests

Unit (`tests/game/test_save_restore.cpp`):
- synthetic Level with 3 SEF persons + tail with 2 matched type-2 + 1
  type-2-only; assert matched order, fields (tile/anchor/direction/route/
  radius/delays/tribe name/dialog/inventory/HP/EP/route index), SEF-only
  kept appended, type-2-only skipped; drop gate variant.
- saved order differing from SEF order keeps BT_ENTS order; `RestoreResult`
  level index/current tile/direction/route index match the decoded records.
- duplicate names k-th match; malformed type-2 body → error; unknown route
  type falls back to `STAY`.
- PersonRuntime saved HP/EP; LevelSimulation restore_person position/direction/
  route_index.

Real (`tests/game/test_save_restore_real.cpp`):
- load the selected section of each of the 28 saves and aggregate the restore
  counts: 496 matched, 43 SEF-only, 3 type-2-only skipped, total 539; pinned
  persons fields from research (slot0/2/21); no containers.
- independent reference values from research scan; no port-copy.

App (`tests/app/test_glgame_screenshot.cpp`):
- `--load-save 0` (slot0, level `L93_4`) log
  `save restore: matched=3 sef_only=0 type2_only=0`; restored Guardian2 fields
  (both duplicates) via `--dump-npc`/log.

Full `ctest` with `GL_GAME_DIR`; docs/STATUS/PROJECT/parity/D-072.

## 7. Acceptance

1. Type-2 decoder maps all fields listed; malformed/truncated → format.
2. Restore: slot0 per-slot 3 matched + 0 SEF-only + 0 type-2-only skipped;
   28-save corpus aggregate 496 matched + 43 SEF-only + 3 skipped; BT_ENTS
   order; no unconditional SEF-only drop.
3. Fields/anchor/current tile/direction/route/radius/delays/tribe/dialog/
   inventory/HP/EP/route index match save (person tuple 0/N).
4. PersonRuntime NPC HP/EP from save; simulation restore current state.
5. `--load-save 0` applies restore before simulation create; loot unchanged
   in this increment.
6. Full ctest green; docs updated.

## 8. Risks

- `route_cells` for a saved route name differing from SEF is unresolved;
  b1b1 keeps SEF and logs.
- `tribe` numeric id→name requires BT_TRIBE parse; if table missing → keep
  SEF tribe + warning.
- HP/EP clamp to maxima unverified; restore raw and log if > max (O7 waiver).
- Reordering simulation while loot uses SEF order is a deliberate b1b1
  boundary; b1b3 owns loot order/gate.
- PersonInstance optional fields must default nullopt for New Game.

## 9. References

- `docs/research/p2-15b1b-restore.md` §Type-2 fields/§Restore/§Port/§Oracle.
- `docs/research/p2-15b1-save-entities.md` §Body layout.
- D-071.
