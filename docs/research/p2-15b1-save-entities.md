# P2-15b1 — Save entities/persons research

Status: complete (Tasks 1–5/5, 2026-11-13); verdict `scoped`.
Phase: P2-15b1 (research-only; follow-up to P2-15a/P2-15b).
Scope: `docs/research/` only; no production code changes.

## Provenance

| Artifact | SHA-256 |
|---|---|
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| `Saves/slot0.sav` | `8AC545FE9715EF11C68B1CCA8617BEB03003ECA360F07F71C26E3FC5E5C73442` |
| 28 saves (slot0..slot27, uppercase per-file SHA-256 joined by `\n`) | manifest SHA-256 `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |

Source: `I:\Games\Златогорье-2\Saves` (28 files `slot0.sav` … `slot27.sav`).
Re-verified 2026-11-13 with `Get-FileHash -Algorithm SHA256` for
`Server.dll`/`slot0.sav`/`decompiled.c`; the scan recomputed the manifest from
the per-file hashes and matched the value above.

## §Tail inventory

### Command

```powershell
py -3 "$env:TEMP\p2_15b1_scan.py"
```

The scan script is kept in `%TEMP%` and is not committed. It writes the full
per-save/per-section log to `%TEMP%\p2_15b1_scan.out.txt`; the tables below are
taken from that output. Script SHA-256:
`399FB2E603068EC4614C4CE8EBE8C85252BFD4A680FE1D1AAFC2C5B771826F78`
(`14896` bytes); the durable oracle is the `P2-15b1a` real-data test over the
28-save corpus, not this ephemeral script.

### Framing used

Follows `docs/research/p2-15a-save-payload.md` §Minimum read set:

1. `raw[0:0x70]` header; `u32(raw,0x68)` = decompressed payload length,
   `u32(raw,0x6c)` = compressed zlib length; payload is
   `raw[0x70 : 0x70 + compressed_len]`.
2. Decompressed payload is seven `tag(8) + u32 size + body` blocks ending exactly
   at the decompressed length.
3. `BT_INFO ` body: field 5 (third length-prefixed cp1251 string) is the level id.
4. Tail dictionary starts at `0x70 + u32(raw,0x6c)`:
   `u32 count`, then per entry `u32 name_len + name + u32 section_len + section`.
   A section is `u32 section_raw_len + u32 section_zlib_len + zlib` with
   `section_zlib_len == section_len - 8`.
5. Each decompressed section contains blocks in the order
   `BT_PNAME` → `BT_RGINF` → `BT_ENTS `.

`BT_PNAME` bodies have no leading count: they are a sequence of
`u32 len + cp1251 bytes` terminated by a zero-length entry (`u32 0`). The entry
count is the number of non-empty names. This framing parses exactly (terminator
and end-of-body) in 2599/2599 sections; a 0-entry body is exactly 4 bytes.
`BT_RGINF` is a fixed 36-byte body. `BT_ENTS ` is the last block, and its `u32`
is the remaining snapshot byte count.

### 28/28 summary

```text
files 28 errors 0
manifest_sha256 F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B
total_sections 2599
tag_presence {'BT_ENTS ': 2599, 'BT_PNAME': 2599, 'BT_RGINF': 2599}
unknown_tags {}
orders {'BT_PNAME,BT_RGINF,BT_ENTS ': 2599}
pname_present 2599 pname_parse_fail 0 pname_names_total 14507
rginf_lens {36: 2599}
ents_sections 2599 ents_decl_ok 2599
ents_len min=67 max=138768 distinct=570
section_raw min=143 max=139858 distinct=572
```

All 28 saves were scanned with 0 errors. Every tail section (2599 total) has
exactly the three blocks in the same order: `BT_PNAME` 2599/2599,
`BT_RGINF` 2599/2599 (always 36 bytes), `BT_ENTS ` 2599/2599. The `BT_ENTS `
declared size equals the remaining snapshot bytes in 2599/2599 sections. The
identity `section_raw = 72 + pname_body_len + ents_len` holds for every section
implied by the exact block consumption; for slot0/L93_4 it is
`72 + 98 + 8381 = 8551`.

### Per-save selected-level section (28/28)

`BT_INFO ` level id selects the section used by the save; all sections have the
same three-block shape. Columns: slot, level id, tail entries in the save,
selected section raw length, `BT_PNAME` entries, `BT_RGINF` body length,
`BT_ENTS ` body length.

| slot | level | tail entries | section raw | PNAME entries | RGINF | ENTS body |
|---|---|---|---:|---:|---:|---:|
| 0 | L93_4 | 213 | 8551 | 3 | 36 | 8381 |
| 1 | L2_1 | 32 | 96059 | 30 | 36 | 95296 |
| 2 | L6_3 | 202 | 3110 | 0 | 36 | 3034 |
| 3 | L93_4 | 204 | 6323 | 3 | 36 | 6153 |
| 4 | L6_1 | 108 | 74880 | 35 | 36 | 74000 |
| 5 | L6_2 | 112 | 130258 | 44 | 36 | 129089 |
| 6 | L1_2 | 2 | 116767 | 40 | 36 | 115816 |
| 7 | L9_2_1 | 124 | 29782 | 17 | 36 | 29280 |
| 8 | L9_3 | 124 | 44555 | 25 | 36 | 43924 |
| 9 | RL_2 | 73 | 4281 | 1 | 36 | 4181 |
| 10 | L3_1 | 75 | 37663 | 14 | 36 | 37252 |
| 11 | L25_1 | 77 | 2208 | 1 | 36 | 2106 |
| 12 | L3_3 | 77 | 112602 | 38 | 36 | 111647 |
| 13 | L4_2_1 | 79 | 1654 | 1 | 36 | 1548 |
| 14 | L2_2_1 | 79 | 31824 | 17 | 36 | 31334 |
| 15 | L2_2_1 | 80 | 31824 | 17 | 36 | 31334 |
| 16 | L2_2_1 | 80 | 30247 | 16 | 36 | 29775 |
| 17 | L3_3 | 80 | 109338 | 38 | 36 | 108383 |
| 18 | L18_1 | 82 | 216 | 0 | 36 | 140 |
| 19 | L18_2 | 83 | 28277 | 11 | 36 | 27958 |
| 20 | L18_2 | 85 | 2613 | 1 | 36 | 2505 |
| 21 | L1_1 | 3 | 31525 | 16 | 36 | 31062 |
| 22 | L3_2 | 85 | 30966 | 14 | 36 | 30513 |
| 23 | L4_1 | 85 | 71962 | 24 | 36 | 71305 |
| 24 | L2_1 | 85 | 119081 | 29 | 36 | 118328 |
| 25 | L19 | 86 | 46262 | 26 | 36 | 45648 |
| 26 | L53 | 90 | 18478 | 1 | 36 | 18375 |
| 27 | L3_3 | 94 | 108689 | 37 | 36 | 107753 |

All 28 selected sections have `BT_PNAME`, `BT_RGINF` (36) and `BT_ENTS `;
PNAME entry counts range 0..44, and two selected sections have 0 names
(slot2/L6_3 and slot18/L18_1, which still carry the 4-byte PNAME terminator).
slot0/L93_4 (`section=8551`, `rginf=36`, `ents=8381`) matches the P2-15b load
result recorded in `docs/STATUS.md`.

### `BT_PNAME` entry-count histogram (all 2599 sections)

| entries | sections | entries | sections | entries | sections |
|---:|---:|---:|---:|---:|---:|
| 0 | 537 | 14 | 53 | 31 | 7 |
| 1 | 877 | 15 | 12 | 33 | 24 |
| 2 | 248 | 16 | 5 | 35 | 6 |
| 3 | 184 | 17 | 23 | 36 | 1 |
| 4 | 116 | 18 | 24 | 37 | 7 |
| 5 | 9 | 19 | 25 | 38 | 18 |
| 6 | 61 | 20 | 9 | 39 | 26 |
| 7 | 7 | 22 | 3 | 40 | 3 |
| 8 | 56 | 23 | 2 | 43 | 7 |
| 9 | 56 | 24 | 41 | 44 | 5 |
| 10 | 4 | 25 | 4 | 48 | 7 |
| 11 | 13 | 26 | 54 | | |
| 12 | 11 | 29 | 22 | | |
| 13 | 28 | 30 | 4 | | |

39 distinct counts, 0..48 entries; 537 sections have no names, 14,507 names
overall. `parse_fail = 0`.

### `BT_ENTS ` body-length histogram (all 2599 sections)

| range (bytes) | sections |
|---|---:|
| 0 | 0 |
| 1–100 | 152 |
| 101–1000 | 367 |
| 1001–10000 | 1375 |
| >10000 | 705 |

`BT_ENTS ` body: 67..138,768 bytes (570 distinct); section raw: 143..139,858
bytes (572 distinct). Tail entries per save: 2 (slot6) .. 213 (slot0).

### Observations

- Every tail section is a complete per-level snapshot with the same three-block
  shape; no section omits `BT_PNAME`, `BT_RGINF` or `BT_ENTS ` (2599/2599).
- `BT_RGINF` is always 36 bytes (2599/2599), consistent with the P2-15a
  `u32 + 0x20` description; semantics resolved in §RGINF (`u32` latch + `0x20`
  datetime).
- `BT_ENTS ` always ends the snapshot and its size field is a byte length, not
  an entity count (declared size == remaining bytes 2599/2599).
- `BT_PNAME` has no leading count and is a zero-terminated sequence of
  length-prefixed names; the 0-entry shape is a lone 4-byte zero terminator.
- Names are cp1251 and carry an origin-level prefix. The prefix is not always the
  section level: slot0's `L1_1` section contains `L1_1.P387_Poison_Plotnik`,
  `L0.P678_citizen4`, `L1.P621_Svet_3CircusWorkerV`; the `L93_4` section contains
  `L93_2.P292_Chernobogs_Guardian2`, `L93_4.P284_Chernobog`. PNAME→`BT_ENTS`
  mapping is therefore not established by name equality alone.

### Unknowns (explicit)

- `BT_RGINF` 36-byte field semantics and restore relevance are resolved in
  §RGINF (latch `+0x26c0` + 0x20-byte last-regeneration game datetime).
- `BT_ENTS ` entity type map and per-type body layout/terminator consumption are
  resolved in §Type map, §Body layout and §Parse algorithm; the 28/28 and
  2599/2599 exact-consumption result is in §Parse algorithm.
- `BT_PNAME` producer rule and its relation to the section's `BT_ENTS `
  records (lookup at decompiled line 38563) are resolved in §PNAME; the
  origin-level prefix meaning remains `unverified` there.
- Only the `BT_INFO ` level's section is loaded by the original save reader
  (`FUN_140325fc`, P2-15a); whether other sections are ever needed for restore
  is out of scope for this inventory.

## Type map

Factory `FUN_1405c83c` (RVA `0x5c83c`, size 409) is a five-case dispatch on the
entity type value. For a matched type it allocates the class size with
`FUN_140057cc(size,1,0,0)`, calls the constructor, and then stores the raw type
at object `+0x14` (`puVar1[5] = param_1`, decompiled 63002; disasm
`0x1405c940 mov [eax+0x14],edx`). Decompiled lines 62935-63005; raw
disassembly `0x1405c83c-0x1405c9d4` (`0x5c9d5-0x5c9d7` is NOP padding).

Callers (`functions.csv` `callers=7`): 29525 (type 8), 35089 (dynamic
`param_2`), 35671 and 35677 (`BT_HERO ` hero + attached list, type 2), 35851
(`BT_ENTS` dynamic type, `FUN_140325fc`), 36646 (type 4), and 38270/38432
(type 2). The `BT_ENTS` list is terminated by type **1** before the factory is
called (35848, 35913). The `BT_HERO ` attached list is not type-terminated: it
is count-prefixed (`FUN_1400129c` at 35673, loop 35676-35683). Type 1 is
therefore only the `BT_ENTS` end marker, not a class.

Serialization is **inferred** to run through vtable slot `+0` and
deserialization through slot `+4`: `FUN_14032c2c` calls
`(**(code **)*puVar2)(local_88)` at 36103, while `FUN_140325fc` calls
`(**(code **)(*piVar8 + 4))(local_6f0,uVar4)` at 35853. The `+0` label rests
only on that call site; bodies `0x1ec40`/`0x24aa8`/`0x2335c`/`0x24298` are
absent from `functions.csv` and `decompiled.c`. The writer's `BT_ENTS` dispatch
is gated at 36097 by `(entity+0x14 & 0x3a) != 0`, and `BT_PNAME` at 36006 by
`(entity+0x14 & 0x3e) == 2` plus `entity+0x150 == 0`; these masks read the same
`+0x14` type dword that the factory stores.

| type | name | factory RVA | ctor RVA | class size | vtable VA | reader (+4) RVA | class/notes |
|---:|---|---|---|---:|---|---|---|
| 1 | terminator | — | — | — | — | — | `BT_ENTS` end marker; consumed before the factory (35848, 35913) |
| 2 | `EF_PERSON` | `0x5c83c` | `0x18f7c` | `0x2bc8` | `0x1408e4a8` | `0x1e758` | person; ctor chains `FUN_14022368` (0x22368, vtable `0x1408e470`) then overrides; reader 21905-22096 |
| 4 | `EF_MAGIC` | `0x5c83c` | `0x22cec` | `0x1a8` | `0x1408f3f0` | none (stub `0x19780`) | magic; base `FUN_1405cb04`/vtable `0x14092cb8`; `+4` = `ret 8`, `+0` = `ret 4`; excluded from `BT_ENTS` by the writer's `&0x3a` mask before dispatch |
| 8 | `EF_INVCONTAINER` | `0x5c83c` | `0x24a30` | `0x15c` | `0x1408f778` | `0x24928` | inventory container; reader has no `functions.csv`/decompiled.c entry (Ghidra gap) |
| 0x10 | `EF_TRIGGER` | `0x5c83c` | `0x23e74` | `0x494` | `0x1408f530` | `0x233d4` | trigger; reader 25856-26022 |
| 0x20 | `EF_DOOR` | `0x5c83c` | `0x24858` | `0x374` | `0x1408f6c8` | `0x242f4` | door; reader 26652-26763 |
| other | — | `0x5c83c` | — | — | — | — | fall-through leaves `eax=0`, then writes type to `[eax+0x14]` (`0x1405c940`) → null-pointer write fault |

Name cross-reference: `docs/research/p4b2-render-order.md` (Client.dll type
dispatch, lines 60-97) independently identifies 2=`EF_PERSON`,
4=`EF_MAGIC`, 8=`EF_INVCONTAINER`, 0x10=`EF_TRIGGER`, 0x20=`EF_DOOR`.

### Vtable slots (32-bit PE, image base `0x14000000`)

Slots dumped from `Server.dll` with a temporary PE reader
(`%TEMP%\p2_15b1_vtables.py`, not committed); pointers are 4-byte VAs.
`+0` = save/writer (**inferred** from the call at 36103; class bodies
`0x1ec40`/`0x24aa8`/`0x2335c`/`0x24298` are absent from `functions.csv` and
`decompiled.c`), `+4` = read/load (called at 35678, 35853). Stub RVAs
`0x1977c` (byte `0xC3`, plain `ret`), `0x19780` (`C2 08 00`, `ret 8`),
`0x19784` (`C2 04 00`, `ret 4`) are three code stubs inside the single 1-byte
`functions.csv` entry `FUN_1401977c` (disassembly `0x14019770-0x14019788`).

| type | vtable | +0x00 save (inferred) | +0x04 read | other overrides |
|---:|---|---|---|---|
| 2 | `0x1408e4a8` | `0x1ec40` | `0x1e758` | `+0x8 0x17aa4`, `+0xc 0x18648`, `+0x10 0x18350`, `+0x14 0x18e3c`, `+0x18 0x1edc4`, `+0x1c 0x1d360`, `+0x20 0x1d400`, `+0x24 0x182c0`, `+0x28 0x19788`, `+0x2c 0x16e68` |
| 4 | `0x1408f3f0` | `0x19784` (ret 4) | `0x19780` (ret 8) | `+0x10 0x22848`, `+0x28 0x22e08`; all other slots are base stubs |
| 8 | `0x1408f778` | `0x24aa8` | `0x24928` | `+0x10 0x24960`, `+0x14 0x249f8`, `+0x28 0x24ad0` |
| 0x10 | `0x1408f530` | `0x2335c` | `0x233d4` | `+0x10 0x2382c`, `+0x14 0x23e44`, `+0x18 0x23aa0`, `+0x28 0x23f94` |
| 0x20 | `0x1408f6c8` | `0x24298` | `0x242f4` | `+0x10 0x24904`, `+0x18 0x24584`, `+0x28 0x24908` |

### Readers

- `EF_PERSON` type 2, `FUN_1401e758` RVA `0x1e758` size 1253, decompiled
  21905-22096 (the function is virtual, `callers=0`). Reads name string, `+0x40`,
  `+0x44`, coordinate pair via `FUN_140011ac`, `+0x5f8`, `+0x5fc`, `+0x654`,
  `+0x658[0x20]`, `+0x151c`, four strings (`+0x48`, `+0x1524`, `+0x267c`,
  `+0x2aac`), `+0x1520`, `+0x2668`, `+0x266c`, `+0x150c`, `+0x6cc`,
  `+0xff0[0x11c]`, `+0x14fc`, `+0x1500`, `+0x1504`, `+0x1630`, then
  `FUN_1401d8bc`; it does not call `FUN_140368d4` itself.
- `EF_TRIGGER` type 0x10, `FUN_140233d4` RVA `0x233d4` size 993, decompiled
  25856-26022. Sets `+4`, four strings at `+0x48`/`+0x174`/`+0x27c`/`+0x384`,
  `+0x2c`, `+0x14 |= value & 0x700`, `+0x490`, `FUN_1405ca18` → `+0x10`,
  `FUN_14023180`.
- `EF_DOOR` type 0x20, `FUN_140242f4` RVA `0x242f4` size 537, decompiled
  26652-26763. Sets `+4`, `+0x2c`, `+0x30`, three strings at
  `+0x48`/`+0x158`/`+0x260`, `+0x24`, `+0x28`, then `FUN_14023fb4`.
- `EF_INVCONTAINER` type 8, RVA `0x24928`, **absent from `functions.csv` and
  `decompiled.c`** (Ghidra did not create this function).
  Raw `llvm-objdump` `0x14024928-0x1402495d`: sets `+0x24 = 1`, `+4 = arg`,
  reads two `u32` through `FUN_140011ac` into `+0x18`/`+0x1c`, then
  `FUN_1405ca18` → `+0x10`, `ret 8`.
- `EF_MAGIC` type 4: `+4` is `0x19780` (`C2 08 00` = `ret 8`), a no-op; `+0`
  is `0x19784` (`ret 4`), also a no-op. Runtime creation is confirmed (36646).
  A type-4 record is never produced by `FUN_14032c2c` because the `BT_ENTS`
  filter at 36097 is `(entity+0x14 & 0x3a) != 0` and `4 & 0x3a == 0`, so type 4
  is skipped before any slot-`+0` dispatch (36103). The `+0`/`+4` stubs mean
  that even a manually forged type-4 record would not round-trip a body.

### `FUN_140368d4` install and `FUN_1401e424`

`FUN_140368d4` (RVA `0x368d4`, size 72, decompiled 38346-38369) appends an
entity pointer to the global `DAT_14088dc4` list at `+0x26e8` (current count
`+0x2768`, capacity `+0x276c`) with a linear duplicate check. It is called by
`FUN_1401e424` at 21871 (hero install), by the `BT_HERO ` attached loop at
35681, and from 17409/17992.

`FUN_1401e424` (RVA `0x1e424`, size 618, decompiled 21780-21874) is the direct
`BT_HERO ` body reader for the active hero (one type-2 person), not a virtual
`+4` method. It reads the person fields, resolves the prototype through
`DAT_140891bc+0x98` vtable `+0xc` (21859-21863), calls `FUN_14021658` /
`FUN_140222d0`, marks `+0x2bc0 = 1`, then installs the object with
`FUN_140368d4` (21870-21871). The `BT_HERO ` count-prefixed attached list that
follows creates each type-2 object with the factory and reads it through vtable
`+4` = `FUN_1401e758` (35677-35683).

### Unresolved types / checks

- Type 4 `EF_MAGIC` is factory-supported but has no `+0`/`+4` implementation.
  The `BT_ENTS` writer's `(entity+0x14 & 0x3a) != 0` filter (36097) excludes it
  before dispatch because `4 & 0x3a == 0`; a manually forged type-4 record
  would not consume a body either. **Resolved:** the Task 3 parse of all 2599
  sections found type 4 = 0; only a forged save could contain one, and its
  `ret 8` `+4` stub consumes a zero-length body (and the explicit type map
  rejects it).
- Type **1** is confirmed as the `BT_ENTS` terminator; any other value not in
  `{2,4,8,0x10,0x20}` hits the null-write fall-through at `0x1405c940` (the
  factory returns `eax = 0` for the unknown case and the unconditional
  `+0x14` store faults). The same fault occurs when the allocation fails.
- No other entity classes exist in this factory. Type values with flag bits
  (e.g. combined masks) are not handled by the dispatch, but whether the writer
  ever stores a combined value at `+0x14` is `unverified` (writer filter reads
  flags from the same dword).
- Per-type body layouts, terminator byte accounting and save-vs-runtime field
  differences are out of scope here (Tasks 3-4).
- Vtable slots other than `+0`/`+4` are listed by RVA only; their bodies and
  semantics are not decompiled in this task.

## Body layout

Each `BT_ENTS ` element is `u32 type` followed by a type-specific body; the
list ends with a bare `u32 1`. The loader reads a type, calls the object's
vtable `+4` reader (which continues on the same stream), then reads the next
type (`FUN_140325fc`, decompiled 35847-35914). All readers consume the save
stream only through the primitives below, so every body is self-delimiting.

### Stream primitives

| helper | RVA | decompiled / disasm | consumption |
|---|---|---|---|
| `FUN_14001324` raw read | `0x1324` | 257-281 | exactly `n` bytes, clamped at stream end |
| `FUN_1400129c` u32 | `0x129c` | 211-222 | 4 bytes (returns default on short read) |
| `FUN_14001114` string | `0x1114` | 83-98; disasm `0x14001114-0x1400114b` | `u32 n` then `min(n,0x104)` bytes |
| `FUN_14001214` string | `0x1214` | 156-161 | calls `FUN_14001114` with cap `0x104` |
| `FUN_140011ac` pair | `0x11ac` | 129-150 | `u32 x + u32 y` = 8 bytes |
| `FUN_1400126c` skip | `0x126c` | 186-191 | 4 bytes via stream vtable `+0` |
| `FUN_1405ca18` container | `0x5ca18` | 63030-63044 | `u32 flag`; if `flag != 0`, `FUN_1404c890` |
| `FUN_1404c890` container body | `0x4c890` | 52407-52490 | `u32 count`, then per item `raw(0xc0) + u32 m + raw(0x14*m)` |
| `FUN_1401d8bc` person tail | `0x1d8bc` | 21261-21295 | `raw(4*0x20) + u32 c + raw(0x14*c)` |

The string helpers read a `u32` declared length, then `min(declared, 0x104)`
bytes. Exact consumption in all saves shows the writer never emits a length
above the cap; the observed maximum declared string length is 63, so the clamp
is never active on the shipped data.

`FUN_1405ca18` is the shared "inventory/magic container" block. Its leading
`u32` is only zero-tested (`if (iVar1 != 0)` at 63037); it is not stored and its
value semantics are unverified (the name `flag` below is shorthand, not an id).
The item count is the first field read by `FUN_1404c890`. Observed: the leading
u32 is always 0 or 1, outer count ≤ 116, per-item sub-count `m` ≤ 11. Define

```text
M = 4                                  if flag == 0
M = 4 + 4 + sum_{i<count} (0xc0 + 4 + 0x14*m_i)   if flag != 0
```

with all reads little-endian. `M` is self-delimiting and needs no size table.
The `0xc0` item record and its `m`×`0x14` sub-array are structurally known from
`FUN_1404c890` but their **semantics are unverified**; exact next checks are the
consume-time object `FUN_1404ec2c` (call at 52444) and the allocation/lookup
pair `FUN_1404bc04`/`FUN_1404bf8c` (63038-63040).

### Type 2 `EF_PERSON` (`FUN_1401e758`, RVA `0x1e758`, decompiled 21905-22096)

| # | field (destination) | kind | bytes |
|---:|---|---|---:|
| 1 | name `+0x48` | string | `4 + n1` |
| 2 | `+0x40` | u32 | 4 |
| 3 | `+0x44` | u32 | 4 |
| 4 | coordinate pair (via `FUN_140011ac`) | 2×u32 | 8 |
| 5 | `+0x5f8` | u32 | 4 |
| 6 | `+0x5fc` | u32 | 4 |
| 7 | `+0x654` | u32 | 4 |
| 8 | `+0x658` | raw | `0x20` |
| 9 | temp (`iVar6`, later `FUN_140222d0`) | u32 | 4 |
| 10 | `+0x151c` | u32 | 4 |
| 11 | `+0x1524` | string | `4 + n2` |
| 12 | `+0x1520` | u32 | 4 |
| 13 | `+0x2668` | u32 | 4 |
| 14 | `+0x266c` | u32 | 4 |
| 15 | `+0x150c` | u32 | 4 |
| 16 | `+0x267c` | string | `4 + n3` |
| 17 | `+0x2aac` | string | `4 + n4` |
| 18 | `+0x6cc` | u32 | 4 |
| 19 | `+0xff0` | raw | `0x11c` |
| 20 | skipped (via `FUN_1400126c`) | raw | 4 |
| 21 | `+0x14fc` | u32 | 4 |
| 22 | `+0x1500` | u32 | 4 |
| 23 | `+0x1504` | u32 | 4 |
| 24 | `+0x1630` | u32 | 4 |
| 25 | magic container `+0xc` | `M` | variable |
| 26 | magic container `+0x10` | `M` | variable |
| 27 | magic container `+0x109c` | `M` | variable |
| 28 | `FUN_1401d8bc` tail: `raw(4*0x20) + u32 c + raw(0x14*c)` | mixed | `0x84 + 0x14*c` |

```text
size(2) = 540 + (n1+n2+n3+n4) + M1 + M2 + M3 + 0x14*c
```

(the 540 counts the four string length prefixes and all fixed raw/u32 fields but
not the three container flag words; the record adds 4 bytes for the type word).
Minimum body = `540 + 3*4 = 552` bytes, since each `M` is at least its 4-byte
zero flag.

The calls at 21962-21963 (`FUN_1401d99c` on `+0x4c`, then a vtable call) do not
touch the save stream: `FUN_1401d99c` (RVA `0x1d99c`) loads the person
script named by the already-read name, so they add 0 bytes to the body.

### Type 8 `EF_INVCONTAINER` (RVA `0x24928`, raw disasm only)

`0x14024928-0x1402495d`: `mov [edi+0x24],1`; `mov [edi+4],eax`;
`call 0x140011ac` (8 bytes) and store the pair at `+0x18`/`+0x1c`;
`call 0x1405ca18` and store at `+0x10`; `ret 8`.

```text
size(8) = 8 + M
```

No name/string field. Minimum body = 12 bytes. Ghidra has no function entry for
this RVA (`functions.csv`/`decompiled.c` gap); the disassembly above is the
evidence.

### Type 0x10 `EF_TRIGGER` (`FUN_140233d4`, RVA `0x233d4`, decompiled 25856-26022)

| # | field (destination) | kind | bytes |
|---:|---|---|---:|
| 1 | name `+0x48` | string | `4 + n1` |
| 2 | `+0x2c` | u32 | 4 |
| 3 | discarded (25915) | u32 | 4 |
| 4 | `+0x174` | string | `4 + n2` |
| 5 | `+0x27c` | string | `4 + n3` |
| 6 | `+0x14 |= value & 0x700` | u32 | 4 |
| 7 | `+0x384` (`+900`) | string | `4 + n4` |
| 8 | `+0x490` | u32 | 4 |
| 9 | magic container `+0x10` | `M` | variable |

```text
size(0x10) = 32 + (n1+n2+n3+n4) + M
```

The 32 is four 4-byte string length prefixes plus four 4-byte u32 fields; the
container's leading flag word is inside `M`. Minimum body = 36 bytes (32 fixed
plus the 4-byte zero flag in `M`); the record adds the 4-byte type word.
`FUN_14023180(this)` (26019) does not read the stream.

### Type 0x20 `EF_DOOR` (`FUN_140242f4`, RVA `0x242f4`, decompiled 26652-26763)

| # | field (destination) | kind | bytes |
|---:|---|---|---:|
| 1 | name `+0x48` | string | `4 + n1` |
| 2 | `+0x2c` | u32 | 4 |
| 3 | `+0x30` | u32 | 4 |
| 4 | `+0x158` | string | `4 + n2` |
| 5 | `+0x260` | string | `4 + n3` |
| 6 | `+0x24` | u32 | 4 |
| 7 | `+0x28` | u32 | 4 |

```text
size(0x20) = 28 + (n1+n2+n3)
```

Minimum body = 28 bytes. `FUN_14023fb4(this)` (26760) does not read the stream.

### Type 1 terminator

A single `u32 1` (4 bytes), no body. It is read before the factory call
(35848-35849, 35913-35914), so it never reaches a reader.

### Type 4 `EF_MAGIC`

Never serialized: the writer filter `(entity+0x14 & 0x3a) != 0` (36097) is false
for type 4 (`4 & 0x3a == 0`). The scan found no type-4 records in any section.
If a type-4 record were forged, its `+4` reader is `ret 8` (`0x19780`) and
consumes no body, so the loader would accept a zero-body record and continue at
the next u32; only the scan's explicit type map rejects type 4 as unmapped.

## Parse algorithm

The scan (`%TEMP%\p2_15b1_scan.py`, not committed) was extended with the
per-type readers above and parses every `BT_ENTS ` body as follows:

1. If `cursor + 4 > len(body)`: fail (truncated).
2. Read `u32 type`; advance 4.
3. If `type == 1`: stop; success requires `cursor == len(body)`.
4. Otherwise apply the type's formula/size (advance the cursor per §Body layout);
   an unmapped type fails with the type value and body offset.
5. Repeat.

Run command:

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
py -3 "$env:TEMP\p2_15b1_scan.py"
```

