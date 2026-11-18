# P2-15c3c — Selected tail live write (design)

Status: implemented (2026-11-18)
Task: `P2-15c3c` (implementation; follow-up to P2-15c3b, D-084)
Scope: `src/assets`, `src/game`, `tests/`, `docs/`; no `tools/` CLI changes
(integration is `P2-15c3f`).

## 1. Context

`P2-15c3a` (D-083) proved the selected tail format and the writer contract:
`FUN_14032c2c` writes `BT_PNAME` (alive standalone type-2 names in entity slot
order), `BT_RGINF` (`+0x26c0` latch + `+0x26a0` 8×u32 datetime) and
`BT_ENTS ` (type word + class body, terminator `1`, order = entity-manager
slot order). Type-2 bodies have a verified field map (`FUN_1401ec40` /
`FUN_1401e758`); types 8/16/32 are not simulated by the port. `P2-15c3b`
(D-084) provides the item/container writer and raw-carry item records.

This task writes the selected tail from live state using **raw-carry +
rebuild list**: loaded entities keep their order and raw bodies; matched
type-2 persons are patched from live state; dead persons are removed;
type-2-only and non-person entities stay raw; only explicit `new_persons`
(spawned since load) are appended. c2 is raw; the RGINF datetime is raw
(calendar blocked c3g).

## 2. Goal

`game::build_selected_tail(loaded, level, simulation, inventory, restore,
alive, rginf)` returns a new `assets::SaveTailEntry` whose section encodes as:

```
BT_PNAME : repeated (u32 len + cp1251 name), then u32 0
BT_RGINF : u32 latch + 0x20 datetime (raw from loaded)
BT_ENTS  : repeated (u32 type + body), then u32 1
```

and is byte-identical to the loaded section when nothing changed.

## 3. Data model

### 3.1 `SavePersonRecord` (game)

Add the fields currently skipped by `decode_save_person`:

- `field_40`, `field_44`, `field_654`, `field_f50`, `field_6cc` (u32);
- `field_658` (`0x20` raw), `field_ff0` (`0x11C` raw), `tail80` (`0x80` raw);
- `ap` (s32); `extra` (`vector<array<u8,0x14>>`), `tail_count` already exists;
- `containers` keep `SaveContainer`; add `flag` (u32) and `raw_items`
  (bytes after the flag: `u32 count + records`) so c2 can be re-emitted
  verbatim. c0/c1 use live item records.

`game::encode_save_person_body(const SavePersonRecord&)` writes the exact
writer order (name, `+0x40`, `+0x44`, current x/y, anchor x/y, `+0x654`,
`+0x658`, direction, route_type, route name, radius, delay min/max, tribe id,
dialog, inventory, `+0x6cc`, `+0xff0`, `+0xf50`, HP, EP, AP, route index,
3 containers, `tail80`, extra count + records). Strings are re-encoded to
cp1251 like `FUN_1400116c` (`u32 len + bytes`).

### 3.2 Tail section (assets)

Add `assets::encode_tail_section(const SaveRginf&, std::span<const std::string>
names, std::span<const SaveEntity> entities)`. It validates:
names non-empty and no NUL; entity body framing; writes the terminator.
No interpretation of entity bodies.

`SavedTailEntry` keeps `raw_len = section.size()` after encoding.

### 3.3 Restore mapping

`RestoredPerson` gains `entity_index` (index in `tail.entities`). The adapter
uses it to map loaded entities to live persons without re-matching.
`alive` is a caller-provided flag per `RestoredPerson` (explicit; the adapter
does not infer death).

## 4. Adapter

`game::build_selected_tail`:

1. Copy `rginf`: `latch = rginf.latch`, `datetime = loaded.rginf.datetime`.
2. Iterate `loaded.entities` with index `i`:
   - type 2 with a `RestoredPerson` whose `entity_index == i`:
     - `alive == false` → skip (removed from ENT and PNAME);
     - else build a `SavePersonRecord` from the loaded body, overwrite
       current tile/direction (`simulation.persons()[level_index]`), HP/EP/AP
       and route index (restored values), route/radius/delays/tribe/dialog/
       inventory from `level.persons[level_index]`, c0/c1 from
       `InventoryRuntime` (main/trade), c2 raw; encode body.
   - type 2 without a match (type-2-only) → raw body;
   - type 8/16/32 → raw body, unchanged position.
