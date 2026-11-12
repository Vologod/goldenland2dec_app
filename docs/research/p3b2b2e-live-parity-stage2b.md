# Phase 1b2b2e Stage 2b — live parity: merge/window/RNG phase/price CW

Research-only. No production code or test changes. Date: 2026-11-11.

## 0. Scope and provenance

The port reproduces the live NPC call order 1:1, inherited ITM effects are
confirmed, and the generation-entry x87 CW is live-measured as `0x007F`
(PC=24, RC=nearest). The residual `l1_1` gate is
`items matched=1 mismatched=49; mods matched=0 mismatched=21`.
This document answers the four Stage 2b questions from the originals and
records a reproduction table plus an implementation recommendation.

Inputs (hashes verified this session):

| Artifact | Size | SHA-256 |
|---|---|---|
| `Server.dll` | 589824 | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `Client.dll` | 1331200 | `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16` |
| `.local/decomp/server/decompiled.c` | — | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| `.local/decomp/client/decompiled.c` | — | `BBD6480E428971893DBCF28A4A73402EFEE7EEBA483BA27D3C23B65B29A4EF16` |
| live capture `.local/live/special-l1_1-trace.txt` | 2956 | `EF83998BF2A4E4D524AE490770942E45024106C67E5952A78FF7403837F6EBB5` |
| `scripts/item_class_specials/specials.scr` (extracted) | — | `4F29A2AC464B86A9B4092736888E8F15623C2964BA2F8AC940AB46E5BA913B79` |

The capture's `# rng_second=0xad5bd7cd check=fail` is the known pre-fix tool
artifact (STATUS/D-062); `rng_state=0xad5bd7cd` is the CRT `_rand` state read
at `_rand` (`Server.dll` RVA `0x6c09f`; state at `pDVar1[5]` = `+0x14`,
decompiled.c:73218-73227) *before* the multiply at RVA `0x6c0a4`, so it is a
valid pre-first-draw state.

Reproduction note: to obtain the port's full main-container output for this
capture, the `# rng_second=` line was removed in a temporary copy under
`%TEMP%` (all `item|`/`mod|` rows and `rng_state` byte-identical). The gate
otherwise rejects the capture on the known tool artifact. That copy was used
only for research; no test was weakened or rerun as a pass claim.

## 1. Q1 — merge/stacking `FUN_1404e3a4` (RVA `0x4e3a4`)

Definition: decompiled.c:53633-54000. Callers inside `FUN_1404b1c0`:
pass 1 at decompiled.c:51179 and pass 2 at decompiled.c:51320, both
`FUN_1404e3a4(entry_proto, 0, 1)`.

### 1.1 Merge condition (decompiled.c:53744-53817)

The merge search walks the prototype's instance registry
(`puVar21[4]` population, `puVar21[5]` bitmap, `puVar21[3]` id array),
resolves each id through the global object table, and merges only when:

- the existing record's prototype id equals the just-created prototype
  identity: `*(undefined ***)(iVar7 + 0xa8) == local_1084` (53790);
- `max_stack >= 2`: `if (*(uint *)(iVar7 + 0x34) < 2) break;` (53791) —
  a `max_stack < 2` prototype breaks to creation (never merges);
- the existing stack is not full: `if (*(uint *)(iVar7 + 0xb0) <
  *(uint *)(iVar7 + 0x34))` (53792).

On merge it increments the existing count by one
(`*(uint *)(iVar7 + 0xb0) += 1`, 53793) and returns the existing id
(53808-53809). If the stack is full the block is skipped; after the scan the
function falls through to creation. `-1` is returned only on failure
(53742 virtual-create failure, 53835 ctor failure); the callers test
`== 0xffffffff` and `break` (51181, 51321).

### 1.2 Count is overwritten, not summed (asm; critical)

The caller overwrites `+0xb0` after the merge/create call. Pass-2 asm
(`Server.dll` VA `0x1404b84c`):

