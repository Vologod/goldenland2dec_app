# P2-15b4c — DIARY record semantics research (plan)

Spec: `docs/specs/2026-11-19-p2-15b4c-diary-records-design.md`
Status: implemented (2026-11-19); research-only.

## Task 1 — Static constructor/consumer map

1. Header builders `FUN_14059f6c` (0x59f6c) and `FUN_1405a21c` (0x5a21c),
   record constructors `FUN_14058de0`/`FUN_14059030`: map the 22 dwords and
   field sources.
2. `FUN_140591b8` (0x591b8) and `FUN_1405a46c` (0x5a46c): identify which
   0x58 fields are read and for what.
3. Record only labels that are directly supported; otherwise mark inferred/
   blocked with the exact next check.

## Task 2 — Corpus scan

1. `diary_scan.py`: per save entries/headers/records, header datetime A/B
   validity, flag cross-tabs, keys, +0x10 range, `+0x54` header/record split.
2. Verify: header datetime A 1,013/1,013; datetime B iff +0x08; record
   blocks have no datetime; record `+0x54` 3,842 pointers and header
   `+0x54`/`+0x0c` cross-tab.

## Task 3 — Verdict and decomposition

1. Write `docs/research/p2-15b4c-diary-records.md` with the field map,
   corpus table, verdict (`scoped`/`blocked`) and decomposition.
2. Update `STATUS`, `PROJECT`, `DECISIONS` (D-091); docs hygiene; reviewer.
