# P6a10 - Hero party state (design)

Status: approved (2026-11-19)
Scope: game party runtime, script access, glgame, tests; no AI/entity flags.

Evidence: RS_AddToHeroPartyName (0x4000007, string, type default 0.0, unknown -1.0) resolves the person and calls FUN_14016fcc, which appends to the 4-slot party and returns 4-size (or -1 when full). RS_RemoveFromHeroPartyName (0x4000008) returns 4 when empty, removes one matching slot, returns 4-size; unknown person default -1.0. RS_TestHeroHasPartyName (0x4000009) returns 0.0/1.0.

Runtime: PartyRuntime ordered names, max 4; add appends duplicates and returns slots left or -1 when full, remove returns slots left (4 when empty), has returns membership. PartyAccess in NativeServices; glgame checks person_runtime_.has_person for unknown names (returns -1.0/0.0). Party clears on New Game and successful load, restores on failed load (not saved).

Non-goals: entity +0x5a8 party flags, AI/follow, save/load.
