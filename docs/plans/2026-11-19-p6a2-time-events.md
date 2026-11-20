# P6a2 — Time-event natives research (plan)

Spec: `docs/specs/2026-11-19-p6a2-time-events-design.md`
Status: implemented (2026-11-19); research-only.

## Task 1 — Thunks

1. Disassemble `0x40e2c`/`0x40ec8`/`0x40f04`; map args, returns, clock
   object calls.

## Task 2 — List helpers

1. Decompile `FUN_1405b9b4/5b80c/5b6c4`; map the 0x24-byte entry, handle
   counter, due predicate and removal return.

## Task 3 — Verdict and next checks

1. Write `docs/research/p6a2-time-events.md`; `STATUS`, `DECISIONS` (D-095).
2. Verdict `blocked (scoped)`; next: clock vtable `+0` or live capture.
