# P2-15a — Save payload research

Status: complete (2026-11-12). Container 28/28, reader/writer mapping,
`BT_SCRN ` preview classification, block domains and Open Questions are
recorded; verdict and P2-15b/P2-15c decomposition in
§Verdict / decomposition.
Phase: P2-15a (research-only; follow-up to P2c header-only save slots).
Scope: `docs/research/` only; no production code changes.

## Provenance

| Artifact | SHA-256 |
|---|---|
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| `Saves/slot0.sav` | `8AC545FE9715EF11C68B1CCA8617BEB03003ECA360F07F71C26E3FC5E5C73442` |
| 28 saves (slot0..slot27, sorted hashes) | manifest SHA-256 `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |

Source: `I:\Games\Златогорье-2\Saves` (28 files `slot0.sav` … `slot27.sav`).
Tool: Python 3 (`zlib`, `struct`), independent container scan (script kept in
`%TEMP%`, not committed). Hashes above re-checked with `Get-FileHash -Algorithm SHA256`.

## §Container

### Framing

- The first 112 bytes of each `.sav` are the header documented in P2c and are
  not part of the payload scan. The payload is the zlib stream starting at
  **`raw[112:]`**: `zlib.decompress(raw[112:])` succeeds for all 28 files.
- The decompressed payload is a flat sequence of blocks. Each block is:
  - **8-byte tag** — ASCII, NUL-free, right-padded with `0x20` (space):
    `BT_SCRN `, `BT_INFO `, `BT_VARS `, `BT_TRIBE`, `BT_DIARY`, `BT_HSTRY`,
    `BT_HERO `.
  - **`u32` little-endian size** — number of body bytes that follow.
  - **body** — exactly `size` bytes.
- Therefore a block occupies `12 + size` bytes, and the next tag starts at
  `offset + 12 + size`. There is no padding, alignment or terminator between
  blocks.
- The scan advances by `12 + size` while `i + 12 <= len(body)`; it reports
  `UNALIGNED` if a tag does not start with `BT_`. None of the 28 files reported
  `UNALIGNED`.

### 28/28 verification

Real scan output (2026-11-12): `files 28`; every save has exactly **7 blocks**
in the same order (`orders = 1` distinct sequence); every scan ends with
`end True` (final `BT_HERO ` end equals the decompressed length). No padding is
present: block N+1 offset equals block N offset + 12 + size for all blocks in
all files.

### Block catalogue (all 28 saves)

| # | tag (8 bytes) | offset rule | slot0 size | size across 28 saves (min…max, distinct) | domain hypothesis |
|---|---|---|---|---|---|
| 1 | `BT_SCRN ` | fixed `0x0` | 750008 | 750008…750008 (1, fixed) | save preview screenshot (client-only) |
| 2 | `BT_INFO ` | after `BT_SCRN ` | 100 | 77…100 (13) | level/party info (`+0x26c4` loot level, 1b2b2b) |
| 3 | `BT_VARS ` | after `BT_INFO ` | 31035 | 27611…31144 (16) | script globals/variables |
| 4 | `BT_TRIBE` | after `BT_VARS ` | 58884 | 58884…58884 (1, fixed) | tribe ids/names + N×N relation byte matrix |
| 5 | `BT_DIARY` | after `BT_TRIBE` | 32960 | 188…33664 (24) | journal/quest diary |
| 6 | `BT_HSTRY` | after `BT_DIARY` | 1592 | 384…1950 (27) | history/event log |
| 7 | `BT_HERO ` | after `BT_HSTRY` | 30388 | 2404…35012 (22) | hero + 3 inventories (P5a) |

Fixed-size blocks: `BT_SCRN ` (always 750008) and `BT_TRIBE` (always 58884).
All other blocks vary between saves, so offsets of blocks 2–7 are per-save.
The final `BT_HERO ` ends exactly at the decompressed length in all 28 files.

### slot0 exact layout

`slot0.sav`: raw 642409 bytes, decompressed 905051 bytes, 7 blocks, contiguous
end == decompressed length.

| # | tag | offset | size | end |
|---|---|---|---|---|
| 1 | `BT_SCRN ` | `0x0` | 750008 | `0xB71C4` |
| 2 | `BT_INFO ` | `0xB71C4` | 100 | `0xB7234` |
| 3 | `BT_VARS ` | `0xB7234` | 31035 | `0xBEB7B` |
| 4 | `BT_TRIBE` | `0xBEB7B` | 58884 | `0xCD18B` |
| 5 | `BT_DIARY` | `0xCD18B` | 32960 | `0xD5257` |
| 6 | `BT_HSTRY` | `0xD5257` | 1592 | `0xD589B` |
| 7 | `BT_HERO ` | `0xD589B` | 30388 | `0xDCF5B` |

`0xDCF5B` = 905051 = decompressed length of `slot0.sav`.

### Commands

```powershell
py -3 "$env:TEMP\p2_15a_scan.py"
```

Scan script (kept in `%TEMP%`, not committed):

```python
import glob, os, struct, zlib
root = r"I:\Games\Златогорье-2\Saves"
files = sorted(glob.glob(os.path.join(root, "slot*.sav")),
               key=lambda p: int(os.path.basename(p)[4:-4]))
print("files", len(files))
for path in files:
    raw = open(path, "rb").read()
    body = zlib.decompress(raw[112:])
    blocks = []
    i = 0
    while i + 12 <= len(body):
        if body[i:i+3] == b"BT_":
            tag = body[i:i+8].decode("ascii", "replace")
            size = struct.unpack_from("<I", body, i + 8)[0]
            blocks.append((tag, i, size))
            i += 12 + size
        else:
            print("UNALIGNED", os.path.basename(path), hex(i)); break
    ok = i == len(body)
    print(os.path.basename(path), "raw", len(raw), "dec", len(body),
          "blocks", len(blocks), "end", ok)
    for tag, off, size in blocks:
        print("   ", tag, hex(off), size, "end", hex(off + 12 + size))
```

Aggregate per-tag min/max (second `%TEMP%` helper) and hash re-checks:

```powershell
py -3 "$env:TEMP\p2_15a_stats.py"
Get-FileHash -Algorithm SHA256 "I:\Games\Златогорье-2\Saves\slot0.sav"
Get-FileHash -Algorithm SHA256 "I:\Games\Златогорье-2\Server.dll"
Get-FileHash -Algorithm SHA256 ".local\decomp\server\decompiled.c"
```

Observed: `orders 1 ('BT_SCRN ', 'BT_INFO ', 'BT_VARS ', 'BT_TRIBE', 'BT_DIARY', 'BT_HSTRY', 'BT_HERO ')`;
slot0, `Server.dll` and `decompiled.c` hashes match the provenance table above.

## Writer (`FUN_14031b64`)

Source: `.local/decomp/server/decompiled.c:35327-35517`; entry comment (35327)
records `rva=0x31b64 size=1445 callers=0`. `this` is the game/server object;
`param_1` is the save name, copied into the 260-byte `local_2e4` buffer at
35375-35381.

### Preconditions and setup

- 35374: saving is attempted only when `*(int *)(DAT_1407fc10 + 0x8c) != 0`
  and `FUN_14027f7c(this) != -1`; otherwise only cleanup runs (35512-35516).
- 35382: `iVar3 = (**(code **)(DAT_140891bc + 0xcc))()` — world/manager vtable
  object used by `BT_SCRN ` (35385), `BT_HSTRY` (35404) and the feedback path
  (35495-35501).
- 35383: `CreateDirectoryA(s_Saves_1408ff5c, 0)` with `s_Saves_1408ff5c` =
  `"Saves"` (string RVA `0x8ff5c`, verified in `Server.dll`).
- Two serializers with vtable `PTR_FUN_1408fe58`: `local_300` for the block
  payload (35369-35373) and `local_1b8` for the header/file stream
  (35423-35427).

### Writer call table (decompiled.c:35384-35422)

| block | writer call | decompiled lines |
|---|---|---|
| `BT_SCRN ` | field callback `iVar3+0x50` with `(500,0x177)` | 35384-35385 |
| `BT_INFO ` | `FUN_1400116c`/`FUN_1400126c`/`FUN_1400127c`, `DAT_1407fcc8+4`, `DAT_14088e14+0x18`, two strings via `DAT_14088e30+8/+0x54`, `this+0x26c8` 8 bytes | 35386-35396 |
| `BT_VARS ` | `FUN_14002324(DAT_1407f9a0, ...)` | 35397-35398 |
| `BT_TRIBE` | `FUN_14016828(DAT_14089fc0, ...)` | 35399-35400 |
| `BT_DIARY` | `FUN_14058768(DAT_14088e04, ...)` | 35401-35402 |
| `BT_HSTRY` | vtable `iVar3+0x28` +4 | 35403-35404 |
| `BT_HERO ` | `FUN_1401e690` + `FUN_1401e340` | 35405, 35421-35422 |

Every block is opened by `FUN_14001000(local_300, "<tag>")` at 35384, 35386,
35397, 35399, 35401, 35403 and 35405 respectively. Exact per-block writes:

```
BT_SCRN  35384 FUN_14001000(local_300, "BT_SCRN ")
         35385 (**(code **)(iVar3 + 0x50))(local_300, 500, 0x177)
BT_INFO  35386 FUN_14001000(local_300, "BT_INFO ")
         35387 FUN_1400116c(local_300, DAT_1407fcc8 + 4)
         35388 FUN_1400126c(local_300)
         35389 (**(code **)(*DAT_14088e14 + 0x18))(local_1e0)
         35390 FUN_1400127c(local_300, local_1e0, 0x20)
         35391 pcVar9 = (**(code **)(*DAT_14088e30 + 8))(0)
         35392 FUN_1400116c(local_300, pcVar9)
         35393 pcVar9 = (**(code **)(*DAT_14088e30 + 0x54))()
         35394 FUN_1400116c(local_300, pcVar9)
         35395 FUN_1400126c(local_300)
         35396 FUN_1400127c(local_300, this + 0x26c8, 8)
