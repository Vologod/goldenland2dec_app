# P5b1: inventory UI layout, filters, item presentation

Дата: 2026-11-05. Статус: **research DONE** (layout/filter/item catalog);
часть исходной геометрии — **BLOCKED** (см. §10). Игровой код не менялся;
в этом коммите — только этот документ.

Задача: `docs/plans/2026-11-04-p5b1-inventory-ui-research.md`, Task 1.
Источник истины: `Client.dll` SHA-256
`FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`
(image base `0x12000000`; RVA = VA − `0x12000000`).
Вспомогательные: `Server.dll` SHA-256
`418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`
(base `0x14000000`), `GoldenLand.exe` SHA-256
`7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF`
(base `0x00400000`).

## 0. Главные выводы (важно для spec/plan)

1. `scripts/ui/inv_gui.scr` — это экран **персонажа/инвентаря**, а не
   отдельная «сетка предметов». Инвентарь в нём показан тремя зонами:
   - **container 0** (расходники/свитки, 8/9 записей) → нижний ряд
     **hero hot slots** (`OBJECT_ID 99`, `10,618,266x40`), 9 используемых
     позиций, иконка **27×27**, `y=624`, x по маске быстрого слота;
   - **container 1** (основной инвентарь, 117/140 записей) → **нижняя
     полоса** (`OBJECT_ID 1`, `138,696,730x66`), **12 слотов на экран**,
     иконка **55×55**, `y=703`, прокрутка по 1 (стрелки) / 3 (колесо);
   - **container 2** (экипировка, 11/11 записей) → **paper-doll puppet**
     (`OBJECT_ID 2`, `8,296,270x320`), фиксированные слоты по equip-маске,
     иконка **55×55**.
2. Таблица из 27 строк (объекты 100…126, 3 колонки × 9 строк,
   x=289/536/783) — это **skills/secondary** экран персонажа
   (`FUN_120661cc`, массивы x/y/ids), а **не** список предметов.
   В P5b1 её нельзя рисовать как инвентарь; это P5c/character UI.
3. Правая панель «имя/описание предмета» из spec §6 в `inv_gui.scr`
   **не найдена**. Оригинал показывает предмет через всплывающий
   tooltip движка (`DAT_12110ef0`), а не через фиксированную правую
   панель. Точная геометрия tooltip — **BLOCKED** (§10).
4. Сетка инвентаря — это не 27 строк, а **12 горизонтальных слотов**
   с прокруткой; фильтры 1..7 применяются к container 1 и задают
   **ровно одну** категорию (`filter_mask` = один бит, кроме filter7=all).
5. Все 296 записей slot0+slot2 разрешаются в ITM + `lit_names` +
   `descriptions` + `items/res` иконку: **0 расхождений** (§8).

## 1. Источники и инструменты

| Что | Где |
|---|---|
| UI-скрипт | `scripts/ui/inv_gui.scr` (167 OBJECT блоков) |
| Фоны | `engineres/newinv/inventory_secondary.bmp`, `inventory_skills.bmp` (1024×768, 8bpp indexed) |
| Кнопки | `filter1..7`, `drop`, `exit`/`exitl`, `inv_left`, `inv_right`, `lm1..7`, `lp1..7`, `cm1..27`, `cp1..27`, `weapon_slot1` |
| Предметы | `items/data/<id>.itm` (1848 файлов), `items/res/<icon_name>` (1938 файлов), `sdb/items/lit_names.sdb`, `sdb/items/descriptions.sdb` (по 1848 записей) |
| Сохранения | `Saves/slot0.sav`, `Saves/slot2.sav` |
| Реверс | Ghidra headless, `%TEMP%\ghidra_glclient`; decompile/scan scripts: `DecompileAt.java`, `RefsTo.java`, `ScanDisp.java`, `DumpData.java` |

Client.dll: VA → RVA = VA − `0x12000000`. Server.dll: VA → RVA = VA −
`0x14000000`. GoldenLand.exe: VA → RVA = VA − `0x00400000`.

## 2. Полная таблица объектов `inv_gui.scr`

167 объектов. `base/lit/press` — `IMAGE_BASE` / `IMAGE_LIGHTED` /
`IMAGE_PRESSED` (пустая строка опущена). Комментарии — из скрипта
(переведены/сохранены как есть, cp1251 → UTF-8).

