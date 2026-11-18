# P2-15c3b — HERO live write (design)

Status: implemented (2026-11-17)
Task: `P2-15c3b` (implementation; follow-up to P2-15c3a, D-083)
Scope: `src/assets`, `src/game`, `tests/`, `docs/`; no `tools/` CLI changes
(integration is `P2-15c3f`).

## 1. Context

`P2-15c3a` (D-083) proved that the save writer is a snapshot and that HERO
item containers require a full inverse: the writer serializes
`0xC0 raw + u32 sub + 0x14*sub` per item, the container presence word is a
literal `1`/`0`, and the three HERO containers are `+0x0c/+0x10/+0x109c`.
HERO `0x000` is the current tile/direction, `0x128` is HP/EP/AP, `0x11c` and
`0x600` are raw blocks. `P2-15c3b` implements this HERO body write as a
library plus a game adapter; `c3f` later feeds it from `write_loaded_save_to`.

The ITM→save modifier mapping is a prerequisite: `docs/research/p2-15c3b-item-modifiers.md`
proves 741/741 matched effect pairs, 0 misalignments, 0 ambiguous word3,
`word4 = 0xFFFFFFFF`:

```
word0 = effect.type
word1 = effect.amount
word2 = effect.flags
word3 = dur_hi | dur_lo<<8 | delay_hi<<16 | delay_lo<<24
word4 = 0xFFFFFFFF
```

## 2. Goal

Given a loaded HERO body and the live port state, produce a `BT_HERO ` body
that:

1. is byte-identical to the loaded body when nothing changed (28/28 saves);
2. reflects live tile/direction, HP/EP/AP, and the three inventory containers;
3. preserves every byte of the save format the port does not model
   (`0x11c`, `0x600`, `+0x2bb4/+0x2bb8`, `0x80`/extra, attached list);
4. writes generated items with the proven ITM→modifier mapping.

## 3. Data model (assets)

### 3.1 `SaveItemRecord` / `parse_item_record`

- Add `std::array<std::uint8_t, 0xC0> raw{}` and `bool has_raw = false` to
  `SaveItemRecord`; `parse_item_record` copies the raw `0xC0` and sets
  `has_raw`.
- Add `encode_item_record(const SaveItemRecord&)` returning `Result<vector>`:
  - if `has_raw`, start from `raw` and patch `+0x08 type`, `+0x0C flags`,
    `+0x10 price`, `+0x14 weight_x10`, `+0x18 unknown_a`, `+0x20 style`,
    `+0x30 material`, `+0xA8 item_id`, `+0xB0 count`, `+0xB4 durability`,
    `+0x78` duplicate subcount and `+0xC0` subcount;
  - otherwise build the fixed record from the model (marker `0x14092248`,
    the same fields, `+0xB8`/`+0xBC` zero, remaining bytes 0);
  - write `u32 mod_count` and `mod_count*5` words from `modifiers`.
  - Unknown/derived fields (`+0xA4` instance id, `+0xAC` owner id) are
    `0` for generated records and raw-preserved for loaded records
    (non-claim in §8).
  - `mod_count` must equal `modifiers.size()`; larger than 65,535 → `format`.

### 3.2 `InventoryItem`

- Add `std::optional<SaveItemRecord> save_record` (or `raw` + `has_raw` +
  5-word modifier storage). The constructor keeps working; loaded items get
  the parsed record, generated items leave it empty.
- Keep the existing typed fields and `mods` for gameplay; the save encoder
  uses `save_record` when present and otherwise builds from the typed fields.
- `SpecialMod` gains two words (`word3`, `word4`) so a raw-loaded modifier can
  round-trip while gameplay keeps `id/value/flags`. Generated mods use the
  formula above (`word3` from time bytes, `word4 = 0xFFFFFFFF`).

### 3.3 HERO body model

New `gl/assets/hero_save_body.hpp`:

```cpp
struct HeroContainerState {
    std::uint32_t flag = 0;                 // literal 0/1 from the save
    std::vector<SaveItemRecord> items;
};
struct HeroSaveBody {
    std::uint32_t tile_x = 0, tile_y = 0, direction = 0;
    std::array<std::uint8_t, 0x11C> stat_block{};
    std::uint32_t hp = 0, ep = 0, ap = 0;
    std::array<std::uint8_t, 0x600> buffer600{};
    std::array<HeroContainerState, 3> containers;
    std::uint32_t field_2bb4 = 0, field_2bb8 = 0;
    std::array<std::uint8_t, 0x80> tail80{};
    std::vector<std::array<std::uint8_t, 0x14>> extra;
    std::uint32_t attached_count = 0;
    std::vector<std::uint8_t> attached_raw; // remaining bytes after the count
};
```

`parse_hero_save_body` consumes the body exactly (same framing as
`hero_scan.py`): 12 B header, `0x11C`, 12 B HP/EP/AP, `0x600`, 3 containers,
2×u32, `0x80`, `extra_count + 0x14*count`, attached count + bodies. For each
container it reads the flag (`0` = absent, no count; `1` = count + item
records); item records are parsed with `parse_item_record`.
`encode_hero_save_body` mirrors it and rejects inconsistent sizes
(`stat_block/buffer600/tail80` fixed arrays are structural; attached is raw
bytes copied verbatim).