```
1404b84c  movl -0x18(%ebp),%ecx   ; target container (this_01)
1404b84f  pushl %eax              ; id returned by FUN_1404e3a4
1404b850  calll FUN_1404dbec
1404b855  movl %eax,%edi          ; item record
1404b857  movl 0x34(%edi),%ebx    ; max_stack
1404b85a  cmpl %esi,%ebx          ; esi = remaining chunk
1404b85c  jbe  1404b860
1404b85e  movl %esi,%ebx          ; ebx = min(remaining, max_stack)
1404b860  movl %ebx,0xb0(%edi)    ; count = chunk   (OVERWRITE)
...
1404b881  subl %ebx,%esi
```

So `FUN_1404e3a4`'s `+1` merge probe is discarded by the caller: after any
merge the record count equals the current chunk (`min(remaining,max_stack)`),
**not** the sum of the merged entries. Consequence: for stackable items a
group of same-prototype `.inv` rows collapses to one record whose final count
is the **last** generated chunk; it is not `a+b+...`.

This is confirmed empirically (see §3.4): under the live-compatible RNG phase,
P692 pass 2 produces two money chunks `89` then `67`; without merge the port
emits two records (89, 67), with merge the live single record is `67`
(overwrite, not 156).

### 1.3 Hot and main are separate containers

`FUN_1404b1c0` resolves two containers up front:
`this_00 = FUN_1404bf8c(this, param_1)` and
`this_01 = FUN_1404bf8c(this, param_2)` (decompiled.c:51092-51093).
The initial-spawn caller passes `param_1 = entity+0xc` (hot) and
`param_2 = entity+0x10` (main):
decompiled.c:22454 and 18090
(`FUN_1404b1c0(manager, this+0xc, this+0x10, inv_name, 1)`), and the
save/load path passes `param_4=0` (22085).

- Pass 1 adds to `this_00` (`FUN_1404dbec(this_00, id)`, 51182) and sets
  `manager+4=1` around the call (51178-51180), which clears the item flag
  `0x80` (53919-53921).
- Pass 2 adds to `this_01` (`FUN_1404dbec(this_01, id)`, 51322) with
  `manager+4=0` (51319), so `0x80` is preserved.

`FUN_1404dbec` (RVA `0x4dbec`, decompiled.c:53283-53345) then verifies the
returned id exists in *that* container by scanning its id array/bitmap; the
caller immediately dereferences the returned record. A merge into a record
held only by the other container would therefore return 0 and be unsafe, and
no live crash/record is observed. Treat the effective behaviour as
per-container: pass-1 merges stay in hot, pass-2 merges stay in main, and the
two passes do not merge into one another. The live scanner reads main
(`entity+0x10`, `tools/live/gl_live_capture.cpp:33, 885-888`), which is why
live sees pass-2-only money stacks while pass 1 still consumes RNG.

### 1.4 Merge and RNG

Merge consumes no RNG. The only RNG wrappers in `Server.dll` are
`FUN_140061e4` (range) and `FUN_14006288` (coin) (decompiled.c:4347, 4385);
their caller list does not include `FUN_1404e3a4`, `FUN_1404bf8c`,
`FUN_1404dbec`, `FUN_1404f4fc` or `FUN_1404f81c`. An empirical test that
inserted one RNG draw per created record did not reproduce live (P678 became
money 136 + item 417 instead of 139 + 420), so record creation is not an RNG
consumer either. Merge can change the record/stack count in main but never the
RNG stream.

## 2. Q2 — P678 money 139 and item 420 at `loot_level=2`

### 2.1 `.inv` data (extracted bytes; VFS-resolved)

