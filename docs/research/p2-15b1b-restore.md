# P2-15b1b — Restore persons from save entities (research)

Status: done (2026-11-14). Tasks 1–4 complete (§Type-2 fields; §Items/
§Container walk; §Restore/§Port/§Oracle; §Verdict/decomposition).
Phase: P2-15b1b research; docs-only.
Scope note: this file contains the field map, the item/container map, corpus
evidence, the restore algorithm, the port mapping/gap table, the oracle plan,
and the verdict/decomposition (verdict `scoped`).

## Provenance

| Artifact | SHA-256 |
|---|---|
| `Server.dll` (game dir) | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| 28 saves `slot0..slot27` | manifest `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |
| `levels/single/l1_1/l1_1.sef` | `A76AA26313BF446B77B84C27F92AFD8DBA54B575E3CF123EE93ADD9B951D61B1` |

Server.dll/decompiled/save manifest match `p2-15b1-save-entities.md`. SEFs were
extracted from `Data/levels.pak` with `build/bin/glpak.exe` into
`%TEMP%\opencode\sef`, not committed.

## Method

- Reader: `FUN_1401e758` (RVA `0x1e758`, decompiled `21903-22096`); primitives
  and body sizes from `p2-15b1-save-entities.md` §Type 2. Decompiled line
  numbers below refer to `.local/decomp/server/decompiled.c`.
- Consumers: `FUN_14018990` (`0x18990`, prototype/SEF load), `FUN_1401edc4`
  (`0x1edc4`, `.sdb` person parser), `FUN_1401c93c` (`0x1c93c`, route update),
  `FUN_1401c378` (`0x1c378`, delay), `FUN_1401c2a0` (`0x1c2a0`, radius),
  `FUN_140222d0` (`0x222d0`, position apply), `FUN_14016998`/`FUN_14016300`
  (`0x16998`/`0x16300`, tribe/relation), `FUN_1401a24c` (`0x1a24c`, state
  message), `FUN_1402b870`/`FUN_140508cc`/`FUN_14050984`
  (`0x2b870`/`0x508cc`/`0x50984`, `+0xff0`).
- Corpus: Python 3.9 scan of all 28 saves and all 2599 tail sections; every
  type-2 body is parsed field-by-field and must consume the whole `BT_ENTS `
  body. SEF correlation uses the selected level section and an order-preserving
  LCS of SEF person names against type-2 names (duplicates are kept by index).
- Scripts are not committed and live only under `%TEMP%\opencode`. Every
  extended corpus number in this document is traceable to one of these scripts:

| script (`%TEMP%\opencode\...`) | SHA-256 | produced |
|---|---|---|
| `p2_15b1b_scan.py` | `BA77E5BE5EC08A2616871986E2CD4B85FCEF8B6BABDAF82ACB9986A7134E7C06` | base save parser; 28/2599/14507; PNAME identity; ranges/constants table; direction and tribe-id membership |
| `p2_15b1b_fields.py` | `8ED2B09AF3BBCD03B51B1A5580BE2C4699C76355905462C4DF2847D778FA5744` | SEF identity table; route-type distribution; radius-zero and route-name lengths |
| `p2_15b1b_sef.py` | `3485BFF58EFCA2C1BEB8AADCE8AF78EC7A9EC5F673762E64CD6A1D69B4097FE4` | SEF person parser and LCS alignment helper (no standalone numbers) |
| `p2_15b1b_corr.py` | `85E4D56D118F3F781554D99AE49A58058FFD0E20897300F6EB8798B79FB309EC` | by-route table; signed `+0x1630`; `+0x654`/`+0x6cc`/tail constants; `+0x40`/`+0x44` distinct counts/ranges; `+0x658`/`+0xff0` record counts; empty-string counts |
| `p2_15b1b_f658.py` | `533C84966DC0DBA3C3E3E366132633683A493BBF402C65043D1E006FD57D2439` | `+0x658` per-dword ranges and 10,616 non-zero records; delay invariant; route index by type |
| `p2_15b1b_f658b.py` | `0B0005860BA8CCD58797190846FD606AF6A3068C2002BE45EE8F9AD730E3163D` | 285/431 names with more than one `+0x658` value |
| `f658c.py` | `8DA78AFEBE5D847A0B25C52A58151C44495C3F7F0F3604DD70778B9F290893E2` | 69,943/116,056 non-zero `+0x658` dwords; high-byte list; low-24-bit range/1,372 distinct |
| `ff0.py` | `10B9C842E3ACEB8BEE35FF4DD57F29B034AF7EB1C6E8DD4340F00E384D829B43` | `+0xff0` 4,996 distinct blocks and 34/431 name-stable |
| `f44.py` | `BF1F5C622BC81D449CBD795C6D87297A26A236E9B3111E17C385FF54DB3B5154` | `+0x44` distribution `{0:13732, 1:526, 2:249}` |
| `extras.py` | `28277336011DD1AFE4009AFD1BB8AA8C296C6BB68F474BE3C53800C8DAB0B390` | per-save SEF-only counts and the 3 type-2-only records |
| `tribe.py` | `C40891A2FBE49702349994D7187A6B34955BA64332895642C0E80FE0A7D315C6` | `BT_TRIBE` 234 unique ids 0..233 per save |
| `p2_15b1b_items.py` | `D2A4C5630FDEE02C95AC81AF0B31C63CDF7B8CA4CC1B1497918DE389F2D33EB5` | 43,521 containers / 115,405 item records; field ranges, marker, id validity, subrecord corpus, ITM compare |
| `p2_15b1b_items2.py` | `22F0D9432E12D56149F4EDDFB6BAD8FC6B08DB8274E806311F785C1CC5A1BA36` | per-slot container roles; flags XOR bits; 129,256 subrecord dwords |
| `p2_15b1b_pins.py` | `D3E91BD2A0EE07A660D176F6A6BFB66D0D5309132DDDBA5335C060E310F5DEB2` | slot0/slot2/slot21 person tuples: position/anchor/direction/route/HP/EP/AP/container item ids+counts |
| `p2_15b1b_dups.py` | `D194A686FEAFA4E2A3A548E432D58629E98606288702A02A6B5946D6C84032C7` | duplicate-name groups: 81 groups, max multiplicity 6, 499 selected-section type-2 records |
| `p2_15b1b_oracle.py` | `425463D776F92C256A58719DEB91C740259B810EF5593C69B20A9C5E04F73C72` | pin raw-body SHA-256, canonical tuple SHA-256, latch/type histogram |
| `items_list.txt` | `5A018787943A7CC77C462D60C195532C5DFFA2216B47978CEC015930FC475112` | `glpak list items.pak` (3,786 item ids) used for id validity |
| `gl_sdb\tech_names.txt` | `147C6F81DB384AE835568DCD71047AEBCA122BC6E876EB557402EA5FA6B5303A` | `glpak decode-sdb sdb/items/tech_names.sdb` (1,848 entries) |

## §Type-2 fields

Runtime object notes:

- Name is a length word at `+0x48` followed by up to `0x104` cp1251 bytes
  (text starts at `+0x4c`); the reader loads the person script from this text
  through `FUN_1401d99c(+0x4c, ...)` at 21962 and stores the resolved prototype
  at `+0x5e0` (21963-21964). `+0x5e0`, `+0x6d0`, `+0x1650`/`+0x1654` and
  `+0x5a8` are recomputed, not saved.
- The coordinate primitives are `FUN_140011ac` (two u32, 8 bytes), strings are
  `u32 n + min(n,0x104)` bytes, and `FUN_1400126c` skips 4 bytes.
- `FUN_140222d0` (25018-25031) converts a tile pair to world coordinates
  `x*12+6`, `y*9+4`, stores the tile at `+0x604`/`+0x608`, the direction at
  `+0x28`, and the world floats at `+0x628`/`+0x62c`/`+0x4fc`/`+0x500`.
- The port uses the same tile constants (`level_simulation.cpp:12-13`,
  `64-65`), so restored positions can be applied without unit conversion.

### Field map

Status legend: **verified** = decompiled consumer plus 0/N corpus or SEF
identity; **inferred** = consumer semantics and corpus consistent, but no
decisive identity; **unverified** = needs the exact next check listed below.

| # | save field | kind / bytes | runtime destination | semantics / unit | consumer evidence (RVA; decompiled lines) | port target | status |
|---:|---|---|---|---|---|---|---|
| 1 | name | string `4+n` | `+0x48` len + `0x104` buf | technical/script name; script at `scripts/persons/<name>.scr` | reader 21930-21964; `FUN_14018990` 18010-18015; PNAME writer 36006; corpus PNAME identity 2599/2599 | `PersonInstance.name` | verified |
| 2 | `+0x40` | u32 | `+0x40` | literary-name id (`LiteraryName`) | reader 21965-21966; `.sdb` `literary_name` 22206-22208; SEF 496/496 | none (gap) | verified |
| 3 | `+0x44` | u32 | `+0x44` | literary-name/role mode: 0 default, 1/2 variants | reader 21967-21968; `.sdb` sets 0 at 22207; prototype copy 18047-18055; corpus `{0:13732, 1:526, 2:249}` | none (gap) | inferred |
| 4 | coordinate pair | 2×u32 | tile `+0x604`/`+0x608` and `+0x4f4`/`+0x4f8`; world int `+0x18`/`+0x1c`; world float `+0x628`/`+0x62c` and `+0x4fc`/`+0x500` | current tile x,y; world = `x*12+6`, `y*9+4` | reader 21969-21971 + `FUN_140222d0` 25018-25031; SEF: STAY 274/274, STAY_ROTATE 13/13 exact | `LevelSimulation` movement x/y (current); `PersonInstance.tile_x/y` is spawn only | verified |
| 5 | `+0x5f8` | u32 | `+0x5f8` | anchor/spawn tile X (STAY target, RANDOM_RADIUS base) | reader 21972-21973; `FUN_1401c93c` 20737-20744/20801-20808; `FUN_1401d814` 21233; SEF 496/496 | `PersonInstance.tile_x`, `State.spawn_tile_x` | verified |
| 6 | `+0x5fc` | u32 | `+0x5fc` | anchor/spawn tile Y | reader 21974-21975; as row 5 | `PersonInstance.tile_y`, `State.spawn_tile_y` | verified |
| 7 | `+0x654` | u32 | `+0x654` | unknown flag; always 0 in corpus | reader 21976-21977; consumer `FUN_1401ab74` 19345; prototype flag set 18093-18094 | none | unverified |
| 8 | `+0x658` | raw `0x20` | `+0x658`..`+0x677` | eight u32, prototype-provided at spawn, runtime-variable | reader 21978; prototype copy 18097-18105 (proto `+0x558..+0x574`); no other Server consumer | none | inferred |
| 9 | direction | u32 | `+0x28` | facing index 0..7: `UP, UP_LEFT, LEFT, DOWN_LEFT, DOWN, DOWN_RIGHT, RIGHT, UP_RIGHT` | reader 21979 + `FUN_140222d0` 25033; `.sdb` parser 22231-22232; mapping `FUN_1401fb1c` 22948-23135; SEF 309/496 (runtime rotation) | `PersonInstance.direction`, `PersonMovement.direction8` | verified semantics; runtime facing 309/496 |
| 10 | `+0x151c` | u32 | `+0x151c` | route type enum 0..5: `STAY, STAY_ROTATE, MOVED, MOVED_FLIP, RANDOM_RADIUS, RANDOM` | reader 21980-21981; `FUN_1401c93c` 20728-20844; `FUN_1401d814` 21232; mapping `FUN_1401f9bc` 22801-22943; SEF 496/496 | `PersonInstance.route_type` / `RouteType` | verified |
| 11 | `+0x1524` | string `4+n` | `+0x1524` len + buf | route name; resolved to route cells from SEF/level | reader 21982-21999; `.sdb` `route` 22280-22288; SEF 57/57 | `PersonInstance.route` -> `route_cells` | verified |
| 12 | `+0x1520` | u32 | `+0x1520` | wander radius in tiles; 0 means engine default 13 | reader 22007-22008; `FUN_1401c2a0` 20405-20416; `.sdb` `radius` 22311-22313; SEF 496/496; corpus 0..75, 122 zeros | `PersonInstance.radius`, `State.radius` | verified |
| 13 | `+0x2668` | u32 | `+0x2668` | delay minimum, milliseconds | reader 22009-22010; `FUN_1401c378` 20448-20463 + `FUN_140061e4` 4347-4358; `.sdb` `delay_min` 22335-22337; SEF 496/496 | `PersonInstance.delay_min`, `State.delay_min` | verified |
| 14 | `+0x266c` | u32 | `+0x266c` | delay maximum, milliseconds | reader 22011-22012; `FUN_1401c378` + `FUN_140061e4`; `.sdb` `delay_max` 22359-22361; SEF 496/496 | `PersonInstance.delay_max`, `State.delay_max` | verified |
| 15 | `+0x150c` | u32 | `+0x150c` | numeric tribe id; index into `BT_TRIBE`; resolved to tribe name | reader 22013-22014; `FUN_14016998` 17038-17045; call sites 18551/18564/19275/19370/19669/38961; debug "Tribe" 27218-27220; `.sdb` `tribe` 22383-22386; `BT_TRIBE` reader 16996-17032 | `PersonInstance.tribe` (name); numeric id not modelled | verified semantics; port gap |
| 16 | `+0x267c` | string `4+n` | `+0x267c` len + buf | dialog script name (`scr_dialog`) | reader 22015-22032; `.sdb` `scr_dialog` 22408-22418; SEF 478/478 | `PersonInstance.dialog` | verified |
| 17 | `+0x2aac` | string `4+n` | `+0x2aac` len + buf | inventory script name (`scr_inv`) | reader 22033-22057; `.sdb` `scr_inv` 22440-22455; SEF 480/480 | `PersonInstance.inventory` | verified |
| 18 | `+0x6cc` | u32 | `+0x6cc` | unknown flag; always 1 in corpus; gates combat-state message | reader 22064-22065; `FUN_1401a24c` 18960-18974 | none | inferred |
| 19 | `+0xff0` | raw `0x11c` | `+0xff0`..`+0x110b` | opaque per-person state block, runtime-variable | reader 22066; `FUN_1402b870` 31283-31303 (compare/copy); `FUN_140508cc` 55323; `FUN_14050984` 55357; prototype copy 18019-18025 (vtable `+0x50`) | none | inferred |
| 20 | skipped | raw 4 | discarded | writer field discarded by reader | reader 22067 (`FUN_1400126c`); no consumer | none | unverified |
| 21 | `+0x14fc` | u32 | `+0x14fc` | current HP, alive when >0 | reader 22068-22069; debug `HP: %d/%d` 27298; alive checks 17591/19193-19194/30098/38540/46443; `FUN_14019674` 18440-18443 | `PersonState` `life` (index 10) | verified |
| 22 | `+0x1500` | u32 | `+0x1500` | current EP | reader 22070-22071; debug `EP: %d/%d` 27303; regen/drain 18740/18921-18941/20978 | `PersonState` `energy` (index 11) | verified |
| 23 | `+0x1504` | u32 | `+0x1504` | current AP (action points) | reader 22072-22073; debug `AP: %d/%d` 27308; ops 18659/18741-18895/18925-18941 | none (`PersonParameter` has no AP) | verified semantics; port gap |
| 24 | `+0x1630` | u32 | `+0x1630` | route waypoint index; signed, -1..15; non-zero only for MOVED/MOVED_FLIP | reader 22074-22075; `FUN_1401c93c` 20772-20844 | `State.route_index` | verified |
| 25 | container `+0xc` | `M` | `+0xc` | entity container c0 (general inventory; all item types) | reader 22076-22077 `FUN_1405ca18`; format §Container walk; fields §Items | none yet | verified structure; role inferred |
| 26 | container `+0x10` | `M` | `+0x10` | entity container c1 (trade/money; MON-dominated) | reader 22078-22079; format §Container walk | none yet | verified structure; role inferred |
| 27 | container `+0x109c` | `M` | `+0x109c` | entity container c2 (0/1 `NPC_*` weapon/appearance item) | reader 22080-22081; format §Container walk | none yet | verified structure; role inferred |
| 28 | tail | `0x80 + u32 c + 0x14*c` | raw 0x80 to `+0xf5c`/`+0xf7c`/`+0xf9c`/`+0xfbc`; entries at `+0x110c + i*0x14`, count `+0x14f4`, capacity `+0x14f8` | person tail via `FUN_1401d8bc`; `c=0` in all corpus bodies | reader 22082; `FUN_1401d8bc` 21261-21295 | none yet | structural (tail, not an item container) |

Notes:

- The "current position" and "anchor/spawn" split is proven by SEF: the
  coordinate pair differs from the SEF position predominantly for moving route
  types (with 2/56 exact for RANDOM and 1/96 exact for RANDOM_RADIUS), while
  `+0x5f8/+0x5fc` equals the SEF position for 496/496 aligned persons.
- `+0x151c` mode 4 uses `+0x5f8/+0x5fc` as its centre; mode 5 uses
  `+0x604/+0x608` (20779-20808), which matches the `RANDOM_RADIUS` / `RANDOM`
  names.
- `FUN_1401d814` (21233-21234) copies the current tile into the anchor when a
  new route mode is assigned; it is a route-reset, not the normal save path.
- `+0x150c` is the same `BT_TRIBE` index used by `FUN_14016998`; `BT_TRIBE` is a
  234-entry `(id, name)` table plus a 234×234 relation matrix (reader
  16996-17032). This resolves OQ-13 for matched persons: save id -> `BT_TRIBE`
  name == SEF `tribe` 496/496.
- `+0xff0` was not decoded; the only static Server consumers are the opaque
  get/copy functions above. The first dwords are small integers (e.g.
  `0c 00 00 00 10 00 00 00 07 00 00 00`), but the block is not stable per name
  (34/431 names have one value; 4,996 distinct blocks in 14,507 records).

## §Items

### Identity with `BT_HERO`

The type-2 reader `FUN_1401e758` reads all three entity containers through
`FUN_1405ca18` (22076-22081), exactly the function used by the hero reader
`FUN_1401e424` for `hero+0xc/+0x10/+0x109c` (21824-21829); the corresponding
writer is the shared `FUN_1405c9d8` (hero writer `FUN_1401e690` 21892-21894).
`FUN_1405ca18` → `FUN_1404c890` and `FUN_1405c9d8` → `FUN_1404c7a8` →
`FUN_1404f4bc` are the same functions that p5-save-equipment proved for
`BT_HERO`. Therefore the entity-container record is the same `0xC0 + u32 sub +
0x14*sub` record, and the corpus confirms it: the item-class marker
`0x14092248` is present in **115,405/115,405** entity item records, and
`+0x78` (in-record subcount) equals the trailing subcount in
**115,405/115,405**.

Decompiled line numbers below refer to `.local/decomp/server/decompiled.c`;
disassembly addresses are RVAs of `Server.dll`.

### Item record map

| offset | size | field | evidence |
|---|---|---|---|
| `+0x00` | u32 | item class/vtable marker `0x14092248` | ctor `FUN_1404f81c` 54685; corpus 115405/115405 |
| `+0x04` | u32 | runtime scratch (not save semantics; 0 in the p5 hero slots) | raw `0xC0` dump; varies in corpus |
| `+0x08` | u32 | item type = ITM `type` | def loader `FUN_1404f4fc` 54547; ITM compare 198/198 |
| `+0x0C` | u32 | flags = ITM `flags`; bits 12..23 are the equip mask on worn items (p5 §6) | loader 54548; accessor `FUN_1404f7bc` 54663; ITM compare |
| `+0x10` | i32 | price = ITM `price` | loader 54549; ITM compare |
| `+0x14` | u32 | weight_x10 = ITM `weight_x10` | loader 54550; ITM compare |
| `+0x18` | u32 | unknown_a = ITM `unknown_a` | loader 54551; ITM compare |
| `+0x20` | u32 | style = ITM `unknown_b[1]` | loader 54554; ITM compare |
| `+0x30` | u32 | material = ITM `material` | loader 54562; ITM compare |
| `+0x78` | u32 | subcount; duplicate of the trailing `+0xC0` u32 | reader zeroes then reloads 52434-52440; corpus 115405/115405 equal |
| `+0x7C` | u32 | subrecord pointer (runtime); value is ignored on load | reader zeroes then reloads 52434-52440 |
| `+0xA4` | u32 | item instance id (index into the world item table) | loader 54543 (`param_1`); accessor 54667 |
| `+0xA8` | u32 | item definition id (`items/data/<id>.itm`, `tech_names.sdb`) | reader 52442-52444; loader 54538; corpus 115405/115405 in both tables |
| `+0xAC` | u32 | owning runtime container id | `FUN_1404ec2c` 54148; accessor 54666; corpus consecutive per entity |
| `+0xB0` | u32 | stack count | `FUN_1404f4fc` default 1 (54644); `FUN_1404ec2c` 54149; reader 52480 |
| `+0xB4` | u32 | durability / condition | `FUN_1404f4fc` default 100 (54645); reader 52481 |
| `+0xB8` | u32 | type-6-only value from def `+0x9c`, otherwise 0 | `FUN_1404f4fc` 54646-54651; corpus non-zero only for type 6 (1,832/1,832) |
| `+0xBC` | u32 | unknown; always 0 in corpus | ctor 54697; reader 52483 |
| `+0xC0` | u32 | subrecord (modifier) count | writer `FUN_1404f4bc` 54516; reader 52436 |
| `+0xC4` | `0x14*sub` | subrecords: 5×u32 each | writer 54519; reader 52438 |

`FUN_1404f7bc` (54659-54676) is an independent accessor that exposes the same
fields, in this order: `[0]=+0xA4`, `[1]=+0xA8`, `[2]=+0xAC`, `[3]=+0x0C`,
`[4]=+0xB0`, `[5]=+0xB4`, `[6]=+0xB8`, `[7]=+0xBC`, `[8]=+0x78`,
`[9]=+0x7C` — a direct cross-check of the offsets above.

### Differences from `BT_HERO`

The serialized layout is the same; only value usage differs:

- `+0x0C` is the ITM `flags` word. p5 read it as an equip mask because the hero
  equipped container has bits 12..23 set. Entity containers never set bits
  12..23 (**0/115,405** records), and in low bits the record can differ from the
  static ITM `flags`: in a 198-record stratified sample, 139/198 match exactly
  and 59/198 differ only in bit 7 (`0x80`) or bit 9 (`0x200`), never in bits
  12..23. So `+0x0C` is runtime item flags, not a pure template copy.
- `+0x10 price`, `+0x14 weight`, `+0x18 unknown_a`, `+0x20 style`,
  `+0x30 material` still equal the ITM values (198/198 sample on those fields),
  even when price/weight are 0 in c2.
- `+0x04` is runtime scratch and is not always 0 outside the p5 hero slots.
- `+0xAC` is the owning container id, which explains the p5 "constant per slot"
  observation (14/656 there): the three containers of one entity are allocated
  with consecutive ids (dominant `(c0,c1,c2)` deltas `(0,1,2)`, 9,848 entities).

### Modifier subrecords (`+0xC4 + i*0x14`)

Each subrecord is 5×u32; semantics are **unverified**, but the corpus shape is
consistent across 129,256 subrecords:

| word | offset | observed |
|---|---|---|
| 0 | `+0x00` | stat/skill id, 0..57 |
| 1 | `+0x04` | signed value, -50..500 |
| 2 | `+0x08` | kind/mask, values `{1,65,66,68,80,96,129,130,132,144}` |
| 3 | `+0x0C` | 0 in 109,345; otherwise small ids plus `2560/5120/7680` |
| 4 | `+0x10` | 0 (22,176) or -1 (107,080) |

## §Container walk

### Save-file format (call-free)

```
container   := u32 present(1) | u32 absent(0)
present     := u32 count ; count * item_record
item_record := 0xC0 raw ; u32 sub ; sub * 0x14
```

- The leading u32 is a **presence flag written as a literal 1/0**, not the
  runtime container id: writer `FUN_1405c9d8` pushes `1` (disasm RVA
  `0x1405c9e7`) or `0` (`0x1405ca09`) into `FUN_1400126c`, which writes the
  4-byte argument (`0x1400126c`). The reader `FUN_1405ca18` only tests it for
  non-zero (`0x1405ca24`) and discards the value, then allocates a fresh
  container id. This corrects the p5 "u32 id" wording for entity containers.
- Corpus: 43,521 containers = 3 × 14,507 type-2 records; 42,192 present (`1`) +
  1,329 absent (`0`); 5,150 zero-count; item counts 0..116.
- Items per slot: c0 82,628, c1 18,621, c2 14,156.
- `entity+0xc/+0x10/+0x109c` store the *runtime* ids returned by
  `FUN_1405ca18`; the file stores no such ids.

Container roles (inferred from item content; not yet proven by a consumer):
c0 = general inventory (all 29 item types), c1 = trade/money (MON-dominated,
plus QST/FOD/POT/armor), c2 = 0/1 generated `NPC_*` weapon/appearance item
(types 0..7 and 28 only, **14,156/14,156** `NPC_*` tech names). Task 3 decides
what is restorable.

### Runtime manager walk (call-free)

```
world = DAT_14088df8                         # RVA 0x88df8
world+0x08  next container id                # FUN_1404bc04 51739
world+0x0c  container map {u32 count}        # FUN_14004a20 2917-2977
world+0x10  container map {entry*}
entry       = {u32 id ; container* obj}       # stride 8, sorted by id
world+0x18  item* array base                  # FUN_1404dbec 53336 / FUN_1404ec2c 54147
world+0x1c  item array size
world+0x20  item used bitmap

