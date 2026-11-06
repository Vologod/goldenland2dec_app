# Parity matrix

Дата: 2026-11-05. Режим: read-only аудит существующего состояния; код порта не
менялся.

Матрица — единый источник истины о покрытии паритета. Строка без evidence не
считается подтверждённой. Источники: `parity-audit-p0-p1.md`,
`parity-audit-p2-p3.md`, `parity-audit-p4.md` (2026-10-22/23), `STATUS.md`,
`PROJECT.md`, specs/plans P4d/A8/P5a/P5b1 и real-data тесты.

Статусы:

- `verified` — подтверждено оригиналом (RVA/offset/live) или точным
  real-data прогоном, который пинит утверждение.
- `partial` — данные/формат доказаны, семантика из вторичного источника или
  тест не пинит count.
- `unverified` — evidence из оригинала нет.
- `missing` — не реализовано.
- `contradicted` — противоречит подтверждённому факту / воспроизводимый дефект.
- `superseded` — заменено более поздним решением (ссылка обязательна).

ID: `P4a-*`, `P4b-*`, `P4b2-*` — прежние A/B/C из аудита (переименованы,
чтобы не путать с backlog-задачами A1–A8).

## Сводка

Счётчики: заполняются из таблиц ниже (см. `## Итог`).

Топ приоритетных пробелов:

1. **B1 VM** — Phase 1a сверила VM с `Server.dll` (`P3-4` verified;
   `P3-5`/`P3-6`/`P3-7`/`P3-8` partial: недоказанные overflow/string/
   budget случаи остаются `unverified`).
2. **B2 нативы** — реестр и `GameScriptHost` есть, но 57/65 нативов
   `unsupported` (persons/inventory/quests/time/world) → возвращают 0
   (`P3-9`); id `0` встречается 51 раз (`P3-3`).
3. **B3 save/load** — payload не читается и не пишется (`P2-15`).
4. **B4 тайминг** — порт 60 Гц, оригинал `sv_fps=35` (`P2-9`, `P4b-16`).
5. **C1 ADSC** — `number/name` игнорируются, анимации ломаются на 4 уровнях
   (`P1-3`).
6. **PAD crops/anchors** — crops отбрасываются, anchor не сверен с
   `Client.dll` (`P4a-6`, `P4a-12`, `P4a-13`).
7. **6-осевой `CanInteract`** — AABB вместо формулы (`P4c` — см. `STATUS`).
8. **331 runtime-клетка** — двери/скрипты не меняют проходимость (`P4b2-5`).
9. **P5+** — бой, магия, мир, торговля, UI-системы не реализованы.