| file | size | SHA-256 |
|---|---|---|
| `p387.inv` | 109 | `AA086A6F0C281BED3DDDAC480228E0B537DF9D89715AE2D67B40F521907F25A5` |
| `inv012_rich_civil.inv` | 1330 | `E90F43120B64E2FDB6922AB75BD640269895162420EB7345F918251573809881` |
| `inv018_food_potion.inv` | 2193 | `4EDEF4097345AD885D8D7B4FB6ADC24216FF0F25B37A1D9CB72946292AB9F3F1` |
| `inv012_civil.inv` | 1330 | `35261EF287E9AE26211F61EFB9F1C864C4C375B752E4CBFB5B61A9D1C32F82E2` |
| `inv014_money.inv` | 649 | `19F01416F184EE99F5A000EC6D9721AF3621437557C67532C57FED2EC7BE2EDE` |
| `inv018_garbage_reagent.inv` | 1314 | `58E5562BA686148EDC045D3869DEE9C001DC5BA6D9B0A0C203008EF5E2C08348` |
| `inv001_swordsman.inv` | 6178 | `4236A677DAA4E33BC32165FB9385692D154D0161095E6964E09E39832952659D` |
| `inv011_caster.inv` | 14287 | `C6FD2FA66EB7AFA68E98CD2E04147D6322AA00A9691D8C59E7810707D56B1C99` |

`inv012_rich_civil.inv`: header `regenerate_chance 75`, `chance_decrease 2`,
`quantity_decrease 1`, `level_offset 0`; 30 rows = 15 `MON_1_0_1` rows then
15 `FOD_1_*` rows. At `loot_level=2` the in-window money rows are the first
three (`-100..10`, qty 50-100, 60-120, 70-140) and all 15 FOD rows
(`-100..100`, chance 6, qty 1-3). Money id `812 = MON_1_0_1`, type 18,
flags `0xC0000000`, `max_stack=100000000`, effects 0. `420 = FOD_1_14_1`,
type 20, flags `0x8000000C`, `max_stack=10000`, effects 6 (types 13..18,
amount 5, flags `0x50`), exactly the 6 live mods.

Text extraction is from `glpak extract` on the VFS, which applies overlays.
`scripts.update.3.pak` contains **no** relevant inventory file; its only
inventory entries are `q112.inv`, `q63.inv`, `unique/love.inv`,
`unique/qsttsvet.inv` (560 entries total). Overlay hypothesis refuted.

### 2.2 RNG replay (MsvcRandom, `state=0xad5bd7cd`)

Constants read from `Server.dll`: `0x1408d7b0 = 0.99999`,
`0x1408d7b8 = 1/32767`, `0x1408d7c4 = 1/32767`, `0x1408d7c8 = 1.0`,
`0x1408d7cc = 0.0`, `0x14091ee0 = 0.01`, `0x14091ef8 = 0.0`.

P387 (`p387.inv`: `regenerate_chance 0`, one `QST_1_0_5` row min1 max100
chance100 qty1..1, item id 975 type25 flags `0x40000028`): R1 draws nothing
(p=0); pass 1 coin(1.0) = draw 1 (`25804`, 0.788 < 1 → success); pass 2
coin(1.0) = draw 2 (`18649`, 0.569 → success); qty `1..1` draws nothing; the
item flag `0x28 & 4 == 0`, so no specials. **P387 consumes exactly 2 draws**
and produces id 975 count 1, matching live.

P678 then depends on where its R1/pass 1/pass 2 start. Under the port model
(no extra draws) P678 pass 2 starts at draw 25 and yields money 78 and no FOD
(verified with the real binaries). Live needs exactly **2 additional draws
consumed after P387 and before P678**. With draws 3-4 consumed, the replay is:

