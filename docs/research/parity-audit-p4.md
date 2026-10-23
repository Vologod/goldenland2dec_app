# Parity audit: P4a, P4b, P4b2 (read-only)

Date: 2026-10-22. Scope: P4a (PAD/person scripts/SEF persons/standing NPC rendering),
P4b (routes/delays/go animation/speed), P4b2 (walk grid/collision/A*/occupancy).
No source changes and no commits were made for this audit.

## Method and evidence levels

Evidence was taken only from repository documents, real-data tests and source. No original
binary was re-run for this audit. Status vocabulary:

- `verified` — the behaviour/format is tied to an original-binary fact (RVA, decompilation or
  live memory) and/or exact real-data match, as cited.
- `partial` — the data/format side or a port test is proven, but a semantic detail is not tied
  to the original binary, or the claim is narrower than the implementation.
- `unverified` — no original-binary evidence; the implementation is a port assumption, a
  `docs/DECISIONS.md` compromise, or only a port test.

Key original source: `Server.dll` v1.3, SHA-256
`E3E0A262B1A60DE687523465AD5A36E8` (`docs/research/00-game-build.md:11`), image base
`0x140000000`; `Client.dll` SHA-256 `D6E519AA7214B406A60F24B75A8165AF`
(`docs/research/00-game-build.md:10`) — no Client.dll rendering evidence was found for P4a.
Live-memory evidence for walkability: `world+0x330`, getter `FUN_00426234`, vtable
`0x484a00` (`docs/research/p4b2-walkability-pathfinding.md:73-96`).

A "port test" below means the test proves only that the port is self-consistent; it is not
evidence of original-game parity.

## P4a — PAD, person scripts, SEF persons, standing NPC rendering

