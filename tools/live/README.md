# Live specials capture (Phase 1b2b2d Task 4)

Read-only, user-assisted capture of NPC modifier records from the original
`GoldenLand.exe`. The port never launches or patches the original; this helper only
opens the process with `PROCESS_VM_READ|PROCESS_QUERY_INFORMATION` and reads memory.
No DRM component is touched or bypassed.

## What to read

`Server.dll` is 32-bit. Image base `0x14000000`, RVA = VA - `0x14000000`.

| Address | Meaning |
|---|---|
| `entity + 0x10` | NPC main-container id; resolve to the container with `FUN_1404bf8c` or a debugger |
| `container + 0x0c` | pointer to `u32` record-id array |
| `container + 0x10` | slot count |
| `container + 0x14` | used-slot bitmap |
| `DAT_14088df8` | item manager, RVA `0x88df8` |
| `manager + 0x18` | pointer to array of item-record pointers |
| `item record + 0x0c` | item flags |
| `item record + 0xa8` | item id |
| `item record + 0xb0` | count |
| `item record + 0x78` | mod count |
| `item record + 0x7c` | pointer to mod array |
| `mod + 0x00` | mod id (stride `0x14`) |
| `mod + 0x04` | mod value, `int32` |
| `mod + 0x08` | mod flags |

The main container is `entity+0x10` (pass 2). The trade container is `entity+0x0c`
and is not part of this oracle.

## Procedure

1. Start the original game, load the level, and note the RNG seed and loot level used
   for NPC generation.
2. In a debugger, locate the NPC entities' main containers (or item records directly).
3. Run the helper from a 32-bit-aware PowerShell:

   ```powershell
   .\tools\live\capture_npc_mods.ps1 `
       -OutFile .\special-l1_1.txt `
       -Level l1_1 -Seed 1 -LootLevel 1 `
       -ProcessName GoldenLand `
       -ServerBase 0x14000000 `
       -NpcContainer "L1_2.P404_Sister_Of_Mercy2=0x1234ABCD"
   ```

   `-NpcContainer` accepts `Person=0x<container>`; `-Item` accepts
   `Person=0x<item record>` when containers are not resolvable.
4. Point the gate at the file and run:

   ```powershell
   $env:GL_SPECIALS_LIVE = "I:\path\to\special-l1_1.txt"
   build\bin\gl_game_tests.exe --test-case="*special*gate*" --success
   ```

The gate compares the captured multiset with the port's `NpcLootGenerator` main
container for the same level/seed/loot level. It skips when `GL_SPECIALS_LIVE` or
`GL_GAME_DIR` is absent, and fails if any record differs. A zero-record capture is
rejected rather than treated as a pass.

## Capture format

```
format=gl-specials-live-1
level=l1_1
seed=1
loot_level=1
person|item_id|count|mod_id|value|mod_flags|item_flags
L1_2.P404_Sister_Of_Mercy2|8|1|25|-1|0x81|0x204
```

One row per modifier. `#` starts a comment. Numeric fields accept decimal or `0x`
hex. Without a real capture the gate stays OPEN; no synthetic file is committed.
