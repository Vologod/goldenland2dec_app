# PROJECT — Goldenland 2: Android Port

Живой план проекта. Обновляется после каждой задачи.

## Репозиторий

`https://github.com/Vologod/goldenland2dec_app` (ветка `main`).
Ассеты игры в репозиторий не попадают.

## Цель

Нативный порт **«Златогорье 2» v1.3** на Android с полной одиночной кампанией.
Базовая платформа — C++20 + SDL2. Логика реконструируется по `Client.dll`/`Server.dll`
и документации форматов; данные игры используются оригинальные, во время выполнения.

## Целевая игра

- Каталог: `I:\Games\Златогорье-2`
- `GoldenLand.exe` + `Client.dll` + `Server.dll` (v1.3, патчи `*.update.3.pak`)
- `protect.dll` — пустой файл; StarForce не используется.
- Подробности сборки и хэши: `docs/research/00-game-build.md`.

## Не-цели

- Распространение ассетов игры.
- Обход DRM/StarForce.
- Запуск оригинальных бинарников (это отдельная, не выбранная стратегия).
- Мультиплеер в первой версии (граница клиент/сервер сохраняется на будущее).

## Выбранный подход

**Чистая реконструкция движка на переносимом C++20 + SDL2.**
Переиспользуются оригинальные данные (`.pak`, `.scr`, `.cs`, `.csx`, `.sdb`, `.itm`, уровни),
поведение сверяется с оригинальной игрой на Windows как с эталоном (oracle).

Альтернативы (отклонены): сборка машинного декомпила «как есть»; гибрид с эмуляцией x86 DLL.
Полное обоснование — в `docs/specs/2026-10-19-goldenland2-android-port-design.md`.

## Декомпозиция

Статусы: `[ ]` не начат, `[~]` в работе, `[x]` готов.