container (0x13c bytes, ctor FUN_1404f090 54244-54263):
  +0x00 vtable       +0x04 u32 id         +0x08 embedded vtable
  +0x0c u32* item_instance_ids            +0x10 u32 count
  +0x14 u8*  used bitmap
```

Call-free walk: read `cid = *(u32*)(entity + 0xc/+0x10/+0x109c)`; binary-search
the `{id,obj}` array at `world+0xc` (key at `+0`, value at `+4`, stride 8) to get
the container; for each used bit `i` read `item_instance_ids[i]` and index the
item table at `world+0x18` (bounds `world+0x1c`, used bitmap `world+0x20`).
`FUN_1404dbec` (53283-53345) implements exactly this id search.

Evidence that the manager lookup returns the object: the decompiled
`FUN_1404bf8c` (51910-51920) drops the value, but disassembly shows it returns
the map value (`0x1404bf8c`: call `FUN_14004f14`, then `movl (%esp),%eax`), and
`FUN_14004f14` returns `entries[index].value` (`0x14004f3e-0x14004f45`), with
`FUN_14004a20` binary-searching an 8-byte-stride array (`0x14004a81`).

An external oracle can use the save-file walk with no game code; the runtime
walk needs only pointer reads and a binary search (no function calls), but the
container ids/pointers exist only in a live process.

## Corpus evidence

### Parse identity

- 28 saves, 2,599 tail sections, 14,507 type-2 records; every section consumed
  exactly (`BT_ENTS ` body end reached). Type histogram over all sections:
  `{2:14507, 8:354, 16:12090, 32:2591}` (matches P2-15b1a).
- `BT_PNAME` list equals the type-2 name list in order: **2599/2599** sections,
  0 mismatches (this is stronger than the earlier count equality).
- SEF `persons` vs selected save section, order-preserving LCS: 539 SEF person
  entries (counted per save-level pair), 496 aligned, 43 SEF entries not present
  in the selected save section (cause not established), and 3 type-2-only
  records: `L3_3.P335_Marvian_Capitan` (save section `L9_2_1`),
  `L9_3.P415_Sotniks_Friend` (`L9_3`), `L0.P656_travelwoman1` (`RL_2`).
  Alignment preserves duplicates by index.

### Field identity against SEF (496 aligned persons)

| field | exact matches |
|---|---:|
| home position `+0x5f8/+0x5fc` vs SEF `position` | 496/496 |
| route type `+0x151c` vs SEF `route_type` | 496/496 |
| radius `+0x1520` vs SEF `radius` | 496/496 |
| delay min `+0x2668` vs SEF `delay_min` | 496/496 |
| delay max `+0x266c` vs SEF `delay_max` | 496/496 |
| tribe `+0x150c` -> `BT_TRIBE` name vs SEF `tribe` | 496/496 |
| literary name `+0x40` vs SEF `literary_name` | 496/496 |
| dialog `+0x267c` vs SEF `scr_dialog` | 478/478 |
| inventory `+0x2aac` vs SEF `scr_inv` | 480/480 |
| route `+0x1524` vs SEF `route` | 57/57 |
| direction `+0x28` vs SEF `direction` | 309/496 (runtime facing) |

Direction and current-position matches split by route type (aligned persons):

| route type | home | current pos | direction |
|---|---:|---:|---:|
| STAY | 274/274 | 274/274 | 268/274 |
| STAY_ROTATE | 13/13 | 13/13 | 1/13 |
| MOVED | 26/26 | 0/26 | 9/26 |
| MOVED_FLIP | 31/31 | 0/31 | 4/31 |
| RANDOM | 56/56 | 2/56 | 12/56 |
| RANDOM_RADIUS | 96/96 | 1/96 | 15/96 |

The direction/current mismatches are the expected runtime state: the save
stores the NPC's current facing and current tile, while SEF stores the initial
spawn state. They do not contradict the mapping.

### Value ranges and constants (14,507 type-2 records)

| field | range / distribution |
|---|---|
| current tile X (`pair[0]`) | 2..423 |
| current tile Y (`pair[1]`) | 1..469 |
| anchor X `+0x5f8` | 5..412 |
| anchor Y `+0x5fc` | 6..439 |
| direction `+0x28` | 0..7 |
| route type `+0x151c` | `{0:6977, 1:268, 2:632, 3:903, 4:4421, 5:1306}` |
| radius `+0x1520` | 0..75 (0 in 122 records) |
| delay min `+0x2668` | 0..25000 ms |
| delay max `+0x266c` | 1..30000 ms; min <= max 14507/14507 |
| tribe id `+0x150c` | 0..233; all 14507 resolve in `BT_TRIBE` |
| `+0x6cc` | always 1 |
| `+0x654` | always 0 |
| `+0x658` | 10,616/14,507 records non-zero; 69,943/116,056 dwords non-zero |
| `+0xff0` | non-zero in 14,507/14,507 records; 4,996 distinct blocks |
| HP `+0x14fc` | 2..826 |
| EP `+0x1500` | 3..782 |
| AP `+0x1504` | 0..60 |
| route index `+0x1630` (signed) | -1..15; 81 negative; non-zero only modes 2/3 |
| `+0x40` | 0..108 (76 distinct) |
| `+0x44` | `{0:13732, 1:526, 2:249}` |
| container item count | 0..119 (sum of the three containers per entity) |
| tail count `c` | always 0 |

### Item-record corpus (115,405 records)

| field | range / distribution |
|---|---|
| class marker `+0x00` | `0x14092248` in 115,405/115,405 |
| item type `+0x08` | 0..28 (29 values; 18 MON alone = 20,072) |
| flags `+0x0C` | low 24 bits 0..789; high bits only `0x40000000`/`0x80000000`; equip bits 12..23 = 0/115,405 |
| price `+0x10` | 0..88,700; zero in all 14,156 c2 items |
| weight `+0x14` | 0..100; zero in all 14,156 c2 items |
| unknown_a `+0x18` | 0..50 |
| style `+0x20` | 1..15 |
| material `+0x30` | 0..14 (plus `0xFFFFFFFF` = -1) |
| subcount `+0xC0` | 0..11; non-zero in 41,029/115,405 |
| instance id `+0xA4` | 0..3,128 (per world/section) |
| definition id `+0xA8` | 1,342 distinct; all 115,405 exist in `items.pak` and in `tech_names.sdb` |
| container id `+0xAC` | 1..15,072 (6,914 distinct); consecutive for c0/c1/c2 of one entity |
| stack count `+0xB0` | 1..122,031 |
| durability `+0xB4` | 33..100; 100 in 115,382/115,405 |
| `+0xB8` | 0 except type 6 (1,832/1,832 type-6 records) |
| `+0xBC` | 0 in 115,405/115,405 |

ITM cross-check: 198 stratified records (3 per slot×type) decoded with
`glpak decode-itm`; type, price, weight_x10, unknown_a, style, material match
198/198; flags match 139/198 exactly and differ only in bit 7/9 in 59/198.

`+0x658` details: eight u32; not a small coordinate pair. High bytes cover
`0x00/0x10/0x20/0x0d/0x02/0x01/0x08/0xff/0x6a/0x6c/...`; low 24 bits range
1..16,777,215 with 1,372 distinct values. The block changes per record/save
(285/431 names have more than one value in the corpus), so it is runtime state,
not a static SEF field.

## §Restore algorithm

Source: `.sav` loader `FUN_1403210c` (RVA `0x3210c`, decompiled 35523-35695) →
section loader `FUN_140325fc` (RVA `0x325fc`, 35704-35931) → type-2 reader
`FUN_1401e758` (RVA `0x1e758`, 21905-22096). Unless stated otherwise, line
numbers are `.local/decomp/server/decompiled.c`.

### Ordered restore

1. **Header + tail dictionary.** `0x68` header, two inflate lengths, compressed
   payload, then `u32 count` and `name + section` entries into the dictionary at
   `level+0x26d8` (35602-35627).
2. **`BT_INFO ` / clock.** Session string, `+0x218`, the `0x20`-byte datetime
   (applied through `DAT_14088e14+0x1c`, so it becomes the current clock),
   description, level id, `+0x26c4` (party level), and 8 bytes into `+0x26c8`
   (35628-35639).
3. **Selected section** `FUN_140325fc(level, level_id, 0)`:
   - **SEF:** open `levels/single/<level>/<level>.sef`; parse only
     `points_entrance` (`FUN_14031844`) and `cell_groups` (`FUN_140319a0`)
     (35779-35810). **SEF persons are not read, matched, or instantiated on
     restore.** The SEF person constructor is `FUN_14018990` (RVA `0x18990`,
     17936-18119), called from the level-spawn paths at 38277/38435, not from
     `FUN_1403210c`/`FUN_140325fc`.
   - **`BT_RGINF`:** read `u32` latch into `level+0x26c0` and `0x20` datetime into
     `+0x26a0` (35812-35846). If the saved latch is `0`, the loader compares
     `saved_datetime + gv_items_regenerate_interval` with the current clock and,
     when due, sets `+0x26c0=1` and overwrites `+0x26a0` with the current clock
     (35818-35835). If the saved latch is non-zero, it sets `+0x26c0=0` and
     keeps the saved datetime. The persisted `1` is therefore not a runtime
     “regenerate now” flag.
   - **`BT_ENTS `:** loop `u32 type`; `type == 1` terminates. For each record:
     create the object with the factory `FUN_1405c83c(type)`, allocate a level
     slot with `FUN_14003718(level+0x2150)` (RVA `0x3718`, 2565-2582), run the
     type's virtual reader, and store the object at `level+0x2564[slot]` with the
     bit set in `level+0x2568` (35847-35915). Type-2 readers use `FUN_1401e758`;
     the slot allocator walks free indices modulo 256, so `BT_ENTS` record order
     is the installation order.
4. **`BT_VARS` → `DAT_1407f9a0`; `BT_TRIBE` → `DAT_14089fc0`; `BT_DIARY` →
   `DAT_14088e04`** (35664-35669).
5. **`BT_HERO` + attached list** (35670-35684): read the hero body with
   `FUN_1401e424`; then a count-prefixed list, each entry created as type 2, read
   with `FUN_1401e758(stream, 0)`, marked `+0x2bc0=1`, `+0xf28=0`, and installed
   into the level attached list `level+0x26e8` with `FUN_140368d4` (RVA
   `0x368d4`, 38346-38370). `FUN_140253fc` (RVA `0x253fc`, 27434-27458) tests
   membership; the `BT_PNAME`/`BT_ENTS` writers skip attached persons
   (36007/36098), so attached persons exist only in `BT_HERO`.
6. **`BT_HSTRY`** (35685-35686).

### Type-2 body application order

`FUN_1401e758` applies the body in stream order, but three steps dominate
restore semantics:

1. **Identity/static.** Name string into `+0x48/+0x4c`; resolve
   `scripts/persons/<name>.scr` and the prototype through `FUN_1401d99c` and
   `DAT_140891bc+0x98` into `+0x5e0` (21930-21964). Then `+0x40`, `+0x44`,
   current tile pair, `+0x5f8/+0x5fc`, `+0x654`, `+0x658[0x20]`, direction,
   `+0x151c`, route string, `+0x1520`, delays, tribe id, dialog/inventory
   strings, `+0x6cc`, `+0xff0[0x11c]`, the skipped `u32`, HP/EP/AP,
   `+0x1630`.
2. **Containers.** Read c0/c1/c2 through `FUN_1405ca18` (22076-22081), then the
   tail via `FUN_1401d8bc` (22082). Serialized item records are present before
   any loot decision.
3. **NPC inventory override.** If `level+0x26c0 != 0` and c0/c1 are valid
   (`!= -1`), call `FUN_1404b1c0(world, c0, c1, +0x2ab0, 0)` (22083-22086).
   `+0x2ab0` is the inventory script name; after the empty-script/container/
   `.inv`-load setup (51092-51139), the call is a **chance-gated
   regeneration** from `scripts/inventory/<scr_inv>.inv` and the party level
   (`FUN_1404b1c0`, RVA `0x4b1c0`, 51044-51290+; chance gate at 51141-51142).
   c2 is never passed and is not regenerated. Trigger type-16 uses the same
   function with force flag `1` (22454); restore uses `0`.
4. **State finalize.** Copy `+0x1650/+0x1654` from `+0x108c/+0x1090`; set
   `+0x5a8=2`; `FUN_14021658(slot, 2)`; then `FUN_140222d0(tile_x, tile_y,
   direction)` converts to world (`x*12+6`, `y*9+4`), stores the current tile,
   direction, and world floats (25018-25031); set `+0x6d0` from the prototype.

### What is replaced, merged, ignored

| Source | Restore behavior | Evidence |
|---|---|---|
| SEF entrances/cell groups | merged into the level tables | 35779-35810 |
| SEF persons | **not used on restore**; original has no SEF-person → save-entity match | restore path never calls `FUN_14018990` (17936) |
| `BT_ENTS` type-2 record | authoritative for the person's saved state; object is created fresh from the body | 35847-35915 |
| `BT_PNAME` | parallel name list used by the port/test for order/name validation and as a fallback; the original restore does not read it and it is not a matching key | 2599/2599 identity; §Corpus evidence |
| matched SEF/prototype data | supplies static appearance, PAD/sprite, cell-group route cells, tribe/dialog/inventory templates when the port reuses a `PersonInstance` | `load_persons` 106-271 |
| `unverified`/`inferred` semantics (`+0x44`, `+0x654`, `+0x658`, `+0xff0`, `+0x6cc`) | ignored or left at prototype/SEF default; not claimed restored | §Type-2 fields status |
| fields with no restore target (`+0x40` literary-name id, `+0x1504` AP, `+0x1630` route index) | `+0x40`/AP dropped; `+0x1630` has `State.route_index` but no restore setter | §Port mapping |
| c0/c1 containers | replaced only when the `+0x26c0` chance-gated regeneration runs; otherwise the serialized records remain | 22076-22086; 51141-51142 |
| c2 container | serialized only; never overwritten by `FUN_1404b1c0` | 22080-22081; 51044 |
| tail (`FUN_1401d8bc`, `c` entries) | consumed but no consumer field; `c == 0` in all corpus bodies | 22082; Task 1 |
| attached `BT_HERO` persons | separate list, never in `BT_ENTS`; must be restored from `BT_HERO`, not matched against saved persons | 35670-35684; 36007/36098 |
| type 8/16/32 entities | installed by the same slot allocator; not persons and outside b1b | 35847-35915 |

### Matching and duplicate policy (port-side)

The original does not match: it creates one object per `BT_ENTS` type-2 record in
record order. The port cannot yet do that directly because `load_persons` builds
`PersonInstance` from SEF only; the equivalent port transform is:

1. Keep the saved type-2 record order (slot order). Drop SEF persons that have no
   matching saved occurrence (the original would not create them).
2. Match by **name occurrence**: for the k-th saved record with a given name, use
   the k-th SEF person with that name (same level). Do not use a global
   name→person map. `BT_PNAME` is only a parallel validation list/fallback.
3. Synthesize a `PersonInstance` for a type-2 record with no SEF occurrence by
   loading the person script/PAD/animations from the saved name; route cells come
   from the named SEF cell group. Corpus has three such records (Task 1 report).
4. Duplicate names are distinct entities: 81 duplicate-name groups, max
   multiplicity 6, over the 499 type-2 records of the 28 selected sections
   (`slot0` has two `L93_2.P292_Chernobogs_Guardian2` with different tiles and
   inventories; `slot21` has two `L0.P744_oficer1`). Every name-keyed store that
   merges duplicates breaks parity.

### RNG implications

- `FUN_1404b1c0` consumes RNG **per person only when `+0x26c0 != 0`, c0/c1 are
  valid, and the inventory script name is non-empty**; the first RNG decision is
  the chance gate at 51141-51142 (`FUN_14006288`, RVA `0x6288`, which calls
  `_rand` for a mixed probability), after the empty-script/container/.inv load at
  51092-51139. A failed gate leaves the serialized containers in place. Further
  draws come from item generation/specials. Order is `BT_ENTS` installation
  order.
- The port currently consumes RNG differently: `LevelSimulation::create` draws
  initial delays/goals for movable persons in `level.persons` (SEF) order
  (`level_simulation.cpp:109-135`), and `generate_npc_inventories` clears every
  person entry, regenerates loot only for persons with a non-empty inventory,
  and calls `consume_person_init_rng` for every non-Hero, all in SEF order and
  without the `+0x26c0` gate (`npc_loot.cpp:143-204`;
  `glgame/main.cpp:1018-1037`, `2062-2085`).
- Restore must apply RNG-consuming steps in `BT_ENTS`/slot order and skip loot
  generation when `+0x26c0 == 0`; if the port keeps `generate_npc_inventories`,
  its ungated generation and `consume_person_init_rng` must be gated and
  ordered. RNG parity is **not** claimed until the original call order and gates
  are replicated.

## §Port mapping / gaps

Status: **available** = field and decoder/wiring exist; **partial** = storage or
decoder exists but the restore input/wiring is missing; **missing** = no port
field or entry point; **unverified** = save semantics unresolved.

| Save/runtime data | Port target | Status | Gap / next action |
|---|---|---|---|
| `BT_ENTS` selected section, record order, slot install | `LevelSimulation.persons_`/`states_`; `Level.persons` | missing | no restore entry point; persons come from SEF `load_persons`; need ordered restore API and BT_ENTS-order creation |
| name `+0x48/+0x4c` | `PersonInstance.name`, `PersonState.name`, `PersonRuntime` index, `InventoryRuntime` keys | available | `PersonRuntime::create` drops the second duplicate (person_runtime.cpp:214-223) and `set_person_items` ignores it; use occurrence-qualified keys |
| current tile pair | `PersonMovement.x/y` | partial | `place_person` (level_simulation.cpp:268-284) teleports and resets movement only; direction/timers/route index not restored (`select_idle` is 286-315) |
| anchor `+0x5f8/+0x5fc` | `PersonInstance.tile_x/y`; `State.spawn_tile_x/y`, `spawn_x/y` | available | set before `LevelSimulation::create`; current tile must be applied after creation |
| direction `+0x28` | `PersonInstance.direction`; `PersonMovement.direction8/16` | partial | applied only at create; no runtime direction setter in `place_person` |
| route type `+0x151c` | `PersonInstance.route_type` (string), `State.route_type` (`RouteType`) | partial | save is numeric 0..5; only `parse_route_type(string)` exists (`person_movement.cpp`); add numeric mapping |
| route `+0x1524` / cells | `PersonInstance.route`/`route_cells`; `State.route` | available for SEF matches | type-2-only record needs standalone person-script/PAD loader; cells resolved from SEF cell groups |
| radius `+0x1520`, delays `+0x2668/+0x266c` | `PersonInstance.radius/delay_min/delay_max`; `State.radius/delay_*` | available | create maps them; no restore-specific issue |
| tribe `+0x150c` | `PersonInstance.tribe` (name) | missing | no `BT_TRIBE` parser or id→name map; `save_payload.cpp` only ignores the block |
| dialog `+0x267c`, inventory `+0x2aac` | `PersonInstance.dialog/inventory` | available | strings preserved; inventory name feeds loot regen |
| primary stats/skills | `PersonState.values`/`skills` via `PersonRuntime` | not in save | type-2 body has no primary-stat/skill fields; they come from the person script/prototype (`PersonRuntime::create`, person_runtime.cpp:246-263) |
| HP `+0x14fc`, EP `+0x1500` | `PersonRuntime::set_parameter(life/energy)` | available | apply after `PersonRuntime::create`; clamped to computed max |
| AP `+0x1504` | none | missing | `PersonParameter` has no AP (person_access.hpp:7-21); add before restore |
| route index `+0x1630` | `LevelSimulation::State.route_index` | missing | private, no setter; needed for MOVED/MOVED_FLIP before first advance |
| `+0x40`, `+0x44`, `+0x654`, `+0x658[8]`, `+0xff0[0x11c]`, `+0x6cc` | none | unverified | keep at prototype/SEF default or preserve raw; not claimed restored |
| c0/c1 items | `InventoryRuntime.persons_` (main), `trades_` (trade/hot); `InventoryItem`/`SpecialMod` | partial | entity-container decoder missing; `parse_hero_inventory` proves the layout but only for `BT_HERO`; c1 role (trade/money) is inferred |
| c2 items | none | missing | 14,156 `NPC_*` weapon/appearance records; no NPC equipment/outfit runtime |
| item modifiers `+0xC0/+0xC4` | `InventoryItem.mod_count`/`mods` | partial | save stores the u32 subcount plus `0x14*sub` 5×u32 subrecords (§Items, §Container walk); subrecord semantics unverified; entity-container decoder missing; the port keeps count only (`hero_equipment.hpp:41`) |
| NPC loot override | `generate_npc_inventories` (npc_loot.cpp:143-204) | missing | clears all person entries; regenerates loot only for non-empty inventory; `consume_person_init_rng` for every non-Hero; no `+0x26c0` gate/chance semantics; must be gated |
| `BT_RGINF` latch/datetime | `SaveRginf` parsed; `LevelSimulation.time_ms_` is monotonic | partial | no runtime `+0x26c0`; calendar/interval source not wired; decision needs `SaveInfo.time` and regenerate interval |
| attached `BT_HERO` list | none | missing | `BT_HERO` attached type-2 bodies are not parsed by b1a; no attached list in `Level`/`LevelSimulation` |
| type 8/16/32 entities | none | missing | outside b1b person scope; raw bytes are parsed by b1a |

Port-side restore order to implement (once a spec is approved): build the
restored `Level.persons` in `BT_ENTS` order (drop SEF-only, synthesize
type-2-only) → `LevelSimulation::create` → apply current tile/direction/route
index/HP/EP → apply serialized containers (occurrence-keyed) when the `+0x26c0`
decision is false → run/gate the loot override only when the decision is true.

## §Oracle plan

The oracle has three levels: static (executable now with the b1a parser plus a
type-2 decoder), port-restore (executable after b1b as a C++ test), and live
original (deferred; user-assisted read-only). No save write is used, so
“round-trip” is not an oracle.

### Canonical tuple

For each type-2 record, in `BT_ENTS` order, one TAB-separated line with fields
`name, cur_x, cur_y, anchor_x, anchor_y, direction, route_type, route, c0, c1,
c2`, where each container is `flag:def*count,def*count,...` (`flag` is the 0/1
presence word; empty item list is the empty string). Lines are joined with LF;
a non-empty list ends with LF. The digest is SHA-256 over the UTF-8 bytes. The
empty person list has digest
`E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855`.

### Pins (28-save corpus, 2026-11-14)

| slot | level | section | raw `BT_ENTS ` len / SHA-256 | `BT_PNAME` len / SHA-256 | `BT_RGINF` SHA-256 | latch | types | canonical SHA-256 | item records |
|---:|---|---:|---|---|---|---|---|---:|---:|
| 0 | `L93_4` | 8551 | 8381 / `559BE5898BC60B4FB6591F56FF8D6E62FE55BF8A9795D1A3CC384718895D7650` | 98 / `F721F1098B7D9D42B59D440A95FCE20F8EDE4B9B54ECEA83948239FBA8ED0485` | `27431ACF6DD0CB71869CEDA164F2121A2933973AA0883FE66C2CCAF392989175` | 0 | `{2:3, 8:1}` | `9DDAFA6D0FE451AAB2B4D9476D2B8C918731A6E4902C8393B5017749FA9BC410` | 14 |
| 2 | `L6_3` | 3110 | 3034 / `FD11E8359E6DCDCBC4B91371332F93516AE483171FF73368F3C31F59C5D7DA4D` | 4 / `DF3F619804A92FDB4057192DC43DD748EA778ADC52BC498CE80524C014B81119` | `D8074D7F89CF5A78B35290D68C99A755B71E19CF838BE95857E1AE63D12DDB0E` | 1 | `{16:11}` | `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855` | 0 |
| 21 | `L1_1` | 31525 | 31062 / `8593CBAFFD73885E0C470FA4BAE17DBCDE5C73E41C48CD56BED52B956A0571A0` | 391 / `38B9D85EBBF8CBAD851E47679D3A012603FDBDF46A99E93F65A1567BBBC3826A` | `8A53FFF313C82D33436510F88811AD7B3C54116FA50FC4AD0B64CD548C80C4E5` | 0 | `{2:16, 16:11}` | `72A83CF28C57FCDB4ACD3E96715D5E2C793F821C8EA0E9339CCC73CFB616ABA3` | 80 |

`slot0` canonical lines:

```
L93_2.P292_Chernobogs_Guardian2	131	115	131	115	4	0		1:812*597,420*1	1:812*289,421*2	1:1784*1
L93_2.P292_Chernobogs_Guardian2	103	122	103	122	5	0		1:812*688,919*50,920*50,921*50,317*1,318*1	1:812*561	1:1784*1
L93_4.P284_Chernobog	120	127	120	127	5	0		0:	1:	1:1661*1
```

`slot21/L1_1` canonical lines:

```
L1_1.P387_Poison_Plotnik	133	12	133	12	5	0		1:975*1	1:975*1	1:1630*1
L0.P678_citizen4	57	51	59	49	1	3	track4	1:812*93,421*3	1:812*72	1:1787*1
L0.P7_Kolbasens_Rodstvennik1	196	56	192	49	5	4		1:812*82,815*2	1:812*76,445*1,446*1,447*1,926*1	1:1781*1
L1_1.P389_Crying_Guardian	38	51	38	51	5	0		1:812*129,294*1,328*1	1:812*100,262*1,338*1,371*1	1:1654*1
L1_2.P404_Sister_Of_Mercy2	127	87	127	87	3	0		1:812*110,19*1,209*1,102*1,120*1,769*1,1515*1,1528*1	1:	1:1787*1
L0.P785_witch3	129	91	129	91	3	0		1:812*87,124*1,1466*1,1515*1,1528*1,1556*1	1:812*77	1:1787*1
L1_2.P403_Sister_Of_Mercy1	134	91	134	91	3	0		1:812*137	1:812*68	1:1786*1
L0.P693_citizen6	99	121	99	121	5	0		1:418*2	1:940*1	1:1786*1
L0.P711_citizen8	102	124	102	124	1	0		1:812*77	1:812*84	1:1780*1
L0.P692_citizen5	157	128	159	124	3	3	track1	1:812*81	1:812*117	1:1787*1
L0.P744_oficer1	66	136	66	136	7	0		1:812*83,263*1	1:812*92	1:1639*1
L0.P744_oficer1	186	147	190	148	1	2	track3	1:812*103,534*1	1:812*112,289*1,295*1	1:1639*1
L0.P640_children2	177	186	71	172	6	3	track2	1:812*87,926*1	1:812*112,442*1	1:1750*1
L1.P621_Svet_3CircusWorkerV	187	185	187	185	5	0		1:812*89,407*3,411*2	1:812*84,411*1	1:1780*1
L0.P733_artist1	191	188	191	188	1	0		1:812*128,407*3,415*3	1:812*82	1:1780*1
L1.P620_Svet_3CircusWorker2	168	199	173	189	3	4		1:812*114	1:	1:1787*1
```

### Checks and 0/N criteria

| # | Check | Pins | Criterion |
|---:|---|---|---|
| O1 | Raw selected-section bytes: `BT_ENTS `, `BT_PNAME`, `BT_RGINF` lengths and SHA-256 | slot0/2/21 | 3/3 digests equal; corpus 2599/2599 sections already consume exactly |
| O2 | Name/type sequence: decoded type-2 names == `BT_PNAME` names == canonical names; type histogram | slot0/2/21 | 3/3, 0/0, 16/16; corpus 2599/2599 identity |
| O3 | Position/direction/anchor: canonical digest and full line diff | slot0/2/21 | 3/3 digests; 14,507/14,507 records have direction 0..7; SEF anchor identity 496/496 (Task 1) |
| O4 | Inventory counts/ids: canonical container tuples; each `def` resolves in `items.pak`/`tech_names.sdb`; totals | slot0/2/21 | 14/14, 0/0, 80/80 item records; 115,405/115,405 ids resolve; any difference is reported as `d/N` and fails |
| O5 | Duplicate policy: repeated names keep distinct ordered tuples | slot0/2/21 | 2/2 (`Guardian2`), 2/2 (`oficer1`), no merge; corpus selected sections 81 groups, max multiplicity 6 |
| O6 | RGINF: saved latch/date decision | slot0/2/21 | raw digests 3/3; latch values 0/1/0; decision table unit test (below); runtime parity deferred |
| O7 | HP/EP restore (outside the canonical tuple) | slot0/2/21 | saved pairs below; after create, `PersonRuntime::get_parameter(life/energy)` equals the saved values: 3/3 slot0, 0/0 slot2, 16/16 slot21; a clamp delta fails unless explicitly waived |

RGINF decision unit test (synthetic clock; `interval = DAT_1407fbcc+0x8c`; the
`/0x1e` and `%0x1e` conversion is at 35834-35835, and the
`due = saved + interval <= now` reading is the one carried from
`p2-15b1-save-entities.md` §Restore flow):

| saved latch | due | expected `+0x26c0` | expected `+0x26a0` |
|---:|---|---:|---|
| 0 | true | 1 | now |
| 0 | false | 0 | saved |
| 1 | any | 0 | saved |

O7 saved HP/EP pairs, in `BT_ENTS` order (the canonical tuple and its digests
above deliberately exclude HP/EP):

- `slot0/L93_4`: `(136,73)`, `(136,73)`, `(826,136)`.
- `slot21/L1_1`: `(51,39)`, `(54,23)`, `(74,39)`, `(59,30)`, `(17,71)`,
  `(51,110)`, `(15,70)`, `(21,76)`, `(54,21)`, `(17,71)`, `(74,39)`,
  `(74,39)`, `(55,30)`, `(44,30)`, `(44,30)`, `(44,32)`.
- `slot2/L6_3`: no type-2 records (0/0).

### Executability and deferrals

- **Now (static):** O1-O6 are executable with `gl::assets::parse_save_payload`
  (raw bodies/hashes) plus a type-2 decoder (O6's raw digests and synthetic
  decision table; runtime parity remains deferred). The research numbers were
  produced by `%TEMP%\opencode\p2_15b1b_pins.py`
  (`D3E91BD2A0EE07A660D176F6A6BFB66D0D5309132DDDBA5335C060E310F5DEB2`),
  `p2_15b1b_dups.py`
  (`D194A686FEAFA4E2A3A548E432D58629E98606288702A02A6B5946D6C84032C7`) and
  `p2_15b1b_oracle.py`
  (`425463D776F92C256A58719DEB91C740259B810EF5593C69B20A9C5E04F73C72`);
  scripts are not committed.
- **After b1b (port test):** build `LevelSimulation` from SEF, apply the restore
  API, then compare its `persons()`/state/`PersonRuntime`/`InventoryRuntime` to
  the canonical lines and O7 HP/EP pairs. Duplicate names must be keyed by
  occurrence; a flat-name map is a test failure.
- **Deferred (live original, user-assisted read-only):** load slot0, slot2,
  slot21; read `DAT_14088dc4+0x2564/+0x2568/+0x256c`, `+0x26c0`, `+0x26a0[8]`,
  and per type-2 person `+0x14/+0x4c/+0x604/+0x608/+0x28/+0xc/+0x10/+0x109c`;
  walk `DAT_14088df8` with the Task 2 map. Report `0/N` differing slots. This is
  the only path that proves the runtime container override; no live run was
  performed here.
- **Deferred:** latch-1-with-persons regeneration pin. `slot2` exercises latch 1
  but has zero persons. `slot3/L93_4` (section 6323, latch 1, 3 persons) is the
  next pin once the entity-container decoder exists; until then the chance-gate
  and override semantics are decompiled-only.

## Unknowns and exact next checks

1. **`+0x654`** — always 0 in 14,507/14,507 records; only consumer is
   `FUN_1401ab74` 19345, where it combines with `+0xf28` to choose the
   tribe-relation id (`"hero"`). Next: decompile the writer method
   `0x1ec40`/`FUN_14032c2c` and any script native that writes `+0x654`, then
   live-diff one NPC before/after the condition that flips it.
2. **`+0x658` (8×u32)** — set from prototype `+0x558..+0x574` only when the
   prototype's first dword is non-zero (flag 18093-18094, copy 18097-18105), but
   serialized always.
   Next: identify the PersonDef fields at `+0x558..+0x574` (search writes in
   the def loader) or live-diff an NPC after an equipment/appearance change;
   check `Client.dll` for a consumer.
3. **`+0xff0` (0x11c bytes)** — opaque block; get/copy only. Next: resolve the
   vtable `+0x50` provider used at 18019-18025 (`DAT_140891bc+0x98`) and the
   callers of `FUN_140508cc`/`FUN_14050984` (no static callers); compare with
   the client render/wear state.
4. **Skipped u32 (reader 22067, `FUN_1400126c`)** — value is discarded by the
   reader. Next: disassemble `0x126c` and the writer's matching emit to identify
   what the writer stores there and whether it is a prototype id.
5. **`+0x44` literary-name mode** — `{0,1,2}`; `.sdb` forces 0; `1` appears
   only on RANDOM, `2` on STAY/RANDOM/RANDOM_RADIUS. Next: decompile the
   prototype `[0x84]`/`[0x85]` producer and the name-generation callback at
   18050, then live-diff a RANDOM citizen after its name is assigned.
6. **`+0x6cc`** — always 1; gates the attack/flee message in `FUN_1401a24c`.
   Next: live-diff a passive/scripted NPC or find a native that sets `+0x6cc`
   (corpus has no zero sample).
7. **AP restore** — `+0x1504` is a verified current AP value, but the port
   `PersonParameter` enum has no AP slot (`person_access.hpp:7-21`). Next: add
   an AP parameter in the P3 parameter spec before restore maps it.
8. **Item modifier subrecords (`+0xC4 + i*0x14`, 5×u32)** — offsets are proven,
   semantics are not. Observed word0 ∈ 0..57, word1 ∈ -50..500, word2 ∈
   `{1,65,66,68,80,96,129,130,132,144}`, word3 small/`2560/5120/7680`, word4
   ∈ `{0,-1}`. Next: decompile the item-effect consumer of `item+0x78/+0x7c`
   (search the same 5-word stride) and map word0 through the skill/parameter
   tables; verify one record by changing a stat in-game.
9. **`+0x0C` flags bits 7/9** — 59/198 sampled records differ from the static
   ITM `flags` only in bit 7 or bit 9. Next: find the item-generation/prefix
   code that toggles these bits and whether they are restore-relevant.
10. **`+0xB8` type-6 field** — copied from def `+0x9c` only when type == 6
    (`FUN_1404f4fc` 54646-54651); serialized for every item. Next: identify the
    def `+0x9c` field and its consumer for ranged items.
11. **Container roles c0/c1/c2** — content strongly suggests general inventory /
    trade-money / NPC appearance-weapon, but no consumer names them. Next:
    decompile the callers of `FUN_1404bf8c(DAT_14088df8, entity+0x10)` and
    `entity+0x109c` (natives/economy) and compare with the trade UI.
12. **`+0xA4` instance id lifetime** — 0..3,128 per save section and not
    sequential inside one container; it is the world item-table index, not a
    stable save id. Next: check whether restore must remap it or may drop it.
13. **SEF persons are not an original restore input.** `FUN_140325fc` reads only
    SEF entrances/cell groups; `FUN_14018990` is the level-spawn person
    constructor. Next: refactor `load_persons` into a by-name person loader so a
    type-2-only record can synthesize a `PersonInstance` from its saved name;
    test the three corpus records from Task 1.
14. **Duplicate-name identity.** 81 duplicate-name groups (max multiplicity 6)
    over the 499 type-2 records of the selected sections; `PersonRuntime::create`
    drops the second duplicate (person_runtime.cpp:214-223) and
    `set_person_items` ignores it (inventory_runtime.cpp:490-500). Next:
    occurrence-qualified keys or per-entity container identity; assert the
    slot0/21 duplicate tuples.
15. **Attached `BT_HERO` list.** b1a parses only `BT_ENTS`/`BT_PNAME`/`BT_RGINF`;
    the count-prefixed attached list after the hero body is not parsed. Next:
    locate the attached count/bodies in `BT_HERO` (35670-35684), parse with the
    type-2 body decoder, and model the marker `+0x2bc0=1`/`+0xf28=0`.
16. **`BT_RGINF` runtime decision.** The port has no `+0x26c0`; the interval
    source (`DAT_1407fbcc+0x8c`) and the datetime arithmetic are not wired.
    Next: decode the config interval and unit-test the O6 decision table against
    synthetic clocks before claiming runtime parity.
17. **Latch-1 regeneration pin.** `slot2/L6_3` has latch 1 but zero persons;
    `slot3/L93_4` (section 6323, latch 1, 3 persons) is the first real override
    pin. Next after the entity-container decoder: run the slot3 canonical pin
    and verify c0/c1 chance-gate/replacement behavior; c2 must stay serialized.

## Open Questions

- OQ-13 (SEF `person.tribe` name -> numeric `BT_TRIBE` id) is **resolved for
  restore** at the save boundary: read `+0x150c`, look it up in the save's
  `BT_TRIBE` `(id,name)` table, and use the name. The port still needs a
  `BT_TRIBE` parser and a name->id reverse map if it must write/compare ids;
  this is a port gap, not a save-format unknown.

## Verdict / decomposition

Task 4, 2026-11-14. Docs-only; no `src/`/`tests/`/`tools/` changes.

### Verdict: `scoped`

The save-side restore is implementable per part, not as one increment.

- **Proven and implementable now:** type-2 identity/position/anchor/direction,
  route type/name, radius, delays, dialog/inventory names, HP/EP, and
  `BT_ENTS` installation order. 496/496 SEF matches for anchor, route type,
  radius, delays, tribe, literary name; 14,507/14,507 parsed type-2 bodies;
  `BT_PNAME` == type-2 order 2599/2599. SEF persons are **not** an original
  restore input (restore path never calls `FUN_14018990`).
- **Format proven, port entry point missing:** entity containers use the same
  `0xC0 + u32 sub + 0x14*sub` record as `BT_HERO` (writer/reader identity,
  marker `0x14092248` 115,405/115,405, in-record/trailing subcount equal
  115,405/115,405); all 115,405 definition ids resolve in `items.pak` and
  `tech_names.sdb`. The port still has no entity-container decoder
  (`parse_hero_inventory` covers `BT_HERO` only), so `b1b2` starts with that
  decoder. Container roles c0/c1/c2 remain inferred.
- **Decompiled, not live-verified:** the `+0x26c0` chance-gated c0/c1
  regeneration (`FUN_1404b1c0`, gate 51141-51142) and c2 preservation. The port
  currently regenerates NPC loot in SEF order with no `+0x26c0` gate and no
  chance semantics. `b1b3` is therefore a gate/ordering task, and RNG parity is
  not claimed.
- **Not restored in this scope:** `+0x44`, `+0x654`, `+0x658[8]`,
  `+0xff0[0x11c]`, `+0x6cc`, the skipped u32, AP (`+0x1504` has no
  `PersonParameter`), `+0x40` (no port field), item-modifier semantics,
  `+0xA4` remap, attached `BT_HERO` list, tail entries, type 8/16/32, and the
  live container-manager oracle. These are explicit gates, not guessed behavior.

### P2-15b1b1 — restore position/direction/params

**Prerequisites:** `BT_TRIBE` parser and id->name map (b1a skips the block);
numeric route-type mapping (port only has `parse_route_type(string)`);
`State.route_index` setter; a by-name person loader so a type-2-only record can
synthesize a `PersonInstance` (person script/PAD/animations) with route cells
from the named SEF cell group.

**Scope:**

1. Add an ordered restore entry point over the selected `BT_ENTS` section:
   create persons in record/slot order; drop SEF persons with no saved
   occurrence; synthesize the three corpus type-2-only records; keep duplicate
   names as distinct entities (occurrence-qualified keys, no flat name map).
2. Apply: name; current tile pair -> `PersonMovement.x/y` after
   `LevelSimulation::create`; anchor `+0x5f8/+0x5fc` ->
   `PersonInstance.tile_x/y`, `State.spawn_tile_x/y`, `spawn_x/y`; direction
   `+0x28` -> `PersonInstance.direction`/`PersonMovement.direction8`; route
   type `+0x151c` (0..5) and route `+0x1524`; radius `+0x1520`; delays
   `+0x2668/+0x266c`; dialog `+0x267c`; inventory name `+0x2aac`; HP `+0x14fc`
   and EP `+0x1500` via `PersonRuntime::set_parameter`; route index `+0x1630`
   for MOVED/MOVED_FLIP before the first advance; tribe `+0x150c` -> name.
3. Explicitly leave at prototype/SEF defaults, not claimed restored: `+0x40`
   (no port field), AP, `+0x44`, `+0x654`, `+0x658`, `+0xff0`, `+0x6cc`, tail,
   attached list.

**Acceptance:**

- O1 raw section digests (3/3), O2 name/type sequence (3/3, 16/16, corpus
  2599/2599), O3 canonical position/direction/anchor digest and full 14,507
  line diff, O5 duplicate tuples (2/2 + 2/2; 81 groups, max multiplicity 6),
  O7 HP/EP pairs (3/3 slot0, 0/0 slot2, 16/16 slot21).
- New unit tests: numeric route mapping, occurrence matching, SEF-only drop,
  type-2-only synthesis (3 records), duplicate ordering.
- No RNG-parity claim in this part.

### P2-15b1b2 — inventory/containers

**Explicit gate:** implement the entity-container decoder before any restore
claim. The record layout is proven (see §Items/§Container walk), but the port
has no decoder for entity containers; the selected-section save walk must also
treat the leading word as the literal 0/1 presence flag, not a runtime id.

**Scope:**

1. Decode c0/c1/c2 for each selected-section type-2 record: presence word,
   count, and item records with the proven fields (`+0x00` marker, `+0x08`
   type, `+0x0C` flags, `+0x10` price, `+0x14` weight, `+0x18` unknown_a,
   `+0x20` style, `+0x30` material, `+0xA4` instance id, `+0xA8` definition id,
   `+0xAC` container id, `+0xB0` count, `+0xB4` durability, `+0xB8`, `+0xBC`,
   `+0xC0`/`+0xC4` subcount and 5×u32 subrecords).
2. Apply serialized c0 items to the occurrence-qualified main inventory and c1
   items to the trade/money side, preserving stacks/counts and storing the
   modifier subcount plus subrecords (the port currently keeps count only,
   `hero_equipment.hpp:41`). Do not interpret modifier semantics.
3. c2: keep decoded raw/structured; no port NPC equipment/outfit runtime, so no
   application claim.

**Acceptance:** O4 container tuples 14/14 slot0, 0/0 slot2, 80/80 slot21; all
115,405 definition ids resolve; any per-record difference reported `d/N` and
fails. Container roles remain `inferred` until a consumer is decompiled.

**Deferred gate:** the live runtime override (`+0x26c0` true -> c0/c1
chance-gated replacement) and the `DAT_14088df8` container-manager oracle need
a live read-only run; until then only the save-file walk is claimed.

### P2-15b1b3 — NPC loot override/ordering/gate

**Scope:**

1. Add the runtime `+0x26c0`/`+0x26a0` decision from `BT_RGINF` (saved latch +
   datetime; interval from the config source) with the O6 synthetic decision
   table unit test.
2. Order restore/loot steps in `BT_ENTS` installation order, not SEF order;
   keep duplicate names distinct.
3. `+0x26c0 == 0`: keep the serialized c0/c1 records and do not regenerate.
   `+0x26c0 == 1`: per person, only with a valid c0/c1 and non-empty
   `scr_inv`, run the chance-gated `FUN_1404b1c0` (force flag 0); a failed
   chance gate keeps the serialized containers. c2 is never regenerated.
4. Gate or replace the current ungated `generate_npc_inventories` and
   `consume_person_init_rng` (every non-Hero, SEF order) so they cannot clear
   or regenerate restored entities outside this decision.

**Acceptance:** gate/ordering unit tests plus O6 raw digests and latch values
0/1/0. RNG parity is **not** claimed: the original pre-loot weapon/spawn draws
and the full loot call order are not replicated. The latch-1-with-persons pin
`slot3/L93_4` is deferred until the `b1b2` decoder exists; live read-only
verification is deferred.

### Out of scope (all of P2-15b1b)

- Save write and `game_save` (P2-15c).
- Type 8/16/32 restore, `BT_SCRN`, and block semantics outside persons.
- Attached `BT_HERO` persons (requires a b1a `BT_HERO` attached-list parser and
  a `Level` attached list).
- Unverified/inferred fields `+0x44`, `+0x654`, `+0x658`, `+0xff0`, `+0x6cc`;
  `+0x40` literary-name id and AP have no port target.
- Item-modifier semantics, `+0xA4` instance-id remapping, c2 NPC equipment.
- Live container-manager oracle and any RNG-stream parity claim.
