# P6a10 - Hero party runtime research

Status: complete (2026-11-19)
Scope: docs; Server.dll SHA-256 418E2560...CC837.

## Evidence

- RS_AddToHeroPartyName (0x4000007, 0x41d28): type gate string only; default
  return 0.0 (_DAT_14091540) on bad type. Resolves the person with
  FUN_1402914c; if null returns -1.0 (_DAT_14091538). Otherwise calls
  FUN_14016fcc(person, party list at +0xf34/+0xf44/+0xf48), which appends
  even when the name is already present, returns 4 - size, or -1 when full.
- RS_RemoveFromHeroPartyName (0x4000008, 0x41e24): default 0.0 on bad type,
  -1.0 on unknown person, 4 when the party is empty; FUN_14017134 removes
  the first matching slot and returns 4 - size.
- RS_TestHeroHasPartyName (0x4000009, 0x41f20): default 0.0; FUN_14016c88
  returns membership as 1.0/0.0.
- Party list is four person indices; the port stores script names and gates
  adds/removes on PersonRuntime::has_person, preserving exact-name behaviour.

## Deviations

- Type failure returns 0.0, unknown person returns -1.0 (port/gate contract).
- Entity +0x5a8 party flags, AI/follow and save/load are not modelled.
