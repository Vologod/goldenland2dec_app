# P6a12 — Location access native (design)

Status: approved (2026-11-21)
Scope: script VM global bridge, `RS_SetLocationAccess` (0x7000008),
glgame/glscript wiring, tests. One subproject.

## Evidence

`docs/research/p6a12-location-access-unblock.md`.

- Native `0x41634`: string+int gate, builds `name + "_state"`, looks it
  up in the script-global table `DAT_1407f9a0`, writes the unsigned
  32-bit arg as double, returns `0.0`.
- `DAT_1407f9a0` is the same global context used by `FUN_14038958` /
  `FUN_14038904` during script execution.
- 68 calls in 37 shipped `.cs` files use literal names such as `"L96"`,
  `"L52_1"`, `"L23_IvlisHome"` and small integer states.

## Non-goals

- Global-map/location UI consumer (the write is still a real script
  global).
- Reproducing the original 0x104 stack-buffer overflow for long names.
- Save/load of the location state beyond normal script globals.

## Script layer

- `GlobalAccess { std::function<void(std::string_view, double)>
  set_double; }`; added to `NativeServices` as `global`.
- `bool ScriptVm::set_global(std::string_view name, Value value)`:
  - updates an existing `globals_` entry and returns `true`; an unknown
    name returns `false` and is not inserted (matching the original
    `-1` lookup);
  - updates an existing `variables_` entry so the current script sees it
    immediately; later `lookup_variable` pulls from `globals_`.
- Native `0x7000008`: `supported`, arity 2, string + int/uint32 gate
  (`[3,0]`; doubles are rejected), key =
  `as_string(args[0]) + "_state"`,
  value = `static_cast<double>(static_cast<std::uint32_t>(as_int(args[1])))`,
  calls `global.set_double` when present, returns `0.0` always.

## App / tools

- glgame: after creating `context->vm`, bind
  `services().global.set_double` to the active `dialog_->vm`.
  Successful writes are stored in `runtime_globals_` and re-applied after
  each dialog VM is seeded, so the state survives dialog switches within
  a session; New Game and save load clear it.
- glscript: bind to its `ScriptVm` after construction.
- Missing service is a no-op, consistent with other `NativeServices`.

## Tests

- `tests/game/test_location_access.cpp`:
  - registry row supported, arity 2, handler non-null;
  - callback receives `L96_state` and `2.0`;
  - `-1` becomes `4294967295.0` (unsigned conversion);
  - doubles, string/int type failures and missing service do not call
    back;
  - a declared global updates, an undeclared `L96_state` stays absent;
  - supported total 36/65 (update the three count tests).
- `tests/script/test_vm.cpp` or `test_saved_globals.cpp`: `set_global`
  updates globals and an existing variable, and a later `str_var` lookup
  sees the value.
- Full `ctest` with `GL_GAME_DIR`.

## Acceptance

- `0x7000008` supported (36/65).
- Native key/value and unsigned conversion asserted.
- `ScriptVm` visibility asserted.
- Docs (`STATUS`, `PROJECT`, `DECISIONS`, research table, p6a4 note)
  updated; full `ctest` green.

## Residual

Global-map UI is not modeled; no shipped `.cs` reads `*_state` as a
literal, so the native is verified at the global-table boundary, not by a
real global-map screen.
