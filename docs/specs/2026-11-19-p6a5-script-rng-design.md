# P6a5 - Script RNG native (design)

Status: implemented (2026-11-19)
Scope: src/script, tools/glgame, tests; no seeding/save changes.

Evidence: RS_GetRandMinMaxI (0x700000A) type-checks two ints, calls FUN_140061e4(low,high), accepts numeric args (VM literals are doubles) and returns the int as double; FUN_140061e4 returns low when high<=low, else low + trunc(((high-low)+0.99999)*rand/32767), already implemented as MsvcRandom::range (D-053).

Runtime/wiring: RandomAccess.range in NativeServices; glgame binds it to LevelSimulation::rng() so scripts share the level/NPC stream, with a --seed/production-seeded fallback outside a level.

Acceptance: native matches a seeded MsvcRandom, inverted range, type/access failures, arity; screenshot and full ctest.

Non-goals: separate seed control, stream persistence, other RNG natives.