### Result (all 28 saves, all sections)

```text
files 28 errors 0
manifest_sha256 F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B
total_sections 2599
ents_sections 2599 ents_decl_ok 2599
ents_term_ok 2599 ents_consume_ok 2599 ents_parse_fail 0
ents_type_hist {2: 14507, 8: 354, 16: 12090, 32: 2591}
pname_eq_person 2599 pname_ne_person 0
```

- Terminator `type == 1` is present and last in **2599/2599** sections.
- Consumed bytes equal the declared `BT_ENTS ` body length in **2599/2599**
  sections (0 mismatches), so the parse reaches the terminator exactly.
- Types observed: 2 (14,507), 8 (354), 0x10 (12,090), 0x20 (2,591); 29,542
  entity records plus 2,599 terminators. Type 4 and any unmapped type: 0.
- Every selected-level section (28/28) also consumes exactly; e.g. slot0/L93_4
  `ents=8381` parses `{2: 3, 8: 1}` with 8381/8381 bytes, slot2/L6_3
  `ents=3034` parses `{16: 11}` with 3034/3034 bytes.
- `BT_PNAME` entry count equals the number of type-2 records in the same
  section in **2599/2599** sections (0 mismatches). This corroborates that
  `BT_PNAME` names belong to person entities; the exact lookup is left to
  Task 4.
