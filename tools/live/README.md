# Live specials capture (Phase 1b2b2d Task 4)

Read-only, user-assisted capture of the original 32-bit `GoldenLand.exe`. The tool
attaches with `DebugActiveProcess` only to install hardware execute breakpoints, then
detaches and reads NPC main containers with `PROCESS_VM_READ`. It never calls
`WriteProcessMemory`, creates remote threads, patches code, injects a module, or touches
DRM. The port never launches or modifies the original.

The capture file is consumed by `tests/game/test_special_gates.cpp` through
`GL_SPECIALS_LIVE`.

## Build

The tool is deliberately not part of the 64-bit CMake build. Build it with the bundled
i686 toolchain:

```powershell
I:\goldenland2dec\.local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin\i686-w64-mingw32-g++.exe `
    -std=c++20 -O2 -static tools/live/gl_live_capture.cpp `
    -o tools/live/gl_live_capture.exe -lpsapi -lshell32
```

The resulting `tools/live/gl_live_capture.exe` is ignored by `.gitignore` and is not
committed. `-lpsapi` is kept for compatibility with the documented toolchain command;
the current implementation uses Toolhelp module/thread enumeration and `shell32` for
`CommandLineToArgvW`.

Smoke check:

```powershell
.\tools\live\gl_live_capture.exe --help
```

## Run

1. Start the original 32-bit game and leave it in the main menu. Find the PID:

   ```powershell
   Get-Process GoldenLand | Select-Object Id,ProcessName
   ```

2. Start the capture **before loading the level**, then load the level in the game. The
   tool arms all current threads (and new threads) with hardware execute breakpoints:

   ```powershell
   .\tools\live\gl_live_capture.exe `
       --pid 12345 --level l1_1 --loot-level 1 `
       --out .\special-l1_1.txt
   ```

   Optional arguments: `--timeout <sec>` (default 120) and `--server <dll>` (default
   `Server.dll`; a full path is accepted). The capture tool must be 32-bit to debug the
   32-bit game. `SeDebugPrivilege` is requested; without it, attach may still work for a
   same-user process, otherwise run from an elevated shell.

   `--scan-only` attaches without installing breakpoints, scans the already loaded
   level once, prints up to 20 `person/item/count/mods` rows, and exits 0 when the
   scan is complete with at least one person, otherwise 4. It is a diagnostic mode;
   it never writes an oracle capture.

3. When the first NPC generation draw is hit, the tool records the CRT `_rand` state and
   the live x87 control word, detaches, waits for the level containers to settle, scans
   every `EF_PERSON` main container, and writes the capture. Without a level load within
   the timeout it detaches, scans what is available, and exits non-zero with a
   `.partial` diagnostic file (it never fabricates a valid capture).

4. Run the live gates:

   ```powershell
   $env:GL_SPECIALS_LIVE = "I:\path\to\special-l1_1.txt"
   build\bin\gl_game_tests.exe --test-case="*special*gate*" --success
   ```

   The FPU-CW gate takes either a single value or a file with `generation=` and `price=`
   keys:

   ```powershell
   $env:GL_SPECIALS_FPU_CW = "0x027F"
   ```

   The capture file contains `# fpu_cw=0x.... source=first-draw` and
   `# fpu_cw=0x.... source=scan-time` comment lines. The tool does not capture the
   client-side price call site, so the `price=` half of `GL_SPECIALS_FPU_CW` must come
   from a separate capture (or from the documented `0x027F` assumption, which remains an
   assumption until proven live).

## What it captures

- Server module base from the process module list (ASLR-safe).
- DR0 at `Server.dll!FUN_1404b1c0` (RVA `0x4b1c0`), the NPC SEF spawn entry.
- DR1 at the `_rand` pre-draw instruction (RVA `0x6c0a4`, `mov ecx,[eax+0x14]`). At this
  point `EAX` is the CRT per-thread pointer and `*(u32*)(EAX+0x14)` is the state **before**
  the first draw; the thread context supplies `FloatSave.ControlWord`.
- After detach: `DAT_14088dc4` level manager, the level entity array at `+0x2564`, used
  bitmap at `+0x2568`, count at `+0x256c`; `EF_PERSON` is `(entity+0x14 & 0x3e) == 2`.
  The person name is the C string at `entity+0x4c`; the main container id is at
  `entity+0x10`.