| # | Подпроект | Результат | Зависит от | Статус |
|---|---|---|---|---|
| P0 | Ассеты и форматы | библиотека `gl_assets` + CLI `glpak`: VFS/PAK/zlib, CSX, SDB, ITM, BMP; unit + real-data тесты; 4 обязательных архива распакованы и сверены, остальные — по FAT | — | `[x]` |
| P1 | Рендер изометрии и уровень | SDL2-рендер реальной локации, камера, анимации | P0 | `[~]` Tasks 1–8: platform, LVL, LAO/SEF, CSX frames, LevelLoader, background, statics/animations, camera/input, debug overlay, screenshot. P1 automated acceptance passed; manual desktop pass pending. Android/touch — P1b |
| P2 | Ядро движка | цикл, `.ini`, консоль/команды, save/load, состояния | P0 | `[~]` P2a/P2b/P2c приняты; полная десериализация/запись save — позже |
| P3 | Скрипт-машина | CS-байткод VM, AgeScript, диалоги, SDB | P2 | `[~]` P3a принята: `.cs` parser, VM + globals, dialog flow + native registry, tools `glpak disasm-cs` / `glscript`, automated acceptance (11/11, 555/555 `.cs`); final hardening: eval budget, assign type coercion, opcodes `1/3..5/12/13/19/20`, sticky errors. P3a/P3c приняты: CS VM + dialogs + SDB + TTF (manual UI gate пройден 2026-10-21). **Phase 1a (VM parity + native dispatch) done 2026-11-06:** VM сверена с `Server.dll` (D-049; `P3-4` verified, `P3-5`–`P3-8` partial), `NativeRegistry` 65/65 + `GameScriptHost` в `glgame`/`glscript`; supported 13/65 (`Exit`/`Signal`/`Console`/`Cmd`/`D_PlaySound` + `RS_GetPersonParameterI`/`RS_SetPersonParameterI`/`RS_GetPersonSkillI` + `RS_TestPersonHasItem`/`RS_GetItemCountI`/`RS_GetMoney` + `RS_PersonAddItem`/`RS_PersonRemoveItem`), vm_internal 3, unsupported 49; 49 unsupported требуют lifecycle/party/остаток inventory/quests/time/world, 3 vm_internal — внутренние ID VM (Phase 1b); **Phase 1b1 (person parameters) done 2026-11-06:** `PersonRuntime` 12 параметров с per-parameter presence + `parse_hero_stats` (`BT_HERO` stat-блок, Hero skills `0x02C+4*id`), D-050, полный `ctest` 18/18. **Phase 1b2a (Hero inventory queries) done 2026-11-06:** `ItemNameIndex` (`tech_names.sdb`) + `InventoryRuntime` (Hero `BT_HERO` container `0x10`, money = type 18), real boat trader/`l1_1`, D-051, полный `ctest` 18/18. **Phase 1b2b1 (Hero inventory mutations) done 2026-11-06:** mutable `InventoryRuntime` add/remove (`ItemTemplate` type/flags/style/max_stack из ITM, money через `MON_1_0_1`), live overlay, real gypsy Remove money `43875→41075`/`122008→119208`, D-052, полный `ctest` 18/18. **Phase 1b2b2b (core NPC `.inv` loot) done 2026-11-08:** `InvTable`/`parse_inv`/`load_inv` (238/238 files, 11 543 items, 362 four-number skips), `NpcLootGenerator` (`FUN_1404b1c0`, two passes, main `+0x10`, hot `+0xc` RNG-only), NPC main containers in `InventoryRuntime` generated at level load in SEF order before `LevelSimulation` with the same `MsvcRandom`; real `l1_1` oracle 174/218/323, D-055, full `ctest` 18/18; specials/restock blocked (documented RNG-desync). P4 (NPC) pending |
| P4 | Персонажи/AI/триггеры | `.prs/.ssh/.inf`, поиск пути, встречи | P3 | `[~]` P4a принят (16 NPC `l1_1`, PAD/SEF, automated + manual). P4b принят (маршруты/`_go`, `LevelSimulation`). P4b2 (коллизии/pathfinding): Tasks 1–5, `WalkGrid` из LVL (слово первой записи чанка, `relief|sound<<16`, биты 8..11, column-major; `l1_1` 18028/51520 заблокировано; сверка с live-сеткой 0/51520), LOS + A*, occupancy/обход NPC, `--walkable-overlay`; **automated acceptance passed** (15/15, 413 case / 466 027 assertions, `gl_game_tests` 41 / 462 766); **manual desktop gate passed; P4b2 принят** (2026-10-22). Аудит паритета P0…P4b2 выполнен (D-029); A1 (RNG) переоткрыт/заменён D-053 (D-030 superseded; RNG-паритет реализован 2026-11-08 (D-054); stream не непрерывен между уровнями), A2 (индексы маршрутов, D-031), A3 (RANDOM, D-032), A4 (STAY/STAY_ROTATE, D-035), A5 (скорость/тайм-база, D-033) и A6 (occupancy — только текущие тайлы, D-034) закрыты/narrowed 2026-10-23. A4: `STAY` возвращается к spawn и не блуждает; `STAY_ROTATE` вращается на месте с паузой 500 мс (длительность действия `SET_DIRECTION` не извлечена), `can_move` больше не форсится в `false`. A6: резервирования удалены, 110/110 P692-abort — текущая клетка `oficer1`, остаток corner clipping (порог не ослаблялся). A7 (Y-sort): статики/LAO до динамических сущностей; `EF_PERSON` — по Y, затем X; тени (проход 2) и тела (проход 7) по общему порядку; PAD-анкоры и полная ячейка кадра; D-036. По `Shot0001` выявлено перекрытие персонажа перилами статика — это cell mask (`ApplyCellMask`, `BLK_MDSC`, `r_masks_*`), не Y-sort; **A8 реализован** (D-037): `LevelMask`, `compose_mask_foreground`, merge масок в Y-проходы, `glview --camera-x/--camera-y/--place-person/--no-masks`; `mask_45` 182090 px. P4c — клик → диалог: Tasks 1–8, hero (spawn на первом walkable entrance, путь, AABB, маркер 20 px), hit-test по PAD-кадру, `dialog_script_path`, `DialogOverlay` поверх уровня, `glgame --click-npc/--click-world`; real `l1_1` `L1.P621_Svet_3CircusWorkerV` (индекс 13 из `Level::persons`, не сырой SEF) → диалог `scripts/dialogs/l1_1/l1.p621_svet_3circusworkerv.d621.age.cs`, `answers=2`, непустой текст; moving NPC `L0.P678_citizen4`/`L0.P7_Kolbasens_Rodstvennik1` также открывают диалог; клик целится в текущую клетку `PersonMovement`, ретаргет при движении, ПКМ/недостижимая цель/non-talkable и приоритет ввода overlay — D-039; **automated acceptance passed** (`glgame_screenshot_tests` 20/20 / 391 assertion; полный ctest 17/17, 865.27 s); **manual desktop gate PASSED 2026-10-24 — P4c принят**. Ограничения: hero-маркер вместо PAD Вертаса, AABB вместо 6-осевой формулы `CanInteract`, server `0x12`/`OpenDialog` и точный визуал Вертаса вне P4c. **Phase 1b2b2a (2026-11-07):** NPC `.inv` loot research — алгоритм восстановлен, полный паритет `blocked` (specials `DAT_14088df4` + shared RNG, `BT_RGINF`, `+0x26c4`, stacking, live-оракул); RNG-паритет follow-up **D-053** (seed `timeGetTime` + truncation) необходим, но недостаточен и затрагивает движение/`STAY_ROTATE`/лут. **P4 RNG parity (2026-11-08, D-053/D-054):** `MsvcRandom` приведён к `Server.dll` — `range` truncation `lo+trunc(((hi-lo)+0.99999)*rand/32767)`, `coin` — float; production seed — `steady_clock` ms, `--seed` override в `glgame`/`glview`, тесты пинят seed 1; movement/`STAY_ROTATE` ожидания пересчитаны; anti-livelock guard — port-side (D-054). Ограничения: `timeGetTime` недетерминирован, RNG-поток не непрерывен между уровнями, NPC-loot specials/`DAT_14088df4` blocked. |
| P5 | Игровые системы | параметры/скиллы, инвентарь, магия, бой | P4 | `[~]` P5a save→outfit **accepted 2026-11-04** (Tasks 1–4): `decode_save_body`/`parse_hero_equipment` (BT_HERO equipped, all items), `parse_herowear` + `resolve_hero_outfit` (item id→TECH→group, set A, naked fallback), `LevelScreen::apply_hero_outfit` без reload, `glgame --equip-save`/меню. Real slot0/slot2 0/N, full ctest 17/17 (1016.00 s), manual visual gate PASSED; set A/B switch риск вне P5a. P5b1 read-only inventory UI **implemented 2026-11-05** (Tasks 1–5): `parse_hero_inventory` (3 контейнера), `ItemCatalog` (`lit_names`/`descriptions`/icons), `visible_inventory`/эксклюзивные фильтры, `InventoryOverlay` (letterbox 1024×768, bottom/hot/puppet, панель D-045, native-size icons), `glgame --inventory`/`I`; real slot0/slot2, полный ctest **18/18** (1278.27 s); manual gate PASSED; paper-doll overlays → P5b2. **Phase 1b2a (2026-11-06):** read-only Hero inventory query runtime (`ItemNameIndex` + `InventoryRuntime`, D-051). **Phase 1b2b1 (2026-11-06):** mutable Hero inventory + `RS_PersonAddItem`/`RS_PersonRemoveItem` (add/remove/stacking по `Server.dll`, live overlay, D-052). **Phase 1b2b2b (core NPC `.inv` loot) done 2026-11-08:** `parse_inv` + `NpcLootGenerator` (`FUN_1404b1c0`) + NPC main containers generated at level load; real `l1_1` 174/218/323; D-055; specials/restock blocked. |
| P6 | Мир | глобальная карта, локации, квесты, дневник, торговля | P5 | `[ ]` |
| P7 | UI | все экраны и тач-раскладка | P5 | `[ ]` |
| P8 | Аудио | замена `Dare.dll` (SDL_mixer/OpenAL) | P1 | `[ ]` |
| P9 | Android | APK, упаковка ассетов, тач-управление, жизненный цикл | P1–P8 | `[ ]` |
| P10 | Паритет и полировка | прохождение кампании, баланс, багфиксы | всё | `[ ]` |