- `BT_ENTS ` body length range 67..138,768 bytes (570 distinct), consistent with
  at least one entity per section: minimum record sizes (type word + minimum
  body) are type 8 = 16, `0x20` = 32, `0x10` = 40, type 2 = 556.

No BLOCKED condition occurred: every type encountered is sized by an explicit
formula and all 2599 bodies consume exactly.

## RGINF

`BT_RGINF` body is 36 bytes: a `u32` latch plus a 0x20-byte game timestamp.

| offset | size | kind | field | destination (runtime) |
|---:|---:|---|---|---|
| `0x00` | 4 | u32 | regeneration latch (persisted verbatim) | `DAT_14088dc4+0x26c0` |
| `0x04` | `0x20` | 8 x u32 | last-regeneration datetime: year, month, day, weekday, hour, minute, second, millisecond | `DAT_14088dc4+0x26a0..0x26bf` |

Writer `FUN_14032c2c` (RVA `0x32c2c`, size 1511, caller `FUN_1403210c`):
decompiled 36040-36042 writes the tag, a dword, then raw `0x20` from
`+0x26a0`. The decompiler drops the dword argument; raw disassembly
`0x14032e7c-0x14032e8b` is `mov -0x14(%ebp),%eax; pushl 0x26c0(%eax);
call FUN_1400126c`, so the dword is `+0x26c0`, and `0x14032e90-0x14032ea2`
then pushes `$0x20`, `leal 0x26a0(%eax),%edx`, and calls `FUN_1400127c`.