| id | type | left | top | w | h | base | lit | press | comment |
|---|---|---|---|---|---|---|---|---|---|
| 1 | GUI_DD_CONTAINER | 138 | 696 | 730 | 66 |  |  |  | container 1 - hero inventory |
| 2 | GUI_DD_CONTAINER | 8 | 296 | 270 | 320 |  |  |  | container 2 - puppet |
| 3 | GUI_SIMPLE_BUTTON | 110 | 702 | 27 | 55 |  |  | engineres\newinv\inv_left | button 3 - left scroll |
| 4 | GUI_SIMPLE_BUTTON | 870 | 703 | 26 | 54 |  |  | engineres\newinv\inv_right | button 4 - right scroll |
| 5 | GUI_SIMPLE_BUTTON | 280 | 4 | 368 | 40 |  |  |  | button 5 - skills |
| 6 | GUI_SIMPLE_BUTTON | 654 | 4 | 360 | 40 |  |  |  | button 6 - secondary |
| 7 | GUI_SIMPLE_BUTTON | 920 | 694 | 75 | 68 |  | engineres\newinv\exitl | engineres\newinv\exit | button 7 - exit |
| 8 | GUI_DD_CONTAINER | 19 | 690 | 81 | 75 |  | engineres\newinv\drop |  | button 8 - drop (!!!container???) |
| 9 | GUI_CHECK_BUTTON | 201 | 672 | 56 | 23 |  |  | engineres\newinv\filter1 | button 9 |
| 10 | GUI_CHECK_BUTTON | 290 | 673 | 56 | 23 |  |  | engineres\newinv\filter2 | button 10 |
| 11 | GUI_CHECK_BUTTON | 383 | 673 | 56 | 23 |  |  | engineres\newinv\filter3 | button 11 |
| 12 | GUI_CHECK_BUTTON | 474 | 673 | 56 | 23 |  |  | engineres\newinv\filter4 | button 12 |
| 13 | GUI_CHECK_BUTTON | 565 | 673 | 56 | 23 |  |  | engineres\newinv\filter5 | button 13 |
| 14 | GUI_CHECK_BUTTON | 658 | 673 | 56 | 23 |  |  | engineres\newinv\filter6 | button 14 |
| 15 | GUI_CHECK_BUTTON | 745 | 673 | 56 | 23 |  |  | engineres\newinv\filter7 | button 15 |
| 16 | GUI_SIMPLE_BUTTON | 168 | 59 | 26 | 26 |  |  | engineres\newinv\lm1 | button 16 |
| 17 | GUI_SIMPLE_BUTTON | 168 | 87 | 26 | 26 |  |  | engineres\newinv\lm2 | button 17 |
| 18 | GUI_SIMPLE_BUTTON | 168 | 115 | 26 | 26 |  |  | engineres\newinv\lm3 | button 18 |
| 19 | GUI_SIMPLE_BUTTON | 168 | 142 | 26 | 26 |  |  | engineres\newinv\lm4 | button 19 |
| 20 | GUI_SIMPLE_BUTTON | 168 | 170 | 26 | 26 |  |  | engineres\newinv\lm5 | button 20 |
| 21 | GUI_SIMPLE_BUTTON | 168 | 198 | 26 | 26 |  |  | engineres\newinv\lm6 | button 21 |
| 22 | GUI_SIMPLE_BUTTON | 168 | 225 | 26 | 26 |  |  | engineres\newinv\lm7 | button 22 |
| 23 | GUI_SIMPLE_BUTTON | 231 | 59 | 26 | 26 |  |  | engineres\newinv\lp1 | button 23 |
| 24 | GUI_SIMPLE_BUTTON | 231 | 87 | 26 | 26 |  |  | engineres\newinv\lp2 | button 24 |
| 25 | GUI_SIMPLE_BUTTON | 231 | 115 | 26 | 26 |  |  | engineres\newinv\lp3 | button 25 |
| 26 | GUI_SIMPLE_BUTTON | 231 | 142 | 26 | 26 |  |  | engineres\newinv\lp4 | button 26 |
| 27 | GUI_SIMPLE_BUTTON | 231 | 170 | 26 | 26 |  |  | engineres\newinv\lp5 | button 27 |
| 28 | GUI_SIMPLE_BUTTON | 231 | 198 | 26 | 26 |  |  | engineres\newinv\lp6 | button 28 |
| 29 | GUI_SIMPLE_BUTTON | 231 | 225 | 26 | 26 |  |  | engineres\newinv\lp7 | button 29 |
| 30 | GUI_SIMPLE_BUTTON | 434 | 124 | 24 | 24 |  |  | engineres\newinv\cm1 | "-" |
| 31 | GUI_SIMPLE_BUTTON | 492 | 124 | 24 | 24 |  |  | engineres\newinv\cp1 | "+" |
| 32 | GUI_SIMPLE_BUTTON | 434 | 156 | 24 | 24 |  |  | engineres\newinv\cm2 | "-" |
| 33 | GUI_SIMPLE_BUTTON | 492 | 156 | 24 | 24 |  |  | engineres\newinv\cp2 | "+" |
| 34 | GUI_SIMPLE_BUTTON | 434 | 189 | 24 | 24 |  |  | engineres\newinv\cm3 | "-" |
| 35 | GUI_SIMPLE_BUTTON | 492 | 189 | 24 | 24 |  |  | engineres\newinv\cp3 | "+" |
| 36 | GUI_SIMPLE_BUTTON | 681 | 124 | 24 | 24 |  |  | engineres\newinv\cm4 | "-" |
| 37 | GUI_SIMPLE_BUTTON | 739 | 124 | 24 | 24 |  |  | engineres\newinv\cp4 | "+" |
| 38 | GUI_SIMPLE_BUTTON | 681 | 156 | 24 | 24 |  |  | engineres\newinv\cm5 | "-" |
| 39 | GUI_SIMPLE_BUTTON | 739 | 156 | 24 | 24 |  |  | engineres\newinv\cp5 | "+" |
| 40 | GUI_SIMPLE_BUTTON | 681 | 189 | 24 | 24 |  |  | engineres\newinv\cm6 | "-" |
| 41 | GUI_SIMPLE_BUTTON | 739 | 189 | 24 | 24 |  |  | engineres\newinv\cp6 | "+" |
| 42 | GUI_SIMPLE_BUTTON | 926 | 124 | 24 | 24 |  |  | engineres\newinv\cm7 | "-" |
| 43 | GUI_SIMPLE_BUTTON | 984 | 124 | 24 | 24 |  |  | engineres\newinv\cp7 | "+" |
| 44 | GUI_SIMPLE_BUTTON | 926 | 156 | 24 | 24 |  |  | engineres\newinv\cm8 | "-" |
| 45 | GUI_SIMPLE_BUTTON | 984 | 156 | 24 | 24 |  |  | engineres\newinv\cp8 | "+" |
| 46 | GUI_SIMPLE_BUTTON | 926 | 189 | 24 | 24 |  |  | engineres\newinv\cm9 | "-" |
| 47 | GUI_SIMPLE_BUTTON | 984 | 189 | 24 | 24 |  |  | engineres\newinv\cp9 | "+" |
| 48 | GUI_SIMPLE_BUTTON | 434 | 310 | 24 | 24 |  |  | engineres\newinv\cm10 | "-" |
| 49 | GUI_SIMPLE_BUTTON | 492 | 310 | 24 | 24 |  |  | engineres\newinv\cp10 | "+" |
| 50 | GUI_SIMPLE_BUTTON | 434 | 343 | 24 | 24 |  |  | engineres\newinv\cm11 | "-" |
| 51 | GUI_SIMPLE_BUTTON | 492 | 343 | 24 | 24 |  |  | engineres\newinv\cp11 | "+" |
| 52 | GUI_SIMPLE_BUTTON | 434 | 375 | 24 | 24 |  |  | engineres\newinv\cm12 | "-" |
| 53 | GUI_SIMPLE_BUTTON | 492 | 375 | 24 | 24 |  |  | engineres\newinv\cp12 | "+" |
| 54 | GUI_SIMPLE_BUTTON | 681 | 310 | 24 | 24 |  |  | engineres\newinv\cm13 | "-" |
| 55 | GUI_SIMPLE_BUTTON | 739 | 310 | 24 | 24 |  |  | engineres\newinv\cp13 | "+" |
| 56 | GUI_SIMPLE_BUTTON | 681 | 343 | 24 | 24 |  |  | engineres\newinv\cm14 | "-" |
| 57 | GUI_SIMPLE_BUTTON | 739 | 343 | 24 | 24 |  |  | engineres\newinv\cp14 | "+" |
| 58 | GUI_SIMPLE_BUTTON | 681 | 375 | 24 | 24 |  |  | engineres\newinv\cm15 | "-" |
| 59 | GUI_SIMPLE_BUTTON | 739 | 375 | 24 | 24 |  |  | engineres\newinv\cp15 | "+" |
| 60 | GUI_SIMPLE_BUTTON | 926 | 310 | 24 | 24 |  |  | engineres\newinv\cm16 | "-" |
| 61 | GUI_SIMPLE_BUTTON | 984 | 310 | 24 | 24 |  |  | engineres\newinv\cp16 | "+" |
| 62 | GUI_SIMPLE_BUTTON | 926 | 343 | 24 | 24 |  |  | engineres\newinv\cm17 | "-" |
| 63 | GUI_SIMPLE_BUTTON | 984 | 343 | 24 | 24 |  |  | engineres\newinv\cp17 | "+" |
| 64 | GUI_SIMPLE_BUTTON | 926 | 375 | 24 | 24 |  |  | engineres\newinv\cm18 | "-" |
| 65 | GUI_SIMPLE_BUTTON | 984 | 375 | 24 | 24 |  |  | engineres\newinv\cp18 | "+" |
| 66 | GUI_SIMPLE_BUTTON | 434 | 495 | 24 | 24 |  |  | engineres\newinv\cm19 | "-" |
| 67 | GUI_SIMPLE_BUTTON | 492 | 495 | 24 | 24 |  |  | engineres\newinv\cp19 | "+" |
| 68 | GUI_SIMPLE_BUTTON | 434 | 527 | 24 | 24 |  |  | engineres\newinv\cm20 | "-" |
| 69 | GUI_SIMPLE_BUTTON | 492 | 527 | 24 | 24 |  |  | engineres\newinv\cp20 | "+" |
| 70 | GUI_SIMPLE_BUTTON | 434 | 559 | 24 | 24 |  |  | engineres\newinv\cm21 | "-" |
| 71 | GUI_SIMPLE_BUTTON | 492 | 559 | 24 | 24 |  |  | engineres\newinv\cp21 | "+" |
| 72 | GUI_SIMPLE_BUTTON | 681 | 495 | 24 | 24 |  |  | engineres\newinv\cm22 | "-" |
| 73 | GUI_SIMPLE_BUTTON | 739 | 495 | 24 | 24 |  |  | engineres\newinv\cp22 | "+" |
| 74 | GUI_SIMPLE_BUTTON | 681 | 527 | 24 | 24 |  |  | engineres\newinv\cm23 | "-" |
| 75 | GUI_SIMPLE_BUTTON | 739 | 527 | 24 | 24 |  |  | engineres\newinv\cp23 | "+" |
| 76 | GUI_SIMPLE_BUTTON | 681 | 559 | 24 | 24 |  |  | engineres\newinv\cm24 | "-" |
| 77 | GUI_SIMPLE_BUTTON | 739 | 559 | 24 | 24 |  |  | engineres\newinv\cp24 | "+" |
| 78 | GUI_SIMPLE_BUTTON | 926 | 495 | 24 | 24 |  |  | engineres\newinv\cm25 | "-" |
| 79 | GUI_SIMPLE_BUTTON | 984 | 495 | 24 | 24 |  |  | engineres\newinv\cp25 | "+" |
| 80 | GUI_SIMPLE_BUTTON | 926 | 527 | 24 | 24 |  |  | engineres\newinv\cm26 | "-" |
| 81 | GUI_SIMPLE_BUTTON | 984 | 527 | 24 | 24 |  |  | engineres\newinv\cp26 | "+" |
| 82 | GUI_SIMPLE_BUTTON | 926 | 559 | 24 | 24 |  |  | engineres\newinv\cm27 | "-" |
| 83 | GUI_SIMPLE_BUTTON | 984 | 559 | 24 | 24 |  |  | engineres\newinv\cp27 | "+" |
| 84 | GUI_SIMPLE_BUTTON | 79 | 507 | 10 | 20 |  |  |  | "1" wear weapon slot |
| 85 | GUI_SIMPLE_BUTTON | 201 | 464 | 10 | 20 |  |  |  | "2" wear weapon slot |
| 90 | GUI_SIMPLE_BUTTON | 20 | 60 | 130 | 23 |  |  |  | primaries |
| 91 | GUI_SIMPLE_BUTTON | 20 | 88 | 130 | 23 |  |  |  |  |
| 92 | GUI_SIMPLE_BUTTON | 20 | 117 | 130 | 23 |  |  |  |  |
| 93 | GUI_SIMPLE_BUTTON | 20 | 144 | 130 | 23 |  |  |  |  |
| 94 | GUI_SIMPLE_BUTTON | 20 | 172 | 130 | 23 |  |  |  |  |
| 95 | GUI_SIMPLE_BUTTON | 20 | 199 | 130 | 23 |  |  |  |  |
| 96 | GUI_SIMPLE_BUTTON | 20 | 226 | 130 | 23 |  |  |  |  |
| 97 | GUI_SIMPLE_BUTTON | 20 | 262 | 181 | 23 |  |  |  |  |
| 98 | GUI_SIMPLE_BUTTON | 216 | 466 | 55 | 60 |  |  |  |  |
| 99 | GUI_DD_CONTAINER | 10 | 618 | 266 | 40 |  |  |  | hero hot slots |
| 100 | GUI_SIMPLE_BUTTON | 289 | 126 | 140 | 20 |  |  |  | skills |
| 101 | GUI_SIMPLE_BUTTON | 289 | 159 | 140 | 20 |  |  |  |  |
| 102 | GUI_SIMPLE_BUTTON | 289 | 190 | 140 | 20 |  |  |  |  |
| 103 | GUI_SIMPLE_BUTTON | 536 | 126 | 140 | 20 |  |  |  |  |
| 104 | GUI_SIMPLE_BUTTON | 536 | 159 | 140 | 20 |  |  |  |  |
| 105 | GUI_SIMPLE_BUTTON | 536 | 190 | 140 | 20 |  |  |  |  |
| 106 | GUI_SIMPLE_BUTTON | 783 | 126 | 140 | 20 |  |  |  |  |
| 107 | GUI_SIMPLE_BUTTON | 783 | 159 | 140 | 20 |  |  |  |  |
| 108 | GUI_SIMPLE_BUTTON | 783 | 190 | 140 | 20 |  |  |  |  |
| 109 | GUI_SIMPLE_BUTTON | 289 | 312 | 140 | 20 |  |  |  |  |
| 110 | GUI_SIMPLE_BUTTON | 289 | 345 | 140 | 20 |  |  |  |  |
| 111 | GUI_SIMPLE_BUTTON | 289 | 376 | 140 | 20 |  |  |  |  |
| 112 | GUI_SIMPLE_BUTTON | 536 | 312 | 140 | 20 |  |  |  |  |
| 113 | GUI_SIMPLE_BUTTON | 536 | 345 | 140 | 20 |  |  |  |  |
| 114 | GUI_SIMPLE_BUTTON | 536 | 376 | 140 | 20 |  |  |  |  |
| 115 | GUI_SIMPLE_BUTTON | 783 | 312 | 140 | 20 |  |  |  |  |
| 116 | GUI_SIMPLE_BUTTON | 783 | 345 | 140 | 20 |  |  |  |  |
| 117 | GUI_SIMPLE_BUTTON | 783 | 376 | 140 | 20 |  |  |  |  |
| 118 | GUI_SIMPLE_BUTTON | 289 | 496 | 140 | 20 |  |  |  |  |
| 119 | GUI_SIMPLE_BUTTON | 289 | 529 | 140 | 20 |  |  |  |  |
| 120 | GUI_SIMPLE_BUTTON | 289 | 561 | 140 | 20 |  |  |  |  |
| 121 | GUI_SIMPLE_BUTTON | 536 | 496 | 140 | 20 |  |  |  |  |
| 122 | GUI_SIMPLE_BUTTON | 536 | 529 | 140 | 20 |  |  |  |  |
| 123 | GUI_SIMPLE_BUTTON | 536 | 561 | 140 | 20 |  |  |  |  |
| 124 | GUI_SIMPLE_BUTTON | 783 | 496 | 140 | 20 |  |  |  |  |
| 125 | GUI_SIMPLE_BUTTON | 783 | 529 | 140 | 20 |  |  |  |  |
| 126 | GUI_SIMPLE_BUTTON | 783 | 561 | 140 | 20 |  |  |  |  |
| 127 | GUI_SIMPLE_BUTTON | 515 | 624 | 210 | 27 |  |  |  |  |
| 160 | GUI_SIMPLE_BUTTON | 296 | 102 | 272 | 24 |  |  |  | secondary characteristics |
| 161 | GUI_SIMPLE_BUTTON | 296 | 134 | 155 | 23 |  |  |  |  |
| 162 | GUI_SIMPLE_BUTTON | 296 | 163 | 272 | 24 |  |  |  |  |
| 163 | GUI_SIMPLE_BUTTON | 296 | 214 | 164 | 20 |  |  |  |  |
| 164 | GUI_SIMPLE_BUTTON | 296 | 240 | 164 | 20 |  |  |  |  |
| 165 | GUI_SIMPLE_BUTTON | 296 | 267 | 254 | 20 |  |  |  |  |
| 166 | GUI_SIMPLE_BUTTON | 296 | 349 | 254 | 20 |  |  |  |  |
| 167 | GUI_SIMPLE_BUTTON | 296 | 376 | 254 | 20 |  |  |  |  |
| 168 | GUI_SIMPLE_BUTTON | 296 | 403 | 254 | 20 |  |  |  |  |
| 169 | GUI_SIMPLE_BUTTON | 296 | 431 | 254 | 20 |  |  |  |  |
| 170 | GUI_SIMPLE_BUTTON | 296 | 458 | 254 | 20 |  |  |  |  |
| 171 | GUI_SIMPLE_BUTTON | 296 | 485 | 254 | 20 |  |  |  |  |
| 172 | GUI_SIMPLE_BUTTON | 296 | 512 | 254 | 20 |  |  |  |  |
| 173 | GUI_SIMPLE_BUTTON | 296 | 539 | 254 | 20 |  |  |  |  |
| 174 | GUI_SIMPLE_BUTTON | 296 | 566 | 254 | 20 |  |  |  |  |
| 175 | GUI_SIMPLE_BUTTON | 294 | 600 | 170 | 21 |  |  |  |  |
| 176 | GUI_SIMPLE_BUTTON | 666 | 105 | 256 | 20 |  |  |  | right side |
| 177 | GUI_SIMPLE_BUTTON | 666 | 138 | 256 | 20 |  |  |  |  |
| 178 | GUI_SIMPLE_BUTTON | 666 | 169 | 256 | 20 |  |  |  |  |
| 179 | GUI_SIMPLE_BUTTON | 666 | 222 | 256 | 20 |  |  |  |  |
| 180 | GUI_SIMPLE_BUTTON | 666 | 253 | 256 | 20 |  |  |  |  |
| 181 | GUI_SIMPLE_BUTTON | 666 | 283 | 256 | 20 |  |  |  |  |
| 182 | GUI_SIMPLE_BUTTON | 666 | 314 | 256 | 20 |  |  |  |  |
| 183 | GUI_SIMPLE_BUTTON | 666 | 344 | 256 | 20 |  |  |  |  |
| 184 | GUI_SIMPLE_BUTTON | 666 | 375 | 256 | 20 |  |  |  |  |
| 185 | GUI_SIMPLE_BUTTON | 666 | 431 | 256 | 20 |  |  |  |  |
| 186 | GUI_SIMPLE_BUTTON | 666 | 458 | 256 | 20 |  |  |  |  |
| 187 | GUI_SIMPLE_BUTTON | 666 | 484 | 256 | 20 |  |  |  |  |
| 188 | GUI_SIMPLE_BUTTON | 666 | 572 | 93 | 20 |  |  |  |  |
| 189 | GUI_SIMPLE_BUTTON | 666 | 597 | 93 | 20 |  |  |  |  |
| 190 | GUI_SIMPLE_BUTTON | 666 | 624 | 93 | 20 |  |  |  |  |
| 191 | GUI_SIMPLE_BUTTON | 844 | 572 | 93 | 20 |  |  |  |  |
| 192 | GUI_SIMPLE_BUTTON | 844 | 597 | 93 | 20 |  |  |  |  |
| 193 | GUI_SIMPLE_BUTTON | 844 | 624 | 93 | 20 |  |  |  |  |
| 194 | GUI_SIMPLE_BUTTON | 348 | 635 | 20 | 20 |  |  |  | 0 |
| 195 | GUI_SIMPLE_BUTTON | 372 | 635 | 20 | 20 |  |  |  | 1 |
| 196 | GUI_SIMPLE_BUTTON | 396 | 635 | 20 | 20 |  |  |  | 2 |
| 197 | GUI_SIMPLE_BUTTON | 420 | 635 | 20 | 20 |  |  |  | 3 |
| 198 | GUI_SIMPLE_BUTTON | 444 | 635 | 20 | 20 |  |  |  | 4 |
| 199 | GUI_SIMPLE_BUTTON | 468 | 635 | 20 | 20 |  |  |  | 5 |
| 200 | GUI_SIMPLE_BUTTON | 492 | 635 | 20 | 20 |  |  |  | 6 |
| 201 | GUI_SIMPLE_BUTTON | 516 | 635 | 20 | 20 |  |  |  | 7 |
| 202 | GUI_SIMPLE_BUTTON | 540 | 635 | 20 | 20 |  |  |  | 8 |
| 203 | GUI_SIMPLE_BUTTON | 564 | 635 | 20 | 20 |  |  |  | 9 |