## P0 — ассеты и форматы

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P0-1 | PAK | magic/count/FAT | `pak.cpp:124-217` | `formats.md:12-39`; вторичный источник | partial | нет SHA-256 `.pak`; суммы распаковки не пинятся | audit p0-p1 |
| P0-2 | PAK | payload `mode==0` raw, иначе zlib | `pak.cpp:73-89,219-279` | `formats.md:25-39`; D-013 | partial | ветка `mode != 1` не подтверждена | audit p0-p1 |
| P0-3 | PAK | запрет дубликатов/лимиты | `pak.cpp:53-151` | unit-тесты | unverified | защитные предположения порта | audit p0-p1 |
| P0-4 | VFS | loose → update.N → base | `vfs.cpp:97-175` | только design | unverified | precedence не подтверждён оригиналом | audit p0-p1 |
| P0-5 | VFS | casefold/`\`→`/` | `cp1251.cpp:145-196`; D-011 | unit-тесты | unverified | эвристика порта | audit p0-p1 |
| P0-6 | cp1251 | cp1251 → UTF-8 | `cp1251.cpp:6-82` | `formats.md:7-10` | verified | байт `0x98` — редкий случай | audit p0-p1 |
| P0-7 | CSX | заголовок/палитра/transparent | `csx.cpp:11-42` | `formats.md:41-58`; XnView plugin | partial | правила прозрачности из плагина | audit p0-p1 |
| P0-8 | CSX | RLE `0x69..0x6C` | `csx.cpp:44-79` | `formats.md:54`; вторичный | partial | пиксельная семантика не сверена | audit p0-p1 |
| P0-9 | CSX | лимиты 64 Ми px | `csx.cpp:7-29`; D-015 | `formats.md:56-58`; D-015 | partial | лимит выбран по максимуму корпуса | audit p0-p1 |
| P0-10 | SDB | подпись/XOR `0xAA` | `sdb.cpp:7-26` | `formats.md:60-70`; real | partial | counts не пинятся; дубликаты не описаны | audit p0-p1 |
| P0-11 | ITM | условные блоки по типу | `itm.cpp:17-89`; D-012/D-014 | `formats.md:72-89`; корпус 1848 | partial | таблица из README/корпуса | audit p0-p1 |
| P0-12 | ITM | неизвестный тип/хвост | `itm.cpp:26-87`; D-012 | D-012 | unverified | поведение оригинала неизвестно | audit p0-p1 |
| P0-13 | images | BMP/PNG/JPG → RGBA | `image.cpp:13-48` | stb_image | partial | 1938 BMP не в sweep; GDI не сверен | audit p0-p1 |
| P0-14 | LVL | 12 блоков, точный payload | `lvl.cpp:196-309` | `formats.md:91-110`; 175/175 | verified (структура) | семантика WTHR/SENV/DOOR не моделируется | audit p0-p1 |
| P0-15 | LVL | тайлы/чанки/проходимость | `walk_grid.cpp:10-32` | live `world+0x330`; 0/51520 | verified | — | audit p0-p1 |
| P0-16 | LAO | пары `(height,delay)` | `lao.cpp:11-31` | `formats.md:112-118`; 33/33 | verified (парсинг) | единица `delay` не извлечена | audit p0-p1 |
| P0-17 | SEF | pack/persons/cell_groups | `sef.cpp:391-483` | `formats.md:119-145`; 261/261 | partial | лексер самописный, поля Person частично | audit p0-p1 |
| P0-18 | PAD | заголовок/маски/crops/shadow | `pad.cpp:196-259` | `formats.md:146-198`; 276/276 | partial | 2450 анимаций не пинятся | audit p0-p1 |
| P0-19 | CSX frames | вертикальные кадры по LAO | `csx_frames.cpp:12-44` | `p1-level-rendering.md:30-34` | partial | на 84 `anim_*.csx` не проверено | audit p0-p1 |
| P0-20 | CS/save | CS/person_script/save парсеры | `cs.cpp`, `person_script.cpp`, `save.cpp` | `p3a-cs-format.md`; `formats.md` | partial | evidence относится к P3a/P4a | audit p0-p1 |

## P1 — уровень и рендер

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P1-1 | загрузчик | SEF → pack → LVL → `layer.jpg` | `level_loader.cpp:33-113` | `p1-level-rendering.md:8-19`; 175 pack | partial | путь подтверждён вручную, не тестом | audit p0-p1 |
| P1-2 | статика | `static_<n>.csx`, сортировка по number | `level_loader.cpp:115-142`; `level_screen.cpp:332-336` | редактор | partial | anchor/порядок/дубликаты number | audit p0-p1 |
| P1-3 | анимации | ADSC ↔ `anim_<i>` ↔ `lao[i]` | `level_loader.cpp:144-208` | редактор; только `l1_1` | contradicted | игнорируются `number/name`; ломается на `l3_2/l4_1/l4_2/l89_1_a` | audit p0-p1, §1 |
| P1-4 | анимации | тайминг `elapsed/delay % frames` | `animation.cpp:5-9` | редактор | unverified | единица delay и `delay==0` не извлечены | audit p0-p1 |
| P1-5 | порядок | фон → статика → анимации | `level_screen.cpp:325-345` | спека P1 | superseded | заменено A7 (Y-sort) и A8 (маски) | audit p0-p1; A7/A8 |
| P1-6 | тайлы | 12×9, чанк 24×18 | `level_screen.cpp:25-26` | Person live; форматы | partial | для статики/фона не подтверждено | audit p0-p1 |
| P1-7 | камера | clamp/pan/zoom | `camera.cpp:8-66` | только порт | unverified | viewer-поведение, не камера игры | audit p0-p1 |
| P1-8 | текстуры | nearest, RGBA32, blend | `sdl_context.cpp:24`; `texture.cpp:57-72` | спека P1 | partial | фильтрация DirectDraw не сверена | audit p0-p1 |
| P1-9 | ошибки | fatal vs warning+skip | `level_loader.cpp:74-208` | спека P1 | partial | нет тестов на несовпадения ADSC/LAO | audit p0-p1 |
| P1-10 | real-data | загрузка всех уровней | тесты только `l1_1`/`l6_2_2` | STATUS | partial | нет sweep 175 уровней | audit p0-p1 |
| P1-11 | скриншот | детерминизм/harness | `glview/main.cpp:212-254` | port tests | partial | SHA-256/точные counts не пинятся | audit p0-p1 |
| P1-12 | маски | загрузка mask/trigger CSX | не грузится | спека P1 | superseded | A8 загружает маски; триггеры — отдельно | audit p0-p1; A8 |

## P2 — ядро, конфиг, консоль, tick, save

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P2-1 | INI | line model/comment/bind | `config.cpp:21-70,212-248` | real INI round-trip | partial | семантика оригинального парсера | audit p2-p3 |
| P2-2 | INI | typed get/set | `config.cpp:137-206` | unit | partial | case/missing/newline — решения порта | audit p2-p3 |
| P2-3 | INI | используемые ключи | `main.cpp:907-949`; `server.cpp:120-207` | real INI | partial | набор/приоритет ключей не извлечён | audit p2-p3 |
| P2-4 | INI | save temp+rename | `config.cpp:102-135`; D-019/D-020 | unit + real | partial | атомарность/ошибки — решение порта | audit p2-p3 |
| P2-5 | консоль | registry/execute/help | `command.cpp:57-127` | unit | unverified | парсер консоли оригинала | audit p2-p3 |
| P2-6 | консоль | buffer/history/keys | `console.cpp:15-53` | unit | unverified | лимиты 256/64/10 — порт | audit p2-p3 |
| P2-7 | команды | 9 команд из ~137 | `main.cpp:802-893` | string scan `Client.dll` | partial | `game_save/load` стабы | audit p2-p3 |
| P2-8 | состояния | boot/menu/level | `state.cpp:6-27` | только порт | unverified | boot/logo/loading/save-menu | audit p2-p3 |
| P2-9 | tick | 60 Гц | `server.hpp:8-11`; `server.cpp:68-95`; D-021 | `sv_fps=35` в live | contradicted | частота не совпадает с оригиналом | audit p2-p3; A5 |
| P2-10 | pause | команды до pause | `server.cpp:68-148` | port tests | partial | порядок/семантика не извлечены | audit p2-p3 |
| P2-11 | граница | ServerCore/ClientCore | `server.cpp:105-218` | design | partial | нет сериализации мира/событий | audit p2-p3 |
| P2-12 | save header | 112 байт/версия/title | `save.cpp:12-50` | 28 слотов | verified | unknown 28/38/104/108 | audit p2-p3 |
| P2-13 | save scan | `slot<N>.sav` | `save_manager.cpp:34-123` | 28 файлов | partial | лимит слотов/чужие файлы | audit p2-p3 |
| P2-14 | save UI | список/скролл/сообщение | `main.cpp:1300-1406` | screenshot tests | partial | оригинальный `save_load_menu.scr` | audit p2-p3 |
| P2-15 | save payload | чтение/запись мира | отсутствует | `game_save` stub | missing | блокирует кампанию | audit p2-p3 |
| P2-16 | provenance | RVA/live для P2 | — | research без RVA | unverified | нужен reverse `GoldenLand.exe` | audit p2-p3 |

## P3 — CS/VM/диалоги/шрифты

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P3-1 | CS | node layout | `cs.cpp:43-126` | 555/555, 338977 nodes | partial | layout из редактора | audit p2-p3 |
| P3-2 | CS | legacy XOR body | `cs.cpp:106-122`; D-022 | 6 файлов, 3 пары | partial | fallback по первому опкоду | audit p2-p3 |
| P3-3 | CS | opcode/function tables | `cs.cpp:128-246` | редактор; id 0 ×51 | partial | id 0 = `unknown` | audit p2-p3 |
| P3-4 | VM | top-level/логика/jmp | `script_vm.cpp:203-650` | research §3–§4: цикл `FUN_1403d944` (`0x3d944`), short-circuit `0x3a7cb`/`0x3a71e`, `jmp` `0x3a141`, `c=d=0` `FUN_1403fb04` `0x3fb15`/`0x3fb18`; D-049 + unit | verified | индекс ответа диалога — P3-8 | Phase 1a Task 2/5 |
| P3-5 | VM | арифметика/битовые/unary | `script_vm.cpp:458-584` | research §4: `/`/`%` `0x3a2a1`/`0x3a222`, `1e10` `0x3a2d8`/`0x3a28f`, `shl`/`sar` `0x3a432`/`0x3a3c2`, unary `0x3a1e2`/`0x3a1ae`; D-049 + unit | partial | int overflow `0x0e-0x10` и DWORD `0x03-0x05` — `unverified` | Phase 1a Task 2/5 |
| P3-6 | VM | assign coercion | `script_vm.cpp:254-287` | research §4: `0x32` `0x3a036`, string dest `0x3a09f-0x3a0b7`; numeric match, D-025 | partial | string-приёмник `unverified` (string-переменных в данных нет) | Phase 1a Task 2/5 |
| P3-7 | VM | limits/errors | `script_vm.hpp:38-40` | research §4: деление/модуль на 0 → `1e10` без ошибки (`0x3a2d8`/`0x3a28f`); sticky errors D-025 | partial | eval budget 100k/limits — решение порта, не извлечено | Phase 1a Task 2/5 |
| P3-8 | VM | dialog data/answer index | `script_vm.cpp:163-201` | research §5: `D_Say` `0x3fc10`, `D_Answer` `0x3fc9c`, `D_CloseDialog` `0x3fc60`; real `2684/2686/2696` | partial | 0-based answer index — порт D-024, `Server.dll` не пинит | Phase 1a Task 2 |
| P3-9 | нативы | dispatch/host | `native_registry.cpp`, `game_script_host.cpp` | `NativeRegistry` 65/65; supported 5 (`Exit`/`Signal`/`Console`/`Cmd`/`D_PlaySound`), vm_internal 3, unsupported 57; `GameScriptHost` one-shot log | partial | 57 нативов требуют подсистем (persons/inventory/quests/time/world) | Phase 1a Task 3/4/6 |
| P3-10 | globals | типы/парсинг `.scr` | `globals.cpp:100-182` | real `zlato_vars.scr` | partial | неизвестные типы/кавычки | audit p2-p3 |
| P3-11 | диалоги | DialogSession | `dialog_session.cpp:12-73` | real demon | partial | покрыт один диалог | audit p2-p3 |
| P3-12 | диалоги | экран/клавиши/панель | `main.cpp:545-583` | screenshot tests | unverified | UI-раскладка порта | audit p2-p3 |
| P3-13 | SDB | parse signature/XOR | `sdb.cpp:7-26` | 315 файлов; 45 mounted | partial | counts/дубликаты не пинятся | audit p2-p3 |
| P3-14 | SDB | id → dialog text | `dialog_session.cpp:51-72` | real 2684/2686/2696 | partial | 12040 записей не sweep | audit p2-p3 |
| P3-15 | шрифт | TTF atlas | `font.cpp:204-440` | `fonts.scr` игнорируется | partial | оригинальные TTF не используются | audit p2-p3 |
| P3-16 | шрифт | normalization/wrap | `font.cpp:54-72,442-537` | только порт | unverified | GDI layout не извлечён | audit p2-p3 |
| P3-17 | шрифт | fallback | `font.cpp:539-562` | screenshot tests | partial | Windows-only; Android нужен TTF | audit p2-p3 |
| P3-18 | cp1251 | строки CS/SDB | `cs.cpp:33-41`; `sdb.cpp:24` | real Cyrillic | verified | undefined bytes не важны | audit p2-p3 |

## P4a — PAD/person/SEF/отрисовка NPC

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P4a-1 | PAD | header | `pad.cpp:196-250` | 276/276 | verified | — | audit p4 |
| P4a-2 | PAD | animation fields | `pad.cpp:103-122` | 276/276; live movement | verified | — | audit p4 |
| P4a-3 | PAD | crops/shadow tables | `pad.cpp:63-155` | 276/276 | verified (parse) | renderer crops игнорирует | audit p4 |
| P4a-4 | PAD | tail contract | `pad.cpp:162-193` | 276/276 | verified | — | audit p4 |
| P4a-5 | speed | `movement_x/y` = speed | `person.cpp:221-241` | live `FUN_1401fef8` | verified | единица времени — B10 | audit p4 |
| P4a-6 | render | anchor/crops | `person_render.cpp:20-55` | `formats.md` (не Client.dll) | partial | crops отбрасываются | audit p4 |
| P4a-7 | directions | 8/16-dir tables | `pad.hpp:66-71` | редактор | partial | mirroring не доказан | audit p4 |
| P4a-8 | person scr | `.scr` parse | `person_script.cpp:146-232` | 15/15 `l1_1` | verified (parse) | `level` не проверяется | audit p4 |
| P4a-9 | SEF | persons fields/count | `sef.cpp:195-304` | 261/261; 16 `l1_1` | verified (`l1_1`) | нет corpus-wide count | audit p4 |
| P4a-10 | SEF | `literary_name` `+0x40/44` | не парсится | live | unverified | поле отбрасывается | audit p4 |
| P4a-11 | загрузка | `rt_stay`/`tb_stay` fallback | `person.cpp:162-249` | реальные данные | partial | порядок выбора не доказан | audit p4 |
| P4a-12 | placement | cell centre − anchor | `level_simulation.cpp:60-69` | live `FUN_140222d0` | partial | `-anchor` не доказан | audit p4 |
| P4a-13 | idle | frame index | `person_render.cpp:11-18` | port tests | partial | restart/phase не извлечены | audit p4 |
| P4a-14 | directions | SEF strings → 0..7 | `person.cpp:55-73` | live `FUN_1401fb1c` | verified | — | audit p4 |
| P4a-15 | порядок | statics → persons, без Y-sort | — | — | superseded | заменено A7/D-036 | audit p4; A7 |
| P4a-16 | тень | identity rows/anchors | `level_screen.cpp:392-412` | port tests | partial | состав тени не доказан | audit p4 |
| P4a-17 | ошибки | warn+skip person | `person.cpp:83-192` | только порт | unverified | fallback оригинала | audit p4 |

## P4b — маршруты/скорость/RNG

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P4b-1 | route types | 0..5 mapping | `person_movement.cpp:13-25` | live `FUN_1401f9bc` | partial | неизвестные → stay без warning | audit p4 |
| P4b-2 | MOVED | index advance | `level_simulation.cpp:250-307` | live + A2/D-031 | superseded | закрыто A2 | audit p4; D-031 |
| P4b-3 | MOVED_FLIP | ±1 coin | `level_simulation.cpp:301-405` | live + A1/A2 | superseded | закрыто A1/A2 | audit p4; D-030/D-031 |
| P4b-4 | RANDOM_RADIUS | вокруг spawn | `level_simulation.cpp:256-289` | live `FUN_1401c2a0`; A3/D-032 | superseded | закрыто A3 | audit p4; D-032 |
| P4b-5 | RANDOM | вокруг текущей | `level_simulation.cpp:256-289` | live; A3/D-032 | superseded | закрыто A3; C12 остаётся | audit p4; D-032 |
| P4b-6 | STAY | возврат к spawn | `level_simulation.cpp:260-316` | A4/D-035 | superseded | закрыто A4 | audit p4; D-035 |
| P4b-7 | STAY_ROTATE | поворот на месте | `level_simulation.cpp:278-305` | A4/D-035 | superseded | длительность `SET_DIRECTION` = 500 мс порт | audit p4; D-035 |
| P4b-8 | delay | `<500` без паузы | `level_simulation.cpp:129-164` | live `FUN_1401c378` | partial | fallback 500 мс — порт | audit p4 |
| P4b-9 | delay | `rand(min..max)` | `level_simulation.cpp:53-56` | live `FUN_140061e4` | partial | inclusive/exclusive не пинится | audit p4 |
| P4b-10 | speed | `iso_speed*1000/(go*0.8)` | `level_simulation.cpp:180-182` | live; A5/D-033 | verified | 35 Гц интеграция | audit p4 |
| P4b-11 | speed | изо-формула | `person_movement.cpp:88-94` | live | verified | — | audit p4 |
| P4b-12 | arrival | `<4 px`, snap | `level_simulation.cpp:183-200` | live-замер | partial | extra `distance<=step` | audit p4 |
| P4b-13 | direction | 16-way thresholds | `person_movement.cpp:7-75` | live `FUN_1402085c` | verified | нормализация 10..25 | audit p4 |
| P4b-14 | direction | 16↔8 maps | `person_movement.cpp:77-86` | live | verified | — | audit p4 |
| P4b-15 | RNG | CRT `rand` | `MsvcRandom`; A1/D-030 | live `_rand` | superseded | закрыто A1 | audit p4; D-030 |
| P4b-16 | tick | 60 Гц vs 35 Гц | `level_simulation.cpp:16,143-150` | live `sv_fps=35` | verified (const) | интеграция 1/60 — порт | audit p4; A5 |
| P4b-17 | init | начальная цель/пауза | `level_simulation.cpp:113-140` | только порт | unverified | spawn-поза/тайминг | audit p4 |
| P4b-18 | fail | replan каждый тик | `level_simulation.cpp:194-342` | только порт | unverified | число retry/сдвиг цели | audit p4 |

## P4b2 — проходимость/A*/occupancy

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P4b2-1 | grid | word `relief|sound<<16` | `walk_grid.cpp:17-28` | live `FUN_00426234` | verified | — | audit p4 |
| P4b2-2 | grid | column-major chunks | `lvl.cpp:224-243` | live reconstruction | verified | — | audit p4 |
| P4b2-3 | grid | sub-tile bit | `walk_grid.cpp:21-29` | live + unit | verified | — | audit p4 |
| P4b2-4 | grid | `l1_1` 18028/51520 | `walk_grid.cpp:10-33` | 0/51520 live | verified | — | audit p4 |
| P4b2-5 | grid | runtime 331 клетка | не моделируется | live | missing | двери/скрипты меняют grid | audit p4; A8 |
| P4b2-6 | LOS | Bresenham | `pathfinder.cpp:31-48` | live `FUN_14012d4c` | partial | start-cell occupancy не описана | audit p4 |
| P4b2-7 | A* | step cost 1 (orig 10) | `pathfinder.cpp:84-89` | research | partial | нормализация 1:1 | audit p4 |
| P4b2-8 | A* | heuristic `max+0.375*min`, w=1.2 | `pathfinder.cpp:21-27` | `0.375` RVA | partial | weight derived | audit p4 |
| P4b2-9 | A* | диагонали без ортогоналей | `pathfinder.cpp:16-90` | live | verified | — | audit p4 |
| P4b2-10 | A* | neighbour order/tie-break | `pathfinder.cpp:66-89` | нет evidence | unverified | равные пути | audit p4 |
| P4b2-11 | occupancy | current tile, `u8` | `level_simulation.cpp:344-391` | live `DAT_14088dac` | partial | owner/id семантика | audit p4 |
| P4b2-12 | occupancy | goal exempt | `level_simulation.cpp:372-397` | нет evidence | unverified | цель занята другим NPC | audit p4; D-032/D-034 |
| P4b2-13 | collision | occupancy-only | `level_simulation.cpp:194-216` | live `FUN_14020a18` | verified | — | audit p4 |
| P4b2-14 | jitter | no walk re-check | `level_simulation.cpp:194-216` | port fix | verified (port) | симптом оригинала не наблюдался | audit p4 |
| P4b2-15 | spawn | blocked spawn | `level_simulation.cpp:41-141` | нет evidence | unverified | `Kolbasens` на blocked | audit p4 |
| P4b2-16 | route | blocked first cell `oficer1` | `level_simulation.cpp:294-342` | A2/D-031 | superseded | закрыто A2 | audit p4; D-031 |
| P4b2-17 | RANDOM | 3-failure abandonment | удалено | D-032 | superseded | закрыто A3 | audit p4; D-032 |
| P4b2-18 | deadlock | нет доказательства | — | нет теста | unverified | возможна взаимная блокировка | audit p4 |
| P4b2-19 | tile map | `lround((x-6)/12)` | `level_simulation.cpp:18-23` | только порт | unverified | округление оригинала | audit p4 |
| P4b2-20 | debug | walkable overlay | `level_screen.cpp:457-470` | debug only | verified (debug) | не gameplay | audit p4 |

## P4c — клик → диалог (принят 2026-10-24)

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P4c-1 | hit-test | клик по PAD-кадру NPC | `level_click` | manual gate | verified | — | STATUS P4c |
| P4c-2 | путь | hero spawn/путь/AABB | `Hero`/`HeroController` | manual gate | verified | — | STATUS P4c |
| P4c-3 | диалог | локальное открытие из `scr_dialog` | `DialogOverlay` | real `l1_1` | verified | server `0x12` не реализован | STATUS P4c |
| P4c-4 | interact | 6-осевая формула | AABB | `Client.dll 0x120b2fb8` | unverified | точная формула не извлечена | STATUS P4c, D-040 |
| P4c-5 | визуал | PAD Вертаса | hero-маркер | live | superseded | заменено P4d (wear) | STATUS; D-040/D-041 |

## P4d — hero equipment/render (принят 2026-10-25/11-04)

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P4d-1 | HAD | парсер кадров/колонок/anchor | `parse_wear_had` | real `tb_stay`/`tb_go` | verified | — | STATUS P4d |
| P4d-2 | IAD | блоки/crop/shadow | `parse_iad` | real `dospeh_kamen` | partial | точные anchors/crop не все сверены | STATUS P4d |
| P4d-3 | SEQ | порядок слоёв | `parse_wear_seq` | real | partial | покрытие корпуса не пинится | STATUS P4d |
| P4d-4 | composer | `FUN_0042279C` | `compose_wear_body` | golden FNV slot0 idle | verified | — | STATUS P4d; D-043 |
| P4d-5 | shadow | destination stride HAD | `compose_wear_shadow` | regression + screenshot | verified | — | STATUS P4d |
| P4d-6 | draw | mirrored draw origin | `person_body_draw_x` | live PID 22800 | verified | ±1 px shadow anchor ниже порога | STATUS P4d |
| P4d-7 | anim | hits/die/attack/run | не выбираются | — | missing | `tb_stay`/`tb_go` только | STATUS P4d |
| P4d-8 | outfit | set A/B switch | set A | slot0/slot2 | missing | переключение не доказано | STATUS P4d/P5a |
| P4d-9 | shadow | точные anchors/пиксельный паритет | — | визуальный gate | partial | полная сверка не сделана | STATUS P4d |

## A8 — cell masks (parity accepted 2026-11-04)

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| A8-1 | masks | `BLK_MDSC` → mask CSX | `LevelMask` | `mask_45` 182090 px | verified | — | STATUS A8; D-037 |
| A8-2 | masks | grid из LVL (`flags/mask`) | `Level::MaskCellGrid` | `GoldenLand.exe FUN_00426490` | verified | — | STATUS A8; D-044 |
| A8-3 | masks | строка `person_y/18` + mask id | `draw_rect_masks` | hook `FUN_1202BC54` | verified | — | STATUS A8; a8-mask-parity |
| A8-4 | masks | multi-cell occupancy | одна клетка | — | missing | большие спрайты | STATUS A8 |
| A8-5 | masks | runtime/alt masks | не моделируются | — | missing | двери/скрипты/alt | STATUS A8 |
| A8-6 | masks | маски только персонажи | `LevelScreen` | manual gate | verified | статики не маскируются | STATUS A8 |

## P5a — save → outfit (принят 2026-11-04)

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P5a-1 | save | header+zlib+`BT_HERO` | `decode_save_body` | 28 save, 0/N | verified | 32 MiB/first `BT_HERO`/BOM отложены | STATUS P5a |
| P5a-2 | items | item → TECH → group | `tech_names.sdb` | 18/18 assets slot0/2 | verified | — | STATUS P5a; D-042 |
| P5a-3 | outfit | resolver + apply | `resolve_hero_outfit` | real + manual gate | verified | — | STATUS P5a |
| P5a-4 | outfit | set A/B switch | set A | — | missing | не доказано | STATUS P5a |
| P5a-5 | parser | deferrals | — | — | partial | 32 MiB/first `BT_HERO`/BOM | STATUS P5a |

## P5b1 — read-only inventory UI (in progress)

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P5b1-1 | inventory | 3 контейнера `BT_HERO` | `parse_hero_inventory` | slot0 `8/117/11`, slot2 `9/140/11` | verified | — | plan P5b1; tests |
| P5b1-2 | catalog | ITM/SDB/icons | `ItemCatalog` | 296/296 items | verified | — | research P5b1 |
| P5b1-3 | UI | 167 объектов/фильтры/панель | `InventoryOverlay` | research + screenshot tests; manual gate PASSED 2026-11-05 | verified | — | research P5b1; D-045/D-046/D-047 |
| P5b1-4 | actions | use/drop/equip | не реализовано | — | missing | вне read-only scope | spec P5b1 |

## Не реализовано (P5b2+)

| ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник |
|---|---|---|---|---|---|---|---|
| P5b2-1 | inventory | paper-doll overlays | не реализовано | отложено P5b1 | missing | ITM `puppet_name` + layer masks | STATUS P5b1 |
| P5b2-2 | P5 | бой/магия/предметы | — | — | missing | нужен RE каждой системы | PROJECT |
| P6 | мир | карта/квесты/дневник/торговля | — | — | missing | — | PROJECT |
| P7 | UI | все экраны/тач | — | — | missing | — | PROJECT |
| P8 | аудио | замена `Dare.dll` | — | — | missing | — | PROJECT |
| P9 | Android | APK/жизненный цикл/тач | — | — | missing | — | PROJECT |
| P10 | паритет | кампания/баланс | — | — | missing | — | PROJECT |

## Итог

Всего строк: **157**. По статусам: `verified` 45, `partial` 60,
`unverified` 22, `missing` 15, `contradicted` 2, `superseded` 13.

Native dispatch infrastructure (Phase 1a): `NativeRegistry` 65/65 +
`GameScriptHost`; 5 supported, 3 vm_internal, 57 unsupported.

`contradicted`: `P1-3` (ADSC) и `P2-9` (60 Гц vs `sv_fps=35`).
`missing` включает новые системы P5b2+ и отсутствующие P4d/P5a-состояния.

Блокеры паритета, требующие отдельных spec/plan в порядке приоритета:

1. `P3-9`: 57 нативов без подсистем (persons/inventory/quests/time/world);
   VM-часть закрыта Phase 1a.
2. `P2-15`: save payload/write/load.
3. `P2-9`/`P4b-16`: 35 Гц vs 60 Гц.
4. `P1-3`: ADSC `number/name`.
5. `P4a-6`/`P4a-12`/`P4a-13`: PAD crops/anchors/animation phase.
6. `P4c-4`: 6-осевой `CanInteract`.
7. `P4b2-5`/`A8-5`: runtime grid/alt masks.
8. `P5b2`+: новые системы.