| # | Behaviour | Implementation (file:line) | Evidence | Status | Gap |
|---|---|---|---|---|---|
| A1 | PAD header: `"PAD "`, `dataSize`, `animationMasks`, known-type mask | `src/assets/src/pad.cpp:196-250` | `docs/research/formats.md:146-182`; real sweep 276/276 `.pad`, 2450 animations, 0 failures (`tests/assets/test_real_persons.cpp:46-87`) | verified (format) | None for format acceptance. |
| A2 | PAD animation fixed fields: `delay`, `framesPerRow`, frame size, anchors, `movement_x/y` | `src/assets/src/pad.cpp:103-122` | `docs/research/formats.md:156-172`; all 276 real PADs parse; real `_go` values `6.0..6.916667` px and `5.0` px, `delay 85 ms` (`docs/research/p4b-person-movement.md:79-87`) | verified (parse) | `delay` is preserved including 0/negative (`docs/research/formats.md:194-195`); consumers must guard it. |
| A3 | PAD crop and shadow tables, `rows = cropsTableSize / 8 / framesPerRow` | `src/assets/src/pad.cpp:63-155` | `docs/research/formats.md:167-178`; real 276/276; synthetic tests `tests/assets/test_pad.cpp:128-171` | verified (parse) | Renderer ignores `crops` and draws the full frame; see A6. |
| A4 | PAD tail contract: optional `u32 count` + pairs + repeated animation records; arbitrary tail rejected | `src/assets/src/pad.cpp:162-193` | `docs/research/formats.md:183-193`; `tests/assets/test_pad.cpp:243-264`; real 276/276 incl. `l0.p1081_interwizards07` | verified | None found. |
| A5 | PAD `movement_x/y` are the values consumed as speed | `src/game/src/person.cpp:221-241`; `src/game/src/level_simulation.cpp:180-182` | `docs/research/p4b-person-movement.md:79-87` (speed written in `FUN_1401fef8`, RVA `0x1fef8`; descriptor `+8/+0xc` = PAD movement) | verified | Semantics of the time unit are not verified; see B10. |
| A6 | PAD `anchor_x/anchor_y` used as draw origin; `crops` ignored | `src/app/src/person_render.cpp:20-31,45-55`; `src/app/src/level_screen.cpp:385-427,414-427` | `docs/research/formats.md:239-260` states the origin formula and that crops are ignored; that statement is based on P4a sheet dimensions, not on `Client.dll` | partial | No Client.dll/RVA evidence for anchor/crop semantics. Crops are discarded, so non-`l1_1` sheets with non-transparent padding will render differently. |
| A7 | Person/shadow direction tables: 8-dir `{0,1,2,3,4,3,2,1}`, 16-dir `{0..8,7..1}`; 5..7/9..15 mirror; shadows identity | `src/assets/include/gl/assets/pad.hpp:66-71`; `src/assets/src/pad.cpp:265-280` | `docs/research/formats.md:175-178,196-197` (secondary source `GoldenLandEditor::PAD_Parser.cpp`); `tests/app/test_person_render.cpp:91-135`; `tests/assets/test_pad.cpp:195-229` | partial | Tables are from the editor/PAD interpretation, not from `Client.dll`; mirroring in the original renderer is not proven. |
| A8 | Person `.scr` parse: `res_name`, `level`, dotted nested values, cp1251, comments | `src/assets/src/person_script.cpp:146-232`; `src/assets/src/person.cpp:128-144` | `docs/research/formats.md:126-144`; real 15/15 unique `l1_1` scripts in `tests/assets/test_real_persons.cpp:132-163` | verified (parse) | Most parsed values are unused; `level` is not checked against the SEF level. |
| A9 | SEF `persons:` fields and the 16-person count on `l1_1` | `src/assets/src/sef.cpp:195-304`; `src/game/src/person.cpp:128-213` | `docs/research/formats.md:136-144`; `docs/research/p4b-person-movement.md:21-38`; real 261/261 SEFs parse (`tests/assets/test_real_persons.cpp:195-232`), 16 persons (`:104-105`), 15 unique scripts (`:132-135`); loader 16 instances (`tests/game/test_real_person_loader.cpp:29-102`) | verified (l1_1 data count) | The "16" is a property of `l1_1`; no corpus-wide person-count assertion exists for all 261 SEFs. |
| A10 | SEF person fields written by the original: `literary_name` at `+0x40`, its type at `+0x44` | Not parsed: `src/assets/src/sef.cpp:218-257` handles only known fields, unknown scalar fields are ignored | `docs/research/p4b-person-movement.md:23-33` lists `literary_name`/type | unverified | `literary_name` and its type are dropped. Not needed for movement, but not parity for person data/dialogue. |
| A11 | Person loading policy: prefer `rt_stay`, fall back to `tb_stay`; prefer `rt_go`, fall back to `tb_go`; skip on fatal resource errors | `src/game/src/person.cpp:162-192,221-249` | `docs/research/formats.md:213-237`; real 16 persons, 6 `tb_stay` (`tests/game/test_real_person_loader.cpp:88-92`) | partial | Preference/fallback is a port rule; no original-binary evidence that this is the animation selection order. |
| A12 | Standing placement starts from cell centre `(tile_x*12+6, tile_y*9+4)` and subtracts anchors | `src/game/src/level_simulation.cpp:60-69`; `src/app/src/person_render.cpp:45-55`; `src/app/src/level_screen.cpp:385-427` | `docs/research/p4b-person-movement.md:35-38` (`FUN_140222d0`, RVA `0x222d0` maps tile to `x*12+6`, `y*9+4`); `docs/research/formats.md:249-255` | partial | Cell-centre mapping is original-verified; the `-anchor` draw origin is not. |
| A13 | Idle frame index `((sim_ms - state_started_ms) / delay) % framesPerRow`; non-positive delay = 1 | `src/app/src/person_render.cpp:11-18`; `src/app/src/level_screen.cpp:382-390` | `docs/research/formats.md:261-264`; port tests `tests/app/test_person_render.cpp:60-73` | partial | No Client.dll evidence for idle animation restart/phase; only port-test evidence. |
| A14 | SEF direction strings `UP, UP_LEFT, LEFT, DOWN_LEFT, DOWN, DOWN_RIGHT, RIGHT, UP_RIGHT` -> 0..7 | `src/game/src/person.cpp:55-73`; `src/game/src/level_screen.cpp:381` | `docs/research/p4b-person-movement.md:95-96` (`FUN_1401fb1c`, RVA `0x1fb1c`); `docs/research/formats.md:236-237` | verified (string map) | Mirroring/direction row use is only partial; see A7. |
| A15 | Draw order: statics/animations first, then all persons in SEF order; no Y-sort | `src/app/src/level_screen.cpp:332-359,371-428` | No original evidence in `docs/research/`; P4a status records no Y-sort (`docs/STATUS.md:124-126`) | unverified | Isometric depth ordering is not reproduced; overlapping persons/statics can be drawn in the wrong order. |
| A16 | Shadow rendering: identity rows, shadow anchors, no mirror, optional texture | `src/app/src/level_screen.cpp:392-412`; `src/app/src/person_render.cpp:33-43,57-66` | `docs/research/formats.md:254-255`; `tests/app/test_person_render.cpp:50-58,121-135` | partial | Shadow draw order shares A15; original shadow composition is not proven. |
| A17 | Missing sprite/animation policy: warn and skip a person; invalid standing geometry skipped | `src/game/src/person.cpp:83-103,128-192`; `src/app/src/level_screen.cpp:162-186` | Port behaviour only; no original-binary evidence | unverified | The original may substitute a fallback animation or keep the person; unknown. |