**P4d (hero equipment/render) — accepted:** HAD/IAD/SEQ-парсеры, engine-композитор (`FUN_0042279C`), `HeroVisual`, hero-рендер в `LevelScreen`/`glview`/`glgame`; golden slot0 idle совпал, shadow draw-origin по live. Экипировка из save — P5a. Детали — `docs/research/p4d-hero-wear.md`, `STATUS.md`.

**A8 parity (cell masks) — implemented:** engine-модель `ApplyCellMask`; `Level::MaskCellGrid` из LVL (`flags=relief|sound<<16`, `mask=LvlTile.mask`), `select_cell_mask` (сканы вправо/вниз), `LevelScreen` выбирает mask id в строке персонажа и рисует клетки кадра с этим id; baseline удалён. `ctest` 17/17; manual gate PASSED 2026-11-04. Детали — `docs/specs/2026-11-03-a8-mask-parity-design.md`, research `a8-mask-parity.md`.

**Parity matrix & decomp reference (Phase 0) — done 2026-11-05:**
`docs/research/parity-matrix.md` — 163 строки покрытия (на 2026-11-08:
`verified` 49, `partial` 63, `unverified` 22, `missing` 17,
`contradicted` 2, `superseded` 10); `docs/research/decomp-index.md` + локальный корпус
`Server.dll` (1108 функций, SHA-256
`418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`) в
`.local/decomp/server/`; D-048. Phase 1a (VM parity + native dispatch)
закрыта 2026-11-06; Phase 1b1 (person parameters) закрыта 2026-11-06,
Phase 1b2a (Hero inventory queries) закрыта 2026-11-06, Phase 1b2b1 (Hero
inventory mutations) закрыта 2026-11-06 (supported 13/65, 49 unsupported +
3 vm_internal); Phase 1b2b2a (NPC `.inv` loot research) закрыта 2026-11-07:
алгоритм восстановлен, полный паритет `blocked` по specials
(`FUN_14047284`/`DAT_14088df4`, shared RNG), `BT_RGINF`, источнику
`+0x26c4`, stacking и live-оракулу; RNG-паритет D-053 реализован
(2026-11-08, D-054). Phase 1b2b2b (core NPC `.inv` loot) закрыта
2026-11-08: `parse_inv`, `NpcLootGenerator` (`FUN_1404b1c0`), NPC main
containers при загрузке уровня, real `l1_1` oracle, D-055; specials и
restock остаются blocked. Дальше — research specials, Transfer/Trade/
`PassToTradePanel` или save payload (`P2-15`), затем 35 Гц.