Field-order evidence: the datetime comparator `FUN_1405b490` (decompiled
62070-62114) compares the eight dwords in turn but skips weekday index 3 (weekday
is derived, not compared), which fixes the year/month/day/weekday/hour/minute/
second/millisecond order used above.

Reader `FUN_140325fc` (RVA `0x325fc`, decompiled 35812-35846):

1. 35813 `FUN_1400129c` reads the u32 into `+0x26c0`; 35815 `FUN_14001324`
   reads `0x20` bytes into `+0x26a0`.
2. 35817: if the saved u32 is `0`, run the due-check below.
3. 35844-35845: if the saved u32 is nonzero, `+0x26c0 = 0`.

Due-check when the saved u32 is `0` (35817-35842):

- 35819 `(*(*DAT_14088e14+0x18))(local_a0)` copies the current game datetime
  (0x20 bytes) to `local_a0`.
- 35820-35827 copy the 8 saved dwords to `local_80..local_64`.
- 35828-35835 zero `local_60..local_44`, then set
  `local_5c = gv_items_regenerate_interval / 0x1e` and
  `local_58 = gv_items_regenerate_interval % 0x1e`; this is a duration of
  `gv_items_regenerate_interval` days in the same datetime layout (index 1 =
  month, index 2 = day).
- 35836 `(*(*DAT_14088e14))(&local_80,&local_60)` adds the duration to the
  saved datetime.
