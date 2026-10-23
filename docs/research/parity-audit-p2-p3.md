# Parity audit: P2 (core/config/console/state/tick/saves) and P3 (CS parser/VM/dialogs/SDB/font)

Date: 2026-10-23. Mode: **read-only**. No source, test or game-data changes were made;
no commits were created for this audit.

Scope: P2 (`src/core`, `src/assets/save.*`, `tools/glgame`) and P3
(`src/assets/cs.*`, `src/script`, `src/assets/sdb.*`, `src/render/font.*`,
dialog code in `tools/glgame`). P4 movement/pathfinding is referenced only where it
supplies the only known original-binary fact relevant to P2 timing.

## 0. Method and status vocabulary

Read: `AGENTS.md`, `docs/STATUS.md`, `docs/PROJECT.md`, `docs/DECISIONS.md`, all
`docs/research/*`, the P2/P3 specs and plans, the listed source files and tests.

Read-only runs used as evidence (2026-10-23, target `I:\Games\Златогорье-2`):

- `glpak info` — `scripts.pak` 4485, `sdb.pak` 45, `sdb.update.3.pak` 7,
  `persons.pak` 5212; all archives opened.
- `glgame --list-saves` — 28 lines; `slot0  2022-11-06  Quick save  642409`.
- `gl_core_real_tests` — 2/2 cases, 190 assertions.
- `gl_script_real_tests` — 2/2 cases, 46 assertions; `globals: line 918/1262:
  missing value`.
- `gl_cs_tools_tests` — 9/9 cases, 67 assertions.
- `gl_assets_real_tests` — 13/13 cases, 299 assertions:
  `.cs parsed=555 failures=0 nodes=338977`;
  `.csx=19704 .sdb=315 .itm=1848`; LVL 175/175; LAO 33/33; PAD 276/276,
  2450 animations; SEF 261/261.
- `font_tests` 8/8 (82 assertions), `gl_core_tests` 71/71 (543),
  `gl_script_tests` 58/58 (443), `gl_assets_tests` 155/155 (1073).
- `glscript --file scripts/dialogs/demon.d1.age.cs --run --sdb
  sdb/dialogs/dialogsphrases.sdb` — `wait_user`, 43 steps,
  `dialogs: 2684 2686 2696`, `answers: 2`, then the three real phrases.
- `glpak decode-sdb sdb/dialogs/dialogsphrases.sdb` — 12040 entries.
- `glpak decode-sdb sdb/diary/cities_lit.sdb` — 16 entries; this is one of
  17 mounted `.sdb` files without the `SDB ` signature (the base `sdb.pak` has
  27/18 signature/no-signature; the VFS overlay yields 28/17), and the decoded
  values are Cyrillic (XOR `0xAA` branch exercised on real data).
- read-only extraction/scan of 555 base `.cs` files from `scripts.pak`:
  opcode and function-id counts, 7,439 non-dialog native calls, 1,154
  `D_PlaySound` calls, 51 calls with function id `0` in 7 scripts.
- read-only ASCII-string scan of `Client.dll`: 137 command-like strings
  (`gv_*`, `cl_*`, `r_*`, `d_*`, `sv_*`, `game_save`, `exit`).
- read-only extraction of `scripts/fonts.scr` and listing of
  `engineres/fonts/ttf/*.ttf`.

Statuses:

- **verified** — tied to an original-binary fact (file, SHA/RVA/offset, live
  memory) and/or an exact real-data run that pins the claim.
- **partial** — the data/format side is proven, but a semantic detail comes from
  a secondary source or is not pinned by a test.
- **unverified** — no original-binary evidence; this is a port decision,
  a design-document choice, an editor-derived rule, or a port test only.

