# P2-15b4c1 — DIARY record payload research

Status: complete (2026-11-19)
Phase: P2-15b4c1 (research-only; follow-up to D-091)
Scope: `docs/`; no production code.

## 1. Provenance

| Artifact | SHA-256 |
|---|---|
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| 28-save manifest | `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |

## 2. Constructor split

D-077/D-091 grouped four constructors together. The decompiled bodies show
two roles:

- **Header builders:** `FUN_14059f6c` (0x59f6c) and `FUN_1405a21c` (0x5a21c)
  fill `+0x14..+0x30` from the calendar (`DAT_14088e14+0x18`), zero
  `+0x34..+0x50`, and set the header flags/id fields. This matches D-091's
  header map.
- **Record builders:** `FUN_14058de0` (0x58de0) and `FUN_14059030` (0x59030)
  take two host handles, call `DAT_14088e0c+0x2c` for both, and fill the
  0x58 block from the returned host data; `FUN_14059030` writes a **stack
  pointer** to `+0x04` (`piVar9[1] = (int)ppuStack_74`), and `FUN_14058de0`
  writes calendar-derived data at the high end of the record.

Consequence: record fields are a **host-object snapshot**, not a
self-describing save structure.

## 3. Field sources (record block)

| offset | source / observation | status |
|---|---|---|
| `+0x00` | entry key (`param_2` in `FUN_14059030`; first host query in `FUN_14058de0`) | proven |
| `+0x04` | host data; `FUN_14059030` writes a **stack pointer**; corpus values are small ids | blocked (nonportable) |
| `+0x08..+0x50` | `DAT_14088e0c+0x2c` host data and calendar-derived fields; layout differs between the two constructors | blocked |
| `+0x54` | runtime `0x1400xxxx` pointer in all 3,842 records (D-091) | proven pointer, blocked meaning |

Corpus does not expose a stable datetime in record blocks: only 1/3,842
record blocks has a valid 8-word datetime (D-091), so the constructor's
calendar stores are not a per-record timestamp at a fixed offset.

## 4. Consumer reads

`FUN_140591b8` operates on the **flattened** copy (`+0x04 = -1`), reads the
header key, `+0x0c` and `+0x14..+0x30`; it does not read `+0x34..+0x50` and
does not dereference `+0x54` (D-091). `FUN_1405a46c` is a different object
(pointer/vtable lifecycle at `param_1[1..6]`), not the record decoder.
The record payload has no static reader that yields named fields.

## 5. Verdict

**`blocked (scoped)`.**

- Record blocks are a serialized host-object snapshot with stack pointers
  (`+0x04`) and process pointers (`+0x54`); the static Server code does not
  provide field labels or a portable inverse.
- Header blocks remain fully usable (D-091); raw record pass-through is the
  only safe write behavior.
- New diary records cannot be generated from live state until the host
  journal object is mapped.

## 6. Exact next checks

1. **Host journal object:** find the allocation/init of `DAT_14088e0c` and
   its class vtable; decompile vtable `+0x2c` (the query used by both record
   builders) and the matching `DAT_14088e08+0x2c`.
2. **Host class layout:** map the returned object's fields to the 22 written
   dwords, especially `+0x04`/`+0x54` pointers and the calendar-derived high
   end.
3. **Live fallback:** user-assisted read-only: capture one journal entry
   before/after a quest event and diff the 0x58 words; if static mapping is
   insufficient, this is the only way to label the record payload.
4. **Then:** `b4c1a` host journal object map, `b4c1b` record field
   classifier, `c3d` new-record write.

## 7. Non-claims

- `+0x04..+0x50` labels and pointer lifetime.
- Whether the stack pointer at `+0x04` is ever serialized (corpus has small
  values, but the constructor writes a stack address).
- New diary record creation, journal UI/quest logic, HSTRY (b4d).

## 8. Reproduce

```powershell
& .local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin\llvm-objdump.exe -d `
  --start-address=0x14058de0 --stop-address=0x14059030 "I:\Games\Златогорье-2\Server.dll"
& .local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin\llvm-objdump.exe -d `
  --start-address=0x14059030 --stop-address=0x140591b8 "I:\Games\Златогорье-2\Server.dll"
```