- 35837 `(*(*DAT_14088e14+8))(&local_80,local_a0)` compares; `FUN_1405b490`
  returns 1 when `saved + interval <= now`.
- 35839-35842 on nonzero: `+0x26c0 = 1` and
  `(*(*DAT_14088e14+0x18))(+0x26a0)` stores `now` as the last-regeneration
  time.

`DAT_14088e14` is the game calendar/clock: `FUN_1405bfdc` allocates `0x48`
bytes and installs vtable `0x14092b98`; slot `+0` = `0x5b38c` (add), `+8` =
`0x5b490` (compare), `+0x18` = `0x5bb84` (read now), `+0x1c` = `0x5bbc4`
(set). `gv_items_regenerate_interval` is `DAT_1407fbcc+0x8c`, loaded from
config in init (decompiled 2765-2774; `7` single-player, `5` multiplayer).

Consumers of the runtime latch `+0x26c0` (all inventory-container
regeneration/resolution paths):

- Person reader `FUN_1401e758` 22083-22087: when `+0x26c0 != 0`,
  `+0xc != -1` and `+0x10 != -1`, it calls
  `FUN_1404b1c0(DAT_14088df8, +0xc, +0x10, this+0x2ab0, 0)`.
- Trigger reader 25998-26016: when `+0x26c0 != 0`, it resolves `+0x10` from
  `scripts/inventory/<...>.inv` through `FUN_1404bb9c` / `FUN_140489ec` /
  `FUN_1404805c`.
- `FUN_1404b1c0` (RVA `0x4b1c0`) regenerates loot at
  `DAT_14088dc4+0x26c4 + 1` (party level + 1; see
  `p3b2b2-npc-loot-server.md` §10.1 and §15.1).