**Source of truth.** `AGENTS.md:53-67` and D-029 (`docs/DECISIONS.md:36`) say the
original game (`Server.dll`/`Client.dll`/`GoldenLand.exe`) is the only source of
truth; "plausible" implementations are forbidden. P2 research
(`p2a-game-shell.md`, `p2b-deterministic-tick.md`, `p2c-save-slots.md`) cites the
shipped INI/saves and port code/tests, not original binaries. P3 VM research
(`p3a-cs-vm.md:1-7`) explicitly cites `DarkContact/GoldenLandEditor` commit
`65f31da` (`CsExecutor.cpp`) — a secondary source. Therefore P2/P3 behaviour below
is **not** "verified against the original" merely because it passes port tests.

## 1. Evidence baseline and fingerprints

The only fingerprints in the repository are MD5 values in
`docs/research/00-game-build.md:7-12`. The "SHA-256" labels in
`p4b-person-movement.md:3-5` and `p4b2-walkability-pathfinding.md:3-4` actually
repeat the MD5 of `Server.dll`. Read-only `Get-FileHash -Algorithm SHA256` gives:

| File | SHA-256 (computed 2026-10-23) | MD5 in 00-game-build.md |
|---|---|---|
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` | `E3E0A262B1A60DE687523465AD5A36E8` |
| `Client.dll` | `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16` | `D6E519AA7214B406A60F24B75A8165AF` |
| `GoldenLand.exe` | `7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF` | `6EE2462796FA41DC3796515009A10763` |
| `GoldenLand.ini` | `4AAAC6E4646327A192A6BBDE499CEFE70228EE220532D09ECCFB7C2E62D05C50` | not in docs |

`scripts.pak` has a documented SHA-256 of
`25C2CF44DC3B45B6A1CC5CF0FF768EFA9B4B76032FA590BC99018E6DAA5AAB46`
(`p3a-cs-format.md:53-58`). The target installation has 28 saves
(`slot0.sav`..`slot27.sav`) plus `Вертас.hero`; all 28 are version 17.

## 2. P2 — core, config, console, state, tick, saves

| # | Item | Implementation | Evidence in docs/research / audit | Status | Gap |
|---|---|---|---|---|---|
| P2-1 | INI line model: blank, `//` comment, `bind`, key-value, inline comment only after space/tab; cp1251 storage | `src/core/src/config.cpp:21-70`, `:212-248`; `src/core/include/gl/core/config.hpp:28-46` | `p2a-game-shell.md:7-22`; real `GoldenLand.ini` (SHA-256 above) round-trips byte-identically (`tests/core/test_real_config.cpp:29-43`); unit tests `tests/core/test_config.cpp:61-133,221-238` | **partial** | Round-trip is proven; equivalence to the original INI parser (case sensitivity, duplicate keys, comment rules) is not extracted from `GoldenLand.exe`. |
| P2-2 | Typed get/set: `get`, `get_int`, `get_float`, `get_bool`, `set`, `set_int`; preserve order/prefix/suffix | `src/core/src/config.cpp:137-206` | `p2a-game-shell.md:14-20`; `tests/core/test_config.cpp:135-297` | **partial** | `get_bool` accepts only exact `0/1`, keys are case-sensitive, newline values are rejected, missing INI = defaults (D-020). All are port choices, not original semantics. |
| P2-3 | Config keys used by the shell: `cl_username`, `r_windowed`, `r_resolution`, `d_console`, `gv_pause`, `gv_free_camera`, `gv_skip_logo`, `bind` | `tools/glgame/main.cpp:907-913`, `:929-949`; `src/core/src/server.cpp:120-207` | `p2a-game-shell.md:14-22`; target INI actually contains only `cl_username`, `d_console`, `r_windowed`, `gv_pause` + 43 `bind` lines (audit parse); `r_resolution`, `r_windowed`, `gv_pause`, `gv_skip_logo` strings exist in `GoldenLand.exe`/`Client.dll` | **partial** | `r_resolution` is absent from the target INI; `gv_free_camera`/`gv_skip_logo` behaviour is a port stub; the original key set and precedence are not reverse-engineered. |
| P2-4 | Config save: temp file + rename, create parent dirs, defaults on missing file | `src/core/src/config.cpp:102-135`; D-019/D-020 (`docs/DECISIONS.md:26-27`) | `p2a-game-shell.md:16-18`; `tests/core/test_config.cpp:152-168,240-281`; real round-trip test | **partial** | Atomic replacement and creating a missing INI are port decisions; original write ordering/failure behaviour is unknown. |
| P2-5 | Command registry: registration, duplicate rejection, execute, sorted `help`, unique-prefix completion | `src/core/src/command.cpp:57-127`; `src/core/include/gl/core/command.hpp:14-32` | `p2a-game-shell.md:32-38`; `tests/core/test_command.cpp:64-172` | **unverified vs original** | Quoting/escaping (`\"`, `\\`), case-sensitive lookup, `help <cmd>` and completion are port semantics; no original console parser evidence. |
| P2-6 | Console buffer and input: 256 output lines, 64 history, last 10 on screen, `~`/backquote, arrows, Tab | `src/core/include/gl/core/console.hpp:9-10`; `src/core/src/console.cpp:15-53`; `tools/glgame/main.cpp:1187-1206`, `:1643-1684` | `p2a-game-shell.md:32-46`; `tests/core/test_console.cpp:5-86` | **unverified vs original** | The limits 256/64/10, `~` key, history reset and Tab behaviour are port constants; original console values are not extracted. |
| P2-7 | Implemented command set and semantics: `help`, `exit`, `d_console`, `gv_pause`, `gv_free_camera`, `r_resolution`, `r_windowed`, `cl_username`, `saves`, `game_save` (stub) | `tools/glgame/main.cpp:802-893`; `src/core/src/server.cpp:120-207` | `p2a-game-shell.md:36-46`; audit string scan of `Client.dll` found 137 command-like strings; `gv_pause`, `r_resolution`, `r_windowed`, `gv_skip_logo`, `gv_free_camera`, `game_save`, `game_load` strings exist in the original binaries | **partial** | Only 9 of ~137 original command-like names are implemented; argument rules/usage text are port-specific; `game_save` is a stub and `game_load`/quick save/load are absent. |
| P2-8 | Application state machine: `boot -> menu -> level`, `level -> menu/level`, `exit` final | `src/core/src/state.cpp:6-27`; `src/core/include/gl/core/state.hpp:4` | `p2a-game-shell.md:48-53`; spec `2026-10-20-p2a-game-shell-design.md:66-75` | **unverified vs original** | This is a shell skeleton. Original boot/logo/loading/pause/save-menu states and their transitions are not reconstructed. |
| P2-9 | Fixed server tick: 60 Hz, `dt` clamp `[0,0.25]`, max 15 steps, accumulator reset, time from `tick_index` | `src/core/include/gl/core/server.hpp:8-11`; `src/core/src/server.cpp:68-95` | `p2b-deterministic-tick.md:8-29`; D-021 (`docs/DECISIONS.md:28`); `p4b-person-movement.md:98-104` records original `sv_fps` default **35** and says the Person update call frequency is not statically pinned | **unverified** | 60 Hz is a port design choice, not an original rate. It conflicts with the only known original timing constant (`sv_fps=35`) and can change movement/AI/animation cadence. |
| P2-10 | Commands applied before pause check; pause freezes tick/time; `gv_pause 0` resumes in the same tick | `src/core/src/server.cpp:68-77`, `:124-148` | `p2b-deterministic-tick.md:17-19,31-53`; `tests/core/test_server.cpp:33-85,390-437` | **partial** | This is a port contract; the original command-application order and pause semantics are not extracted. |
| P2-11 | `ServerCore`/`ClientCore` boundary: snapshot and event drain, no sockets | `src/core/src/server.cpp:105-118,209-218`; `src/core/include/gl/core/server.hpp:34-76` | `p2b-deterministic-tick.md:31-38`; `00-game-build.md:56-58` (original is client-server) | **partial** | The boundary is preserved, but the snapshot is only pause/free-camera/tick/time; no world/event serialization or network protocol exists. |
| P2-12 | Save header: 112 bytes, magic 20, version u32, date u16s, title 64 bytes at offset 40 cp1251/NUL, trailing-space trim | `src/assets/src/save.cpp:12-50`; `src/assets/include/gl/assets/save.hpp:8-15` | `p2c-save-slots.md:7-21`; real 28-slot test `tests/core/test_real_saves.cpp:18-57`; audit `--list-saves` shows `Quick save`, `Убмйство кукуя`, `Продаем артифакты` | **verified** (layout/version/title encoding on target) | Bytes at 28/38/104/108 (`unknown_a/b/c/d`) are ignored; version is not constrained; trailing-space trimming is a port choice. The real-slot test pins year/month but **not** day or file size, although the research claims all three. |
| P2-13 | Save scan: case-insensitive `slot<N>.sav`, sort by slot, broken files stay invalid, missing dir = empty | `src/core/src/save_manager.cpp:34-123`; `src/core/include/gl/core/save_manager.hpp:11-33` | `p2c-save-slots.md:24-45`; `tests/core/test_save_manager.cpp:64-154`; audit 28 files slot0..slot27 | **partial** | No slot limit is enforced (any `slot<N>` is accepted); non-slot `.sav` files are listed as invalid; the original slot count/limit and handling of foreign saves are unknown. |
| P2-14 | Save-slot UI and `--list-saves`: row format, scrolling, `saves` command, load message | `tools/glgame/main.cpp:315-339`, `:1300-1406`, `:1519-1582`, `:1743-1757` | `p2c-save-slots.md:26-39`; screenshot/list tests `tests/app/test_glgame_screenshot.cpp:383-409`; audit 28 lines | **partial** | The UI is custom. The original uses `scripts/ui/save_load_menu.scr` and `engineres/interface/save_load_menu/*` (strings in `Client.dll`); no portraits/quick-save/load. |
| P2-15 | Save payload/write/load | absent; `game_save` only returns a stub string | `tools/glgame/main.cpp:884-892`; `:1403-1406`; `p2c-save-slots-design.md:24-28`; original strings `game_save (%s), %d`, `game_save (Quick save), 0`, `game_load %d` in `Client.dll` | **unverified / absent** | Only headers are read. Full world deserialization and save writing are missing, so campaign save/load parity is blocked. |
| P2-16 | Provenance of P2 semantics | — | `p2a-game-shell.md:3-5`, `p2b-deterministic-tick.md:4-6`, `p2c-save-slots.md:3-5` cite shipped files and port code/tests, not original binaries | **unverified** | There is no `Server.dll`/`Client.dll`/`GoldenLand.exe` RVA or live-memory evidence for config/console/state/tick semantics. |