## P4b — routes, delays, go animation, speed, directions

| # | Behaviour | Implementation (file:line) | Evidence | Status | Gap |
|---|---|---|---|---|---|
| B1 | `route_type` mapping: `STAY=0`, `STAY_ROTATE=1`, `MOVED=2`, `MOVED_FLIP=3`, `RANDOM_RADIUS=4`, `RANDOM=5` | `src/game/src/person_movement.cpp:13-25` | `docs/research/p4b-person-movement.md:8-19` (`FUN_1401f9bc`, RVA `0x1f9bc`); `tests/game/test_person_movement.cpp:6-14` | partial | Known strings match; unknown values silently become `stay` with no warning (`docs/STATUS.md:94-96`). |
| B2 | `MOVED`: wrap index, pick `route[index]`, then advance | `src/game/src/level_simulation.cpp:250-255,294-307` | `docs/research/p4b-person-movement.md:42-47` (`FUN_1401c93c`, RVA `0x1c93c`) | partial | Original advances the index immediately after selecting the target; port advances only on arrival (`consume_goal`). Failure/blocked-goal cases diverge. |
| B3 | `MOVED_FLIP`: same, index `+= rand(0.5) ? +1 : -1` | `src/game/src/level_simulation.cpp:250-255,301-303,393-405` | `docs/research/p4b-person-movement.md:48-50` | partial | ±1 rule is implemented, but the RNG is not the original CRT `rand`, and the advance happens on arrival, not at selection. |
| B4 | `RANDOM_RADIUS`: target around spawn; `radius==0 -> 13` | `src/game/src/level_simulation.cpp:52,256-289` | `docs/research/p4b-person-movement.md:51-59` (`FUN_1401c2a0`, RVA `0x1c2a0`); D-032 | partial (superseded by D-032) | Stale 64-attempt cap / occupancy filter / 3-failure abandonment removed by D-032: the loop is unbounded except the port's 1'000'000 safety cap, rejects only out-of-bounds/non-walkable candidates and uses CRT RNG. Residual occupied-goal gap tracked as C12. |
| B5 | `RANDOM`: target around current position | `src/game/src/level_simulation.cpp:256-289` | `docs/research/p4b-person-movement.md:52-59`; D-032 | partial (superseded by D-032) | Same loop/RNG as B4 after D-032; current tile now uses `tile_of` (matches the stored tile). Stale `floor(x/12)` note superseded by D-032. |
| B6 | `STAY`: if displaced from spawn, return; otherwise stand | `src/game/src/level_simulation.cpp:73-74,260-275,289-297,312-316` | `docs/research/p4b-person-movement.md` («STAY / STAY_ROTATE (A4)», `FUN_1401c93c`, типы 0/1); D-035; synthetic tests in `tests/game/test_level_simulation.cpp` | resolved (A4/D-035) | Return-to-spawn uses the normal LOS/A*/occupancy pipeline; arrival is exact. No residual gap beyond the shared pathfinding/A* assumptions. |
| B7 | `STAY_ROTATE`: rotate in place | `src/game/src/level_simulation.cpp:71-72,278-305` | `docs/research/p4b-person-movement.md` («STAY / STAY_ROTATE (A4)»: `+0x63c=0`, `+0x2990=0`, `rand(10,25)` -> `SET_DIRECTION`); D-035; synthetic tests | resolved (A4/D-035) | Direction code mapping is implemented (`direction16 = d-10`, `direction8` via `direction16_to8`). Action duration/animation of `SET_DIRECTION` is not extracted; port uses a bounded 500 ms pause. `+0x600 = DAT_14088dbc` is not modelled; code 26 (`range` may return `max+1`) is clamped to 25. |
| B8 | Delay `< 500 ms`: no pause, immediately next target | `src/game/src/level_simulation.cpp:129-136,157-164` | `docs/research/p4b-person-movement.md:61-72` (`FUN_1401c378`, RVA `0x1c378`); `tests/game/test_level_simulation.cpp:72-82,168-175` | partial | Threshold is verified; the port adds a 500 ms fallback pause when the immediate replan fails, which is not in the described original path. |
| B9 | Random delay `rand(delay_min..delay_max)`; delay values clamped/swapped | `src/game/src/level_simulation.cpp:53-56,400-405` | `docs/research/p4b-person-movement.md:66-69` (`FUN_140061e4`; "inclusive-ish CRT rand") | partial | Inclusive vs exclusive bounds and the distribution are not pinned; clamping to >=0 and swapping min/max are port additions. |
| B10 | Speed unit: `movement_x/y` are px per go-animation frame; `speed = iso_speed(...)*1000/(go_delay*0.8)` px/s; fixed 1/60 s step | `src/game/src/level_simulation.cpp:180-182`; `src/game/src/level_simulation.cpp:16,143-150` | `docs/research/p4b-person-movement.md:165-191` live measurement: per-update step equals `iso_speed` (5.254 px for move 6/5, 3.862 px for move 4/3), update interval = `go_delay*0.8` quantized at 35 Hz (80→~63 ms, 85→~68 ms); D-033; `tests/game/test_level_simulation.cpp:150-176`, `tests/game/test_person_movement.cpp:52-54` | verified (A5/D-033, live measurement) | Unit and 0.8 factor are pinned; the original quantizes updates to 35 Hz (56/84 ms), while the port integrates at 1/60 s (`docs/DECISIONS.md:40`). Exact per-tick integration order remains a port choice. |
| B11 | Isometric speed blend `(1-fx²)*move_y + fx²*move_x` | `src/game/src/person_movement.cpp:88-94` | `docs/research/p4b-person-movement.md:84-87`; `tests/game/test_person_movement.cpp:45-50` | verified (formula) | None for the formula itself. |
| B12 | Arrival when `|dx| < 4 && |dy| < 4`; snap to centre | `src/game/src/level_simulation.cpp:183-184,187-200` | `docs/research/p4b-person-movement.md:78`; `tests/game/test_level_simulation.cpp:84-94` | partial | Port adds `distance <= step` (snap within one step), not described in the original arrival condition; timing near arrival can differ. |
| B13 | 16-way direction thresholds `0.2079, 0.5878, 0.8616, 0.9863` | `src/game/src/person_movement.cpp:7-10,27-75` | `docs/research/p4b-person-movement.md:88-90` (`FUN_1402085c`, RVA `0x2085c`); exhaustive unit test `tests/game/test_person_movement.cpp:16-34` | verified (thresholds and quadrant mapping) | Port normalises original codes 10..25 to 0..15; equivalent only if the offset assumption is exact. |
| B14 | 16 -> 8 direction map and 8 -> 16 map | `src/game/src/person_movement.cpp:77-86` | `docs/research/p4b-person-movement.md:91-94` (`FUN_14020e64` RVA `0x20e64`, `FUN_14020e14` RVA `0x20e14`); `tests/game/test_person_movement.cpp:36-43` | verified (mapping) | None found. |
| B15 | RNG: xorshift32 seeded from level/name/index, reset to `0x12345678` if zero | `src/game/src/level_simulation.cpp:28-33,70-73,393-405` | `docs/DECISIONS.md:33` (D-026) says deterministic xorshift is a deliberate port choice; original uses CRT `rand` (`docs/research/p4b-person-movement.md:66-69`) | unverified | Random sequences, waits, route-flip choices and random goals cannot match the original run. |
| B16 | Simulation update: fixed 60 Hz, one advance per fixed step | `src/game/src/level_simulation.cpp:16,143-150` | `docs/research/p4b-person-movement.md:165-191` live measurement: `sv_fps=35`, `sv_game_speed=0`, `gv_anim_speed=0`, person update interval = `go_delay*0.8` quantized to 35 Hz; D-033 | verified (A5/D-033, live measurement) | Original update cadence (35 Hz, `go_delay*0.8`) is pinned; the port's fixed 1/60 s integration and exact per-tick order remain documented port choices. |
| B17 | Initial goal planning and initial wait/`state_started_ms` | `src/game/src/level_simulation.cpp:113-140` | Port behaviour only | unverified | Initial pose/timing relative to the original spawn state is not evidenced. |
| B18 | Failed plan / occupied next tile: stop, replan each tick; `MOVED` goal retried indefinitely | `src/game/src/level_simulation.cpp:194-228,309-342`; `tests/game/test_level_simulation.cpp:289-303` | Port test only; original `FUN_14015b48`/`FUN_14020a18` partially described (`docs/research/p4b2-walkability-pathfinding.md:46-47,98-103`) | unverified | Number of retries, whether the goal is skipped, and the exact stop/replan cadence are not established. |