Corpus check (new scan `%TEMP%\p2_15b1_rginf_scan.py`, same framing as
§Tail inventory): all 2599 sections parse with a 36-byte body. The first u32
is `0` in 1920 sections and `1` in 679. The 0x20 bytes are never all-zero and
decode as 691 distinct timestamps with year 1124-1126, month 1-12, day 1-31,
weekday 0-6, hour 0-23, minute/second 0-59, millisecond 0-999. The block is a
real per-section snapshot time, not opaque padding.

Restore relevance: `BT_RGINF` carries no entity field. It only decides whether
NPC/trigger inventories are regenerated after load. Entity position/name/type
restoration does not depend on it; NPC inventory parity does.

Unverified: the gameplay producer of a persisted `1`. The latch is written only
in init (`34138`) and in the load path (`35814`, `35818`, `35840`, `35845`), so
a saved `1` is the persisted due-check result of a previous load and is
suppressed on the next load. Exact next check: confirm there is no other
`DAT_14088dc4+0x26c0` writer in `Server.dll` outside `FUN_1403210c` /
`FUN_140325fc` (none in `decompiled.c`), then observe one save/load cycle in a
read-only session.

## PNAME

Writer `FUN_14032c2c` (35980-36039):

- 35980 writes tag `BT_PNAME`; the body has no leading count.
- The loop walks the occupied level entity slots in slot order (`+0x2564`
  array, `+0x256c` bound, `+0x2568` bitmap). For the entity in the current
  slot:
  - 36006 requires `(entity+0x14 & 0x3e) == 2` (type-2 `EF_PERSON`) and
    `entity+0x150 == 0` (alive; `+0x150` is set to 1 with a timestamp at
    `+0x154` on removal/death, e.g. `FUN_14016e68` 17345-17346).
  - 36007 `FUN_140253fc(level, entity)` returns 1 when the entity is in the
    attached list (`level+0x26e8`, count `+0x2768`). Only when it returns 0
    (standalone) does 36009 `FUN_1400116c(local_88, entity+0x4c)` append the
    name as `u32 len + cp1251 bytes`.
- 36039 `FUN_1400116c(local_88, &DAT_1408c854)` appends the zero-length
  string; `DAT_1408c854` is four zero bytes, so the body ends with `u32 0`.

The person name is `{size_t len at +0x48; char text[260] at +0x4c}` (person
reader 21954-21960 copies `0x42` size_t units = `0x108` bytes); the writer
emits `+0x4c`. `FUN_1401d99c(+0x4c)` (21962, 18084) loads
`scripts/persons/<name>.scr` from that string, so `+0x4c` is the full person
name including its level prefix.

Order/count relation to `BT_ENTS`: the entity writer at 36096-36105 uses the
same slot order and the same filters (`type & 0x3a != 0`, `+0x150 == 0`, and
skip attached type-2 via `FUN_140253fc`). Therefore the k-th `BT_PNAME` name is
the name of the k-th type-2 record in `BT_ENTS`, and the counts are equal
(2599/2599 in §Parse algorithm). `BT_PNAME` has entries only for type-2
persons; triggers/doors/containers are absent.

Lookup at decompiled line 38563 is `FUN_14036b58` (RVA `0x36b58`, size 834):

- 38553/38537 open the tail section keyed by `param_1` from the `+0x2578`
  dictionary.
- 38563 `FUN_14001084(local_250,"BT_PNAME")` reads the tag.
- 38571-38587 loop `FUN_14001214` string reads (cap `0x104`) until the empty
  terminator, copying each name into `local_128`/`local_230`.
- 38597-38623 compare each name to `param_2`; return 1 on an exact match and
  0 after the terminator.
- Fallback when the section is absent: `FUN_1402914c(this,param_2)` (person
  lookup) and `+0x14fc > 0` (alive) at 38539-38543, i.e. "is this person
  present in the level".

So the lookup is a flat name-presence query, not an entity-body parser.

Origin-level prefix: observed names are `L1_1.P387_Poison_Plotnik`,
`L0.P678_citizen4`, `L93_2.P292_Chernobogs_Guardian2`, etc. The prefix is a
level id but not always the current section's level (slot0's `L1_1` snapshot
contains `L0.*` and `L1.*`; the `L93_4` section contains `L93_2.*`). The lookup
does exact string comparison; the PNAME-to-ENTS mapping is by order, not by
prefix parsing. Prefix meaning is `unverified` (hypothesis: level where the
prototype was originally spawned / the NPC's home level). Exact next check:
trace where `+0x4c` is assigned for dynamically spawned persons (writes around
`FUN_1401d99c` / the person constructor) and compare the prefix of a person
that moved between two saves.

Restore relevance: PNAME is not needed to rebuild entity bodies; it is needed
only for the presence lookup and for ordering validation. The port already
exposes `payload.tail[].name` but does not parse `BT_PNAME` or entity records.

## Restore flow

The original load is a two-function sequence.

`FUN_1403210c` (RVA `0x3210c`, .sav loader, decompiled 35523-35695; comment
header 35521):

1. Read `0x68` header bytes with `FUN_14001324` (35602), then the two `u32`
   inflate lengths at `0x68`/`0x6c` through `FUN_14001668` (35603); the
   compressed payload starts at `0x70`, the tail dictionary starts at
   `0x70 + compressed_len`, and its `u32 count` plus name/section entries are
   loaded into the dictionary at `+0x26d8` (35600-35627).
2. `BT_INFO ` (35628-35639): session string, `+0x218`, the 0x20-byte game
   datetime (applied with `DAT_14088e14+0x1c`, so it becomes the current
   clock), description string, level-id string, `+0x26c4` (party level), and
   8 bytes into `+0x26c8`.
3. `FUN_140325fc(this, level_id, 0)` loads the selected tail section (35640).
4. `BT_VARS` -> `DAT_1407f9a0`; `BT_TRIBE` -> `DAT_14089fc0`; `BT_DIARY` ->
   `DAT_14088e04` (35664-35669).
5. `BT_HERO` (35670-35684): create one type-2 object with
   `FUN_1405c83c(2)` and read it with `FUN_1401e424` (a direct reader, not
   vtable `+4`); then a count-prefixed attached list: create type-2, read via
   vtable `+4` = `FUN_1401e758`, set `+0x2bc0=1` and `+0xf28=0`, and install
   with `FUN_140368d4` into the attached list `+0x26e8`.
6. `BT_HSTRY` (35685-35686).

`FUN_140325fc` (RVA `0x325fc`, one level snapshot, decompiled 35704-35931):

1. Open the section stream from the `+0x26d8` dictionary (35770-35773).
2. Open the SEF `levels/single/<level>/<level>.sef` (or multiplayer)
   (35779-35785); parse `points_entrance` with `FUN_14031844` into the
   entrance table (`+0x1a74`, records `+0x1a84` = x, y, direction) and
   `cell_groups` with `FUN_140319a0` (35803-35810). SEF supplies static
   entrances and cell groups; it does not create `BT_ENTS` entities.
3. `BT_RGINF` (35812-35846, see §RGINF).
4. `BT_ENTS` (35847-35915): loop `u32 type`; `type == 1` ends the list;
   otherwise:
   - `FUN_1405c83c(type)` creates the object (class size/vtable from
     §Type map; the factory stores `type` at `+0x14`).
   - `uVar4 = FUN_14003718(this+0x2150)` allocates a free slot in the
     0..0xff slot table.
   - `(*(*obj+4))(stream, uVar4)` runs the type's virtual reader (type 2 =
     `FUN_1401e758`, type 0x10 = `FUN_140233d4`, type 0x20 = `FUN_140242f4`,
     type 8 = `0x24928`).
   - Unless the multiplayer type-8 branch disposes it, store the object at
     `entities[uVar4]` (`+0x2564`) and set its bit in the occupancy bitmap
     `+0x2568` (35854-35905).