## 3. P3 — CS parser, VM, globals, dialogs, SDB, font

| # | Item | Implementation | Evidence in docs/research / audit | Status | Gap |
|---|---|---|---|---|---|
| P3-1 | CS node layout: u32 size, opcode, four ints / f64 / cp1251 C-string / func(9 args, `-1`) / jmp | `src/assets/src/cs.cpp:43-89`, `:92-126`; `src/assets/include/gl/assets/cs.hpp:10-22` | `p3a-cs-format.md:7-23,53-58`; real `555/555`, `338977` nodes (`tests/assets/test_real_cs.cpp:27-77`); `scripts.pak` SHA-256 above | **verified** (corpus parse) / **partial** (provenance) | The layout comes from `GoldenLandEditor::CS_Parser`, not from a `Server.dll` reader. Exact parse on 555 files is strong for the data, but not for original reader limits/error handling. |
| P3-2 | Legacy XOR body: `plain[i] = data[i] ^ ((i+0x55)&0xFF)`, `i>=4`, tried only if first opcode invalid | `src/assets/src/cs.cpp:106-122`; D-022 (`docs/DECISIONS.md:29`) | `p3a-cs-format.md:25-51`; six base scripts; three pairs cross-checked against `scripts.update.3.pak`; audit 555/555 parse | **partial** | Only three of six decoded files have an independent update copy; the fallback depends on the first opcode, so a differently corrupted plain file is not retried. |
| P3-3 | Opcode and native-function name tables | `src/assets/src/cs.cpp:128-164`, `:166-246`; `src/script/include/gl/script/function_ids.hpp:5-13` | `p3a-cs-format.md:21-23`; `p3a-cs-vm.md:64-74`; audit function scan | **partial** | Names are editor-derived. `cs_function_name(0)` is `unknown`, yet real data calls id `0` 51 times in 7 scripts with three meaningful args (`"Hero"`, `"STRENGTH"`, `4` etc.); the table also has gaps (e.g. `0x7000011/0x7000012`). |
| P3-4 | VM top-level flow, logical ops (0..2), comparisons (6..11), jmp, dialog recording | `src/script/src/script_vm.cpp:203-325`, `:624-650`; dialog recording `:443-456` | `p3a-cs-vm.md:9-47,76-97`; real `demon.d1.age.cs` to `wait_user`; audit opcode counts | **partial** | Semantics are from `GoldenLandEditor::CsExecutor`; no `Server.dll` RVA. Real execution coverage is one dialog; other scripts are only parsed. |
| P3-5 | Arithmetic (14..18), bitwise (3..5, 12/13), unary (19/20) | `src/script/src/script_vm.cpp:458-584` | `p3a-cs-vm.md:19-21,51-62`; synthetic tests `tests/script/test_vm.cpp:689-785`; audit shows 1/3/4/5/9/12/13/18/19/20 never appear in 555 real `.cs` | **partial / unverified** | Bitwise/unary are synthetic-only; arithmetic/relational real use is tiny (`16` once, `17` once, `8` three times). Semantics (signed/unsigned, shift, `%`, div-by-zero) are port decisions. |
| P3-6 | `assign` type coercion to receiver (`int32`/`uint32`/`double`/`string`) | `src/script/src/script_vm.cpp:254-287`, `:586-622` | `p3a-cs-vm.md:58-62`; D-025 (`docs/DECISIONS.md:32`); tests `tests/script/test_vm.cpp:574-642` | **unverified vs original** | The editor itself had a TODO here; the port chose receiver coercion. This can change variable values and branch outcomes. |
| P3-7 | Limits/error handling: 10M steps, `infinity`, 100k eval budget/step, sticky errors, max depth 1024 | `src/script/include/gl/script/script_vm.hpp:38-40`; `src/script/src/script_vm.cpp:203-220`, `:331-340`, `:678-688` | `p3a-cs-vm.md:39-47,147-154`; D-023/D-025 (`docs/DECISIONS.md:30-32`) | **unverified** | 10M vs editor 10k, the 100k eval budget, sticky-error stop and `infinity` are deliberate port choices; no original limit/error evidence. |
| P3-8 | Dialog data: `dialogs_data()`, `answers_count()`, `user_input` (0-based answers), first `D_Say`, `D_CloseDialog -> end` | `src/script/src/script_vm.cpp:163-201`, `:443-456` | `p3a-cs-vm.md:76-97`; D-023/D-024 (`docs/DECISIONS.md:30-31`); real demon dialog | **partial** | 0-based answer numbering differs from the editor's `dialogFuncs` indexing; `D_CloseDialog` ends instead of returning 0; only the first `D_Say` is exposed. All are documented deviations, not original-verified. |
| P3-9 | Native dispatch and host stubs; `RS_GetPersonParameterI`, `Exit`, `Signal`, `Console`, `Cmd`, known/unknown logging | `src/script/src/script_vm.cpp:424-456`; `src/script/src/host.cpp:17-39` | `p3a-cs-vm.md:64-97`; audit: 7,439 non-dialog native calls + 1,154 `D_PlaySound` calls in 555 scripts, all handled as stubs | **unverified / absent** | Every native returns `0`; no `Server.dll` semantics. Real scripts call `RS_SetPersonParameterI`, items, quests, party, time, events, etc.; none of it changes state. This is the largest P3 behavioural gap. |
| P3-10 | Globals: `int`/`DWORD`/`float`/`string`, comments, cp1251, BOM, malformed lines | `src/script/src/globals.cpp:100-182` | `p3a-cs-vm.md:106-113`; real `zlato_vars.scr` (784 variables, warnings 918/1262); `tests/script/test_globals.cpp:17-124` | **partial** | Unknown types are skipped with a warning while the editor defaulted to `int32 0`; quoting/unescaping is port-defined; only one real globals file is tested. |
| P3-11 | `DialogSession`: start/choose, phrase + answers, missing SDB id fallback | `src/script/src/dialog_session.cpp:12-73` | `p3c-dialogs-sdb-font.md:9-25`; real test `tests/script/test_real_dialog_session.cpp:61-114`; audit `glscript --sdb` output | **verified** (demon path) / **partial** (coverage) | Only `demon.d1.age.cs` is exercised; no NPC-to-dialog binding; fallback `<phrase N>` is a port convention. |
| P3-12 | Dialog screen: panel, NPC text wrap, numbered answers, Up/Down, Enter, mouse, Esc | `tools/glgame/main.cpp:545-583`, `:785-800`, `:1435-1517` | `p3c-dialogs-sdb-font.md:45-75`; screenshot tests `tests/app/test_glgame_screenshot.cpp:414-592` | **unverified vs original** | Layout, keys, panel height and no-scroll are port UI. Original dialog UI assets/portraits/sound are not reconstructed. |
| P3-13 | SDB parse: optional `SDB ` signature, XOR `0xAA`, u32 id + u32 length + cp1251 | `src/assets/src/sdb.cpp:7-26`; `src/assets/include/gl/assets/sdb.hpp:9-20` | `formats.md:60-70`; `p3c-dialogs-sdb-font.md:9-10`; audit: 315 `.sdb` parse; 45 mounted files = 28 signature / 17 no-signature (base `sdb.pak` 27/18); `dialogsphrases` 12040 entries; decoded Cyrillic from `cities_lit.sdb` | **partial** (both branches exercised) | The real-data test only checks `failures == 0` and `decoded > 0`; it does not pin 315/12040, duplicate ids or per-file content. |
| P3-14 | SDB -> dialog text mapping and `glscript --sdb` output | `src/script/src/dialog_session.cpp:51-72`; `tools/glscript/main.cpp:172-183` | `p3c-dialogs-sdb-font.md:12-25`; audit output: `2684 Чего уставился?`, `2686 Я не думал, что демоны разумны...`, `2696 Ничего. Я пошел...` | **verified** (specific data) / **partial** (coverage) | Only one dialog and a handful of ids are checked; 12,040 SDB entries are not swept against scripts. |
| P3-15 | TTF atlas: stb_truetype, ASCII + Latin-1 + Cyrillic + punctuation + U+2116/U+2122, `[8,256]` px, UTF-8 | `src/render/src/font.cpp:204-256`, `:340-440`; `src/render/include/gl/render/font.hpp:22-54` | `p3c-dialogs-sdb-font.md:27-43`; `font_tests` 8/8; `tests/render/test_font.cpp:110-256` | **partial** | Works, but the original ships `engineres/fonts/ttf/arial.ttf`, `console.ttf`, `pala.ttf` etc. and defines named fonts in `scripts/fonts.scr` (e.g. `arial_dialog`: Arial 9, weight 600). The port ignores `fonts.scr` and uses Segoe UI 20 px. |
| P3-16 | Text normalization and wrapping: tabs -> 4 spaces, NBSP -> space, C0/C1 ignored, greedy word wrap, long-word split | `src/render/src/font.cpp:54-72`, `:442-537` | `p3c-dialogs-sdb-font.md:30-34`; `tests/render/test_font.cpp:129-190` | **unverified vs original** | Greedy wrapping and control/NBSP rules are port choices; original GDI `CFont`/`CreateFontA` layout and line breaking are not extracted. |
| P3-17 | Font search/fallback: `--font` -> system font -> ASCII stb_easy_font (`?` for non-ASCII) | `src/render/src/font.cpp:539-562`; `tools/glgame/main.cpp:517-542` | `p3c-dialogs-sdb-font.md:35-43`; screenshot tests | **partial** | Windows-only; Android needs a bundled TTF. ASCII fallback loses all Cyrillic. Original font files are in `engineres.pak` but are not loaded. |
| P3-18 | cp1251 -> UTF-8 for CS strings, globals, SDB; UTF-8 decoder for text | `src/assets/src/cs.cpp:33-41`; `src/assets/src/sdb.cpp:24`; `src/script/src/globals.cpp:52-56`; `src/render/src/font.cpp:54-72` | `formats.md:7-10`; `p3c-dialogs-sdb-font.md:9-10`; audit real Cyrillic output | **verified** (standard mapping + real strings) | Undefined cp1251 bytes and invalid UTF-8 rely on fallback behaviour; not a known gameplay issue. |

