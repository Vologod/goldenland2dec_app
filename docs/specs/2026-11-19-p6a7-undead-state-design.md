# P6a7 - Undead state native (design)

Status: approved (2026-11-19)
Scope: src/game person runtime, src/script, tests; no AI/rendering.

Evidence: RS_SetUndeadState (0x140415e0, args string+int, returns 0.0) resolves the person by name and writes entity +0xf58 = (arg != 0). Unknown person is a no-op.

Runtime: PersonState gains an undead bool; PersonRuntime exposes set_undead(name,bool)/undead(name); PersonAccess gains set_undead. Native 0x700000F becomes supported (28/65), string+numeric, returns 0.0; glgame already wires person_runtime_.access().

Acceptance: runtime flag per known person, unknown no-op, native dispatch/0.0/type, arity; screenshot and full ctest.

Non-goals: undead AI/rendering, +0xf58 exact byte layout, save/load.
