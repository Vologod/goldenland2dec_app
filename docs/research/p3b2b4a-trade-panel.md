# Phase 1b2b4a: trade panel research (Client.dll/Server.dll/assets)

Дата: 2026-11-09. Задача: Task 1 плана
`docs/plans/2026-11-09-p3b2b4a-trade-overlay.md`. Только research,
production-код не менялся.

## 0. Вердикт

**`implementable`** для read-only overlay: layout/ассеты, списки, деньги,
цены и реальный торговец доказаны. Внедрение возможно без покупки/продажи
и без trade-мутаций.

Ключевой факт, меняющий план Task 2: **список товаров NPC — это
trade-контейнер `entity+0xC` (pass 1 NPC-лута), а не main `entity+0x10`.**
Порт 1b2b2b сейчас **отбрасывает pass 1 (`hot`)**; для overlay его нужно
сохранять (RNG уже продвигается корректно).

Остаётся недоказанным (не блокирует read-only, см. §8):
item-type → filter-bit группировка для trade-списков; источник id в
`local_player+0x6f8/+0x6fc` (staging-контейнеры выбора); modifier-блок
цены для неопознанных предметов; точная привязка exit-кнопки к команде.

## 1. Provenance / метод

| Файл | SHA-256 |
|---|---|
| `I:\Games\Златогорье-2\Client.dll` | `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16` |
| `I:\Games\Златогорье-2\Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |

Client base `0x12000000`, Server base `0x14000000`. Для `.text`
Client.dll file offset == RVA; для `.data1` (RVA `0x122000`) file offset =
RVA − `0x1B000`; для Server `.data1` file offset = RVA − `0xC000`.

- Client decomp-корпус сгенерирован локально (gitignored):
  `.local/decomp/client/decompiled.c` (1936 функций, 1901 ok, 0 failed,
  35 thunks) через `tools/ghidra/ExportDecomp.java`. Ссылки ниже:
  `RVA` + `client decompiled.c:<line>`.
- Server decomp: `.local/decomp/server/decompiled.c`,
  handlers: `.local/decomp/server/natives/natives.c`.
- Ассеты: `glpak extract --prefix engineres/trade`,
  `--file scripts/ui/trade.scr`; BMP-заголовки читались Python/struct.
- Реальные данные: `scripts.pak` (`l13_1.sef`, `inv013_barman.inv`,
  `l13_1.p517_boat_trader.d517.age.cs`), `items/data/*.itm`,
  `sdb/items/tech_names.sdb`, `Saves/slot0.sav`/`slot2.sav`.
- Oracle: независимый Python-референс `NpcLootGenerator` (MSVC LCG,
  `coin` float, truncating `range`, оба прохода, без specials — как в
  порте); валидация по уже принятому 1b2b2b oracle (см. §7.4).

## 2. Ассеты и layout

### 2.1 BMP (`engineres/trade/`, 53 файла)

| Файл | W×H | bpp | Роль |
|---|---:|---:|---|
| `trade.bmp` | 1024×768 | 8 | фон окна (грузится в ctor, color key `0xff00ff`) |
| `weapon_slot1.bmp` | 266×71 | 8 | фон/рамка активного набора оружия B (`screen+0x22c != 0`) |
| `filter1..7.bmp` | 56×23 | 8 | hero-инвентарь, кнопки 9..15 |
| `afilter1..7.bmp` | 52×20 | 8 | trade-список NPC, кнопки 30..36 |
| `inv_left.bmp` | 27×55 | 8 | скролл hero-инвентаря влево (obj 3) |
| `inv_right.bmp` | 26×54 | 8 | скролл hero-инвентаря вправо (obj 4) |
| `left1..3.bmp` / `right1..3.bmp` | 27×55 | 8 | скролл трёх trade-рядов (obj 37..42) |
| `lbig1..5.bmp` / `pbig1..5.bmp` | 167×44, `lbig2` 110×43 | 8 | сервисные кнопки (lit/pressed) |
| `exit.bmp` / `exitl.bmp` | 75×68 | 8 | exit (pressed/lit), obj 7 |
| `drop.bmp` | 81×75 | 8 | drop, obj 8/100 |
| `lm1..7.bmp` / `lp1..7.bmp` | 26×26 | 8 | primary stat −/+ (левая панель) |
| `wear_1.bmp` / `wear_2.bmp` | 10×20 | 8 | не используются в `trade.scr` (10×20 wear-слоты) |

### 2.2 `scripts/ui/trade.scr` (1175 строк)

Файл — гибрид: левая часть совпадает с `inv_gui.scr` (hero-инвентарь,
puppet, primaries, hot slots), правая/центральная — собственно trade.
Полный список объектов (id/type/rect/base/lit/press) снят скриптом;
ключевые объекты:

| id | type | rect | image | смысл |
|---:|---|---|---|---|
| 1 | GUI_DD_CONTAINER | 138,696,730×66 | — | hero main, 12 слотов |
| 2 | GUI_DD_CONTAINER | 8,296,270×320 | — | paper-doll |
| 3/4 | BUTTON | 110,702,27×55 / 870,703,26×54 | `inv_left`/`inv_right` | скролл hero main |
| 7 | BUTTON | 920,694,75×68 | `exitl`/`exit` | выход |
| 8/100 | CONTAINER/BUTTON | 19,690,81×75 | `drop` | drop |
| 9..15 | CHECK_BUTTON | y=672/673, 56×23, x=201/290/383/474/565/658/745 | `filter1..7` | hero main filters |
| 16..29 | BUTTON | x=168/231, y=59..225, 26×26 | `lm1..7`/`lp1..7` | primaries |
| 30..36 | CHECK_BUTTON | y=94/95, 52×20, x=350/439/531/621/713/806/893 | `afilter1..7` | NPC trade filters |
| 37/38 | BUTTON | 312,28,27×55 / 954,26,27×55 | `left3`/`right3` | скролл верхнего trade-ряда |
| 39/40 | BUTTON | 313,208 / 955,207, 27×55 | `left2`/`right2` | скролл среднего trade-ряда |
| 41/42 | BUTTON | 314,477 / 955,477, 27×55 | `left1`/`right1` | скролл нижнего trade-ряда |
| 43..47 | BUTTON | 368,311,167×44; 579,320,110×43; 736,311,167×44; 368,378,167×44; 736,378,167×44 | `lbig1..5`/`pbig1..5` | сервисы: repair, exchange, identify, charging, remove curse (звуки `sounds\ui\trade\*`) |
| 48 | GUI_DD_CONTAINER | 343,21,609×63 | — | trade-ряд 1 (верх) |
| 49 | GUI_DD_CONTAINER | 343,204,609×63 | — | trade-ряд 2 (середина) |
| 50 | GUI_DD_CONTAINER | 343,471,609×63 | — | trade-ряд 3 (низ) |
| 84/85 | BUTTON | 79,507 / 201,464, 10×20 | — | wear weapon slot 1/2 |
| 90..98 | BUTTON | primaries/puppet hotspots | — | из `inv_gui.scr` |
| 99 | GUI_DD_CONTAINER | 10,618,266×40 | — | hero hot slots |
| 101..103 | BUTTON | 560,126 / 560,386 / 560,613 | — | quantity/label позиции |

Имена контейнеров в скрипте («48 hero sel», «49 trader sel», «50 trader»)
**не соответствуют** фактическому коду: ниже доказано, что верхний ряд
(48, `screen+0x198`) — товары NPC, а нижние два — staging-выбор.

### 2.3 Геометрия trade-рядов (код, не скрипт)

Из `FUN_1207A2A8` (RVA `0x7A2A8`, client decompiled.c:79729) и
`FUN_12071014` (RVA `0x71014`, :75571):

| Ряд | y иконок | x слотов (10) | видимых | scroll field | источник контейнера |
|---|---:|---|---:|---|---|
| верх (obj 48) | `0x1A` = 26 | 347,407,467,528,589,650,711,773,834,895 | 10 | `screen+0x1C0` | `screen+0x198` |
| середина (obj 49) | `0xD0` = 208 | те же | 10 | `screen+0x1B4` | `screen+0x1A8` |
| низ (obj 50) | `0x1DE` = 478 | те же | 10 | `screen+0x1AC` | `screen+0x1A4` |
| hero main (obj 1) | `0x2BF` = 703 | 140,201,261,321,382,443,504,565,627,688,749,811 | 12 | `screen+0x1BC` | `screen+0x190` |
| hero hot (obj 99) | `0x270` = 624 | 14,43,72,101,130,159,188,217,246 | 9 | — | `screen+0x1A0` |

Icon target rect: **55×55** (prop `0x25 = 0x37,0x37`); native-size bitmap,
без масштаба (как P5b1). Scroll: стрелки ±1, колесо/команды ±3
(`FUN_1207C41C`, client decompiled.c:80792; для hero main :80835-80860,
для верхнего trade-ряда :80872-80897). При смене фильтра scroll = 0.

Деньги/подписи рисуются кодом, не объектами скрипта (см. §4.4):
`y = 126` (верх, деньги NPC), `y = 386` (разница sell−buy), `y = 612`
(hero money); x = window offset + `0x232` (562).

## 3. Client.dll: функции

| RVA | client decompiled.c | Роль |
|---:|---|---|
| `0x708E0` | 75284 | ctor/init: грузит `engineres\trade\trade`, `weapon_slot1`, `scripts\UI\trade.scr`; `+0x218 = 0x100` |
| `0x7209C` | 76110 | ctor объекта (vtable `PTR_LAB_12129688`), обнуление полей |
| `0x3CFD0` | 46388 | переключатель экранов; `param_1 == 0xF` — **открытие trade** (:46781-46831) |
| `0x70770` | 75186 | очистка списков/bitset'ов |
| `0x70C88` | 75422 | освобождение текстур/списков (destroy) |
| `0x6FBC0` | 74158 | заполнение hero main `+0x10` → list `+0x184`; prop `0x2A=1`; `FUN_120723E8(...,1)` |
| `0x6F5BC` | 73644 | заполнение hero equip `+0x66C` → list `+0x188`; prop `0x2A=2`; stack count prop `0x28` |
| `0x6EFB8` | 73130 | заполнение hero hot `+0xC` → list `+0x19C`; prop `0x2A=99`; `FUN_120723E8(...,99)` |
| `0x70198` | 74672 | заполнение **NPC trade `+0xC`** (screen `+0x198`) → list `+0x18C`; prop `0x2A=0`; `FUN_120723E8(...,0)` |
| `0x7A2A8` | 79729 | пересчёт selection/bitset'ов и отрисовка 6 списков; координаты §2.3 |
| `0x71014` | 75571 | render: фильтры/кнопки/итоги/деньги; `FUN_12074C04` |
| `0x70DAC` | 75470 | input/update кадра; exit/weapon-set команды |
| `0x6EC98` | 72997 | `screen+0x198 = person+0xC`, если `person+0x14 & 2`, иначе −1 |
| `0x6DA08` | 72241 | `money(container) = count MON_1_0_1` |
| `0x6E8D0` | 72880 | `price(uid, mode)` (см. §4) |
| `0x723E8` | 76237 | item display: stack count + **per-unit price** = `price(uid,mode)/count` |
| `0x725C4` | 76301 | tooltip/иконка: prop `0x57` + per-unit price |
| `0x74938` | 77690 | визуальные pressed/lit состояния фильтров (`+0xC`, `+0x10`) |
| `0x7C41C` | 80792 | GUI-команды: фильтры, скроллы, drop, exit |
| `0x54A20` | 58107 | копирует `local_player+0x6F8/+0x6FC/+0x700` в другой менеджер (trade start) |
| `0xC33D8` | (вне списка функций Ghidra; asm `0x120C33D8`) | vtable slot 26 `DAT_12110F28`: set/register `+0x6F8/+0x6FC/+0x700` через `FUN_120A47C0`/`FUN_120A4B24`/`FUN_120A481C` |

### 3.1 Открытие (`FUN_1203CFD0`, case `param_1 == 0xF`)

`client decompiled.c:46781-46831`:

```c
iVar6 = selected person entity; // = FUN_120c54dc(...) -> массив персон
screen->0x190 = *(iVar6 + 0x10);        // hero main
screen->0x194 = *(local_player + 0x66C); // hero equip
screen->0x1A0 = *(iVar6 + 0xC);          // hero hot
screen->0x1A4 = *(local_player + 0x6F8); // staging A
screen->0x1A8 = *(local_player + 0x6FC); // staging B
FUN_12070770(); screen->0x10=0; screen->0x1BC=0; screen->0x1C0=0;
FUN_1206FBC0(); FUN_1206F5BC(); FUN_1206EFB8(); FUN_12070198();
FUN_1207A2A8(); FUN_12074938(); FUN_1206DA30();
```

`screen+0x198` в init не ставится; его единственный writer —
`FUN_1206EC98` (selected person `+0xC`), т.е. trade-контейнер выбранной
персоны (NPC).

### 3.2 Закрытие

- `FUN_12070C88` (:75422) уничтожает текстуры/списки (destroy объекта).
- `FUN_12070DAC` (:75470) при команде `0x2B` вызывает
  `(**(DAT_12110954+8))(0xD, -1)` и `(**(DAT_12110F48+0x6C))()` — возврат
  на экран `0xD`/закрытие; команда `0x36` — переключение набора оружия
  (не exit). Точная привязка exit-кнопки (obj 7) к `0x2B` не трассирована;
  для overlay достаточно Esc/exit → close.

## 4. Данные и цены

### 4.1 Источник списков

| Список | screen field | источник | доказательство |
|---|---|---|---|
| верх (товары NPC) | `+0x198` | **selected person `entity+0xC`** (NPC trade container) | `FUN_1206EC98` (:72997); server `FUN_14028404` кладёт NPC `+0xC` в hero `+0x4D4`; `RS_PersonAddItemToTrade`/`Remove` используют `entity+0xC` |
| hero main | `+0x190` | `entity+0x10` | `FUN_1203CFD0` :46796; `FUN_1206FBC0` |
| hero hot | `+0x1A0` | `entity+0xC` | :46798; `FUN_1206EFB8` |
| hero equip | `+0x194` | `local_player+0x66C` | :46797; `FUN_1206F5BC` |
| staging A (низ) | `+0x1A4` | `local_player+0x6F8` | :46799; `FUN_12070198` не участвует |
| staging B (середина) | `+0x1A8` | `local_player+0x6FC` | :46800 |

`+0x1A4`/`+0x1A8` — клиентские контейнеры выбора; их id регистрируются
vtable-сеттером `0x120C33D8` (slot 26) через менеджер `DAT_12110EDC`.
Источник самих id (сетевой пакет/серверный trade-start) не найден —
`unverified`; для read-only верхнего ряда и hero-инвентаря не требуется.

**NPC trade container = pass 1 NPC-лута.** `1b2b2b` §15.2: pass 1
`FUN_1404b1c0` пишет в `+0xC`, pass 2 — в `+0x10`. `RS_PersonAddItemToTrade`
(`0x5000006`, `natives.c:1931`) и `RS_PersonRemoveItemToTrade`
(`0x5000007`, :1961) работают с `*(int*)(entity+0xC)`. Порт сейчас
отбрасывает `hot`; Task 2 должен сохранять pass 1.

### 4.2 Деньги

`FUN_1206DA08(containerId)` (:72241) = `FUN_120A52C8(container,
"MON_1_0_1")` — **count TECH `MON_1_0_1`**. Render:
`FUN_1206DA08(screen+0x198)` (деньги NPC, y=126),
`FUN_1206DA08(screen+0x190)` (деньги hero, y=612);
третий текст (y=386) = `sell_total(+0x250) − buy_total(+0x24C)`
(asm `0x1207176A-0x1207177D`).

Type 18 (`money`) не фильтруется фильтрами (`FUN_120626F4`, P5b1 §7);
`FUN_120723E8` не показывает цену для type `0x12`.

### 4.3 Цена (`FUN_1206E8D0`, RVA `0x6E8D0`, :72880)

Сигнатура фактически `price(uid, mode)` (`__fastcall`, uid в `ecx`,
mode на стеке). Псевдокод:

```
rec = item_pool[uid]; if (!rec) return 0;
base0 = trunc( rec.price(+0x10) * (rec.durability(+0xB4) * 0.0025 + 0.75) );
base  = base0;
if ((rec.flags(+0xC) & 0x80) == 0 && rec.mod_count(+0x78) != 0)
    base += Σ modifier_value;          // unidentified modifiers, см. §8
if (base < base0/3) base = base0/3;
skill = hero_skill(SKILL_TRADE);        // id 0x10 = 16
f = 2.5 - 0.1*skill;                    // _DAT_12128fe8=-0.1, _DAT_12128ff0=2.5
if (hardcore) {                         // DAT_12106bc0+0x8C != 0
    if (perks & 0x10)  f = (f + (-1.0))*0.75 + 1.0;  // _fd8=-1.0, _fd0=0.85
    if (perks & 0x400) f = (f + (-1.0))*0.85 + 1.0;
}
if (f < 1.0) f = 1.0;                   // _fe0=1.0
if (mode != 0) f = 1.0 / f;             // sell
unit = trunc(f * base); if (unit < 1) unit = 1;
return unit * rec.count(+0xB0);
```

- `mode == 0` — **buy** (игрок платит): factor `2.5 − 0.1*skill`.
- `mode != 0` — **sell** (игрок получает): reciprocal.
- Округление — **truncation**: asm `fldcw [esp+8]` с `or eax,0xC00`
  (`0x1206E9A2-0x1206E9BA`); `ROUND` decompiler = `fistp` с RC=trunc.
- `base0`: `ITM price` (`+0x10`) × durability factor; для нового лута
  durability = 100 (Server `FUN_1404E3A4`, :54645), поэтому `base0 =
  ITM price`.
- Per-unit price в UI: `FUN_120723E8`/`FUN_120725C4` делят `price(uid,mode)`
  на `count(+0xB0)` (integer div) → **per-unit**.
- Режимы по спискам: верхний NPC `FUN_12070198` → `mode 0` (buy);
  hero main `FUN_1206FBC0` → `1` (sell); hero hot `FUN_1206EFB8` → `99`
  (sell); hero equip `FUN_1206F5BC` цену не вызывает.
- Итоги render: `+0x24C = Σ price(uid,0)` по staging B (`+0x1A8`);
  `+0x250 = Σ price(uid,1)` по staging A (`+0x1A4`)
  (client decompiled.c:75837-75866).

### 4.4 Фильтры

`FUN_1207C41C` (:80792) команда `0x3F`:

- ids **9..15** → `screen+0xC = 1,2,4,8,0x10,0x20,0`; `screen+0x1BC=0`
  (hero main scroll) → фильтры hero main (obj 1).
- ids **30..36** → `screen+0x10 = 1,2,4,8,0x10,0x20,0`; `screen+0x1C0=0`
  (верхний trade scroll) → фильтры верхнего NPC-ряда (`+0x198`).
- Маска **эксклюзивная** (один бит, 0 = all), не toggle. Визуал
  pressed/lit — `FUN_12074938` (:77690).
- Item-type → bit группировка для trade-списков **не доказана**;
  для inventory она в `FUN_120626F4` (P5b1 §7): 1 → types 0..7,28;
  2 → 11..14; 4 → 15..17; 8 → 21,22; 0x10 → 8,9,10,19,20,23,24,26,27;
  0x20 → 25; type 18 (money) не фильтруется. Использовать эту
  группировку для trade — assumption, не паритет (§8).

### 4.5 Прочие поля экрана

- `screen+0x10` — trade filter mask; `+0xC` — hero filter mask.
- `+0x22C` — активный набор оружия B; при `!=0` рисуется
  `weapon_slot1` (`FUN_12079854` :79337); `+0x234` — выбранный item uid;
  `+0x230` — флаг выбора; `+0x24C/+0x250` — итоги; `+0x254` — кэш итогов.
- `+0x218` — следующий динамический GUI object id (старт `0x100`).
- `+0x190/+0x194/+0x198/+0x1A0/+0x1A4/+0x1A8` — container ids (§4.1).
- `+0x1BC/+0x1C0/+0x1B4/+0x1AC` — scroll offsets (hero main, верх,
  середина, низ).

## 5. Server: `RS_PassToTradePanel` / `FUN_14028404`

Handler `native_700000d_RS_PassToTradePanel` (RVA `0x41558`,
`natives.c:2491-2520`):

```
if (FUN_14038c6c(-1) == 0) return 0.0;         // arity -1 (любая)
FUN_14028404(DAT_14088dc4, dialog+0x2D4, dialog+0x2D8);
dialog+0x5528 = 1;                              // close flag
return 0.0;                                     // _DAT_140914b0 = 0.0
```

`FUN_14028404` (RVA `0x28404`, server decompiled.c:29249) —
`(world, param_1, param_2)`:

1. Находит в world-списке (`world+0x1CF4`, count `+0x1CFC`, bitset
   `+0x1CF8`) сущность с `*entity == param_1`.
2. В первый свободный из 4 action-слотов (`+0x4B0/+0x4B4/+0x4B8/+0x4BC`)
   пишет тип **3** (trade), аргумент (`+0x4C0..+0x4CC`) = `param_2`.
3. Находит в списке `world+0x2564` сущность `param_2`; если есть —
   `entity->+0x4D4 = param_2_entity->+0xC` (trade-контейнер), иначе `-1`.

Семантика аргументов доказана `OpenDialog`: `FUN_1404410C(this, p1, p2)`
проверяет, что сущность `p2` имеет `(type & 0x3E) == 2`, и передаёт её
`+0x2680` (dialog script); `FUN_140441D8` пишет
`dialog+0x2D4 = p1`, `dialog+0x2D8 = p2` (`server decompiled.c:46400,
46529-46530`). Отсюда `p1 = hero/player`, `p2 = NPC speaker`:
`FUN_14028404` добавляет trade-action **hero** с аргументом NPC и
записывает в hero `+0x4D4` = **NPC `+0xC`** (trade-контейнер NPC).

`dialog+0x5528` читается в dialog-update (`server decompiled.c:46092`):
при `1` диалог завершает/закрывает текущую ветку (клиент открывает
экран `0xF` по action type 3).

## 6. Реальный торговец для E2E

### 6.1 Выбор

`L13_1.P517_Boat_Trader` (`l13_1`):

- SEF `levels/single/l13_1/l13_1.sef`: 8 persons; индекс **3**
  (`--click-npc 3`), `scr_dialog = l13_1\l13_1.p517_boat_trader.d517.age`,
  `scr_inv = inv013_barman`.
- `scripts/persons/l13_1.p517_boat_trader.scr`: `trade_panel`
  `change: 1`, `repair: 1`, `charging: 0`, `identification: 0`,
  `take_off_curse: 0`.
- `inv013_barman.inv`: `regenerate_chance 75`, `chance_decrease 50`,
  `quantity_decrease 10`; 15 `MON_1_0_1`, 15 `FOD_1_*`, 6 `POT_1_00_2x`.
- Диалог `l13_1.p517_boat_trader.d517.age.cs` (`glpak disasm-cs`):
  первый экран `D_Say(8868)` с ответами `[3498, 4214]`; ответ 3498
  (`--answer 0`) → bytecode 127 `RS_PassToTradePanel()`, затем
  bytecode 131 `D_CloseDialog(0)`. Подтверждено `glscript`
  (см. §7.3): `dialogs: 8868 3498 4214`, native 0x700000D вызван.

### 6.2 NPC trade stock (pass 1 hot)

Условия oracle: `--level l13_1 --equip-save 0|2 --seed 1`;
`loot_level = hero_level(EXP)+1` (`tools/glgame/main.cpp:784`):
slot0 EXP 81099 → level 40 → **loot_level 41**; slot2 EXP 65133 →
level 36 → **loot_level 37**. `SWR/specials` не моделируются (как в
порте; документированный RNG-desync).

**slot0** (loot 41, seed 1) — `L13_1.P517_Boat_Trader`, 16 записей:

| TECH | id | count |
|---|---:|---:|
| `MON_1_0_1` | 812 | 12266 |
| `MON_1_0_1` | 812 | 17040 |
| `MON_1_0_1` | 812 | 12836 |
| `FOD_1_2_1` | 408 | 10 |
| `FOD_1_3_1` | 409 | 4 |
| `FOD_1_4_1` | 410 | 8 |
| `FOD_1_9_1` | 415 | 2 |
| `FOD_1_10_1` | 416 | 1 |
| `FOD_1_13_1` | 419 | 2 |
| `FOD_1_15_1` | 421 | 2 |
| `POT_1_00_25` | 951 | 53 |
| `POT_1_00_26` | 952 | 31 |
| `POT_1_00_27` | 953 | 14 |
| `POT_1_00_28` | 954 | 51 |
| `POT_1_00_29` | 955 | 29 |
| `POT_1_00_30` | 956 | 16 |

NPC money (sum MON) = **42142**.

**slot2** (loot 37, seed 1) — 17 записей:

| TECH | id | count |
|---|---:|---:|
| `MON_1_0_1` | 812 | 9813 |
| `MON_1_0_1` | 812 | 13632 |
| `MON_1_0_1` | 812 | 10266 |
| `MON_1_0_1` | 812 | 15197 |
| `FOD_1_2_1` | 408 | 4 |
| `FOD_1_3_1` | 409 | 8 |
| `FOD_1_8_1` | 414 | 2 |
| `FOD_1_9_1` | 415 | 1 |
| `FOD_1_12_1` | 418 | 2 |
| `FOD_1_14_1` | 420 | 2 |
| `FOD_1_15_1` | 421 | 8 |
| `POT_1_00_25` | 951 | 46 |
| `POT_1_00_26` | 952 | 29 |
| `POT_1_00_27` | 953 | 17 |
| `POT_1_00_28` | 954 | 43 |
| `POT_1_00_29` | 955 | 32 |
| `POT_1_00_30` | 956 | 16 |

NPC money = **48908**.

### 6.3 Hero oracle

| ключ | slot0 | slot2 |
|---|---:|---:|
| `Hero.MON_1_0_1` | 43875 | 122008 |
| `Hero.POT_1_00_27` (id 953) | 51 | 25 |
| `Hero.RGT_1_0_8` (id 444) | 6 | 1 |
| main records | 117 | 140 |
| hot records | 8 | 9 |
| equipped | 11 | 11 |
| `SKILL_TRADE` (id 16) | 1 | 5 |

Hero item price fields (save), example id 953: `+0x10 price=500`,
`+0xB4 durability=100`, `+0x78 modifiers=0`; money id 812 price=1.

### 6.4 Buy/sell per-unit (trunc)

`ITM price` из `items/data/<id>.itm`; `durability=100`; `base=price`.
`buy = trunc(f*price)`, `sell = trunc(price/f)`, min 1.

slot0 (`SKILL_TRADE=1`, buy factor 2.4, sell factor 1/2.4):

| TECH | price | buy | sell |
|---|---:|---:|---:|
| MON_1_0_1 | 1 | 2 | 1 |
| FOD_1_2_1 | 95 | 228 | 39 |
| FOD_1_3_1 | 35 | 84 | 14 |
| FOD_1_4_1 | 40 | 96 | 16 |
| FOD_1_9_1 | 80 | 192 | 33 |
| FOD_1_10_1 | 85 | 204 | 35 |
| FOD_1_13_1 | 160 | 384 | 66 |
| FOD_1_15_1 | 180 | 432 | 75 |
| POT_1_00_25/28 | 50 | 120 | 20 |
| POT_1_00_26/29 | 100 | 240 | 41 |
| POT_1_00_27/30 | 500 | 1200 | 208 |

slot2 (`SKILL_TRADE=5`, factor 2.0 / 0.5):

| TECH | price | buy | sell |
|---|---:|---:|---:|
| MON_1_0_1 | 1 | 2 | 1 |
| FOD_1_2_1 | 95 | 190 | 47 |
| FOD_1_3_1 | 35 | 70 | 17 |
| FOD_1_8_1 | 60 | 120 | 30 |
| FOD_1_9_1 | 80 | 160 | 40 |
| FOD_1_12_1 | 85 | 170 | 42 |
| FOD_1_14_1 | 25 | 50 | 12 |
| FOD_1_15_1 | 180 | 360 | 90 |
| POT_1_00_25/28 | 50 | 100 | 25 |
| POT_1_00_26/29 | 100 | 200 | 50 |
| POT_1_00_27/30 | 500 | 1000 | 250 |

Hero sell examples: id 953 price 500, dur 100 → slot0 sell 208, slot2 250.

## 7. Проверка (фактический вывод)

### 7.1 Хэши/ассеты

```
Client.dll FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16
Server.dll 418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837
glpak extract --prefix engineres/trade -> extracted 53 file(s)
trade.bmp (1024,768) 8bpp; filter1..7 (56,23); afilter1..7 (52,20);
lbig1 (167,44); lbig2 (110,43); exit (75,68); drop (81,75)
```

### 7.2 Ghidra

```
ExportDecomp.java -> done functions=1936 ok=1901 failed=0 thunks=35
RefsTo 12129698/121296b0/121296d0 -> FUN_120708e0 (ctor)
RefsTo 120708e0 -> FUN_120c9cc0 (global init)
ListFuncs 12070000..12073000 -> 13 функций trade-экрана
DumpData 120f88f8/120f8920/120f8948 -> x=347..895 (10 слотов)
DumpData 120f8970 -> x=140..811 (12 слотов)
```

### 7.3 Диалог/native

`glscript` (boat trader, `--answer 0`):

```
status: wait_user
dialogs: 8868 3498 4214
answers: 2
status: end
[info] script native RS_PassToTradePanel (0x700000D): unimplemented - requires quest/event runtime
```

`glgame --level l13_1 --click-npc 3 --equip-save 0 --seed 1 --no-font`:

```
level 'l13_1': 8 persons
npc inventory: level 'l13_1', loot level 41, populated 3/8
dialog: 'scripts/dialogs/l13_1/l13_1.p517_boat_trader.d517.age.cs'
        speaker='L13_1.P517_Boat_Trader' answers=2
```

### 7.4 Валидация Python-референса

Пуск на `l1_1`, loot_level 1, seed 1 воспроизводит принятый 1b2b2b
oracle: `L0.P7_Kolbasens_Rodstvennik1` main = две записи
`MON_1_0_1` по 87 (сумма **174**), `L1_1.P387_Poison_Plotnik` main =
`QST_1_0_5=1`. Для `l13_1` Python даёт `populated 3/8` (P239/P707/P519),
совпадающее с логом `glgame` §7.3, значит RNG-поток, включая pass 1
P517, совпадает с портом.

## 8. Unverified / missing evidence

1. **Filter type grouping для trade-списков.** Кнопки/mask/list/scroll
   доказаны; item-type → bit для NPC-ряда не найден (inventory-группировка
   `FUN_120626F4` — только кандидат). Read-only overlay может показывать
   все товары; фильтры — rendering/selection по доказанным маскам, без
   паритетного скрытия типов до доказательства.
2. **Источник id `local_player+0x6F8/+0x6FC`** (staging A/B). Сеттер
   `0x120C33D8` найден; caller/сетевой пакет — нет. Для read-only
   верхнего ряда/hero main не нужен.
3. **Modifier-блок цены** (`rec+0x78`/`+0x7C`) для неопознанных предметов:
   добавляет стоимость модификаторов; точные поля не декодированы.
   Свежий NPC-лут имеет `+0x78=0`; hero-предметы — проверить.
4. **Exit-кнопка → команда:** `0x2B` ведёт к закрытию (screen 0xD);
   точная привязка obj 7 не трассирована.
5. **Live-оракул** (память запущенной игры) не снимался; oracle —
   save + SEF/INV/ITM + независимый Python-референс + реальные `.cs`.
6. **Production seed** недетерминирован (`timeGetTime`); oracle-значения
   выше — для `--seed 1` (как принято в 1b2b2b). В реальной игре список
   NPC будет другим (тот же алгоритм, другой seed).
7. `weapon_slot1`/`+0x22C` (набор B) и paper-doll overlays — не часть
   read-only списка; не проверялись.

## 9. Следствия для Task 2

1. `NpcLootGenerator`/`generate_npc_inventories` — сохранять pass 1
   (`loot.hot`) как NPC trade container (`entity+0xC`); RNG уже корректен.
2. `InventoryRuntime`: accessor NPC trade items + money; hero main/hot.
3. `ItemCatalog`/save: добавить `price(+0x10)` и `durability(+0xB4)`
   (и, при необходимости, modifiers) в runtime-запись для точной цены;
   иначе показывать ITM-price с durability 100 (documented deviation).
4. `TradeLayout`: §2.3 константы (ряды, x-массивы, y, 55×55, filters,
   money y, exit/scroll).
5. `NativeServices::trade_panel` + handler 0x700000D → 0.0; `glgame`
   закрывает диалог и открывает `TradeOverlay` (NPC speaker).
6. Read-only: верх = NPC trade container (buy price), низ = hero main
   (sell price), hot/equip — опционально; деньги NPC/hero; без
   buy/sell/transfer/trade-мутаций.
