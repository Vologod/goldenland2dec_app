# Design: P4a — NPCs on the level (persons, PAD, standing animation)

Статус: **утверждён**.
Дата: 2026-10-21.
Базируется на: P1 (LevelScreen), P3c (dialogs), `docs/research/`.

## 1. Цель

Показать реальных NPC на уровне `l1_1`: прочитать `persons:` из SEF, найти person-скрипт,
`res_name`, спрайтовый каталог в `persons.pak`, разобрать `.pad` и отрисовать
стоячую анимацию (`rt_stay`) в нужном направлении. Без движения/AI/диалогов по клику.

P4a — первый шаг P4; движение и взаимодействие — P4b/P4c.

## 2. Границы

Входит:

- `gl_assets`: PAD-парсер (анимации/кадры/направления), парсер person `.scr` (ключи,
  блоки `weapon/skills/magic/ai/trade_panel`), расширение SEF-парсера секцией `persons`.
- `gl_game`: `PersonDef`/`PersonInstance` (позиция в тайлах, направление, route/dialog/inv,
  res_name), загрузчик спрайтов.
- `gl_app`/`glgame`: отрисовка NPC на уровне (rt_stay, 8 направлений; тень — опционально).
- Тесты и screenshot.

Не входит:

- Движение/поиск пути, AI/маршруты, встречи (P4b).
- Клик → диалог, торговля, бой (P4c/P5).
- Глобальная карта, анимации `tb_go/rt_go` (P4b).

## 3. Данные (подтверждено)

- `levels/single/l1_1/l1_1.sef` → секция `persons:` — **16** записей (всего в файле
  41 блок `name:`, но остальные 25 принадлежат `points_entrance`, `cell_groups` и
  `triggers`); поля: `name`, `position X Y` (тайлы), `literary_name`, `direction`,
  `route_type`, `route`, `radius`, `delay_min/max`, `tribe`, `scr_dialog`, `scr_inv`.
- Person-скрипт: `scripts/persons/<lower(name)>.scr` (например,
  `l1_1.p387_poison_plotnik.scr`).
- `.scr`: `res_name: "L0.P1131_Inhabitans03"`, статы, `skills: {}`, `magic: {}`,
  `ai: {}`, `trade_panel: {}`, `weapon: {}`.
- Спрайты: `persons/<lower(res_name)>/`:
  - `<res>.pad` — метаданные анимаций;
  - `animation/rt_stay.csx`, `rt_fun`, `tb_stay`, `tb_fun`, `tb_go`, `rt_go`, `cast`,
    `suffer`, `die`, `ss_attack`, `hits0-3`;
  - `shadows/<same>.csx`.
- PAD: magic `"PAD "`, `u32 dataSize`, `u32 animationMasks`, затем анимации:
  `u32 type`, `i32 size`, `i32 delay`, `framesPerRow`, `frameWidth`, `frameHeight`,
  `anchorX`, `anchorY`, `float movementX/Y`, `u32 cropsTableSize`, crops (`i16` x/y/w/h),
  shadowFrame (`i32` width/height/anchorX/anchorY), `u32 shadowTableSize`, shadowCrops.
  `dataSize` — абсолютный offset конца области анимаций (не длина payload): сразу после
  него может идти trailer-таблица `u32 count` + `count*(i32,i32)`. Размер строки:
  `framesPerRow`; число строк = `tableSize / 8 / framesPerRow` (таблица обязана делиться
  без остатка). Для `_go` — 16 направлений, иначе 8; зеркалирование направлений — как в
  `PersonAnimation` (см. `PAD_Data`), тени не зеркалируются. `delay` сохраняется как есть;
  при `delay <= 0` P4a Task 4 использует 1/skip.
- Позиция на экране: `x * 12`, `y * 9` (тайлы 12×9), плюс anchor из PAD.
- Направление SEF (`DOWN_RIGHT` и т.п.) → индекс 0..7 (`up, up_left, left, down_left,
  down, down_right, right, up_right`).
- Кадр: `(simulation_time_ms / delay) % framesPerRow`; текстура — одна на анимацию,
  UV = кадр×направление; при mirror — swap U.

## 4. Архитектура

```
assets (PAD, person .scr, SEF persons) → game (PersonLoader/PersonInstance) → render/app
```

- `gl_assets`: `pad.hpp/cpp`, `person_script.hpp/cpp`; `sef.hpp/cpp` расширяется
  `persons`.
- `gl_game`: `PersonDef { res_name; stats... }`, `PersonInstance { name, x, y, direction,
  route_type, route, dialog_path, inv_path, def, PAD, textures }`.
- `gl_app`: `LevelScreen` загружает persons и рисует их между фоном и статикой/анимациями
  (порядок: фон → NPC/статика/анимации по Y? в P4a — фон → статика → анимации → NPC,
  либо NPC по Y; зафиксировать в спеке: NPC после фона, до/после объектов по Y-сортировке —
  P4a рисует NPC после статики/анимаций для простоты, сортировка в P4b).
- `glgame`: без отдельных изменений, кроме загрузки через LevelScreen.

## 5. Тестирование

Unit:

- PAD: синтетика (одна анимация, crops, shadow), битые размеры; реальные `.pad` из
  `persons.pak` — все парсятся (0 ошибок), счётчик > 0.
- Person `.scr`: res_name, статы, блоки; битые строки; реальные 15 уникальных
  person-скриптов `l1_1` (16 записей, один `name` повторяется).
- SEF persons: синтетика + `l1_1` = 16, поля корректны; битые секции.

Integration:

- Для каждого из 16 NPC `l1_1`: `.scr` найден, `res_name` не пуст, `.pad` и
  `animation/rt_stay.csx` найдены, PAD содержит `rt_stay`.
- Screenshot `l1_1`: 1280×720, NPC-пиксели присутствуют (диалог с фоном без NPC),
  число загруженных NPC = 16.
- Отсутствующий спрайт/скрипт → warning, NPC пропускается.

## 6. Критерии готовности

1. 16 NPC `l1_1` загружаются и рисуются в правильных позициях/направлениях.
2. Все реальные `.pad` в `persons.pak` парсятся; ошибок 0.
3. Screenshot с NPC отличается от кадра без NPC.
4. `gl_assets`/`gl_game` SDL-free; документы обновлены.

## 7. Риски

- PAD-направления/зеркалирование: сверить с `PersonAnimation` (UV/якоря).
- Тень: выравнивание по PAD `shadowFrame`; в P4a можно отключить/включить флагом.
- Размер `persons.pak` (327 МБ): читать только нужные файлы через VFS.
- Отсутствующие у части NPC `rt_stay`/спрайтов — warning/skip.

## 8. Отложено

Движение/маршруты/поиск пути, AI, клик → диалог, торговля, бой, глобальная карта.