Person field provenance:

- From the `BT_ENTS` type-2 body: name, tile x/y, direction, the three inline
  inventory containers, and all fixed/raw fields in §Body layout.
- From SEF / person script / SDB prototype: static appearance, animation and
  sprite (`FUN_1401d99c` at 21962 loads `scripts/persons/<name>.scr`; the
  prototype is resolved through `DAT_140891bc+0x98`), route/tribe/dialog/
  inventory template names, entrances and cell groups.
- From loot regeneration: after install, a set `+0x26c0` makes `FUN_1404b1c0`
  or the trigger path regenerate the saved containers from
  `scripts/inventory/<name>.inv` and the party-level loot table, so serialized
  item records can be replaced.
- The hero is never in `BT_ENTS`; it is `BT_HERO` (step 5 above).

Order: header/dictionary -> `BT_INFO`/clock -> level snapshot (SEF ->
`BT_RGINF` -> `BT_ENTS`) -> `BT_VARS` -> `BT_TRIBE` -> `BT_DIARY` ->
`BT_HERO` (+ attached list) -> `BT_HSTRY`.

Correction to the task wording: `FUN_140368d4` installs `BT_HERO`'s attached
entities into the attached list `+0x26e8`; `BT_ENTS` entities are installed
into the level slot array `+0x2564` through the `FUN_14003718` slot allocator,
not through `FUN_140368d4`.

## Port mapping

Status: **available** = exact field/decoder exists and is wired; **partial** =
storage/decoder exists but the save input is not wired; **missing** = no port
field; **unverified** = save semantics not resolved yet.