## P4b2 — walk grid, collision, A*, occupancy

| # | Behaviour | Implementation (file:line) | Evidence | Status | Gap |
|---|---|---|---|---|---|
| C1 | Walkability word from first tile of each LVL chunk: `relief | sound<<16`, bits 8..11 | `src/game/src/walk_grid.cpp:17-28` | `docs/research/p4b2-walkability-pathfinding.md:8-24`; live memory `world+0x330`, getter `FUN_00426234`, vtable `0x484a00` (`:73-96`) | verified | None found for the static grid. |
| C2 | Chunks are column-major: file index `cx*chunk_height+cy` | `src/assets/src/lvl.cpp:224-243`; `src/game/src/walk_grid.cpp:17-20` | `docs/research/p4b2-walkability-pathfinding.md:11-13,83-86`; memory-verified reconstruction | verified | None found. |
| C3 | Sub-tile bit: `sub=(x&1)+2*(y&1)`, `blocked=(word & (0x100<<sub)) != 0` | `src/game/src/walk_grid.cpp:21-29` | `docs/research/p4b2-walkability-pathfinding.md:18-19,87-88`; synthetic tests `tests/game/test_walk_grid.cpp:44-61` | verified | None found. |
| C4 | `l1_1` walk grid: 230x224 = 51520 cells, 18028 blocked; exact original live-grid match | `src/game/src/walk_grid.cpp:10-33`; `src/game/src/level_loader.cpp:89-92` | `docs/research/p4b2-walkability-pathfinding.md:89-94` (0/51520 after load); real test `tests/game/test_walk_grid.cpp:106-118` (18028 blocked) | verified | None found. |
| C5 | Runtime walkability changes (~331 cells by doors/scripts) | Not modelled; overlay and simulation use the static grid `src/game/src/walk_grid.cpp:10-33`; debug overlay `src/app/src/level_screen.cpp:457-470` | `docs/research/p4b2-walkability-pathfinding.md:89-94`; `docs/STATUS.md:29-30` | partial | Dynamic blockers are outside P4b2; `level.walkable` remains the post-load static grid, so door states cannot match the original at runtime. |
| C6 | Line of sight: Bresenham walk, fail on first blocked/occupied cell | `src/game/src/pathfinder.cpp:31-48` | `docs/research/p4b2-walkability-pathfinding.md:39-41` (`FUN_14012d4c`, RVA `0x12d4c`); tests `tests/game/test_pathfinder.cpp:19-24,74-83` | partial | Port explicitly exempts the start cell from occupancy (`tests/game/test_pathfinder.cpp:74-83`); the original start-cell treatment is not documented. |
| C7 | A* step cost: every one of 8 directions costs 1 (original 10) | `src/game/src/pathfinder.cpp:84-89` | `docs/research/p4b2-walkability-pathfinding.md:51-55` states original step 10 and port normalisation; no RVA for the step-cost constant is cited | partial | Equal cost and 1:1 normalisation are documented, but the original constant/function is not cited line-by-line. |
| C8 | Heuristic `max + 0.375*min`, weight 1.2 | `src/game/src/pathfinder.cpp:21-27` | `docs/research/p4b2-walkability-pathfinding.md:55-58`; `0.375` at `Server.dll:0x1408c500`; weight derived from step 10 | partial | `0.375` is sourced; the 1.2 weight is a derived normalisation, and the exact original `f` arithmetic is not shown. |
| C9 | Diagonals allowed without orthogonals check | `src/game/src/pathfinder.cpp:16-19,81-90`; `tests/game/test_pathfinder.cpp:37-50` | `docs/research/p4b2-walkability-pathfinding.md:59-60` | verified | None found. |
| C10 | Neighbour order and tie-break | `src/game/src/pathfinder.cpp:66-70,75,89` (right, left, down, up, diagonals; FIFO by insertion order) | `docs/research/p4b2-walkability-pathfinding.md:61-63` says exact original order/tie-break may differ; no original evidence | unverified | Different equal-cost paths can be chosen; residual documented risk. |
| C11 | Occupancy: current tile per NPC; own tile exempt; `u8` map | `src/game/src/level_simulation.cpp:344-364,382-391` | `docs/research/p4b2-walkability-pathfinding.md:26-32` (`DAT_14088dac` is an `int` array initialised to -1; RVAs `0x12b18/0x12cdc/0x12d00/0x12d2c`) | partial | Original stores an owner/id (`-1` empty); port stores 0/1. Own-cell exemption matches, but the owner semantics and exact update points are not modelled. |
| C12 | Own goal exempt from occupancy for all route types; no goal/path reservation marks | `src/game/src/level_simulation.cpp:372-397` | No original evidence; `blocked_for` clears `state.goal` whenever `has_goal`, and `tile_occupied` ignores that tile; no `mark_occupied` is called for the goal or path | unverified | Residual parity gap (D-032/D-034): any route's goal may be a cell occupied by another NPC, and because the goal is exempt, the walker paths into it and overlaps the occupant; there is no abort/resample or reservation. Original is described as treating other NPC cells as occupied (`docs/research/p4b2-walkability-pathfinding.md:31-32`) without a goal exception. Occupancy is rebuilt from current tiles each tick; no planned-goal mark exists. |
| C13 | Movement collision checks occupancy only, not tile flags; tile flags used only for planning | `src/game/src/level_simulation.cpp:194-216` | `docs/research/p4b2-walkability-pathfinding.md:98-103` (original `FUN_14020a18` -> `DAT_14088dac`, not `+0x64` tile flag); real test `tests/game/test_level_simulation.cpp:345-395` | verified (occupancy-only) | Exact per-tick check placement and whether the original checks the entered cell only when the cell changes are not fully evidenced. |
| C14 | Jitter fix: no walkability re-check under the feet; only occupancy during movement | `src/game/src/level_simulation.cpp:194-216`; `tests/game/test_stability.cpp:15-78` | `docs/research/p4b2-walkability-pathfinding.md:98-111`; status records 38 toggles after fix vs ~5731 before (`docs/STATUS.md:50-59`) | verified (port fix) | The original-game symptom is inferred, not directly observed; the regression test is a port test. |
| C15 | Blocked spawn `L0.P7_Kolbasens_Rodstvennik1` | No spawn-walkability handling in `LevelSimulation::create`; `src/game/src/level_simulation.cpp:41-141`; path may start from a blocked cell via `src/game/src/pathfinder.cpp:53-55` | `docs/STATUS.md:60-62` records it as a known limitation; no original evidence | unverified | Port allows a mover to spawn on a blocked tile and plan out; original reconcile/spawn rule is unknown. |
| C16 | Blocked first route cell `L0.P744_oficer1` (186,147) | `src/game/src/level_simulation.cpp:294-307,309-342` (failed `MOVED` goal is retried, never skipped) | `docs/STATUS.md:62-65`; port test `tests/game/test_level_simulation.cpp:289-303`; no original evidence | unverified | Because the index advances only on arrival, the blocked route cell can never be skipped. Original index-advance timing would likely select the next cell. |
| C17 | RANDOM goal abandonment after 3 failed plans (~1.5 s) | — | Removed by D-032; port-only `kRandomGoalRetries=3` no longer exists | superseded by D-032 | Stale: the 3-failure abandonment was a port invention and has been deleted. Kept for audit history. |
| C18 | No global deadlock proof | No proof/test; only 60s/180s determinism and no-overlap tests | `docs/STATUS.md:65-66`; `tests/game/test_level_simulation.cpp:345-395`; `tests/game/test_stability.cpp:15-78` | unverified | Two NPCs can still form a mutual blockage; not proven absent. |
| C19 | Pixel position -> tile for occupancy/movement: `lround((x-6)/12)`, `lround((y-4)/9)` | `src/game/src/level_simulation.cpp:18-23` | No original evidence; only port tests use the same formula (`tests/game/test_level_simulation.cpp:214-216,281-284,335-338`) | unverified | Original pixel-to-tile rounding/truncation is not documented; occupancy boundaries can differ. |
| C20 | `--walkable-overlay` debug view of blocked cells | `src/app/src/level_screen.cpp:457-470` | Debug-only; `docs/STATUS.md:41-49`; no original-parity requirement | unverified | Not a gameplay parity item; overlay uses the static grid (see C5). |