## 4. Cross-cutting findings

1. **P2/P3 have no original-binary evidence.** P2 research is limited to shipped
   INI/saves and port code; P3 VM research names `GoldenLandEditor` as its source.
   Only P4 research contains `Server.dll` RVAs and live-memory facts. Under D-029,
   P2/P3 semantics are therefore `partial`/`unverified`, not `verified`.
2. **The original console is much larger.** `Client.dll` contains 137 command-like
   strings; the port has 9 commands. `r_resolution` is absent from the target
   `GoldenLand.ini` even though the string exists in the binaries.
3. **Timing mismatch is concrete.** P2 uses fixed 60 Hz; the only known original
   constant is `sv_fps=35` (`p4b-person-movement.md:100-104`), and Person update
   frequency is explicitly unpinned. P2b's determinism tests prove port
   determinism, not original cadence.
4. **Save format is header-only.** The 112-byte header and cp1251 titles are
   verified on 28 real files, but unknown fields are ignored, slot limits are
   unknown, and the payload is never read or written. `game_save`/`game_load`
   exist as original strings but are stubs/absent in the port.
5. **The original font system is unused.** `scripts/fonts.scr` names
   `arial_dialog` (Arial 9/600), `main_interface` (Palatino 12), `console`
   (Lucida Console 10), and `engineres/fonts/ttf/*.ttf` are shipped. The port
   uses a system Segoe UI at 20 px and its own greedy wrap.