| draw | value | site | result |
|---|---|---|---|
| 5 | 5741 | P678 R1 `coin(0.75)` | 0.175 < 0.75 → continue |
| 6/7 | 23960 / 29088 | pass1 money e1 coin / `range(50,100)` | success → 114 |
| 8/9 | 11043 / 18230 | pass1 money e2 coin / `range(60,120)` | success → 109 |
| 10/11 | 23765 / 21344 | pass1 money e3 coin / `range(70,140)` | success → 132 |
| 12-26 | — | pass1 15 FOD coins `coin(0.06)` | all > 0.06 → no FOD |
| 27 | 28578 | pass2 money e1 `coin(0.5)` | 0.872 → fail |
| 28 | 30137 | pass2 money e2 `coin(0.5)` | 0.919 → fail |
| 29 | 12511 | pass2 money e3 `coin(0.5)` | 0.382 → success |
| 30 | 32223 | pass2 money e3 `range(70,140)` | **139** |
| 31-43 | — | pass2 FOD rows 1-13 `coin(0.03)` | all > 0.03 |
| 44 | 114 | pass2 FOD row 14 `coin(0.03)` | 0.00348 → success (id 420) |
| 45 | 6647 | FOD row 14 `range(1,3)` | **1** (mod_count 6 inherited) |

This reproduces live P678 exactly: `812 count=139 flags=0xC0000000` and
`420 count=1 flags=0x8000000C mod_count=6`. Money 139 is therefore a **single
pass-2 `range(70,140)` roll of the third `.inv` row**, not a merged sum and
not a wider window.

### 2.3 Rejected hypotheses

- `loot_level=3`: adds the fourth money row (`min 3`, qty 80-160). With the
  same phase this makes the last-triggered money chunk 93 (merge) or emits
  139+93 (no merge) — does not match the single live 139. Also the level is
  read from `BT_INFO+0x26c4` = 1, `+1` = 2 both by the tool
  (`tools/live/gl_live_capture.cpp:36`) and by `FUN_1404b1c0`
  (`iVar5 = *(+0x26c4) + 1`, decompiled.c:51149).
- merge-sum: rejected by §1.2/§3.4 (P692 merge gives 67, not 156) and by the
  P678 replay (only one money row triggers in pass 2).
- overlay: refuted in §2.1.
- reverse `.inv` iteration / uniform per-call offset / one draw per created
  record: tested in the analytic simulator, all fail P678 and later persons
  (see §3.5).

## 3. Q3 — P693 item 938 vs port 407

Both ids are entries in `inv018_food_potion.inv` (not two names for one item):

- `938 = POT_1_00_12` (`sdb/items/tech_names.sdb` line 939), row
  `POT_1_00_12 1 100 5 1 1`, type 19, flags `0x0C`, `max_stack=10000`,
  effects 2 → types 11/1/0x50 and 9/-1/0x50 (the 2 live mods).
- `407 = FOD_1_1_1`, row `FOD_1_1_1 1 100 5 1 3`, type 20, flags
  `0x8000000C`, `max_stack=10000`, effects 0.

Both are in-window at level 2. Under the port's current RNG alignment the
food rows win (`407 count=3`); under the live-compatible phase the potion row
wins and yields `938 count=1` with 2 inherited mods. There is **no independent
name→id or selection difference**; P693 is downstream of the same per-call RNG
phase that produces P678. Fixing the phase reproduces 938 exactly.

## 4. Q4 — price x87 CW at `FUN_1206E8D0`

`Client.dll` preferred base `0x12000000`; `FUN_1206E8D0` RVA `0x6e8d0`
(client `functions.csv` rva 452816, size 966, callers 5;
decompiled.c:72880-72885). It is the item-price/value function and its first
x87 instruction is at VA `0x1206e967` (`fildl 0x10(%esi)`).

The function reads the ambient CW, forces only the rounding-control bits to
truncate for the final rounding step, then restores the CW:

```
1206e9a2  fnstcw (%esp)
1206e9a5  movzwl (%esp),%eax
1206e9a9  orl $0xC00,%eax        ; RC=truncate only
1206e9b2  fldcw 0x8(%esp)
1206e9b6  fistpl 0x10(%esp)
1206e9ba  fldcw (%esp)           ; restore ambient CW
```

