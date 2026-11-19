# P2-15b4c — DIARY record semantics research

Status: complete (2026-11-19)
Phase: P2-15b4c (research-only; follow-up to D-077/D-078)
Scope: `docs/`; script in `.local/research/b4c/` (not committed).

## 1. Provenance

| Artifact | SHA-256 |
|---|---|
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| `.local/research/b4c/diary_scan.py` | `64D2EC552E62D73CC6A1ED99754C16BA3AA47C3BD6465DB711A1D4FE76A32D71` |
| 28-save manifest | `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |

## 2. Header vs record blocks

Each entry is `u32 block_count` followed by `block_count` × 0x58 bytes. The
first block is the **header**; the rest are **records**. Evidence:

- Header datetimes: 1,013/1,013 headers have a valid 8-word datetime A;
  only 1/3,842 record blocks does (a coincidental payload).
- Header layout (`+0x00..+0x54`) is stable; record payload is not.

## 3. Header block map

| offset | words | label | status | evidence |
|---|---:|---|---|---|
| `+0x00` | 1 | entry key | proven | corpus 0..550; repeated in every record of the entry |
| `+0x04` | 1 | record count (`block_count-1`) | proven | header value equals `block_count-1`, 0 mismatches / 1,013; `FUN_14058768` uses it |
| `+0x08` | 1 | end-time flag | inferred | `1` ⇔ datetime B valid, 1,013/1,013 cross-tab |
| `+0x0c` | 1 | status/type flag | inferred | 0:767 / 1:246; exactly tied to `+0x54 == -1` |
| `+0x10` | 1 | narrative/text id | inferred | range 180..510, 102 distinct; repeats per entry |
| `+0x14..+0x30` | 8 | datetime A | proven | 8-word layout (D-088), valid 1,013/1,013 |
| `+0x34..+0x50` | 8 | datetime B | proven layout, inferred meaning | valid 797/1,013; all-zero when `+0x08 == 0` (216/216) |
| `+0x54` | 1 | linked id/state | blocked | 0..15 (767) or `-1` (246); `-1` ⇔ `+0x0c == 1`, 1,013/1,013 |

Datetime A/B use the same layout as `BT_INFO`/`BT_RGINF` (D-088): year,
month, day, weekday, hour, minute, second, millisecond (1-based month/day in
the save).

**Flattened header is a different object.** `FUN_14058b1c` (the UI flatten
path) sets the output header `+0x04 = -1`; `FUN_140591b8` filters on that
value. The **persisted** header written by `FUN_14058768` uses
`+0x04 = block_count-1`. Do not emit `-1` into a save header.

## 4. Record blocks

| offset | label | status | evidence |
|---|---|---|---|
| `+0x00` | entry key | proven | equals header key, 3,842/3,842 |
| `+0x04` | record id/key | blocked | varies per entry; constructors `FUN_14058de0`/`FUN_14059030` copy a caller-built struct |
| `+0x08..+0x10` | ids/flags | blocked | small values, varies |
| `+0x14..+0x50` | opaque runtime payload | blocked | no datetime structure (1/3,842 coincidental) |
| `+0x54` | runtime pointer | proven pointer-like | all 3,842 record blocks are `0x1400xxxx` process addresses, not portable |

Static evidence: `FUN_14059f6c`/`FUN_1405a21c` build a header record, fill
`+0x14..+0x30` from `DAT_14088e14+0x18` (current calendar time), zero
`+0x34..+0x50` and set the type/flags; they are **not** a verbatim 22-dword
copy. `FUN_14058de0`/`FUN_14059030` copy a caller-built record struct.
`FUN_140591b8` reads `+0x00`, requires flattened `+0x04 == -1`, reads
`+0x0c`, `+0x14..+0x30` and `+0x54`; it does **not** read `+0x34..+0x50`
and does not dereference `+0x54`. Record payload labels therefore remain
blocked on `FUN_1405a46c` and the record constructors/accessors.

## 5. Corpus scan

`diary_scan.py`: 28 saves, 1,013 entries.

| metric | result |
|---|---|
| entries / headers / records | 1,013 / 1,013 / 3,842 |
| header datetime A valid | 1,013 / 1,013 |
| header datetime B valid | 797 / 1,013 |
| `+0x08 == 1` ⇔ B valid | 1,013 / 1,013 |
| `+0x0c` values | 0:767, 1:246 |
| `+0x54` header | 0..15 (767), `-1` (246); `-1` ⇔ `+0x0c == 1` |
| key range | 0..550 |
| `+0x10` range | 180..510 (102 distinct) |
| record `+0x54` | 3,842/3,842 `0x1400xxxx` pointers |

## 6. Verdict

**`scoped`.** The header block is mapped well enough for a read-only journal
model and for raw pass-through writes: key, record count, two flags, text id
and two datetimes. The record payload remains **blocked**: it is a copied
runtime struct with process pointers at `+0x54` and unresolved ids at
`+0x04..+0x10/+0x14..+0x50`.

Consequences for write-back:

- Preserving DIARY raw and updating only datetime A/B (`+0x14..+0x50`) and
  the `+0x08` flag is possible; when `+0x08 == 0`, B must be zero.
- `+0x0c`/`+0x54` and record creation from live journal state remain
  **blocked** until the record payload map is resolved.

## 7. Decomposition

| Task | Scope | Acceptance |
|---|---|---|
| `P2-15b4c1` record payload map | decompile `FUN_1405a46c`, the record constructors `FUN_14058de0`/`FUN_14059030` and accessors; classify `+0x04..+0x10` and the `+0x14..+0x50` payload; resolve `+0x0c`/`+0x54` | field map with record-key/datetime invariants 0/N; blocked fields named |
| `P2-15b4c2` journal model | read-only journal entries from header map (key, flags, text id, datetimes) and raw record payload | journal list from 28 saves; no write |
| `P2-15c3d` DIARY write | raw-preserving datetime A/B + `+0x08` update (B zero when `+0x08 == 0`); `+0x0c`/`+0x54` and new-record write gated on b4c1 | save→parse round-trip; datetime/flag updates 0/N; new records only if b4c1 resolved |

## 8. Non-claims

- `+0x08`/`+0x0c`/`+0x10` meanings are inferred (datetime B presence, status,
  text id target); only the value correlations are proven.
- Record payload semantics, `+0x0c`/`+0x54` meaning and pointer lifetime are
  blocked.
- Journal UI/quest logic, HSTRY ring (b4d), and DIARY write are out of scope.

## 9. Reproduce

```powershell
py -3 .local\research\b4c\diary_scan.py
Get-FileHash -Algorithm SHA256 .local\research\b4c\diary_scan.py
```