## Most important gaps (summary)

1. **Speed/time base (B10/B16) — resolved by A5/D-033**: live measurement pinned the
   per-go-frame step to `iso_speed(...)` and the update interval to `go_delay*0.8`
   quantized at 35 Hz; `movement_x/y` are px per go frame. Residual open item: the port
   integrates at 1/60 s while the original quantizes at 35 Hz, and the exact per-tick
   order is not extracted (see B10/B16).
2. **RNG (B15) is a deliberate deviation**: per-person xorshift vs original CRT `rand`; random
   goals, waits, `MOVED_FLIP` choices and therefore runs cannot match.
3. **Route index timing (B2/B3)**: original advances `MOVED`/`MOVED_FLIP` index when the target
   is selected; port advances on arrival, so blocked/failed goals retry instead of advancing.
4. **Occupancy goal exemption (C12) and 1-bit model (C11)** are not evidenced;
   the original's int-owner occupancy has no documented goal exception; there is no
   goal/path reservation.
5. **RANDOM/RANDOM_RADIUS selection (B4/B5/C17)**: superseded by D-032 — the 64-attempt cap,
   occupancy filter and 3-failure abandonment were port inventions and have been removed.
   Residual occupied-goal exemption is tracked as C12.
6. **PAD anchor/crops rendering (A6)**: crops are ignored and anchor semantics are not tied to
   `Client.dll`; only the 16 `l1_1` sheets are known to fit.