So the ambient CW at entry controls the `fild/fmul/fadd` intermediates
(PC bits), while the final `fistp` uses RC=truncate regardless. A hardware
execution breakpoint at `Client.dll base + 0x6e8d0` can read
`CONTEXT.FloatSave.ControlWord` (`tools/live/gl_live_capture.cpp:746,1276,1320`
already do this for generation). A slightly better site is the first x87
instruction VA `0x1206e967` (or the `fnstcw` at `0x1206e9a2` for the exact
pre-rounding CW).

Tool extension needed (research only; tool not modified here):

1. The tool already resolves arbitrary modules via `find_module` (line 413)
   and already reads `FloatSave.ControlWord`, but only for `Server.dll`
   (`--server`, default `Server.dll`, line 74) and only at
   `Server+0x6c0a4`.
2. Add a client module (`Client.dll`) and a price breakpoint at
   `client.base + 0x6e8d0` (or `+0x6e967`); on hit read the thread
   `FloatSave.ControlWord` and emit e.g.
   `# fpu_cw=0x.... source=price` (price-thread id, DR slot).
3. Reuse the existing DR save/restore and per-thread arming; note the known
   minor: DR0 `RW0` bits are not cleared on `ensure_thread`.

Acceptance: the price-time CW is captured on the thread that calls
`FUN_1206E8D0`; expected `0x007F` (PC=24, RC=nearest) to match the
generation CW, with the function's internal `0xC00` truncate override still
modelled by the port.

## 5. Reproduction table

Simulator: faithful port of the decompiled `FUN_1404b1c0` (both passes,
`FUN_14006288`/`FUN_140061e4` constants, `specials.scr` + `SpecialEngine`
R4/R5 semantics, `max_stack` chunking) over the real `.inv`/ITM data.
"Phase" = extra CRT `_rand` draws inserted before each NPC generation call.

| Hypothesis | P678 main | P692 main | P693 main | Live match |
|---|---|---|---|---|
| Port as-is (H0) | 812:78 | 812:98, 812:127 | 407:3 | no |
| H0 + merge only | 812:78 | 812:127 | 407:3 | no |
| `loot_level=3` + merge | 812:93 | differs | differs | no |
| overlay `.update.3` | unchanged | unchanged | unchanged | no (no files) |
| one RNG draw per created record | 812:136 + 417:1 | differs | differs | no |
| reverse `.inv` iteration | differs | differs | differs | no |
| uniform per-call offset `c` (0..30) | best 3/22 items | — | — | no |
| **per-call phase + merge** | **812:139 + 420:1 (6 mods)** | **812:67** | **938:1 (2 mods)** | **all 22 items / 14 mods** |
| per-call phase, **no merge** | 812:139 + 420:1 | 812:89, 812:67 | 938:1 | no (record count) |

The per-call phase + merge run matches the live mod multiset exactly as well
(`14/14` `(person,item_id,count,item_flags,mod_id,value,mod_flags)` tuples:
`420` 13..18/5/0x50; `293` 54/2/0x41; `8` 45/2/0x41; `113` 6/2/0x41;
`938` 11/1/0x50 and 9/-1/0x50; `332` 24/2/0x41; `419` 6/2/0x50 and
8/-2/0x50). This also confirms the `SpecialEngine`/inherited-effects model is
correct; only the RNG phase is off.

Per-call phase increments recovered by the search (`gaps[i+1]` = extra draws
after person `i`): `2,3,4,2,2,3,7,2,4,3,?,4,2,3,3` for
P387,P678,P7,P389,P404,P785,P403,P693,P711,P692,P744,P640,P621,P733,P620
(the duplicate `P744` was not searched). The increments are variable and
positive; the cumulative prefix is monotone. Persons whose inventory is
money-only (`P403` uses `inv014_money`, `P711` uses `inv014_money`) require
+7 and +4 extra draws even though they contain no specials-capable item
(money flags `0xC0000000 & 4 == 0`), so the extra consumption is **outside the
decompiled loot draw sites**, not a specials/window/quantity artefact.

## 6. Recommendation for the follow-up implementation plan