Примечание: OBJECT_ID 86…89 и 128…159 в `inv_gui.scr` отсутствуют
(пропуски в нумерации; ссылок на них в коде не найдено).

## 3. Контейнер → UI-зона (доказано по коду)

`FUN_1203cfd0` (RVA `0x3cfd0`) при инициализации экрана присваивает:

| Поле экрана | Источник | Смысл |
|---|---|---|
| `+0x188` | `hero + 0x10` | container 1 (основной инвентарь) |
| `+0x184` | `hero + 0x0C` | container 0 (расходники) |
| `+0x18C` | `local_player + 0x66C` | container 2 (экипировка) |

`+0x188`/`+0x18C`/`+0x184` — id наборов в менеджере `DAT_12110edc`
(`FUN_120a4b24`). Для каждого набора `FUN_1205eb44` / `FUN_1205f148` /
`FUN_1205f74c` создают GUI-иконки и привязывают их к динамическим
контейнерам:

| Набор | Контейнер иконок | Функция создания | Зона |
|---|---|---|---|
| `+0x184` (container 0) | `+0x180` | `FUN_1205eb44` | hero hot slots, `y=624` |
| `+0x18C` (container 2) | `+0x17C` | `FUN_1205f148` | puppet, фиксированные слоты |
| `+0x188` (container 1) | `+0x178` | `FUN_1205f74c` | нижняя полоса, `y=703` |

