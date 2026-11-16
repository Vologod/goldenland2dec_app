# P2-15b4a — BT_DIARY / BT_HSTRY Research Plan

> Research-only (docs-only). No production code.

**Goal:** Establish what `BT_DIARY`/`BT_HSTRY` framing and semantics are
proven by static decompilation and 28-save scans, and decompose the
implementation.

**Spec:** `docs/specs/2026-11-16-p2-15b4a-diary-hstry-research-design.md`.
**Result:** `docs/research/p2-15b4-diary-hstry.md`.

- [x] **Task 1: DIARY container decompile** — `FUN_14058768` (writer),
  `FUN_140588c8` (reader), `FUN_14058b1c` (flatten), `FUN_14058d68`
  (free), `FUN_14058de0`/`FUN_14059030` (record creation). Confirmed
  `u32 entry_count`, per-entry `u32 block_count = records+1` and
  `block_count*0x58` raw bytes, sparse section, flattened header `+0x04=-1`.
- [x] **Task 2: 0x58 record and corpus** — 22 dwords copied verbatim;
  corpus 1,013 entries / 4,855 blocks / 3,842 records / sparse 0;
  pointer-like `+0x54` values in 4,088/4,855 blocks; journal consumers
  `FUN_140591b8`/`FUN_14059f6c` use `+0x0c/+0x54/+0x5c/+0x60/+0x64`, labels
  unverified.
- [x] **Task 3: HSTRY schema/host** — body `head + 40*(len,text,id) + tail`
  exact 28/28; head 1..37, tail 1/2, ids all 0; host methods in
  `GoldenLand.exe` (`DAT_140891bc+0xcc`/`+0x28`), blocked for static
  Server-side research.
- [x] **Task 4: verdict/decomposition/docs** — verdict `scoped`; b4b
  framing parser, b4c record semantics, b4d host ring; D-077; STATUS/
  PROJECT/parity updates.

**Verification:** docs-only; `git diff --stat` touches docs only; UTF-8
no BOM/NUL; no `src/`/`tests/`/`tools/` changes.
