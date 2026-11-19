# P2-15b4c1 — DIARY record payload research (plan)

Spec: `docs/specs/2026-11-19-p2-15b4c1-diary-payload-design.md`
Status: implemented (2026-11-19); research-only.

## Task 1 — Constructor field trace

1. `FUN_14058de0`: map `+0x00..+0x15` sources (`uStack_78/7c/70`,
   `DAT_14088e0c+0x2c` ×2, `DAT_14088e14+0x18`).
2. `FUN_14059030`: map `param_2`, the stack pointer at `+0x04`, and the
   `DAT_14088e0c+0x2c` query sources.
3. Record the pointer/stack-pointer evidence for non-portability.

## Task 2 — Consumer reads

1. `FUN_140591b8` (flattened header path) and `FUN_1405a46c`: list which
   header/record offsets are read; separate header-only from record payload.
2. Identify whether `+0x54` is dereferenced (D-091: not by 591b8).

## Task 3 — Verdict and next checks

1. Write `docs/research/p2-15b4c1-diary-payload.md` with a
   proven/inferred/blocked table and exact next checks
   (`DAT_14088e0c` allocation/init, vtable `+0x2c`, host object class, or
   user-assisted live read).
2. Verdict `blocked (scoped)` unless the host object is resolved.
3. Update `STATUS`, `PROJECT`, `DECISIONS` (D-092); reviewer.