6. **Native stubs dominate real scripts.** Of 35,356 `func` calls in 555 real
   `.cs`, 27,917 are `D_*` dialog calls handled by the VM; the remaining 7,439
   (plus 1,154 `D_PlaySound`) go to stubs returning 0. Function id `0` is used
   51 times with meaningful arguments but is mapped to `unknown`.
7. **Test pins are weaker than the docs.** Examples: `test_real_data.cpp:119-126`
   checks `decoded > 0`, not the 315/12040 counts; `test_real_saves.cpp:36-39`
   checks year/month but not day/size; `test_real_dialogs.cpp` executes only one
   dialog; `font_tests` depend on a system font, not the game's fonts.
8. **Hash labels are wrong.** `00-game-build.md` gives MD5; two P4 research files
   call the same MD5 a SHA-256. The computed SHA-256 values are in §1 above.

## 5. Priority gaps

1. **blocks parity** — All P3 VM semantics come from `GoldenLandEditor`, not
   `Server.dll`/`Client.dll` (`src/script/src/script_vm.cpp:203-688`).
2. **blocks parity** — Native functions are stubs returning 0; 7,439 non-dialog
   calls plus 1,154 `D_PlaySound` in 555 real scripts do nothing; id `0` is used
   51 times but is `unknown` (`src/script/src/host.cpp:17-39`,
   `src/assets/src/cs.cpp:166-168`).