7. **No Y-sorting (A15)**: persons are drawn after all statics/animations in SEF order.
8. **STAY semantics (B6/B7) — resolved by A4/D-035**: return-to-spawn and
   `STAY_ROTATE` are implemented. Residual: `SET_DIRECTION` action duration is not
   extracted (port uses 500 ms), and `+0x600` bookkeeping is not modelled.
9. **Blocked spawn (C15)**: `Kolbasens` can start on a blocked tile; no original reconcile rule.
10. **Blocked first route cell (C16)**: `oficer1` never skips a blocked route cell because of
    the route-index timing.
11. **Arrival/tile mapping (B12/C19)**: extra `distance <= step` snap and `lround` pixel-to-tile
    mapping are not original-verified.
12. **Delay distribution/fallback (B8/B9)**: inclusive vs exclusive range and the 500 ms
    fallback pause are not established.
13. **A* (C7/C8/C10)**: 0.375 is sourced, but the step-cost/weight derivation and neighbour
    order/tie-break are not; equal-cost paths remain uncertain.
14. **PAD direction/mirroring (A7)**: tables come from the PAD/editor interpretation and port
    tests, not from the original client renderer.
15. **Runtime walkability (C5) and deadlock proof (C18)**: ~331 dynamic cells are not modelled;
    no global no-deadlock proof exists.

## Items explicitly **not** found to be gaps

- PAD header/body/tail acceptance on all 276 real files (`A1-A4`).
- SEF and person-script acceptance on real data; `l1_1` 16 persons (`A8-A9`).
- PAD movement fields are the same fields the original speed code consumes (`A5`).
- SEF direction string mapping and 16/8 direction maps (`A14`, `B13-B14`).
- Isometric speed formula (`B11`).
- LVL chunk word, column-major order, `0x100<<sub`, 18028/51520 blocked and the live-grid
  0/51520 match (`C1-C4`).
- A* diagonal corner-cutting (`C9`).
- `STAY`/`STAY_ROTATE` selection flow (`B6-B7`, A4/D-035); only the rotate action
  duration and `+0x600` bookkeeping remain open.
- Occupancy-only collision during movement and the jitter fix (`C13-C14`).
