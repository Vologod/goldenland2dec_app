# P2-15b4c1 — DIARY record payload research (design)

Status: implemented (2026-11-19)
Task: `P2-15b4c1` (research-only; follow-up to D-091)
Scope: `docs/research/`; no `src/` changes.

## 1. Context

D-091 mapped the DIARY header block but left the record payload
(`+0x04..+0x50`, `+0x54`) blocked. D-077 named the record constructors
`FUN_14058de0`/`FUN_14059030` and consumers `FUN_140591b8`/`FUN_1405a46c`.
This task maps what the constructors/consumers prove and fixes the next
checks.

## 2. Questions

1. What sources fill record fields `+0x00..+0x15` in each constructor?
2. Which fields are ids, host-object snapshots, stack pointers or raw
   runtime pointers?
3. Which consumer offsets are actually read, and what remains blocked?
4. What is the exact next check to resolve the payload (host journal class,
   vtable slot, live capture)?

## 3. Method

- Static: `FUN_14058de0` (0x58de0), `FUN_14059030` (0x59030), consumer
  `FUN_1405a46c` (0x5a46c) and `FUN_140591b8` (0x591b8); trace
  `DAT_14088e0c+0x2c`, `DAT_14088e08+0x2c`, `DAT_14088e14+0x18` calls.
- Corpus: cross-check record `+0x00`/`+0x04`/`+0x54` against headers and
  pointer ranges (D-091 corpus).
- No live capture required for the verdict; a live read is listed as the
  fallback.

## 4. Deliverable

`docs/research/p2-15b4c1-diary-payload.md` with the constructor field map,
consumer offset reads, proven/inferred/blocked status and exact next checks;
verdict `blocked (scoped)` unless the host object can be resolved statically.

## 5. Acceptance

- Every constructor field has a source or an explicit blocked status.
- Blocked fields name the exact host/vtable/consumer next check.
- No code/test changes; docs UTF-8 no BOM/NUL.
