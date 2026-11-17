# P2-15c3a — Save write-back research

Status: complete (2026-11-17)
Phase: P2-15c3a (research-only; follow-up to P2-15c4b, D-082)
Scope: `docs/` only; `src/`/`tests/`/`tools/` unchanged.
Spec: `docs/specs/2026-11-17-p2-15c3a-write-back-research-design.md`
Plan: `docs/plans/2026-11-17-p2-15c3a-write-back-research.md`

## 1. Provenance

| Artifact | SHA-256 |
|---|---|
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| `Saves/slot0.sav` | `8AC545FE9715EF11C68B1CCA8617BEB03003ECA360F07F71C26E3FC5E5C73442` |
| 28-save manifest | `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |
| `.local/research/c3a/reencode.py` | `6D2AA3AEE2F7A6134261CB65ACBF75664C571310D212BCDAB3532E82DF2D5586` |
| `.local/research/c3a/hero_scan.py` | `FAD291C78587C0281A0E2BB101B0717D028F3E9BEB3683AF03EC886796460C60` |

Sources: `I:\Games\Златогорье-2\Saves` (28 files `slot0.sav`…`slot27.sav`);
`.local/decomp/server/decompiled.c` (ExportDecomp of `Server.dll`);
`llvm-objdump` for the writer functions that are not in `decompiled.c`
(`FUN_1401ec40`). Scripts are kept in `.local/research/c3a/` and are not
committed; outputs are in `.local/research/c3a/out/`.

## 2. Question and result in one paragraph

The current writer is a **snapshot**: it rebuilds the seven blocks from the
loaded decompressed stream, re-encodes `BT_VARS`/`BT_TRIBE` from the loaded
values, patches the header, and copies `BT_HERO`, all tail sections and every
container body verbatim. The research question was to decide, per domain,
whether this **patch-in-raw** approach can carry live campaign state or
whether a **full inverse** of the original writer is required.

**Verdict: hybrid.** Raw preservation is sufficient for every domain the port
does not model (opaque type-2 blocks, HERO `0x600`/`0x80`/extra/attached, ENT
types 8/16/32, DIARY/HSTRY, `BT_SCRN`); these rows are `raw`, and the rows
whose semantics are unresolved at all are `blocked`. **Full inverse is
required for four variable-length live domains**: HERO item containers, the
selected tail `BT_ENTS`/`BT_PNAME` list, the tail dictionary across level
changes, and the `BT_INFO` level id. Two live-state prerequisites are missing
and block parity: a **game calendar** (`BT_INFO` `0x20` world time, `BT_RGINF`
datetime) and a **persistent entity-latch value** (`BT_RGINF` `+0x26c0`).
`BT_VARS` wire-up from the live VM is also missing even though the encoder
exists.

## 3. Method

1. Static writer/reader contract: block order and every writer source object
   from `decompiled.c` plus `llvm-objdump` disassembly of `FUN_1401ec40`
   (type-2 writer, absent from `decompiled.c`).
2. Baseline re-encode of all 28 saves with the current writer policy
   (`reencode.py`): semantic equality, header diffs, zlib length deltas,
   first byte diff, independent `BT_VARS`/`BT_TRIBE` re-encode equality.
3. HERO corpus scan (`hero_scan.py`): exact body consume, three containers,
   tail/extra/attached counts, `0x600` halves, item records/mods.
4. Per-domain verdict matrix and live-scenario cross-check against the port.

## 4. Static writer/reader contract

Writer `FUN_14031b64` (RVA `0x31b64`, size 1445) is the save entry point.
Reader `FUN_1403210c` (RVA `0x3210c`, size 1261) is its inverse. Decompiled
line numbers below are from `decompiled.c`; VAs are `Server.dll` addresses
(RVA = VA − `0x14000000`).

### 4.1 Block order and sources

| Block | Writer | Reader | Source / notes |
|---|---|---|---|
| header `0x68` | `FUN_14031b64` 35423-35475 | `FUN_1403210c` 35600-35603 | version `0x11` (35428), `GetLocalTime` (35450), title/username buffers; `patch_save_header` (D-081) covers version/date/weekday/title |
| `BT_SCRN ` | 35384-35385 | skipped by tag+size | client vtable `+0x50(500, 0x177)` → 500×375 preview; server never reads it |
| `BT_INFO ` | 35386-35396 | 35628-35639 | level id `DAT_1407fcc8+4`, world time `DAT_14088e14+0x18` (`0x20`), two strings `DAT_14088e30+8/+0x54`, party level `this+0x26c4` (`pushl 0x26c4(%eax)` at `0x14031d0a`, `%eax = this`), 8 bytes `this+0x26c8` |
| `BT_VARS ` | `FUN_14002324` 1383-1416 | `FUN_140023f0` 1424-1456 | script globals; encoder done (D-080) |
| `BT_TRIBE` | `FUN_14016828` 16955-16990 | `FUN_140168e4` 16996-17032 | id/name table + `count²` matrix; encoder done (D-080) |
| `BT_DIARY` | `FUN_14058768` 60171-60226 | `FUN_140588c8` 60232-60358 | framing only (D-078); field semantics blocked (b4c) |
| `BT_HSTRY` | 35404, host object `iVar3+0x28` vtable `+4` | 35686, vtable `+0` | ring ordering blocked (b4d) |
| `BT_HERO ` | `FUN_1401e690` 21880-21899 + `FUN_1401e340` 21743-21774 | `FUN_1401e424` 21780-21874 + 35670-35684 | see §4.2 |
| tail dictionary | `FUN_14032c2c` 35937-36130 | 35604-35626 | see §4.4 |

Reader order is asymmetric for the last two blocks: reader reads
`BT_HERO ` (35670) and the attached list (35673-35684) **before** `BT_HSTRY`
(35685); writer writes `BT_HSTRY` (35403-35404) **before** `BT_HERO `
(35405-35422). `encode_save_payload` follows the writer order (D-079).

### 4.2 `BT_HERO ` body (body-relative)

The writer sequence is proven by disassembly `0x1401e690`:

| offset | size | source | reader | port model |
|---|---|---|---|---|
| `0x000` | 3×u32 | `+0x604` cur tile x, `+0x608` cur tile y, `FUN_14020e64` direction | 21806-21808 + `FUN_140222d0` 21868 | `HeroController` tile + direction |
| `0x00C` | `0x11C` | raw `+0xff0` state block | 21809 | `HeroStats` (9 stats + 27 skills); unknown remainder |
| `0x128` | 3×u32 | `+0x14fc` HP, `+0x1500` EP, `+0x1504` AP | 21810-21815 | `HeroStats.life/energy/extra`; AP not restored |
| `0x134` | `0x600` | raw `+0x1510` buffer | 21816-21818 | none |
| `0x734` | 3 containers | `+0x0C`, `+0x10`, `+0x109C` via `FUN_1405c9d8` | 21824-21829 via `FUN_1405ca18` | `InventoryRuntime` hot/items/equipped |
| after containers | 2×u32 | `+0x2bb4`, `+0x2bb8` | 21830-21833 | none |
| next | `0x80` | 4×`0x20` at `+0xf5c/+0xf7c/+0xf9c/+0xfbc` (`FUN_1401d84c`) | 21834 `FUN_1401d8bc` | none |
| next | `4 + 0x14*c` | `+0x14f4` count + `+0x14f4*0x14` at `+0x110c` | 21277-21292 | none |
| last | `4 + list` | `+0xf44` attached count; each `+0xf34[i]` resolved in the entity manager and serialized via vtable[0] (`FUN_1401e340`) | 35673-35684: create type-2, vtable[1] reader, mark `+0x2bc0=1`, `+0x3ca=0` | none |

`FUN_14020e64` returns `+0x28` unchanged when `+0x594 == 0` (the normal case;
`FUN_140222d0` sets `+0x594 = 0` on load); for `+0x28` values 10..25 and a
non-zero `+0x594` it collapses to 0..7. The HERO direction is therefore the
runtime facing index.

### 4.3 Type-2 entity body (`BT_ENTS` record after the u32 type)

Type-2 writer `FUN_1401ec40` (RVA `0x1ec40`, disassembly; not exported to
`decompiled.c`) and reader `FUN_1401e758` (21905-22096) agree field by field:

| # | field | source | port model |
|---:|---|---|---|
| 1 | name (len+cp1251) | `+0x4c` (std::string `+0x48`) | `PersonInstance.name` |
| 2 | `+0x40` | literary-name id | none |
| 3 | `+0x44` | literary-name/role mode | none |
| 4 | `+0x604` | current tile x | `LevelSimulation` movement |
| 5 | `+0x608` | current tile y | `LevelSimulation` movement |
| 6 | `+0x5f8` | anchor tile x | `PersonInstance.tile_x` |
| 7 | `+0x5fc` | anchor tile y | `PersonInstance.tile_y` |
| 8 | `+0x654` | unknown flag (0 in corpus) | none |
| 9 | `+0x658` raw `0x20` | prototype/runtime block | none |
| 10 | direction | `FUN_14020e64` | `direction8` |
| 11 | `+0x151c` | route type enum | route type string |
| 12 | `+0x1528` string | route name (std `+0x1524`) | `PersonInstance.route` |
| 13 | `+0x1520` | radius | `radius` |
| 14 | `+0x2668` | delay min (ms) | `delay_min` |
| 15 | `+0x266c` | delay max (ms) | `delay_max` |
| 16 | `+0x150c` | tribe id | tribe name only |
| 17 | `+0x2680` string | dialog script (`+0x267c`) | `dialog` |
| 18 | `+0x2ab0` string | inventory script (`+0x2aac`) | `inventory` |
| 19 | `+0x6cc` | unknown flag (1 in corpus) | none |
| 20 | `+0xff0` raw `0x11C` | person state block | none |
| 21 | `+0xf50` | written, **discarded** by reader (22067) | none |
| 22 | `+0x14fc` | HP | `PersonState` life |
| 23 | `+0x1500` | EP | `PersonState` energy |
| 24 | `+0x1504` | AP | none (`PersonParameter` gap) |
| 25 | `+0x1630` | route waypoint index (signed) | no setter |
| 26 | 3 containers | `+0x0C`, `+0x10`, `+0x109C` | main/trade/structure |
| 27 | tail `0x80 + 4 + 0x14*c` | `+0xf5c/+0xf7c/+0xf9c/+0xfbc`, `+0x14f4`, `+0x110c` | none |

Item record writer `FUN_1404f4bc` (54510-54522) is `0xC0 raw + u32 subcount
(+0x78) + 0x14*sub` from `+0x7c`; the container writer `FUN_1405c9d8`
(63011-63024) writes a literal `1` when the container runtime id is not `-1`
and `0` otherwise, then `FUN_1404c7a8` writes the used-slot count and each
used item (occupancy-bit order). The item map is the one proven in
`p2-15b1b-restore.md` §Item record map (`+0x00` marker `0x14092248`, `+0x08`
type, `+0x0C` flags, `+0x10` price, `+0x14` weight, `+0x18` unknown_a,
`+0x20` style, `+0x30` material, `+0xA4` instance id, `+0xA8` definition id,
`+0xAC` owner container, `+0xB0` count, `+0xB4` durability, `+0xB8`/`+0xBC`).

### 4.4 Tail dictionary and per-level snapshot (`FUN_14032c2c`)

`FUN_14032c2c` is decompiled 35937-36130 (RVA `0x32c2c`, size 1511).

- `BT_PNAME` (35980-36039): no count; for each entity in entity-manager slot
  order, include the name when `(flags & 0x3e) == 2` (type 2),
  `+0x150 == 0` (alive), and `FUN_140253fc(level, entity) == 0` (not attached
  to the hero); terminated by an empty name (36039).
- `BT_RGINF` (36040-36042): `u32` latch from `level+0x26c0`, then `0x20` bytes
  copied from `level+0x26a0` (8×u32 calendar datetime).
- `BT_ENTS ` (36043-36129): for each entity in slot order, include when
  `(flags & 0x3a) != 0` (types 2/8/16/32) and `+0x54 == 0` (alive; same offset
  as `+0x150`); attached type-2 persons are skipped (36098). Each record is
  `u32 type` + `vtable[0]` body; the list is terminated by type `1`
  (36069-36070).
- The dictionary itself is `level+0x26d8`: writer 35479 iterates
  `FUN_14005c1c` keys and writes `name + section` (35484-35489); sections are
  per-level zlib blobs. `FUN_14032c2c` updates **only the current level entry**
  (36083-36089: `FUN_14006040` lookup, mark old, `FUN_14005e78` insert); the
  other visited-level sections stay in the dictionary unchanged. This answers
  OQ-C3-3: the save writer dumps all visited levels, but a single snapshot call
  refreshes one level.

## 5. Baseline re-encode (28 saves)

`reencode.py` parses each save, rebuilds it with the current policy (header
bytes kept, main stream `tag+u32+body`, zlib level 6, tail sections
re-compressed), re-parses the rebuilt file, and separately re-encodes
`BT_VARS`/`BT_TRIBE`: numeric values are decoded from the saved f64 and
re-packed as f64, names/ids/matrix are re-emitted from the parsed fields.

| metric | result |
|---|---|
| slots | 28 |
| main decompressed stream equality (rebuilt file re-parsed) | 28/28 |
| tail entry name/order equality (rebuilt file re-parsed) | 28/28 |
| tail section raw equality (rebuilt file re-parsed) | 28/28 |
| `BT_VARS` typed byte equality (decode f64 → re-pack) | 28/28 |
| `BT_TRIBE` typed byte equality (ids/names/matrix re-emitted) | 28/28 |
| header byte diffs, control (no patch applied) | 0 |
| raw length total | 24,331,789 B |
| original main-stream compressed total | 6,780,334 B |
| re-compressed main-stream total | 6,874,158 B |
| main-stream compressed delta | +93,824 B (+1.38%), per-slot 1,468..8,560 B |
| first byte diff | `0x6c` (27 saves) / `0x6d` (1 save), compressed-length word |
| tail entry counts | 2..213 per slot |

Interpretation: the snapshot writer already reproduces every loaded byte
**semantically** and is deterministic for `BT_VARS`/`BT_TRIBE`; the only
byte-level difference is zlib output length. The gap is not in re-encoding the
loaded payload but in the absence of any path from live state into the HERO,
tail, DIARY and HSTRY bodies.

## 6. HERO corpus scan (28 saves)

`hero_scan.py` parses the `BT_HERO ` body exactly as §4.2 (including
`0x11c`, `0x600`, three containers, `0x80`, extra records and the attached
list; item bodies are walked as `0xC0 + 4 + 0x14*sub`).

| metric | result |
|---|---|
| exact body consume | 28/28 |
| body size range | 2,404..35,012 B |
| attached list count | `0` in 28/28 |
| extra record count | `0` in 27/28; `1` in slot16 |
| non-empty containers | 80/84 |
| item records in HERO containers | 1,599 |
| modifier subrecords | 924 |
| item marker `0x14092248` in non-empty containers | 80/80 containers |
| `0x600` second half non-zero | 26/28 slots |

The `0x600` second half result is significant: the reader zeroes
`hero+0x1510+0x200` after loading (21819-21823), yet 26 of 28 saved bodies
carry non-zero bytes there. The buffer is runtime-populated, not a stable
copy; raw preservation of a loaded save is therefore **not** the original
writer's behavior.

Selected pins: slot0 HERO `(123,131,1)`, HP/EP/AP `263/0/5`, containers
`8/117/11`; slot21 `(187,158,3)`, `34/33/26`, containers with
`0` in c0/c2; slot16 is the only extra-record slot.

## 7. Write-back matrix

Statuses: **available** = decoder/writer path and live port state exist;
**partial** = path exists but live state or inverse field is missing;
**missing** = no port state.

| Domain | SavePayload / port state | Writer contract | Corpus evidence | Verdict | Follow-up | Oracle |
|---|---|---|---|---|---|---|
| header (version/date/title) | `SaveHeader` / wall clock | 35423-35475 | 28/28 parse; version `0x11` | `patch-in-raw` (done) | — | header words patched (D-081) |
| `BT_SCRN ` | raw block / none | 35384-35385 (client preview) | server ignores | `raw` (stale preview non-claim) | optional preview capture | none (non-claim) |
| `BT_INFO ` level id | `info.level_id` / level id | `DAT_1407fcc8+4` | 28/28 | `raw` when unchanged; `full-inverse` on level change | c3e | reparse level id |
| `BT_INFO ` time `0x20` | `info.time` / **no calendar** | `DAT_14088e14+0x18` | real 8×u32 datetime | `blocked` (no game clock) | c3g; next: same 8-word decoder as `BT_RGINF` (`p2-15b1-save-entities.md` §RGINF), live-diff one save cycle | datetime 0/N |
| `BT_INFO ` `+0x218` | `info.field_218` / none | 35388-35390 | 28/28 | `raw` (semantics unverified) | — | byte-equal |
| `BT_INFO ` party level `+0x26c4` | `info.party_level` / load-only | `this+0x26c4` (`0x14031d0a`) | 28/28 | `patch-in-raw` when live party level exists; currently `raw` | c3f | reloaded party level |
| `BT_INFO ` `+0x26c8` | `info.field_26c8` / none | `this+0x26c8` (`0x14031d26`) | 28/28 | `raw` | — | byte-equal |
| `BT_VARS ` | `vars` + ScriptVm globals | `FUN_14002324` | 879..1006 vars (slot0 1002), 28/28 re-encode | `full-inverse` encoder done; **live wiring missing** | c3f; call `update_vars_from_globals` | var tuples 0/N |
| `BT_TRIBE` | `tribes` | `FUN_14016828` | 234 entries, 28/28 re-encode | `full-inverse` done; matrix mutations unmodeled → raw | c3f | relation tuples 0/N |
| `BT_DIARY` | framing raw / no runtime journal | `FUN_14058768` | 1,013 entries | `blocked` (b4c) | c3d; next: b4c field decode `FUN_140591b8`/`FUN_14059f6c` | entry tuples 0/N |
| `BT_HSTRY` | framing raw / no runtime log | host vtable | 40 slots | `blocked` (b4d) | c3d; next: b4d host ring `GoldenLand.exe DAT_140891bc+0xcc` | slot tuples 0/N |
| HERO `0x000` tile/direction | `HeroController` tile/direction | `+0x604/+0x608/+0x28` | 28/28 | `patch-in-raw` | c3b | hero tuple |
| HERO `0x00C` `0x11C` | `HeroStats` (9 stats + 27 skills); rest unknown | raw `+0xff0` | 28/28 | `raw` now; `patch-in-raw` for known fields when stats mutate | c3b; next: map block offsets 0x24/0x28 and 0x98..0x11B | hero stats tuple |
| HERO `0x128` HP/EP/AP | `HeroStats.life/energy/extra` | `+0x14fc/+0x1500/+0x1504` | 28/28 | `patch-in-raw` | c3b | HP/EP/AP tuple |
| HERO `0x134` `0x600` | none | raw `+0x1510` | second half non-zero 26/28 | `blocked` (runtime buffer semantics) | c3b; next: decompile producers/consumers of `hero+0x1510` (search refs to `+0x1510`), live-diff one save cycle | buffer 0/N |
| HERO containers | `InventoryRuntime` `hot/items/equipped` | `FUN_1405c9d8` | 1,599 records, 924 mods | `full-inverse` (raw record carry + rebuild) | c3b | container tuples 0/N |
| HERO `+0x2bb4/+0x2bb8` | none | 2×u32 | 28/28 | `raw` | c3b | byte-equal |
| HERO `0x80` + extra | none | `FUN_1401d84c` | extra 0/1 | `raw` | c3b; next: `FUN_1401d8bc` consumers / `0x110c` array semantics | byte-equal |
| HERO attached list | none | `FUN_1401e340` | count 0 28/28 | `raw` (non-claim) | c3b; next: attached-list producer `+0xf34`/`+0xf44` | byte-equal |
| tail dictionary names/order | `tail[]` / loaded only | 35479-35491 | 2..213 entries | `raw` for unchanged; `full-inverse` on level change | c3e | name/order 0/N |
| `BT_RGINF` latch | `SaveRginf.latch` / decision not persisted | `+0x26c0` | latch 0/1 1920/679 | `patch-in-raw` + persist latch | c3c | latch/decision 0/N |
| `BT_RGINF` datetime | `SaveRginf.datetime` / **no calendar** | `+0x26a0` | real 8×u32 date | `blocked` (c3g) | c3g; next: live-diff one save cycle | datetime 0/N |
| `BT_PNAME` | loaded names / `Level.persons` | 35980-36039 | 2,599 sections | `full-inverse` (alive standalone order) | c3c | name list 0/N |
| `BT_ENTS ` type 2 | raw body / restored persons | `FUN_1401ec40` | 14,507 records | `full-inverse` per record | c3c | person tuples 0/N |
| `BT_ENTS ` types 8/16/32 | raw body / none | class vtable[0] | 15,035 records (8/16/32) | `raw` bodies, order preserved | c3c | body byte-equal |
| `BT_ENTS ` list framing/terminator | `tail[].entities` | 36102-36103 + terminator | 2,599 sections | `full-inverse` (drop dead, append new, keep raw order) | c3c | exact consume 0/N |

Field-level verdicts for the type-2 body follow §4.3 and
`p2-15b1b-restore.md` §Field map: `patch-in-raw` for name, current tile,
anchor, direction, route type/name, radius, delays, tribe (id lookup via
`SaveTribeTable`), dialog, inventory, HP, EP; `+0x40` is verified but
unmodeled → raw; `+0x44`, `+0x654`, `+0x658`, `+0x6cc`, `+0xff0`, `+0xf50`
are raw/blocked (exact next checks in `p2-15b1b-restore.md` §Unknowns 1-6,
plus `FUN_1401ec40` emit `+0xf50`);
`patch-in-raw` only after the port gains AP and a route-index setter;
containers `full-inverse`.

## 8. Live-scenario cross-check

| Scenario (port) | Save domain that must change | Covered by |
|---|---|---|
| player moves/turns (`HeroController`) | HERO `0x000` | c3b |
| hero HP/EP/AP change | HERO `0x128` | c3b |
| hero item add/remove/equip/transfer/trade | HERO containers 0/1/2 | c3b |
| hero stats/skills change (future) | HERO `0x11c` known fields | c3b |
| NPC moves/turns (`LevelSimulation`) | ENT type-2 `+0x604/+0x608/direction` | c3c |
| NPC HP/EP change / death | ENT type-2 `+0x14fc/+0x1500`; drop record + PNAME | c3c |
| NPC spawn/despawn | ENT append/drop; PNAME | c3c |
| NPC container change (loot/trade/transfer) | ENT containers c0/c1/c2 | c3b+c3c |
| loot regeneration / restock | ENT containers + RGINF latch/datetime + RNG order | c3c (+c3g) |
| script global change | `BT_VARS` | c3f |
| quest/diary update | `BT_DIARY` | blocked (c3d) |
| event log update | `BT_HSTRY` | blocked (c3d) |
| level change | tail dictionary new/updated entry, `BT_INFO` level id | c3e |
| save preview | `BT_SCRN ` | non-claim |

All live mutations reachable in the port today map to c3b/c3c/c3f; the rest
are blocked by missing runtime systems, not by the save format.

## 9. Verdict and decomposition

**Raw preservation is the right default.** The writer already preserves every
unmodeled domain semantically (28/28 baseline), and the port cannot mutate
what it does not model. `patch-in-raw` is used only for fixed, known fields;
opaque type-2 blocks and HERO `0x600`/tail/extra are kept raw (with parity
`blocked` where the original runtime behavior is unresolved). This is a
self-consistent snapshot, not an original-parity claim.

**Full inverse is required only where the port genuinely changes the shape or
the variable-length content:** HERO containers, the selected `BT_ENTS`/
`BT_PNAME` list, the tail dictionary on level change, and the `BT_INFO` level
id. These use the original writer order (`tag/type word`, then body) and the
proven field maps; zlib bytes do not need to match the original.

**Blocked by missing live state, not by format:** world calendar
(`BT_INFO` `0x20`, `BT_RGINF` datetime), persistent `+0x26c0` latch, HERO
`0x600` runtime buffer, DIARY/HSTRY runtime systems.

### Follow-up tasks

| Task | Scope | Depends on | Acceptance oracle | Non-claims |
|---|---|---|---|---|
| `P2-15c3b` HERO live write | Extend `InventoryItem` with the raw `0xC0` record and 5×u32 mods (preserve on load, build from ITM for new items); write three containers with the presence flag and used-slot order; patch tile/direction, HP/EP/AP; patch stats/skills when the hero stats model becomes live; keep `0x600`/tail/extra/attached raw | c4b, D-073/D-081 | load slot0/slot2/slot21 → mutate hero (move, add/remove/transfer item) → save → reload: hero tuple 0/N; untouched item raw fields byte-equal; new/changed item fields match the mutated container | `0x11c` unknown remainder, `0x600`, `+0x2bb4/+0x2bb8`, `0x80`/extra/attached, modifier semantics |
| `P2-15c3c` selected tail live write | Iterate `tail[selected].entities` in BT_ENTS order; patch type-2 bodies from live persons by name+occurrence (reuse restore mapping); write type-2 c0/c1/c2 through the c3b container writer; drop dead, append new type-2 records; keep types 8/16/32 bodies raw; rebuild `BT_PNAME` (alive, standalone, slot order); patch `BT_RGINF` latch; datetime blocked | c3b (container writer), b1b1/b1b2/b1b3 | load slot0/slot21 → move/damage/mutate NPCs and containers → save → reload: person tuples 0/N; container tuples 0/N; PNAME equals the alive standalone list; raw type 8/16/32 bodies byte-equal; section exact consume | RNG/restock parity (D-064), RGINF datetime, opaque type-2 blocks |
| `P2-15c3d` DIARY/HSTRY live write | Runtime journal/log model and encode from live state; blocked until b4c/b4d | b4c, b4d | raw pass-through now; later entry/slot tuples 0/N | 0x58 field labels and HSTRY ring ordering |
| `P2-15c3e` multi-level tail + level id | Snapshot the old level when leaving, add/update the dictionary entry, patch `BT_INFO` level id (variable string); keep untouched sections raw | c3c | two-level scripted transition → save → reload both levels: tail names/order and sections 0/N; unchanged levels byte-equal | levels never visited in this session |
| `P2-15c3f` live save integration | Feed `write_loaded_save_to` from live state: `HeroController`, `InventoryRuntime`, `LevelSimulation`, party level, RGINF latch, and `script::update_vars_from_globals`; same path for `game_save` | c3b, c3c | CLI E2E in a temp dir: load slot → mutate → save → reload → compare tuples 0/N; full `ctest` | original wall-clock save UI behavior |
| `P2-15c3g` game calendar (research) | World time `0x20` layout (OQ-3), `BT_RGINF` datetime, restock interval and timer ownership | — | live-diff one save cycle; encode/decode 0/N | save preview, wall-clock header (already done) |

Recommended order: `c3b` → `c3c` → `c3f`; `c3e` after `c3c`; `c3d` and `c3g`
are blocked prerequisites for full parity.

### Open-question answers

- **OQ-C3-1 (hero tile/direction).** The HERO `0x000` triple is the current
  tile x, tile y and facing index: writer pushes `+0x604`, `+0x608` and
  `FUN_14020e64` (disasm `0x1401e698-0x1401e6bc`); reader `FUN_1401e424`
  passes the same three values to `FUN_140222d0` (21868), which stores
  `+0x604/+0x608/+0x28`.
- **OQ-C3-2 (type-2 opaque stability).** `+0x40/+0x44` are prototype/name
  fields assigned at spawn (18047-18055); `+0x658` is copied at spawn from
  prototype `+0x558..0x574` when `proto[0x156] != 0` (18093-18105); `+0x654`
  is spawn-set (18093-18094); `+0x6cc` is always 1 in the corpus, written by
  the save reader (22065) and by a runtime path (`FUN_14042308` 45368), not by
  the spawn copy; `+0xff0` is runtime-variable (4,996 distinct blocks in
  14,507 records) with only opaque get/copy consumers (`FUN_1402b870`,
  `FUN_140508cc`, `FUN_14050984`). The spawn-set fields are stable per entity
  but unmodeled; `+0x6cc`/`+0xff0` can change at runtime. All stay raw.
- **OQ-C3-3 (dictionary scope).** `FUN_14032c2c` refreshes only the current
  level entry (36083-36089); other visited-level sections remain in
  `level+0x26d8` and are dumped by the writer at 35479-35491.
- **OQ-C3-4 (attached list).** The attached list is the final `u32` count
  `+0xf44` plus `+0xf34[i]` entities; it is `0` in 28/28 HERO bodies. The
  slot16 anomaly is the `extra_count` record in the `0x80` tail, not an
  attached person. Campaign necessity of attachments is unverified and is
  recorded as a non-claim (closed for this research).
- **OQ-C3-5 (container presence).** The leading container word is a literal
  `1` when the runtime container id is not `-1` and `0` otherwise (disasm
  `0x1405c9e7` `push 1`, `0x1405ca09` `push 0`); the reader `FUN_1405ca18`
  allocates a new container id when the word is non-zero. It is not a
  recomputed invariant.

## 10. Unknowns and non-claims

1. **World calendar.** `BT_INFO` `0x20` and `BT_RGINF` `0x20` are real
   datetimes (year/month/day/weekday/hour/min/sec/ms), but the port has no
   calendar. Raw values are snapshots, not live time.
2. **HERO `0x600`.** Writer copies `hero+0x1510`; the reader zeroes the second
   half. 26/28 saves carry non-zero bytes there. Raw preservation is
   self-consistent but not the original post-load behavior.
3. **Opaque type-2 blocks.** `+0x658` (non-zero 10,616/14,507), `+0xff0`
   (distinct 4,996), `+0x654` (0), `+0x6cc` (1), `+0xf50` (discarded) are kept
   raw. The port does not mutate them; original runtime mutations are
   unverified.
4. **`+0x40/+0x44`** literary-name id/mode are not modeled; raw.
5. **AP and route index.** Both are verified save fields; the port has no AP
   parameter and no route-index setter. They stay raw until those exist.
6. **Item modifier semantics.** The 5×u32 modifier words are structurally
   proven but semantically unverified (b1b); write-back must preserve all five
   raw words, not just the port's three-field `SpecialMod`.
7. **Container roles.** c0/c1/c2 roles remain inferred (main/trade/structure);
   write-back writes them per their save identity, not by name.
8. **Attached list and extra records.** Attached list is count 0 in 28/28 and
   has no port model; extra records exist in slot16. Raw only.
9. **RNG/restock parity.** Still a non-claim (D-064); write-back does not
   claim the saved loot sequence matches the original.
10. **Zlib byte equality.** Not required and not achieved: re-compression uses
    a different encoder and is ~1.38% larger in the baseline.

## 11. Commands

```powershell
py -3 .local\research\c3a\reencode.py --saves "I:\Games\Златогорье-2\Saves" --out .local\research\c3a\out
py -3 .local\research\c3a\hero_scan.py
Get-FileHash -Algorithm SHA256 .local\research\c3a\reencode.py
Get-FileHash -Algorithm SHA256 .local\research\c3a\hero_scan.py
$od = ".local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin\llvm-objdump.exe"
& $od -d --start-address=0x1401e690 --stop-address=0x1401e756 "I:\Games\Златогорье-2\Server.dll"
& $od -d --start-address=0x1401e340 --stop-address=0x1401e424 "I:\Games\Златогорье-2\Server.dll"
& $od -d --start-address=0x1401ec40 --stop-address=0x1401edc4 "I:\Games\Златогорье-2\Server.dll"
& $od -d --start-address=0x1401d84c --stop-address=0x1401d8bc "I:\Games\Златогорье-2\Server.dll"
& $od -d --start-address=0x1405c9d8 --stop-address=0x1405ca18 "I:\Games\Златогорье-2\Server.dll"
& $od -d --start-address=0x14031b64 --stop-address=0x14032109 "I:\Games\Златогорье-2\Server.dll" |
  Select-String "26c4|26c8|1408e30|14088e14|1407fcc8"
git diff --check
```