BT_VARS  35397 FUN_14001000(local_300, "BT_VARS ")
         35398 FUN_14002324(DAT_1407f9a0, local_300)
BT_TRIBE 35399 FUN_14001000(local_300, "BT_TRIBE")
         35400 FUN_14016828(DAT_14089fc0, local_300)
BT_DIARY 35401 FUN_14001000(local_300, "BT_DIARY")
         35402 FUN_14058768(DAT_14088e04, local_300)
BT_HSTRY 35403 FUN_14001000(local_300, "BT_HSTRY")
         35404 (**(code **)(**(int **)(iVar3 + 0x28) + 4))(local_300)
BT_HERO  35405 FUN_14001000(local_300, "BT_HERO ")
         35406-35420 select pvVar8 = hero at index FUN_14027f7c(this) using
                     presence bitmap this+0x256c / this+0x2568 and array
                     this+0x2564 (else pvVar8 = 0)
         35421 FUN_1401e690(pvVar8, local_300)
         35422 FUN_1401e340(pvVar8, local_300)
```

### Resolved RVAs (`functions.csv`)

CSV columns are `rva,size,name,thunk,callers`.

| function | RVA | size | callers | definition (decompiled.c) | writer call site |
|---|---|---|---|---|---|
| `FUN_14031b64` | `0x31b64` | 1445 | 0 | 35329 | — (save entry) |
| `FUN_14002324` | `0x2324` | 202 | 1 | 1383 | 35398 |
| `FUN_14016828` | `0x16828` | 185 | 1 | 16955 | 35400 |
| `FUN_14058768` | `0x58768` | 350 | 1 | 60171 | 35402 |
| `FUN_1401e690` | `0x1e690` | 198 | 1 | 21880 | 35421 |
| `FUN_1401e340` | `0x1e340` | 226 | 1 | 21743 | 35422 |

`FUN_14031b64` has `callers=0` in static xrefs (invoked through a vtable or
function pointer); each of the five block helpers has exactly one static caller,
this writer.

### Header write sequence (decompiled.c:35423-35522)

1. 35423-35427: create file serializer `local_1b8` (vtable `PTR_FUN_1408fe58`),
   zero counters, `local_8 = 2`.
2. 35428: `local_188 = 0x11` — version 17.
3. 35429-35448: measure `s_Goldenland_II_save_1408fe60` and copy it into the
   20-byte `local_19c`; the literal is `"Goldenland II save "` (19 chars + NUL,
   RVA `0x8fe60`).
4. 35449-35450: `local_174[0] &= 0xffffff00`; `GetLocalTime(&local_184)` —
   16-byte `SYSTEMTIME`.
5. 35451-35466: zero `local_174[0..0xf]` (64-byte name field).
6. 35467-35474: copy the save name `local_2e4` into `local_174`.
7. 35475: `FUN_1400127c(local_1b8, local_19c, 0x68)` — write the 104-byte
   header.
8. 35476: `FUN_14001050(local_300)` — finalize the block serializer.
9. 35477: `FUN_14001580(local_1b8, (int)local_300, 3)` — append the block
   stream to the file stream (section type 3).
10. 35478: `FUN_14032c2c(this)`.
11. 35479-35492: enumerate `this+0x26d8` (`iVar5` entries), for each write its
    name string (`FUN_1400116c`/`FUN_1400126c`) and call
    `FUN_140013b0(pvVar8, 0, 0)`; then `FUN_14005bcc`.
12. 35493: `FUN_1406ad5a(local_12c, s_Saves_slot_d_sav_1408ffb8)` — format
    `"Saves\slot%d.sav"` (RVA `0x8ffb8`).
13. 35494: `FUN_1400153c(local_1b8, local_12c)` — finalize/write the file.
14. 35495-35497: write UI message id `0xb2` through the vtable.
15. 35498-35501: if `*(iVar3 + 0x34) != 0`, print
    `"Game was saved to file: %s"` (RVA `0x8ffcc`) with the path.
16. 35503-35510: destroy both serializers, restore `ExceptionList`, return.

The `0x68`-byte header written at 35475:

| offset | size | source | content |
|---|---|---|---|
| `0x00` | 20 | `local_19c` | `"Goldenland II save \0"` |
| `0x14` | 4 | `local_188` | version `0x11` (17) |
| `0x18` | 16 | `local_184` | `SYSTEMTIME` from `GetLocalTime` |
| `0x28` | 64 | `local_174` | save name, NUL-padded |

Cross-check against `slot0.sav` `raw[0:0x70]`: magic `"Goldenland II save \0"`,
version `11 00 00 00`, `SYSTEMTIME` = 2022-11-06 17:59:10.068, name
`"Quick save"`, then `5b cf 0d 00` = `0x0dcf5b` = 905051 = decompressed payload
length and `51 a8 03 00` = `0x03a851` at `0x6c`, with zlib `78 da` starting at
`0x70` (112). The writer's `0x68` header therefore equals `raw[0:0x68]`; the
remaining 8 bytes before the zlib stream are emitted by the stream/file layer
(`FUN_14001580`/`FUN_1400153c`), not by the block writers. Their exact semantics
are deferred to the reader task (open question: the second `u32` `0x03a851`).

### Block order matches the 28/28 container scan

Emission order: `BT_SCRN ` → `BT_INFO ` → `BT_VARS ` → `BT_TRIBE` →
`BT_DIARY` → `BT_HSTRY` → `BT_HERO ` (35384, 35386, 35397, 35399, 35401,
35403, 35405). Re-running the container scan (2026-11-12) over the 28 saves
gives `files 28 distinct_orders 1`, and the single tuple is exactly this
sequence — writer order equals on-disk order in all 28 files.

### Tag literals

Tags are fixed 8-byte values taken from literals in `Server.dll`:
`BT_SCRN ` `0x8ff64`, `BT_INFO ` `0x8ff70`, `BT_VARS ` `0x8ff7c`,
`BT_TRIBE` `0x8ff88`, `BT_DIARY` `0x8ff94`, `BT_HSTRY` `0x8ffa0`,
`BT_HERO ` `0x8ffac`. `BT_SCRN `/`BT_INFO `/`BT_VARS `/`BT_HERO ` are 7 chars
plus trailing `0x20`; `BT_TRIBE`/`BT_DIARY`/`BT_HSTRY` are exactly 8 letters
with no pad byte. This refines the "right-padded with `0x20`" wording of
§Container/Framing for those three tags.

## Reader (`FUN_1403210c`)

Source: `.local/decomp/server/decompiled.c:35523-35696`; entry comment (35521)
`rva=0x3210c size=1261 callers=1`. Its single caller is `FUN_14033214`
(RVA `0x33214`, load-game entry) at 36222. `param_1` is the game object, the
same `this` the writer receives. Client-side references below use
`Client.dll` SHA-256 `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`
and `.local/decomp/client/decompiled.c`.

### Entry sequence (decompiled.c:35598-35627)

| lines | call | effect |
|---|---|---|
| 35598 | `iVar3 = (**(code **)(DAT_140891bc + 0xcc))()` | same world/manager object as writer 35382 |
| 35599 | `(**(code **)(**(int **)(iVar3 + 0x18) + 0x1c))(0)` | pre-load call on object `*(iVar3+0x18)`, vtable `+0x1c`, argument `0`; happens before the `.sav` is opened |
| 35600 | `FUN_1406ad5a(local_36c, "Saves\slot%d.sav")` | path format |
| 35601 | `FUN_140014c0(local_40c)` | open and read the whole `.sav` (RVA `0x14c0`) |
| 35602 | `FUN_14001324(local_40c, local_3d4, 0x68)` | read the 0x68 header |
| 35603 | `FUN_14001668(local_40c, local_3f0)` | read two `u32`, then inflate the block payload (RVA `0x1668`) |
| 35604-35626 | count/name/size/inflate loop | post-zlib extra entries into `this+0x26d8` |
| 35627 | `FUN_14001484(local_40c)` | free the file stream |

### Payload framing resolved (open question from §Writer/Header)

`FUN_14001668` (decompiled.c:493-512) reads `u32 local_10` (line 500) and
`u32 local_c` (501), allocates `local_10` bytes (502), inflates `local_c`
bytes from the file with `FUN_1405d6b4` (RVA `0x5d6b4`, 503-504), and advances
the file position by `local_c` (510). Therefore:

- `raw[0x68]` is the **uncompressed** payload length (slot0 `0x0dcf5b` = 905051).
- `raw[0x6c]` is the **compressed** zlib length (slot0 `0x03a851` = 239697);
  the stream is `raw[0x70 : 0x70+0x3a851]` ending at `0x3a8c1`.

This closes the writer's open question: the second `u32` `0x03a851` is the
compressed length, not another payload field.

The extra entries after the zlib stream map 1:1 between writer
(35479-35492) and reader (35604-35626):

| writer | reader | meaning |
|---|---|---|
| 35479 `FUN_14005c1c(this+0x26d8, ...)` | 35604 `FUN_1400129c(local_40c,-1)` | entry count |
| 35485 `FUN_1400116c(local_1b8, name)` | 35608 `FUN_14001114(local_40c, local_36c, 0x104)` | name string (u32 length + bytes) |
| 35486 `FUN_1400126c(local_1b8)` | 35609 `FUN_1400129c(local_40c,-1)` | per-entry size |
| 35487-35488 `FUN_140013e4` / `FUN_140013b0` | 35620-35621 `FUN_140013e4` / `FUN_140013b0` | entry content |
| 35479-35492 store in `this+0x26d8` | 35622 `FUN_14005e78(this+0x26d8, name, obj)` | dictionary insert |

slot0 cross-check (2026-11-12, `py -3`, read-only): count at `0x3a8c1` = 213;
213 × `(u32 len + name + u32 size + data)` ends exactly at `0x9cd69` = raw
length 642409. Names are level ids (e.g. `L1_1`, `L93_4`, `RL_30`).

### Block read order (decompiled.c:35628-35686)

| read order | block | reader call(s) | lines |
|---|---|---|---|
| 1 | `BT_INFO ` | `FUN_14001084(local_3f0, "BT_INFO ")` + fields (see next table) | 35628-35662 |
| 2 | `BT_VARS ` | `FUN_14001084(...)`; `FUN_140023f0(DAT_1407f9a0, local_3f0)` (RVA `0x23f0`) | 35664-35665 |
| 3 | `BT_TRIBE` | `FUN_14001084(...)`; `FUN_140168e4(DAT_14089fc0, local_3f0)` (RVA `0x168e4`) | 35666-35667 |
| 4 | `BT_DIARY` | `FUN_14001084(...)`; `FUN_140588c8(DAT_14088e04, local_3f0)` (RVA `0x588c8`) | 35668-35669 |
| 5 | `BT_HERO ` | `FUN_14001084(...)`; `FUN_1401e424(this, local_3f0, 0)` (RVA `0x1e424`); count via `FUN_1400129c`; per-person loop `FUN_1405c83c(2)` (RVA `0x5c83c`), vtable `+4` read, `piVar8[0xaf0]=1`, `piVar8[0x3ca]=0`, `FUN_140368d4(param_1, piVar8)` (RVA `0x368d4`) | 35670-35684 |
| 6 | `BT_HSTRY` | `FUN_14001084(...)`; `(**(code **)**(undefined4 **)(iVar3 + 0x28))(local_3f0)` — object at `iVar3+0x28`, vtable slot `+0` | 35685-35686 |

`BT_SCRN ` (on-disk block 1) is not opened by tag in `FUN_1403210c`; see
§BT_SCRN below.

Reader helper RVAs (`functions.csv`, columns `rva,size,name,thunk,callers`):
`FUN_14001084` `0x1084` (144, callers 3), `FUN_14001114` `0x1114` (87, 4),
`FUN_14001214` `0x1214` (21, 6), `FUN_140014c0` `0x14c0` (124, 1),
`FUN_14001668` `0x1668` (125, 3), `FUN_140023f0` `0x23f0` (287, 1),
`FUN_140037d0` `0x37d0` (121, 1), `FUN_140168e4` `0x168e4` (177, 1),
`FUN_1401e424` `0x1e424` (618, 1), `FUN_140325fc` `0x325fc` (1584, 2),
`FUN_140368d4` `0x368d4` (72, 4), `FUN_140588c8` `0x588c8` (594, 1),
`FUN_1405c83c` `0x5c83c` (409, 7).

### `BT_INFO ` reader fields (decompiled.c:35628-35662)

| order | writer (line) | reader (line) | destination |
|---|---|---|---|
| 1 | 35387 string from `DAT_1407fcc8+4` | 35629-35630 `FUN_14001214` → `FUN_140037d0(DAT_1407fcc8, buf)` | global `DAT_1407fcc8` |
| 2 | 35388 `FUN_1400126c` (value arg lost in decompile) | 35631-35632 `FUN_1400129c` | `param_1+0x218` |
| 3 | 35389-35390 `DAT_14088e14` vtable `+0x18` → write | 35633-35634 read 0x20 → `DAT_14088e14` vtable `+0x1c` | `DAT_14088e14` object |
| 4 | 35391-35392 string from `DAT_14088e30` vtable `+8` | 35635 `FUN_14001214` (result discarded into `DAT_1407f880`) | global `DAT_1407f880` |
| 5 | 35393-35394 string from `DAT_14088e30` vtable `+0x54` | 35636 `FUN_14001114(local_3f0, local_244, 0x104)` | `local_244` |
| 6 | 35395 `FUN_1400126c` (value arg lost in decompile) | 35637-35638 `FUN_1400129c` | `param_1+0x26c4` |
| 7 | 35396 8 bytes from `this+0x26c8` | 35639 read 8 bytes | `param_1+0x26c8` |

Requested BT_INFO offsets, exact lines:

- `+0x218` ← `u32` at **35631-35632** (`uVar7 = FUN_1400129c(...)`;
  `*(undefined4 *)((int)param_1 + 0x218) = uVar7`). Caller `FUN_14033214`
  zeroes it at 36164 before dispatch.
- `+0x26c4` ← `u32` at **35637-35638**.
- `+0x26c8` ← **8 bytes** at **35639** (`FUN_14001324(..., +0x26c8, 8)`);
  writer reads the same 8 bytes at 35396.
- string field: `local_244` is read at **35636** and passed to
  `FUN_140325fc(param_1, &local_244, 0)` at **35640**. `FUN_140325fc`
  (RVA `0x325fc`, 35704-...) looks the name up in the extra-entry dictionary
  `this+0x26d8` (35770), attaches/inflates that entry (35772), then loads the
  level `.sef` plus `BT_RGINF`/`BT_ENTS` (35774-35847). The same name is then
  length-prefixed and copied into `param_1+0x2574` (**35641-35662**, 0x42
  qwords = 0x210 bytes), matching the load-entry's own copy at 36181-36193.

slot0 cross-check (`.sav` `8AC545FE...`, `py -3`): `BT_INFO ` body is 100
bytes and parses exactly as writer order — string1 `Вертас` (cp1251),
`u32 #1 = 0x0`, 0x20 blob (`466 5 1e 4 14 1d 19 78` as `u32`s), string2
`Запчасть Черного - тронный зал`, string3 `L93_4`, `u32 #2 = 0x28`,
8 bytes `(0x4d0, 0x1a8)`; consumed 100/100. `L93_4` is present in the 213
extra entries. This confirms the field order and the
`FUN_140325fc` name → extra-entry relationship.

