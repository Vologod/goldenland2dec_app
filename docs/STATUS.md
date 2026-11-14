# STATUS

Обновлено: 2026-11-13

## P2-15b1a — Save entities parser (in progress 2026-11-14)

- **Задача:** parse BT_ENTS (types/bodies), BT_PNAME, BT_RGINF во всех
  tail-секциях; `--dump-save` counts; без restore.
- **Spec:** `docs/specs/2026-11-14-p2-15b1a-entities-parser-design.md`.
- **Plan:** `docs/plans/2026-11-14-p2-15b1a-entities-parser.md`.
- **Критерии:** 2599/2599 exact; histogram; PNAME==type2; malformed errors.
- **Статус:** `in progress`.

## P2-15b1 — Save entities/persons research (done 2026-11-13)

- **Задача:** BT_ENTS class map/body layout, BT_RGINF/BT_PNAME, restore
  persons; docs-only.
- **Spec:** `docs/specs/2026-11-13-p2-15b1-entities-research-design.md`.
- **Plan:** `docs/plans/2026-11-13-p2-15b1-entities-research.md`.
- **Результат (research `p2-15b1-save-entities.md`, verdict `scoped`):**
  - Типы `BT_ENTS`: `{2: 14507, 8: 354, 16: 12090, 32: 2591}`, terminator
    `type 1`; type 4 (`4 & 0x3a == 0`) никогда не сериализуется, `+0`/`+4` —
    no-op stubs. Class map: `2=EF_PERSON`, `8=EF_INVCONTAINER`,
    `16=EF_TRIGGER`, `32=EF_DOOR`; фабрика `FUN_1405c83c`, readers
    `FUN_1401e758`/`0x24928`/`FUN_140233d4`/`FUN_140242f4`.
  - Body sizes из readers (self-delimiting): type 2 = `540 + sum(n) + M1+M2+M3 +
    0x14*c`; type 8 = `8 + M`; type 16 = `32 + sum(n) + M`; type 32 =
    `28 + sum(n)`; `M = 4` при нулевом флаге, иначе `4 + 4 + sum(0xc0+4+0x14*m)`.
  - Parse 2599/2599 секций (28/28 save): terminator последний,
    consumed == declared, 0 mismatches. `BT_PNAME` count == type-2 count
    2599/2599.
  - `BT_RGINF` = `u32` latch + `0x20` game datetime (8×u32); latch
    `+0x26c0`, datetime `+0x26a0`; решает только регенерацию NPC/trigger
    инвентарей (`FUN_1404b1c0`/trigger-путь), не поля entity.
  - Restore-порядок: dictionary → `BT_INFO`/clock → SEF (entrances/cell groups)
    → `BT_RGINF` → `BT_ENTS` (slot alloc `FUN_14003718`, не `FUN_140368d4`) →
    `BT_VARS` → `BT_TRIBE` → `BT_DIARY` → `BT_HERO` (+attached) → `BT_HSTRY`.
- **Unknowns:** семантика ~15+ полей person-body (`+0x40`/`+0x44`, `+0x5f8`/
  `+0x5fc`, `+0x654`/`+0x658`, `+0x151c`/`+0x1520`/`+0x1524`, `+0x2668`/
  `+0x266c`, `+0x6cc`, `+0xff0`, `+0x1630`, `FUN_1401d8bc` tail); item-record
  `0xC0` offsets доказаны только для `BT_HERO`; producer persisted RGINF
  latch `1`; origin-level prefix `BT_PNAME`.
- **Декомпозиция реализации:** `P2-15b1a` — parser+type map (приёмка:
  exact consume 2599/2599, PNAME count, RGINF 36 B, real 28-save tests);
  `P2-15b1b` — person restore (порядок SEF→RGINF→ENTS, name/tile/direction +
  containers, честный RGINF latch, unverified поля не заявляются; inventory
  parity ждёт entity-record offset + container-map oracle). `BT_RGINF`/
  `BT_PNAME` входят в `P2-15b1a`; отдельного `P2-15b1c` нет.
- **Out of scope:** `BT_DIARY`/`BT_HSTRY`/`BT_VARS` values/`BT_TRIBE`/
  `BT_SCRN`, save write (P2-15c), type-4 restore.
- **Next:** начать `P2-15b1a` (production entity/`BT_RGINF`/`BT_PNAME` parser +
  real-data tests) по spec/plan `P2-15b1`.
- **Проверка:** `docs/research/p2-15b1-save-entities.md` §Verdict/decomposition
  (acceptance `P2-15b1a`/`P2-15b1b`); durable oracle — real-data тесты `b1a`
  на 28 save (2599/2599 exact consume).
- **Статус:** `done` (research-only; реализация — отдельные spec/plan).

## P2-9a/b — 35 Hz tick implementation (done 2026-11-13)

- **Задача:** ServerCore 28 ms gate без catch-up/clamp; LevelSimulation один
  шаг на тик; headless 16 ms schedule; oracle refresh.
- **Spec:** `docs/specs/2026-11-13-p2-9-35hz-implementation-design.md` —
  `implemented (2026-11-13)`.
- **Plan:** `docs/plans/2026-11-13-p2-9-35hz-implementation.md`.
- **Сделано (Tasks 1–3, D-068):** `ServerCore` — integer-ms wall clock
  (`clock_ms_`) + отдельный simulation clock (`simulation_clock_ms_`),
  `kDefaultSvFps=35`, `kDefaultSvGameSpeed=0`,
  `kTickIntervalMs=28`, `kNominalTickHz=35`, `tick_interval_ms()`
  (`35→28`, `0/<0/>1000→0` gate skipped, `1000→1`); gate
  `simulation_clock_ms_-last_ms_>=28`, ровно один тик за `tick(dt)`,
  `simulation_time_ms` = latch simulation clock; paused-кадры двигают только
  wall `clock_ms_` (`simulation_clock_ms_` заморожен), поэтому паузное wall-время
  не попадает в тики и resume delta = только новый кадр; удалены accumulator/
  `kFixedDt`/`kMaxFrameDt`/`kMaxStepsPerTick`. `LevelSimulation::advance` — ровно один шаг,
  `time_ms_ += llround(dt*1000)`, движение `speed*delta_ms/1000`; удалены
  `kFixedStep`/accumulator/`time_ms_exact_`; STAY_ROTATE turn delay =
  `max(1, ((go_delay_ms-17)/3)*2)` (C2); `0.8` movement factor остаётся
  (U-C1 follow-up). Headless `glgame`/`glview` — declared 16 ms frame grid,
  без per-frame draw (`LevelScreen::advance_simulation`); interactive
  250 ms clamp удалён; логи `headless sim: frames=1250 ticks=625
  simulation_ms=20000` (20 s) и `glview sim: frames=625 ticks=312
  simulation_ms=9984` (10 s).
- **Проверка (2026-11-13, `GL_GAME_DIR=I:\Games\Златогорье-2`):**
  `cmake --build build` exit 0; полный
  `ctest --test-dir build --output-on-failure` — **19/19 passed**,
  `Total Test time (real) = 1468.89 sec`, exit 0. Fix-wave focused:
  `gl_core_tests` 70/70 (pause-регрессия: 1000 ms паузы -> +32 ms после
  resume), `gl_game_tests` 1/1, `glgame_screenshot_tests` `*save*` 9/9 и
  `*move*` 1/1, `glview_screenshot_tests` `*move*`/`*animation*` 5/5.
- **Остаточные ограничения:** U1 — host frame time/эффективный период
  (`F*ceil(28/F)` при `F<28`, `F` при `F>=28`) не измерены; pause/gate
  interaction — paused wall time excluded (port choice), оригинальная
  pause-семантика unverified (OQ-T4-3); U-C4 — per-frame animation
  catch-up abstraction (одно animation-обновление на выполненный тик);
  movement `0.8` vs additive `-17` (U-C1) не менялся; STAY_ROTATE turn
  delay `max(1, ((go_delay_ms-17)/3)*2)` с 1 ms floor при отсутствии `_go`
  данных; config/console plumbing `sv_fps`/`sv_game_speed`/`gv_anim_speed`
  отсутствует (fixed defaults); `sv_fps != 35` (U5) не покрыт; AI/restock
  per-tick cadence (U-C5/U-C2) — conditional. Parity P2-9 остаётся
  `partial` (gate implemented/verified, но U1/pause не закрыты).
- **Дальше:** опционально U-C1 (`anim_word0` identity) и U1 live frame-time
  capture; затем P5+ системы / P6 / P7.

## P2-9 — 35 Hz tick research (done 2026-11-13)

- **Задача:** доказать точную сетку тика оригинала (`sv_fps=35`),
  потребителей шага и scope перевода порта с 60 Гц; docs-only.
- **Spec:** `docs/specs/2026-11-12-p2-9-35hz-research-design.md`.
- **Plan:** `docs/plans/2026-11-12-p2-9-35hz-research.md`.
- **Research:** `docs/research/p2-9-35hz.md` (verdict
  `implementable` (scoped)).
- **Сделано (Tasks 1–5):** evidence `sv_fps`/`sv_game_speed`
  (registration/defaults, RVA/lines); tick model — host gate
  `GoldenLand.exe` RVA `0x184ac`, `interval = trunc(1000/sv_fps)` =
  **28 ms** at 35, one gate per host frame, `last_ms` = tick-start sample,
  no accumulator/catch-up, no max-frame clamp; QPC-derived absolute
  integer-ms clock (`0x46d800`/`0x408ff0`) latched в `DAT_14088e1c`;
  consumers table (movement/pause/spawn/script/AI) — ms deadlines; AI
  think/see — per invocation; per-tick только если подтвердится U-C5,
  restock cadence unresolved (U-C2); port inventory P1–P28 и impact matrix A–D;
  verdict/decomposition P2-9a/b (+ conditional P2-9c).
- **Доказано:** эффективный период frame-quantized:
  `F*ceil(28/F)` при `F < 28 ms`, `F` при `F >= 28 ms`; ровно 28 ms —
  только если сетка кадров попадает на 28; непрерывных 35.0 Гц нет.
  `sv_game_speed` — additive ms к animation delay, live 0. D-033
  consistent, но не независимое доказательство; решает asm truncation.
- **Unknowns:** U1 — host frame time/блокировки (vsync) не измерены;
  U-C1 — identity `anim_word0`; U-C3 — client animation interpolation;
  U-C4 — multi-frame catch-up (влияет на one-pass choice в P2-9a);
  U-C5 — per-tick связь entity `vtable+0xc` → AI think/see;
  U-C2 — restock cadence; OQ-T4-3 — pause/gate interaction;
  command dispatch per frame vs per tick; U2/U3 (wrap/float) и
  `sv_fps != 35` — out of scope.
- **Проверка:** docs-only; `src/`, `tests/`, `tools/` не менялись;
  encoding UTF-8 no BOM/NUL; `git diff --check`; spec/plan/research в
  одном docs-коммите.
- **Статус:** `done` (research); реализация — **P2-9a/P2-9b done
  2026-11-13 (D-068)**. P2-9c не открыт (animations/timers остаются
  ms-driven; AI/restock — conditional).
- **Дальше:** residual U1 (host frame time), pause/gate (OQ-T4-3), U-C1
  (`anim_word0`), config plumbing; затем P5+/P6/P7. Implementation evidence
  — `docs/research/p2-9-35hz.md` §Implementation result.

## P2-15b — Save load, first increment (done 2026-11-12)

- **Задача:** декодировать `.sav` payload, загрузить уровень по `BT_INFO` +
  hero (P5a), Continue/`--load-save`; без записи.
- **Spec:** `docs/specs/2026-11-12-p2-15b-save-load-design.md` —
  `implemented (first increment)`.
- **Plan:** `docs/plans/2026-11-12-p2-15b-save-load.md`.
- **Сделано (Tasks 1–6):** `gl::assets::SavePayload` — 7 блоков tag+size;
  `BT_INFO` (level/party level/time), `BT_VARS` (types/names/raw values),
  tail-словарь (`level -> section`) с raw `BT_RGINF`/`BT_ENTS`, тело `BT_HERO`
  (P5a stats/inventory/equipment/outfit); `BT_SCRN`/`BT_DIARY`/`BT_HSTRY`/`BT_TRIBE`
  пропускаются; `glgame --dump-save <slot>` печатает decoded counts;
  `--load-save <slot>` и Continue загружают уровень из save (SEF) и применяют
  hero; при ошибке загрузки уровня — inventory runtime восстанавливается из
  snapshot (atomic rollback), NPC-контейнеры генерируются после успешной загрузки.
- **Доказательства:** real decode `28/28`; slot0 `L93_4` vars=1002 tail=213
  section=8551 rginf=36 ents=8381 hero_items=117 hero_equipped=11; slot2 `L6_3`;
  live `--load-save 0` — `save: loaded slot=0 level=L93_4 vars=1002 tail=213`,
  `inventory: slot 0`, `outfit: slot 0`; полный `ctest` 19/19 passed
  (1471.40 s, exit 0, 2026-11-12).
- **Fix wave (2026-11-12):** rollback E2E (`save: rollback slot=0`), new
  malformed-payload тесты (truncated body, wrong order, trailing bytes),
  `raw_len <= kMaxEntrySize` + `std::length_error`; полный `ctest` 19/19
  passed (1569.37 s, exit 0); `gl_assets_tests` 227/227,
  `glgame_screenshot_tests --test-case="*save*"` 9/9, `gl_game_tests`
  332/332.
- **Остаточные ограничения / deferred minors:** `BT_DIARY`/`BT_HSTRY`
  (квесты/лог), именованные поля `BT_ENTS` (raw, не split, OQ-12), значения
  `BT_VARS`, семантика `BT_TRIBE`, сохранение (write) — вне scope (full
  P2-15b/P2-15c). Failure-path rollback теперь test-backed: E2E проверяет
  `save: rollback slot=0` вместе с `cannot load level` и exit 3; дублирующий
  `rebuild_person_runtime` в `load_save` удалён; `raw_len` ограничен
  `kMaxEntrySize` (+ `std::length_error`); неиспользуемый `last_save_payload_`
  удалён. Остаются: copy-cost `InventoryRuntime` на успехе; pinned NPC
  name/item в E2E привязан к real slot0.
- **Дальше:** full P2-15b (diary/hstry, `BT_ENTS` named fields, `BT_VARS`
  values, `BT_TRIBE`) и P2-15c (write/`game_save`, round-trip).
## P2-15a — Save payload research (done 2026-11-12)

- **Задача:** research-only разбор `.sav` payload после 112 байт: zlib,
  каталог блоков, reader/writer `Server.dll`, state domains, декомпозиция
  P2-15b/P2-15c.
- **Spec:** `docs/specs/2026-11-12-p2-15a-save-payload-research-design.md`.
- **Plan:** `docs/plans/2026-11-12-p2-15a-save-payload-research.md`.
- **Research:** `docs/research/p2-15a-save-payload.md` (verdict
  `implementable`).
- **Доказано:** payload = zlib от `raw[112:]`; 7 блоков (`BT_SCRN `/
  `BT_INFO `/`BT_VARS `/`BT_TRIBE`/`BT_DIARY`/`BT_HSTRY`/`BT_HERO `),
  один порядок 28/28, конец = decompressed length; writer `FUN_14031b64`
  RVA `0x31b64` (per-block calls 35384-35422), reader `FUN_1403210c`
  RVA `0x3210c` (35628-35686) + `FUN_14001668` framing (`raw[0x68]` =
  uncompressed, `raw[0x6c]` = compressed); tail-словарь уровней
  (`BT_RGINF`/`BT_ENTS`, 213 entry в slot0); `BT_SCRN ` = client-only
  preview 500×375×4 (`750008 = 8 + 500*375*4`), server его не читает;
  domains: `BT_VARS ` globals, `BT_TRIBE` 234×234 matrix, `BT_DIARY`
  journal, `BT_HSTRY` 40-slot log, `BT_HERO ` hero+inventory. 13 Open
  Questions (OQ-1…OQ-13) с точными decompile-шагами; live diff не нужен.
- **Команды:** Python scan 28 save; Ghidra/decompiled.c refs; encoding check.
- **Статус:** `done`; write/load остаются `absent` до P2-15b/P2-15c.
- **Дальше:** `P2-15b` (read-only load; core subset first: header/inflate →
  tail dict → `BT_INFO ` → SEF+`BT_RGINF`/`BT_ENTS` → `BT_VARS `
  names/count → `BT_HERO `; затем `BT_TRIBE` → `BT_DIARY` → `BT_HSTRY`).
  First-increment acceptance: 0/N level, raw `BT_ENTS` bytes, globals
  names/count, hero inventory/equipment/stats; deterministic. Full
  acceptance requires OQ-5/OQ-10/OQ-11/OQ-12 (+OQ-9 for log) and quest/log
  checks. Затем `P2-15c` (write/`game_save`, round-trip write→read→compare).
## Phase 1b2b2e Stage 4 — Live price x87 CW (done 2026-11-12)

- **Задача:** снять live `FloatSave.ControlWord` у `Client.dll+0x6e8d0`
  (`FUN_1206E8D0`, цена предмета) и закрыть FPU price-гейт.
- **Spec/evidence:** `docs/specs/2026-11-11-p3b2b2e-specials-live-parity-design.md`
  Stage 4; research `p3b2b2e-live-parity-stage2b.md` §4/§10; D-062/D-063/D-065.
- **Критерии приёмки:** `price=<cw>` снят на thread вызова цены;
  ожидается `0x007F` (PC=24/RC=nearest); `special gates: live original FPU
  control word` PASS с `GL_SPECIALS_FPU_CW` (generation+price).
- **Команды проверки:** `tools/live/gl_live_capture.exe --help`;
  `gl_live_capture.exe --pid <pid> --price-cw --out .local/live/price-cw.txt`;
  `GL_SPECIALS_FPU_CW=<file> build\bin\gl_game_tests.exe
  --test-case="*live original FPU control word*" --success`.
- **Проверка (2026-11-12):** user-assisted capture на запущенной игре
  (`GoldenLand.exe` PID 21648, `Client.dll` base `0x12000000`, DR0
  `0x1206e8d0`): `.local/live/price-cw.txt` — `price=0x007f`,
  `# source=client-price`, thread 6084; SHA-256
  `EDB81608D877C684AE648E63AC0844BAC7CEE997FBFEF61B9DFECF6AD2AEB3FF`.
  FPU-гейт: `1/1` test case, `7/7` assertions, `SUCCESS!` с
  `GL_SPECIALS_FPU_CW` (`generation=0x007F`, `price=0x007F`).
- **Статус:** `done`; live price CW равен generation CW `0x007F`
  (PC=24/RC=nearest), Stage 4 закрыт.
## Phase 1b2b2f — Per-person pre-loot RNG (done 2026-11-11)

- **Задача:** потреблять weapon (`FUN_1401d99c`) + spawn
  (`FUN_14021bc8`) RNG-draw в SEF-порядке после loot каждого персонажа.
- **Spec:** `docs/specs/2026-11-11-p3b2b2f-person-rng-design.md`.
- **Plan:** `docs/plans/2026-11-11-p3b2b2f-person-rng.md`.
- **Сделано (Tasks 1–6):** `parse_person_weapon_block` →
  `PersonDef.weapons/weapon_level_offset/has_weapon`; `game::consume_person_init_rng`
  (`coin(0.5)` по eligible `weapon`-строкам в файловом порядке с wrap, затем
  `range(3000,10000)` spawn-init); `generate_npc_inventories` потребляет для
  каждой не-Hero `level.persons` (пустой инвентарь и дубликаты включены);
  live-гейт `test_special_gates.cpp` (first-wins дубликаты, RNG на каждый
  SEF-entry); deterministic seed-1 оракулы пересчитаны независимым
  Python-референсом, валидированным 0/N на live capture (Task 5).
- **Критерии приёмки:** live l1_1 oracle
  `mismatched_items==0 && mismatched_mods==0`; deterministic seed-1 оракулы
  пересчитаны независимым reference; полный `ctest` с `GL_GAME_DIR`.
- **Команды проверки:** `cmake --build build`;
  `build\bin\gl_game_tests.exe --test-case="*person_rng*,*npc_loot*"`;
  `GL_SPECIALS_LIVE=<capture> build\bin\gl_game_tests.exe
  --test-case="*live NPC-mod oracle*" --success`;
  `ctest --test-dir build --output-on-failure`.
- **Проверка (2026-11-11):** live l1_1 gate — `items captured=22 port=22
  matched=22 mismatched=0; mods captured=14 port=14 matched=14 mismatched=0`
  (14/15 unique persons compared; reduced-scope evidence). `cmake --build
  build` — up to date, exit 0. Full `ctest --test-dir build
  --output-on-failure` (`GL_GAME_DIR=I:\Games\Златогорье-2`) —
  `100% tests passed out of 19`, `Total Test time (real) = 1266.69 sec`,
  exit 0.
- **Residual limitations:** материализация оружия/экипировки, хранение
  `spawn_roll` в состоянии, Hero spawn и пост-генерационный RNG — вне scope;
  persons, пропущенные загрузчиком/без ресурсов, не потребляют RNG.
- **Follow-up (minor, deferred):** детерминированный `run_level` оракул в
  `tests/game/test_npc_loot_specials.cpp` — generator-scope: вызывает
  `NpcLootGenerator::generate` напрямую и намеренно не потребляет
  person-init draws. Production stream parity покрыт
  `test_inventory_runtime_real.cpp` и live-гейтом; переписывать этот оракул
  — отдельная задача.
- **Статус:** `done`.
## Phase 1b2b2e — Specials live parity (regression/docs done 2026-11-11; live gates closed 2026-11-12: NPC oracle by 1b2b2f/D-064, price CW by D-065)

- **Задача:** закрыть live-гейты specials (`1b2b2d` был `partial`):
  provenance live-захвата, порядок/scope RNG, inherited ITM effects,
  precision-модель, финальный 0/N на `l1_1`.
- **Spec:** `docs/specs/2026-11-11-p3b2b2e-specials-live-parity-design.md`.
- **Plan:** `docs/plans/2026-11-11-p3b2b2e-specials-live-parity.md`.
- **Стадии:** Stage 0 tool correctness (map-резолв + `--scan-only`);
  Stage 1 provenance (call-order, LCG double-draw, `loot_level` из
  `BT_INFO+0x26c4`); Stage 2 модель по evidence (порядок persons,
  inherited effects, merge при необходимости); Stage 3 precision
  (binary32/PC=24 для specials и `TradePrice`); Stage 4 live-приёмка
  (NPC 0/N + CW у `FUN_1206E8D0`).
- **Сделано (Tasks 0–5):** Stage 0 — tool map-резолв + `--scan-only`
  (без ложного capture); Stage 1 — provenance: call-order trace (RF после
  первого draw), LCG double-draw, `loot_level` из `BT_INFO+0x26c4`
  (source=memory); Stage 2a — inherited ITM effects копируются в runtime
  mods (`id=type`, `value=int32(amount)`, `flags`) и
  `mod_count = effects.size() + generated`; live-гейт их сравнивает;
  Stage 3 — specials и `TradePrice` переведены на binary32
  (`assume_x87_single`, live CW
  `0x007F` = PC=24/RC=nearest). Stage 2b (merge/window) и Stage 4 (price
  CW) не входили в этот plan.
- **Доказано при design:** live item flags = raw ITM flags (маска не
  нужна: `812`=`0xC0000000`, `372`=`0xC0000284`, `933`=`0x8000000C`);
  inherited mods = ITM effects (`933` `7/1/0x50`+`8/-1/0x50`, `418`
  `27/5/0x50`); container map — `FUN_1404bf8c` → `FUN_14004f14(manager+0xc)`
  → `FUN_14004a20` (count `+0`, entries `+4`, stride 8); `level->persons`
  уже в SEF-порядке.
- **Live evidence (2026-11-11):** capture `.local/live/special-l1_1-trace.txt`
  (`rng_state=0xad5bd7cd`, `loot_level=2` source=memory, 16 calls).
  Live call order совпал с port SEF order **1:1**, включая дубль `P744`
  (порядок/scope не причина расхождения). Gate:
  `items captured=22 port=29 matched=1 mismatched=49; mods captured=14
  port=7 matched=0 mismatched=21`. Inherited-effects модель подтверждена:
  ITM `420` 6 effects = 6 live mods, ITM `938` 2 effects = 2 live mods.
  `rng_second check=fail` — tool artifact (повторный DR1 до первого draw),
  не признак плохого RNG; tool исправлен. Гипотезы (unverified): merge
  стаков (`FUN_1404e3a4`), loot window/quantity (money 139 > 50..100),
  поздняя RNG-дивергенция у `P678/P693`. Evidence:
  `docs/research/p3b2b2c-npc-loot-specials.md` §11.
- **Критерии приёмки:** live NPC oracle `mismatched_items==0 &&
  mismatched_mods==0` на `l1_1`; FPU-гейт на измеренных generation/price
  CW (PC=24, RC=nearest); полный `ctest` с `GL_GAME_DIR`; docs/parity.
- **Команды проверки:** i686-сборка tool + `--help`; `cmake --build
  build`; `GL_SPECIALS_LIVE=<capture> GL_SPECIALS_FPU_CW=<file>
  build\bin\gl_game_tests.exe --test-case="*special*gate*" --success`;
  `ctest --test-dir build --output-on-failure`.
- **Проверка (2026-11-11, HEAD `9962d38`):** `cmake --build build` — up to
  date, exit 0; `gl_game_tests.exe` — 313/313 passed, 2 skipped, 560305
  assertions, exit 0; `trade_overlay_tests.exe` — 25/25, 669 assertions,
  exit 0; live gate с `GL_SPECIALS_LIVE` — ожидаемый `partial` fail:
  `items captured=22 port=29 matched=1 mismatched=49; mods captured=14
  port=7 matched=0 mismatched=21` (6 failed assertions в live-oracle
  test-case); FPU-CW gate skipped без `GL_SPECIALS_FPU_CW`. Полный
  `ctest --test-dir build --output-on-failure` (`GL_GAME_DIR=I:\Games\Златогорье-2`) —
  `100% tests passed out of 19`, `Total Test time (real) = 1991.87 sec`, exit 0.
- **Final review fix wave (2026-11-11, base `aeac96d`):** `TradePrice`
  использует double-константы/операнды с округлением каждого результата до
  binary32 (live `0x007F`): boundary `5/100/skill 9` → buy 8/sell 3,
  новый durability-кейс `75/36/skill 1` → 151/26; boat-trader оракулы без
  изменений. FPU-гейт: reference `0x007F` (PC=24/RC=nearest), price-половина
  остаётся Stage 4. Live-гейт отвергает `rng_second ... check=fail`
  (pre-fix `special-l1_1-trace.txt` падает на этой проверке и остаётся
  pre-fix evidence). Real-data lookups передают ITM `effects`; аналитический
  NPC-oracle отделяет inherited effects от документированных 12 generated
  mods / 9 events. Focused runs: `gl_game_tests` full **314/314** (2 skipped,
  560309 assertions), `*trade_price*` **16/16** (49 assertions),
  `trade_overlay_tests` **23/23** (2 skipped, 610 assertions); live gate
  **7/8** (FPU pass; live oracle fails `check=fail`); i686 tool rebuild 0
  warnings, `--help` exit 0.
- **Final verification after fix wave (2026-11-11, HEAD `593be52`):**
  `cmake --build build` exit 0; full
  `ctest --test-dir build --output-on-failure` (`GL_GAME_DIR=...`) —
  `100% tests passed out of 19`, `Total Test time (real) = 2175.64 sec`,
  exit 0. Final scoped re-review: все Critical/Important закрыты; deferred
  minors: `tools/live/README.md` всё ещё говорит, что все `#`-комментарии
  игнорируются (но `rng_second check=` парсится), и DR0 `RW0`-биты
  (16–17) не очищаются в `ensure_thread` (маска `0x00FF00FF` была бы
  безопаснее). Live oracle остаётся `partial` (pre-fix capture `check=fail`;
  Stage 2b/4 — follow-up).
- **Part 2 (merge + RNG phase + price CW, 2026-11-11):** merge/stacking
  `FUN_1404e3a4` реализован: per-container, same item id, `max_stack>=2`,
  `count<max_stack`; caller **overwrites** count с текущим chunk (не sum),
  RNG не потребляется. Полный `ctest` после merge — **19/19 passed**,
  2068.71 s, exit 0; app-level trade oracles обновлены под 14-item NPC stock
  (`12836`/`15197` money). Live draw-trace (новый старт игры,
  `.local/live/special-start-draws5.txt`, `check=ok`, 8 calls) нашёл источник
  переменной фазы: `FUN_1401d99c` — per-person random initial weapon
  (`coin(0.5)` до успеха, 1–7 draw) и `FUN_14021bc8` — spawn init
  (`range(+0x5e4,+0x5e8)`, 1 draw), между loot-вызовами. Порт эти системы не
  моделирует → loot-stream сдвинут на 2–8 draw/person; это Part 3
  (P4/P5 equipment/spawn RNG interleaving), не loot-баг. Tool: `--price-cw`
  (`Client.dll+0x6e8d0`, ambient CW) готов; live price CW снят 2026-11-12
  (`0x007F`, D-065).
  Evidence: `docs/research/p3b2b2e-live-parity-stage2b.md` §8/§10; D-063/D-065.