Отрисовка/раскладка — `FUN_120688f0` (RVA `0x688f0`); создание иконок,
пути и счётчик стека — `FUN_12063d80` (RVA `0x63d80`); ввод/скролл/фильтры
— `FUN_120697ec` (RVA `0x697ec`).

## 4. Геометрия инвентаря (container 1)

- Видимая область: объект 1, `(138,696,730×66)`.
- Иконка предмета: **55×55** (создаётся с `prop 0x25 = 0x37,0x37`).
- `y = 0x2BF = 703` (из `FUN_120688f0`, `PUSH 0x2bf`).
- x-координаты 12 слотов (`DAT_120f87d4`, VA `0x120f87d4`):
  `140, 201, 261, 321, 382, 443, 504, 565, 627, 688, 749, 811`.
- Слот `k` (0..11) рисуется в `(x[k], 703)`, `55×55`; шаг ~60–61 px.
- Прокрутка: поле `screen+0x190` = индекс первого видимого предмета.
  - стрелки (объекты 3/4): `±1`;
  - колесо/команды `0x51`/`0x52`: `±3`;
  - clamp: `0 .. max(0, count−12)`; при смене фильтра `page=0`.
- В `FUN_120688f0` позиция берётся как
  `DAT_120f87d4[(absolute_index − page) % 12]`, `y=703`.