### Writer/reader order asymmetry

Writer/on-disk tail order is `… BT_DIARY` → `BT_HSTRY` (35403) →
`BT_HERO ` (35405). Reader tail order is `… BT_DIARY` → `BT_HERO `
(35670) → `BT_HSTRY` (35685). Recorded as observed; no format inference
beyond this.

The asymmetry is harmless because each lookup is independent of position:
`FUN_14001084` (RVA `0x1084`, decompiled.c:50-77) seeks to offset 0 (59),
reads the 8-byte tag as two `u32` (63-69), returns after the matching tag's
`u32` size at (70) with the stream at the body start, and for a non-match
reads the size (73) and seeks forward by it (74). Client `FUN_12001000`
(RVA `0x1000`, Client decompiled.c:5-32) is the same scan. Tags+size make
block order irrelevant; only the tag and its size define a block.

Other observed writer/reader asymmetries:

- `BT_HSTRY` vtable slot: writer uses object `iVar3+0x28` slot `+4` (35404);
  reader uses slot `+0` (35686).
- `BT_INFO ` 0x20-byte payload: writer uses `DAT_14088e14` vtable `+0x18`
  (35389); reader uses `+0x1c` (35634).
- `BT_HERO `: writer serializes the single active hero via `FUN_1401e690` /
  `FUN_1401e340` (35421-35422); reader uses `FUN_1401e424` and a
  count/read loop that allocates each person (`FUN_1405c83c(2)`), reads it
  via vtable `+4`, and installs it with `FUN_140368d4` (35671-35683).

### `BT_SCRN ` status

**Server side: unverified — absent from `FUN_1403210c`.** `FUN_1403210c`
never opens `BT_SCRN ` by tag and never calls the writer's `iVar3+0x50`
preview-generation callback (35385). Its first lookup is `BT_INFO ` at 35628; the
tag+size scan inside `FUN_14001084` starts at offset 0 (59), reads the
`BT_SCRN ` tag and its 750008 size from the stream, and seeks past the body
(73-74) to `0x0b71c4`, where `BT_INFO ` begins. So the server inflates the
whole payload including `BT_SCRN ` at 35603 but does not parse its body.
The call at 35599 (`*(iVar3+0x18)` vtable `+0x1c`, argument `0`) runs before
the `.sav` is opened, so it cannot consume the block.

Xref evidence: `s_BT_SCRN_1408ff64` (RVA `0x8ff64`) appears exactly once in
`Server.dll`'s `decompiled.c` — writer 35384. In `Client.dll`'s
`decompiled.c` the tag appears exactly once — 88193.

**Client side: consumer identified (save-slot metadata loader).**
`FUN_12088444` (Client.dll RVA `0x88444`, callers 2, lines 88148-88228)
opens the block at **88193** with `FUN_12001000(local_44, "BT_SCRN ")`, then
calls `FUN_120228a8(DAT_1210fd04, param_1+0x3ecc)` (88194) and
`FUN_12022b98(this, local_44, param_1+0x3ecc)` (88197). `FUN_12022b98`
(RVA `0x22b98`, callers 1, lines 27088-27247) reads two `u32` dimensions
(27138-27139), then `width*height` `u32` cells (27226-27237). Each cell is
converted by `FUN_12005710` (RVA `0x5710`, 27229): its third argument is a
pixel-format descriptor (descriptor words `[0..5]` supply shifts/masks,
`FUN_12005710` 3933-3957), and `this+0x4740` is that descriptor, not the
target grid. The converted `u16` is written at 27230 to the locked surface
buffer (`local_7c`/`puVar7`; lock at 27217, unlock at 27239) with row pitch
`iStack_90`. slot0: dimensions are **500 × 375**; body is
`750008 = 8 + 500*375*4`, ending exactly. The caller of `FUN_12088444` is the
slot-selection UI (`FUN_1208886c` RVA `0x8886c` call at 88345;
`FUN_12088e60` RVA `0x88e60` call at 88515), and `FUN_12088444` also copies
`BT_INFO ` metadata into the slot record: `+0x5ed0` 0x20 bytes and `+0x3ed0`
a 0x2000 text buffer (88199-88202); 88203-88220 append the formatted date
into `+0x3ed0`. This is the save-slot preview path.