3. **blocks parity** — Fixed 60 Hz tick is a port design; original `sv_fps` is 35
   and update cadence is unpinned (`src/core/include/gl/core/server.hpp:8-11`,
   `docs/research/p4b-person-movement.md:98-104`).
4. **blocks parity** — Save payload/load and `game_save`/`game_load` are not
   implemented (`src/core/src/save_manager.cpp:34-59`,
   `tools/glgame/main.cpp:884-892,1403-1406`).
5. **blocks parity** — Save slot limit, ordering and unknown header fields are
   not established; any `slot<N>` is accepted and unknown fields are ignored
   (`src/core/src/save_manager.cpp:62-123`, `src/assets/src/save.cpp:31-46`).
6. **minor** — Config/console semantics are unverified against the original;
   target INI has 4 setting keys (+43 binds), the original has 137 command-like
   strings, the port implements 9 (`src/core/src/config.cpp:21-70`,
   `tools/glgame/main.cpp:802-893`).
7. **minor** — App state flow is a skeleton; original boot/logo/loading/save-menu
   states are not reconstructed (`src/core/src/state.cpp:6-27`).
8. **minor** — Save UI is custom, not the original `save_load_menu.scr`/slots
   assets; no quick save/load (`tools/glgame/main.cpp:1519-1582`).