| save field | port struct/field | status |
|---|---|---|
| `BT_INFO.time[0x20]` | `assets::SaveInfo.time` (`save_payload.hpp:20`) | available (parsed) |
| `BT_INFO.+0x26c4` | `SaveInfo.party_level` (`save_payload.cpp:106`) | partial: parsed but unused; glgame loot level is `hero_level_from_experience(EXP)+1` (`tools/glgame/main.cpp:1009-1012`) and omits the original Scout/Identify bonuses (`p3b2b2` §15.1) |
| `BT_INFO.+0x26c8[8]` | `SaveInfo.field_26c8` (`:23`) | available (opaque) |
| `BT_PNAME` names | `PersonInstance.name`, `PersonState.name` (`SaveTailEntry.name` is the tail/level key, not a person) | missing (no PNAME parser) |
| `BT_RGINF` u32 latch | none; `SavePayload.rginf` raw 36 bytes (`save_payload.hpp:47`) | partial (raw only) |
| `BT_RGINF` datetime | none; `LevelSimulation.time_ms_` is a monotonic sim clock, not a calendar | missing |
| `BT_ENTS` type word | `Level.persons` / `LevelSimulation.persons`, built from SEF only | missing |
| person name (body #1) | `PersonInstance.name`, `PersonState.name`, `LevelSimulation::State.name`, `InventoryRuntime.persons_` key | available |
| tile x/y (body #4 pair) | `PersonInstance.tile_x/tile_y`; `State.spawn_tile_x/y`, `PersonMovement.x/y` | available (`FUN_140222d0` uses `x*12+6`, `y*9+4` = `level_simulation.cpp:64-66`) |
| direction (body #9) | `PersonInstance.direction`; `PersonMovement.direction8` | available; 0..7 vs 8-dir encoding unverified |
| route type/route/cells | `PersonInstance.route_type`/`route`/`route_cells`; `LevelSimulation::State.route`/`route_type` | unverified: no matching body field; port values come from SEF (`person.cpp:203-219`) |
| `+0x40`/`+0x44` | none (SDB prototype kind/id; inferred from 18047-18054) | missing |
| `+0x5f8`/`+0x5fc` | none | unverified/missing |
| `+0x654`/`+0x658[0x20]` | none | unverified/missing |
| `+0x151c`/`+0x1520`/`+0x1524` | none | unverified (maybe dialog/proto) |
| `+0x2668`/`+0x266c` | none | unverified |
| `+0x150c` | `PersonInstance.tribe` is a name string; saved value is a numeric tribe id | partial (numeric id not modelled) |
| `+0x267c`/`+0x2aac` | `PersonInstance.dialog`/`inventory` (names) | unverified (order/strings) |
| `+0x6cc` | none | unverified/missing |
| `+0xff0[0x11c]` | none | unverified (appearance/wear/mask?) |
| skipped `u32` (body #20) | none; must be consumed | n/a |
| `+0x14fc`, `+0x1500`, `+0x1504` | `PersonState.values[life/energy/...]` via `PersonParameter` | inferred life/energy; save restore missing |
| `+0x1630` | none | unverified/missing |
| containers `+0xc`/`+0x10`/`+0x109c` (record `0xC0 + 4 + 0x14*sub`; id `+0xA8`, count `+0xB0`, durability `+0xB4`, type `+0x08`, style `+0x20`, flags `+0x0C`) | `InventoryRuntime.persons_` items / `InventoryItem`; `hero_equipment.cpp` decoders | partial (decoder exists, entity wiring missing) |
| `FUN_1401d8bc` tail (4x0x20 + u32 c + 0x14*c) | none | unverified/missing |
| `BT_ENTS` slot install | `LevelSimulation.persons_` from `Level.persons` only | missing (no restore entry point) |
| `BT_HERO` + attached list | `HeroStats`, `HeroInventory`, `HeroEquipment` (`parse_hero_stats`, `parse_hero_inventory`, `parse_hero_equipment`) | partial (stats/equipment available; attached list missing) |

Next check for the `unverified` group (one pass): map each unverified person
field against the SDB prototype copy at `decompiled.c:18020-18105` and the
consumers named in §Restore flow, then confirm the direction 0..7 vs 8-dir
encoding with a read-only live capture.

Port loaders relevant to a future restore: `load_level`/`load_persons`
(SEF+script+PAD) produce `Level.persons`; `LevelSimulation::create` copies that
to movement states; `generate_npc_inventories` builds NPC items;
`parse_hero_inventory` already decodes the same `0xC0+4+0x14*m` record that
entity containers use.

### Oracle plan

Static (no live game, run by the agent):

1. Reuse the `%TEMP%` scanner with a canonical entity tuple
   `{type, name, tile_x, tile_y, direction, container item id/count/durability
   tuples}`. Require exact `BT_ENTS` consumption on 2599/2599 sections (already
   achieved) and PNAME count == type-2 count (2599/2599).
2. Pin four sections: slot0/`L93_4` (3 persons + 1 type-8 container, ents
   8381), slot2/`L6_3` (0 persons, 11 triggers, ents 3034), slot21/`L1_1`
   (16 persons, ents 31062), and slot18/`L18_1` (0 PNAME names). Record the
   tuple list and the RGINF bytes as the reference.
3. For slot21/`L1_1`, diff the 16 saved person names and tile/direction
   tuples against `load_persons(vfs,"L1_1")` (SEF). Any difference is either a
   runtime move (expected) or an unmapped field; report `0/N` for names and
   list position deltas.

Read-only live diff (requires the user to launch the original; same protocol as
`p2-15a-save-payload.md` §Runtime read-only diff):

4. User loads slot0, then slot2, then slot21/`L1_1`. A `%TEMP%` `ctypes` script
   with `OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION)` +
   `ReadProcessMemory` (no writes) reads the `Server.dll` base plus `0x88dc4`,
   then `+0x256c` (entity bound), `+0x2564` (slot array), `+0x2568` (bitmap),
   `+0x26c0`, and `+0x26a0[8]`.
5. For each type-2 slot, read `+0x14` (type), `+0x4c` (name), `+0x604`/`+0x608`
   (tile x/y), `+0x28` (direction), and `+0xc`/`+0x10`/`+0x109c` (container
   ids). Compare to the static tuples and report differing entities as `0/N`.
6. Inventory counts (deferred, `unverified`): this check is not executable
   read-only as written. A `ReadProcessMemory` script cannot call
    `FUN_1404bf8c(id)`; it must read the container manager object at
    `DAT_14088df8` directly. Extra reverse needed before this can run:
    (a) decode the map at `DAT_14088df8+0xc` (built by `FUN_14004f64`, probed by
    `FUN_14004b40`/`FUN_14004f14`, see `FUN_1404bb9c` 51711-51719) to turn a
    container id into a container pointer; (b) re-prove the `0xC0` item-record
    offsets for entity containers - `p5-save-equipment.md` §5 proves them for
    `BT_HERO` only. Until both are done, item-count parity stays `unverified`;
    the static byte check in steps 1-2 is the only proven byte-level oracle
    (inventory item-field labels pending).
7. RGINF: compare the runtime `+0x26c0`/`+0x26a0` after load with the static
   parse and the computed due result. Select offline a section whose saved
   latch is `0` and whose `last + gv_items_regenerate_interval <= BT_INFO.time`
   (the static scan can list them) to exercise the `+0x26c0 = 1` branch; no
   writes are required.

If the user cannot run the original, steps 1-3 are the static oracle; steps 4-5
and 7 remain `unverified` (runtime consumer behavior is from decompilation
only), and step 6 is deferred by the two reverse-engineering items above.

## Verdict / decomposition

**Verdict: `scoped`.** The container format is implementable now; full person
restore is not fully implementable because several person-body field semantics
and the entity-level item record remain `unverified`. No `blocked` condition:
the `BT_ENTS` list is exactly parseable on the whole corpus (2599/2599 sections,
0 mismatches, terminator `type == 1` always last), and `BT_PNAME` count equals
the type-2 count in 2599/2599 sections.

Evidence for `scoped` rather than `implementable`:

- Parsing needs no live oracle: four entity types (`2`, `8`, `16`, `32`) are
  sized by explicit reader formulas; type `4` is never serialized; type `1` is
  the terminator. All 2599 sections consume exactly to the terminator.
- Restore semantics do not share that coverage: 15+ person fields are
  `unverified`/`missing` (§Port mapping), the `0xC0 + 4 + 0x14*m` item record is
  byte-proven only for `BT_HERO` (`p5-save-equipment.md` §5), and the gameplay
  producer of a persisted `BT_RGINF` latch `1` is open (§RGINF).
- Runtime container consumption, trigger `.inv` resolution and NPC loot
  regeneration are known from decompilation + static analysis but have no live
  evidence; the read-only live diff (§Oracle plan steps 4-5,7) is optional and
  not required to unblock the parser.

### P2-15b1a — entity parser + type map

Goal: decode `BT_ENTS`/`BT_RGINF`/`BT_PNAME` into typed records without
changing restore behavior. Acceptance:

1. Decoder consumes a full `BT_ENTS` body exactly: loop `u32 type`; `type == 1`
   ends and requires `cursor == len(body)`; `{2, 8, 16, 32}` advance by the
   §Body layout formulas; any other type (including `4`) is a decode error with
   type value and offset.
2. `BT_ENTS` decodes every one of the 2599 tail sections (and all 28
   selected-level sections) to the terminator with `0` consumed-byte
   mismatches; type histogram matches `{2: 14507, 8: 354, 16: 12090, 32: 2591}`.
3. `BT_PNAME` decodes as a zero-terminated sequence of `u32 len + cp1251`
   names (0-entry body = 4-byte zero); decoded name count equals the section's
   type-2 count in 2599/2599 sections.
4. `BT_RGINF` decodes as a fixed 36-byte body: `u32` latch + eight `u32`
   datetime fields (year, month, day, weekday, hour, minute, second,
   millisecond).
5. Real-data tests over the 28-save corpus; the spec's §6 acceptance is met.
6. Out of scope: mapping parsed fields onto simulation state, item-field
   semantics, magic type 4.

`BT_RGINF` and `BT_PNAME` are part of `P2-15b1a`; there is no separate
`P2-15b1c` (the earlier plan's optional split is folded in, so the decomposition
is self-contained).

### P2-15b1b — person restore

Goal: rebuild persons/entities from the parsed records and wire them into
`LevelSimulation` in original order. Acceptance:

1. Restore ordering matches `FUN_1403210c`/`FUN_140325fc`: tail dictionary ->
   `BT_INFO`/clock -> selected section (SEF entrances/cell groups -> `BT_RGINF`
   -> `BT_ENTS`) -> `BT_VARS` -> `BT_TRIBE` -> `BT_DIARY` -> `BT_HERO`
   (attached list) -> `BT_HSTRY`. `BT_ENTS` objects install into the level slot
   array `+0x2564` via the `FUN_14003718` slot allocator, **not** through
   `FUN_140368d4`.
2. At minimum restore the proven person fields: name (`#1`), tile x/y (`#4`,
   using `x*12+6`, `y*9+4`), direction (`#9`), and the three inline containers
   byte-level. Static appearance/animation/sprite/route/tribe/dialog/inventory
   template names continue to come from SEF/person script/prototype, as in the
   original.
3. Honour `BT_RGINF` `+0x26c0`: when set, NPC/trigger inventories are
   regenerated from `scripts/inventory/*.inv` and the party-level loot table
   (`FUN_1404b1c0`), so serialized items may be replaced; entity
   position/name/type restore does not depend on it.
4. Fields with `unverified`/`missing` semantics (§Port mapping) are not claimed
   as restored; they are either left at prototype/SEF defaults or gated behind
   an explicit flag, each listed in the implementation spec.
5. Inventory item parity is **not** claimed until the oracle is unblocked:
   prove the `0xC0` item-record offsets for entity containers (currently
   `BT_HERO`-only) and decode the `DAT_14088df8+0xc` container map for a
   read-only live diff (oracle step 6). Static byte-level oracle (steps 1-2) is
   the proven gate until then.
6. Out of scope: hero (owned by `BT_HERO`/P2-15b), `BT_DIARY`/`BT_HSTRY`/
   `BT_VARS` values/`BT_TRIBE`, save write (P2-15c), magic type 4.

### Explicitly out of scope (all of P2-15b1)

- `BT_DIARY`, `BT_HSTRY`, `BT_VARS` values, `BT_TRIBE`, `BT_SCRN`.
- Save write (P2-15c).
- Type 4 `EF_MAGIC` restore: never serialized (`4 & 0x3a == 0` filter) and its
  `+0`/`+4` slots are no-op stubs, so no body round-trips.
- Live gameplay producer of a persisted `BT_RGINF` latch `1`.
- Item-field semantic labels for entity containers beyond the byte-level layout
  proven for `BT_HERO`.
