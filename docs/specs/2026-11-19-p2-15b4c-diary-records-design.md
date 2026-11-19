# P2-15b4c — DIARY record semantics research (design)

Status: implemented (2026-11-19)
Task: `P2-15b4c` (research-only; follow-up to D-077/D-078)
Scope: `docs/research/`; no `src/` changes.

## 1. Context

`BT_DIARY` framing is parsed and byte-preserved (`SavePayload.diary`), but
the 0x58 records are opaque. D-077 named the constructors
`FUN_14058de0`/`FUN_14059030`, the flatten helper `FUN_14058b1c` and the
consumers `FUN_140591b8`/`FUN_14059f6c`. This task maps what can be proven.

## 2. Questions

1. Header record (`block 0` of each entry) vs record blocks (`block > 0`):
   how are they distinguished, and what are the fixed fields?
2. Are the two 8-word datetime regions real, and when is the second present?
3. Which fields are ids/flags vs runtime pointers, and what is blocked?
4. What is the minimal decomposition to a journal model and DIARY write?

## 3. Method

- Static: header builders `FUN_14059f6c`/`FUN_1405a21c`, record constructors
  `FUN_14058de0`/`FUN_14059030`, consumers `FUN_140591b8`/`FUN_1405a46c`;
  compare field stores, not a verbatim stack copy.
- Corpus: 28 saves / 1,013 entries / 4,855 blocks; per-block field ranges,
  header-vs-record datetime validity, flag cross-tabs, `+0x54` header/record
  split.
- Script: `.local/research/b4c/diary_scan.py` (read-only).

## 4. Deliverable and acceptance

`docs/research/p2-15b4c-diary-records.md`:

- header field map with offsets/labels (proven/inferred/blocked);
- record-block classification and pointer evidence;
- corpus table (1,013 headers / 3,842 records / datetime validity / flags);
- verdict and decomposition (`b4c1` record payload map, `b4c2` journal
  model, `c3d` datetime/flag write now; new records gated on b4c1);
- no code/test changes; docs UTF-8 no BOM/NUL.