9. **minor** — VM limits/errors/coercion differ deliberately (10M vs 10k steps,
   100k eval budget, sticky errors, receiver coercion) and are not
   original-verified (`src/script/include/gl/script/script_vm.hpp:38-40`,
   `src/script/src/script_vm.cpp:678-688`).
10. **minor** — Dialog flow deviations: 0-based answers vs editor indexing,
    `D_CloseDialog` ending, first `D_Say` only (`src/script/src/script_vm.cpp:163-201,443-456`).
11. **minor** — Opcodes 1/3/4/5/9/12/13/18/19/20 never occur in 555 real `.cs`;
    bitwise/unary are synthetic-only (`src/script/src/script_vm.cpp:458-584`).
12. **minor** — SDB counts (315/12040) are not pinned by tests and duplicate ids
    are unspecified (`src/assets/src/sdb.cpp:10-25`,
    `tests/assets/test_real_data.cpp:100-103`).
13. **minor** — Font parity: original bundled TTF + `scripts/fonts.scr` are
    ignored; port uses Segoe UI 20 px and its own wrap
    (`src/render/src/font.cpp:204-256,442-562`).
14. **minor** — Real-data P3 coverage is narrow: one dialog, one globals file;
    no all-script VM run (`tests/script/test_real_dialogs.cpp:22-70`,
    `tests/script/test_real_dialog_session.cpp:61-114`).
15. **minor** — Evidence integrity: P2/P3 research has no binary hashes/RVAs;
    MD5 is mislabelled as SHA-256 and `GoldenLand.ini` has no documented hash
    (`docs/research/00-game-build.md:7-21`,
    `docs/research/p4b-person-movement.md:3-5`).

## 6. What was not checked

- No new Ghidra/IDA session and no live-memory reading of `Server.dll`,
  `Client.dll` or `GoldenLand.exe` were performed for this audit.
- The game was not run; no visual or behavioural oracle comparison was made.
- Full save payload decoding, original console command semantics, original
  dialog UI layout and original font metrics remain unextracted.
- Only the base `scripts.pak` corpus (555 `.cs`) was scanned for opcode/function
  distribution; `scripts.update.3.pak` versions were not aggregated.
