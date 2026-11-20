# P6a2 — Time-event natives research (design)

Status: implemented (2026-11-19)
Scope: `docs/`; no code.

## 1. Questions

1. What do `RS_SetEvent`/`RS_GetEvent`/`RS_ClearEvent` (0x7000005..7)
   store and return?
2. Is the one-int argument an event id or a handle?
3. What is missing for a faithful `TimeEventRuntime` on `GameCalendar`?

## 2. Method

Disassemble the thunks `0x14040e2c..0x14040f34`; decompile
`FUN_1405b9b4/5b80c/5b6c4`; map the clock list entry and returns.

## 3. Deliverable

`docs/research/p6a2-time-events.md` with proven/blocked fields and exact
next checks; verdict `blocked (scoped)` unless the clock due computation is
recovered.

## 4. Acceptance

- Thunk args/returns and list entry mapped or explicitly blocked.
- Next checks named; docs-only; UTF-8 no BOM/NUL.
