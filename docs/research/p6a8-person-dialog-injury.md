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