Status: **confirmed save preview/thumbnail (client-only).** The body is
exactly `{u32 width, u32 height, u32 cell[width*height]}`; slot0 is
500×375, and `750008 = 8 + 500*375*4` (`py -3` check, 2026-11-12). The
server writer emits it at 35384-35385 (`iVar3+0x50(local_300, 500, 0x177)`;
`0x177 = 375`), and the server load path never parses the body (first tag
lookup is `BT_INFO ` at 35628; `FUN_14001084` seeks over the 750008 bytes at
59/73-74). The only reader in either DLL is the client slot-preview loader
`FUN_12088444` → `FUN_12022b98` above; the `BT_SCRN ` tag occurs exactly once
in each `decompiled.c` (server 35384, client 88193). The rendered preview
destination surface has no other identified consumer; whether it is retained
as the slot thumbnail is `unverified`. `this+0x4740` is only the pixel-format
descriptor, not the destination. See §BT_SCRN and world state.

Exact next Ghidra checks:

1. (resolved, Task 4) `Server.dll`: xrefs to string RVA `0x8ff64`
   (`BT_SCRN `) — only the writer `FUN_14031b64`; no reader xref.
2. (open) `Server.dll`: decompile the callback stored at field `+0x50` of the
   object returned by `(*(DAT_140891bc + 0xcc))` (invoked in the writer at
   35385), and any sibling read method (`+0x4c`/`+0x54`) to identify how the
   preview is generated.
3. (resolved, Task 4) `Client.dll`: the `BT_SCRN ` string (label
   `s_BT_SCRN_1212c9ac`) has exactly one xref, `FUN_12088444` (88193), in the
   slot-preview path (`FUN_1208886c`/`FUN_12088e60`); the buffer-reuse
   question moved to §BT_SCRN and world state.
4. (open) If static results are inconclusive, attach to a running game and
   watch the inflated 750008-byte buffer during a load to see which code reads
   it.

## BT_SCRN and world state

Task 4 result (2026-11-12). Static evidence only; runtime checks below are
marked `unverified`.

### Preview confirmation

- Arithmetic: `750008 = 8 + 500*375*4` (`py -3`, exact `True`). The 8-byte
  prefix is two `u32` dimensions; the rest is one `u32` per cell.
- Server writer: `FUN_14031b64` opens the tag at 35384 and calls
  `(**(code **)(iVar3 + 0x50))(local_300, 500, 0x177)` at 35385; `0x177 = 375`.
  The tag string RVA `0x8ff64` has no other server xref.
- Server reader: `FUN_1403210c` never opens `BT_SCRN `. Its first tag lookup is
  `BT_INFO ` at 35628; the scan in `FUN_14001084` (59, 73-74) reads the
  `BT_SCRN ` tag/size and seeks over the 750008-byte body to `0x0b71c4`.
  The payload is inflated whole at 35603, but the body is not parsed.
- Client consumer: `FUN_12088444` (RVA `0x88444`, 88148-88228) opens the save
  file (88180), seeks `0x68` (88191), opens the block at **88193**, and calls
  `FUN_12022b98` at **88197**. `FUN_12022b98` (RVA `0x22b98`, 27088-27247)
  reads width at 27138 and height at 27139, then `width*height` `u32` cells at
  27226-27233. `FUN_12005710` (27229; RVA `0x5710`, 3933-3957) uses its third
  argument as a pixel-format descriptor (words `[0..5]` supply shifts/masks,
  3945-3955); `this+0x4740` is that descriptor. The converted `u16` is
  written at 27230 to the locked surface buffer (`local_7c`/`puVar7`; lock at
  27217, unlock at 27239) with row pitch `iStack_90`. Slot0 dimensions are
  500×375 and the block ends exactly. `FUN_12088444` then reads `BT_INFO `
  metadata into the slot record (88198-88202; date appended 88203-88220), and
  its callers are the slot-selection UI (88345, 88515).
- Classification: `BT_SCRN ` is a save-slot preview/thumbnail consumed only by
  the client slot UI. Gameplay scene/world data is not read from it. The
  remaining `unverified` point is the destination surface's lifetime/use:
  exact next check: decompile `FUN_120223b8`/`FUN_12022838` (lock path at
  27217) and trace the locked surface consumers; do not use `this+0x4740`
  (pixel-format descriptor) for this check.

### Where dynamic world state lives

The save splits static and dynamic level data:

- **Static terrain** is reloaded from SEF using the level id in `BT_INFO `.
  The third `BT_INFO ` string (`local_244`, read 35636) is passed to
  `FUN_140325fc` at 35640. That function formats
  `levels/single/<name>.sef` (35779-35785), opens the pack, and reads
  `points_entrance` / `cell_groups` (35803-35810). Static geometry, entrances
  and cell groups therefore do not come from the save body.
- **Dynamic world state (persons/entities) is in the post-zlib tail**, not in
  the seven main blocks. After writing the main payload, the writer calls
  `FUN_14032c2c(this)` at **35478** and then writes the `this+0x26d8`
  dictionary at 35479-35492. The reader rebuilds the same dictionary at
  **35604-35626** before parsing blocks. When `BT_INFO ` is read,
  `FUN_140325fc` looks up the current level name in that dictionary (35770),
  inflates the per-level snapshot (35772), and reads:
  - `BT_RGINF` (35812-35815): `u32` + 0x20 bytes (36-byte body; contents
    `unverified`).
  - `BT_ENTS` (35847-35914): entity list. Each record is `u32 entity_type`
    followed by the entity body (`FUN_1405c83c(entity_type)` creates it,
    vtable `+4` reads it); the list terminates when the next `u32` equals
    **1** (reader 35848-35914).
- `FUN_14032c2c` produces that snapshot: `BT_PNAME` at 35980 (names of
  person entities with `(flags & 0x3e) == 2` and `+0x150 == 0`,
  36006-36010), `BT_RGINF` at 36040-36042, then `BT_ENTS` at 36043-36105 for
  entities with `(flags & 0x3a) != 0` and `+0x54 == 0` (36097-36103).

Slot0 parse (`py -3`, read-only): 213 tail entries, one per visited level
(names `L1_1`, `L93_4`, …). Entry section wrapper is
`u32 uncompressed_len`, `u32 compressed_len`, zlib stream. `L1_1` is 22975
bytes = three 12-byte tag+size headers + bodies 302 + 36 + 22601; `L93_4` is
8551 = 36 + 98 + 36 + 8381. `BT_PNAME` names look like
`L1_1.P387_Poison_Plotnik`, `L0.P678_citizen4`,
`L93_2.P292_Chernobogs_Guardian2`.

### What each main block carries

| block | evidence | domain |
|---|---|---|
| `BT_INFO ` | reader 35628-35640 | level id (third string), 0x20-byte world-time blob through `DAT_14088e14`, `u32` +0x218, `u32` +0x26c4, 8 bytes +0x26c8; triggers SEF + tail load |
| `BT_VARS ` | writer `FUN_14002324` 1383-1416; reader `FUN_140023f0` 1424-1456 | script globals: `u32` count, then per item `u32` type, name string, value (8 bytes for type <3, string for type 3). slot0: 1002 vars, types `{0:905, 1:94, 2:3}`, e.g. `voevoda_povtor`, `L1_plotnik_son_life` |
| `BT_TRIBE` | writer `FUN_14016828` 16955-16990; reader `FUN_140168e4` 16996-17032 | tribe ids/names + relation matrix: `u32` count, count×( `u32` id + string ), then count×count bytes. slot0: count 234, header 4128, matrix 234²=54756, total 58884. `FUN_14016998` (17038-17045) returns `matrix[i][j]`, default 2; call sites compare entity tribe id `+0x150c` (18551, 18564, 19275, 19370, 19669, 38961) |
| `BT_DIARY` | writer `FUN_14058768` 60171-60226; reader `FUN_140588c8` 60232-60358 | journal/quest records: `u32` count, per entry `u32 block_count = n+1` then `block_count*0x58` bytes (block 0 = entry header, blocks 1..n = records); second `u32` count then that many 0x58-byte records. slot0: 76 entries, second count 0, consumes all 32960 bytes |
| `BT_HSTRY` | writer 35404 (object `iVar3+0x28`, vtable `+4`); reader 35686 (vtable `+0`) | event/history log: slot0 1592 bytes, outer `u32` 5, then cp1251 length-prefixed strings + ids (first text `"Вертас использует предмет Слабое зелье здоровья"`); not world data |
| `BT_HERO ` | writer `FUN_1401e690` 21880-21899 + `FUN_1401e340` 21743-21774; reader `FUN_1401e424` 21780-21874 + 35670-35684 | the active hero person (three `u32` fields, 0x11c state, 0x600 buffer, 3 coordinates, nested `FUN_1401d84c` data) plus a count-prefixed list of attached entities installed with `FUN_140368d4`. slot0 starts `123, 131, 1`; this is hero/party state, not all level entities |

Conclusion: `BT_TRIBE` is a faction-relation matrix, `BT_VARS` holds script
globals, `BT_DIARY`/`BT_HSTRY` are UI logs, `BT_INFO` selects the level and
world time, and `BT_HERO ` holds the hero + attached entities. The full
dynamic population of every visited level is the tail `BT_ENTS`/`BT_RGINF`
snapshot keyed by level id.

### `unverified` items and exact next checks

1. **`BT_TRIBE` relation values.** Meaning of byte values (0/1/2; default 2)
   is `unverified`. Next: decompile the branches at RVA `0x168e4` call sites
   18551, 18564, 19275, 19370, 19669, 38961; cross-check the 234² matrix
   values across the 28 saves.
2. **`BT_HSTRY` exact schema.** Outer group count 5 and per-record id semantics
   are `unverified`. Next: decompile the object at `iVar3+0x28` in the writer
   (`+0x4` slot) and reader (`+0` slot); parse all 28 `BT_HSTRY` bodies.
3. **`BT_INFO ` 0x20-byte time blob.** Exact field layout (`DAT_14088e14`
   vtable `+0x18`/`+0x1c`/`+8`, `FUN_140037d0`) is `unverified`. Next:
   decompile those vtable slots and compare the slot0 blob
   (`466, 5, 0x1e, 4, 0x14, 0x1d, 0x19, 0x78` as `u32`s) across saves.
4. **`BT_HERO ` nested fields.** Meaning of the 0x11c state, 0x600 buffer and
   `FUN_1401d84c` tail is `unverified`. Next: decompile `FUN_1401d84c` /
   `FUN_1401d8bc` and the person vtable slots `+0x0`/`+0x4`.
