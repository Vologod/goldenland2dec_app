# P2-15b4 — BT_DIARY / BT_HSTRY research

Phase: P2-15b4a (research-only; docs-only).
Verdict: **`scoped`** — framing parser implementable now; 0x58 record
semantics partially blocked; HSTRY ring semantics blocked on host.

Sources: `Server.dll` SHA-256
`418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`;
`.local/decomp/server/decompiled.c` (same corpus as prior P2-15 research);
28 `Saves/slot*.sav` read-only; temporary Python scripts (not committed):
`diary_scan.py` `28A00141DF7E64A5E765A5325FDE4C02B878934A34203848113AF4450568F7AC`,
`diary_dump.py` `17334B4D41BE2F424F00AC89AB2B5FC8D92E24BD2EE50789816FD11F994F8825`,
`diary_hstry_scan.py` `C510466575A8A757201064BABCF36908D79D86E7DECD233E77967B5BADDF7598`.

## 1. DIARY container

- Writer `FUN_14058768` (RVA `0x58768`, size 350, decompiled 60171-60226):
  - `u32 count` of map entries (`FUN_14004b00(this+4)` returns the count);
  - per entry: `u32 block_count`, then `block_count * 0x58` raw bytes. The
    writer passes `*(int *)(entry + 4) * 0x58 + 0x58` at 60193, i.e.
    `block_count = record_count + 1`;
  - after the map: a sparse section `u32 sparse_count` + `sparse_count *
    0x58` raw records (60197-60226, bitmap walk).
- Reader `FUN_140588c8` (RVA `0x588c8`, size 594, decompiled 60232-60358)
  mirrors it; the flattened entry header's `+0x04` is replaced with
  `0xffffffff` (60288-60297), so `+0x04` is loader state, not the on-disk
  count, after load.
- Flattening `FUN_14058b1c` (RVA `0x58b1c`, size 587, decompiled 60364-
  60457) walks the same map, copies `1 + *(+0x04)` 0x58 records per entry,
  sets the output header `+0x04 = -1` (60418), and returns the total 0x58
  block count. `FUN_14058d68` (RVA `0x58d68`) frees the container
  (called at 37421).
- Record construction `FUN_14058de0` (RVA `0x58de0`, size 589, decompiled
  60504-60608) and `FUN_14059030` (RVA `0x59030`, size 392) resolve two
  references through `DAT_14088e0c` vtable `+0x2c` and write one 0x58
  record; the record payload is copied from a caller-built stack struct
  (22 dwords at 60582-60603). This confirms the record is an in-memory
  structure copied verbatim to save, not a serialized field-by-field
  format.
- `FUN_140591b8` (RVA `0x591b8`, size 3505) and `FUN_14059f6c` (RVA
  `0x59f6c`, size 687) are the large diary/journal consumers; record
  accesses use `base + index*0x58` with offsets `+0x0c`, `+0x54`, `+0x5c`,
  `+0x60`, `+0x64` (decompiled 60858-61837). Their full semantics were not
  mapped in this task.

### Corpus

28 saves: **1,013 entries**, **4,855 0x58 blocks** = 1,013 entry headers
+ 3,842 record blocks, **sparse_count = 0** in all saves; every body
consumes exactly. Selected sections: slot0 `76 entries / 371 blocks`,
slot2 `75 / 377`, slot21 `1 / 2`. First slot0 entry: `key=0`, count `4`,
then 4 raw records; slot21: `key=482`, count `1`, record dwords
`482, 483, 0, -1, ...`.

### Record (0x58 bytes / 22 dwords)

Only structural facts are confirmed:

- `+0x00` header key, `+0x04` record count for the entry header; records
  themselves are opaque 22-dword copies.
- Values are **not portable**: `+0x54` has a non-zero high byte in
  4,088/4,855 blocks; e.g. slot0 record tails contain `0x14004F33`,
  `0x0019E7AC`, `0x0019E814` (code/heap-like addresses). Per-save field
  distributions show near-constant dwords 17/18 (e.g. `0x19E6EC`,
  `0x19E814`) and highly variable others; no stable field labels can be
  claimed from the corpus alone.
- `+0x0c/+0x54/+0x5c/+0x60/+0x64` are used by the journal consumers; their
  meaning (quest id, state, dates, text buffer) remains `unverified`.

**DIARY verdict:** framing is implementable now; field semantics are
`partial/blocked` — do not map 0x58 fields without accessor analysis.

## 2. HSTRY container

- Body schema verified on all 28 saves: `u32 head`, then 40 slots of
  `u32 text_len + cp1251 text + u32 id`, then `u32 tail`; every body
  consumes exactly (`q == len(body)`).
- Corpus: 40 slots in 28/28; `head` range 1..37; `tail = 1` in 26 saves,
  `2` in the 2-record saves; all **1,120** slot ids are `0`.
- Writer/reader live in the host object: writer calls
  `*(iVar3 + 0x28) + 4` (35403-35404), reader `+0` (35685-35686), where
  `iVar3` comes from `DAT_140891bc+0xcc` and is implemented by
  `GoldenLand.exe` (`GetServerAPI` at `0x5cbf0`, 63133-63146). The two
  method bodies are absent from `Server.dll`'s `decompiled.c`.

**HSTRY verdict:** body parse/round-trip implementable; ring ordering and
`head`/`tail`/`id` semantics are `blocked` on `GoldenLand.exe` decompile
(or read-only live observation).

## 3. Decomposition

- **P2-15b4b — framing parser (implementable now):** `SavePayload` stores
  diary `entries` (key, declared count, raw 0x58 blocks) and `sparse`
  records, plus HSTRY `head/slots(text,id)/tail`; exact consume 28/28;
  expose raw bytes only; no field interpretation; `--dump-save` counts.
  This also gives P2-15c a raw round-trip for both blocks.
- **P2-15b4c — diary record semantics (research):** decompile
  `FUN_140591b8`/`FUN_14059f6c` and the `0x58` accessors, map
  `+0x0c/+0x54/+0x5c/+0x60/+0x64`, decide whether pointer-like words are
  remapped on load or discarded; only then build a quest/journal model.
- **P2-15b4d — HSTRY ring (research, host):** decompile `GoldenLand.exe`
  host object `DAT_140891bc+0xcc` / `+0x28` slots `+0`/`+4`; resolve
  `head`/`tail` ordering and the `id=0` value; alternatively read-only live
  ring comparison.

## 4. Acceptance result

1. DIARY framing confirmed 28/28 (entries/blocks/sparse exact).
2. 0x58 record: structural header confirmed; field labels marked
   `unverified`; pointer-like values documented.
3. HSTRY body confirmed 28/28; host methods localized to
   `GoldenLand.exe`, marked `blocked`.
4. Verdict `scoped` and decomposition above.
5. Docs-only; UTF-8 no BOM/NUL; no `src/`/`tests/`/`tools/` changes.

## Implementation result (P2-15b4b, 2026-11-16)

`SavePayload` now stores DIARY entries (`key`, `declared_count`, raw 0x58
header+record blocks), sparse 0x58 records, and HSTRY (`head`, 40
`(text,id)` slots, `tail`). Parsers require exact body consume and reject
truncated/trailing framing. Real 28 saves: 1,013 entries / 4,855 blocks /
3,842 records / sparse 0 / header-count mismatches 0; HSTRY 40 slots,
head 1..37, tail 1×26/2×2, all ids 0. Raw bytes are preserved for
P2-15c; 0x58 field labels, sparse semantics and ring ordering remain
unverified.
