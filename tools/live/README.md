# Live specials capture (Phase 1b2b2d Task 4)

Read-only, user-assisted capture of NPC item/modifier records from the original
`GoldenLand.exe`. The port never launches or patches the original; this helper only
opens the process with `PROCESS_VM_READ|PROCESS_QUERY_INFORMATION` and reads memory.
No DRM component is touched or bypassed.

Static import check (2026-11-10):

```text
llvm-objdump -p Server.dll Client.dll GoldenLand.exe
```

None of the three binaries imports `_controlfp` or `_control87`. The original does not
program the x87 control word through those CRT entry points; the documented `0x027F`
reference is therefore an extraction-time assumption until a live breakpoint capture
proves it.

## What to read

`Server.dll` is 32-bit. Use the real ASLR-resolved module base; the script tries to
read it from the process modules and otherwise accepts `-ServerBase`.

| Address | Meaning |
|---|---|
| `entity + 0x10` | NPC main-container id; resolve to the container with `FUN_1404bf8c` or a debugger |
| `container + 0x0c` | pointer to `u32` record-id array |
| `container + 0x10` | slot count |
| `container + 0x14` | used-slot bitmap |
| `DAT_14088df8` | item manager, RVA `0x88df8` |
| `manager + 0x18` | pointer to array of item-record pointers |
| `manager + 0x1c` | record count |
| `manager + 0x20` | record used-bitmap |
| `item record + 0x0c` | item flags |
| `item record + 0xa4` | record uid |
| `item record + 0xa8` | item id |
| `item record + 0xb0` | count |
| `item record + 0x78` | mod count |
| `item record + 0x7c` | pointer to mod array |
| `mod + 0x00` | mod id (stride `0x14`) |
| `mod + 0x04` | mod value, `int32` |
| `mod + 0x08` | mod flags |

The main container is `entity+0x10` (pass 2). The trade container is `entity+0x0c`
and is not part of this oracle.

## RNG state

A user-supplied seed is not enough: previous draws may have advanced the CRT stream.
Capture the MSVC CRT `_rand` state (`holdrand`) at a breakpoint immediately before the
first NPC generation (before the first `FUN_14006288`/`_rand` call for pass 1), or set a
breakpoint on `_rand` and record the state before its first draw. Pass it as `-RngState`;
the oracle initialises `MsvcRandom` from that state, not from the seed. `seed` is kept
as provenance metadata only.

## Procedure

1. Start the original game, load the level, and note the loot level used for NPC
   generation plus the CRT `_rand` state at the first generation draw.
2. In a debugger, locate the NPC entities' main containers (or item records directly).
3. Run the helper from PowerShell:

   ```powershell
   .\tools\live\capture_npc_mods.ps1 `
       -OutFile .\special-l1_1.txt `
       -Level l1_1 -RngState 0x12345678 -Seed 1 -LootLevel 1 `
       -ProcessName GoldenLand `
       -NpcContainer "L1_2.P404_Sister_Of_Mercy2=0x1234ABCD"
   ```

   `-NpcContainer` accepts `Person=0x<container>`; `-Item` accepts
   `Person=0x<item record>` when containers are not resolvable. `-ServerBase` can be
   omitted when `Server.dll` is visible in `$process.Modules`.
4. Point the gate at the file and run:

   ```powershell
   $env:GL_SPECIALS_LIVE = "I:\path\to\special-l1_1.txt"
   build\bin\gl_game_tests.exe --test-case="*special*gate*" --success
   ```

The oracle compares item rows and mod rows separately. A subset of NPCs is allowed:
the port still generates every NPC (to preserve the RNG stream) but only records for
captured persons are compared. That is reduced-scope evidence, not full-level parity;
full parity requires every relevant NPC main container.

## Open question: inherited ITM prototype effects

The original stores per-item prototype effects from ITM `effects` in the same
`+0x78/+0x7c` mod array as generated random specials (research §2.5). The port models
only generated specials and keeps inherited effects as a count-only `mod_count`; their
value/flags/duration semantics are not proven. Therefore the gate **excludes** captured
and port records whose item template has `effects > 0` (it reports how many rows were
skipped) and compares only generated-special items. This is an explicit known
limitation, not a pass; it must be revisited if prototype-effect semantics are
extracted.

## Capture format

```
format=gl-specials-live-2
level=l1_1
seed=1
rng_state=0x12345678
loot_level=1
item|L1_2.P404_Sister_Of_Mercy2|8|1|0x204|1
mod|L1_2.P404_Sister_Of_Mercy2|8|1|0x204|25|-1|0x81
```

One `item` row per main-container item (including zero-mod items) and one `mod` row per
modifier. `#` starts a comment. Numeric fields accept decimal or `0x` hex. Without a
real capture the gate stays OPEN; no synthetic file is committed.