5. **`BT_RGINF` semantics.** The `u32` + 0x20 body of each tail snapshot is
   `unverified`. Next: decompile `FUN_14032c2c` 36040-36042 and
   `FUN_140325fc` 35812-35815, then trace consumers of `this+0x26c0` and
   `this+0x26a0`.
6. **`BT_ENTS ` entity types.** Class map for `FUN_1405c83c(type)` and the
   `type == 1` terminator are `unverified`. Next: decompile the entity factory
   and per-class vtable `+0`/`+0x4`, then diff entity type sequences across
   saves.

### Minimum read set for P2-15b

Ordered by dependency (mirrors `FUN_1403210c` / `FUN_14033214`):

1. **Header + inflate.** `raw[0:0x68]` header, `raw[0x68]` uncompressed
   length, `raw[0x6c]` compressed length, zlib at `raw[0x70 : 0x70+len]`
   (`FUN_14001668`, 493-512).
2. **Tail dictionary.** After the zlib stream: `u32 count`, then per entry
   `u32 name_len + name + u32 section_len + section`, where section is
   `u32 uncompressed_len + u32 compressed_len + zlib`. Build
   `level_name → section` (reader 35604-35626).
3. **`BT_INFO `.** Level id (third string) selects both the SEF file and the
   tail section; also restore the 0x20 time blob, `+0x218`, `+0x26c4` and the
   8 bytes `+0x26c8` (35628-35640).
4. **Static SEF + dynamic snapshot.** Load `levels/single/<level>.sef`
   (`points_entrance`, `cell_groups`, 35779-35810), then the tail section's
   `BT_RGINF` and `BT_ENTS` (35812-35914; entity list terminated by type 1).
   `BT_PNAME` is not read by `FUN_140325fc`; it is read by the separate
   name→entity lookup at 38563, so load it only if that lookup is needed.
5. **`BT_VARS `** script globals (35664-35665).
6. **`BT_TRIBE`** faction relations (opaque matrix is enough initially;
   35666-35667).
7. **`BT_DIARY`** journal records (35668-35669).
8. **`BT_HERO `** hero + attached entities (35670-35684).
9. **`BT_HSTRY`** event log (35685-35686).

Essential for restoring a playable level: 1-5 + `BT_HERO `. `BT_TRIBE` can be
loaded opaquely until faction AI needs it. Deferrable: `BT_DIARY` and
`BT_HSTRY` (journal/log UI), and `BT_SCRN ` entirely (preview thumbnail; the
server never reads it, so P2-15b can skip it by tag+size and regenerate or
ignore the slot preview).

## Block domains and port coverage

Task 5 result (2026-11-12). Same `Server.dll` / `decompiled.c` hashes as
§Provenance. Corpus checks are read-only `py -3` scans of all 28
`Saves/slot*.sav` (parser kept in `%TEMP%`, not committed); the scans re-derived
each payload from `zlib.decompress(raw[112:])` and consumed every block body
exactly (`0` length mismatches on 28/28).

### Summary table

| block | domain | reader (RVA + `decompiled.c` lines) | writer (RVA + `decompiled.c` lines) | port coverage | status |
|---|---|---|---|---|---|
| `BT_SCRN ` | save-slot preview thumbnail (client-only) | client `FUN_12088444`/`FUN_12022b98` `0x88444`/`0x22b98`, client lines 88193/88197; server never opens it | `FUN_14031b64` `0x31b64`, 35384-35385 (`iVar3+0x50`, 500×375) | none (P2-15b may skip by tag+size) | confirmed preview; destination-surface lifetime `unverified` |
| `BT_INFO ` | level id, party level, world-time blob, session strings | `FUN_1403210c` `0x3210c`, 35628-35662 | `FUN_14031b64` `0x31b64`, 35386-35396 | `+0x26c4` model as caller-supplied `loot_level` (`npc_loot.hpp:43`, `npc_loot.cpp:89`); no save parser | research confirmed (§15.1/§15.5); port partial |
| `BT_VARS ` | script globals: type/name/value | `FUN_140023f0` `0x23f0`, 1424-1456 (call 35664-35665) | `FUN_14002324` `0x2324`, 1383-1416 (call 35397-35398) | `gl::script::ScriptGlobal` + `parse_globals` + `ScriptVm::globals_`; text `.scr` only, no save codec | layout known; port partial |
| `BT_TRIBE` | tribe ids/names + N×N relation matrix | `FUN_140168e4` `0x168e4`, 16996-17032 (call 35666-35667) | `FUN_14016828` `0x16828`, 16955-16990 (call 35399-35400) | none; SEF stores only `person.tribe` name (`sef.cpp:245-247`) | layout verified 28/28; semantics `unverified`; port missing |
| `BT_DIARY` | quest journal entries + records | `FUN_140588c8` `0x588c8`, 60232-60358 (call 35668-35669) | `FUN_14058768` `0x58768`, 60171-60226 (call 35401-35402) | none | framing verified 28/28; record fields `unverified`; port missing |
| `BT_HSTRY` | UI event/history log (40-slot ring) | host vtable `iVar3+0x28` slot `+0`, 35685-35686 | host vtable `iVar3+0x28` slot `+4`, 35403-35404 | none (`core::ConsoleBuffer` history is unrelated) | body verified 28/28; slot fields `unverified`; port missing |
| `BT_HERO ` | hero person + 3 inventories (hot/items/equipped) | `FUN_1401e424` `0x1e424`, 21780-21874 + 35670-35684 | `FUN_1401e690` `0x1e690` 21880-21899 / `FUN_1401e340` `0x1e340` 21743-21774; calls 35421-35422 | `decode_save_body`/`parse_hero_inventory`/`parse_hero_equipment` (`hero_equipment.cpp:44/82/142`) and `parse_hero_stats` (`hero_stats.cpp:24`); real slot0/2 tests | partial: inventory/equipment/stats verified; rest of hero block `unverified` |

`BT_VARS `, `BT_TRIBE`, `BT_DIARY` and `BT_HSTRY` have no occurrence in
`Client.dll`'s `decompiled.c`; they are server-side save blocks. The
`BT_HSTRY` tag string `s_BT_HSTRY_1408ffa0` is referenced twice in
`Server.dll`: writer 35403 and reader 35685.

### `BT_VARS ` — script globals

- Reader `FUN_140023f0` (RVA `0x23f0`, size 287, callers 1), body
  1424-1456; writer `FUN_14002324` (RVA `0x2324`, size 202, callers 1), body
  1383-1416. Global object `DAT_1407f9a0`; save call sites 35664-35665 and
  35397-35398.
- Body: `u32 count`, then per variable:
  - `u32 type` (`0` int, `1` DWORD, `2` float, `3` string; `0`/`1` are
    indistinguishable in the VM, `decomp-index.md:26` and
    `p3-vm-natives-server.md:274`);
  - `u32 name_len` + cp1251 name bytes, no NUL (`FUN_1400116c`; slot0 first
    name is `name_len=14`, `"voevoda_povtor"`);
  - type `< 3`: **8 raw bytes**, little-endian IEEE-754 `f64` (writer 1405
    writes 8 bytes from the variable object; reader 1442 reads 8 bytes into
    `local_228`). This is encoding-level, not declared type: slot0 type 0 has
    `10.0`, `1.0`, `3.0`; type 1 has `0.0`, `1.0`; type 2 has `0.5`. All 28
    saves contain only types 0/1/2 (`{0:23498, 1:2632, 2:61}`), so type 3 was
    not exercised on disk;
  - type `3`: `u32 value_len` + cp1251 value bytes, no NUL (writer 1408-1409,
    reader 1448-1449).
- Slot0: 1002 variables, types `{0:905, 1:94, 2:3}`, no type 3. Across 28
  saves: variable count 879-1006 (16 distinct), block size 27611-31144
  (16 distinct).
- Reader numeric path passes the value through setters `this+0x18`/`+0x1c`
  (or `+0x20` for strings). Ghidra renders the numeric argument as
  `_DAT_1408c3a0` at 1444/1450 while the 8 bytes land in `local_228`; the exact
  setter conversion is **`unverified`**. Exact next check: decompile the
  virtual setters at `DAT_1407f9a0` vtable slots `+0x18`, `+0x1c`, `+0x20`
  and the instruction at RVA `0x23f0+0x2e` (`0x241e`) to see whether the f64
  is passed in `xmm0`/`[esp]` or converted per type.
- Port coverage: `gl::script::ScriptGlobal{type,name,Value}` and
  `Value = variant<int32_t,uint32_t,double,string>` (`globals.hpp:10-16`,
  `value.hpp:7`) match the semantic fields; `parse_globals`
  (`globals.cpp:100`) parses text `.scr` declarations and `ScriptVm::set_globals`
  (`script_vm.cpp:214-216`) stores them in `globals_`. **No `BT_VARS` binary
  codec exists**; a loader must convert the save's f64 payload to the declared
  `int`/`DWORD`/`float`/`string` variant and a writer must convert back to f64.

### `BT_TRIBE` — faction relations

- Reader `FUN_140168e4` (RVA `0x168e4`, size 177, callers 1), body
  16996-17032; writer `FUN_14016828` (RVA `0x16828`, size 185, callers 1),
  body 16955-16990. Global object `DAT_14089fc0`; save calls 35666-35667 and
  35399-35400.
- Body: `u32 count`, then `count × (u32 id, u32 name_len, cp1251 name)`
  (no NUL), then `count × count` single bytes, row-major (`char`). Writer:
  count read at 16967, written at 16969, reset vtable `+0x14`, iterator vtable `+0x18` emits
  id+name, then each row is written as `count` raw bytes via the serializer
  vtable at 16975-16982. Reader: reset `FUN_140167c4` (17004), count into
  `this+0x20`, insert each `(name,id)` via vtable `+0x1c` (17010-17012),
  allocate `count*4` row pointers (`this+0x24`), allocate/read each `count`-byte
  row (17020-17030).
