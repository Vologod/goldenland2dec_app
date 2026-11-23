# P6a11 — Door state native research

Status: complete (2026-11-21)
Scope: static reverse; `docs` + tool scripts only, no product code.

## Sources

- `I:\Games\Златогорье-2\Server.dll`, SHA-256
  `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`,
  PE32, image base `0x140000000`. RVA = address − `0x140000000`.
- `I:\Games\Златогорье-2\GoldenLand.exe`, SHA-256
  `7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF`,
  PE32, image base `0x400000`.
- Data: `levels/lvl/*.lvl` (175), `levels/single/*/*.sef` (247), extracted
  with `glpak`; scripts in `%TEMP%\opencode` (not committed).
- Tools: `llvm-objdump` 2026-10-06, Ghidra 12.1.4 decompiler output
  (`.local/decomp/server|client/decompiled.c`).

## Native `RS_SetDoorState` (0x7000014, RVA `0x41c74`)

- Type gate `FUN_14038c6c(types = [3,0], terminator -1)`: string + int.
  On failure returns `fldl 0x14091520` (`0.0`).
- Calls `FUN_14029388(server, arg0_string)` and passes its result plus
  `arg1_int` to `FUN_14024228`; returns `0.0` on every valid-type path.
- No null check: an unknown door name dereferences `0` in the original.

### `FUN_14029388` (RVA `0x29388`) — door lookup

- Iterates entity ids through server vtable `+0xf8` (first) / `+0xfc`
  (next); `-1` ends the scan.
- Entity storage: array `server+0x2564`, alive bitmap pointer `+0x2568`,
  count `+0x256c`. Entity is taken only when its id bit is set.
- Match: `(entity+0x14 & 0x3e) == 0x20` (`EF_DOOR`) and exact,
  case-sensitive `entity+0x4c` string equality with the argument.
- Returns the entity pointer or `0`.

### `FUN_14024228` (RVA `0x24228`) — set state and cell flags

- `state = arg` is stored at `entity+0x24`.
- `state == 0` (closed): for each cell of the door's cell group call world
  vtable `+0x78(group_index, 0x100)` and `+0x78(group_index, 0x1000)`
  (set movement and sight flags).
- `state != 0` (open): `+0x74(group_index, 0x100)` and
  `+0x74(group_index, 0x1000)` (clear both flags).
- `group_index` is `entity+0x368`.

## World cell flags (`GoldenLand.exe`)

- World object: engine interface `+0xa4` (`DAT_00469d64`), vtable
  `0x484a00`; the server reads it as `DAT_14088e30`.
- Slots: `+0x64 = 0x426234` get flag (shifts by `(x&1)+2*(y&1)`),
  `+0x68 = 0x426290` set flag (`OR`), `+0x6c = 0x4262ec` clear flag
  (`(x | f) ^ f`, i.e. `x & ~f`), `+0x74 = 0x4263d0` clear flag for every
  cell of a group, `+0x78 = 0x426430` set flag for every cell of a group.
- Flags: `0x100` movement (used by server pathing), `0x1000` sight.

### Door entity registration (`FUN_14023fb4`, RVA `0x23fb4`)

- `+0x14 |= 0x200`.
- World `+0x14(entity+0x4c)` returns an object index `i`; world `+0x2c(i)`
  returns `world + 0x24154 + i*0xC60`; `+0x31c` inside that record is a
  cell-group name; world `+0x18(name)` maps it to `entity+0x368`.
- `+0x36c` receives `FUN_14028a20(server)`, then
  `FUN_14024228(entity, entity+0x24)` applies the initial state.

### LVL `BLK_DOOR` / `BLK_CGRP`

- `GoldenLand.exe` `BLK_DOOR` parser at VA `0x4272d0..0x42769d`:
  `u32 count`, then per record six `u32 length + cp1251` strings copied
  into `world+0x24154 + i*0xC60`:
  `+0x000` name, `+0x108` open trigger, `+0x210` close trigger,
  `+0x318` cell-group name (`+0x31c` chars), `+0x420` fifth string
  (`default` in all observed records), `+0x528` sixth string
  (static object name).
  The name is inserted into the map at `world+0x3d58c` with value `i`.
- `BLK_CGRP` parser at VA `0x4277bd`: names and cell pairs are inserted
  into the group map at `world+0x3d5bc` (name → group index).
- Map lookup returns `-1` when the key is absent (`0x403754` default
  argument).

### SEF `doors:`

- Loaded by `FUN_140315f4` (RVA `0x315f4`) as entity type `0x20`; the
  per-entity reader is `FUN_14024584` (RVA `0x24584`):
  - `name` → `+0x48`/`+0x4c`;
  - `literary_name_open` → `+0x2c`;
  - `literary_name_close` → `+0x30`;
  - `script_name` → `+0x158`;
  - `cells_name` → `+0x260`;
  - `is_opened` → `+0x24`;
  - `is_inversed` → `+0x28`.
- Unknown keys are ignored. After the block, `FUN_14023fb4` registers the
  entity and applies `+0x24`.
- Constructor `FUN_14024858` (RVA `0x24858`) calls base `FUN_1405cb04`,
  which zeroes dwords `+4..+0x3c`; therefore `+0x24` defaults to `0`
  (closed) when `is_opened` is absent.

## Data scan (real files)

- 97 `levels/single` SEF files contain `doors:`; 201 door entries total.
- Fields: `literary_name_open`/`literary_name_close`/`cells_name` in
  201/201; `is_opened` present in 98 and always `1`; `is_inversed` never
  present; `script_name` in 3.
- Resolution uses the SEF `pack` field, not the SEF folder name: all 201
  entries resolve to a matching `levels/lvl/<pack>.lvl` `BLK_DOOR`
  record and every one of those records has a `BLK_CGRP` group
  (missing names 0/201, missing groups 0/201).
- LVL `BLK_DOOR`: 175/175 files parse exactly, 226 records. One
  LVL-only record in `l9_2_1` (`door_closet`) references
  `__NOT_DEFINED__`, which is absent from `BLK_CGRP`; that name has no
  SEF door entity in the shipped data, so the port never resolves it.
- Door group cells over all 226 LVL records: 5,481 total; 626 are
  already blocked by the static LVL movement bit, 4,855 are free.
  Closed doors (state 0) therefore add blockers; open doors clear the
  flag on the whole group.
- 98 SEF doors start with `is_opened=1` (149 group cells were already
  blocked, 4,722 free): the original clears the movement bit on all of
  them at load, including cells blocked for other reasons. The port
  applies all 98 open entries through the pack-resolved LVL.

## Exact port rule

For each SEF door resolved through the LVL name → cell-group map:

- `state == 0` → set movement-blocked on every group cell;
- `state != 0` → clear movement-blocked on every group cell.

The movement bit (`0x100`) is the one used by `WalkGrid`; the sight bit
(`0x1000`) stays out of scope (same residual as P4b2). Names are exact
and case-sensitive. Unknown names are a port no-op (original AV).

## Verification script

`%TEMP%\opencode\door_grid_pack.py` (not committed) rebuilds the static
grid from `BLK_MHDR`, resolves each SEF door through its `pack` LVL and
counts blocked group cells; output:

```text
open doors 98 cells blocked 149 cells free 4722
```

`%TEMP%\opencode\cross_doors_pack.py` (not committed) checks resolution
for all 97 SEF files: `levels 97 doors 201 open 98`, missing names 0,
missing groups 0. `door_grid_analysis.py` (folder-name matching) is
superseded by these pack-based scripts; folder names differ from packs
(e.g. `l10_1_2.sef` uses pack `l10_1`).