1. **Implement merge/stacking in `NpcLootGenerator` (required).**
   Match `FUN_1404e3a4` + caller exactly:
   - per container (hot and main separate; never cross-merge);
   - if a same-prototype record exists, `max_stack >= 2`, and
     `record.count < max_stack` → keep the existing record/slot and set its
     count to the current chunk (`min(remaining,max_stack)`), i.e. overwrite;
     do **not** sum and do **not** create a record; merge consumes no RNG;
   - otherwise (no match, `max_stack < 2`, or `count == max_stack`) → create a
     new record with `min(remaining,max_stack)`;
   - inherit `flags`/`mods` from the first record; specials apply to the
     record returned by the merge/create step (only when `chunk==1`,
     `flags&4`, `mod_count==0`).
   Acceptance oracle (analytic/unit):
   - chunk 89 then 67 → one record count 67 (P692 under the corrected phase);
   - full stack then next chunk → a new record; `max_stack < 2` → new record;
   - hot and main never merge; merged record keeps first-record flags/mods;
   - live/analytic P678/P692/P693 multisets match once phase is corrected.

2. **Do not change window/quantity or VFS overlay handling.**
   Evidence: level 2 is read consistently; the three rich-civil money rows and
   all 15 FOD rows are the correct window; quantity formulae (pass-1 raw,
   pass-2 `/chance_decrease` and `/quantity_decrease` with the ceil-style
   bound) are correct; `loot_level=3` and `.update.3` overlays are refuted.
   Acceptance: keep the existing real `.inv` tests; add a regression asserting
   `inv012_rich_civil` at level 2 selects 3 money + 15 FOD rows and that
   `scripts.update.3.pak` has no inventory `.inv` overrides for these files.

3. **Close the RNG phase gap (blocker for 0/N).**
   The port under-consumes a variable number of CRT `_rand` calls per
   `FUN_1404b1c0` call (2-7 in the recovered phase solution). This is outside
   the decompiled loot draw sites (money-only inventories need it too, and
   merge consumes none). Do not paper over it with a fixed seed offset.
   Follow-up tool work:
   - keep DR1 armed across the whole generation window and count `_rand`
     pre-draw hits between consecutive `FUN_1404b1c0` (DR0) hits;
   - emit `call=N ... draws=M` (and optionally state before/after) per call,
     plus the existing hot/main/inventory identity;
   - optionally break on the per-entity init path around the
     `FUN_1404b1c0` caller (decompiled.c:22422-22455, 18090, 22085) to find
     the variable consumer.
   Only after the measured per-call draw counts match the model can the NPC
   oracle be claimed 0/N.
   Acceptance oracle: for `l1_1` with the corrected RNG start,
   `mismatched_items == 0 && mismatched_mods == 0`, including P678
   `812:139 + 420:1(6 mods)`, P692 `812:67`, P693 `938:1(2 mods)`.

4. **Measure the live price CW (Stage 4).**
   Extend the tool as in §4 (Client module + DR at `base+0x6e8d0`/`+0x6e967`,
   read `FloatSave.ControlWord`, emit `source=price`). Acceptance: captured
   price CW equals the generation CW `0x007F` (PC=24/RC=nearest) and the
   Stage 3 binary32/trunc price model reproduces the live per-unit prices.

## 7. Open questions / limits

- The per-call phase solution is an existence proof: it reproduces all 22
  live items and 14 mods with the decompiled loot model + merge, but the
  physical source of the variable extra draws is not yet identified. The
  most likely class is per-entity/level-load RNG consumption outside NPC loot
  (money-only inventories require it). This must be measured, not guessed.
- The capture was produced with the pre-fix tool (`rng_second check=fail`), so
  the capture cannot be used as-is by the gate; a re-capture with the fixed
  tool (per-call draw counts) is the recommended next live step.
- No production code or tests were modified; the research copy of the capture
  lives only under `%TEMP%`.

## 8. Per-call draw trace (2026-11-11)

