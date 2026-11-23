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

## Entity party flag `+0x5a8` (2026-11-21)

- `FUN_14016fcc` (RVA `0x16fcc`; AddToParty) checks the 4-slot party,
  resolves the target person, appends its id and sets `person+0x5a8 = 1`,
  `person+0xf28 = hero id`, `+0x5c0 = 0`, `+0x44 = 2` and copies the name
  into `+0x40`.
- `FUN_14017134` (RVA `0x17134`; Remove) returns 4 without touching the
  person when the party is empty. Otherwise it sets `person+0x5a8 = 2` and
  `person+0xf28 = -1` before removing the first matching slot; a known
  person that was not listed therefore still gets role 2.
- `FUN_14016b94` (RVA `0x16b94`) and other consumers treat `0` as
  outside the party, `1` as an active member and `2` as removed.
  The hero is not specially excluded by Add/Remove; the built-in `"Hero"`
  PersonState is just a normal known person in port tests. Tooling:
  `llvm-objdump`/Ghidra decompiled Server.dll (SHA-256 `418E2560…CC837`).
- Port models the role only: `PersonState.party_role` (0/1/2),
  `PersonRuntime::set_party_role/party_role`, `PartyRuntime` updates it
  when a `PersonRuntime` is attached. `+0xf28` (hero id), `+0x44`,
  `+0x5c0`, name copy, AI/follow, death cleanup and save/load are not
  modelled.

## Deviations

- Type failure returns 0.0, unknown person returns -1.0 (port/gate contract).
- Entity `+0x5a8` role is modelled; hero id `+0xf28`, AI/follow and
  save/load are not.
