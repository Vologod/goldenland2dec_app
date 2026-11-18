# P2-15c3b — ITM effect → save item modifier mapping (research)

Status: complete (2026-11-17)
Phase: P2-15c3b (research step for the HERO live write; gate per spec)
Scope: `docs/`; scripts in `.local/research/c3b/` (not committed).

## 1. Provenance

| Artifact | SHA-256 |
|---|---|
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| `.local/research/c3b/modifier_map.py` | `47EC2C25FFA1326A61F53FF547E17105C64FD5273464E4B6E16C58D8705420FA` |
| 28-save manifest | `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |

ITM corpus: `items.pak` extracted with `glpak extract ... --prefix items/data/`
to `.local/research/c3b/itm/` (1,848 files). Save scan: 28 `slot*.sav`,
decompressed main stream, marker `0x14092248` (item class marker) then the
`0xC0 + u32 sub + 0x14*sub` record.

## 2. Question

`FUN_1404f4fc` (item creation from a definition, decompiled 54510-54522)
copies `+0x78` (modifier count) and `+0x7c` (modifier array) from the ITM
definition object, and `FUN_1404f4bc` (54510) serializes `0xC0 raw + u32 sub +
0x14*sub`. The ITM file stores each effect in **16 bytes**
(`type u32, amount u32, flags u32, duration_hi u8, duration_lo u8,
delay_hi u8, delay_lo u8`), while the save subrecord is **20 bytes** (5×u32).
The two extra words are produced by the runtime ITM loader; their values are
needed to write newly generated items.

## 3. Method

1. Parse all 1,848 extracted ITM files with the same field order as
   `src/assets/src/itm.cpp` (magic 4, type-conditional blocks, effects).
2. Scan all 28 saves for item records (marker, `+0xA8` id, `+0xC0` subcount).
3. For each record whose item definition has effects, align effect `i` with
   modifier `i` and require `word0/1/2 == type/amount/flags`.
4. Group by the effect tuple `(type, flags, duration_hi, duration_lo,
   delay_hi, delay_lo)` and record the observed `word3`/`word4`.

## 4. Result

| metric | result |
|---|---|
| ITM files parsed | 1,848 |
| item records found in 28 saves | 1,599 |
| records whose item has effects | 250 |
| matched `(effect, modifier)` pairs | 741 |
| misaligned pairs (`type/amount/flags` mismatch) | 0 |
| records with `effect_count == subcount` | 250/250 |
| distinct effect tuples | 81 |
| tuples with ambiguous `word3` | **0** |
| `word4` histogram | `0xFFFFFFFF` in 741/741 |

**Mapping (proven 741/741, 0/N):**

```
word0 = effect.type
word1 = effect.amount
word2 = effect.flags
word3 = duration_hi | (duration_lo << 8) | (delay_hi << 16) | (delay_lo << 24)
word4 = 0xFFFFFFFF
```

`word3` is exactly the four time bytes packed little-endian. Examples from
the corpus:

| effect | ITM time bytes | `word3` |
|---|---|---|
| type 3, flags 0x50 | dur (0,30), delay (0,0) | `0x00001E00` |
| type 3, flags 0x50 | dur (1,30), delay (0,0) | `0x00001E01` |
| type 9, flags 0x60 | dur (0,30), delay (1,0) | `0x00011E00` |
| type 6, flags 0x60 | dur (0,0), delay (0,30) | `0x1E000000` |
| type 27, flags 0x60 | dur (0,10), delay (0,20) | `0x14000A00` |
| type 11, flags 0x50 | dur (24,0), delay (0,0) | `0x00000018` |

## 5. Consequences for the port

- `ItemEffect` already stores `duration.hi/lo` and `delay.hi/lo`; no format
  change is needed to generate the 5 words.
- `game::ItemTemplateEffect` currently carries only `type/value/flags`; it must
  also carry the four time bytes so `InventoryRuntime`/`NpcLootGenerator` can
  build save-ready modifiers for generated items.
- Loaded items keep their raw modifier words; generated items use the formula
  above.
- The `word4 = 0` cases in the earlier all-record histogram
  (`p2-15b1b-restore.md` §Modifier subrecords) are records without matching ITM
  effects (special-generated or other runtime paths); this mapping covers the
  ITM-effect subset that c3b writes for new items.

## 6. Gate verdict

`implementable`. The ITM→save modifier mapping is deterministic on the corpus:
741/741 pairs, 0 misalignments, 0 ambiguous `word3`, constant `word4`. No
host/live capture is required. c3b may build generated-item modifiers from ITM
effects.

## 7. Reproduce

```powershell
& build\bin\glpak.exe extract "I:\Games\Златогорье-2" --prefix "items/data/" `
  --out .local\research\c3b\itm
py -3 .local\research\c3b\modifier_map.py
Get-FileHash -Algorithm SHA256 .local\research\c3b\modifier_map.py
```

## 8. Implementation result (2026-11-17)

The mapping is implemented as `game::item_effect_word3` and used by
`InventoryRuntime::add_item`, `NpcLootGenerator`, and
`save_item_from_inventory`. Tests: `gl_assets_tests`
`*item_record*,*hero save body*,*save_item*` 15 cases / 3,810 assertions;
`gl_assets_real_tests` `*ITM effects match*` re-walks 250 HERO records /
741 pairs / 0 mismatches; full `ctest` 19/19 (1533.46 s). D-084.