- **Open Questions (1b2b2e):**
  1. Точный порядок вызовов `FUN_1404b1c0` (SEF vs entity/spawn) —
     resolved 2026-11-11: live order = port SEF order 1:1.
  2. Нужен ли merge/stacking для 0/N на `l1_1` — resolved: да, реализован
     (overwrite per container).
  3. Live CW у `FUN_1206E8D0` и модель цены — tool `--price-cw` готов;
     live-значение ещё не снято (Stage 4).
  4. ~~Отсутствие generated mod `372`~~ — артефакт старого capture.
  5. Per-person initial weapon (`FUN_1401d99c`) и spawn init
     (`FUN_14021bc8`) RNG draws: реализовать в SEF-порядке (Part 3).
- **Статус:** Part 1 (Stage 0/1/2a/3) и Part 2 (merge, draw-trace, price-CW
  tool) реализованы; полный `ctest` 19/19. NPC oracle `0/N` не достигнут —
  **`partial`**: остаток вызван не loot-моделью, а per-person equipment/spawn
  RNG (Part 3) и непроверенным live price CW (Stage 4). Полный паритет не
  заявлен.
  **Update 2026-11-12:** Part 3 закрыт 1b2b2f/D-064 (live `0/N`); Stage 4
  закрыт D-065 (`price=0x007F`, FPU-гейт PASS). Live gates 1b2b2e закрыты;
  residual: reduced-scope 14/15 unique persons, `run_level` generator-scope,
  материализация экипировки/spawn.

## Phase 1b2b2d — NPC loot specials implementation (done 2026-11-10)

- **Задача:** `specials.scr` parser, `SpecialEngine` (R4–R10),
  интеграция в `NpcLootGenerator`, `InventoryItem.mods`, production-wiring
  и trade/price guard.
- **Spec:** `docs/specs/2026-11-10-p3b2b2d-specials-implementation-design.md`.
- **Plan:** `docs/plans/2026-11-10-p3b2b2d-specials-implementation.md`.
- **Реализация (Tasks 1–5):** parser `specials.scr` (58 defs/15 pools,
  value 3 + time 6 slots, cp1251); `SpecialEngine::apply` R6–R10 и
  `random_special_count` R4/R5 на общем `MsvcRandom`; `NpcLootGenerator`/
  `generate_npc_inventories` принимают `SpecialDatabase*`; `InventoryItem.mods`
  + `mod_count == mods.size()`. **Production:** `glgame` загружает
  `scripts/item_class_specials/specials.scr` (ошибка → warning +
  no-specials), передаёт DB в генерацию, `ItemTemplate::mod_count` берётся
  из `ITM effects.size()` во всех production/test lookup'ах (Task 5
  blocker закрыт); `--no-specials` → null DB для детерминированных
  no-specials тестов/скриншотов. `--dump-npc <name>` — headless dump
  main/trade items и mods.
- **Analytic regression oracle `l1_1` seed 1 level 1:** no-specials `174/218/323`;
  specials `174/287/0` (`P7` без изменений, `P389` `218→287`, `P711`
  `323→0` из-за downstream RNG); 18 кандидатов/9 срабатываний/12 модов,
  1057→1329 draw, final `0x48CAA370`; первый divergence — `P389` pass1
  (`ELB_1_04_1`, `nspec=0`). Production E2E: specials item
  `L1_2.P404_Sister_Of_Mercy2` `BRL_1_01_5` id 8, flags `0x20C`,
  `mod=25/-1/0x81`, `mod_count=1`; price/buy → `unsupported`
  (D-058 guard не менялся), no-specials — item отсутствует.
- **Гейты (OPEN, 2026-11-10):** `GL_SPECIALS_FPU_CW` (live CW до
  `FUN_14047284`/`FUN_1206E8D0`, ожидается `0x027F` PC=53/RC=nearest;
  **superseded by D-062:** live `0x007F`/PC=24) и
  `GL_SPECIALS_LIVE` (NPC-mod oracle v2, `rng_state` + item/mod
  multiset) не заданы → skip с причиной; live-захваты не сняты.
  `0x027F` остаётся extraction-assumption; build-машина `0x037F` — только
  diagnostic. Port обязан сохранять default MXCSR rounding (nearest):
  `TradePrice` чувствителен к non-nearest (2/3 режима). **Superseded by
  D-062 (1b2b2e):** live generation CW `0x007F` (PC=24) снят; ожидание
  `0x027F`/PC=53 — историческая extraction-предпосылка.
- **Проверка fix wave (`GL_GAME_DIR=I:\Games\Златогорье-2`):**
  `cmake --build build` exit 0. Focus reruns: `gl_game_tests` full
  **311/311** (2 skipped, 560289 assertions); `*special*gate*` **7/7**
  (134); `*special*` **21/21** (619, 292 skipped); specials E2E **1/1**
  (29); `glgame_screenshot_tests` full **36/36** (927). Full `ctest` —
  **19/19 passed**, `Total Test time (real) = 2280.38 sec`, exit 0.
- **Fix wave (inherited mods):** `LootItem`/`InventoryItem.mod_count =
  ItemTemplate.mod_count + mods.size()`; `append_items` инициализирует
  `mod_count` из template; transfer копирует `source.mods`; live-гейт
  исключает `effects>0` rows (Open Question ниже). Добавлены tests:
  inherited template (price guard), `chunk!=1`, missing `SpecialDef`,
  FPU-CW file fallback, synthetic inherited capture, transfer mods.
  **Superseded by D-062 (scope):** inherited effects теперь сравниваются,
  не исключаются.
- **Статус:** **`partial`**; full parity запрещён до live NPC-mod 0/N и
  live FPU-CW `0x027F`. Остаётся unverified: `random_sign` (место
  применения), item flag `0x40`, time-slots 4–6; save/сериализация mods —
  вне scope. **Open Question:** ITM prototype effects (`effects>0`)
  сохраняются count-only (`mod_count = template.mod_count + generated`,
  `mods` пуст) — value/flags/duration семантика не доказана; live-гейт
  исключает такие item/mod rows и сообщает skipped counts
  (`tools/live/README.md`). **Superseded by D-062:** inherited ITM effects
  копируются в `mods` (`id=type`, `value=int32(amount)`, `flags`) и
  сравниваются live-гейтом.
- **Коммиты Task 4:** `3b62fc8` + `ca4fdca` (`test(game): harden specials
  live gates`).
- **Коммиты Task 5:** `feat(tools): enable NPC specials` (production
  wiring, `mod_count`, `--no-specials`, E2E/guard tests) +
  `docs: record specials implementation` (D-060/parity/STATUS/PROJECT/plan)
  + `fix(game): preserve inherited mods and scope live gate`.
- **Final review (2026-11-10):** With fixes (inherited mod_count,
  append/transfer mods, live-gate scoping) → `3bef8c8`; scoped re-review
  clean. Ready to merge = Yes как `partial`; full parity — только после
  live FPU-CW/NPC-mod. Отложенные minors: vacuous 0/0 gate assertion,
  merge не копирует mods в существующий стек, helper unexercised.
- **Live gates (2026-11-10, BLOCKED/PARTIAL):** user-assisted capture
  (`.local/live/special-l1_1-entry.txt`, 32-bit read-only tracer,
  артефакт не коммитить): `level=l1_1`, `rng_state=0x71df3143`,
  `loot_level=2`, 21 item / 4 mod (P389 item 372 mod 28/2/0x41;
  P693 item 933 mods 7/1/0x50 и 8/-1/0x50).
  **Live x87 CW = `0x007F`** (generation-entry/first-draw/scan-time),
  не `0x027F` → single precision; gate `*special*gate*` падает на
  CW-check (127 vs 639). NPC-oracle: captured items=19 / port=31,
  matched=1, mismatched=48; mods captured=1 / port=0. Гипотезы (не
  подтверждены): single vs double precision; порядок/scope общего
  RNG-потока (port сортирует persons, live идёт в entity/SEF-порядке);
  inherited ITM `effects>0` вне scope. Полный паритет НЕ заявлен —
  статус **`partial`**; live-гейты остаются OPEN/BLOCKED. **Superseded by
  D-062 (1b2b2e):** precision = binary32/PC=24, порядок = SEF 1:1,
  inherited effects реализованы; NPC-oracle остаётся `partial`
  (1/49 items, 0/21 mods).
- **Next task (planned, отдельный подпроект):** live-parity разбор:
  (1) зафиксировать live CW `0x007F` и пересмотреть precision-модель
  specials/price; (2) модель порядка генерации persons;
  (3) scope inherited effects. До этого — не «дожать гейт» ослаблением.
  **Update:** выполнено в 1b2b2e/D-062; residual — Stage 2b/4.
- **Дальше:** отдельный подпроект live-parity (см. выше) либо
  `P2-15` (save payload), затем 35 Гц (`P2-9`).

## Phase 1b2b2c — NPC loot specials research (done 2026-11-10)

- **Задача:** доказать/опровергнуть воспроизводимость specials NPC-лута
  (`FUN_14047284`/`DAT_14088df4`, R4–R10). Research-only; production-код
  не менялся.
- **Spec:** `docs/specs/2026-11-10-p3b2b2c-npc-loot-specials-research-design.md`.
- **Plan:** `docs/plans/2026-11-10-p3b2b2c-npc-loot-specials-research.md`.
- **Research:** `docs/research/p3b2b2c-npc-loot-specials.md`.
- **Вердикт:** **`implementable` (scoped).** Gate снят: DB source —
  `scripts/item_class_specials/specials.scr` + hardcoded `IDSPEC/SPECF/T_*`
  в `GoldenLand.exe` (не `.itm`/`itemdescparts.sdb`/`magic.pak`);
  `DAT_14088df4 = *(host+0x84) = DAT_00469d54`; vtable
  `+4/+8/+0x14/+0x1c/+0x28` и запись def `0x98` (3 value + 6 time slots)
  восстановлены. `FUN_14047284`/`roll_special_count` (R4/R5) и полный
  RNG-порядок R6–R10 восстановлены; `ROUND` = trunc только на финальном
  `fistp` (RC=`0xC00`), `fval`/`fval*level` — под входящим CW
  (default `0x027F`: PC=53/RC=nearest; **superseded by D-062:** live
  `0x007F`/PC=24). Conditions:
  `assigned==1 && flags&4 && item+0x78==0`; R4/R5 лимиты
  3 (`flags&0x40==0`) / 9 (`flags&0x40!=0`); даже `nspec==0` потребляет R4;
  coin/range без `_rand` при `p<=0`/`p>1`/`hi<=lo`.
- **Реальные данные:** 1848 ITM → 522 modifier-capable (`type` в pool,
  `flags&4`, `effects==0`; все `max_stack==1`), 20 с `flags&0x80==0`
  (D-058 guard active), modifier-capable stackable 0; RNG candidates
  (`flags&4`, `effects==0`) — 533 = 522 + 11 type19/20 stackable с
  пустым pool (draw R4/R5, модов нет). `l1_1` seed1 level1 (analytic
  Python reference; level contract `v=int(fval*loot_level)`,
  self-check `fval=1,loot=1 -> 1`; без specials совпал с принятым
  oracle 174/218/323): 18 кандидатов, 9 срабатываний, 12 модов,
  1057→1329 draw (+272; special-path 275 = R4/R5 30 + R6 233 +
  R9/R10 12; downstream -3); первое RNG-расхождение —
  `Crying_Guardian` pass1 (`ELB_1_04_1`, `nspec=0`, 1 draw), первое
  изменение выдачи — его pass2; `P387/P678/Kolbasens` hot/main
  идентичны (Kolbasens pass2 `2×87` в обоих). Принятые no-specials
  значения `174/218/323` под true parity **superseded**.
- **D-058:** modifier `classMul/typeMul` — это price-поля той же
  `special_desc`-записи (`+0x74/+0x84/+0x94`, `+0x14…+0x64`); client
  `DAT_12110ed8` и server `DAT_14088df4` — один объект (host `+0x84`);
  price читает только `id/value/flags` (не `+0x0c/+0x10`; generated —
  0/0). Основной missing-gap `p3b2b4b` §8 п.2 закрыт структурно;
  остаётся live FPU-CW (PC=53/RC=nearest; **superseded by D-062:**
  generation CW `0x007F`/PC=24; live price CW снят `0x007F`, D-065) и реализация.
- **Не доказано live:** захват NPC-модов из запущенной игры; live
  FPU-CW **перед `FUN_14047284` (генерация) и `FUN_1206E8D0` (цена)**;
  `random_sign` (def+0x04) парсится, но не читается генерацией/ценой
  (unverified только место применения); семантика item flag `0x40`;
  time-slots 4–6 генерацией не читаются.
- **Acceptance next phase:** unit (DB/pool/R4–R10), deterministic
  `l1_1` seed1 level1 events §5.2 + final aggregates, live/differential
  gate (RNG-state + NPC `+0x10`/mod records, 0/N), live FPU-CW;
  обновить/пометить `superseded` no-specials oracle.
- **Коммиты:** `0e4402e..a920b22` (spec/plan, research, RNG-order/caveat
  fixes; verdict/handoff — `0af9ab7`; fix wording — `841bed7`) —
  **docs-only**, production-код не менялся; не запушено.
- **Следующее:** implementation spec/plan для specials (parser
  `specials.scr`, `FUN_14047284`, R4–R10 в обоих проходах, modifier
  price под PC=53; D-062: binary32/PC=24) **можно начинать сейчас**; live NPC-mod oracle и
  live FPU-CW — обязательные acceptance-гейты **до заявки на full
  parity**, не блокеры старта. Затем `P2-15`/35 Гц.

## Phase 1b2b4b — trade transactions (done 2026-11-09)

- **Итог (Tasks 1–5, done):** trade overlay интерактивный: `TradePrice`
  (buy/sell по research §4.3), whole-stack `buy_trade`/`sell_trade`,
  `exchange_trade` (net money, atomic errors), staging/Exchange UI и
  `glgame` wiring. Оригинальный flow — drag всего стека в buy staging
  (средний y=208) / sell staging (нижний y=478), Exchange obj 44
  (`lbig2`, 579,320) → net money; без отдельного confirm/quantity
  selector. Modifier guard `mod_count != 0 && (flags & 0x80) == 0` →
  `unsupported` для buy и sell; `gv_addon`/perks (`0x10/0x400`) не
   моделируются. Цена считается в `double` под явной `assume_pc53`
   (PC=53), TU с `-ffp-contract=off`. **Superseded by D-062:**
   binary32/`assume_x87_single` (PC=24).
- **Spec:** `docs/specs/2026-11-09-p3b2b4b-trade-transactions-design.md`.
- **Plan:** `docs/plans/2026-11-09-p3b2b4b-trade-transactions.md`.
- **Research:** `docs/research/p3b2b4b-trade-transactions.md`; D-058.
  - Сделка: `buy <= sell` → NPC платит `sell-buy`; `buy > sell` → Hero
    платит `buy-sell`; равно — no-op; предметы buy NPC trade → Hero
    main, sell Hero main → NPC trade, затем net money (`MON_1_0_1`);
    ошибки (`missing_person`/`missing_template`/`insufficient_stock`/
    `insufficient_money`/`unsupported_price`) без мутаций.
  - UI: отдельные buy/sell staging-ряды (per-row лимит 16, независимый
    scroll), net-total только x=562/y=386, NPC/Hero money y=126/612,
    типы 18/25 скрыты из source-рядов, ошибки рисуются; hooks
    `--trade-buy/--trade-sell/--trade-exchange`.
- **Проверка (`GL_GAME_DIR=I:\Games\Златогорье-2`):**
  `cmake --build build` exit 0; targeted `ctest -R "gl_script_tests|
  gl_script_real_tests|gl_game_tests|gl_assets_tests|
  gl_assets_real_tests|glgame_screenshot_tests|glview_screenshot_tests|
  level_click_tests|trade_overlay_tests"` **9/9 passed**, 3171.29 s,
  exit 0; full `ctest` **19/19 passed**, 2723.07 s, exit 0 (targeted и
  full мерялись в отдельных прогонах при разной нагрузке — времена не
  сравнимы напрямую).
  `glgame_screenshot_tests --test-case="*trade*"` **8/8** (164
  assertions, 27 skipped); `trade_overlay_tests` **25/25** (669);
  `gl_game_tests --test-case="*trade*"` **59/59** (935, 230 skipped);
  full `gl_game_tests` **289/289** (559623).
- **Oracle / screenshots:** slot0 buy action 16800 / sell action 10608
  (51×208; unit 1200/208; 14-стек stage-buy quote buy 16800/sell 2912),
  NPC/Hero money 42142/43875 → buy 58942/27075, sell 31534/54483;
  mixed и sell-side insufficient. slot2 pure buy
  **105008/65908**, sell **128258/42658**, mixed (unit 1000/250);
  item totals сохраняются (65/42). Screenshot diffs на 1024×768
  (`trade_staging*`, `trade_buy/sell0/2`, `trade_mixed2`,
  `trade_nomoney/npcmoney/stock`). After-state — **analytic (PC=53 —
  superseded by D-062: binary32/PC=24), not observed**; before-state
  real/validated.
- **Ограничения:** no persistent server staging `+0x4E4/+0x4E8` и
  rollback action `0xD` (staging = overlay quotes); save не пишется;
  дубликаты item id матчатся по item id+count (не first-id); net/суммы
  ограничены signed int32;
  unstage append в конец source; клиентский invocation staging id не
  трассирован; `gv_addon`/perks не моделируются; specials NPC-лута не
  генерируются (D-055); modifier-free NPC stock доказан только
  slot0/slot2; live FPU-CW не снят (generation CW снят в 1b2b2e/D-062;
  price CW — Stage 4).
- **Final review (2026-11-09):** With fixes (transfer metadata retention,
  item_id+count stack match, guards/int32/docs) → final fix `19087a9`;
  scoped re-review clean. Ready to merge = Yes. Полный `ctest` после
  final fix (`19087a9`, 2026-11-10): **19/19 passed**, 1267.41 s, exit 0
  (затронутые suites до этого: 289/289 gl_game, 25/25 trade_overlay,
  8/8 trade screenshots).
- **Состояние (2026-11-09 21:58, сессия остановлена):** коммиты
  `4b5c1f8..19087a9` — **локальные, не запушены**; SDD ledger
  `.superpowers/sdd/2026-11-09-p3b2b4b-trade-transactions/progress.md`.
  Продолжить с: final STATUS note (этот), push, затем specials/P2-15.
- **Дальше:** research specials NPC-лута (R4–R10) / `P2-15` (save
  payload) / 35 Гц (`P2-9`), затем остальные системы P5+/P6/P7.

## Phase 1b2b4a — read-only trade overlay (done 2026-11-09)

- **Итог (Tasks 1–5, done):** `RS_PassToTradePanel` (`0x700000D`)
  supported, registry **16/65** (3 vm_internal, 46 unsupported); callback
  закрывает диалог и открывает read-only `TradeOverlay` со спикером.
  Layout/assets — `engineres/trade` + `scripts/ui/trade.scr`; NPC-фильтры
  obj 30..36 (`afilter1..7`) имеют per-id Y `{94,94,95,95,95,95,95}`
  (id 30/31 `trade.scr:506-541`, id 32..36 `:543-631`). NPC trade list =
  pass-1 hot `entity+0xC`, сохранённый в `InventoryRuntime::trade_items`;
  Hero — main `entity+0x10` + `money()`; две money values (NPC y=126,
  Hero y=612, x=562). Read-only: selection/filter/scroll/exit; нет
  buy/sell/confirm и мутаций trade-контейнера; **цены не отображаются**
  (price UI — 1b2b4b). Docs: D-057; parity 170 строк на 1b2b4a (174 после 1b2b4b).
- **Проверка (`GL_GAME_DIR=I:\Games\Златогорье-2`):**
  `cmake --build build` exit 0; targeted `ctest` (`gl_script_tests|
  gl_script_real_tests|gl_game_tests|gl_assets_tests|gl_assets_real_tests|
  glgame_screenshot_tests|glview_screenshot_tests|level_click_tests|
  trade_overlay_tests`) **9/9 passed**, 1721.92 s, exit 0; full `ctest`
  **19/19 passed**, 1838.21 s, exit 0. Oracle slot0
  `16/42142/117/43875`, slot2 `17/48908/140/122008`; asserted все NPC
  trade items и первые 12 Hero main; screenshot 1024×768 diff > 2 MB
  байт + top-region > 100000 px; regression `l1_1 --click-npc 13`.
- **Ограничения:** client `FUN_1206EC98` invocation не трассирован
  (server/native-level source only); NPC/hero-фильтры применяют
  inventory-grouping как assumption (type→bit для trade не доказан,
  P3d-4 partial); exact exit command `0x2B` не трассирована; lists —
  снимок на момент открытия; нет live-memory oracle; NPC specials не
  генерируются (D-055).
- **Final review (2026-11-09):** Ready to merge = Yes после fix wave
  `9406f15` (NPC list filtering + minors); Critical/Important нет.
  Отложенные minors: unknown-person `--trade-npc` test, trade E2E seed
  дублируется, duplicate trade names first-wins (P451 documented).
- **Дальше:** Phase 1b2b4b buy/sell (цены, `0x5000006`/`0x5000007`,
  confirm) или research specials / `P2-15` (save payload).
- **Задача:** `RS_PassToTradePanel` (`0x700000D`) → supported; overlay
  NPC↔Hero items/деньги/фильтры (read-only).
- **Spec:** `docs/specs/2026-11-09-p3b2b4a-trade-overlay-design.md`.
- **Plan:** `docs/plans/2026-11-09-p3b2b4a-trade-overlay.md`.
- **Task 1 (research gate, done 2026-11-09):** вердикт **`implementable`**
  для read-only overlay. Research:
  `docs/research/p3b2b4a-trade-panel.md`.
  - Ассеты/layout: `engineres/trade/trade.bmp` 1024×768 +
    `scripts/ui/trade.scr`; 3 trade-ряда (obj 48/49/50, 10 слотов,
    x=347..895, y=26/208/478, icon 55×55); hero main 12×55 (y=703),
    hot 9×27 (y=624), puppet; фильтры `filter1..7` (y=672) и
    `afilter1..7` (per-id y `{94,94,95,95,95,95,95}`); exit (920,694), scroll, сервисные
    `lbig/pbig` (repair/exchange/identify/charge/curse); деньги
    y=126/386/612.
  - Client: открытие `FUN_1203CFD0` case `0xF` (:46778); ctor
    `FUN_120708E0` (:75286); списки `FUN_1206FBC0/6F5BC/6EFB8/70198`;
    render `FUN_12071014`, update `FUN_1207A2A8`; money
    `FUN_1206DA08` = count `MON_1_0_1`; price `FUN_1206E8D0`
    (ITM price × durability, SKILL_TRADE id 16, buy
    `2.5−0.1*skill`, sell reciprocal, trunc); per-unit в
    `FUN_120723E8`/`120725C4`; hot/equip price call нет.
  - **Источник NPC-списка — trade-контейнер `entity+0xC` (pass 1
    NPC-лута), не main `+0x10`.** Server/native-level proven
    (`FUN_14028404` hero `+0x4D4 = NPC+0xC` + `0x5000006/7`);
    клиентский setter `FUN_1206EC98` (callers=0) invocation
    **не трассирован** — client binding `unverified`. Порт 1b2b2b
    на момент research отбрасывал pass 1 (`hot`); Task 2 сохранил его;
    pass 1 — единственный источник NPC-списка.
  - `RS_PassToTradePanel`: handler RVA `0x41558` (`-1`, return 0.0);
    `FUN_14028404` (RVA `0x28404`) добавляет hero action type 3 с
    NPC-id и пишет hero `+0x4D4 = NPC+0xC`; `dialog+0x5528=1`
    (close flag).
  - Real oracle: `L13_1.P517_Boat_Trader` (l13_1, `--click-npc 3`,
    inv013_barman, dialog 8868 answers `[3498,4214]` → bytecode 127
    `PassToTradePanel`). slot0 loot 41: NPC 16 записей / money 42142,
    hero money 43875; slot2 loot 37: 17 записей / money 48908, hero
    money 122008; buy/sell per-unit таблицы — research §6;
    `populated` slot0 3/8, slot2 6/8; hero first 12 — research §6.3.
  - **Fix round 1 (2026-11-09):** source propagated в spec §3.2/3.3/4,
    plan Task 2/3 (`loot.hot` retention + `trade_items`/`trade_money`,
    `open(npc_items,npc_money,hero_items,hero_money)`); client-side
    `FUN_1206EC98` invocation downgraded; hot mode 99 removed;
    oracle reproducibility/seed/inputs + hero first 12 recorded;
    two money values; `trade.scr` 101/102/103 — code-drawn placeholders.
    Parity-matrix на момент fix round не менялся (Task 5 — 170 строк).
  - **Gate:** был снят; Tasks 2–4 done. Остаётся unverified:
    item-type→filter-bit для trade-списков (кнопки/mask доказаны),
    client invocation `FUN_1206EC98`/источник `local_player+0x6F8/+0x6FC`,
    modifier-блок цены, точная exit-привязка; read-only overlay ими не
    блокируется.
- **Task 2 (overlay core, done 2026-11-09):** pass-1 `loot.hot` retained as
  NPC trade container (`trade_items`/`trade_money`); `TradeLayout` +
  read-only `TradeOverlay` (letterbox 1024×768, NPC/Hero rows, two money,
  filters, scroll, exit); unit + real screenshot tests. Commits `4273a0a`,
  `eac9a3c`.
- **Task 3 (native/service wiring, done 2026-11-09):**
  `NativeServices::trade_panel`; `0x700000D` supported, arity 0, callback
  invoked, always returns `double{0.0}`, empty callback logs. Registry
  **16/65 supported**. `glgame` sets the callback per dialog, defers
  close/open until the VM call returns (avoids use-after-free), then
  `close_dialog()` + `TradeOverlay` from dialog speaker
  `trade_items`/`trade_money` and Hero main/`money()`; input blocking,
  Esc/exit, `render`. No active dialog/speaker → warning + no-op. Hooks:
  `--trade-npc <name>` (with empty-list warning) and `--dialog-answer <n>`.
  `close_dialog()` clears stale `pending_trade_npc_`; `dialog_activate`
  aborts on failed `choose()` before opening. Commits `577edd6`
  (implementation) + fix `fix(tools): reset pending trade and cover dialog path`.
  Tests (`GL_GAME_DIR`): `gl_script_tests` **123/123** (961 assertions),
  `gl_game_tests` **234/234** (558702), `trade_overlay_tests` **11/11**
  (257), `level_click_tests` pass; ctest targeted 4/4 (131.11 s);
  `glgame_screenshot_tests` new two cases **2/2** (29 assertions):
  `--trade-npc` real boat-trader overlay with pixel diff vs level baseline,
  and real dialog answer 0 → **`RS_PassToTradePanel`** → dialog closed once
  → `trade: opened npc='L13_1.P517_Boat_Trader' npc_items=16
  npc_money=42142 hero_items=117 hero_money=43875`. Smoke
  `l1_1 --click-npc 13` exit 0; `--trade-npc` with empty stock warns
  (empty-list warning, not an unknown-person check).
- **Final fix (2026-11-09, review):** NPC trade row now filters through
  `gl::game::item_matches_filter` (same grouping as hero row/inventory
  P5b1) for visible list, scroll clamp and hit-test — **assumption** for
  P3d-4 (partial), not proven trade parity (type→bit for trade lists still
  unverified, research §8.1). «All» filter (mask 0) is no longer drawn as
  pressed on open until explicitly selected (separate
  `npc_filter_index`/`hero_filter_index`); `run_headless_click` stops when
  the trade overlay is visible; a failed `DialogSession::start()` resets
  state without logging `dialog: closed`; trade E2E passes explicit
  `--seed 1`; `--trade-npc` warning is empty-stock only. Tests (`GL_GAME_DIR`):
  `trade_overlay_tests` **13/13** (488), `gl_game_tests` **236/236**
  (558842), `gl_script_tests` **123/123** (961),
  `glgame_screenshot_tests --test-case=*trade*` **2/2** (61, 27 filtered).
  Commit `fix(app): filter NPC trade list`.
- **Вне scope:** покупка/продажа, trade-мутации, confirm; цены в
  read-only overlay не показываются (1b2b4b).

## Phase 1b2b3 — inventory transfers (done 2026-11-08)

- **Задача:** `RS_PersonTransferItemI` (`0x5000001`) и
  `RS_PersonTransferAllItemsI` (`0x5000003`) между Hero/NPC.
- **Spec:** `docs/specs/2026-11-08-p3b2b3-transfers-design.md`.
- **Plan:** `docs/plans/2026-11-08-p3b2b3-transfers.md`.
- **Research (Task 1, done):** `docs/research/p3b2b3-transfers-server.md`.
  `FUN_14048790` (RVA `0x48790`) / `FUN_14048518` (RVA `0x48518`);
  handler RVA `0x401dc`/`0x40fb0`, sig `3,3,3,0`/`3,3`; UID/id
  (`FUN_1404c690`, `+0xA4`/`+0xA8`/`+0xB0`); real oracle slot0/slot2
  §7; real dialog `l1_1.p387_poison_plotnik`; 284 `TransferItemI` в
  111 `.cs`, 0 `TransferAllItemsI`.
- **Implementation (Tasks 2–4, done):** `InventoryRuntime::transfer_item`/
  `transfer_all` + `InventoryAccess` callbacks; handlers в
  `native_registry.cpp`; Hero/NPC main контейнеры mutable, `add/remove`
  остаются Hero-only. Registry: **15/65 supported** (3 vm_internal, 47
  unsupported). Commits `feat(game): transfer items between persons` +
  fix `fix(game): reject missing transfer persons`,
  `feat(script): implement inventory transfer natives`,
  `test(inventory): cover real transfer dialog`.