- `DAT_14088df8` item manager. Containers are resolved through the manager's
  key→value map at `+0x0c` (count at `+0`, entry array at `+4`, stride 8:
  key at `+0`, container pointer at `+4`), matching `FUN_1404bf8c` →
  `FUN_14004f14(manager+0xc)` → `FUN_14004a20`. The old array-index lookup was
  wrong. A container record has `+0x0c` record-id array, `+0x10` slot count,
  `+0x14` used bitmap.
- Item record: flags `+0x0c`, uid `+0xa4`, item id `+0xa8`, count `+0xb0`, mod count
  `+0x78`, mod array `+0x7c`. Mod stride is `0x14`: id `+0x00`, value `+0x04`,
  flags `+0x08`.
- Duplicate `EF_PERSON` names are first-wins, matching the port's duplicate handling.
  `Hero` is skipped. Invalid uid/count/item id, out-of-range ids, null records, unused
  bitmap bits, null mod arrays, and unreadable mods are skipped with warnings; the tool
  does not crash and does not silently emit invalid rows.

## Capture format

```
# gl-specials-live-capture v2
format=gl-specials-live-2
level=l1_1
seed=0
rng_state=0x12345678
loot_level=1
# fpu_cw=0x027f source=generation-entry
# fpu_cw=0x027f source=first-draw
# fpu_cw=0x037f source=scan-time
item|L1_2.P404_Sister_Of_Mercy2|8|1|0x204|1
mod|L1_2.P404_Sister_Of_Mercy2|8|1|0x204|25|-1|0x81
```

`seed=0` is provenance metadata only; `rng_state` is authoritative. Comments start with
`#` and are ignored by the gate parser. One `item` row is written per main-container item
(including zero-mod items) and one `mod` row per modifier. Numeric fields accept decimal
or `0x` hex in the parser; the tool writes decimal counts/ids and `0x` flags.

If the RNG state or NPC items are unavailable, the tool writes
`format=gl-specials-live-2-partial` to `<out>.partial` and exits non-zero. The gate
rejects that format, so an incomplete run cannot be mistaken for a real oracle.

## Exit codes

| Code | Meaning |
|---:|---|
| 0 | Valid capture written |
| 1 | Usage or argument error |
| 2 | Module/process/attach error |
| 3 | Timeout or RNG state not captured (partial file) |
| 4 | RNG state captured but no NPC items found (partial file) |
| 5 | Output write error |
| 6 | Target process exited before capture completed |

## Limitations and caveats

- The tool is 32-bit only, matching the original game. A 64-bit build will not attach to
  the 32-bit target.
- Start the tool before the level load. If the first NPC generation already happened,
  the breakpoints cannot recover the pre-draw CRT state; the tool times out and writes a
  partial diagnostic file. It does not guess a seed.
- The scan starts after `DebugActiveProcessStop` and polls for up to 15 seconds until the
  item/mod count is stable for 500 ms. This gives level generation time to finish; it is
  not a fixed sleep and it is not evidence by itself.
- Only the main container (`entity+0x10`) is captured. The trade container
  (`entity+0x0c`) is out of scope for this oracle.
- Captured rows include inherited ITM prototype effects in the same `+0x78/+0x7c` mod
  array. The gate excludes items with ITM `effects > 0` and reports how many rows were
  skipped; inherited effect semantics are not modelled.
- The live x87 control word is captured at the NPC spawn entry and at the first `_rand`
  pre-draw site, plus a scan-time sample. The client-side `FUN_1206E8D0` price call site
  is not captured; `GL_SPECIALS_FPU_CW` still needs a `price=` value from a separate
  capture or the explicit `0x027F` assumption.
- `SeDebugPrivilege` may be unavailable in a non-elevated shell. Attach can still work
  for a same-user process, but protected/anti-debug targets may deny it. The tool does
  not bypass DRM; it reports the failure and exits.
- If `DebugActiveProcessStop` fails, the tool reports it and exits; because
  `DebugSetProcessKillOnExit(FALSE)` is set, the target is detached when the tool process
  exits and is not killed.
- A subset of NPCs is still reduced-scope evidence for the oracle. Full parity requires
  all relevant NPC main containers in the capture.

## Static import check

Static inspection on 2026-11-10:

```text
llvm-objdump -p Server.dll Client.dll GoldenLand.exe
```

None of the three binaries imports `_controlfp` or `_control87`. The original does not
program the x87 control word through those CRT entry points; the documented `0x027F`
reference is therefore an extraction-time assumption until a live breakpoint capture
proves it.