## 5. Hero hot slots (container 0)

- Объект 99: `(10,618,266×40)`.
- Иконка: **27×27** (`prop 0x25 = 0x1B,0x1B`).
- `y = 0x270 = 624`.
- x-координаты (`DAT_120f87ac`, VA `0x120f87ac`, 10 значений):
  `17, 14, 43, 72, 101, 130, 159, 188, 217, 246`.
- Используются индексы **1..9** (x = 14…246): слот быстрого доступа
  `uVar14 = (record+0x0C & 0x0000F000) >> 12`; рисуется только если
  `1 <= uVar14 <= 9`. Индекс 0 (`x=17`) этим путём не используется.
- Пагинации нет; позиция определяется маской записи, а не порядком.

## 6. Paper-doll puppet (container 2)

Иконка слота: **55×55**. Позиции — равенство `(record+0x0C & 0x00FFF000)`
одному из equip-значений (`FUN_120688f0`, RVA `0x688f0`):

| equip mask | координата (x,y) | категория |
|---|---|---|
| `0x004000` | `(67,307)` | helmet |
| `0x008000` | `(168,308)` | amulet |
| `0x010000` | `(0,354)` | ring A |
| `0x020000` | `(233,354)` | ring B |
| `0x040000` | `(4,402)` | bracer A |
| `0x080000` | `(229,402)` | bracer B |
| `0x001000` | `(22,472)` | weapon A |
| `0x400000` | `(216,472)` | shield A |
| `0x002000` | `(16,466)` | weapon B |
| `0x800000` | `(211,466)` | shield B |
| `0x100000` | `(16,547)` | armor |
| `0x200000` | `(216,547)` | elbarms/leggings |

Дополнительно:
- Активный набор оружия/щита задаётся `screen+0x1D4` (0 = A, 1 = B;
  заполняется из `local_player+0x6E4` в `FUN_1203cfd0`).
- Активная пара weapon/shield получает вариант изображения
  `prop 1 = (0x34,0x35)` и `(0x32,0x33)`; неактивная —
  `(0x34,0x34)`, `(0x32,0x32)` и `prop 0x57 = (0,0)`. Точная визуальная
  семантика `0x57` не декодирована (§10).
- Fallback для записи без известной equip-маски:
  `x = base_x + i*0x3B, y = base_y`, где `base_x/base_y` берутся из
  `FUN_1203f6c8(gui, 2, 4, &rect, 0)`; свойство 4 не декодировано.
  В slot0/slot2 таких записей нет — fallback **BLOCKED**, но не влияет
  на принятые данные.
- Оригинал дополнительно рисует **paper-doll overlays** из
  `puppet_name` (BMP 190×230) поверх фигуры героя
  (`FUN_120683a8`, RVA `0x683a8`, маски слоёв
  `0x200000,0x100000,0xF8000,0x4000,0xC00000,0x3000`). Это относится
  к P4d/P5a outfit, не к слотам инвентаря; для P5b1 слоты — это
  `icon_name` 55×55.

## 7. Фильтры 1..7 (container 1)

