# P6a12 — Location access native unblock research

Status: complete (2026-11-21)
Scope: static reverse + script usage; feeds spec/plan, no product code.

## Sources

- Server.dll SHA-256 `418E2560…CC837`, image base `0x140000000`;
  `Server.dll` RVAs below.
- GoldenLand.exe / Client.dll decompiled overview in
  `p6a4-location-access.md`.
- `scripts.pak` extracted with `glpak`; 556 `.cs` disassembled via
  `glpak disasm-cs` (temporary scanner, not committed).

## Native contract (`0x41634`)

Disassembly `0x14041634..0x140416e7`:

1. Type gate `FUN_14038c6c([3,0], -1)`: string + int; on failure return
   `fldl 0x140914e8` (`0.0`).
2. Copy arg0 (`0x14089740`) into a `0x104` stack buffer, then append the
   literal `_state` (`0x1408fbcc`).
3. `ecx = *DAT_1407f9a0`; call vtable `+0x14(key)`. A result of `-1`
   means unknown and skips the write.
4. Convert arg1 (`0x14089748`) with `fildl`; `shrl 31` + `fadds`
   `0x140914f8/0x140914fc` adds `0` or `4294967296.0`, so the stored
   value is the **unsigned 32-bit** value as a double.
5. Call vtable `+0x1c(key, double)`; every path returns `0.0`.

## `DAT_1407f9a0` is the script-global table

- Created in `Server.dll` startup: `FUN_140057cc(0x14)` then
  `FUN_14002510` (`decompiled.c:34033-34041`); object is 0x14 bytes.
- Script execution uses the same object as the global context:
  `FUN_14038958(local, name, len, DAT_1407f9a0, 0, 0)` and
  `FUN_14038904(local, DAT_1407f9a0)` (`decompiled.c:25842-25843`;
  `FUN_140232b0` evaluates a variable name against it).
- `+0x14` is a string→index lookup (returns `-1` when absent), `+0x1c`
  stores a double, `+0xc` reads a double (used by door/trigger scripts as
  `bOpenDoor`/`bOpenTrigger`).
- Therefore `RS_SetLocationAccess(name, value)` writes the global variable
  `<name>_state` as a double. It does not create a private map.

## Exact key and values in shipped dialogs

Scanner over all 556 `.cs` (parallel `glpak disasm-cs`, 4 workers):

- 37 files, **68 calls** to `117440520` (`RS_SetLocationAccess`).
- First arguments are literal location names: `"L96"`, `"L52_1"`,
  `"L23_IvlisHome"`, etc.; second arguments are small integers
  (`2`, `1`, `0`, ...). Example
  `scripts/dialogs/zlato_czen_1/l5_2.p116_talec.d116.age.cs:566`.
- Only the globals declared in `scripts/dialogs_special/zlato_vars.scr`
  (`ZLATO_state`, `MARVIA_state`, `L7_1_state`, `L10_1_state`, ...,
  `L31_1_state`; 20 entries) and values present in save `BT_VARS` are
  known at runtime. Names such as `L96_state`, `L52_1_state` and
  `L23_IvlisHome_state` are absent from that declaration and are therefore
  no-ops in the original on a fresh session.
- No `.cs` reads a literal `*_state` variable via `str_var`; the value is
  consumed by engine/client code (`Client.dll` builds `_state` names from
  the literal `"_state"` at `0x12131924`), i.e. global-map/location state.

## Port mapping

The port already has the equivalent global table: `ScriptVm::globals_`
(`std::map<std::string, Value>`), read by `lookup_variable` and rebuilt
from saved globals. The unblock is a small VM bridge:

- add `GlobalAccess { std::function<void(std::string_view, double)>
  set_double; }` to `NativeServices`;
- `ScriptVm::set_global(name, Value)` updates only an existing
  `globals_` entry (unknown names return false/are not inserted, matching
  the original `-1`) and the current `variables_` entry;
- glgame/glscript bind the service to the active `ScriptVm`;
- glgame keeps successful writes in `runtime_globals_` and re-applies
  them after each dialog VM is seeded, so the value persists across
  dialog switches until New Game/save load;
- the native builds `name + "_state"`, converts arg1 to uint32 double,
  calls the service, returns `0.0`.

## Residuals

- The global-map/location consumer is not modeled in the port; the write
  is observable to scripts and future UI, not to a global-map screen yet.
- The original copies `name` into a fixed 0x104 buffer without a bounds
  check; the port uses `std::string` and does not reproduce overflow.
- Unknown names are no-ops (same as the original `-1` lookup).
