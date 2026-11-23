# P6a8 - Person dialog/injury natives research

Status: complete (2026-11-19)
Scope: docs only; Server.dll SHA-256 418E2560...CC837.

## RS_GetDialogEnabled (0x700000E, 0x140410b4)

Type gate: one int; default return 0.0. Flow: current person index (FUN_14027f7c), FUN_14009f08(person,index) -> int, float multiply by 0.01 (0x1409146c), FUN_14006288(probability) -> nonzero, then return 0x14091468 (1.0), otherwise 0.0. It is a person dialog-state percentage gated by the shared RNG, not a pure getter. Blocked on FUN_14009f08 state layout and RNG/probability contract; marking it supported would be false parity.

## RS_SetInjured (0x7000013, 0x14041c34)

Type gate string+int; resolves the person by name and calls FUN_14018848(person,arg). FUN_14018848 manipulates a list at +0x14f4 (count), +0x14f8 (capacity), entries of 0x14 bytes at +0x110c copied from DAT_140790e8.. and does not store a simple bool. It is an injury/limb animation state, not a flag. Blocked on the entry semantics and the consumer/animation contract. Keep 0x7000013 unsupported.

## Next checks

1. Map FUN_14009f08 return units and FUN_14006288 probability/RNG contract; add a real dialog test.
2. Map FUN_14018848 entry fields and their animation/limb consumers; then a time-aware injury runtime.
3. Do not mark either native supported until a real script path is asserted.

## Resolution (2026-11-23)

- `RS_GetDialogEnabled` handler `0x410b4`: type gate `FUN_14038c6c(0, -1)`
  (one numeric argument, ignored), `FUN_14027f7c` returns the first valid
  controlled-party id (`world+0x1cf4/+0x1cf8/+0x1cfc`; the same list is
  averaged by `FUN_140254bc`), `FUN_14009f08` uses
  `SKILL_TALKING` (`0x15`) and effective INT, `+20` for `SKILL_TRADE`
  (`0x10`) > 9, then `FUN_14006288((float)percent * 0.01f)`.
- Constants: `DAT_1408d7a0` 0.5f, `DAT_1408d7a4` -0.5f,
  `DAT_1408d7a8` 0.0f, `DAT_1408d7c4` 1/32767f (`0x38000100`),
  `DAT_1408d7c8` 1.0f, `DAT_1408d7cc` 0.0f, `DAT_1409146c` 0.01f,
  `DAT_14091468` 1.0f, `DAT_14091460/70` 0.0f.
- `RS_SetInjured` handler `0x41c34`: type gate `(3, 0, -1)`,
  `FUN_1402914c`, `FUN_14018848`. The fixed key `DAT_140790e8` is
  `16 00 00 00 cf ff ff ff 81 00 00 00 00 00 00 00 ff ff ff ff`
  (dwords `{22, -49, 129, 0, -1}`); the function removes all matching
  records and appends the key when the argument is non-zero.
- Usage re-scanned with `glpak extract` over `scripts.pak` +
  `scripts.update.3.pak`: `RS_GetDialogEnabled` 100 calls / 34 files
  (99 plain + 1 legacy-XOR `l2.p588_kor_aferist.d588`),
  `RS_SetInjured` 8 calls / 2 files; args `"Hero"` + 0/1; the
  `RS_GetDialogEnabled` int argument is 2 in sampled sites.
- Port decision: `FUN_14007fb0` identity, `manager+0xC` gate 0 and
  `gv_addon` 0 follow D-050; hero is the single-player controlled
  party member. Consumers, save interpretation and other effect
  producers remain out of scope.