### 3.4 Item conversion

- `SaveItemRecord` → `InventoryItem`: `item_id`, `count`, `flags`, `type`,
  `style`, `durability`, `mod_count`, `mods` (id/value/flags from words 0..2,
  word3/word4 preserved), `save_record = record`.
- `InventoryItem` → `SaveItemRecord` for writing: if `save_record` exists,
  copy it and patch known fields; else build from typed fields + `mods`
  (word0/1/2 from id/value/flags, word3 from the stored time bytes if
  available, else 0, word4 = `0xFFFFFFFF`).

## 4. Game adapter

New `gl/game/hero_save_write.hpp`:

```cpp
struct HeroLiveState {
    std::uint32_t tile_x = 0, tile_y = 0, direction = 0;
    std::uint32_t hp = 0, ep = 0, ap = 0;
    std::array<std::vector<gl::assets::InventoryItem>, 3> containers;
    std::array<bool, 3> present{true, true, true};
};gl::assets::HeroSaveBody make_hero_save_body(const gl::assets::HeroSaveBody& loaded,
                                             const HeroLiveState& live);
```

`make_hero_save_body` copies the raw blocks from `loaded`, overwrites
tile/direction, HP/EP/AP, container presence and items, and keeps
`+0x2bb4/+0x2bb8`, `0x80`/extra and attached raw. It converts the three
runtime containers (hot/items/equipped) through §3.4.

`ItemTemplateEffect` (game) gains `duration_hi/lo`, `delay_hi/lo`;
`make_item_template` (glgame) and `NpcLootGenerator` fill them from
`assets::ItemEffect`, and generated `SpecialMod` values get
`word3 = time bytes`, `word4 = 0xFFFFFFFF`.

## 5. Error handling

- Truncated/short HERO body, container count without flag, container flag
  outside 0/1, item record shorter than `0xC4`, subcount out of range,
  extra count/record truncation, attached count truncation → `format`.
  Attached bodies are stored as raw remaining bytes (no per-body parse), so
  an attached-body cut is not distinguishable from a shorter raw tail.
- `encode_hero_save_body` rejects `mod_count != modifiers.size()`, item
  records > `0xFFFF` modifiers, fixed-array size is compile-time.
- No partial output on error; no filesystem I/O in `assets`/`game`.

## 6. Acceptance criteria

1. **Byte identity, no changes:** for all 28 saves,
   `encode_hero_save_body(parse_hero_save_body(body)) == body` (byte-equal).
2. **Item round-trip:** all 1,599 HERO item records parse→encode byte-equal;
   the same for the entity-container corpus (115,405 records) as a shared
   decoder test.
3. **Mapping:** generated item modifiers equal the ITM formula; a corpus test
   re-encodes 741/741 matched pairs with 0 mismatches.
4. **Mutations:** tile/direction, HP/EP/AP, add/remove/change count/durability,
   container swap → `encode→parse` returns the mutated values; a save body
   built from a mutated state re-parses and matches the expected tuple.
5. **Adapter:** `make_hero_save_body(loaded, live)` with no live changes is
   byte-identical to `loaded`; with changes only patches the live fields.
6. **Regressions:** full `ctest` with `GL_GAME_DIR`; existing `gl_assets`,
   `gl_game`, real and E2E tests unchanged/passing.

## 7. Test files

- `tests/assets/test_save_item.cpp` — raw carry, encode, generated record,
  5-word mods, error cases. Add a real 1,599/115,405 corpus case to
  `tests/assets/test_save_payload_real.cpp` (or a new real file).
- `tests/assets/test_hero_save_body.cpp` — parse/encode framing, containers,
  presence, truncation, byte identity on a synthetic body.
- `tests/assets/test_hero_save_body_real.cpp` — 28/28 byte identity and
  item-record round-trip (real, skip 77).
- `tests/assets/test_item_modifier_map_real.cpp` — 250 HERO records / 741 ITM
  effect pairs, 0 mismatches (real, skip 77).
- `tests/game/test_hero_save_write.cpp` — adapter, live mutations, generated
  items, mapping.
- `tests/assets/CMakeLists.txt`, `tests/game/CMakeLists.txt` — add files.

## 8. Non-goals / non-claims

- No CLI/`game_save` wiring (c3f).
- No NPC containers/`BT_ENTS` write (c3c); the item encoder is shared but
  not wired into tail entities here.
- No claim for `+0xA4` instance id / `+0xAC` owner id of generated items
  (reader reassigns them); no byte-parity claim for `0x11c`, `0x600`,
  `0x80`/extra, attached (raw only).
- No original-parity claim for special-added modifiers outside the ITM-effect
  subset (`word4 = 0` cases are not produced by this mapping).
- No new save header/tail/VARS behavior.

## 9. Risks

- `InventoryItem` gains a raw record; trade/inventory code must keep both
  representations consistent. The adapter always writes the typed fields from
  the live item, so stale raw values cannot override a mutation.
- `SaveItemRecord` raw bytes increase memory by ~192 B per item; the HERO and
  entity corpora are small (1,599 + 115,405), acceptable.
- Generated modifier `word4` is proven only for ITM effects; specials-added
  mods keep the port's current 3-word values and are excluded from parity.
