# P6a4 — Location access research

Status: complete (2026-11-19; unblocked 2026-11-21)
Scope: `docs/`; no code.

**Unblock (2026-11-21, P6a12/D-108):** `DAT_1407f9a0` is the
script-global table (`FUN_14038958`/`FUN_14038904` use it during script
execution). The port maps it to `ScriptVm::globals_` through
`GlobalAccess`; `RS_SetLocationAccess` is now supported (36/65). See
`p6a12-location-access-unblock.md`.

## 1. Evidence

`RS_SetLocationAccess` (`0x14041634`, SHA-256
`418E2560…CC837`, args `[3,0]`, returns `0.0`):
type-checks string+int, copies the name, appends the literal `_state`
(`0x1408fbcc` in `.data1`), looks it up in the global object `DAT_1407f9a0`
(vtable `+0x14`) and, only when the result is not `-1`, writes the argument
via vtable `+0x1c`. The value is converted with `fildl` / unsigned fixup
(`0x140914f8/0x140914fc`) and stored as a **double** in the global table.
Unknown names are no-ops; the native returns `0.0` on every path.

## 2. Why it was blocked (historical)

- The port has no object/table equivalent to `DAT_1407f9a0`, and no mapping
  of its keys (`name + "_state"`) to `BT_VARS` globals.
- `NativeServices` is independent of `ScriptVm::globals_`; a private map
  would not be visible to scripts, so marking `0x7000008` supported would be
  false parity.
- Unknown-name semantics require the original table predicate; the port
  cannot distinguish unknown from known without that table.
- The value is stored as an unsigned 32-bit bit pattern widened to double,
  not as `int32`.

**Historical verdict: `blocked (scoped)`.** Superseded by P6a12/D-108:
`DAT_1407f9a0` is the script-global table and the port bridges it to
`ScriptVm`; `0x7000008` is supported (36/65).

## 3. Exact next checks

1. Map `DAT_1407f9a0` (allocation/type, vtable `+0x14/+0x1c`) and where its
   entries are created/persisted; compare with `BT_VARS` names.
2. Find a script-visible bridge (`ScriptVm::globals_` or save globals) and
   prove `name_state` reads/writes in a real dialog.
3. Only then implement `WorldStateRuntime` and switch the native to
   supported; add real-script E2E and save/load seeding.