- All 28 saves: count is **234**; count+id/name table is 4128 bytes; matrix is
  `234² = 54756`; block is the fixed **58884** bytes. Names are cp1251 (e.g.
  `snake`, `L12_3_citizen`, `rebel`).
- Matrix values over all 28 saves: `{0:1257, 1:6096, 2:1519260, 3:6552, 4:3}`.
  The diagonal `matrix[i][i]` is **3** for all 234 entries. Per file the set is
  `{0,1,2,3}` (25 saves) or `{0,1,2,3,4}` (3 saves). `FUN_14016998` (RVA
  `0x16998`, 17038-17045) returns the signed byte `matrix[i][j]`, defaulting to
  **2** when `i` or `j` is out of range.
- Persons reference tribe ids at `person+0x150c`; call sites compare
  `FUN_14016998(DAT_14089fc0, this+0x150c, other+0x150c)` at 18551 (returns
  `1 < relation`), 18564, 19275, 19370, 19669 and 38961. Value labels are
  therefore **`unverified`** (observed: 18551 treats `<=1` as hostile; 19669
  and 38961 treat `0/1` as one branch and default `2` as another). Exact next
  check: decompile the full branch bodies at those six call sites and compare
  the 28 matrices. A controlled runtime experiment (setting two known tribes
  to each value and observing AI hostility) would require writing game
  memory and is out of scope; runtime confirmation is limited to read-only
  observation of existing saves and the static branch reading.
- Port coverage: none. `person.tribe` is a name string loaded from SEF
  (`sef.cpp:245-247`, `person.hpp:30`); there is no numeric tribe id, no
  name→id table and no relation matrix. `RS_GetTribesRelation` exists only as
  an unsupported CS native name (`cs.cpp:188`).

### `BT_DIARY` — quest journal

- Reader `FUN_140588c8` (RVA `0x588c8`, size 594, callers 1), body
  60232-60358; writer `FUN_14058768` (RVA `0x58768`, size 350, callers 1),
  body 60171-60226. Global object `DAT_14088e04`; save calls 35668-35669 and
  35401-35402.
- Body, first section: `u32 entry_count`; per entry `u32 block_count`, then
  `block_count × 0x58` bytes (writer 60186-60196, reader 60288-60297).
  `block_count = n+1`, where `n` is the record count stored at header
  `+0x04`; block 0 is the entry header, blocks 1..n are records. Checked on
  all 1013 serialized entries across 28 saves: `header[+0x04] ==
  block_count-1` with **0** mismatches. Block 0 also holds the entry key at
  `+0x00` (observed 0-550; the same key repeats across different saves); the
  remaining 0x50 header bytes and the 0x58 record fields are not mapped.
- Body, second section: `u32 sparse_count`, then `sparse_count × 0x58` bytes
  (writer 60197-60224, reader 60299-60358). In all 28 saves
  `sparse_count == 0`. The reader installs these into the sparse array
  (`this+0x10`, bitmap `this+0x18`) and the first section into the
  key→entry container (`this+4`, keyed by the entry key at `+0x00`).
- Packet arithmetic: minimum block 188 = `4 + (4 + 2×0x58) + 4`; slot0 is
  32960 bytes, 76 entries and `sparse_count = 0`. Across 28 saves: size
  188-33664 (24 distinct), entry_count 1-76 (13 distinct), per-entry
  `block_count` 2-14 (`n = 1..13`), `sparse_count = 0` in 28/28.
- Domain: quest/journal records, inferred from the writer's container walk and
  the existing domain hypothesis; individual field meanings are
  **`unverified`**. Exact
  next check: decompile `FUN_14058b1c` (`0x58b1c`, 60364-...) and
  `FUN_14058d68` (`0x58d68`, 60463-...) plus the 0x58-record accessors, then
  map the 76 slot0 headers and their records.
- Port coverage: none (no quest/journal data model or save code).

### `BT_HSTRY` — history/event log

- Writer 35403-35404 opens the tag and calls
  `(**(code **)(**(int **)(iVar3 + 0x28) + 4))(local_300)`; reader
  35685-35686 calls the same object's vtable slot `+0`. `iVar3` is the object
  returned by `(**(code **)(DAT_140891bc + 0xcc))()` (writer 35382, reader
  35598). `DAT_140891bc` is the host API object assigned in `GetServerAPI`
  (`0x5cbf0`, 63133-63146; caller is `GoldenLand.exe` per
  `p5-save-equipment.md`). Therefore the `+0xcc` callback and the object's
  vtable at field `+0x28` are implemented in the host executable, not in
  `Server.dll`; the two method bodies are not present in `decompiled.c`.
- Body schema was recovered from 28/28 saves: `u32 head`, then exactly **40
  ring slots**, each `u32 text_len` (length in bytes, excluding NUL),
  `text_len` cp1251 bytes, and `u32 id`; then `u32 tail`. Every save serializes
  40 slots even when most are empty (`text_len = 0`, id 0). Empty save
  arithmetic confirms the fixed ring: slot6/slot21 are 384 bytes =
  `4 + 40×8 + 56 + 4`, with only 2 non-empty texts (`25` and `31` bytes,
  excluding NUL). No non-empty text contains or ends in `0x00` (`0/1044`
  texts). All 1120 serialized record ids across 28 saves are **0**.
- Slot0: `head = 5`, 40 non-empty slots, `tail = 1`; first text is
  `"Вертас использует предмет Слабое зелье здоровья"`, later entries include
  `"Игра сохранена"`, `"Добавлена запись в журнал"`,
  `"Потерян предмет: Сфера-ключ"`, `"Вертас - замораживание"`,
  `"Вертас - отравление"`.
  Across 28 saves: size 384-1950 (27 distinct); `head` 1-37; `tail = 1` in 26
  saves and `2` in the two 2-record saves (slot6/slot21).
- Domain: UI event/history log (status-bar history), not world state; the
  texts are player-visible event messages. `head`/`tail` semantics and ring
  ordering are **`unverified`** (hypothesis: `head` is the most recently
  written ring index, since the 2-record saves have `head = 1`). Exact next
  check: decompile `GoldenLand.exe` at the `GetServerAPI` host object
  (`DAT_140891bc+0xcc`) and the `+0x28` object's vtable slots `+0`/`+4`, or
  read the object in memory at runtime; then confirm how `head` maps to the
  serialized slot order.
- Port coverage: none. `core::ConsoleBuffer` history
  (`console.hpp:10`, `console.cpp:23-53`) is console command history with a
  64-line cap and is unrelated to this save block.

### Existing `BT_INFO ` and `BT_HERO ` coverage

- `BT_INFO `: the save format is fully parsed in §Reader/`BT_INFO` (lines
  35628-35662); `+0x26c4` is the party level, and loot level is
  `+0x26c4 + 1` (`p3b2b2-npc-loot-server.md` §15.1, real 28/28 check in
  §15.5). Port side models the output but not the save input:
  `NpcLootGenerator::generate(..., std::int32_t loot_level, ...)`
  (`npc_loot.hpp:43`, `npc_loot.cpp:89`) and
  `generate_npc_inventories(..., loot_level, ...)` consume a level, but no
  `src/` code reads `BT_INFO ` or `+0x26c4` from a `.sav`; `SaveManager`
  loads only the 112-byte header (`save_manager.cpp:34-58`). Status: partial.
- `BT_HERO `: layout, equip marker at `+0x0C`, the three containers
  (`0x0C`, `0x10`, `0x109C`), item record (`0xC4 + 0x14×subcount`) and the
  equipped set are in `p5-save-equipment.md` §3-§9. Port side implements
  `decode_save_body` (`hero_equipment.cpp:44`), `parse_hero_inventory`
  (`:82`), `parse_hero_equipment` (`:142`) and `parse_hero_stats`
  (`hero_stats.cpp:24`), with real tests pinning slot0/slot2 container counts
  and equipped ids (`tests/assets/test_hero_equipment_real.cpp`). The stat
  block (`+0x00C`) **is** modelled by `parse_hero_stats`; the header
  (`+0x000`), 0x600 buffer (`+0x134`), tail 0x80/extra records and the
  attached-entity list are not modelled. Status: partial
  (equipment/inventory/stats verified, rest `unverified`).

### `unverified` items added by this task

1. **`BT_VARS` numeric setter conversion.** Whether the f64 saved bytes are
   converted to int/DWORD/float by the setter or stored as f64 in memory.
   Next: decompile `DAT_1407f9a0` vtable slots `+0x18`/`+0x1c`/`+0x20` and
   the call at RVA `0x241e`.
2. **`BT_TRIBE` relation value labels 0-4.** The matrix is verified and the
   diagonal is 3, but the gameplay meaning of 0/1/2/3/4 is not. Next:
   decompile the consumers at 18551, 18564, 19275, 19370, 19669, 38961.
3. **`BT_DIARY` 0x58 record fields.** Entry key and record count are verified;
   the remaining 0x50 header bytes and the record payload are opaque. Next:
   decompile `FUN_14058b1c`/`FUN_14058d68` and map slot0's 76 entries.
4. **`BT_HSTRY` host vtable methods and `head`/`tail`.** The body is verified
   on 28/28, but the 40-record ring ordering and the two header words are not.
   Next: decompile the `GoldenLand.exe` host object at
   `DAT_140891bc+0xcc` / vtable `+0x28` slots `+0`/`+4`.
5. **`BT_SCRN` destination surface lifetime.** Unchanged from §BT_SCRN:
   next check is `FUN_120223b8`/`FUN_12022838` and the surface consumers.

## Verdict / decomposition

Task 7 result (2026-11-12).

### Verdict: `implementable`

The save payload is implementable from static evidence plus the 28-save
corpus. The container framing is confirmed 28/28 (7 contiguous tag+size
blocks ending exactly at the decompressed length), and both directions are
located and mapped: writer `FUN_14031b64` (RVA `0x31b64`) and reader
`FUN_1403210c` (RVA `0x3210c`), with per-block helper RVAs. `BT_SCRN ` is a
client-only 500×375 preview, the tail dictionary holds per-level dynamic
snapshots (`BT_RGINF`/`BT_ENTS`), and the remaining gaps are field semantics
(OQ-1…OQ-12) with named decompile steps, not unknown framing. P2-15b can
start from the minimum read set in §Block domains and port coverage;
P2-15c builds on the same writer order.