Кнопки: объекты 9…15, `GUI_CHECK_BUTTON`, `56×23`,
`y=672/673`, `x=201/290/383/474/565/658/745`, изображения
`engineres\newinv\filter1..7`.

Ввод (`FUN_120697ec`, команда `0x3F`, `*param_1` = id объекта):
`id 9..15` → `screen+4` (filter_mask) = `1,2,4,8,0x10,0x20,0`; затем
`page=0` и перерисовка. Фильтр **эксклюзивный** (устанавливается ровно
один бит), не аддитивный.

Применение: `FUN_120688f0` → `FUN_120626f4(list, filter_mask)` для
container 1. `filter_mask == 0` — без фильтрации. Иначе запись остаётся,
если её `type` (`record+0x08`) попадает в группу:

| Бит | Кнопка | ItemType (enum) | Пример |
|---|---|---|---|
| `0x01` | filter1 (id9) | 0..7, 28 = sword, axe, spear, bow, crossbow, rifle, staff, blunt, stone | axe 486 |
| `0x02` | filter2 (id10) | 11..14 = helmet, cuirass, shield, leggings | armor 318 |
| `0x04` | filter3 (id11) | 15..17 = bracers, amulet, ring | ring 233 |
| `0x08` | filter4 (id12) | 21,22 = book, scroll | scroll 1575 |
| `0x10` | filter5 (id13) | 8,9,10 (bullets, bolts, arrows), 19,20 (potion, food), 23,24 (smith_material, alchemy_ingredient), 26 (recipe), 27 (junk) | potion 953 |
| `0x20` | filter6 (id14) | 25 = quest_item | quest item 1037 |
| `0` | filter7 (id15) | все | — |

Особый случай: **type 18 (money)** остаётся при любом фильтре
(`case 0x12` в `FUN_120626f4` не удаляется).

## 8. Item catalog: ITM → SDB → icon

### 8.1 Формат/поля

`items/data/<id>.itm` (парсер `src/assets/src/itm.cpp`):

| ITM поле | Использование |
|---|---|
| `description_id` | = save item id (`record+0xA8`) и ключ SDB |
| `type` | `record+0x08`; фильтры 1..7; equip-маска не из type |
| `price` | `record+0x10`; UI-характеристика |
| `weight_x10` | `record+0x14`; UI-характеристика |
| `icon_name` | путь иконки инвентаря/слота |
| `puppet_name` | путь paper-doll overlay (190×230), P4d/P5a |

Тексты: `GoldenLand.exe` загружает `sdb\items\lit_names.sdb` и
`sdb\items\descriptions.sdb` (VA `0x485CF0` / `0x485D08`, загрузчик
`FUN_0042CA7C`). Ключ — `description_id`; в обоих SDB по 1848 записей.
Имя = `lit_names[description_id]`, описание = `descriptions[description_id]`.
Client.dll сам эти SDB не открывает.

