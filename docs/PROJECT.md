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
| P3 | Скрипт-машина | CS-байткод VM, AgeScript, диалоги, SDB | P2 | `[~]` P3a принята: `.cs` parser, VM + globals, dialog flow + native registry, tools `glpak disasm-cs` / `glscript`, automated acceptance (11/11, 555/555 `.cs`); final hardening: eval budget, assign type coercion, opcodes `1/3..5/12/13/19/20`, sticky errors. P3a/P3c приняты: CS VM + dialogs + SDB + TTF (manual UI gate пройден 2026-10-21); P4 (NPC) pending |
| P4 | Персонажи/AI/триггеры | `.prs/.ssh/.inf`, поиск пути, встречи | P3 | `[~]` P4a принят (16 NPC `l1_1`, PAD/SEF, automated + manual). P4b принят (маршруты/`_go`, `LevelSimulation`). P4b2 (коллизии/pathfinding): Tasks 1–5, `WalkGrid` из LVL (слово первой записи чанка, `relief|sound<<16`, биты 8..11, column-major; `l1_1` 18028/51520 заблокировано; сверка с live-сеткой 0/51520), LOS + A*, occupancy/обход NPC, `--walkable-overlay`; **automated acceptance passed** (15/15, 413 case / 466 027 assertions, `gl_game_tests` 41 / 462 766); **manual desktop gate passed; P4b2 принят** (2026-10-22). Аудит паритета P0…P4b2 выполнен (D-029); A1 (RNG, D-030), A2 (индексы маршрутов, D-031), A3 (RANDOM, D-032), A4 (STAY/STAY_ROTATE, D-035), A5 (скорость/тайм-база, D-033) и A6 (occupancy — только текущие тайлы, D-034) закрыты/narrowed 2026-10-23. A4: `STAY` возвращается к spawn и не блуждает; `STAY_ROTATE` вращается на месте с паузой 500 мс (длительность действия `SET_DIRECTION` не извлечена), `can_move` больше не форсится в `false`. A6: резервирования удалены, 110/110 P692-abort — текущая клетка `oficer1`, остаток corner clipping (порог не ослаблялся). A7 (Y-sort): статики/LAO до динамических сущностей; `EF_PERSON` — по Y, затем X; тени (проход 2) и тела (проход 7) по общему порядку; PAD-анкоры и полная ячейка кадра; D-036. По `Shot0001` выявлено перекрытие персонажа перилами статика — это cell mask (`ApplyCellMask`, `BLK_MDSC`, `r_masks_*`), не Y-sort; **A8 реализован** (D-037): `LevelMask`, `compose_mask_foreground`, merge масок в Y-проходы, `glview --camera-x/--camera-y/--place-person/--no-masks`; `mask_45` 182090 px. P4c — клик → диалог: Tasks 1–8, hero (spawn на первом walkable entrance, путь, AABB, маркер 20 px), hit-test по PAD-кадру, `dialog_script_path`, `DialogOverlay` поверх уровня, `glgame --click-npc/--click-world`; real `l1_1` `L1.P621_Svet_3CircusWorkerV` (индекс 13 из `Level::persons`, не сырой SEF) → диалог `scripts/dialogs/l1_1/l1.p621_svet_3circusworkerv.d621.age.cs`, `answers=2`, непустой текст; moving NPC `L0.P678_citizen4`/`L0.P7_Kolbasens_Rodstvennik1` также открывают диалог; клик целится в текущую клетку `PersonMovement`, ретаргет при движении, ПКМ/недостижимая цель/non-talkable и приоритет ввода overlay — D-039; **automated acceptance passed** (`glgame_screenshot_tests` 20/20 / 391 assertion; полный ctest 17/17, 865.27 s); **manual desktop gate PASSED 2026-10-24 — P4c принят**. Ограничения: hero-маркер вместо PAD Вертаса, AABB вместо 6-осевой формулы `CanInteract`, server `0x12`/`OpenDialog` и точный визуал Вертаса вне P4c |
| P5 | Игровые системы | параметры/скиллы, инвентарь, магия, бой | P4 | `[ ]` |
| P6 | Мир | глобальная карта, локации, квесты, дневник, торговля | P5 | `[ ]` |
| P7 | UI | все экраны и тач-раскладка | P5 | `[ ]` |
| P8 | Аудио | замена `Dare.dll` (SDL_mixer/OpenAL) | P1 | `[ ]` |
| P9 | Android | APK, упаковка ассетов, тач-управление, жизненный цикл | P1–P8 | `[ ]` |
| P10 | Паритет и полировка | прохождение кампании, баланс, багфиксы | всё | `[ ]` |

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