### P2-15b — read-only load

Scope: load a `.sav` into the port's world/server state without writing
anything. Ordered read set (mirrors `FUN_1403210c`/`FUN_14033214`):

1. Header + inflate: `raw[0:0x68]`, uncompressed length at `0x68`,
   compressed length at `0x6c`, zlib at `0x70` (`FUN_14001668`, 493-512).
2. Tail dictionary: `u32 count`, then per entry
   `u32 name_len + name + u32 section_len + section`, where section is
   `u32 raw_len + u32 zlib_len + zlib`; build `level_name → section`
   (reader 35604-35626).
3. `BT_INFO `: level id (third string) selects both the SEF file and the
   tail section; restore the 0x20 time blob, `+0x218`, `+0x26c4` and the
   8 bytes `+0x26c8` (35628-35640).
4. Static SEF (`levels/single/<level>.sef`: `points_entrance`,
   `cell_groups`) + dynamic snapshot `BT_RGINF` / `BT_ENTS` from the tail
   section (35779-35914; entity list terminated by type 1).
5. `BT_VARS ` script globals (35664-35665).
6. `BT_HERO ` hero + attached entities (35670-35684).
7. `BT_TRIBE` faction matrix, loaded opaquely until the relation labels are
   known (35666-35667).
8. `BT_DIARY` journal and `BT_HSTRY` event log (35668-35669, 35685-35686);
   deferrable to a follow-up inside P2-15b.
9. `BT_SCRN ` is skipped by tag+size; the server never reads it.

Acceptance:

- Loading slot0 (and at least one other save) reproduces the save's decoded
  values with **0/N mismatches** for: level id, dynamic persons (`BT_ENTS`
  type sequence + per-person fields), quests (`BT_DIARY` entry/record
  counts), script globals (`BT_VARS` names/count) and hero
  inventory/equipment/stats (`BT_HERO`, already pinned by P5a real tests).
- Determinism: loading the same file twice yields identical state (no
  iteration-order/RNG dependence).
- Read-only: no file writes, no `game_save`, no process-memory writes;
  `BT_SCRN ` may be ignored entirely.

No acceptance item depends on a live original run; the residuals are
semantics questions with named decompile steps (see Open Questions).

### P2-15c — write / `game_save`

Scope: serialize port state into a valid `.sav` and wire `game_save`.

- Block order: `BT_SCRN ` → `BT_INFO ` → `BT_VARS ` → `BT_TRIBE` →
  `BT_DIARY` → `BT_HSTRY` → `BT_HERO ` (writer 35384-35405), then
  `FUN_14032c2c(this)` and the `this+0x26d8` tail dictionary
  (35478-35492), then the `0x68` header (magic, version `0x11`,
  `GetLocalTime`, name) + the 8 stream-layer bytes + the zlib stream
  (`FUN_14001580`/`FUN_1400153c`).
- Writer reuse: follow `FUN_14031b64`'s per-block call table (§Writer) and
  reuse the existing port encoders for `BT_HERO `
  inventory/equipment/stats; `BT_VARS ` must convert typed variants back to
  the on-disk f64/string encoding (OQ-5). `BT_SCRN ` can be emitted as a
  deterministic placeholder or a regenerated 500×375 buffer; OQ-2 only
  affects pixel fidelity.
- File target: `Saves\slot<N>.sav`; header `version 17`; name field from the
  save name.
- Acceptance (round-trip): write port state → decode the produced file with
  the port reader and the original layout rules → compare field by field
  with the source state, **0/N mismatches**; re-reading the written file
  twice is byte-identical (deterministic). Block order on disk must equal
  the confirmed 28/28 order; block sizes are recomputed from bodies (no
  fixed padding). An original-game load of the written file is an optional
  user-assisted gate, not a static requirement.

### Out of scope (both subprojects)

- Save-slot UI changes and quick-save/console wiring beyond the
  `game_save`/`game_load` entry points.
- Rendering or retaining the `BT_SCRN ` preview thumbnail in the port's slot
  UI; destination-surface lifetime (OQ-1) and host preview callback (OQ-2).
- Full quest/journal field semantics (OQ-8/OQ-9) beyond counts and opaque
  round-trip; faction relation labels (OQ-7) beyond loading the matrix.
- `GoldenLand.exe` host methods (`BT_HSTRY`/preview callbacks) unless a
  subproject explicitly needs them.
- Any write to game memory or controlled value experiments; live work stays
  read-only.
- DRM/`protect.dll` and running original binaries.

## Open Questions

Task 6 result (2026-11-12). Every item still `unverified` in §Reader,
§`BT_SCRN ` and world state, and §Block domains and port coverage is listed
once below. Each item names the evidence already checked (RVA/`decompiled.c`
line/corpus) and the exact next check; none is closed by a live diff (see
§Live diff decision). Items are ordered by block, not by priority.

### OQ-1 `BT_SCRN ` destination surface lifetime

- **Question:** which surface receives the converted 500×375 preview, and is
  the thumbnail retained for the slot or regenerated on each load?
- **Checked:** client `FUN_12088444` (`0x88444`, 88148-88228) →
  `FUN_12022b98` (`0x22b98`, 27088-27247); surface lock 27217, `u16` write
  27230, unlock 27239; cell converter `FUN_12005710` (`0x5710`, 3933-3957);
  `this+0x4740` is the pixel-format descriptor, not the destination;
  `750008 = 8 + 500*375*4` verified on slot0.
- **Next:** decompile `FUN_120223b8` (client RVA `0x223b8`, 1152 bytes,
  26 callers) and `FUN_12022838` (`0x22838`, 111 bytes, 52 callers) and trace
  every consumer of the surface locked at 27217.
- **Impact:** P2-15b can skip `BT_SCRN ` by tag+size (the server never reads
  it). P2-15c writer fidelity (emitting a valid 750008-byte block) is a
  separate topic and is tracked as OQ-2; OQ-1 only affects the client-side
  display/validation path and the destination surface's lifetime.

### OQ-2 `BT_SCRN ` server preview-generation callback

- **Question:** how does the function pointer stored at field `+0x50` of the
  object returned by `(**(code **)(DAT_140891bc + 0xcc))()` (writer 35385)
  render the 500×375 preview? This is a field callback, not a vtable slot.
- **Checked:** writer 35384-35385
  `(**(code **)(iVar3 + 0x50))(local_300, 500, 0x177)`; `DAT_140891bc` is the
  host API object set in `GetServerAPI` (`0x5cbf0`, 63133-63146) by
  `GoldenLand.exe`; the `BT_SCRN ` tag has no other server xref. Task 4
  records the same as the "callback stored at field `+0x50`".
- **Next:** decompile `GoldenLand.exe`
  (`I:\Games\Златогорье-2\GoldenLand.exe`, SHA-256
  `7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF`,
  372736 bytes): decompile the host `+0xcc` getter at `DAT_140891bc+0xcc`,
  follow the object it returns, and decompile the function pointer stored at
  field `+0x50` (and the adjacent `+0x4c`/`+0x54` fields).
- **Impact:** P2-15c write fidelity only; P2-15b is unaffected.

### OQ-3 `BT_INFO ` 0x20-byte world-time blob

- **Question:** what is the field layout and unit of the 0x20-byte blob
  written through `DAT_14088e14` vtable `+0x18` (35389) and read through
  `+0x1c` (35633-35634)?
- **Checked:** writer 35389-35390
  `(**(code **)(*DAT_14088e14 + 0x18))(local_1e0)` then
  `FUN_1400127c(local_300, local_1e0, 0x20)`; reader 35633-35634 reads 0x20
  bytes and calls `DAT_14088e14` vtable `+0x1c`; slot0 blob as `u32`s is
  `466, 5, 0x1e, 4, 0x14, 0x1d, 0x19, 0x78`.
- **Next:** decompile `DAT_14088e14` vtable slots `+0x18`/`+0x1c`, then
  compare the 0x20-byte blob across the 28 saves.
- **Impact:** P2-15b world time / day-night restore; P2-15c round-trip
  fidelity.

### OQ-4 `BT_INFO ` session strings and scalar fields

- **Question:** what do string1 (`DAT_1407fcc8+4`, writer 35387 / reader
  35629-35630), string2 (`DAT_14088e30` vtable `+8`, 35391-35392 / 35635),
  the `u32` at `+0x218` (35631-35632) and the 8-byte `+0x26c8` (35396 / 35639)
  mean?
- **Checked:** slot0 is `Вертас` / `Запчасть Черного - тронный зал` /
  `L93_4`, `+0x218 = 0`, `+0x26c8 = (0x4d0, 0x1a8)`, `+0x26c4 = 0x28`;
  string3 (level id) → `FUN_140325fc` and `+0x26c4` are already resolved; the
  other fields have no identified consumer beyond storage.
- **Next:** decompile `DAT_1407fcc8` (+4), `DAT_14088e30` vtable
  `+8`/`+0x54`, and the consumers of `param_1+0x218` and `param_1+0x26c8` in
  the load entry `FUN_14033214` (`0x33214`) and gameplay code.
- **Impact:** medium — UI labels and round-trip; the level id and `+0x26c4`
  do not block P2-15b.

### OQ-5 `BT_VARS ` numeric setter conversion

- **Question:** does the reader convert the saved f64 to the declared
  int/DWORD/float, or store f64 in memory unchanged?
- **Checked:** writer `FUN_14002324` 1383-1416 writes 8 raw bytes; reader
  `FUN_140023f0` 1424-1456 reads 8 bytes into `local_228` and passes them
  through `DAT_1407f9a0` vtable `+0x18`/`+0x1c` (`+0x20` for strings);
  Ghidra renders the numeric argument as `_DAT_1408c3a0` at 1444/1450; corpus
  values are IEEE-754 f64 (`10.0`, `1.0`, `0.5`).