- **Tested semantics:** same container → true; `count == 0` → true;
  `count < 0` → лимит 1 000 000; absent item (+ → false, 0/− → true);
  partial/over; source-записи удаляются; merge в dest по item_id; новая
  dest-запись `type/flags/style/max_stack` из ITM-шаблона, merge
  сохраняет flags dest; transfer_all — все main-записи в slot order,
  source очищается; missing person/container → false/no-op; real P387
  (preload `Hero.QST_1_0_3=1`, ответы `0,0`): `Hero 0`, `P387 1`,
  `P387.QST_1_0_5=1`, `L1_Svetlograd_n3_GetElexir=5`, `plotnik_povtor=2`;
  control без preload не переносит (`plotnik_povtor=1`).
- **Отклонения/guards (D-056):** `transfer_all` missing person → `false`/
  no-op (в оригинале AV — null `entity+0x10`); `transfer_item` missing
  person/container и missing container в обоих хелперах → `0`/false как
  в оригинале (не deviation); `transfer_all`: source без ITM-шаблона →
  false без мутаций (оригинал продолжает с непроверенной add-ошибкой),
  `max_stack == 0` → false (оригинальный creation-loop не завершается);
  `transfer_item`: missing template → positive false/zero-negative true
  (add-ошибка не проверяется), `max_stack == 0` → positive false/
  zero-negative true (creation-loop); `count` —
  `as_int` (усечение) vs оригинальный `(int)ROUND`; append vs первый
  свободный слот. Не моделируются notify `FUN_140482e8`, container
  `+0x18`, `entity+0x4dc`; save не пишется.
- **Проверка (`GL_GAME_DIR=I:\Games\Златогорье-2`):** `cmake --build
  build` exit 0; targeted `ctest` (`gl_script_tests|gl_script_real_tests|
  gl_game_tests|gl_assets_tests|gl_assets_real_tests|
  glgame_screenshot_tests|glview_screenshot_tests|level_click_tests`)
  **8/8 passed**, 1094.72 s, exit 0; full `ctest` **18/18 passed**,
  1093.75 s, exit 0.
- **Docs:** D-056; parity-matrix 166 строк (`verified` 50, `partial` 65,
  `unverified` 22, `missing` 17, `contradicted` 2, `superseded` 10);
  PROJECT P3/P5/parity summary обновлены.
- **Ограничения:** нет live-оракула (save scan + Python NPC-loot
  reference + real `.cs`); trade-контейнер `+0xC`/`0x5000006`/`7`/
  `RS_PassToTradePanel` вне scope; specials/restock NPC blocked; save
  payload не читается/не пишется.
- **Final review (2026-11-09):** Ready to merge = Yes; Critical/Important
  нет. Отложенные minors: `transfer_all` second template lookup без
  re-check; `append_items` return ignored; нет unit-теста
  `transfer_item max_stack==0`; count truncation vs ROUND; duplicate
  15/65 assertion; read_save open check; globals type check; fixture
  loot level 1; full/targeted time variance.
- **Дальше:** trade/`RS_PassToTradePanel` (`0x5000006`/`0x5000007`),
  research specials R4–R10 (`FUN_14047284`/`DAT_14088df4`), затем save
  payload (`P2-15`), затем 35 Гц.

## Phase 1b2b2b — core NPC `.inv` loot (done 2026-11-08)

- **Задача:** `parse_inv` + `NpcLootGenerator` + NPC-контейнеры в
  `InventoryRuntime`; specials не генерируются (documented RNG-desync).
- **Spec:** `docs/specs/2026-11-08-p3b2b2b-core-npc-loot-design.md`.
- **Plan:** `docs/plans/2026-11-08-p3b2b2b-core-npc-loot.md`.
- **Task 1 (research gate, done 2026-11-08; revised 2026-11-08):**
  вердикт **`implementable`**. Research:
  `docs/research/p3b2b2-npc-loot-server.md` §15.
  - `BT_INFO +0x26c4` = сохранённый **средний уровень партии**
    (`FUN_140254bc`: среднее `FUN_1400cbc0(EXP)` по party-членам,
    `+5` за `SKILL_SCOUT>9`, `+5` за `SKILL_IDENTIFY>4`; update
    `30577-30580`, writer asm `0x14031d0a`, reader `35637-38`, init
    `33982`). Loot level = `value + 1` (`49517`). 28/28 save:
    value == hero level (slot0=`40`/`L93_4`, slot2=`36`/`L6_3`,
    slot21=`1`/`L1_1`); Scout base max `2`, Identify max `0` (пороги
    не превышены); точные BT_INFO-смещения — §15.1.
  - Prerequisites: ITM `type/flags/max_stack/style`
    (`proto+4/+8/+0x1c/+0x30` → runtime `+8/+0xc/+0x20/+0x34`).
    R1 `coin(regenerate_chance)` всегда (`49510`/`51141`).
  - **Реальный SEF-путь — `FUN_1404b1c0` (два прохода), не
    `FUN_140489ec`.** Pass 1 → hot `+0xc`:
    `FUN_1404e3a4(name,0,1)` (`this+4=1` только очищает item-флаг
    `0x80`, не gate merge), прямой `count=min(remaining,max)`;
    merge возможен/`unverified`; chunk-loop нет. Pass 2 → main
    `+0x10` (его читают `TestPersonHasItem`/`GetItemCountI`):
    `coin((0.01*n3)/chance_decrease)` (при `n3==0` coin нет), qty
    `1/quantity_decrease` (trunc/+1) → `range`, creation `this+4=0`;
    merge возможен/`unverified`. RNG-порядок: R1, pass1 R2/R3,
    pass2 R2'/R3'. Порог метаданных `100000` практически недостижим —
    **4-stack exit отсутствует**. Деление на
    `chance_decrease`/`quantity_decrease` не защищено; в корпусе нулей
    нет, behavior at 0 — `unverified`/D-055.
  - Merge `+1` — shared `FUN_1404e3a4`; caller-квирки `qty -= chunk`/
    early-exit `>=4` — только `FUN_140489ec` (hero/иной путь). К
    `FUN_1404b1c0` эти caller-квирки **не** применять.
  - RNG order: SEF `persons:` в порядке файла; `scr_inv` →
    `FUN_1401edc4` `22422-22455` → `FUN_1404b1c0`; лут потребляет RNG
    раньше движения (movement RNG — в update-тиках `20759`/
    `20410-11`). Порт: один поток; оба прохода в SEF-порядке, main
    сохраняется, hot — если поддерживается (RNG pass 1 обязателен);
    затем `LevelSimulation` с тем же state; расхождения — §15.3.
  - **Ревизия spec/plan (историческая запись Task 1):** target обновлён
    на `FUN_1404b1c0` (main = pass 2, hot = pass 1; second pass **в
    scope** как main, не non-goal). На момент Task 1 Task 2 можно было
    начинать сразу; Tasks 3–4 — после ревизии (тот commit).
- **Gate (историческая запись Task 1):** level source доказан; Task 2
  не был blocked; Tasks 3–4 разблокированы после ревизии spec/plan.
- **D-055:** записан в `DECISIONS.md` (2026-11-08).
- **Критерии:** 238 `.inv` parse; generator tests (two passes, no
  4-stack, pass2 scaling); NPC main counts; full `ctest`;
  D-055/parity/STATUS.
- **Коммиты Task 1:** `637f184` (research) + `12a7a57`, `7774e07` и
  `docs: align merge and restock semantics` (fixes); range
  `637f184..HEAD`.
- **Task 2 (`.inv` parser, done 2026-11-08, code commit `98cfa62`):**
  - `gl_assets::InvTable`/`InvItem`, `parse_inv`, `load_inv` →
    `scripts/inventory/<name>.inv` (lowercase, forward slashes).
  - Defaults `1.0/1.0/1.0/0` из clear `FUN_140471cc`
    (`decompiled.c:48322-48327`); spec §3.1 исправлен (ранее ошибочно
    `0.0`). `regenerate_chance` хранится масштабированным (`*0.01`),
    `chance_percent` — сырой `n3`.
  - Грамматика/лексер по research §2–§5: `//`-комментарии (включая
    attached-склейку), кавычки, произвольный whitespace, заголовки в
    любом порядке; 4-number `item` — skip + один warning на файл;
    `special` игнорируется.
  - **Проверено:** `gl_assets_tests` — 200/200 cases, 1344/1344
    assertions (из них `inv_table` 9/50); targeted
    `gl_assets_real_tests --test-case=*inv_table*` — 2/2 cases, 50/50
    assertions; реальные данные **238/238** файлов, **11 543** item,
    **0** failures, **362** skip в 10 файлах (log-verified;
    `boxes/iron.inv` — 0 записей). `inv001_swordsman.inv` — 165
    записей, header `75/2/1/4`.
  - **Full `ctest`** (с `GL_GAME_DIR`): **18/18 passed**, exit 0,
    `Total Test time (real) = 1882.64 sec`
    (`gl_assets_real_tests` 672.94 s, `glgame_screenshot_tests`
    822.39 s).
  - Minors задокументированы в spec §3.1: повторная проверка
    невалидного header-значения как ключа, строгий charset double,
    ASCII-only `tech_name`, отсутствие лимита длины токена,
    незакрытая кавычка → format error (synthetic-тест), skip-count
    только в warning.
- **Task 3 (`NpcLootGenerator`, done 2026-11-08; code `ce6ccb8`, flag fix
  `b7b188a`):** `gl::game::NpcLootGenerator::generate` реализует initial
  spawn `FUN_1404b1c0`: R1 `coin(regenerate_chance)` всегда; pass 1 →
  `hot` (R2 `coin(0.01*n3)`, R3 `range(n4,n5)`, split `min(rem,max)`,
  item-флаг `0x80` очищен), pass 2 → `main` (chance
  `(0.01*n3)/chance_decrease`, qty `1/quantity_decrease` trunc/+1,
  `n3==0` без coin, флаг `0x80` сохранён); 4-stack exit нет; merge не
  моделируется (`unverified`); zero-divisor и `max_stack==0` — guards.
  Unit: `test_npc_loot.cpp` 11/11 (242 assertions).
- **Task 4 (NPC-контейнеры + level-load, done 2026-11-08; code `b2fd51b`,
  duplicates fix `229ec13`):** `InventoryRuntime` хранит NPC main
  (`+0x10`, pass 2) по точному имени; first-wins + warning для
  дубликатов; hot только для RNG и отбрасывается. `LevelScreen::load`
  hook генерирует лут в SEF-порядке до `LevelSimulation` на общем
  `MsvcRandom` и передаёт тот же advanced state в симуляцию;
  `--no-persons` генерацию пропускает. Real `l1_1` main counts:
  `L0.P7_Kolbasens_Rodstvennik1` `MON_1_0_1`=174,
  `L1_1.P389_Crying_Guardian`=218, `L0.P711_citizen8`=323,
  `L1_1.P387_Poison_Plotnik` `QST_1_0_5`=1; `L0.P678_citizen4` пуст;
  `L0.P744_oficer1` first-wins (ids 263/371, money 0); Hero не изменён.
  **Oracle provenance:** значения выведены независимым Python-референсом
  (MSVC LCG, float32 `coin`, truncating `range`, оба прохода,
  `.inv`/`tech_names.sdb`/ITM), не из C++-портации; derivation/SEF-порядок —
  `docs/research/p3b2b2-npc-loot-server.md` §15.6; захардкожены в
  `tests/game/test_inventory_runtime_real.cpp`.
- **Task 5 (docs/parity/regression, done 2026-11-08):** D-055 записан;
  parity-matrix `P3b-1..P3b-5` + пересчёт (163 строки: verified 49,
  partial 63, unverified 22, missing 17, contradicted 2, superseded 10 —
  на момент 1b2b2b); PROJECT/plan обновлены.
- **Проверка (фактический вывод 2026-11-08, `GL_GAME_DIR=I:\Games\Златогорье-2`):**
  - `cmake --build build` — exit 0.
  - targeted `ctest --test-dir build --output-on-failure -R "gl_script_tests|gl_script_real_tests|gl_game_tests|gl_assets_tests|gl_assets_real_tests|glgame_screenshot_tests|glview_screenshot_tests|level_click_tests"`
    — **8/8 passed**, `Total Test time (real) = 1135.51 sec`, exit 0:
    `gl_assets_tests` 5.41, `gl_assets_real_tests` 537.14,
    `gl_game_tests` 58.72, `gl_script_tests` 0.43,
    `gl_script_real_tests` 9.92, `glview_screenshot_tests` 102.52,
    `glgame_screenshot_tests` 410.06, `level_click_tests` 10.55.
  - full `ctest --test-dir build --output-on-failure` — **18/18 passed**,
    `Total Test time (real) = 1210.51 sec`, exit 0:
    `gl_assets_tests` 4.08, `gl_assets_real_tests` 421.18,
    `gl_game_tests` 55.65, `gl_core_tests` 0.11,
    `gl_core_real_tests` 0.02, `gl_script_tests` 0.40,
    `gl_script_real_tests` 9.00, `glpak_cli_tests` 0.07,
    `gl_cs_tools_tests` 6.50, `camera_tests` 0.03, `utf8_tests` 0.02,
    `font_tests` 1.21, `glview_screenshot_tests` 95.88,
    `glgame_screenshot_tests` 580.28, `person_render_tests` 4.27,
    `level_click_tests` 16.14, `dialog_overlay_tests` 0.76,
    `inventory_overlay_tests` 12.63.
- **Ограничения (D-055):** specials R4–R10
  (`FUN_14047284`/`DAT_14088df4`) не генерируются — документированный
  RNG-desync после special-способного предмета; нет live-оракула;
  merge `+1` `unverified`; нулевые `chance_decrease`/`quantity_decrease`
  и `max_stack==0` — безопасные guards, не паритет; loot level — EXP-
  эквивалент `+0x26c4`, средний уровень партии без Scout/Identify;
  hot отбрасывается; NPC add/remove — no-op; restock/Transfer/Trade/
  PassToTradePanel — вне scope; активация save на уже загруженном уровне
  не перегенерирует NPC-контейнеры до следующего `load_level`;
  `--no-persons` генерацию пропускает.
- **Следующее:** research specials (`FUN_14047284`/`DAT_14088df4`) или
  Transfer/Trade/`PassToTradePanel`, либо `P2-15` (save payload).
- **Коммиты:** `98cfa62` (parser), `ce6ccb8` (generator), `b7b188a`
  (flag 0x80 fix), `b2fd51b` (containers/level-load), `229ec13`
  (duplicate first-wins); docs — `docs: record core NPC loot`.
- **Final review (2026-11-08):** Ready to merge = Yes; Critical/Important
  нет. Отложенные minors: `max_stack` верхний clamp (защита от
  нетерминирующего цикла; реальные данные < 2^31), oracle не покрывает
  NPC после duplicate `P744`, merge/live-oracle residual, save/New Game
  lifecycle, `--no-persons`/save activation gaps.

## P4 — RNG parity (D-053 implementation) (done 2026-11-08)

- **Задача:** `MsvcRandom::range` — truncation, `coin` — float,
  production seed — `steady_clock` ms вместо `1` (D-053).
- **Spec:** `docs/specs/2026-11-07-p4-rng-parity-design.md`.
- **Plan:** `docs/plans/2026-11-07-p4-rng-parity.md`.
- **Сделано (Tasks 1–4):**
  - `range(lo,hi)` = `lo + trunc(((hi-lo)+0.99999)*next()*(1.0/32767.0))`;
    `coin(p)` — float (`(float)next()*(1.0f/32767.0f) < p`); `next()`/`fraction()`
    без изменений.
  - `MsvcRandom::production_seed()` = `steady_clock` ms; `LevelScreen::load`
    использует его по умолчанию; `glgame`/`glview` — `--seed <n>`; тесты
    пинят `--seed 1`; movement/`STAY_ROTATE` ожидания пересчитаны.
  - Anti-livelock guard — port-side robustness (D-054), не паритет.
  - D-054 расширен RNG-паритетом и ограничениями; parity-matrix RNG-строки
    обновлены (`P4b-3`/`P4b-9`/`P4b-15` verified, `P4b-7` partial — 500 мс
    `SET_DIRECTION`).
- **Проверено (2026-11-08, `GL_GAME_DIR=I:\Games\Златогорье-2`):**
  - build: `cmake --build build` → exit 0.
  - targeted: `ctest --test-dir build -R "gl_script_tests|gl_script_real_tests|gl_game_tests|gl_assets_tests|gl_assets_real_tests|glgame_screenshot_tests|glview_screenshot_tests|level_click_tests"` → `100% tests passed out of 8`, `Total Test time (real) = 1152.24 sec`, exit 0.
  - full: `ctest --test-dir build --output-on-failure` → `100% tests passed out of 18`, `Total Test time (real) = 1241.95 sec`, exit 0.
  - doctest-счётчики (прогон Task 3/4): `gl_game_tests` 185/185
    (558 021 assertions), `glgame_screenshot_tests` 26/26 (721),
    `glview_screenshot_tests` 11/11, `level_click_tests` 23/23; полный
    `ctest` 2026-11-08 их подтвердил.
- **Ограничения:** оригинальный `timeGetTime` недетерминирован; порт —
  `steady_clock` ms (не тот же источник); RNG-поток не непрерывен между
  уровнями (глобальный поток оригинала vs `MsvcRandom` на `LevelSimulation`);
  anti-livelock guard добавляет `range()` draw (port-side); NPC-loot
  specials/`DAT_14088df4` blocked.
- **Следующее:** Phase 1b2b2b (NPC `.inv` лут) после закрытия/явного scope
  specials, `BT_RGINF`, `+0x26c4`, stacking и live-оракула; либо
  Transfer/Trade/`PassToTradePanel`.
- **Коммит:** `docs: record RNG parity fix` (docs-only).
- **Final review (2026-11-08):** Task 4 review clean; whole-branch review
  «with fixes» (2 Important docs issues) → docs-fix `c652e24`; scoped
  re-review — все findings addressed. Код/тесты не менялись после
  `1adf89c`; full `ctest` подтверждён 18/18 (1068.24 s, exit 0).
- **Коммиты:** `9c9eb72`…`c652e24` (RNG parity + docs). Запушены.

## Phase 1b2b2a — NPC `.inv` loot generation research (done 2026-11-07)

- **Задача:** доказать/опровергнуть точное воспроизведение стартового
  NPC-инвентаря из `scripts/inventory/*.inv` по `Server.dll`
  (`FUN_14047784` парсер, `FUN_140489ec` генератор, RNG `FUN_14006288`).
  Research-only, production-код не менялся.
- **Spec:** `docs/specs/2026-11-07-p3b2b2a-npc-loot-research-design.md`.
- **Plan:** `docs/plans/2026-11-07-p3b2b2a-npc-loot-research.md`.
- **Research:** `docs/research/p3b2b2-npc-loot-server.md` (§1–14).
- **Вердикт:** алгоритм восстановлен (`recovered`), полный паритет
  `blocked`. Блокеры: specials `FUN_14047284`/`DAT_14088df4` с общим
  RNG-потоком (R6–R10; пропуск/stub specials сдвигает последующие
  chance/quantity-роллы), гейт `BT_RGINF`, источник уровня `+0x26c4`,
  stacking в `FUN_1404e3a4`, отсутствие live-оракула. RNG-фикс D-053
  необходим, но недостаточен. Phase 1b2b2b должна сначала закрыть эти
  пункты (или минимальный набор для выбранного объёма) либо быть явно
  ограничена «без specials» с задокументированным в `DECISIONS.md`
  глобальным RNG-рассогласованием.
- **Доказано:**
  - Парсер `FUN_14047784`: токенная грамматика и `//`-комментарии,
    4 независимых прохода (header-ключи, первое валидное вхождение),
    `special` после parse безусловно сбрасывается в identity `[0..57]`.
    Реальные данные: 238/238 `.inv`, 11 543 five-number `item`,
    362 four-number `item` отбрасываются кодом (10 файлов),
    `boxes/iron.inv` — 0 записей.
  - Генератор `FUN_140489ec`: записи перебираются в порядке файла
    (выбора случайной записи нет), окно
    `n1+level_offset <= level <= n2+level_offset`, chance/quantity,
    chunking `min(qty, max_stack)` (чанки `max_stack`, остаток в
    конце), порядок, ранний выход при `>= 4` стеках; отдельного
    money-ролла нет (записи `MON_*` — обычные `.inv`-предметы);
    `chance_decrease`/`quantity_decrease` используются
    restock-функциями (§10.2); **уточнение 1b2b2b:** `FUN_1404b1c0`
    pass 2 использует их и при initial spawn (§15.2) — см. текущую
    секцию 1b2b2b (эта запись superseded).
  - RNG: `_rand` — MSVC LCG `0x343FD/0x269EC3`,
    `(state>>16)&0x7FFF`; seed — `FUN_1406c092(timeGetTime())`
    (`decompiled.c:34129`); `FUN_14006288` — coin; `FUN_140061e4` —
    truncation (`fistp`, RC=`0xC00`); полный упорядоченный список
    вызовов R1–R10 (§9.4).
  - Тайминг: первичная генерация — в конструкторах/спавне
    (`FUN_14018990`/`FUN_140233d4`/`FUN_14023aa0`), не лениво при
    открытии инвентаря; gate `BT_RGINF` (`+0x26c0`).
- **Не доказано / блокеры:**
  - random-special `FUN_14047284`/`DAT_14088df4`: поля записей specials,
    allow-list типов предметов и порядок доп. роллов (R6–R10) не
    восстановлены; specials используют тот же MSVC-поток, поэтому
    «без specials» — документированное RNG-рассогласование, а не
    паритет ядра;
  - `BT_RGINF` (`+0x26c0`): наблюдаемая производная записана, но
    семантика флага/polarity `unverified`;
  - `+0x26c4`: читается в `FUN_1403210c` (35638), обнуляется в
    `FUN_1402fdc0` (33982); смысл (уровень мира/сессии vs игрока)
    `unverified`;
  - stacking в `FUN_1404e3a4`: merge-путь +1 и `return`
    (`53793`/`53809`), полное условие входа `unverified`;
  - live-оракул: память запущенной игры и seed не снимались; нужна
    сверка контейнеров 0/N;
  - `MsvcRandom::range` в порте — `std::nearbyint`, оригинал —
    truncation; `coin` считает в `double`, оригинал — во `float`
    (D-053).
- **D-053:** RNG-паритет — seed/clock (`timeGetTime`) + truncation;
  затрагивает движение (`FUN_1401c2a0`), `STAY_ROTATE` и лут, не только
  NPC loot. Реализация `MsvcRandom` — отдельная задача; до неё это
  документированное отклонение паритета.
- **Следующее:**
  1. RNG-паритет по D-053: `MsvcRandom` seed/time + truncation +
  float-арифметика `coin`; аудит движения/`STAY_ROTATE`/лута.
  2. (опционально) live-оракул specials: снять контейнеры при известном
  seed, восстановить `DAT_14088df4`.
  3. Phase 1b2b2b (реализация NPC `.inv` лута) — только после (1) и
  закрытия/явного scope specials, `BT_RGINF`, `+0x26c4`, stacking.
- **Коммиты:** `426d25b..ce88b04` (spec/plan, Task 1/2 research,
  verdict, cross-reference fixes; head `ce88b04`). Всё docs-only;
  production-код не менялся; **не запушено**.
- **Проверка:** `DECISIONS.md`/`STATUS.md`/`PROJECT.md` — UTF-8 без BOM,
  `nul=0`, без escape-артефактов переноса в новых секциях.

## Phase 1b2b1 — Hero inventory mutations (done 2026-11-06)

- **Задача:** реализовать `RS_PersonAddItem` (`0x5000004`) и
  `RS_PersonRemoveItem` (`0x5000005`) на mutable `InventoryRuntime`
  (Hero); money — через `MON_1_0_1` (type 18); save не пишется.