Иконка: путь в ITM хранится с `\` и разным регистром
(`T_AXE\Two_handed\Almaz\01.bmp`). В архиве путь
`items/res/<icon_name>` в нижнем регистре:
`items/res/t_axe/two_handed/almaz/01.bmp`. Иконки `icon_name` — BMP
**55×55**, 8bpp indexed; `puppet_name` — BMP **190×230**, 8bpp indexed.
Виртуальная ФС порта регистронезависима, поэтому путь нужно нормализовать
(`\`→`/`, lower-case для сверки с архивом).

### 8.2 0/N проверка slot0 + slot2 (все 3 контейнера)

Сохранения распакованы zlib (offset 112), найден `BT_HERO `, тело с
`0x734`, записи `0xC0 + 4 + subcount*0x14`.

| Слот | container 0 | container 1 | container 2 |
|---|---|---|---|
| slot0 | 8 | 117 | 11 |
| slot2 | 9 | 140 | 11 |
| всего | 17 | 257 | 22 |

| Проверка | N | Расхождений |
|---|---|---|
| ITM `items/data/<id>.itm` найден | 296 | **0** |
| `lit_names[description_id]` найден, непустой | 296 | **0** |
| `descriptions[description_id]` найден, непустой | 296 | **0** |
| `items/res/<icon_name>` найден | 296 | **0** |
| пустые имена/описания | 296 | **0** |
| slot0 counts = P5a (8/117/11) | — | **0** |
| slot2 counts = P5a (9/140/11) | — | **0** |

Итог: **296/296** записей полностью разрешаются; отсутствующих
ITM/SDB/иконок на slot0/slot2 нет.

### 8.3 Распределение по типам (slot0+slot2)

| Слот | Контейнер | Type | Кол-во записей / сумма count |
|---|---|---|---|
| 0 | 0 | 19 potion | 3 / 3 |
| 0 | 0 | 22 scroll | 5 / 5 |
| 0 | 1 | 18 money | 1 / 43875 |
| 0 | 1 | 19 potion | 19 / 112 |
| 0 | 1 | 20 food | 13 / 152 |
| 0 | 1 | 21 book | 1 / 1 |
| 0 | 1 | 22 scroll | 25 / 57 |
| 0 | 1 | 23 smith_material | 6 / 71 |
| 0 | 1 | 24 alchemy_ingredient | 15 / 252 |
| 0 | 1 | 25 quest_item | 33 / 33 |
| 0 | 1 | 27 junk | 4 / 5 |
| 0 | 2 | 0,1,11,12,13,14,15,16,17 | 1,1,1,1,1,1,2,1,2 (по 1) |
| 2 | 0 | 19 potion | 6 / 6 |
| 2 | 0 | 22 scroll | 3 / 3 |
| 2 | 1 | 15 bracers | 2 / 2 |
| 2 | 1 | 18 money | 1 / 122008 |
| 2 | 1 | 19 potion | 21 / 77 |
| 2 | 1 | 20 food | 13 / 85 |
| 2 | 1 | 22 scroll | 62 / 136 |
| 2 | 1 | 23 smith_material | 14 / 70 |
| 2 | 1 | 24 alchemy_ingredient | 12 / 100 |
| 2 | 1 | 25 quest_item | 13 / 13 |
| 2 | 1 | 27 junk | 2 / 4 |
| 2 | 2 | 0,1,11,12,13,14,15,16,17 | 1,1,1,1,1,1,2,1,2 (по 1) |

### 8.4 Экипировка slot0/slot2 (container 2)

| Слот | idx | item id | type | имя | icon_name | price | weight_x10 | raw mask |
|---|---|---|---|---|---|---|---|---|
| 0 | 0 | 72 | 15 bracers | Проклятый браслет из голубой стали | `T_BRACELET\Goluboi\03.bmp` | 10000 | 1 | `0x0004016C` |
| 0 | 1 | 397 | 14 leggings | Простые поножи из голубого металла | `T_ELBARMS\Goluboi\01.bmp` | 11270 | 36 | `0xC0200204` |
| 0 | 2 | 280 | 11 helmet | Шлем из черного металла | `T_HELM\Cherny\00.bmp` | 12740 | 21 | `0x00004204` |
| 0 | 3 | 318 | 12 cuirass | Алмазные латы | `T_ARMOUR\Almaz\00.bmp` | 22294 | 98 | `0xC0100204` |
| 0 | 4 | 80 | 15 bracers | Сложный браслет из черного металла | `T_BRACELET\Cherny\05.bmp` | 12850 | 1 | `0x0008020C` |
| 0 | 5 | 233 | 17 ring | Кольцо Ярины | `T_RING\Bio\01.bmp` | 6200 | 1 | `0x8002006C` |
| 0 | 6 | 554 | 0 sword | Палаш Кукуя | `T_SWORD\One_handed\Platina\00.bmp` | 3590 | 26 | `0x00002064` |
| 0 | 7 | 141 | 16 amulet | Обычный амулет из красного металла | `T_AMULET\Krasny\01.bmp` | 5370 | 1 | `0x0000820C` |
| 0 | 8 | 486 | 1 axe | Топор Сумеречного Палача | `T_AXE\Two_handed\Almaz\01.bmp` | 66230 | 45 | `0x00001025` |
| 0 | 9 | 353 | 13 shield | Крепкий костяной щит | `T_SHIELD\Bio\00.bmp` | 2901 | 23 | `0x00800204` |
| 0 | 10 | 245 | 17 ring | Кольцо Звезда Марвии | `T_RING\Meteorit\02.bmp` | 4910 | 1 | `0x8001002C` |
| 2 | 0 | 486 | 1 axe | Топор Сумеречного Палача | `T_AXE\Two_handed\Almaz\01.bmp` | 66230 | 45 | `0x00001025` |
| 2 | 1 | 359 | 13 shield | Простой щит из черного металла | `T_SHIELD\Cherny\01.bmp` | 12270 | 34 | `0x00800204` |
| 2 | 2 | 554 | 0 sword | Палаш Кукуя | `T_SWORD\One_handed\Platina\00.bmp` | 3590 | 26 | `0x00002064` |
| 2 | 3 | 66 | 15 bracers | Толстый костяной браслет | `T_BRACELET\Bio\03.bmp` | 2780 | 1 | `0x0004020C` |
| 2 | 4 | 278 | 11 helmet | Шлем из голубой стали | `T_HELM\Goluboi\00.bmp` | 9100 | 19 | `0x00004204` |
| 2 | 5 | 317 | 12 cuirass | Кираса из черного металла | `T_ARMOUR\Cherny\01.bmp` | 17520 | 70 | `0x40100204` |
| 2 | 6 | 108 | 16 amulet | Изящный железный амулет | `T_AMULET\Jelezo\02.bmp` | 410 | 1 | `0x0000820C` |
| 2 | 7 | 396 | 14 leggings | Облегченные поножи из голубого металла | `T_ELBARMS\Goluboi\00.bmp` | 10237 | 32 | `0xC0200204` |
| 2 | 8 | 99 | 15 bracers | Классический таулиновый браслет | `T_BRACELET\Techno\05.bmp` | 4700 | 1 | `0x0008020C` |
| 2 | 9 | 236 | 17 ring | Кольцо Времени | `T_RING\Goluboi\01.bmp` | 16920 | 1 | `0x8001002C` |
| 2 | 10 | 243 | 17 ring | Классическое метеоритное кольцо | `T_RING\Meteorit\00.bmp` | 1184 | 1 | `0x8002020C` |

`raw mask` — `record+0x0C` как в save; equip-маска =
`raw & 0x00FFF000`. Для слотов puppet использовать §6.

### 8.5 Быстрые слоты (container 0) slot0/slot2

| Слот | item id | type | имя | raw mask | quick slot |
|---|---|---|---|---|---|
| 0 | 953 | 19 potion | Сильное зелье здоровья | `0x8000520C` | 5 |
| 0 | 1521 | 22 scroll | Свиток "Око тьмы" | `0x00003008` | 3 |
| 0 | 1554 | 22 scroll | Свиток "Божественное пламя" | `0x00001008` | 1 |
| 0 | 1575 | 22 scroll | Свиток "Призыв ночной тени" | `0x00009008` | 9 |
| 0 | 1579 | 22 scroll | Свиток "Крылатый раб" | `0x00008008` | 8 |
| 2 | 953 | 19 potion | Сильное зелье здоровья | `0x8000120C` | 1 |
| 2 | 1532 | 22 scroll | Свиток "Слепота" | `0x00006008` | 6 |
| 2 | 1575 | 22 scroll | Свиток "Призыв ночной тени" | `0x00004008` | 4 |

### 8.6 Fallback'и (наблюдаемое поведение оригинала)

- **Нет ITM**: запись не имеет item-объекта; в UI нечего показывать.
  Техническое имя `item <id>` — это **решение порта** (spec §9), не
  поведение оригинала.
- **Нет строки в SDB**: lookup возвращает пустую строку; встроенного
  технического имени в Client/GoldenLand не найдено.
- **Нет иконки**: `items\res\%s` не загружается; предмет остаётся в
  списке без bitmap. `engineres\interface\bag(.csx)` / `bag_shadow.csx`
  (18×18) — это **drag cursor** (`FUN_120398f8`, RVA `0x398f8`), а не
  fallback-иконка предмета. Использовать `bag.csx` как fallback — решение
  порта.
- **Нет save/парсер error**: пустые контейнеры + warning (spec §9).

## 9. Проверка геометрии по фону (0/N)

Метод: координаты из кода наложены на `inventory_secondary.bmp`;
проверены центры всех вычисленных rect'ов и границы контейнеров.
Evidence-изображение `%TEMP%\inv_overlay.png` (не коммитится; содержит
оригинальный фон). Проверки:

| Проверка | N | Расхождений |
|---|---|---|
| нижняя полоса: 12 rect'ов `(x[k],703,55×55)` внутри объекта 1 `(138,696,730×66)` | 12 | **0** (`811+55=866<=868`, `703+55=758<=762`) |
| hot slots: 9 rect'ов `(x[k],624,27×27)` внутри объекта 99 `(10,618,266×40)` | 9 | **0** (`14+27=41 … 246+27=273<=276`, `624+27=651<=658`) |
| puppet: 12 координат §6 попадают в чёрные слоты фона (центр `x+27,y+27` — near-black `R,G,B<16`) | 12 | **0** |
| black 55×55 слот на `(67,307)` совпадает с компонентой фона (bounding box `67,307,55×55`) | 1 | **0** |
| tab-кнопки (5/6) внутри верхних панелей фона | 2 | **0** |
| filter1..7 (y=672/673) над нижней полосой, не перекрывают её | 7 | **0** |
| scroll left/right (3/4), exit (7), drop (8) внутри нижних панелей | 4 | **0** |
| `inventory_skills.bmp` имеет те же нижние зоны/puppet; отличается только центральная/правая часть (skills/secondary rows) | 1 | **0** |

Ограничение проверки: `inventory_secondary` и `inventory_skills`
содержат разный шум текстуры по всему кадру, поэтому побайтовый diff
не использовался; проверялись геометрия и near-black области слотов.

## 10. BLOCKED / не выводить догадки

1. **Puppet fallback без equip-маски** (`x=base_x+i*59, y=base_y`):
   `base_x/base_y` берутся из свойства 4 GUI-объекта 2; семантика
   `FUN_1203f6c8(gui,2,4,...)` не декодирована. На slot0/slot2 таких
   записей нет (все 22 экипированных имеют известную маску), поэтому
   на принятые данные это не влияет. Evidence: `FUN_120688f0`
   `0x1206945E`, `0x120696E3`, `0x120696EC`.
2. **Семантика `prop 0x57`** для неактивного weapon/shield набора
   (вероятно, приглушение/скрытие) не декодирована; координаты и
   варианты изображения известны, визуальный эффект — нет.
3. **Tooltip предмета**: оригинал использует плавающее окно движка
   (`DAT_12110ef0`, создаётся в `FUN_120604B0`, инициализация 400 px),
   а не правую панель из spec §6. Точные rect/шрифт/переносы не
   извлекались. Это **расхождение spec**, требующее решения:
   либо порт повторяет tooltip, либо фиксированная правая панель —
   осознанное отклонение.
4. **Tab label ↔ background**: объект 5 (`button 5 - skills`) в
   `FUN_120697EC` ставит `+0x1D0=0` → фон `inventory_secondary`
   (`+0x1BC`); объект 6 (`secondary`) ставит `1` → `inventory_skills`
   (`+0x1C0`). Комментарии скрипта и имена фонов противоречат друг
   другу. Точные подписи вкладок рисуются шрифтом/кодом, не в скрипте;
   для P5b1 (skills disabled) достаточно default `+0x1D0=0`.
5. **27-строчная таблица** (объекты 100…126) — skills/secondary,
   подтверждено `FUN_120661CC`: x-массивы `DAT_120f8520`
   (`295/544/790`), y-массивы `DAT_120f85A0` (`130/162/194, 315/348/381,
   500/532/564`), id-массив `DAT_120f8620` (`49..76`). Не использовать
   как item grid.
6. **Page count**: оригинал — не дискретные страницы, а scroll offset
   (`screen+0x190`) с viewport 12; шаг 1/3. «Страница» в port UI —
   производная от offset, не отдельная сущность.

## 11. Что должен использовать P5b1 (без догадок)

- Логический холст 1024×768; фон `inventory_secondary` (default,
  `+0x1D0=0`) / `inventory_skills` (если skills включён).
- Container 0 → 9 hot slots: `(14,624,27×27)` … `(246,624,27×27)`,
  позиция по `(raw_mask & 0xF000)>>12` (1..9).
- Container 1 → 12 слотов: `(140,703)` … `(811,703)`, `55×55`,
  scroll offset 0..count−12, шаг 1 (стрелки) / 3 (колесо), фильтр
  сбрасывает offset в 0.
- Container 2 → puppet: таблица §6, иконка `icon_name` `55×55`;
  paper-doll overlay `puppet_name` — отдельный слой (P4d/P5a).
- Filter mapping §7; type 18 (money) не фильтруется; type 25 — filter6;
  остальные — по таблице.
- ItemCatalog: ITM `description_id/type/price/weight_x10/icon_name`;
  `lit_names`/`descriptions` по `description_id`; icon path
  `items/res/<icon_name>` (case-insensitive, `\`→`/`).
- Счётчик стека: `FUN_12063d80` ставит `prop 0x28` (decimal) только
  если `record+0x34 > 1` (в save `record+0xB0` = count).
- Текст имени/описания: если сохранять паритет — tooltip, не правая
  панель (см. §10.3).

## 12. Команды проверки (воспроизводимость)

```powershell
# распаковка и подсчёт (реальные данные, не коммитить)
build\bin\glpak.exe list "I:\Games\Златогорье-2"
build\bin\glpak.exe decode-sdb "I:\Games\Златогорье-2" --file "sdb/items/lit_names.sdb" --out "$env:TEMP\glsdb\lit_names.txt"
build\bin\glpak.exe decode-sdb "I:\Games\Златогорье-2" --file "sdb/items/descriptions.sdb" --out "$env:TEMP\glsdb\descriptions.txt"
build\bin\glpak.exe extract "I:\Games\Златогорье-2" --prefix "items/data" --out "$env:TEMP\glitems"
# Ghidra: decompile/scan
$env:JAVA_HOME="$PWD\.local\toolchains\jdk-21.0.12.1+1"
$proj="$env:TEMP\ghidra_glclient"
& .local\toolchains\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat `
  $proj glclient -process Client.dll -noanalysis `
  -scriptPath "$env:TEMP" -postScript "DecompileAt.java" 1205fe20 120604b0 120688f0 12063d80 120697ec 120626f4
```

0/N получено offline-скриптом (парсер ITM + SDB + zlib save):
`ITM parsed total=1848 iconSet=1938 lit=1848 desc=1848`,
`missing rows=0` для 296 записей slot0+slot2. Скрипт и CSV — в
`%TEMP%` (`parse_items.ps1`, `inv_items.csv`), не коммитятся.

## 13. Artifacts (вне репозитория)

- `%TEMP%\inv_items.csv` — 296 записей (slot/container/index/id/type/
  count/mask/itm/desc_id/name/desc_len/icon/icon_exists/price/weight).
- `%TEMP%\inv_summary.txt` — type counts, equipped, distinct inventory.
- `%TEMP%\inv_overlay.png` — наложение rect'ов на фон (не коммитить:
  содержит оригинальный арт).
- Ghidra project: `%TEMP%\ghidra_glclient`.