3. Append the explicit `new_persons` indices (spawned since load; caller-provided)
   that are not already represented by `restore.persons`: build a type-2 body
   from `PersonInstance` + simulation, c0/c1 from inventory, c2 empty/flag 0.
   Opaque blocks are zero (`+0x654=0`, `+0x658=0`, `+0xff0=0`, `+0xf50=0`,
   `+0x6cc=1`, `tail80=0`, no extra) — explicit non-claim. SEF-only persons
   that were not in the loaded save are **not** appended automatically.
4. PNAME = names of every type-2 record written (matched alive, raw
   type-2-only, appended), in the same order. Non-person types never
   contribute.
5. Duplicate names: `InventoryRuntime` is name-keyed, so live c0/c1 overlay is
   applied only when the name is unique among `level.persons`; duplicate
   names keep their decoded containers (occurrence-keyed live inventory is a
   non-claim).
6. Encode the section.

`PersonInstance` provides anchor/route/radius/delays/tribe (name→id via
`SaveTribeTable`), dialog/inventory. `route_type` string → numeric mapping
(`STAY`=0 … `RANDOM`=5); unknown → `format`.

## 5. Error handling

- Malformed loaded entity body, name NUL, empty name, container flag `0/1`
  reader parity (decode accepts any non-zero as present, encode normalizes to
  `1`), item/extra truncation, entity type `1` in the body list → `format`.
- Missing tribe id for a person with a non-empty tribe → `format` (or raw
  keep? choose `format`: a live person's tribe must resolve).
- Missing inventory entry → empty containers; missing simulation entry →
  `format` (level/restore mismatch).
- No partial output.

## 6. Acceptance

1. **Type-2 inverse:** all 14,507 type-2 bodies in 28 saves
   `encode(decode(body)) == body` (byte-identical), including opaque blocks,
   tail and containers.
2. **Tail identity:** all 28 selected sections and all 2,599 tail sections
   `encode(parse(section)) == section` with no changes (types 8/16/32 raw,
   PNAME/RGINF copied).
3. **Mutations:** move/turn, HP/EP/AP, c0/c1 add/remove/transfer, dead →
   record absent from ENT and PNAME, new person appended; `parse→reload`
   tuples 0/N (slot0/slot21), exact section consume, PNAME equals the written
   type-2 names.
4. **RGINF:** latch equals the caller value; datetime bytes equal the loaded
   datetime.
5. **Raw preservation:** type 8/16/32 bodies and type-2-only raw bodies are
   byte-equal; c2 bytes are byte-equal for untouched persons.
6. Full `ctest`; no CLI changes.

## 7. Tests

- `tests/game/test_save_person.cpp` — extended decode/encode unit cases and
  truncation; byte identity on a synthetic body.
- `tests/game/test_save_person_real.cpp` — 14,507 type-2 bodies byte identity
  and selected pins (real, skip 77).
- `tests/assets/test_tail_section.cpp` — encoder framing/errors.
- `tests/assets/test_tail_section_real.cpp` — 28 selected + all 2,599 sections
  byte identity (real, skip 77).
- `tests/game/test_tail_write.cpp` — adapter mutations, dead/new, PNAME,
  RGINF, c2 raw, non-person raw.

## 8. Non-goals / non-claims

- c2 is raw; no NPC equipment/appearance runtime.
- Opaque type-2 blocks (`+0x40`, `+0x44`, `+0x654`, `+0x658`, `+0x6cc`,
  `+0xff0`, `+0xf50`) are raw for matched persons and zero for appended
  persons.
- Attached hero list, DIARY/HSTRY, calendar datetime, RNG/restock parity
  (D-064), entity slot order for appended persons (append at end).
- CLI/`game_save` wiring (c3f).