- **Spec:** `docs/specs/2026-11-06-p3b2b1-inventory-mutations-design.md`.
- **Plan:** `docs/plans/2026-11-06-p3b2b1-inventory-mutations.md`.
- **Research:** `docs/research/p3b2b1-inventory-mutations-server.md`.
- **Сделано (Tasks 1–6):**
  - Research: handler RVA `0x41320`/`0x41478`, signature `(3,3,0)`,
    return `0.0`; `FUN_1404cd28` (add: stacking по item id,
    `max_stack` = `rec+0x34` из ITM, split на новые записи,
    type/flags/style из ITM) и `FUN_1404cae8` (remove: частичное/
    полное удаление, `count > available` удаляет найденное); container
    `+0x18` write-only и `FUN_140482e8` notify не моделируются; real
    oracle slot0/slot2 (money `43875`/`122008`, potion `953`).
  - `gl::game::InventoryRuntime` mutable: Hero `std::vector<InventoryItem>`,
    `add_item`/`remove_item` по `Server.dll`; `ItemTemplate { type,
    flags, style(unknown_b[1]), max_stack }` из `ItemCatalogEntry`/ITM;
    live overlay view (`hero_inventory()`); мутации переживают диалоги
    (`open_dialog` больше не reseed'ит из save).
  - `gl::script::InventoryAccess` add/remove callbacks + handlers
    `0x5000004`/`0x5000005` (`3,3,0`, return `0.0`); `NativeRegistry`
    **13 supported, 3 vm_internal, 49 unsupported (всего 65)**.
  - Real-диалог: `scripts/dialogs/unique/gypsy.age.cs` через production
    `ScriptVm → GameScriptHost → InventoryRuntime`; money
    slot0 `43875 → 41075`, slot2 `122008 → 119208` (Remove 2800).
- **Проверка (фактический вывод 2026-11-06):**
  - `cmake --build build` — exit 0.
  - Targeted `ctest -R "gl_script_tests|gl_script_real_tests|gl_game_tests|
    gl_assets_tests|gl_assets_real_tests|glgame_screenshot_tests"` с
    `GL_GAME_DIR` — **6/6 passed**, 1280.46 s, exit 0: `gl_assets_tests`
    5.84 s, `gl_assets_real_tests` 589.86 s, `gl_game_tests` 80.16 s,
    `gl_script_tests` 0.63 s, `gl_script_real_tests` 13.64 s,
    `glgame_screenshot_tests` 590.20 s.
  - Полный `ctest --test-dir build --output-on-failure` с `GL_GAME_DIR`
    — **18/18 passed**, 1487.28 s, exit 0: `gl_assets_tests` 5.81,
    `gl_assets_real_tests` 592.77, `gl_game_tests` 79.59,
    `gl_core_tests` 0.74, `gl_core_real_tests` 0.30, `gl_script_tests`
    0.65, `gl_script_real_tests` 13.71, `glpak_cli_tests` 0.13,
    `gl_cs_tools_tests` 9.93, `camera_tests` 0.70, `utf8_tests` 0.20,
    `font_tests` 2.61, `glview_screenshot_tests` 150.35,
    `glgame_screenshot_tests` 594.45, `person_render_tests` 4.96,
    `level_click_tests` 15.43, `dialog_overlay_tests` 0.76,
    `inventory_overlay_tests` 12.12.
- **Ограничения (D-052):** mutations — только `Hero`; NPC/EF type
  `0x10` контейнеры → no-op; отрицательный `count` → no-op (в оригинале
  uint-wrap, поведение не доказано live); известный TECH без ITM-
  шаблона → add no-op (research §3.4: стекинг в существующую запись
  мог бы произойти); runtime-снятие бита `0x80` (`DAT_14088df8+4`)
  не моделируется; `max_stack` не хранится per-record (берётся текущий
  ITM); `max_stack == 0` → add no-op (в оригинале цикл); новая запись
  добавляется в конец, а оригинал — в первый свободный слот (порядок
  overlay после delete+add может отличаться); `count` усекается к int32
  (в оригинале `ROUND`; counts в `.cs` целые); нет
  `Transfer`/`Trade`/`PassToTradePanel`; save не пишется;
  автоматического интеграционного теста на два последовательных
  диалога нет (мутации переживают диалог, но проверен один проход).
- **Следующее:** `RS_PersonTransferItemI`/`RS_PersonTransferAllItemsI`/
  Trade, NPC `.inv` лут (Phase 1b2b/c) или `P2-15` (save payload).

## Phase 1b2a — Hero inventory query natives (done 2026-11-06)

- **Задача:** реализовать `RS_TestPersonHasItem` (`0x5000000`),
  `RS_GetItemCountI` (`0x5000002`), `RS_GetMoney` (`0x5000008`) на
  `HeroInventory` из save; TECH-имя → item id через `tech_names.sdb`;
  money — sum type 18.
- **Spec:** `docs/specs/2026-11-06-p3b2a-inventory-queries-design.md`.
- **Plan:** `docs/plans/2026-11-06-p3b2a-inventory-queries.md`.
- **Research:** `docs/research/p3b2a-inventory-queries-server.md`.
- **Сделано (Tasks 1–6):**
  - Research: handler RVA `0x40144`/`0x40d40`/`0x41120`, arity/типы
    `(3,3)`/`(3,3)`/`(-1)`, default-константы `1.0`/`0.0`, lookup
    `entity+0x10` (container 1), `FUN_1404c574` (TECH → item id + sum
    count), `FUN_1404c494` (sum type `0x12`), `FUN_1402914c`/`FUN_14029270`;
    `tech_names.sdb` 1848 уникальных имён; save oracle slot0/slot2.
  - `gl::assets::ItemNameIndex` — exact/case-sensitive `TECH → id` из
    `sdb/items/tech_names.sdb`; `gl::script::InventoryAccess` +
    три handler'а; `NativeRegistry` 11 supported, 3 vm_internal,
    51 unsupported (всего 65).
  - `gl::game::InventoryRuntime` — `HeroInventory.items` из save
    (container `entity+0x10`), precomputed counts и money (sum type 18);
    только `"Hero"`, unknown/не-`Hero` → 0.
  - `glgame` wiring: `ItemNameIndex`/`InventoryRuntime` в `boot`/
    `apply_equip_save`, `services.inventory` в `open_dialog`; boot-order
    fix `f212ac0` — save загружается до CLI-dialog/`--click-npc`.
  - Real tests: boat trader
    `scripts/dialogs/l13_1/l13_1.p517_boat_trader.d517.age.cs`
    (`money` slot0 = 43875, slot2 = 122008 через production
    `ScriptVm → GameScriptHost → InventoryRuntime`) и `l1_1` NPC click
    (`--click-npc 13`, dialog открывается, exit 0).
- **Проверка (фактический вывод 2026-11-06):**
  - `cmake --build build` — exit 0.
  - Targeted `ctest -R "gl_script_tests|gl_script_real_tests|gl_game_tests|
    gl_assets_tests|gl_assets_real_tests|glgame_screenshot_tests"` с
    `GL_GAME_DIR` — **6/6 passed**, 1705.37 s, exit 0: `gl_assets_tests`
    10.07 s, `gl_assets_real_tests` 858.08 s, `gl_game_tests` 95.34 s,
    `gl_script_tests` 0.78 s, `gl_script_real_tests` 10.55 s,
    `glgame_screenshot_tests` 730.33 s.
  - Полный `ctest --test-dir build --output-on-failure` с `GL_GAME_DIR`
    — **18/18 passed**, 1649.10 s, exit 0: `gl_assets_tests` 6.43,
    `gl_assets_real_tests` 736.47, `gl_game_tests` 95.38, `gl_core_tests`
    0.66, `gl_core_real_tests` 0.36, `gl_script_tests` 0.70,
    `gl_script_real_tests` 10.89, `glpak_cli_tests` 0.19,
    `gl_cs_tools_tests` 12.98, `camera_tests` 1.01, `utf8_tests` 0.25,
    `font_tests` 3.26, `glview_screenshot_tests` 187.75,
    `glgame_screenshot_tests` 525.90, `person_render_tests` 8.39,
    `level_click_tests` 27.55, `dialog_overlay_tests` 1.57,
    `inventory_overlay_tests` 21.06.
- **Ограничения (D-051):** inventory — только `Hero`; NPC-инвентарь
  (`.inv`) и fallback `FUN_14029270` по entity type `0x10` (`EF_TRIGGER`,
  не `EF_INVCONTAINER` = type `8`) не моделируются и возвращают 0 до
  1b2b/c; отсутствующая/не-`Hero` персона → 0 (в оригинале `Count` — AV).
  Нет `Transfer/Add/Remove/Trade`; запросы read-only, save не пишется;
  UI инвентаря от этих нативов не обновляется (снимок на момент
  открытия, P5b1).
- **Final review fixes (2026-11-06):** boot-order test now asserts
  `money=43875` before the `dialog:` marker and that the last runtime log
  before it is non-zero (RED on old order, GREEN on fixed); D-051 records
  the missing-person/AV deviation; parity P3-9 lists the three hex IDs and
  precise EF type wording; real oracle adds non-money `RGT_1_0_8` count
  (slot0 6 / slot2 1); unused `<functional>` removed.
- **Следующее:** Phase 1b2b inventory mutations (`Transfer`/`Add`/`Remove`/
  `Trade`, NPC `.inv`) или `P2-15` (save payload).

## Phase 1b1 — Person parameter natives (done 2026-11-06)

- **Задача:** реализовать `RS_GetPersonParameterI` (`0x4000000`),
  `RS_SetPersonParameterI` (`0x4000001`), `RS_GetPersonSkillI` (`0x400000C`)
  на состоянии порта: NPC-статы из `.scr`, Hero — из `BT_HERO` stat-блока
  save; семантика — из `Server.dll` (D-029).
- **Spec:** `docs/specs/2026-11-06-p3b1-person-parameters-design.md`.
- **Plan:** `docs/plans/2026-11-06-p3b1-person-parameters.md`.
- **Research:** `docs/research/p3b1-person-parameters-server.md`.
- **Сделано (Tasks 1–6):**
  - Research: 12 параметров (getter/setter RVA, формулы, clamp'ы),
    default-константы `0.0`, skill id `0..26` (`verified`), layout
    `BT_HERO` (`0x00C` + body `0x128`/`0x12C`/`0x130`), real slot0/slot2.
  - `gl_script`: `PersonParameter`/`person_parameter_from_name`,
    `PersonAccess`, три handler'а; `NativeRegistry` 8 supported, 3
    vm_internal, 54 unsupported (всего 65).
  - `gl_assets::parse_hero_stats` — read-only stat-блок `BT_HERO`
    (`0x00C`, life/energy `0x128`/`0x12C`, skills `0x02C + 4*id`).
    Raw `life` 263/201 при computed max 216/170 хранится как есть;
    get LIFE клампит к max (D-050, research §8.9).
  - `gl_game::PersonRuntime` — 12 параметров на персону с per-parameter
    presence: нетронутые параметры без save возвращают 0 (не clamp),
    setter помечает только свой параметр; `Hero` всегда доступен (нули
    без save, все 12 present из save); Hero-навыки из `0x02C+4*id`,
    clamp `[0,15]`; формулы getter'ов из research; `glgame` строит
    runtime в `load_level`/`apply_equip_save` и передаёт `access()` в
    `GameScriptHost`.
  - Real test `tests/script/test_real_person_natives.cpp` через
    production `ScriptVm → GameScriptHost → PersonRuntime` на
    `scripts/dialogs/old_test.age.cs`: get+set `("Hero",
    "REPUTATION")`, `40 → 38` (штатный clamp-safe сценарий; исходный
    `inter_kid3.age.cs`/`777` недостижим линейно и климпится в `50`).
  - Ограничения (D-050): identity-кривая модификаторов (рантайм-таблица
    не загружена/не доказана), `gate=0` default (getter силы не прибавляет
    `strength_bonus`, запись bonus не наблюдаема), `+0xF58` undead-gate не
    моделируется; engine quirk воспроизведён — все семь stat-setter'ов
    добавляют delta в bonus силы; лимиты захардкожены (primary 30,
    reputation ±50, EXP 500000), reputation half-multiplier опущен,
    NPC life/energy default 0.
- **Проверка (фактический вывод 2026-11-06, final review fix):**
  - `cmake --build build` — exit 0.
  - Targeted `ctest -R "gl_script_tests|gl_script_real_tests|
    gl_game_tests|gl_assets_tests|gl_assets_real_tests"` с `GL_GAME_DIR`
    — **5/5 passed**, 475.64 s, exit 0: `gl_assets_tests` 3.11 s,
    `gl_assets_real_tests` 416.55 s, `gl_game_tests` 52.57 s,
    `gl_script_tests` 0.42 s, `gl_script_real_tests` 2.94 s.
  - Полный `ctest --test-dir build --output-on-failure` с `GL_GAME_DIR`
    — **18/18 passed**, 1030.25 s, exit 0: `gl_assets_tests` 3.15,
    `gl_assets_real_tests` 414.37, `gl_game_tests` 51.09,
    `gl_core_tests` 0.36, `gl_core_real_tests` 0.44, `gl_script_tests`
    0.42, `gl_script_real_tests` 2.81, `glpak_cli_tests` 0.10,
    `gl_cs_tools_tests` 6.55, `camera_tests` 0.44, `utf8_tests` 0.13,
    `font_tests` 1.73, `glview_screenshot_tests` 100.51,
    `glgame_screenshot_tests` 421.54, `person_render_tests` 3.09,
    `level_click_tests` 10.78, `dialog_overlay_tests` 0.45,
    `inventory_overlay_tests` 8.66.
- **Вне scope:** `RS_IsPersonExistsI`, `RS_AddExp`, lifecycle, inventory,
  party, quests/time/world.
- **Следующее:** Phase 1b2 inventory natives (`P3-9` inventory-часть) или
  `P2-15` (save payload).

## Phase 1a — Server.dll VM parity & native dispatch (done 2026-11-06)

- **Задача:** снять `P3-4`–`P3-9`: сверить VM-семантику с `Server.dll` и
  ввести `NativeRegistry` (65 ID) + `GameScriptHost`; реально работают
  только нативы на существующем состоянии, остальные — типизированные
  `unsupported` с одной info-записью на ID.
- **Spec:** `docs/specs/2026-11-05-p3-vm-natives-design.md`.
- **Plan:** `docs/plans/2026-11-05-p3-vm-natives.md`.
- **Сделано (Tasks 1–7):**
  - Task 1: `tools/ghidra/ExportNativeHandlers.java`; экспорт всех 65
    handler'ов — `done natives=65 created=33 ok=65 failed=0`; корпус
    `.local/decomp/server/natives/{natives.csv,natives.c}`;
    `decomp-index.md` обновлён.
  - Task 2: `docs/research/p3-vm-natives-server.md` — layout узла,
    семантика `0x00..0x32` с RVA, delta-таблица (23 строки), нативная
    таблица 65/65 `ID → RVA handler`.
  - Task 3: `NativeRegistry` — 65 ID: 5 supported (`Exit`, `Signal`,
    `Console`, `Cmd`, `D_PlaySound`), 3 vm_internal (`D_Say`, `D_Answer`,
    `D_CloseDialog`), 57 unsupported с reason.
  - Task 4: `GameScriptHost` в `gl_game` — диспетчеризация через реестр,
    один info-лог на unsupported ID, warning на vm_internal/unknown.
  - Task 5: VM parity по §4 research (D-049): short-circuit `0x00/0x02`,
    порядок `b,a` и `1e10` для `/`/`%`, int32-сдвиги `cl&31`, операнд `b`
    у `0x13/0x14`, eval `0x31/0x32`, завершение top-level value-узлов.
  - Task 6: `glgame`/`glscript` переведены на `GameScriptHost`.
  - Task 7: регрессия + `STATUS`/`PROJECT`/`parity-matrix`.
- **Критерии приёмки:** research 65/65 `ID → RVA handler` + семантика VM;
  реестр 65/65, имена совпадают с `cs_function_name`; каждое изменение VM
  имеет evidence и тест; `glgame`/`glscript` на `GameScriptHost`;
  targeted + полный `ctest` с `GL_GAME_DIR` pass; `STATUS`/`PROJECT`/
  `parity-matrix` обновлены.
- **Проверка (фактический вывод 2026-11-06):**
  - `cmake --build build` — exit 0.
  - Targeted `ctest -R "gl_script_tests|gl_script_real_tests|gl_game_tests|
    glgame_screenshot_tests|glscript_tests"` — **4/4 passed**, 480.08 s,
    exit 0: `gl_game_tests` 43.68 s, `gl_script_tests` 0.43 s,
    `gl_script_real_tests` 2.07 s, `glgame_screenshot_tests` 433.81 s.
    Отдельного зарегистрированного `glscript_tests` нет (18 тестов в
    проекте), поэтому regexp выбирает 4.
  - `glscript --game-dir ... --file scripts/dialogs/demon.d1.age.cs --run`
    — `status: wait_user`, `steps: 43`, `dialogs: 2684 2686 2696`,
    `answers: 2`, exit 0.
  - Полный `ctest --test-dir build --output-on-failure` с `GL_GAME_DIR` —
    **18/18 passed**, 1224.01 s, exit 0.
- **Final review fixes (2026-11-06):**
  - `0x14 !` сравнивает **double** с `0.0` (evaluator `0x3a1ae`, executor
    `0x3ab3f`): `!0.5 == 0`; тесты `0.0→1`, `0.5→0`, `5.0→0`; research
    §3.1/§4 и D-049 исправлены.
  - Executor `/`/`%`: вычисляет `a`, затем `b`, всегда оба операнда
    (`0x3ac3a`/`0x3abb3`); `thread_local` убран, режим — явный
    `ScriptVm::EvalMode` (member + scoped executor для аргументов `func`).
    Тест: div/mod как аргумент функции с side-effecting операндами.
  - `Exit(nonzero)` останавливает VM: `ScriptHost::stop_requested` →
    `GameScriptHost` (флаг по `0x3fd38`) → `ScriptVm` (`end`); `Exit(0)`
    продолжает. Тесты на оба случая.
  - `NativeRegistry::call`: arity из research §5, warn + `0` при
    недостатке аргументов; безопасный `as_int` (non-finite/clamp);
    `size()`.
  - Minors: тесты vm_internal/Signal/`%` skip-a; удалены dead `17/18`,
    unused `<vector>`, stale `host.hpp`, redundant `gl_script` link;
    ASCII `-` в сообщении; Signal/`D_PlaySound` документированы как
    service stubs; plan Tasks 2–7 отмечены `[x]`.
  - **Проверка после final-fix:** `cmake --build build` — exit 0;
    `gl_script_tests` **79/79** (751 assertion), `gl_script_real_tests`
    **2/2** (46), `gl_game_tests --test-case=*script*host*` **7/7** (11);
    все exit 0. Полный `ctest --test-dir build --output-on-failure` с
    `GL_GAME_DIR` — **18/18 passed**, 1307.64 s, exit 0.
  - **Отложено (final review):** 555-script execution sweep (нужен новый
    harness); near-limit stack test (кадр `eval` 952 B при
    `kMaxEvalDepth=1024`, ≈0.97 MB стека Windows; cyclic-тест проходит);
    process-wide dedup one-shot логов; assertion на warning-ветку
    vm_internal; заметка о Signal/`D_PlaySound` как service stubs в
    `parity-matrix` (в research §5 уже есть).
- **Следующее:** Phase 1b — person natives (остаток `P3-9`: persons,
  inventory, quests, time, world) или `P2-15` (save payload).

## Состояние на 2026-11-05 (handoff)

- **Принято:** P4d (shadow draw-origin), A8 (cell masks: строка
  `person_y/18` + клетки кадра с тем же mask id), P5a (save → outfit).
- **P5b1 принят 2026-11-05:** read-only inventory UI (Tasks 1–5);
  финальный полный `ctest` после всех fix waves — **18/18 passed**,
  1278.27 s, exit 0; **manual gate PASSED** с одним отложенным
  пунктом (paper-doll overlays → P5b2).
- **Проверка P5b1 (final-fix + re-review):** `inventory_overlay_tests`
  19/19 (537 assertions), `glgame_screenshot_tests` 25 cases,
  `font_tests` 9/9, `gl_assets_real_tests` passed, `gl_assets_tests` passed;
  B-only puppet fix `7f13d89`; полный прогон — 18/18, 1278.27 s.
- **P5a-pending:** set A/B switching, parser deferrals (32 MiB, first
  `BT_HERO`, BOM).
- **P5b2 backlog:** inventory paper-doll overlays (ITM `puppet_name` +
  layer masks) на фигуре; отдельный spec/plan.
- **P5b1 manual gate PASSED:** overlay проверен на slot0/slot2;
  вкладка `inventory_skills` в P5b1 disabled. Отложено в P5b2:
  **paper-doll overlays** поверх фигуры (ITM `puppet_name` + layer masks).
- **Не запушено:** коммиты только локально.

## Parity matrix & decomp reference — Phase 0 (2026-11-05)

- **Задача:** зафиксировать измеримую основу полной реконструкции логики в
  современный C++20: матрицу покрытия и локальный декомпил-корпус
  `Server.dll`. Код порта не меняется.
- **Критерии приёмки:** все строки аудитов P0–P4b2 + P4c/P4d/A8/P5a/P5b1
  имеют статус и evidence; есть сводка counts и приоритетные пробелы;
  `decomp-index.md` содержит SHA-256 и число функций; корпус непуст;
  документы UTF-8 без BOM/NUL; `ctest` не требуется.
- **Сделано:**
  - `docs/research/parity-matrix.md` — 157 строк (на 2026-11-05):
    `verified` 44, `partial` 57, `unverified` 25, `missing` 16,
    `contradicted` 2, `superseded` 13. `contradicted`: `P1-3` (ADSC `number/name`) и
    `P2-9` (60 Гц vs `sv_fps=35`).
  - `docs/specs/2026-11-05-parity-matrix-design.md`,
    `docs/plans/2026-11-05-parity-matrix-phase0.md`.
  - D-048: результат — современный C++20; matching/VC6 не используется;
    корпус локален, в git только индекс и скрипт.
  - `tools/ghidra/ExportDecomp.java` + `docs/research/decomp-index.md`.
- **Проверка (фактический вывод):**
  - `parity-matrix.md`: 157 строк (на 2026-11-05); `verified` 44,
    `partial` 57, `unverified` 25, `missing` 16, `contradicted` 2,
    `superseded` 13.
  - Ghidra: `Server.dll` SHA-256
    `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`;
    импорт и анализ — 109 s; `done functions=1108 ok=1107 failed=0 thunks=1`.
  - Корпус: `.local/decomp/server/decompiled.c` 2 473 000 B,
    `functions.csv` 31 476 B (1108 строк), `meta.txt`.
  - Sanity корпуса: `_rand` найден по RVA `0x6c09f` (строка 896
    `functions.csv`, size 34), Ghidra опознала его как library function.
  - Документы: BOM=false, NUL=0; литеральный `\n` встречается только в
    правилах и путях (`wear\...`, `\r`/`\n`), не как артефакт.
  - Код порта не менялся; `ctest` для Phase 0 не требуется.
- **Следующее:** Phase 1 — `P3-4`–`P3-9` (VM и нативы из `Server.dll`),
  затем `P2-15` (save payload) и `P2-9`/`P4b-16` (35 Гц).

## P5b1 final review fixes (2026-11-05)

- **Android/no-font panel:** добавлен `gl::render::Font::load_from_memory`;
  `glgame` при отсутствии системного шрифта (Android) или `--no-font`
  читает VFS `engineres/fonts/ttf/arial.ttf` в отдельный `inventory_font_`.
  `--no-font` по-прежнему даёт ASCII для меню/диалогов/уровня, но правая
  панель инвентаря получает TTF (D-047).
- **Puppet A/B overlap:** D-046 — показывается active set A (как P5a);
  B-слот пары (`weapon 0x002000`, `shield 0x800000`) скрывается при наличии
  A и учитывается в hit-test только когда A нет. Real slot0: клик в
  перекрытие `(30,480)` выбирает A=486 (топор), а не невидимый B=554.
- **P5b1 re-review:** hit-test puppet сначала проверяет слоты с предметом,
  пустые — только как fallback для очистки. Пустой A-слот больше не
  перехватывает клик по видимому B. Real slot0 shield B (item 353,
  mask `0x800000`) выбирается тапом в центр B `(238,493)`; при наличии
  обоих A остаётся приоритет A.
- **E2E selected panel:** `glgame --inventory-select <n>`; screenshot-тесты
  сравнивают пустую панель и выбранные слоты 0/1 на 1024×768, 1280×720 и
  600×800 (портрет), плюс `--no-font` panel-тест.
- **Android input:** `SDL_FINGERDOWN/UP` = tap, вертикальный
  `SDL_FINGERMOTION` (шаг 24 px) = page/panel scroll; синтезированные
  touch-mouse события в overlay игнорируются; `SDLK_AC_BACK` = Esc.
- **Minor:** selection очищается при фильтре, скрывшем предмет, и при клике
  в пустой hot/puppet; панель показывает `(пусто)` для пустого выбора/строк;
  stack count — маленький bitmap-шрифт с fit/clamp; активный фильтр имеет
  pressed-подсветку; panel scroll не требует предварительного render
  (`set_font` + `ensure_panel_scroll_max`); `parse_hero_equipment` ловит
  `bad_alloc`; unit fallback `ItemCatalog` на пустом VFS.
- **Проверка:** targeted `ctest -R
  "inventory_overlay_tests|glgame_screenshot_tests|font_tests|gl_assets_real_tests|gl_assets_tests"`
  с `GL_GAME_DIR` — **5/5 passed**, 876.64 s (до re-review).
  Re-review: `ctest -R "inventory_overlay_tests|glgame_screenshot_tests"` —
  **2/2 passed**, 395.37 s; `inventory_overlay_tests` 19/19
  (537 assertions). `glgame_screenshot_tests` 25 cases,
  `font_tests` 9/9, `gl_assets_real_tests` passed (452.90 s),
  `gl_assets_tests` passed.
- **Manual gate OPEN:** агент pass не подтверждает; см. раздел P5b1.
- **Commits:** `8f5259d` (fix+tests), docs-коммит этой записи.

## P5b1 — read-only inventory UI (accepted 2026-11-05)

- **Задача:** показать read-only инвентарь выбранного save в оригинальном
  UI 1024×768: нижняя полоса (container 1), hot slots (container 0),
  paper-doll (container 2), фильтры/скролл/правая панель.
- **Реализовано:** Tasks 1–5.
  - `gl::assets::parse_hero_inventory(body)` — все три контейнера `BT_HERO`
    (`0x734`, `0xC0 + 4 + subcount*0x14`): `hot`/`items`/`equipped`,
    `InventoryItem { item_id, count, flags, type, style }`;
    `parse_hero_equipment` сохранён для P5a. Контейнер 0=hot, 1=items,
    2=equipped; equipped count = 1.
  - `gl::assets::ItemCatalog::load/find` — `sdb/items/lit_names.sdb`,
    `descriptions.sdb`, lazy `items/data/<id>.itm`, icon
    `items/res/<icon_name>` (нормализация `\`→`/`, lower-case), fallback
    `engineres/interface/bag`.
  - `gl::game::visible_inventory`/`clamp_page` — эксклюзивные фильтры
    id9..15 (`1,2,4,8,0x10,0x20,0`), type 18 (money) видим всегда,
    offset-пагинация 12 слотов, clamp `0..max(0,count−12)`.
  - `gl::app::InventoryOverlay` — letterbox 1024×768; фон
    `inventory_secondary`; bottom 12 слотов 55×55 `x=140…811,y=703`; hot slots
    1..9 27×27 (`y=624`, назначение по `(flags&0xF000)>>12`); puppet
    55×55 по `flags&0x00FFF000`; фильтры/скролл ±1/±3/exit; правая
    панель D-045 `(654,44,360,520)`; иконки native-size 55/45/30/22 без
    scale, центрирование `off=max(0,(target−tex)/2)`; стек-счётчик при
    count>1.
  - `glgame --inventory`/`I`/Esc/консоль `inventory`; inventory и outfit
    читаются из одного выбранного save (P5a); без save/невалидный слот —
    пустой overlay, exit 0; ввод уровня при открытом overlay блокируется.
- **Иконки (port behavior):** BMP-иконки предметов 8bpp indexed
  используют color-key **magenta `(255,0,255)`**: ровно эти пиксели
  переводятся в `alpha=0` перед upload. Keying применяется только к
  icon-текстурам; фон/кнопки (`inventory_secondary`, `filter*`, `exit` и
  т.п.) не keyed; CSX сохраняют палитровую прозрачность.
- **Проверка:** re-review targeted `ctest` с `GL_GAME_DIR` — **2/2 passed**,
  395.37 s; final-fix прогон — 5/5, 876.64 s. `inventory_overlay_tests`
  19/19 (537 assertions; magenta-key regression на реальной иконке 318,
  нулевые magenta-пиксели в слотах, active A/B, пустой A + видимый B,
  фильтр/пустой слот, panel scroll без render, active filter),
  `glgame_screenshot_tests` 25 cases (`--inventory`, `--equip-save 0`,
  invalid slot, `--exec inventory`, `--inventory-select` на 1024×768/
  1280×720/600×800, `--no-font` panel), `gl_assets_real_tests` passed
  (452.90 s в этом прогоне), `font_tests` 9/9, `gl_assets_tests` passed.
  Финальный полный прогон после всех fix waves — 18/18, 1278.27 s.
- **Commits:** `5e9156f`, `827e357`, `ee9610a`, `d7d854a`, `e813f4d`,
  `4c44324`, `0b9347a`, `f2c4d2c`, `8f5259d`, `7f13d89` (+ docs commits).
- **Manual gate PASSED 2026-11-05:** overlay проверен на slot0/slot2.
  Paper-doll overlays поверх фигуры (ITM `puppet_name` + layer masks)
  отложены в отдельную задачу **P5b2**.
- **Не входит:** use/drop/equip и изменение save; skills/parameters
  (P5c); оригинальный tooltip (заменён D-045); overlay — снимок
  inventory на момент открытия; on-screen touch inventory button (P1b).
- **Spec/plan/research:** `docs/specs/2026-11-04-p5b1-inventory-ui-design.md`,
  `docs/plans/2026-11-04-p5b1-inventory-ui-implementation.md`,
  `docs/research/p5b1-inventory-ui.md`.

## P5a — save → hero outfit (accepted 2026-11-04)

- **Задача:** выбор save-слота в существующем меню применяет
  экипировку из `BT_HERO` к hero (только outfit, без загрузки мира).
- **Реализовано:** Tasks 1–4.
  - `gl::assets::decode_save_body(file)` — header + zlib, возвращает тело
    `BT_HERO`; `gl::assets::parse_hero_equipment(body)` — fixed prefix
    `0x734`, 3 контейнера, equipped — третий; запись
    `0xC0 + u32 subcount + subcount*0x14`; `EquippedItem { item_id, type,
    flags, style }` (`+0xA8`, `+0x08`, `+0x0C` raw, `+0x20`).
  - `gl::assets::parse_herowear` (`scripts/herowear.scr`) +
    `gl::game::resolve_hero_outfit(equipment, tech_names, wear)`:
    item id → TECH (`sdb/items/tech_names.sdb`) → group; активен set A,
    base set — из weapon либо `noweapon_thrw`.
  - `LevelScreen::apply_hero_outfit` перезагружает wear без reload уровня;
    `glgame --equip-save <slot>` и пункт меню читают все equipped-записи
    (`HeroEquipment`, включая аксессуары) в `pending_equipment_`; resolver
    строит `HeroOutfit` только в `load_level`/применении. Headless hook
    `--equip-save <slot> --equip-save-pending` проверяет путь
    menu → pending → следующий `load_level`.
- **Fallback:** ошибка чтения/декодирования/parse/asset-load → naked hero
  (`noweapon_thrw`, пустые слои) + warning; неизвестный отдельный item
  сбрасывает только свою категорию. Меню/уровень не блокируются.
- **Real-data:** slot0 → `486,318,280,397,554,353,72,80,233,245,141`;
  slot2 → `486,317,278,396,554,359,66,99,236,243,108`.
  Оба разрешаются в `wear_set=two_axes_maces`, `weapon=axe_goluboy2`,
  `helmet=shlem_goluboy`, `elbarms=ponogi_goluboy`; armor —
  `dospeh_almaz` (slot0) / `dospeh_cherny` (slot2). Все 18 A+B asset
  путей slot0/slot2 есть в `wear.pak` (18/18); `shlem_cherny` не
  используется (отсутствует в `herowear.scr`/`wear.pak`).
- **Проверка:** `cmake --build build` — успех; targeted
  `ctest -R "gl_assets_tests|gl_game_tests|glgame_screenshot_tests"` с
  `GL_GAME_DIR` — **3/3 passed**, 314.38 s. `gl_assets_tests` 185/185
  (1237 assertions), `glgame_screenshot_tests` 21/21 (включая
  `--equip-save`/`--equip-save-pending` pending-load case),
  `gl_game_tests` passed. Финальный полный `ctest` на `adc3ba2` —
  **17/17 passed**, 1016.00 s, exit 0.
- **Manual gate PASSED 2026-11-04:** пользователь сравнил hero в `glgame`
  для slot0 и slot2 с оригиналом на тех же save — визуально совпало.
  Остаточный риск: выбран активный set A; переключение A/B после загрузки
  не доказано (вне P5a).
- **Spec:** `docs/specs/2026-11-04-p5-save-outfit-design.md`.
- **Research:** `docs/research/p5-save-equipment.md` — BT_HERO layout,
  0/N на 28 save, item→TECH (`tech_names.sdb`), herowear groups, slot0/slot2.
- **Plan:** `docs/plans/2026-11-04-p5a-save-outfit-implementation.md`.
- **Статус:** implementation automated passed; **P5a не принят** до
  пользовательского visual gate.

## A8 exact cell-mask research (resolved 2026-11-04)

- **Задача:** понять точную семантику `FUN_1202BF60`/`FUN_1202BC54` для
  выбора клеток и `mask_id`.
- **Результат:** внешний x86 hook-tracer (hardware breakpoints,
  `EFlags.RF`, read-only) показал: `ApplyCellMask` — одна строка
  `cell_y` персонажа по x-диапазону кадра; `MaskRenderer` рисует
  foreground для всех клеток кадра с тем же `mask_id`.
- **Проверка:** hook-лог `person(74,18) → Apply(72..75,18) →
  Renderer(72..75,12..19)`; реализация в `LevelScreen::draw_rect_masks`.
- **Статус:** **PASSED**, пользователь проверил угол/полное
  перекрытие/дерево; research — `docs/research/a8-mask-parity.md`
  (RESOLVED).

## P4d — shadow/mirror draw origin (fixed 2026-11-04)

- **Live-факт:** в оригинале (PID 22800, `l1_1`) source-rect тела и тени
  (`render item +0x0C` и `+0x1C`) имеют **одинаковую колонку**; ряд тени —
  identity-направление, ряд тела — PAD-таблица (8/16). Поэтому «отставание»
  тени было не фазой анимации, а **сдвигом draw-origin**.
- **Root cause:** для mirrored-строк оригинала body anchor = `frame_width -
  anchor_x`, а не `anchor_x`. Live: `rt_stay` 65−34=31 (вместо 34),
  `rt_go` 75−34=41; тень при этом остаётся на static shadow anchor
  (`rt_stay` 45/46, `rt_go` 50/51, ±1 px). Порт рисовал body на
  `x−anchor` и делал `SDL_FLIP_HORIZONTAL` вокруг центра — тень
  оказывалась на ~6 px (≈1 кадр хода) позади тела.
- **Fix:** `gl::app::person_body_draw_x(x, frame_width, anchor_x, mirror)`;
  используется в `place_person[_px]` и `draw_hero_wear`. Тень не зеркалится
  (identity-ряд, static anchor) — как в live.
- **Проверка:** новый regression-тест `mirrored placement flips the draw
  origin around the anchor`; targeted `person_render_tests` 18/18,
  `level_click_tests` 22/22, `glview_screenshot_tests` и
  `glgame_screenshot_tests` passed; полный `ctest` — **17/17 passed**,
  1039.94 s.
- **Open:** пользовательский visual gate (тень при движении вниз-вправо,
  страж, голова у угла дома). Live shadow-anchor отличается от HAD на
  +1 px для mirrored-направлений — ниже порога кадра, не правилось.

## A8 parity — cell mask `ApplyCellMask` (implemented 2026-11-03)

- **Реализовано:** `Level::MaskCellGrid`/`mask_numbers` из LVL;
  `gl::game::select_cell_mask` (точная модель `FUN_1202BC54`);
  `LevelScreen` рисует маску только для клетки персонажа/hero, тело и тень;
  baseline-merge удалён.
- **Gate PASSED:** `GoldenLand.exe FUN_00426490` строит grid напрямую из
  LVL: `flags = relief | (sound<<16)`, `mask = LvlTile.mask` (`0xFFFF→-1`);
  размеры — `BLK_MHDR`; `flags&1/&2` — биты `relief`.
- **Проверка:** hero у двери `(90,12)`: masked vs plain различаются только
  выбранной клеткой правого края (441 px), полного перекрытия нет;
  стражник/персонаж за башней: 206 px перекрытия (выбранная клетка);
  `real l1_1 cell mask grid matches live data`; `gl_game_tests` 98/98
  (557 146 assertions); полный `ctest` — **17/17 passed**, 1485.32 s.
- **Mask row by hook:** маска выбирается в строке `person_y/18` по
  x-диапазону кадра, затем foreground рисуется для всех клеток кадра с
  тем же `mask_id` (см. RESOLVED ниже); не «все клетки» и не только низ.
- **Hero direction16:** `Hero::direction16` сохраняется из
  `direction16_from_velocity`; `LevelScreen` использует его для walk-строки
  тела/тени (раньше `direction8_to16` давал только чётные направления —
  диагональная анимация/тень ломались).
- **Live hero animation 2026-11-03:** оригинал на `l1_1` рисует hero из
  `rt_stay` (surface 780×460, shadow 864×304), не `tb_stay`. Порт переключён
  на `rt_stay`/`rt_go` (65×92 / 75×96, shadow 72×38 / 84×42); HAD anchors
  `(34,78)`/`(45,22)` совпали с live draw. После правки полный `ctest` —
  **17/17 passed**, 2002.77 s.
- **RESOLVED (2026-11-04):** A8-маски. Внешний x86 hook-tracer
  (hardware breakpoints `FUN_1202BC54`/`FUN_1202F17C`, RF, read-only)
  показал: `ApplyCellMask` идёт по **одной строке `cell_y` персонажа**
  (`renderer+0x958C`) и x-диапазону кадра, а `MaskRenderer` рисует
  foreground для **всех клеток кадра с тем же `mask_id`**. Порт:
  `LevelScreen::draw_rect_masks` собирает id в строке `person_y/18`,
  затем рисует только клетки кадра с этим id. Формула не эвристика —
  снята hook'ом. Детали — `docs/research/a8-mask-parity.md` (RESOLVED).
- **Manual gate PASSED:** пользователь проверил угол дома, место правее,
  полное перекрытие и дерево — ок. Regression
  select_row_mask_ids collects ids across the person row; полный ctest
  — **17/17 passed**, 1155.42 s.
- **Открыто:** проверить у пользователя диагональную тень и стража; при
  необходимости сверить draw/shadow anchors NPC с live.
- **Ограничения:** одна клетка occupancy (без multi-cell больших спрайтов);
  маски применяются только к персонажам/hero, не к статикам; рантайм-смены
  масок дверями/скриптами и alt-маски — вне задачи.
- Spec/plan: `docs/specs/2026-11-03-a8-mask-parity-design.md`,
  `docs/plans/2026-11-03-a8-mask-parity.md`.
- **Manual gate:** P4d тень подтверждена; A8 подтверждён пользователем
  (угол, место правее, полное перекрытие, дерево). P4d/A8 manual gate —
  **PASSED 2026-11-04**.

## P4d — hero equipment/render (implemented 2026-10-25; manual gate OPEN)

- **Реализовано:** `gl::assets::parse_wear_had` (HAD), `parse_wear_seq` (SEQ),
  `parse_iad` расширен (layer offsets, shadow frame/offset/crops);
  `gl::game::load_hero_visual` + `compose_wear_body/shadow` (engine-формула
  `FUN_0042279C`, порядок слоёв `weapon,shield,elbarms,helmet,armor`);
  `LevelScreen` рисует hero из готовых листов (`position - anchor`, тень
  проходом 2); `glview --hero-wear/--hero-set/--hero-{armor,elbarms,helmet,
  shield,weapon}/--hero-outfit none`; `glgame` — slot0 outfit.
- **Паритет:** golden FNV-1a композита slot0 idle = `0x1A0C2E25C84A1174`
  (совпал); реальные значения HAD (`tb_stay` 88/12/48×92/anchor 24,77;
  `tb_go` 82/65×95/movement 5.8333,5.0), IAD type4 (off 165,154; shadow
  40×20/152,205) и SEQ проверены.
- **Проверка:** `cmake --build build` — успех; `gl_assets_tests` 164/164
  (1 skip), `gl_assets_real_tests` 20/20 (real wear 5/5), `gl_game_tests`
  86/86 (8 skip), `glview_screenshot_tests` 11/11, `glgame_screenshot_tests`
  20/20, `level_click_tests` 22/22; полный `ctest` с `GL_GAME_DIR` —
  **17/17 passed**, 1036.25 s (до shadow fix).
- **Скриншот (агент):** `glview l1_1 --no-persons --no-masks --camera-x 1982
  --camera-y 94 --hero-wear` — hero в синем slot0-наборе (Almaz + Goluboy +
  Shlem_goluboy) с тенью; отличие от кадра без hero 1833 px.
- **Manual desktop gate OPEN:** пользователь должен запустить
  `glgame --game-dir "I:\Games\Златогорье-2" --level l1_1` и/или `glview`
  с `--hero-wear`, проверить внешний вид/анимацию idle+walk, тень, края
  слоёв и отсутствие «плавающих» offset; агент manual pass не подтверждает.
- **Manual findings 2026-10-25/26 (gate NOT passed):**
  - **Z-order:** hero перекрывается зданием — это дефект A8 mask-модели
    (baseline-приближение), не P4d. Воспроизведено: `click-world 180 32`,
    `sim-ms 300`; hero `(2166,216)` под baseline маски 8 (`y=-17`, bottom
    218), при `y=263` виден. `Client.dll FUN_1202bc54` использует per-cell
    `mask_id` + continuity bits; LVL `tile.mask` (в точке `(180,24)` = 6) —
    это 0-based индекс в списке `BLK_MDSC` (`lvl.masks[6].number == 8`).
    Нужен отдельный подпроект A8-parity (`ApplyCellMask`, `tile.mask`,
    `+0x8344`). До него P4d manual OPEN.
  - **Чёрные точки при анимации — исправлено 2026-11-03.** Причина:
    shadow-композит брал destination-шаг из IAD `F/G`
    (`shadow_frame_width/height`), а engine берёт destination из HAD
    `shadow_width/height`; source — из `F/G`. Из-за сдвига слои тени
    рассыпались в точки. Фикс в `compose_wear_shadow` + regression-тест
    `compose_wear_shadow uses base shadow stride for destination columns`;
    формулы исправлены в research/spec/plan. Скриншот walking-hero
    показывает чистую тень. После shadow/NPC-alpha фиксов: gl_game_tests 86/86 (67 226 assertions), полный ctest 17/17, 1914.40 s.
  - P4d остаётся `automated passed, manual OPEN` только из-за z-order —
    до реализации A8-parity.
- **Ограничения:** экипировка из save не подключена (outfit задаётся
  явно; item→tech/`herowear.scr` — P5); рендерятся только `tb_stay`/`tb_go`
  (hits/die/attack загружаются, но не выбираются); точная shadow-пара
  `shadow_anchor`/порядок не сверена пиксельно; `run` не подключён.
- **Следующее:** A8 mask parity (ApplyCellMask/	ile.mask) для z-order, затем manual gate P4d; далее P5 (item→herowear) или P1b.

## P4d — research (closed 2026-10-25)

- **Базовая модель найдена:** отдельного файлового PAD у hero нет; база — `wear.pak` →
  `wear/noweapon_thrw/animation/tb_stay.csx` (idle, 576x460 = 12x5,
  кадр 48x92). Слои экипировки — `armor/helmet/elbarms/shield` с `.iad`+CSX.
  `scripts/herowear.scr` — карта item-групп на wear-каталоги.
- `.hero` — статы/скиллы (первые 7 int32 = Mage 6/7/9/10/13/15/9), не внешность.
  `hero_generator.scr` — только GUI; preview — концепт.
- **Сделано:** `LevelScreenOptions::draw_hero_base`; `LevelScreen` загружает
  `wear/noweapon_thrw/animation/tb_stay.csx` и рисует idle (row0 = back,
  48x92, low-center) под hero; `glgame` включает base, `glview --hero-base`.
  Скриншот `glview --hero-base --no-persons --no-masks --camera-x 1982
  --camera-y 94` показывает базового hero. Тесты: `glview_screenshot_tests`
  10/10, `person_render_tests` 18/18, `level_click_tests` 22/22;
  `glgame_screenshot_tests` 20/20.
- **Направление:** live slot0 показал hero **спиной** (SEF direction 0);
  правильно `pad_person_row(tb_stay)` без инверсии: row0 = back, row4 = front.
  Раньше стояла инверсия `row = 4-row`, из-за неё hero был боком/лицом —
  это и давало «криво». Walk `tb_go` 60x95/13 колонок/80 ms, 16-direction;
  `run` пока не подключён.
- **Slot0 outfit overlay:** live original PID 9640, hero entity `0x3BFA400`;
  в памяти есть и `wear\bows\...`, и `wear\noweapon_thrw\...`, но на
  live-скриншоте у hero **нет колчана**, значит активная база —
  `noweapon_thrw`, не `bows`. Item ID: `318` armor Almaz, `397` elbarms
  Goluboy, `280` helmet (tech `HLM_1_12_1`, группа `Shlem_goluboy`;
  полное имя «Шлем из чёрного металла удара судьбы»).
  `hero_armor_`/`hero_legs_`/`hero_helmet_` — noweapon: base 48x92,
  armor 36x37 offset `(7,17)`, legs 36x51 offset `(9,36)`,
  helmet 17x23 offset `(18,9)` (row0). Направление — row0=back без
  инверсии. Осталось сверить точные anchors/offset с оригиналом.
- IAD-блоки: заголовок 8×u32 (`type,size,frames,fw,fh,iw,ih,table_offset`);
  `dospeh_kamen` блок type4: 12 кадров, 36x37, таблица 60 записей по 8 байт
  (`x,y,anchor_x,anchor_y` гипотеза). Точная семантика/анкоры ещё не
  подтверждены.
- **IAD parser:** `gl::assets::parse_iad` (`src/assets/.../iad.hpp/.cpp`)
  читает контейнер блоков (`type,size,frames,fw,fh,iw,ih,table_size`) и
  crop-таблицу `x,y,w,h`; synthetic + real `dospeh_kamen.iad` tests
  (блоки 1/4, 12 кадров, 60 crop-записей). `gl_assets_tests` 158/158,
  `gl_assets_real_tests --test-case=*dospeh_kamen*` 1/1.
- **Сверка с оригиналом:** item ID `318/397/280` из slot0 →
  `ARM_1_13_1`/`ELB_1_11_2`/`HLM_1_12_1` → `Dospeh_almaz`/
  `Ponogi_goluboy`/`Shlem_goluboy`; `wear.pak` содержит эти каталоги.
  `T_HELM\Cherny`/`shlem_cherny` — старое неверное предположение.
- **UNBLOCKED 2026-10-25:** engine-схема извлечена полностью, см.
  `docs/research/p4d-hero-wear.md`. `FUN_004239C0` (wear loader) →
  `FUN_00423334` (IAD) → `FUN_0042279C` (композитор) → PersonVisual;
  `Client.dll 0x1202C424` рисует его как PAD. HAD даёт кадр/колонки/ряды,
  **anchor** (`tb_stay` 24,77), speed (`movement`), layer offsets; IAD даёт
  crop-таблицы и смещения слоя; `.seq` — порядок слоёв по кадрам.
- Точная формула слоя: `dest = col*fw + layer_off + crop.x1/y1 - aoff`;
  crop из IAD (u16 x1,y1,x2,y2). Проверено на `tb_stay` row0/4 +
  `dospeh_almaz/ponogi_goluboy/shlem_goluboy`: смещения `(8,17)`, `(9,46)`,
  `(18,3)`; композит совпал с оригиналом.
- Осталось для принятия: spec+plan, реализация HAD/IAD/SEQ + композитора и
  hero-рендера (draw = `position - anchor`), выбор набора/экипировки,
  тени, parity-тесты и manual gate. Item→tech (`herowear.scr`) — открытый
  вопрос P5, геометрию P4d не блокирует.

## P4c parity debt — визуал Вертаса и 6-осевая CanInteract (открыто, 2026-10-24)

- Закрыто исследование PAD: отдельного `persons\hero` PAD нет
  (`scripts/persons_res/hero.prs` — только звуки). Live entity `0x1AE04E90`
  (`Вертас`, мир `(438,1974)`). Пользователь подтвердил: внешний вид Вертаса
  меняется от надетых предметов, NPC используют готовые пресеты. Значит,
  точный визуал — базовая модель + wear/equipment overlay (`wear.pak`/IAD),
  а не один PAD. Текущий hero-маркер остаётся; точный рендер — отдельная
  задача «hero equipment».
- `Client.dll 0x120b2fb8` — 6-осевая проверка; hero-функции
  `0x1201fb10/0x12020620/0x120205b8/0x120204e8/0x12020550/0x12020380/0x12020468`
  берут кадр/анкор героя и клампят к тайлу, сравнивая с NPC `+0x34..+0x4c`.
  Привязка порогов к направлениям/экипировке ещё не доказана; AABB остаётся.
- Live-проверка раздетого героя (slot21, PID 20884): entity `0x19FA2AF8`,
  имя `Вертас` по `+0x44`, мир `(2250,1426)`; в кадре 9 сущностей. У сущности
  нет прямого/indirect `PAD `-указателя; resource cache `+0x410` содержит
  чужие PAD (P1164/P1049). `slot21.sav` после 112-байтового заголовка —
  **zlib**: decompress даёт 839650 B; внутри script-переменные, item ID
  числовые, полного save-парсера нет. `hero_generator.scr` — только
  GUI; preview в `main.bmp` статичен. Базовая модель Вертаса пока не
  идентифицирована; p1165/p1166 пользователь отклонил.
- Следующая parity-задача: hero equipment/базовая модель (PAD+wear) —
  сначала decode IAD/CSX и найти базу (live/resource cache или generator),
  затем wear overlay; после этого точная 6-осевая формула и parity-тесты.
  P4c остаётся принятым; parity debt не должен ломать P4c acceptance.

## P4c — клик → диалог (принят 2026-10-24; automated + manual gate passed)

- Реализованы Tasks 1–8: SEF `points_entrance`; `Hero`/`HeroController` (spawn
  на первом walkable entrance, путь через `find_path`, AABB `can_interact`,
  скорость `iso_speed`); hero-маркер 20 px участвует в Y-sort и масках;
  `ClickTarget`/`click_target` и hit-test по PAD-кадру; `dialog_script_path`;
  `DialogOverlay` поверх уровня; в `glgame` — `--click-npc`/`--click-world`,
  pending talk до подхода и `Esc` → возврат в уровень. D-038.
- Whole-branch review fixes (D-039): клик по NPC целится в текущую клетку
  `PersonMovement` (`tile_at`), а не в SEF spawn; при смещении NPC pending talk
  пересчитывает цель и сбрасывается при отказе пути. ПКМ очищает hero
  goal/path и pending talk; non-talkable NPC не ставит цель; недостижимая
  свободная цель отклоняется без fallback-соседа (fallback — только для
  клетки, занятой NPC). Пока активен dialog overlay или save slots, WASD/
  стрелки и wheel не панорамируют/не зумят уровень.
- Real `l1_1` automated: индекс `L1.P621_Svet_3CircusWorkerV` берётся из
  загруженного `Level::persons` (не из сырого SEF; фактически 13).
  `glgame --level l1_1 --click-npc 13 --screenshot` → exit 0, герой доходит,
  открывается `scripts/dialogs/l1_1/l1.p621_svet_3circusworkerv.d621.age.cs`,
  в логе `speaker='L1.P621_Svet_3CircusWorkerV'`, `answers=2`, непустой
  `text_len`; нижние 220 px отличаются от кадра без клика больше чем на 10%.
  Движущиеся talkable NPC `L0.P678_citizen4` (MOVED_FLIP) и
  `L0.P7_Kolbasens_Rodstvennik1` (RANDOM_RADIUS) также открывают
  `scripts/dialogs/l1_1_nap/l1_svet_malepeasant1.age.cs` с непустым текстом.
  Невалидный индекс `--click-npc` не открывает overlay и не меняет кадр
  (E2E); ветка non-talkable NPC без `scr_dialog` покрыта unit-helper, но не
  real E2E — в `l1_1` нет такого NPC. Отсутствующий dialog script — unit +
  warning-путь; `--click-world` проверен на не-spawn клетке `(218,50)`.
- Проверка: `cmake --build build` — успех; `glgame_screenshot_tests` — **20/20**,
  391 assertion; `level_click_tests` — **22/22**, 71 assertion; `gl_game_tests`
  — **82/82**, 67 211 assertions (8 skipped); полный `ctest` с `GL_GAME_DIR`
  — **17/17 passed**, 865.27 s, exit 0.
- **Manual desktop gate PASSED 2026-10-24:** пользователь проверил в `glgame`
  клик по NPC → подход → диалог поверх уровня, `Esc` → уровень, клик по земле и
  NPC без диалога. **P4c принят.** Известные ограничения: hero — маркер, а не
  PAD Вертаса; `CanInteract` — AABB-приближение вместо 6-осевой формулы
  оригинала; серверный `0x12`/`OpenDialog` не реализован; точный PAD/визуал
  Вертаса — вне P4c.
- Spec/plan готовы: `docs/specs/2026-10-23-p4c-click-dialog-design.md`,
  `docs/plans/2026-10-23-p4c-click-dialog.md`.
**Реверс клиента:**

- Live-тест: один левый клик по дальнему стражнику → герой идёт к нему, затем
  диалог открывается сам. `Shot0000.bmp` SHA-256 `58E5054C…376C1F`: синий
  герой слева, коричневый стражник справа, ~90 px; диалог уже открыт.
- Сервер: talk `0x12` → `OpenDialog`, проверки расстояния нет (см. research).
- Клиент: talk-курсор `0xc` (`talk.ani`) ставится по флагу `+0x14 & 0x40`
  (`FUN_120c80a4`, `0x120c8142`) — клиент различает talkable NPC без
  distance-гейта. `gv_meet_offset=0.1` (`0x12106adc`) используется в
  `FUN_120b3464`/`FUN_120b3574`, но это AI/meet-предикаты из debug/AI-панели,
  не доказанный обработчик клика.
- Ввод мыши — в `GoldenLand.exe` (`7854019E…09FF`): `FUN_0040ad04` считает
  edge-флаги ЛКМ/ПКМ/СКМ, позицию и double-click; `gv_double_click_speed`/
  `gv_mouse_speed` = 1.2; vtable ввода `0x47fce0`, геттеры `+0x50`/`+0x54`.
- World-click найден в Client.dll: `CLevelScreen::update` (`0x1203f7e4`) →
  `0x12041520` (опрос input) → `0x12040084` (left-click) / `0x12040f68`
  (right-click). Для entity `+0x9c == 0x32` и action `+0xc0` 6/7 ставится
  команда в очередь `0x12040c28` (code `0x4f`/`0x52`/`0x45`/`0x56`).
  Узел 0x14 байт `{id, code, arg, ?, next}`, head `screen+0x4`.
- Очередь читает `CLevelScreen::update` (`0x1203f7e4`, возвращает head
  `screen+0x4`); wrapper `0x1204cf39` → `0x1203e3a8` (таблица
  `0x12126164`, коды `0x27..0x45`). Для уровня `0x1205625c`: code `0x4f`
  → `0x1204bf94(entity id)`, также `0x56`/`0x46`/`0x47`/`0x41`/`0x4b`/`0x43`.
  `0x1204bf94` — ветка `entity+0x8 == 6` → `0x1204c361`, кадры героя
  `0x12`/`0x16`.
- Локальный диалог: `0x1205625c` при code `0x27` → `0x1204c7ac` (открыть) →
  `0x1204c464` (панель/строки). Клиент умеет открывать диалог локально, не
  дожидаясь server `0x12`/`OpenDialog`; server-путь — сетевая ветка.
  `gv_meet_offset` в этом пути не участвует — это AI/meet. ~90 px на скриншоте
  нельзя объявлять фиксированным радиусом.
- Детали — `docs/research/p4b2-render-order.md`, раздел P4c.

## A8 — Cell mask / foreground occlusion (реализовано, 2026-10-23)

- Причина перекрытия на `Shot0001` — не Y-sort, а foreground-маски:
  `BLK_MDSC` (62 записи) + `levels/pack/l1_1/bitmaps/masks/mask_<number>.csx`.
  `mask_45` 687×741 при (885,1275) — деревянная вышка; alpha=0-силуэт точно
  совпал с вышкой в `layer.jpg` (182090 px). D-037.
- Порт: `LevelMask` в `Level`; `compose_mask_foreground` (фон через alpha=0);
  `LevelScreen` вставляет маски в Y-отсортированные проходы теней/тел по
  baseline `y+height`, tie-break `x+width/2`. Добавлены
  `--camera-x/--camera-y`, `--place-person`, `--no-masks` в `glview` для
  проверки.
- Проверка: `person_render_tests` — 18/18, 84 assertions; `glview_screenshot_tests`
  — 10/10, 112 assertions; кадр «персонаж за вышкой» с масками и без:
  разница >500 px, SHA-256 `219D87D8…B701F0` / `E2A55F80…DEFB6`. Полный
  `ctest` с `GL_GAME_DIR` — **15/15 passed**, 630.63 s, exit 0.
- Осталось: точная семантика `ApplyCellMask`/`r_masks_mode`/alt-масок;
  интерактивный gate после появления hero. Spec/plan:
  `docs/specs/2026-10-23-a8-cell-mask-occlusion-design.md`,
  `docs/plans/2026-10-23-a8-cell-mask-occlusion.md`.

## A7 — Y-sort и анкоры (реализовано, 2026-10-23; скриншоты оригинала получены)

- Реверс завершён: `FUN_120c8e68` собирает видимые сущности (`DAT_12111360+0x95d0`,
  128), `FUN_1202effc` рисует статики (`FUN_1202d53c`) и LAO-анимации
  (`FUN_1202d984`) до динамических сущностей, затем сортирует список `qsort`
  (`FUN_120e7e90`) компаратором `FUN_1202bbdc` и выполняет 9 проходов.
  `EF_PERSON` (type 2): ключ `+0x1c` (Y), затем `+0x18` (X); проход 2 —
  тени (режим 2, alpha 0.5), проход 7 — тела (режим 1, alpha 1.0).
  Анкоры PAD: `draw = position - anchor`; источник кадра — полная ячейка.
  Детали — `docs/research/p4b2-render-order.md`; D-036.
- Порт: `PersonDrawKey`/`sort_person_indices` (stable sort по Y, затем X) в
  `person_render`; `frame_rect` (полная ячейка, crops не применяются);
  `LevelScreen` рисует тени и тела двумя проходами по одному порядку.
  Статики/анимации остаются до динамических сущностей.
- Проверка: `cmake --build build` — успех; `person_render_tests` — 14/14,
  63 assertions; targeted real `l1_1` — 7/7, 489 891 assertions; `glview l1_1`
  t=0 и t=10 с — PNG 1280×720, SHA-256 `4C494F76…9C77A8` и
  `1AB7BF61…DD32D8`; повторный t=0 совпал побайтово. Полный `ctest` с
  `GL_GAME_DIR` — **15/15 passed**, 766.49 s, exit 0.
- Скриншоты оригинала: `Screenshots\Shot0000–0003.bmp` (1920×1080, игровая
  область 1024×768), уровень `l1_1`, сцена цирка; SHA-256 в research. На
  `Shot0002` нижние по экрану шут и силач перекрывают более высокого NPC —
  Y-sort совпадает визуально. Live-снимок рендерера: `count=4`, порядок
  `EF_TRIGGER` → `EF_PERSON y=1355` → `EF_PERSON y=1669` (совпадает с
  компаратором).
- **Важно:** на `Shot0001` игрок (зелёная подсветка) стоит за перилами
  деревянной вышки, и перила перекрывают его. Вышка уже есть в `layer.jpg`,
  поэтому это **не** Y-sort статиков, а клеточная маска: `FUN_1202bc54`
  (`ApplyCellMask`), `r_masks_*`, LVL `tile.mask`/`BLK_MDSC`. Реализовано в
  **A8** (см. выше): персонаж за вышкой перекрывается.
- Осталось: manual gate A7/A8 после появления hero; spec/plan A7 —
  `docs/specs/2026-10-23-a7-render-order-design.md`,
  `docs/plans/2026-10-23-a7-render-order.md`.

## A4 review fixes (2026-10-23)

- Исправлены счётчики в spec §5 и research: t=60 с — 2 moving + 4 waiting
  (до A4 было 3+3).
- Зафиксирован остаток: частота опроса стоящего `STAY` в оригинале не извлечена;
  порт вызывает гейт каждый тик 1/60 и при `d < 500` потребляет общий MSVC-поток,
  поэтому t=60 — состояние порта, а не доказательство паритета (research
  «Что не подтверждено», D-035).
- D-035 перенесён после D-034 (числовой порядок).
- Добавлены тесты: `STAY_ROTATE` при `d >= 500` ждёт без поворота; код 26
  ограничивается до индекса 15; at-spawn `STAY` с `delay_max 9000` + RANDOM-mover
  не потребляет rand каждый тик (цель второго выбора совпадает с ожидаемым потоком).
- `place_person` помечен как test/utility seam (сбрасывает movement/goal).
- Проверка: `cmake --build build` — успех; новые тесты 4/4, 3493 assertions;
  полный `ctest` с `GL_GAME_DIR` — 15/15, 670.10 s.

## A4 — STAY / STAY_ROTATE (2026-10-23)

- `FUN_1401c93c` (RVA `0x1c93c`), типы 0/1: `STAY` после гейта паузы сравнивает
  текущую клетку (`+0x604/+0x608`) со spawn (`+0x5f8/+0x5fc`); при равенстве
  ничего не делает, при смещении планирует GO к spawn и по прибытии снова
  `STAY`. `STAY_ROTATE` после гейта при `d < 500` обнуляет `+0x63c`/`+0x2990`,
  берёт `rand(10,25)`, ставит `SET_DIRECTION` (`direction16 = d-10`,
  `direction8 = direction16_to8`), при `d >= 500` ждёт. `+0x600 =
  DAT_14088dbc` не моделируется (нет поведенческих доказательств).
- Порт: `can_move` для `STAY`/`STAY_ROTATE` больше не форсится в `false`;
  `STAY` активен при наличии `_go`, `STAY_ROTATE` — всегда. `STAY` возвращается
  к spawn через существующий LOS/A*/occupancy, проверка — по тайлам. Для теста
  смещения добавлен `LevelSimulation::place_person`. `STAY_ROTATE` не двигается,
  меняет направление из `rng_.range(10,25)` и ставит паузу **500 мс**
  (`kRotatePauseMs`; историческое значение, **superseded by D-068**: turn
  delay = `max(1, ((go_delay_ms-17)/3)*2)`); длительность действия/анимация
  `SET_DIRECTION` в оригинале не извлечена. Код 26 от
  `range` (`max+1`, A1) ограничивается до 25.
- Real `l1_1`: `STAY` на spawn не двигаются; 6 route-movers теперь получают
  сдвинутую последовательность общего MSVC `rand`, потому что 10 `STAY` тоже
  проходят гейт паузы. В t=60 с — 2 moving + 4 waiting из 6 movers (было 3+3).
  Stability: P692 6 aborted (<150), остальные movers <50; пороги не менялись.
- Тесты: synthetic — `STAY` в spawn не двигается; смещённый `STAY` возвращается
  точно в центр spawn и останавливается без блуждания; `STAY_ROTATE` меняет
  направление по ожидаемой последовательности `rand(10,25)` каждые 500 мс и
  никогда не двигается. Real 60-с тест обновлён на route-movers + проверку
  «все `STAY` на spawn».
- Проверка: `cmake --build build` — успех; `gl_game_tests` (с `--no-skip`) —
  **68/68**, 550 029 assertions, 0 failed; полный `ctest` с `GL_GAME_DIR` —
  **15/15 passed**, 666.51 s. D-035;
  `docs/research/p4b-person-movement.md` («STAY / STAY_ROTATE (A4)»), spec §4.2,
  `parity-audit-2026-10-22.md` (A4 resolved), `parity-audit-p4.md` (B6/B7).
- Следующая — **A7** (Y-сортировка/отрисовка), затем P4c.

## A6 — Occupancy model (2026-10-23)

- Подтверждено по `Server.dll` v1.3: `DAT_14088dac` (RVA `0x88dac`) — клеточная
  занятость из **текущих** позиций, запись/чтение `FUN_14012cdc`/`FUN_14012d2c`,
  пересборка каждый апдейт; `FUN_14020a18` (RVA `0x20a18`) проверяет клетки входа
  против этой сетки плюс своя клетка. Резервирований цели/пути нет.
- Порт: удалены `mark_occupied(path.back())` и `mark_occupied(path[path_index])`;
  после хода помечается только `tile_of(m)`, `rebuild_occupancy` каждый тик. Внутри
  тика occupancy = текущие тайлы плюс только что занятый тайл; освобождённая клетка
  не очищается до следующего rebuild. `blocked_for`/`tile_occupied` освобождают свою
  текущую клетку и свою цель (`has_goal`) для всех route-типов — это не резерв. D-034.
- Диагностика `l1_1` 180 с, seed 1 (временный лог удалён): у P692 **110**
  occupancy-abort, **все 110** — текущая клетка `oficer1` `(143,151)`; ни одного
  из-за резерва. После правки те же 110 occupancy-abort / **113** aborted-переходов
  (у остальных: P678=3, P640=5, суммарно без P692 — 8). Резервирования не были
  причиной конфликта A5.
- Остаток — corner clipping: P692 в `(1717.528,1358.220)` (тайл `(143,150)`) идёт к
  waypoint `(142,151)`=`(1710,1363)`, но диагональный шаг даёт сырой
  `tile_at(next)=(143,151)` (x ещё в колонке 143, y уже перешёл строку 151) — вход в
  занятую клетку до `~t=22667`, пока `oficer1` не уйдёт. Правило входа не менялось
  без доказательств из оригинала; порог P692 (`<150`) не ослаблялся.
- Регрессия: `same-tick planning ignores another NPC's reserved goal cell` — два NPC
  планируют в одном тике; цель первого — единственный проход на маршруте второго;
  план второго успешен и проходит через эту клетку, пока первый ещё не дошёл до неё.
  На коде с `mark_occupied(path.back())` план второго не строится (проверено
  временным возвратом обоих вызовов, тест падает на `REQUIRE(second.moving)`).
- Проверка: `cmake --build build` — успех; `gl_game_tests` —
  **65/65**, 509 029 assertions, 0 failed; полный `ctest` с `GL_GAME_DIR` —
  **15/15 passed**, 728.17 s. Документы: `docs/research/p4b-person-movement.md`
  («Занятость (occupancy) — A6»), spec P4b2 §4.3, `parity-audit-2026-10-22.md`
  (A1/A2/A3/A4/A5 resolved, A6 narrowed).
- Следующая — **A7**, затем P4c.

## A5 — Movement speed/time base (2026-10-23)

- Скорость NPC: `iso_speed(dx,dy,move_x,move_y) * 1000 / (go_delay_ms * 0.8)`
  (`kMovementDelayFactor = 0.8`, D-033) — прежняя формула ×1.25. Шаг порта
  остаётся 1/60 с; оригинал квантует обновления к 35 Гц.
- Live-память `GoldenLand.exe` v1.3, `l1_1`: шаг позиции точно равен `iso_speed`
  (5.254 px у move 6/5, 3.862 px у move 4/3); интервал 80 мс go → ~63 мс,
  85 мс → ~68 мс (тики 35 Гц: 56/84 мс); `sv_fps = 35`, `sv_game_speed = 0`,
  `gv_anim_speed = 0`. Метод и адреса — `docs/research/p4b-person-movement.md`.
- Тесты: два unit-теста формулы скорости — горизонтальные (`move_x`, y-дельта = 0)
  для go 80/move 6×5 → 93.75 px/s и go 85/move 4×3 → 58.82 px/s; плюс направленный
  `iso_speed(7,12,6,5)` → 5.254 px за go-кадр (значение из live-измерения). 4-px
  прибытие пересчитано на 26 тиков/3.5 px; `MOVED_FLIP` проверяется на первом
  прибытии; 60-с ожидания — 3 moving + 3 waiting. Пороги stability — прежние `<50`
  на NPC и суммарно без P692; `L0.P692_citizen5` остаётся `<150` (113 abort за
  180 с из-за corner clipping с `oficer1`; A6 показал, что резервирования ни при
  чём, поэтому порог не ослаблялся и не ужесточался; прежний баг A1 давал 266).
- Проверка: `cmake --build build` — успех; `gl_game_tests` (с `--no-skip`) —
  **64/64**, 509 022 assertions, 0 failed; полный `ctest` с `GL_GAME_DIR` —
  **15/15 passed**, 611.38 s. D-033.
- Следующая — **A6** (occupancy), затем A4/A7.

## A3 — RANDOM/RANDOM_RADIUS parity (2026-10-23)

- `LevelSimulation::pick_goal` для `RANDOM`/`RANDOM_RADIUS` воспроизводит
  `FUN_1401c2a0`: база — spawn для `RANDOM_RADIUS`, текущая позиция для `RANDOM`;
  `r = radius` (0 → 13); вне-граничная база возвращается без выборок; иначе
  `rand(-r..r)` по обеим осям до клетки в границах и проходимой по `WalkGrid`.
  Убраны портовые 64 попытки и occupancy-фильтр кандидата.
- Для `RANDOM*` первая неудача планирования сбрасывает `has_goal` — следующий
  выбор сэмплирует новую цель (прежнего счётчика из 3 неудач нет). `MOVED`/
  `MOVED_FLIP` и mid-path replan не менялись.
- Цель `RANDOM*` может совпасть с клеткой другого NPC. В порте она исключена из
  occupancy (`blocked_for` очищает `state.goal`, `tile_occupied` её игнорирует),
  поэтому NPC идёт в эту клетку и встаёт поверх занявшего — не abort и не
  resample. Остаточное расхождение с оригиналом: C12 в
  `docs/research/parity-audit-p4.md` (unverified).
- Предохранитель от вырожденной карты: 1'000'000 выборок, затем warning с
  level/person/base tile (без повторов для той же вырожденной цели) и откат на
  базовую клетку; это не изменение поведения оригинала.
- Тесты: synthetic — только in-bounds/walkable цели при заблокированных клетках;
  повтор кандидатов; цель может быть занята другим NPC; вне-граничная база
  возвращается как есть без расхода RNG (`RANDOM` и `RANDOM_RADIUS`); после первой
  неудачи `RANDOM` сразу сэмплирует заново (симуляция доводится до точного
  `wait_until_ms`); fallback на полностью заблокированном радиусе.
- Проверка: `ctest` **15/15** (556.71 s) с `GL_GAME_DIR`; `gl_game_tests`
  **61/61**, 508 900 assertions. D-032.
- Следующая — **A5/A6**; затем A4/A7.

## A2 — Route index semantics (2026-10-23)

- `LevelSimulation` продвигает индекс `MOVED`/`MOVED_FLIP` **в момент выбора цели**
  (не на прибытии): MOVED `index=(index+1)%count`; MOVED_FLIP `index += coin(0.5)?1:-1`.
  Mid-path replan к той же цели индекс не меняет.
- Если планирование к только что выбранной клетке не удалось, цель сбрасывается и
  следующая попытка берёт следующую клетку маршрута (заблокированная клетка не
  ретраится вечно) — как в оригинале.
- Live-подтверждение (`L0.P744_oficer1`, track3): индекс уже сдвинут во время
  стояния/движения; `route[0]=(186,147)` заблокирован в live-сетке, но NPC ушёл на
  `route[2]` — блокированная клетка пропущена; сэмплы в `docs/research/p4b-person-movement.md`.
- Тесты: synthetic skip blocked route cell; live `oficer1` не застревает (60 с);
  stability зелёный. `ctest` 15/15 (573.57 s); `gl_game_tests` 54/54, 472 865 assertions. D-031.
- Следующая — **A3: RANDOM/RANDOM_RADIUS** (убрать портовые 64 попытки/отказ после
  3 неудач, воспроизвести цикл `FUN_1401c2a0`).


## A1 — RNG parity (2026-10-23)

- `LevelSimulation` использует `gl::game::MsvcRandom` (`msvc_random.hpp/.cpp`) —
  один глобальный поток MSVC CRT `rand` (`state*0x343FD+0x269EC3`,
  `(state>>16)&0x7FFF`), seed 1; per-person xorshift и hash-seed удалены. Публичные
  `next_random`/`random_range` убраны; `MsvcRandom::range` =
  `min + round((max-min+0.99999)*rand/32767.0)` (включая `max+1` при `rand=32767`),
  `coin(p)` = `p<=0 ? false : p>1 ? true : fraction() < p`. `LevelScreen` передаёт seed 1.
- Порядок инициализации: в `create` сначала бросается пауза
  (`range(delay_min,delay_max)`), цель выбирается только при `<500`
  (`FUN_1401c378` до `FUN_1401c93c`); на прибытии/после паузы — та же
  последовательность. `MOVED_FLIP` coin — при выборе клетки; `RANDOM*` — два броска
  на попытку.
- Тесты: `MsvcRandom` напрямую (MSVC-последовательность из state 1, degenerate range,
  `max+1`, coin 0/1/>1); integration-тесты прибивают порядок «пауза → цель» для
  `RANDOM` и `MOVED_FLIP` по известной MSVC-последовательности.
- Стабильность: вместо порога `toggles` считаются aborted moves (переход
  `moving true→false` при смещении < 0.05 px); порог `< 50` на NPC и суммарно,
  `max_5s_window < 100`. `L0.P692_citizen5` при исправленном порядке: 18 toggles,
  9 завершённых ходов, 0 aborted/short, средняя длина 193.2 px (max 224.3),
  `max_5s_window=1` — это легитимные длинные ходы. Контрольный прогон со старым
  порядком дал те же 541 toggle, 266 aborted и 264 short-ходов — прежние
  переключения были thrash из-за выбора цели до броска паузы, а не короткими ходами.
- `l1_1`, seed 1, t=60 с: 1 moving + 5 waiting из 6 подвижных (10 `STAY`);
  детерминизм и границы сохранены.
- Проверено: `cmake --build build` — успех; полный `ctest` с `GL_GAME_DIR` —
  **15/15 passed**, 547.97 s; `gl_game_tests` (с `--no-skip`) — **53/53 case /
  473 459 assertions**, 0 failed.
- Следующая — **A2/A3** (индексы маршрутов, RANDOM), затем A5/A6, A4/A7.

## Аудит паритета P0…P4b2 — выполнен (2026-10-22)

Правило AGENTS/D-029. Отчёт: `docs/research/parity-audit-2026-10-22.md` +
`parity-audit-p0-p1.md`, `parity-audit-p2-p3.md`, `parity-audit-p4.md`.

Ключевые выводы:

- подтверждено: форматы на реальных данных, PAD 276/276, SEF/person, collision
  grid (0/51520 с live-памятью), параметры A*;
- частично: PAD crops/якоря, RNG, тайминг индексов маршрутов, RANDOM-правила,
  конфиг/консоль, лимиты VM, шрифт/диалоги;
- блокирует паритет: VM-семантика взята из редактора, а не из DLL; нативы —
  заглушки; save/load отсутствует; ADSC `number/name`; порядок/якоря/тайминг
  отрисовки; скорость/тайм-база и occupancy-модель NPC.

Backlog задач — в отчёте (A1…A8, B1…B6, C1…C5) и в «Следующих задачах» ниже.
Ближайшая — **A5/A6**, затем A4/A7. A1 (RNG), A2 (индексы маршрутов) и A3
(RANDOM) закрыты 2026-10-23 (см. выше).

## P4b2 — Collisions and pathfinding (2026-10-22)

- Tasks 1–5 завершены на ветке `p4b2-collisions`: `WalkGrid` из LVL (слово
  первой записи чанка, `relief` биты 8..11, column-major порядок),
  `line_of_sight` + A*, occupancy/обход в `LevelSimulation`, `--walkable-overlay`,
  automated acceptance + docs. P4b-поведение сохранено.
- Правило проходимости сверено с live-сеткой оригинала в памяти (`world+0x330`,
  getter `FUN_00426234`): реконструкция из LVL совпала точно — **0/51520**
  расхождений после загрузки; ~331 клетка меняется в рантайме дверями/скриптами
  (вне P4b2).
- Build: `cmake --build build` — успех (`Copying MinGW runtime DLLs`).
- Полный `ctest` с `GL_GAME_DIR` — **15/15 passed**, 748.38 s, exit 0
  (`gl_assets_real_tests` 563.48 s, `glview_screenshot_tests` 75.17 s,
  `glgame_screenshot_tests` 73.91 s, `gl_game_tests` 20.71 s,
  `gl_cs_tools_tests` 8.61 s; остальные — < 3 s).
- `gl_game_tests` целиком — 41 case / **462 766 assertions**, 0 failed; в его
  составе real `l1_1` 60 с (`seed 42`): ни один NPC с проходимым спавном не зашёл
  в заблокированный тайл, ни одна пара не делила тайл, два прогона совпали;
  4 из 5 таких movers строят непустой path (`L0.P744_oficer1` — см. ограничения).
  Targeted real-прогон (4 cases) — 450 108 assertions, 16.2 s, exit 0.
- `--walkable-overlay` (исправленная модель, `--frames 5`): `glview l1_1`
  1280×720, PNG 2 969 157 B, SHA-256
  `8CBC2A57CF4DEF2D3C354651117213585AA153FBC4949387090B9E160A34D1F9`;
  от обычного кадра отличаются **570 960 красных px** (полное разрешение;
  142 866 при выборке каждые 2 px). `glgame l1_1 --sim-ms 10000` — exit 0,
  1280×720. В логах glview/glgame — **0 warning/error**, ошибок pathfinding нет.
- **P4b2 automated acceptance passed.** **Manual desktop gate пройден пользователем:**
  обход домов/заборов/NPC и overlay совпадают с оригиналом, движение похоже на
  оригинал; **P4b2 принят** (2026-10-22).
- **Fix round (2026-10-22): движение и A* приведены к оригиналу.** При свободной
  линии путь — `{from, goal}`, движение по прямой без остановок в центрах клеток;
  клетка, в которую NPC собирается войти, проверяется каждый тик, при блокировке/
  занятости — replan к той же цели (своя цель исключена из occupancy). A*: шаг 1 для всех
  8 направлений (в оригинале 10), `h = max + 0.375*min`, вес 1.2, диагональ без
  проверки ортогональных соседей, FIFO-ничьи по порядку вставки. Добавлены тесты
  на прямую линию и диагональ рядом с блоком; старые wall/crossing-LOS зелёные.
  Полный `ctest` с `GL_GAME_DIR` — **15/15 passed**, 598.74 с; `gl_game_tests` —
  43 case / **473 571 assertion**, 0 failed. Остаточный риск: точный порядок
  соседей/разрешение ничьих может отличаться от оригинала (см. research).
- Известные ограничения: `L0.P7_Kolbasens_Rodstvennik1` спавнится на заблокированном
  тайле (walkability-проверка только для movers с проходимым спавном; NPC может
  выйти, если путь появится); `L0.P744_oficer1` (`MOVED`) не строит путь, потому что
  первая клетка маршрута `(186,147)` заблокирована по `relief` (route не скипается);
  достижимая RANDOM-цель может быть отброшена после 3 неудачных планов (~1.5 с; снято
  A3/D-032 — счётчика нет); глобального доказательства отсутствия дедлоков нет; `--walkable-overlay` не
  восстанавливает blend mode (влияет только на debug-кадр).
- Следующая задача — **P4c** (клик → диалог).

## P4b — NPC movement (2026-10-22)

- Tasks 1–6 завершены на ветке `p4b-movement`: SEF `cell_groups`, route/direction
  helpers, route + `_go` metadata, детерминированный `LevelSimulation`, отрисовка
  движущихся NPC, automated acceptance + docs.
- Полный `ctest` с `GL_GAME_DIR` — **15/15 passed**, 503.48 s, exit 0
  (`gl_assets_real_tests` 385.15 s, `glgame_screenshot_tests` 55.72 s,
  `glview_screenshot_tests` 46.63 s, `gl_cs_tools_tests` 6.29 s,
  `gl_game_tests` 4.99 s; остальные — < 2 s). `cmake --build build` — успех.
  Суммарно по 15 исполняемым файлам: **393 test case / 5581 assertions**, 0 failed.
- PAD: **276/276** реальных `.pad`, 2450 анимаций, 0 ошибок; SEF sweep **261/261**;
  `l1_1` — **16 persons**, 9 групп `cell_groups` (5 `box*` + 4 `track*`:
  `track1` 9, `track2` 22, `track3` 8, `track4` 7 клеток), у всех 6 movers есть `_go`.
- Screenshot `glview l1_1` 1280x720: `16 persons`, `uploaded 16 person sprites`,
  ни одного warning про `_go`. NPC-mask XOR между `--sim-ms 0` и `--sim-ms 10000` —
  **3001 px** (>1000); маска NPC к фону без persons — 3869 px (t=0) и 3574 px (t=10 с).
  Повторный прогон t=10 с побайтово совпал (SHA-256
  `0AD62384FBCC50144203AA88ECF125DEC64504C80F1D08B7561821752851EB3E`).
- Детерминизм 60 с (`seed 0x5EED`, как в `LevelScreen`): два прогона совпали во всех
  16 позициях/состояниях. Типы: 10 `STAY`, 1 `MOVED`, 3 `MOVED_FLIP`, 2 `RANDOM_RADIUS`;
  в t=60 с — 3 moving + 3 waiting у 6 подвижных, 10 `STAY`. Все позиции в границах.
- **P4b automated acceptance passed.** **Manual desktop gate пройден пользователем:**
  NPC ходят по маршрутам, направления/анимации и `Esc`/меню корректны; **P4b принят**
  (2026-10-22). Пользователь зафиксировал ограничение: NPC проходят сквозь визуальные
  объекты — коллизии/pathfinding в P4b не входили.
- Известные ограничения: `parse_route_type` неизвестное значение молча даёт `stay`
  (warning отложен); `RANDOM*` не клампят кандидата, а отбрасывают его и после 64
  попыток возвращаются к базовой клетке; `RANDOM_RADIUS` в t=60 с может быть в wait;
  **нет проверки проходимости и обхода статики**.
- Следующая задача — **P4b2** (коллизии/pathfinding по флагам чанков LVL), затем **P4c**
  (клик → диалог). Решение о переносе коллизий — D-027.

## P4a — NPCs on the level (2026-10-21)

- Tasks 1–5 завершены на ветке `p4a-npcs`: PAD/person `.scr`/SEF `persons`,
  `gl::game::load_persons`, отрисовка стоячей анимации в `LevelScreen`,
  автоматизированная приёмка и документы.
- Полный `ctest` с `GL_GAME_DIR` — **15/15 passed**, 849.95 s, exit 0
  (`gl_assets_real_tests` 684.72 s, `glgame_screenshot_tests` 80.52 s,
  `glview_screenshot_tests` 56.69 s, `gl_game_tests` 5.25 s,
  `gl_cs_tools_tests` 15.24 s; остальные — < 5 s). `cmake --build build` —
  `ninja: no work to do`.
- PAD: все **276/276** реальных `.pad` из `persons.pak` разобраны, 2450 анимаций,
  0 ошибок (targeted real-тест, 0.85 s). `l1_1` — **16** persons, 15 уникальных
  person-скриптов, все разобраны; `gl_game_tests` — 3/3 cases, 192 assertions.
- Screenshot `l1_1` 1280x720 через `glview`: `16 persons`, `uploaded 16 person
  sprites`; PNG 3 120 861 байт, 72 887 цветов (выборка каждые 2 px); отличается от
  `--no-persons` на **3 828** пикселей; повторный прогон побайтово совпал (SHA-256
  `8B377011E3607CFE75688203513B5C6B6002381C54927E455EC542D5E074AA7A`).
- Критерии spec §6 выполнены автоматизированно: 16 NPC загружаются и рисуются,
  все `.pad` парсятся, screenshot с NPC отличается от кадра без NPC,
  `gl_assets`/`gl_game` остаются SDL-free.
- **P4a automated acceptance passed**; manual desktop gate пройден пользователем
  2026-10-21 (NPC в правильных местах/направлениях, idle-анимация, `Esc`/меню);
  **P4a принята**.
- Известные ограничения P4a: нет Y-сортировки (P4b); `--no-persons` не пропускает
  загрузку/декодирование спрайтов (только отрисовку); NPC с общим `res_name`
  загружают текстуру повторно. Следующая задача — **P4b** (движение/маршруты/AI).

## P3c final fix wave (2026-10-21)

- Font atlas now covers U+00A0-U+00FF plus U+2116/U+2122; NBSP normalizes to a space;
  `Font::has_glyph()` exposes atlas coverage; NaN/Inf pixel heights are rejected.
- Tests: cp1251 punctuation/symbol coverage and measurement, dialog-before-renderer
  lifetime, strict UTF-8 Cyrillic checks, line-start phrase IDs, no `infinity` acceptance.
- `DialogSession::choose()` now fails when `ScriptVm::user_input()` rejects the answer;
  `DialogSession::failed()` added. `open_dialog` logs VFS/parse errors.
- Verified: `cmake --build build`; `ctest --test-dir build --output-on-failure` with
  `GL_GAME_DIR` - 13/13 passed, 655.80 s; targeted font/glgame/cs-tools/real-script tests pass.

## Текущая фаза

P0 завершена и принята. P1 — Level viewer: реализация и автоматизированная приёмка
завершены (2026-10-20); открыт финальный manual desktop pass (см. Open Questions).
P2a — оболочка игры и консоль: реализация, автоматизированная приёмка и manual desktop
gate завершены (2026-10-20); P2a принята. P2b — детерминированный тик: реализация,
автоматизированная приёмка и manual interactive gate завершены (2026-10-20); P2b принята.
P2c — save/load каркас: реализация и автоматизированная приёмка завершены (2026-10-20);
открыт manual UI gate (Continue → слоты, Esc). P3a — CS VM: Tasks 1–5 завершены
2026-10-21, автоматизированная приёмка пройдена, P3a принята (manual UI gate для P3a
нет). P3c — dialogs + SDB + TTF: Tasks 1–4 завершены 2026-10-21; автоматизированная
приёмка пройдена (13/13 ctest, `glscript --sdb`, dialog PNG), manual dialog UI gate
пройден пользователем 2026-10-21. P4a — NPC на уровне: Tasks 1–5 завершены
2026-10-21, automated acceptance passed, manual gate пройден — **P4a принята**.
P4b — движение NPC: Tasks 1–6 завершены 2026-10-22, automated acceptance passed,
manual desktop gate пройден — **P4b принят**. P4b2 — коллизии/pathfinding: Tasks 1–5
завершены 2026-10-22, **automated acceptance passed** (15/15 ctest, 413 cases /
466 027 assertions; правило сверено с live-сеткой: 0/51520), **manual desktop gate
OPEN — не принят**. P4c — клик → диалог: Tasks 1–8 завершены, automated
acceptance passed (`glgame_screenshot_tests` 20/20; полный ctest 17/17),
**manual desktop gate пройден 2026-10-24 — P4c принят**. Следующие — P1b
(Android/touch) и P2 backfill.

## Текущая задача

**P4c — клик → диалог: принят 2026-10-24 (Tasks 1–8, automated + manual gate passed).**
Реализованы hero (spawn на первом walkable entrance, путь, AABB `can_interact`,
маркер 20 px в Y-sort/масках), hit-test по PAD-кадру, `dialog_script_path`,
`DialogOverlay` поверх уровня и `glgame --click-npc/--click-world` с pending talk.
Real `l1_1`: `L1.P621_Svet_3CircusWorkerV` (индекс 13 из `Level::persons`,
не сырой SEF) → exit 0, открывается
`scripts/dialogs/l1_1/l1.p621_svet_3circusworkerv.d621.age.cs`,
`speaker='L1.P621_Svet_3CircusWorkerV'`, `answers=2`, непустой текст, overlay
поверх уровня; движущиеся `L0.P678_citizen4` и `L0.P7_Kolbasens_Rodstvennik1`
также открывают диалог по текущей клетке. Семантика клика/ретаргета и
приоритет ввода overlay — D-039.
Проверка: `glgame_screenshot_tests` **20/20** (391 assertion);
`level_click_tests` **22/22**; полный `ctest` с `GL_GAME_DIR` — **17/17
passed**, 865.27 s, exit 0. **Manual desktop gate PASSED 2026-10-24:** клик по
NPC → подход → диалог, `Esc` → уровень, клик по земле, NPC без диалога.
**P4c принят.** Ограничения: hero — маркер, а не PAD Вертаса; `CanInteract` —
AABB вместо 6-осевой формулы; server `0x12` не реализован; точный
PAD/визуал Вертаса вне P4c.

**A8 — Cell mask / foreground occlusion реализован 2026-10-23.**
`BLK_MDSC`/`mask_<number>.csx`: alpha=0-силуэт = occluding foreground, top-left.
Порт: `LevelMask`, `compose_mask_foreground`, merge масок в Y-отсортированные
проходы теней/тел по `y+height`; `glview` получил `--camera-x/--camera-y`,
`--place-person`, `--no-masks`. `mask_45` 182090 px; кадр «персонаж за вышкой»
с/без масок различается >500 px. `person_render_tests` 18/18,
`glview_screenshot_tests` 10/10. D-037. Следующая — P4c.

**A7 — Y-sort и анкоры реализованы 2026-10-23; скриншоты оригинала получены.**
Реверс Client.dll: статики и LAO-анимации рисуются до динамических сущностей и
не сортируются; `EF_PERSON` сортируется по Y (`+0x1c`), затем X (`+0x18`);
тени (проход 2, alpha 0.5) и тела (проход 7, alpha 1.0) — два прохода по
общему порядку. Порт: stable sort по `(movement.y, movement.x)`, два прохода
теней/тел, полная ячейка кадра, PAD-анкоры без изменений. Оригинал `l1_1`:
`Shot0000–0003` (цирк), на `Shot0002` нижние NPC перекрывают верхнего; live
renderer даёт `EF_TRIGGER → PERSON y=1355 → PERSON y=1669`. На `Shot0001`
игрок за перилами вышки перекрыт — это cell mask (`ApplyCellMask`), не Y-sort;
реализовано в A8. `person_render_tests` 18/18 (84 assertions); targeted real
`l1_1` 7/7 (489 891 assertions). D-036; spec/plan в
`docs/specs/2026-10-23-a7-render-order-design.md` и
`docs/plans/2026-10-23-a7-render-order.md`. Следующая — P4c.

**A6 — Occupancy model (narrowed) завершён 2026-10-23.** Резервирования цели/пути
удалены (D-034): occupancy — только текущие тайлы, пересборка каждый тик. Измерение
`l1_1` 180 с: 110 occupancy-abort P692, все — текущая клетка `oficer1` `(143,151)`,
0 из-за резервов; после правки те же 113 aborted. Остаток — corner clipping, правило
входа не менялось, порог P692 не ослаблялся. `ctest` **15/15** (749.81 s),
`gl_game_tests` **65/65** (509 027 assertions). Следующая — A7 (A4 закрыт выше).

**A1 — RNG parity завершён 2026-10-23.** Глобальный MSVC CRT `rand` с seed 1,
формула `FUN_140061e4` (включая `max+1`) и `coin` из `FUN_14006288`, порядок
«пауза → выбор цели». `cmake --build build` — успех; полный `ctest` с
`GL_GAME_DIR` — **15/15 passed**, 557.07 s; `gl_game_tests` (с `--no-skip`) —
**48/48 case / 473 432 assertions**, 0 failed. Следующая — A2/A3.

**P4b2 — Task 5 (automated acceptance + docs) завершён 2026-10-22; P4b2 automated
acceptance passed.** Полный `ctest` с `GL_GAME_DIR` — **15/15 passed**, 748.38 s,
exit 0; `cmake --build build` — успех (`Copying MinGW runtime DLLs`). Суммарно по
15 исполняемым файлам — **413 test case / 466 027 assertions**, 0 failed;
`gl_game_tests` — 41 case / 462 766 assertions (real `l1_1` 60 с: walkable spawn
movers, occupancy, determinism), targeted real-прогон — 450 108 assertions /
16.2 s. Overlay `glview l1_1` 1280×720 (`--frames 5`): PNG 2 969 157 B, SHA-256
`8CBC2A57...34D1F9`, отличие от обычного кадра **570 960 красных px** (полное
разрешение; 142 866 при выборке каждые 2 px); `glgame --sim-ms 10000` — exit 0;
в логах 0 warning/error. **Manual desktop gate OPEN:** пользователь должен запустить
`glgame --game-dir "I:\Games\Златогорье-2" --level l1_1` и
`glview --game-dir "I:\Games\Златогорье-2" --level l1_1 --walkable-overlay`,
проверить обход препятствий/NPC и совпадение overlay с фоном; агент GUI не
запускал и manual pass не подтверждает. Следующая задача — P4c.

**P4b — Task 6 (automated acceptance + docs) завершён 2026-10-22; P4b automated
acceptance passed.** Fix rounds добавили постоянный real-тест 60 с `l1_1`
(10 stationary / 6 movable, bounds, детерминизм, 4 route + 2 `RANDOM_RADIUS`)
и финальную волну hardening. Полный `ctest` с `GL_GAME_DIR` — **15/15 passed**,
503.48 s, exit 0;
`cmake --build build` — успех (`Copying MinGW runtime DLLs`). По 15
исполняемым файлам — **393 test case / 5581 assertions**, 0 failed; PAD
`entries=276 parsed=276 failures=0 animations=2450`; SEF `261/261`; `l1_1` —
`persons=16`, 6 movers с `_go`. Screenshot `glview` 1280x720: `16 persons`,
`uploaded 16 person sprites`, без `_go`-warning; NPC-mask XOR t=0 vs t=10 с —
**3001 px**; повторный прогон t=10 с совпал побайтово (SHA-256
`0AD62384...2851EB3E`). 60-с детерминизм (seed `0x5EED`): два прогона совпали;
в t=60 с 3 moving + 3 waiting из 6 подвижных, 10 `STAY`.
**Manual desktop gate OPEN:** пользователь должен запустить
`glgame --game-dir "I:\Games\Златогорье-2" --level l1_1`, проверить 4 NPC на
маршрутах + 2 `RANDOM_RADIUS`, направления/анимации, `Esc`/меню; агент GUI не
запускал и manual pass не подтверждает. Следующая задача — P4c.

**P3c — Task 4 (automated acceptance + docs) завершён 2026-10-21; P3c automated
acceptance passed.** `cmake --build build` — `ninja: no work to do`, exit 0. Полный
`ctest --test-dir build --output-on-failure` с `GL_GAME_DIR` — **13/13 passed**,
652.23 s, exit 0 (real-data 562.27 s, glview 32.26 s, glgame 42.46 s); суммарно
318 test cases / 2649 assertions, 0 failed. `glscript --game-dir
"I:\Games\Златогорье-2" --file scripts/dialogs/demon.d1.age.cs --run --sdb
sdb/dialogs/dialogsphrases.sdb` — exit 0, `status: wait_user`, `steps: 43`,
`dialogs: 2684 2686 2696`, `answers: 2`, затем `2684<TAB>Чего уставился?`,
`2686<TAB>Я не думал, что демоны разумны...`, `2696<TAB>Ничего. Я пошел...`;
два ожидаемых warning `globals: line 918/1262: missing value`.
`glgame --game-dir ... --dialog scripts/dialogs/demon.d1.age.cs --screenshot
%TEMP%\p3c_dialog.png` — exit 0; PNG 1280×720, 54 332 байта, 552 цвета (выборка
каждые 2 px; 771 уникальный цвет), SHA-256
`2544FFD6F2A4B5729EF43413C26D20862B669E62F6C8CC71875EB45E65F2BFE1`; два прогона
побайтово совпали, загружен `C:\Windows\Fonts\segoeui.ttf`, console=0. Дефектов
автоматизированная приёмка не выявила. Ограничение P2c с `?` в заголовках save-слотов
закрыто TTF (ASCII-fallback остаётся только без шрифта). **Manual dialog UI gate OPEN:**
человек должен запустить `glgame --dialog ...` и проверить читаемость кириллицы, выбор
ответа (Up/Down, Enter/мышь) и `Esc`; агент это не проверял. Следующая задача —
P4 (NPC) или P1b (Android/touch).

**P3a — финальная волна правок (whole-branch review) завершена 2026-10-21.**
Исправлены: бюджет вычисления выражения (`kEvalBudget = 100'000` вызовов `eval` на
`step()`) против циклических/разделяемых графов; assign приводит RHS к текущему типу
приёмника (`int32`/`uint32`/`double`/`string`), включая float `0.5` и DWORD
`0xFFFFFFFF`; реализованы `1 ^^`, `3..5`, `12/13`, `19/20` (строки → `failed()`);
ошибки исполнения стали «липкими» и останавливают `step()` без лавины логов, а
`user_input` больше не сбрасывает `failed()`; `glscript`-тест VM-ошибки теперь
падает именно в VM (валидный `jmp` за границу), а не в `parse_cs`; `parse_globals`
игнорирует UTF-8 BOM и предупреждает о незакрытой кавычке. Несколько `D_Say` на
странице — по-прежнему только первый (как в `CsExecutor`, задокументировано).
Проверка: `cmake --build build` — успех; `gl_script_tests` — 49 cases / 374 assertions;
`gl_script_real_tests` — 1/20; `gl_cs_tools_tests` — 8/56; полный `ctest` с
`GL_GAME_DIR` — **11/11 passed**, 560.62 s, exit 0.

**P3a — Task 5 (acceptance + docs) завершён 2026-10-21. P3a принята.**
`cmake --build build` — `ninja: no work to do`, exit 0. Полный
`ctest --test-dir build --output-on-failure` с `GL_GAME_DIR` — **11/11 passed**,
573.09 s, exit 0 (real-data 518.81 s, `gl_cs_tools` 8.30 s, glview 26.26 s,
glgame 17.39 s); в логе real-data: `parsed=555 failures=0 nodes=338977`.
`glpak disasm-cs ... --file scripts/dialogs/demon.d1.age.cs --out %TEMP%\demon_disasm.txt`
— exit 0, **5353 байта / 240 строк**, дамп начинается с `0: jmp | 1 1`,
`1: == | 2 3 193 4`, `2: str_var | LastPhrase`, `3: num | 0`.
`glscript ... --file scripts/dialogs/demon.d1.age.cs --run` — exit 0,
`status: wait_user`, `steps: 43`, `dialogs: 2684 2686 2696`, `answers: 2`
(два ожидаемых warning `globals: line 918/1262: missing value`).
Тот же запуск с `--answer 0` — exit 0, второй останов:
`status: wait_user`, `steps: 84`, `dialogs: 2719 1529 2721`, `answers: 2`.
Дефектов приёмка не выявила. P3a automated acceptance passed; следующий подпроект —
P3b (AgeScript) / P3c (dialogs + SDB), затем P1b.

**P3a — Task 4 (tools `glpak disasm-cs`, `glscript`) завершён 2026-10-21.**
`glpak disasm-cs <game_dir> --file <path> [--out <txt>]` читает `.cs` через `GameVfs`,
парсит `parse_cs` и печатает по строке на узел (`index: opcode | поля | value/text |
funcName(args)`); кавычки и control-символы в `text` экранируются (`\"`, `\\n`, `\\xHH`),
поэтому дамп остаётся по одной строке на узел. Без `--out` — в stdout, с `--out` —
UTF-8 текст. Команда добавлена в `--help`, коды выхода 0/1/2/3 как у остальных команд.
Новый SDL-free `tools/glscript` (`--game-dir <dir> [--globals <path>] --file <path>
[--run] [--answer N]`) читает CS и globals через VFS (по умолчанию
`scripts/dialogs_special/zlato_vars.scr`), исполняет до `wait_user`/`end`/`infinity`,
печатает `status`, `steps`, id из `dialogs_data()` и `answers_count()`. Повторяемый
`--answer N` подаётся в `user_input`, после чего исполнение продолжается до следующего
останова с печатью нового состояния. `--answer` без `--run` и ответ вне диапазона —
exit 1; ошибка исполнения VM (`ScriptVm::failed()`, включая ошибки типов/вычислений
и `infinity`) — exit 3. `ScriptVm::failed()` теперь отделяет такие ошибки от
нормального `end`/`wait_user`; `user_input()` возвращает `bool`.
Тесты: `gl_cs_tools_tests` (8 cases / 56 assertions, skip 77 без `GL_GAME_DIR` или при
пустом значении) — `disasm-cs` в `--out` и stdout (непустой дамп с `func`/`jmp`),
экранирование кавычек/переводов строк, `glscript --run` (`wait_user`, answers > 0),
`--run --answer 0` (exit 0, `steps` растут), reject `--answer` без `--run`,
out-of-range answer → exit 1, missing script → exit 2, synthetic VM failure → exit 3.
`gl_script_tests` — 33 cases / 247 assertions; полный `ctest` с `GL_GAME_DIR` — **11/11
passed**, 571.23 s. Task 5 повторно прогнал полный набор: 11/11, 573.09 s, exit 0.

**P3a — Task 3 (dialog flow + native registry) завершён 2026-10-21.**
`ScriptVm` получил `dialogs_data()` (id `D_Say` + id всех `D_Answer`, действительно только
при `wait_user`), `answers_count()` и `user_input(answer)` с нумерацией ответов с нуля:
при успехе выставляет `LastPhrase`/`LastAnswer` (`uint32`), очищает записанные вызовы,
переходит к узлу 0 со статусом `restart`; выход за границы — warning без изменений.
Добавлен SDL-free `DefaultScriptHost`: `RS_GetPersonParameterI` → 0,
`Exit`/`Signal`/`Console`/`Cmd` → 0 + info, прочие известные функции → 0 + debug,
неизвестные → 0 + warning; тесты могут наследовать хост. Task 2 minors: правый операнд
сравнений/логики не вычисляется после ошибки левого; top-level `func` больше не
использует `d` как переход (реальные данные имеют `c = d = -1`); формулировки
unknown-opcode (`end` + error log) выровнены в spec/plan. Fix round 1: `Exit`/`Signal`/
`Console`/`Cmd` больше не перехватываются VM, а идут через `ScriptHost` (их обрабатывает
`DefaultScriptHost`); добавлен `gl::assets::cs_function_is_known` и общий
`gl/script/function_ids.hpp`; тесты на `user_input(1)` и делегирование stub-функций.
Тесты: `gl_script_tests` — 33 cases / 233 assertions; `gl_script_real_tests` (skip 77) —
`demon.d1.age.cs` + `zlato_vars.scr` → `wait_user`, `answers_count() == 2`,
`user_input(0)` продолжает без падения (end/wait), ids > 0. Полный `ctest` с
`GL_GAME_DIR` — **10/10 passed**, 565.78 s (real-data 516.06 s, gl_script_real 1.30 s).
Следующая задача — P3a Task 4 (tools).

**P3a — Task 2 (VM core + globals) завершён 2026-10-21.**
Добавлена SDL-free библиотека `gl_script`: `Value`, `ScriptGlobal`/`parse_globals`
(`int`/`DWORD`/`float`/`string`, `//`-комментарии, cp1251→UTF-8, строки без значения/битые
строки пропускаются с warning), `ScriptHost`, `ScriptVm` (opcodes 0/2, 6..11, 14..18,
21..24, 48..50; логика/сравнения/арифметика/assign/jmp; `D_Say`/`D_Answer` записываются,
`D_CloseDialog` → `end`, `Exit`/`Signal`/`Console`/`Cmd` — заглушки, остальные — host),
`restart(true/false)`, счётчик 10 000 000 шагов → `infinity`, безопасная обработка битых
индексов/опкодов. Семантика сверена с `GoldenLandEditor::CsExecutor` и реальными `.cs`
(`docs/research/p3a-cs-vm.md`, D-023). Тесты: `gl_script_tests` — 22 cases / 139
assertions; полный `ctest` с `GL_GAME_DIR` — **9/9 passed**, 594.42 s. Smoke-прогон
`demon.d1.age.cs` + `zlato_vars.scr` → `wait_user` (43 шага). Следующая задача — P3a
Task 3 (dialogs + native registry).

**P3a — Task 1 (`.cs` parser + name tables) завершён 2026-10-21.**
`gl_assets` получил `CsNode`, `parse_cs`, `cs_opcode_name`, `cs_function_name`,
`cs_opcode_valid`; строки cp1251→UTF-8; строгая проверка размера/опкодов/усечения;
func-аргументы до 9 с обязательным `-1`. Обнаружены 6 легаси-скриптов в `scripts.pak`
с XOR-обфускацией тела (`plain[i] = data[i] ^ ((i + 0x55) & 0xFF)`, `i >= 4`);
`parse_cs` распознаёт их fallback-разбором (D-022, spec §3; подтверждено копиями из
`scripts.update.3.pak`, см. `docs/research/p3a-cs-format.md`). Fix round 1: XOR-разбор
выполняется только при невалидном первом опкоде, ошибка объединяет обе попытки;
real-тест читает `scripts.pak` в память один раз. Тесты:
`gl_assets_tests` — 120 cases / 748 assertions; real-data — **555/555** `.cs`,
0 ошибок, 338 977 узлов (real `.cs` test 1.26 s). `cmake --build build` — успех;
полный `ctest` с `GL_GAME_DIR` — **8/8 passed**, 453.16 s. Следующая задача —
P3a Task 2 (VM core + globals).

**P2c — Task 4 (automated acceptance + docs) завершён 2026-10-20.**
`cmake --build build` — `ninja: no work to do` (exit 0); полный `ctest --test-dir build
--output-on-failure` с `GL_GAME_DIR` — **8/8 passed**, 395.96 s (real-data 366.67 s,
glview 17.44 s, glgame 11.55 s). `glgame --game-dir "I:\Games\Златогорье-2" --list-saves`
— exit 0, **28 строк**, slot0 `2022-11-06  Quick save  642409`; реальные заголовки
slot16 `Убмйство кукуя`, slot25 `Продаем артифакты` выводятся без изменений. P2c
automated acceptance passed. **Manual UI gate OPEN:** запустить
`glgame --game-dir "I:\Games\Златогорье-2"`, меню → `Continue` (активен при наличии
валидных слотов) → экран `Save slots`; проверить прокрутку списка при маленьком
разрешении, `Enter` → сообщение `world deserialization not implemented (P2c)`,
`Esc` → возврат в меню. Кириллица в заголовках отображается как `?` (ограничение
`stb_easy_font`, вынесено в P7). Следующая задача — P3 (скрипт-машина) или P1b.

**P2b — Task 4 (automated acceptance + docs) завершён 2026-10-20.**
`cmake --build build` — `ninja: no work to do`; полный `ctest --test-dir build
--output-on-failure` с `GL_GAME_DIR` — **8/8 passed**, 541.46 s (real-data 510.18 s,
glview 18.80 s, glgame 12.14 s). Все три screenshot-прогона `glgame` — exit 0,
PNG 1280×720 и не одноцветные: menu 38 273 B / 15 цветов, level 2 715 273 B /
115 244 цвета, paused 2 714 099 B / 115 177 цветов (SHA-256 в task-4-report).
P2b automated acceptance passed; manual interactive gate пройден пользователем
2026-10-20 (анимации `l1_1`, `gv_pause 1/0`). P1 manual desktop pass остаётся открытым.

**P2a — Task 7 (automated acceptance + docs) завершён 2026-10-20.**
`cmake --build build` — `ninja: no work to do`; полный `ctest --test-dir build
--output-on-failure` с `GL_GAME_DIR` — **8/8 passed**, 550.76 s (real-data 512.93 s,
glview 25.30 s, glgame 12.16 s). `glgame --menu --screenshot` и `--level l1_1
--screenshot` — exit 0, оба PNG 1280×720 (menu.png 38 273 B, 15 цветов;
level.png 2 715 273 B, 115 244 цвета; SHA-256 в task-7-report). Manual desktop gate
**пройден 2026-10-20** (подтверждено пользователем): New Game → `l1_1`, `~`, `help`,
`gv_pause`, `exit` на обычном рабочем столе с реальным вводом. P2a принята.

**P2a — Task 6 (`glgame` shell) завершён 2026-10-20.** Добавлен `tools/glgame`
(меню, уровень через `LevelScreen`, консоль `~`, команды, конфиг/`cl_username`
round-trip); `gl_core` отклоняет дубли имён команд. `--screenshot` меню/уровня,
`--exec`, коды выхода 0/1/2/3. Тесты `glgame_screenshot_tests` (skip 77 без
`GL_GAME_DIR`, управляемый temp INI): CLI `--width/--height` приоритетнее
`r_resolution`, меню и уровень 1280×720, `help`+unknown не фатальны, `cl_username`
сохраняется с `bind`/комментариями, `gv_pause`/`r_windowed` персистятся.
`d_console` применяется из конфига при старте, server-события проходят через
`ClientCore`. Полный `ctest` с `GL_GAME_DIR`: 8/8 passed (real-data 503.11 s,
glgame 11.22 s).

**P2a — Task 5 (state machine + ServerCore/ClientCore skeleton) завершён 2026-10-20.**
В SDL-free `gl_core` добавлены `StateMachine` (`boot→menu→level→menu`, `level` reload,
`exit` final) и `ServerCore`/`ClientCore`: `submit(Command)` + парсящий overload,
`tick`, `paused()`/`free_camera()`, `drain_events()`. Контракт `Event`:
`paused_changed` — `flag` = новое состояние; `config_changed` — `key`/`value`/`text`
(`flag` = bool для `r_windowed`); `log` — `text` = сообщение. `ServerCore` не пишет файлы;
`Config` применяет оболочка (Task 6). `level→level` — reload только на уровне state machine,
`ServerCore` не сбрасывается, reload мира принадлежит оболочке. Лишние аргументы
`gv_pause`/`r_windowed`/`cl_username` дают usage-log и ничего не меняют. Тесты
`test_state.cpp`/`test_server.cpp` в `gl_core_tests` зелёные.

P2a — Task 2 (реестр команд и консоль) завершён 2026-10-20: `CommandRegistry`
(регистрация, execute, `help` с сортировкой, автодополнение по префиксу),
`split_command_line` (пробелы, кавычки, экранирование `\"`/`\\`) и `ConsoleBuffer`
(вывод до 256 строк, история до 64 с навигацией и сбросом).

P2a — Task 1 (`gl_core` + Config) завершён 2026-10-20: `gl::assets::utf8_to_cp1251` и
SDL-free `gl::core::Config` (cp1251 INI, сохранение неизвестных строк/комментариев/`bind`,
порядка и смешанных CRLF/LF; `get`/`set` строк/int/float/bool).

P1 — Level viewer: автоматизированная приёмка завершена 2026-10-20; финальный manual
desktop pass (WASD/стрелки, `Home`, визуальный `F1`) остаётся открытым (Open Questions).

### История задач P1

Task 1 завершён: SDL2 (`release-2.32.10`, FetchContent),
`gl_platform` (`SdlContext`, `Window`), placeholder-таргеты `gl_render`/`gl_game`,
`glview` с `--help` и окном 1280×720 (закрытие по Esc/quit).
Проверено: сборка без предупреждений в first-party коде; `ctest` без `GL_GAME_DIR` —
2 passed + 1 skipped; `gl_assets_real_tests` с `GL_GAME_DIR` — 2/2 (32 assertions);
окно открыто и закрыто с exit 0. Коды выхода `glview`: `0` успех, `1` аргументы,
`2` I/O, `3` формат/загрузка.

Task 2 завершён: `gl::assets::parse_lvl` (`lvl.hpp`/`lvl.cpp`) разбирает 12 блоков LVL
в строгом порядке, строки cp1251→UTF-8, проверяет имена/размеры/counts/границы;
SENV/WTHR/DOOR валидируются без моделирования. После fix round: overflow-safe проверка
`BLK_MHDR`, структурная валидация SENV (path + 48-byte запись), D-017.
Тесты: `test_lvl.cpp` (unit 79 cases, 475 assertions) и `test_real_levels.cpp` —
175/175 реальных `levels/lvl/*.lvl` из `levels.pak`, 0 ошибок.
Полный `ctest` с `GL_GAME_DIR`: 3/3 passed.

Task 3 завершён: `gl::assets::parse_lao` (`lao.hpp`/`lao.cpp`) и `parse_sef`
(`sef.hpp`/`sef.cpp`). LAO: 33/33 `data/animated/*.lao` из `levels.pak` (33 pack-каталога),
0 ошибок; пустой файл допустим. SEF: `levels/single/l1_1/l1_1.sef` → pack `l1_1`;
реальный ключ `exit_to_globalmap` принимается как алиас `exit_to_global_map`, pack
нормализуется в ASCII-lowercase (D-018). Unit: 86 cases / 506 assertions.
Полный `ctest` с `GL_GAME_DIR`: 3/3 passed.

Task 4 завершён: `gl::assets::slice_csx_frames` (`csx_frames.hpp`/`csx_frames.cpp`) режет
декодированный CSX на вертикальные кадры фиксированной высоты; последний кадр может быть
короче. Отклоняются нулевые размеры, `frame_height == 0`, `frame_height > image.height`
и несоответствие размера RGBA-буфера. Unit: 90 cases / 547 assertions (+4 cases / +41).
Полный `ctest` с `GL_GAME_DIR`: 3/3 passed.

Task 5 завершён: `gl::game::load_level` (`level.hpp`/`level_loader.cpp`) загружает
SEF→pack, LVL, фон, статику и анимации из `GameVfs`; статика сортируется по `number`
(порядок отрисовки), отсутствующие необязательные файлы — warning и пропуск, фатальны только
SEF/LVL/фон. `gl_game` — статическая библиотека без SDL, зависит только от `gl_assets`.
Интеграционные тесты: `l1_1` — 2760×2016, 2 статики, 2 анимации, позиции, кадры и ceil-размер
соответствуют CSX/LAO; `l6_2_2` — статика в порядке `number`; отсутствующий уровень → io/format.
Полный `ctest` с `GL_GAME_DIR`: 3/3 passed (real-data 389.13 s).

Task 6 завершён: `gl_render` (`Texture`, `Camera`, `to_screen`, `draw_background`) и `glview`
с `--game-dir/--level/--width/--height/--screenshot/--frames`. Фон рисуется одним
`SDL_RenderCopyF` из видимой области текстуры, скриншот — `SDL_RenderReadPixels` RGBA →
`stbi_write_png`. Пути из `argv` конвертируются из UTF-8 (SDL2main на Windows). Render-тест
`glview_screenshot_tests`: `l1_1`, 1280×720, ≥2 цветов; без `GL_GAME_DIR` — skipped (77).
Полный `ctest` с `GL_GAME_DIR`: 4/4 passed (real-data 389.59 s); без `GL_GAME_DIR`: 2 passed,
2 skipped. Реальное окно открыто (процесс жив через 3 с), PNG с реальным драйвером — 1280×720.
Fix round 1: `gl_render` зависит от `gl_platform` + `gl_assets` (не от `gl_game`);
отсутствие обязательных SEF/LVL/фона возвращает `ErrorCode::format` (exit 3), добавлен
CLI-тест на пустой каталог; `parse_args` обёрнут в try/catch (exit 1). Полный `ctest`
после правок: 4/4 passed (real-data 392.88 s).

Task 7 завершён: `gl_render::draw_texture` (`SDL_RenderCopyF` в `to_screen`, пропуск
невидимых спрайтов) и отрисовка статики/анимаций в `glview`. Текстуры статики и кадров
анимаций создаются при загрузке (SDL-зависимость осталась только в app/render);
порядок — фон → статика (по `number`) → анимации. Кадр анимации:
`(elapsed_ms / delay_ms) % frames.size()`, при `delay_ms == 0` — первый кадр.
Интерактивный режим использует `SDL_GetTicks64`, скриншот — детерминированные
`elapsed_ms = N * 16`. Добавлен флаг `--background-only` (описан в `--help`).
Render-тест: normal `--frames 5` и `--background-only` — оба 1280×720, различие
> 1000 пикселей. Полный `ctest` с `GL_GAME_DIR`: 4/4 passed (real-data 392.15 s,
render 8.27 s).

Fix round 1: все текстуры создаются с `SDL_BLENDMODE_BLEND` (проверяется render-тестом),
`delay_ms == 0` всегда даёт кадр 0, viewport запрашивается один раз на кадр и передаётся
в `draw_texture`, предупреждения о пропущенных текстурах содержат уровень/номер/позицию,
тестовые PNG и временные каталоги освобождаются RAII. Полный `ctest` с `GL_GAME_DIR`:
4/4 passed (real-data 421.25 s, render 8.73 s).

Task 8 завершён (приёмка P1): в `glview` добавлены камера (WASD/стрелки `600/zoom`,
drag ЛКМ, wheel-зум к курсору `0.25…4.0` шагом `1.1`, `Home`, `Esc`), clamp к границам
карты (карта меньше окна центрируется), `F1` — рамки статики/анимаций, info-лог
`level '<name>' (pack '<pack>'): WxH px, N statics, M animations`.
Логика clamp/панорамы/зума вынесена в `gl_render` (`clamp_camera`, `pan_camera`,
`zoom_camera_at`), unit `camera_tests` — 6 cases / 31 assertion. Скриншот `l1_1`
1280×720: exit 0, PNG 3 122 294 байта, лог `2760x2016 px, 2 statics, 2 animations`.
Добавлен `--debug-overlay` для screenshot-режима (по умолчанию выключен, на `--screenshot`
без флага не влияет); render-тест снимает одинаковые кадры с флагом и без и проверяет
различие (рамки статики/анимаций нарисованы). Интерактивное окно открывается и
закрывается; drag-панорама и wheel-зум проверены на реальном уровне инъекцией мыши
(software renderer): отличие кадров 2932/14442 и 5383/14442 при фоновом различии
анимации 207/14442. WASD/стрелки, `Home` и визуальный `F1` toggle end-to-end не
прогонялись (в headless-сессии недоступен клавиатурный фокус): их код проверен ревью,
математика панорамы/зума — unit-тестами.
Полный `ctest` с `GL_GAME_DIR`: 5/5 passed (real-data 377.04 s, render 12.77 s).

Финальная волна правок P1 (2026-10-20): тайминг анимации вынесен в
`gl_render::animation_frame_index` с unit-тестами; screenshot-тест `--frames 1` vs `--frames 8`
подтверждает смену кадра; alpha-blend тест перенесён в негейтед `camera_tests`
(`SDL_VIDEODRIVER=dummy`). Полный `ctest` с `GL_GAME_DIR`: 5/5 passed (real-data 401.94 s,
render 18.73 s).

**P0 — Ассеты и форматы.** Реализация и приёмка завершены (Tasks 0–10).

Перед запуском `glpak` и тестов LLVM-MinGW `bin` должен быть в `PATH`
(иначе `0xC0000135` — не найдены runtime DLL).

Проверено на `I:\Games\Златогорье-2`:
- `cmake --build build` — успех, без предупреждений.
- `ctest --test-dir build --output-on-failure` (`GL_GAME_DIR` задан): unit 68/68
  (390 assertions), real-data 2/2 (32 assertions), glpak CLI 1/1 (12 assertions),
  0 skipped. Без `GL_GAME_DIR` real-data тест помечается skipped (exit 77).
- Real-data декодирование: `.csx` 19 704, `.sdb` 315, `.itm` 1848; 1 пустой `.csx`
  (`levels/pack/tl41/bitmaps/minimap.csx`) пропущен как отсутствующий ресурс.
- `glpak info` открывает все 16 архивов без ошибок; количества не изменились
  (scripts 4485, sdb 45, items 3786, engineres 1041).
- Полностью распакованы 4 обязательных архива, количества и размеры совпали:
  scripts 4485/9 324 686, sdb 45/1 411 188, items 3786/28 847 595,
  engineres 1041/62 293 207. Остальные базовые архивы распакованы не были: их суммы
  измерены по полю `size` FAT и сверены с фактической распаковкой этих четырёх.
- `glpak decode-csx --file engineres/globalmap/hero.csx` → `%TEMP%\hero.png`
  (27×27, 253 байта; исходный `hero.csx` — 390 байт). PNG проверен программно
  (загрузка через System.Drawing, размеры совпали); визуальная проверка не выполнялась.
  `engineres/editor/person.csx` → 32×81 PNG.

P0 закрыт 2026-10-19; далее выполнялись задачи P1 (см. выше).

## Критерии приёмки P0 (кратко)

- `gl_assets` собирается на Windows x64 (CMake + Ninja, C++20), не зависит от SDL/ОС.
- `glpak` открывает все `.pak` в `I:\Games\Златогорье-2\Data`, включая `*.update.*.pak`.
- Распаковка обязательных архивов совпадает по количеству файлов и суммарным размерам
  с эталоном; остальные архивы сверены по суммарному `size` из FAT.
- Декодеры CSX/SDB/ITM/BMP покрыты unit-тестами и не падают на всех файлах игры.
- `ctest` зелёный; результаты команд зафиксированы в этом файле.
- Обновлены `PROJECT.md`, `DECISIONS.md`, `docs/research/`.

## Сделано

- 2026-10-19: обследованы `I:\Games\Златогорье-2` и `I:\Games\Златогорье2 ХН`.
- 2026-10-19: выбран путь «чистая реконструкция C++20 + SDL2».
- 2026-10-19: декомпозиция P0–P10, протокол сохранения контекста, design-spec.
- 2026-10-19: создан git-репозиторий, коммит `cd8009e` опубликован в
  `https://github.com/Vologod/goldenland2dec_app` (ветка `main`).
  SSH: ключ `~/.ssh/id_ed25519_gl2`, repo-local `core.sshCommand`.
- 2026-10-19: написан план реализации P0 (`docs/plans/2026-10-19-p0-assets-formats.md`).
- 2026-10-19: Task 9 — CLI `glpak` (`info/list/extract/decode-*`), сборка LLVM-MinGW,
  smoke-тест на `I:\Games\Златогорье-2`, `ctest` зелёный.
- 2026-10-19: исправлен формат payload `.pak` (D-013) и правило `damage` в ITM (D-014);
  факты зафиксированы в `docs/research/formats.md`.
- 2026-10-19: Task 10 — `gl_assets_real_tests` (`GL_GAME_DIR`), приёмка распаковки
  и CSX-декодирования; P0 закрыт.
- 2026-10-19: P1 Task 1 — `cmake/SDL2.cmake` (FetchContent, SDL2 `release-2.32.10`),
  `gl_platform` (`SdlContext`/`Window`), `gl_render`/`gl_game` placeholders, `glview`.
- 2026-10-19: финальная волна правок P0 — продолжение batch-extract после битых
  записей (exit 3), CLI-тест, полное real-data декодирование `.csx/.sdb/.itm`,
  проверка размеров изображений до аллокации, валидация дубликатов/перекрытий PAK,
  контекст ошибок архив/файл/offset.
- 2026-10-19: P1 Task 2 — парсер LVL (`gl_assets::parse_lvl`), synthetic + real-data
  тесты (175/175 уровней), `ctest` 3/3.
- 2026-10-20: P1 Task 3 — парсеры LAO (`parse_lao`) и минимального SEF (`parse_sef`);
  33/33 LAO и `l1_1.sef` на реальных данных, `ctest` 3/3.
- 2026-10-20: P1 Task 4 — `slice_csx_frames`: нарезка CSX на кадры, unit 90/547, `ctest` 3/3.
- 2026-10-20: P1 Task 6 — `gl_render` (Texture/Camera/background) и `glview`
  (load/background/screenshot); render-тест на реальном `l1_1`, `ctest` 4/4.
- 2026-10-20: P1 Task 7 — `draw_texture`, текстуры статики/анимаций, детерминированный
  `--frames`, `--background-only`; render-тест на различие с фоном, `ctest` 4/4.
- 2026-10-20: P1 Task 8 — камера/ввод, `F1` overlay, `clamp_camera`/`pan_camera`/
  `zoom_camera_at` в `gl_render`, `camera_tests`; `--debug-overlay` и render-тест
  оверлея; автоматизированная приёмка P1 (`ctest` 5/5); открыт manual desktop pass.
- 2026-10-20: P2a Task 1 — `gl_core` (`Config`: cp1251 INI, комментарии/`bind`/порядок,
  byte-identical round-trip, `get`/`set`/`set_int`), `utf8_to_cp1251` в `gl_assets`,
  `gl_core_tests`; сборка без предупреждений, `ctest` без `GL_GAME_DIR`: 4 passed,
  2 skipped.
- 2026-10-20: P2a Task 1 fix round 1 — пустой `Config` и загрузка отсутствующего INI как
  defaults (с warning), `save` создаёт файл; real-data round-trip `GoldenLand.ini`
  (`gl_core_real_tests`, skip 77); `get_float` через `from_chars`; append использует
  доминирующий EOL; D-019 (`Result<void>`); план Task 1 отмечен `[x]`.
- 2026-10-20: P2a Task 2 — `CommandRegistry`/`split_command_line`/`ConsoleBuffer` в
  `gl_core`; тесты `gl_core_tests` 25 cases / 227 assertions; полный `ctest` без
  `GL_GAME_DIR`: 4 passed, 3 skipped.
- 2026-10-20: P2a Task 5 — `StateMachine` и `ServerCore`/`ClientCore` skeleton;
  `Command`/`Event` (`key`/`value`) контракт зафиксирован для Task 6; fix round 1
  добавил `submit(Command)`, usage-ошибки на лишние аргументы и тесты payload.
- 2026-10-20: P2a Task 6 — `glgame` (меню, `LevelScreen`, консоль с историей и
  автодополнением, команды `help`/`exit`/`d_console`/`gv_pause`/`gv_free_camera`/
  `r_resolution`/`r_windowed`/`cl_username`/`game_save`, сохранение Config);
  `glgame_screenshot_tests`, отказ от дублей команд в `gl_core`; `ctest` с
  `GL_GAME_DIR` 8/8.
- 2026-10-20: P2a Task 6 fix round 1 — CLI `--width/--height` приоритетнее
  `r_resolution`, `gv_pause` персистится, `d_console` применяется при старте,
  server-события идут через `ClientCore`, пустые строки не попадают в консоль/историю,
  результаты команд логируются; тесты на persistence и пустую историю; `ctest` 8/8.
- 2026-10-20: P2a Task 7 — automated acceptance: `ctest` 8/8 (550.76 s), menu/level
  screenshots 1280×720 (exit 0), research note `docs/research/p2a-game-shell.md`.
- 2026-10-20: P2a manual desktop gate пройден пользователем (меню, New Game → `l1_1`,
  `~`, `help`, `gv_pause`, `exit`); P2a принята.
- 2026-10-20: P2a final fix round — `r_resolution` идёт через `ServerCore`
  (`config_changed`), атомарная запись Config, `//` внутри значения не комментарий,
  отказ от `\r`/`\n` в `set`, New Game всегда `l1_1`, D-020 (отсутствующий INI =
  defaults + warning); `gv_free_camera`/`gv_skip_logo` помечены как P2a-заглушки.
- 2026-10-20: P2b Task 1 — фиксированный 60 Гц `ServerCore` (`tick_index`,
  `simulation_time_ms`, `snapshot`), лимиты `kMaxFrameDt=0.25`/`kMaxStepsPerTick=15`,
  защита от отрицательного и нефинитного `dt`; D-021, unit-тесты детерминизма/паузы.
- 2026-10-20: P2b Task 2 — `ClientCore::on_snapshot`/`snapshot`; snapshot не зависит
  от последующих тиков сервера.
- 2026-10-20: P2b Task 3 — `glgame` тикает сервер от frame delta (кап 250 мс), передаёт
  snapshot клиенту, рисует анимации по `simulation_time_ms`, `PAUSED` — из snapshot;
  ручной `animation_ms_` удалён, screenshot-режим остаётся детерминированным.
- 2026-10-20: P2b Task 4 — automated acceptance: `ctest` 8/8 (541.46 s), menu/level/paused
  screenshots exit 0, 1280×720, не одноцветные; research note
  `docs/research/p2b-deterministic-tick.md`.
- 2026-10-20: P2b manual interactive gate пройден пользователем (анимации `l1_1`,
  `gv_pause 1/0` замирание/возобновление); P2b принят.
- 2026-10-20: P2c Task 1 — `gl::assets::parse_save_header` (magic 20 B, version/date/times,
  title[64] cp1251→UTF-8, offset 40; короткий файл/битый magic → `format`), unit-тесты.
- 2026-10-20: P2c Task 2 — `gl::core::SaveManager` (скан `Saves`, `slot<N>.sav`
  регистронезависимо, сортировка, `valid=false` для битых/не-slot файлов, отсутствующий
  каталог → пустой список), unit и real-data тесты (28 слотов, version 17).
- 2026-10-20: P2c Task 3 — `glgame`: пункт меню `Continue`, экран `Save slots`
  (Up/Down/мышь, прокрутка, `Enter` → сообщение, `Esc` → меню), команда `saves`,
  CLI `--list-saves` (28 строк, exit 0), screenshot-тест меню.
- 2026-10-20: P2c Task 4 — automated acceptance: `ctest` 8/8 (395.96 s),
  `--list-saves` 28 строк / exit 0; research note `docs/research/p2c-save-slots.md`.
- 2026-10-20: P2c manual UI gate пройден пользователем (`Continue` → `Save slots`,
  прокрутка, `Enter`/`Esc`); P2c принят. P1 manual desktop pass остаётся открытым.
- 2026-10-21: P3a Task 1 — `gl_assets`: `parse_cs`/`CsNode`/таблицы имён опкодов и
  функций, cp1251→UTF-8, строгая валидация, fallback для 6 XOR-обфусцированных
  легаси-скриптов; `test_cs.cpp` и `test_real_cs.cpp` (555/555, 0 ошибок,
  338 977 узлов); research note `docs/research/p3a-cs-format.md`.
- 2026-10-21: P3a Task 1 fix round 1 — D-022 + spec §3 (два кодирования), XOR-fallback
  только при невалидном первом опкоде с объединённой ошибкой, real-тест читает
  `scripts.pak` через `PakArchive::open_from_memory` (1.26 s); тесты усечений
  опкода/func и битого legacy; `ctest` 8/8 (453.16 s).
- 2026-10-21: P3a Task 2 — `gl_script` (`parse_globals`, `ScriptHost`, `ScriptVm`):
  логика/сравнения/арифметика/assign/jmp/func, `D_Say`/`D_Answer`/`D_CloseDialog`,
  restart-семантика, лимит 10M шагов; D-023; research note `docs/research/p3a-cs-vm.md`;
  `gl_script_tests` 22 cases / 139 assertions; полный `ctest` 9/9 (594.42 s, по
  фактическому отчёту Task 2; ранее в STATUS было ошибочно указано 478.88 s).
- 2026-10-21: P3a Task 3 — `ScriptVm::dialogs_data()`/`answers_count()`/`user_input()`
  (ответы с нуля, `LastPhrase`/`LastAnswer`, `restart` на узле 0, warning при выходе за
  границы), `DefaultScriptHost` (0 + info/debug/warning по категориям), тесты синтетических
  диалогов и real `demon.d1.age.cs` + `zlato_vars.scr` (`gl_script_real_tests`, skip 77);
  Task 2 minors (правый операнд после ошибки левого, top-level `func` без `d`-перехода,
  unknown-opcode = `end` в spec/plan); D-024; `gl_script_tests` 31 cases / 197 assertions,
  `ctest` 10/10 (561.69 s).
- 2026-10-21: P3a Task 4 — `glpak disasm-cs` (stdout/`--out`, one line per node) и
  `glscript` (`--run`, repeatable zero-based `--answer`, status/steps/dialogs/answers,
  exit 0/1/2/3); `gl_cs_tools_tests` 8 cases / 56 assertions; `ctest` 11/11 (571.23 s).
- 2026-10-21: P3a Task 5 — automated acceptance + docs: build exit 0; `ctest` **11/11**
  (573.09 s, exit 0); real-data `parsed=555 failures=0 nodes=338977`; disasm
  `demon.d1.age.cs` 5353 B / 240 lines; `glscript --run` → `wait_user`, 43 steps,
  dialogs `2684 2686 2696`, 2 answers; `--answer 0` → second `wait_user`, 84 steps,
  dialogs `2719 1529 2721`, 2 answers. P3a принята.
- 2026-10-21: P3a final fix wave — eval budget, assign type coercion, opcodes
  `1/3..5/12/13/19/20`, sticky error semantics (stop + no log flood), VM-failure CLI
  test, globals BOM/unterminated-quote warning, multiple `D_Say` documented;
  `gl_script_tests` 49/374, `gl_script_real_tests` 1/20, `gl_cs_tools_tests` 8/56,
  полный `ctest` 11/11 (560.62 s).
- 2026-10-21: P3c Task 2 — `gl::script::DialogSession` (`dialog_session.hpp/.cpp`):
  `start`/`choose` гонят VM до `wait_user`/`end`/`infinity`, id→текст через `Sdb::find`,
  fallback `<phrase N>`, `waiting`/`npc_text`/`answers`; `choose` возвращает `argument`
  за границей; synthetic 5 cases и real `demon.d1.age.cs` + `dialogsphrases.sdb`
  (`gl_script_real_tests`, skip 77); `gl_script_tests` 54/411; full `ctest` 13/13 (606.15 s).
- 2026-10-21: P3c Task 2 fix round 1 — `vm.failed()` после `start`/`choose` → `ErrorCode::format`
  (`status()`/`failed()` остаются доступны); тесты end-path и падающей VM; real-тест проверяет
  `!vm.failed()` и не-ASCII текст; fallback `<phrase N>` покрыт для ответов; контракт
  `start()` (`restart(true)`) задокументирован в заголовке и Open Questions;
  `gl_script_tests` 58/443, `gl_script_real_tests` 2/44, full `ctest` 13/13 (536.01 s).
- 2026-10-21: P3c Task 3 — `glgame`: `--dialog`/`--font`, меню `Test Dialog`, экран
  диалога (панель, TTF-перенос, нумерованные ответы, Up/Down/мышь, Enter/клик, Esc),
  TTF для save-строк; `glscript --sdb` (`id<TAB>text`); research note
  `docs/research/p3c-dialogs-sdb-font.md`.
- 2026-10-21: P3c Task 3 fix round 1 — прямой `--dialog` выключает консоль независимо от
  `d_console`; шрифты `--font` → системный → ASCII; видимое сообщение `choose()` при
  `waiting`; подсказка `Esc to close` без ответов; вертикальный clamp длинного текста
  (скролл — backlog); тесты `--font`/fallback/отличие от меню/нижняя панель/`d_console`/
  реальная загрузка шрифта, 77 без шрифта; `gl_cs_tools_tests` 9/9 (67 assertions),
  `glgame_screenshot_tests` 11/11 (182), полный `ctest` 13/13 (629.23 s).
- 2026-10-21: P3c Task 3 fix round 2 — `--no-font` (ASCII dev/debug); цепочка шрифтов
  закреплена тестами A/B/C (другой системный TTF, ASCII, невалидный `--font` = дефолт);
  подсветка обрезается по `content_bottom`; `dialog_message_` в отдельной полосе ниже
  ответов; проба шрифта кэширована; `glgame_screenshot_tests` 13/13 (236), полный
  `ctest` 13/13 (607.05 s).
- 2026-10-21: P3c Task 4 — automated acceptance + docs: build exit 0; полный `ctest`
  13/13 (652.23 s), 318 test cases / 2649 assertions; `glscript --sdb` — `2684<TAB>Чего
  уставился?` и др.; dialog PNG 1280×720, 552 sampled / 771 unique colours, SHA-256
  `2544FFD6...`; P3c automated acceptance passed.
  Ограничение P2c с `?` в заголовках save-слотов закрыто TTF.
- 2026-10-21: P3c manual dialog UI gate пройден пользователем (кириллица, ответы,
  Up/Down/Enter/мышь, Esc); **P3c принят**.
- 2026-10-21: P4a Task 1 — `parse_pad` (PAD): 276/276 реальных `.pad`, 2450 анимаций;
  контракт хвоста, direction helpers (`pad_person_row`/`pad_shadow_row`), `delay <= 0` —
  ответственность вызывающего; fix round 1 (c8ba5ae..c9dc9a2), ревью чистое.
- 2026-10-21: P4a Task 2 — person `.scr` (`parse_person_script`, вложенные блоки
  плющатся в `block.key`) + SEF `persons`: `l1_1` = **16** NPC (не 41; 41 — все `name:`
  блоки), 15 уникальных скриптов, все парсятся; sweep 261/261 `.sef`; fix round 1
  (6bd9b22..d67babd), ревью чистое; полный `ctest` 13/13 (1332.96 s).
- 2026-10-21: P4a Task 3 — `gl::game` (`person.hpp/.cpp`): `PersonInstance`,
  `load_persons` (SEF → person `.scr` → PAD → `rt_stay`/`tb_stay` CSX + optional
  shadow), `Level::persons`; missing resources → warning + skip; real `l1_1` —
  16 instances (10 `rt_stay`, 6 `tb_stay`); `gl_game_tests` 3/3 (192 assertions);
  commit `1e5005b`.
- 2026-10-21: P4a Task 4 — `gl::app::person_render` (`person_frame_index`,
  `place_person`, `place_person_shadow`) и отрисовка NPC в `LevelScreen`
  (background → statics → animations → persons, shadows behind, `SDL_FLIP_HORIZONTAL`
  для зеркальных направлений); `--no-persons` в `glview`/`glgame`; screenshot-тесты
  normal vs `--no-persons`; `person_render_tests` 6/6 (41 assertion); commit `94d8037`.
- 2026-10-21: P4a Task 5 — automated acceptance + docs: полный `ctest` **15/15**
  (849.95 s, exit 0); PAD **276/276**, 2450 анимаций, 0 ошибок; `l1_1` 16 persons;
  screenshot 1280x720 (`16 persons`, diff 3 828 px vs `--no-persons`, SHA-256
  `8B377011...`); P4a automated acceptance passed.
- 2026-10-21: P4a manual desktop gate пройден пользователем: NPC в правильных позициях,
  idle-анимация есть, `Esc`/меню работают; **P4a принят** (движение — P4b).
- 2026-10-21: build — MinGW runtime DLLs (`libc++.dll`, `libunwind.dll`,
  `libwinpthread-1.dll`) копируются в `build/bin`, запуск без toolchain в `PATH`;
  commit `dd3445b`.
- 2026-10-22: P4b Task 1 — SEF `cell_groups` (`SefCellGroup`, `SefData::cell_groups`,
  `cell_NN x y` в любом порядке, пустая группа); `l1_1` — 9 групп (`track1` 9,
  `track2` 22, `track3` 8, `track4` 7, 5 `box*`), SEF sweep 261/261.
- 2026-10-22: P4b Task 2 — `gl::game::person_movement`: `RouteType`,
  `direction16_from_velocity` (пороги 0.2079/0.5878/0.8616/0.9863),
  `direction16_to8`/`direction8_to16`, `iso_speed`.
- 2026-10-22: P4b Task 3 — `PersonInstance::route_cells`/`go_type`/`go_move_x/y`/
  `go_delay_ms`; `cell_groups` резолвятся регистронезависимо, `rt_go` с fallback на
  `tb_go`; `RANDOM*` не требуют клеток; warning только для route-типов без маршрута/`_go`.
- 2026-10-22: P4b Task 4 — `gl::game::LevelSimulation` (SDL-free, шаг 1/60,
  xorshift32 seed от уровня/имени/индекса, `MOVED`/`MOVED_FLIP`, `RANDOM*`,
  паузы, 4-px прибытие, диагональная скорость, `state_started_ms`).
- 2026-10-22: P4b Task 5 — `LevelScreen` владеет симуляцией, грузит go-листы/тени и
  рисует движущихся NPC (16 направлений); `--sim-ms`/`--no-persons` в `glview`/`glgame`;
  screenshot-тесты t=0 vs t=10 с; `person_render_tests` 9/9 (52 assertions).
- 2026-10-22: P4b Task 6 — automated acceptance + docs: полный `ctest` **15/15**
  (534.58 s, exit 0), **391 cases / 5544 assertions**, NPC-mask XOR 3001 px,
  детерминизм 60 с подтверждён; **P4b automated acceptance passed**, manual gate OPEN.
  Спека/план/`formats.md` выровнены с реализацией (silent unknown route, RANDOM
  fallback, реальные индексы маршрутов, `state_started_ms`).
- 2026-10-22: P4b Task 6 fix round 1 — постоянный real-тест
  `real data: l1_1 60 s simulation is bounded and deterministic`: 16 persons,
  10 stationary / 6 movable, границы `2760x2016` на каждом шаге, бит-в-бит
  повторный прогон, 1 `MOVED` + 3 `MOVED_FLIP` с маршрутами, 2 `RANDOM_RADIUS`
  без маршрута; **392 cases / 5560 assertions**, полный `ctest` 15/15 (528.13 s).
- 2026-10-22: P4b final review fixes — POSIX capture `2>&1`; cap `--sim-ms` на
  86'400'000 ms в `glview`/`glgame`; warning о corrupt `_go` (delay/movement) и
  `can_move=false`; `PersonMovement` очищен до публичных полей; формулы в
  `formats.md`/spec выровнены; тесты zero-velocity/`iso_speed` и synthetic `tb_go`;
  60-с приёмка усилена (displacement + 3 moving / 3 waiting). Полный `ctest` 15/15
  (503.48 s): **393 cases / 5581 assertions**.

## Следующие задачи

1. P1b: Android/touch — отложенный follow-up P1 (следующий подпроект).
2. P2 backfill: отложенные P2-задачи (меню/сейвы/конфиг/UI).
3. P7-follow-up: свободный TTF для Android (P9), прокрутка длинных диалогов, layout-константы.
4. Parity debt: точный PAD/визуал Вертаса; точная 6-осевая `CanInteract`; hero speed constants; SEF unknown direction hard error.

## Открытые вопросы

- A1/RNG (D-053, supersedes D-030): оригинал инициализирует process-global
  CRT `rand` через `srand` = `FUN_1406c092(timeGetTime())`
  (`decompiled.c:34129`) и усекает в `FUN_140061e4` (`fistp`, RC=`0xC00`),
  а порт использует seed=1, `std::nearbyint` и создаёт `MsvcRandom(seed)`
  заново на каждый уровень. Поэтому утверждение «внутри одного уровня
  последовательность совпадает» более не верно (seed и округление
  различаются). Нужен фикс `MsvcRandom` (seed/clock + truncation +
  float-`coin`) и аудит движения/`STAY_ROTATE`/лута.
- NPC `.inv` loot (1b2b2a, D-053): алгоритм восстановлен, полный паритет
  `blocked`. RNG-фикс D-053 необходим, но недостаточен: specials
  `FUN_14047284`/`DAT_14088df4` используют тот же поток (R6–R10 —
  пропуск/stub сдвигает последующие роллы), а также `BT_RGINF`
  (`+0x26c0`), источник/смысл `+0x26c4`, условие слияния стекинга
  `FUN_1404e3a4` и live-оракул (память/seed не снимались). 1b2b2b —
  только после их закрытия/явного scope.
- P4b: manual gate пройден; принято ограничение — NPC проходят сквозь визуальные
  объекты. Коллизии/pathfinding вынесены в P4b2 (D-027). В t=60 с симуляция даёт
  3 moving + 3 waiting из 6 подвижных (10 `STAY`); «4 движущихся» — это 4
  route-followers (`MOVED`/`MOVED_FLIP`), а не мгновенное число moving.
- P1: финальный manual desktop pass — WASD/стрелки, `Home` и визуальный `F1` toggle
  на обычном рабочем столе. В текущей headless-сессии клавиатурный фокус недоступен;
  drag-панорама и wheel-зум проверены инъекцией мыши, overlay — screenshot-тестом.
- P3c: `DialogSession::start()` intentionally calls `ScriptVm::restart(true)` — variables are
  rebuilt from globals and seeded runtime state is discarded. `start()`/`choose()` map
  `vm.failed()` to `ErrorCode::format`; `status()`/`failed()` remain available for diagnosis.
- Полная десериализация `.sav` и запись сохранений не реализованы (P2c — read-only каркас).
- SDL2 `release-2.32.10` выдаёт CMake deprecation warning из-за своего
  `cmake_minimum_required`; следить при обновлении CMake/SDL.
- `.clang-format` (LLVM base) не совпадает со стилем исходников P0; repo-wide
  форматирование не применялось.

## Backlog P1 (отложенные minor из ревью P0)

- PAK: `reserve(count)` без верхней границы; двойное копирование payload в памяти;
  `PakArchive::find` — O(n) с пересчётом ключа; `mz_uncompress` игнорирует хвост zlib;
  mount читает mode-заголовок каждого entry (I/O при монтировании).
- VFS: `.UPDATE.N.PAK` классифицируется как base (маркер регистрозависим); ошибки обхода
  каталогов проглатываются; дубликаты имён архивов резолвятся в первый; loose-файлы
  не попадают в `list()`/`extract --all`.
- SDB/ITM: копирование строк даже при наличии сигнатуры; `len==0`/дубликаты id не покрыты;
  строковые длины ограничены только размером файла (нет перевода `bad_alloc`).
- CSX/Image: `Result<CsxImage>` копирует пиксели; `bad_alloc` при построении `Error`
  вне guard; лишняя копия RGBA в CLI.
- CLI: недостижимый код выхода 4; `--prefix` не учитывает границу сегмента;
  тест не проверяет запись ошибки в stderr; ANSI-пути Windows.
- Сборка/тесты: `${CMAKE_SOURCE_DIR}` в tests (не embeddable); нет тестов `log`.

## Backlog P1b/P2 (после P1)

- Manual desktop pass: WASD/стрелки, Home, F1 (см. Open Questions).
- Screenshot/write_png: поддержка не-ASCII путей (wide/_wfopen или stbi_write_png_to_func + ofstream).
- SEF: отвергать `pack: ""` (brace-aware секции и sweep всех `levels/single/*/*.sef`,
  261/261, закрыты в P4a Task 2).
- LevelLoader: предупреждение при пустом LAO и непустых ADSC; отличать I/O от format для required-файлов; тесты missing optional.
- Память: освобождать CPU-копии Image после загрузки текстур (background/static/anim frames).
- Проверить соответствие `anim_<i>` ↔ `description.number` на уровне с несортированными/непоследовательными ADSC.
- Сборка без SDL2: опция `GL_BUILD_VIEWER` и условный FetchContent.
- Platform/render nits: SOFTWARE fallback без vsync; null-renderer ErrorCode; `<utility>` include; теоретический uint32 wrap в CSX-нарезке.
- CI: пометить real-data тест как slow / кэшировать фикстуры (~7 мин).

## Backlog P2a → P2b/P7

- P2b: детерминированный тик выполнен (D-021); отложены интерполяция движения и сеть.
- Вынести Game-логику из 	ools/glgame/main.cpp в тестируемый компонент (меню/консоль/состояния покрыты только subprocess-скриншотами и ручным gate).
- gv_free_camera: заглушка P2a — подключить к камере (отключение clamp/следования) или убрать.
- gv_skip_logo: no-op в P2a; удалить из активных ключей или реализовать при экране логотипа.
- Конфиг: валидировать ключи (не только значения) на CR/LF; атомарное сохранение уже есть.
- ServerCore 
_resolution: зафиксировать допустимые границы в контракте события (сейчас shell ограничивает, сервер — нет).
- Тесты: 	est_real_main возвращает 77 безусловно при отсутствии GL_GAME_DIR; text height scaling; non-ASCII UI — P7.
- Уточнить exit code в spec §7: отсутствующий обязательный аргумент CLI — 1, отсутствующий каталог — 2.

## Backlog P2c → P7

- Proper font / cp1251 glyphs: save-slot titles currently show `?` for Cyrillic bytes
  (`stb_easy_font` is ASCII-only); accepted P2c limitation, implement in P7.

## Backlog P2b → P2c

- Добавить determinism-тест «разные последовательности dt с одинаковой суммой → одинаковые tick/time».
- glgame: вынести sync-последовательность 	ick → on_snapshot → process events в helper.
- Screenshot-тесты проверяют размеры, не пиксели; золотые хэши — позже.
- Анимации стартуют от глобального simulation_time_ms; для one-shot анимаций уровня понадобится фаза/сброс.
- 	est_server: non-finite тест переупорядочить (Inf → NaN), чтобы он реально ловил регрессию.

## Backlog P3a → P3b/P4

- Bitwise operands: 0xFFFFFFFF литерал через 	o_int32 теряет старший бит; уточнить семантику DWORD-литералов и >> (логический/арифметический).
- 	o_uint32(-1.0) → 0; при необходимости — модульный каст.
- Sticky-fatal: ошибки типов теперь останавливают VM; прогнать остальные реальные диалоги (не только demon.d1.age.cs), чтобы убедиться в отсутствии ложных остановок.
- coerce_assignment для необъявленной переменной создаёт int32 0; string→undeclared теперь фатален — подтвердить на реальных скриптах.
- AgeScript .scr компилятор/интерпретатор (P3b).
- Полный набор native-функций, диалоговый UI и SDB-фразы (P3c/P4).
- P3c: длинные диалоги/узкие окна — сейчас простой вертикальный clamp (лишние строки
  не рисуются); нужен scroll/пагинация ответов и NPC-текста.

## Backlog P3c → P4/P7/P9

- Длинные диалоги: прокрутка/пагинация текста и ответов; клавиатурный выбор только видимых ответов.
- Android: приложить свободный TTF (или путь --font); системные пути Windows — P9.
- Атлас: добавить U+20AC (€), U+0490/U+0491 (Ґ/ґ); учитывать лимит max texture size при больших pixel_height.
- SDL_RenderGeometry — поднять/проверить минимум SDL (2.0.18) и pragma stb_truetype для Clang/NDK.
- UI выбора диалога (список 555 .cs) и привязка к NPC — P4.

## Backlog P4a (после приёмки)

- SefPerson не хранит literary_name (есть в SEF, P4a не нужен) — добавить в P4b.
- Нет Y-сортировки NPC (P4b); --no-persons всё равно декодирует текстуры; дубликат 
es_name грузится дважды; загрузка person'ов eager при каждом load_level — сделать ленивой/опциональной в P4b.
- Нет валидации размеров CSX против PAD в загрузчике (рендер проверяет); битая опциональная тень молчит.


- 2026-10-22: P4b2 bugfix — «дёргание» NPC (L0.P7_Kolbasens_Rodstvennik1,
  ~5731 переключений moving/180 с). Причина: per-tick проверка проходимости
  тайла под ногами при движении; оригинал проверяет только занятость другими
  сущностями (FUN_14020a18). Исправлено: движение проверяет только occupancy,
  WalkGrid — только при планировании. Добавлен регресс-тест
  	ests/game/test_stability.cpp (180 с, seed 0x5EED; после фикса 38
  переключений); gl_game_tests 44/44, 473 605 assertions.