Valid new-game capture `.local/live/special-start-draws5.txt`:
`# rng_second=0x33a1acb4 check=ok`, `rng_state=0x8e09c205`, 8 calls,
1869 `# draw=` rows. Caller offsets were corrected per wrapper:
`FUN_140061e4` (int range) caller at `[esp+0x30]`;
`FUN_1400624c` (float range) and `FUN_14006288` (coin) caller at
`[esp+0xc]`.

Caller histogram (RVA → function):

| caller RVA | function | draws | role |
|---|---|---:|---|
| `0x4b32d/0x4b406/0x4b444/0x4b6fd/0x4b7e8/0x4baf2/0x4ba0e` | `FUN_1404b1c0` | 344+343+… | NPC loot (R1/pass1/pass2) |
| `0x47378/0x47604/0x47625/0x47677/0x4773b` | `FUN_14047284` | 131 | specials |
| `0x1dd4e` | `FUN_1401d99c` | 9–? per call | per-person random initial weapon (`coin(0.5)` loop) |
| `0x221c0` | `FUN_14021bc8` | 1 per call | per-entity spawn init `range(+0x5e4,+0x5e8)` |
| `0x48c0b/0x48c4c` | `FUN_140489ec` | 139+33 | generic/hero loot (post-generation) |
| `0x4951d` | `FUN_14048fac` | 6 | generic loot |
| `0x5c53f` | `FUN_1405c4c8` | 672 | post-generation AI/update |
| `0x1c3a4/0x1c2f8/0x1c305` | movement | 42/7/7 | post-generation AI/update |
| `0x186dc/0x18786` | not in functions.csv | 9/8 | additional init (unmapped) |

Per-call draws and the tail extras (draws between this loot call and the
next one):

| call | inventory | draws | tail extras before next call |
|---:|---|---:|---|
| 1 | `inv011_caster` | 284 | `0x1dd4e`×2 + `0x221c0`×1 |
| 2 | `inv002_rich_spearman` | 69 | `0x1dd4e`×7 + `0x221c0`×1 |
| 3 | `q1` | 3 | `0x1dd4e`×1 + `0x221c0`×1 |
| 4 | `inv001_swordsman` | 83 | `0x1dd4e`×4 + `0x221c0`×1 |
| 5 | `inv001_swordsman` | 107 | `0x1dd4e`×? + `0x221c0`×1 |
| 6 | `inv014_money` | 14 | `0x1dd4e` + `0x221c0` |
| 7 | `inv011_caster` | 246 | `0x1dd4e` + `0x221c0` |
| 8 | `inv001_swordsman` | 1063 | post-generation pollution (not loot) |

Conclusion: the variable per-call phase is **per-person pre-loot RNG**.
`FUN_1401d99c` parses the person parameter file (`weapon` block) and selects
an initial weapon by repeating `coin(0.5)` until success (1–7 draws);
`FUN_14021bc8` performs spawn init (`range(this+0x5e4,this+0x5e8)`, one
draw). These draws happen after the previous person's loot call and before
the next person's `FUN_1404b1c0`, so the port's pure loot stream is shifted
by 2–8 draws per person. The port does not model these systems
(`NpcLootGenerator` is loot-only). Part 3 must implement the person
parameter/equipment selection and spawn-init draws in the SEF order; without
that, the `l1_1` loot oracle cannot reach 0/N. Merge/stacking and
window/quantity are already correct (Part 2 Task 1).

## 9. Per-person RNG model (2026-11-11)

Weapon (`FUN_1401d99c`, caller RVA `0x1dd4e`) + spawn init
(`FUN_14021bc8`, caller RVA `0x221c0`) are consumed after each person's
loot: `coin(0.5)` over the person-script `weapon` block (file order,
`min+offset <= level <= max+offset`, wrap) then one `range(3000,10000)`.
Retail corpus: 782/782 person scripts have a weapon block with >=1 eligible
row. Implemented in `game::consume_person_init_rng`; live l1_1 gate is
`items 22/22, mods 14/14` (0 mismatched).
