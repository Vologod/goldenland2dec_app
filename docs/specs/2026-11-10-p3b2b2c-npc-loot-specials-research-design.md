# Phase 1b2b2c — NPC loot specials research (design)

Дата: 2026-11-10. Статус: approved (пользователь, 2026-11-10).

## 1. Цель

Определить, можно ли точно воспроизвести specials NPC-лута
(`FUN_14047284` + item manager `DAT_14088df4`): выбор модификаторов,
RNG R4–R10, применяемые поля, источник базы модификаторов. Research-only;
production-код не меняется.

## 2. Контекст

- `Server.dll` v1.3; research `p3b2b2-npc-loot-server.md` §8/§15:
  `roll_special_count` (R4/R5) для `chunk==1`, `flags&4`,
  `item+0x78==0`; `FUN_14047284` (R6–R10) выбирает модификаторы.
- `DAT_14088df4` — item manager; vtable `+0x14` parse special,
  `+4/+8/+0x1c` modifier defs (`natives.c:48810`,
  `decompiled.c:48337-48573`, init `63169`).
- `InventoryItem`/NPC loot хранят `mod_count` (`+0x78`), flags, durability
  (`+0xB4`); modifier-цены не декодированы (D-058).
- Реальные `.inv`: активных `special` нет, но specials-код может
  срабатывать по флагам предметов; exact effect на стату/цену неизвестен.

## 3. Вопросы research

1. Источник modifier DB: `.itm`, `sdb/items/itemdescparts.sdb`,
   `magic.pak`, иное; загрузка `DAT_14088df4` (init `63169`) и vtable.
2. `FUN_14047284`: пулы/группы (`+0x68/+0x78/+0x88`,
   `+0xc/+0x1c/+0x2c`), веса/классы/типы, R6–R10 порядок/аргументы,
   `ROUND`/trunc, применяемые поля записи и flags.
3. `roll_special_count` R4/R5: точные условия и значения.
4. Полный RNG-порядок special-path и влияние на последующие записи.
5. Реальные модификаторы в данных: сколько, какие поля; как это влияет
   на цену/торговлю (D-058 guard).
6. Oracle: live-память/дифференциальный тест; если недостижимо —
   verdict `blocked` с недостающими фактами.
7. Verdict `implementable`/`blocked`; если реализуемо — acceptance для
   следующей implementation-фазы.

## 4. Non-goals

- Production-код, UI, торговля, save.
- Полный реверс магии/эффектов вне item modifier-полей.

## 5. Deliverable

`docs/research/p3b2b2c-npc-loot-specials.md`:

- DB source/layout + RVA/file:line;
- псевдокод `FUN_14047284`/`roll_special_count`;
- полный RNG-список special-path;
- поля записи/цены/flags;
- real-data проверка;
- verdict и handoff.

## 6. Критерии приёмки

1. Research: DB source, алгоритм, RNG, поля, real-data, verdict.
2. `STATUS`/`PROJECT` обновлены; при blocked — Open Questions.
3. UTF-8 без BOM/NUL.

## 7. Риски

- Modifier DB может быть зашита в код/бинарь без файлового формата.
- Live-oracle может быть недостижим; тогда specials остаются blocked,
   а NPC-лут — `partial` (D-055).