## Архитектура (кратко)

```
app/        состояния приложения, точка входа
platform/   окно, ввод (клавиатура/мышь/тач/геймпад), время, ФС, логирование (SDL2/Android)
core/       цикл, конфиг .ini, консоль/команды, события
assets/     VFS, форматы и декодеры (без SDL и платформенных API)
render/     текстуры, спрайты, изометрия, камера, анимации, UI-примитивы
script/     CS VM + AgeScript + биндинги нативных функций
game/       мир, персонажи, предметы, локации, системы
ui/         экраны, диалоги, HUD, тач-раскладка
audio/      музыка/звук
ServerCore / ClientCore — граница оригинальной клиент-серверной архитектуры
```

Соответствие оригиналу:

- `SERVER.DLL` → `game/` + `script/` (логика, симуляция, скрипты).
- `CLIENT.DLL` → `render/` + `ui/` + `audio/` (представление).
- `GoldenLand.exe` → `app/` + `platform/` + `core/` (хост, окно, консоль, цикл).

## Документы

- `AGENTS.md` — правила работы и сохранения контекста.
- `docs/STATUS.md` — текущее состояние и следующая задача.
- `docs/DECISIONS.md` — журнал решений (ADR).
- `docs/specs/` — утверждённые спецификации подпроектов.
- `docs/plans/` — планы реализации.
- `docs/research/` — факты о бинарниках, форматах, поведении (с источниками).