- **Next:** decompile `DAT_1407f9a0` vtable slots `+0x18`/`+0x1c`/`+0x20`
  and the instruction at server RVA `0x241e` (`0x23f0+0x2e`).
- **Impact:** high for P2-15b script-global values (typed VM storage) and
  P2-15c write-back.

### OQ-6 `BT_VARS ` type 3 (string) not exercised

- **Question:** confirm the type-3 on-disk encoding (`u32 value_len` +
  cp1251, no NUL) end-to-end, since 0/28 saves contain type 3.
- **Checked:** writer 1408-1409 and reader 1448-1449; corpus aggregate types
  `{0:23498, 1:2632, 2:61}` — no type 3.
- **Next:** decompile the writer type dispatch 1404-1410 and reader
  1445-1450 to confirm both use `FUN_1400116c`/`FUN_14001114`. The 28-save
  corpus contains no type-3 sample, so the only read-only route to an
  end-to-end sample is a user-created save that happens to contain a string
  global; our access stays read-only.
- **Impact:** P2-15c completeness only; P2-15b can accept the
  statically-derived layout.

### OQ-7 `BT_TRIBE` relation value labels

- **Question:** what is the gameplay meaning of matrix values `0..4`
  (diagonal = 3; default 2 out of range)?
- **Checked:** `FUN_140168e4`/`FUN_14016828`; `FUN_14016998` (`0x16998`,
  17038-17045) returns the signed byte with default 2; persons carry tribe id
  `+0x150c`; consumers 18551, 18564, 19275, 19370, 19669, 38961; corpus
  `{0:1257, 1:6096, 2:1519260, 3:6552, 4:3}`, diagonal 3 for all 234.
- **Next:** decompile the full branch bodies at the six consumer sites;
  read-only runtime observation of existing saves is permitted (see
  §`BT_TRIBE` — faction relations and §Live diff decision), while any
  controlled value experiment that writes game memory is out of scope.
- **Impact:** high for P2-15b faction AI; the matrix can be loaded opaquely
  first.

### OQ-8 `BT_DIARY` 0x58 record fields and sparse section

- **Question:** what are the 0x50 header bytes after the entry key, the 0x58
  record payloads, and the second (sparse) section?
- **Checked:** `FUN_14058768` 60171-60226 / `FUN_140588c8` 60232-60358; entry
  key `+0x00` and count `+0x04 == n` verified on all 1013 entries; slot0 has
  76 entries and `sparse_count = 0`; 28/28 sparse counts are 0.
- **Next:** decompile `FUN_14058b1c` (server RVA `0x58b1c`, definition
  60364) and `FUN_14058d68` (`0x58d68`, definition 60463) plus the 0x58-record
  accessors; map slot0's 76 entries field by field.
- **Impact:** low for a playable P2-15b (journal UI); required for full
  P2-15b/P2-15c.

### OQ-9 `BT_HSTRY` host vtable, slot ids and `head`/`tail`

- **Question:** do `head`/`tail` index the 40-slot ring, in which order are
  slots serialized, and what is the `u32 id` that ends every slot?
- **Checked:** writer 35403-35404 (`+0x28` object vtable `+4`), reader
  35685-35686 (slot `+0`); the host object comes from `DAT_140891bc+0xcc`, set
  in `GetServerAPI` (`0x5cbf0`, 63133-63146) by `GoldenLand.exe`; body schema
  verified 28/28 (40 slots, `head` 1-37, `tail` 1 in 26 saves / 2 in the
  2-record saves), and each slot ends with a `u32 id` whose value is **0**
  for all 1120 serialized ids across the 28 saves.
- **Next:** decompile `GoldenLand.exe` via the host `+0xcc` getter, follow the
  returned object, and decompile the object at field `+0x28` vtable slots
  `+0`/`+4` (writer/reader) to confirm `head`/`tail` ordering and the meaning
  and use of the trailing id; if the static result is inconclusive, the
  fallback is a read-only runtime read of the 40-slot ring while a save is
  loaded (user-assisted; see §Live diff decision).
- **Impact:** low (UI log); P2-15c needs correct `head`/`tail` and id when
  writing.

### OQ-10 `BT_HERO ` nested fields

- **Question:** what do the header at `+0x000`, the 0x11c state, the 0x600
  buffer (`+0x134`), the `FUN_1401d84c` tail (0x80/extra records) and the
  attached-entity list mean?
- **Checked:** writer `FUN_1401e690` 21880-21899 / `FUN_1401e340`
  21743-21774; reader `FUN_1401e424` 21780-21874; the port already parses
  stats `+0x00C`, three inventories and equipment (`hero_equipment.cpp`,
  `hero_stats.cpp`); slot0 starts `123, 131, 1`.
- **Next:** decompile `FUN_1401d84c` (server RVA `0x1d84c`, 110 bytes) and
  `FUN_1401d8bc` (`0x1d8bc`, 222 bytes) plus person vtable slots `+0`/`+4`.
- **Impact:** medium-high for P2-15b (hero attachments/state beyond
  equipment).

### OQ-11 `BT_RGINF` semantics

- **Question:** what is the meaning of the 36-byte tail-snapshot body
  (`u32` + 0x20 bytes)?
- **Checked:** writer `FUN_14032c2c` (`0x32c2c`) 36040-36042; reader
  `FUN_140325fc` (`0x325fc`) 35812-35815; slot0 tail sections contain it
  (`L1_1` 302+36+22601, `L93_4` 36+98+36+8381).
- **Next:** decompile `FUN_14032c2c` 36040-36042 and `FUN_140325fc`
  35812-35815, then trace consumers of `this+0x26c0` and `this+0x26a0`.
- **Impact:** medium for P2-15b region/world restore.

### OQ-12 `BT_ENTS ` entity types

- **Question:** what is the class map for `FUN_1405c83c(type)`, and what does
  the `type == 1` terminator mean?
- **Checked:** `FUN_1405c83c` (server RVA `0x5c83c`, 409 bytes, 7 callers);
  reader entity loop 35848-35914 (type `u32`, then `FUN_1405c83c(type)` and a
  vtable `+4` read); slot0 tail has per-level `BT_ENTS` sections.
- **Next:** decompile the entity factory `FUN_1405c83c` and each class's
  vtable `+0`/`+4`; diff entity-type sequences across the 28 saves.
- **Impact:** high for P2-15b dynamic entity population.

## Live diff decision

Task 6 result (2026-11-12): **live diff not needed** for P2-15a.

Rationale:

- The container and the framing of all seven blocks are confirmed; the
  `BT_SCRN ` layout (`750008 = 8 + 500*375*4`), the
  `BT_VARS`/`BT_TRIBE`/`BT_DIARY` layouts, the `BT_INFO ` field order
  (35628-35662) and the `BT_HSTRY` 40-slot body are already confirmed from
  static code plus 28/28 corpus scans. `BT_HERO ` equipment, inventories and
  stats are verified; its remaining nested fields are OQ-10. None of the
  confirmed pieces is ambiguous after Tasks 1-5.
- Every remaining item (OQ-1…OQ-12) is a semantics question whose answer sits
  in named functions/RVAs available for static decompilation; each "next
  check" is a decompile step, not a memory comparison.
- The two host-side items (OQ-2, OQ-9) live in `GoldenLand.exe`, which is
  present and identified (SHA-256 `7854019E…`); they can be resolved by
  decompiling the host executable, so a runtime diff is not the first resort.
- A read-only diff can only compare bytes at runtime; it cannot label the
  `BT_TRIBE` relation values (OQ-7) nor convert f64 to typed VM storage
  (OQ-5), and a controlled value experiment for OQ-7 would require writing
  game memory, which this task prohibits.

Revisit trigger: perform the read-only live diff only if the static
decompile of the functions named in OQ-1…OQ-12 is inconclusive (missing or
inlined code, host methods absent). Exact procedure, if triggered:

1. User launches the original `GoldenLand.exe` and loads `Saves/slot0.sav`
   (header name `"Quick save"`, `BT_INFO ` level `L93_4`). Nothing is
   modified in-game.
2. Read-only attach: `OpenProcess(PROCESS_VM_READ |
   PROCESS_QUERY_INFORMATION)` + `ReadProcessMemory` (a `%TEMP%` Python
   `ctypes` script, not committed), or an external debugger in read-only
   mode. No `WriteProcessMemory`, no breakpoint/memory writes.
3. Record per capture: save slot, level, process (`GoldenLand.exe`), module
   and module base (`GoldenLand.exe`/`Server.dll`/`Client.dll` via
   `EnumProcessModules`), absolute address = module base + RVA, memory
   region/length, and the comparison result as `0/N` differing bytes.
4. Targets:
   - `BT_SCRN ` (OQ-1): compare like with like. The inflated block is a
     `u32`-cell buffer whose 8-byte prefix is `f4 01 00 00 77 01 00 00`
     (500, 375); the client `FUN_12022b98` converts each cell to `u16` and
     writes it to a locked destination surface, so the `u32` prefix will not
     appear in that surface. Locate the inflated 750008-byte `u32` block by
     its prefix during a load and compare the next 750000 bytes to the
     decoded slot0 `BT_SCRN ` body; separately resolve the locked
     destination surface via the lock path at 27217 / `FUN_120223b8` and
     record its owning object and lifetime. Do not search the `u16` surface
     for the `u32` prefix.
   - `BT_HSTRY` (OQ-9): resolve the host API pointer at `Server.dll` module
     base + RVA `0x891bc` (`DAT_140891bc`), follow the object returned by its
     `+0xcc` method, then read the object at field `+0x28` and compare its
     40-slot ring to the decoded slot0 `BT_HSTRY` body.
   - `BT_VARS ` (OQ-5): locate `DAT_1407f9a0` (`Server.dll` base + RVA
     `0x7f9a0`) and inspect a variable's in-memory storage after load against
     the saved f64 bytes.
5. Record the `0/N` result here; do not change game state or process memory.
