# P5a: equipped-записи `BT_HERO` в save-файлах

Дата: 2026-11-04. Статус: **research DONE** (layout + item→tech/group;
проверено на 28 save). Задача: `docs/plans/2026-11-04-p5a-save-outfit-research.md`,
Task 1-2. Игровой код не менялся; коммитится только этот документ.

## 1. Источники истины

| Файл | SHA-256 | Image base | Основной код |
|---|---|---|---|
| `GoldenLand.exe` | `7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF` | `0x00400000` | загрузка слота/заголовка |
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` | `0x14000000` | запись/чтение `BT_HERO` |

Сохранения: 28 файлов `Saves/slot0.sav` … `slot27.sav`.

### Важная поправка к spec/plan

- Строки `BT_HERO`/`BT_SCRN` находятся **только в `Server.dll`**
  (`0x1408ffac` / `0x1408ff64`), а не в `GoldenLand.exe`.
- В `GoldenLand.exe` есть только `"Goldenland II save "` (`0x00483308`) и
  `"Saves\slot%d.sav"` (`0x0048331c`); `FUN_0041d504` читает 0x68-байтный
  заголовок и проверяет версию `0x11`. Полный body пишет/читает `Server.dll`.
- Поэтому адреса из spec (`0x1401e690`, `0x1404f4bc`, `0x1401e340`) **корректны
  для `Server.dll`** (base `0x14000000`) и не должны искаться в образе exe.
  В Ghidra-проекте `GoldenLand.exe` они действительно не резолвятся.

## 2. Ghidra: функции записи/чтения

Все адреса — VA; RVA = VA − image base. Проект: `%TEMP%\ghidra_server` (`Server.dll`).

| VA | RVA | Роль | Как найдено |
|---|---|---|---|
| `0x14031b64` | `0x31b64` | writer целого save: `BT_SCRN…BT_HERO`, затем header | ref на `BT_HERO` @ `0x14031dba` |
| `0x1403210c` | `0x3210c` | reader целого save | ref на `BT_HERO` @ `0x140324cc` |
| `0x1401e690` | `0x1e690` | writer hero: fixed header + 3 контейнера + `FUN_1401d84c` | вызов из save writer @ `0x14031e59` |
| `0x1405c9d8` | `0x5c9d8` | writer контейнера: u32 id, затем `FUN_1404c7a8` | вызовы из `FUN_1401e690` |
| `0x1404c7a8` | `0x4c7a8` | writer item-контейнера: u32 count + записи | вызов из `FUN_1405c9d8` |
| `0x1404f4bc` | `0x4f4bc` | writer item-записи: 0xC0 raw + u32 subcount + `0x14*subcount` | вызов из `FUN_1404c7a8` |
| `0x1401d84c` | `0x1d84c` | writer: 4×0x20 + u32 count + `0x14*count` | вызов из `FUN_1401e690` |
| `0x1401e340` | `0x1e340` | отдельный список (count+items), конец `BT_HERO` | вызов из save writer @ `0x14031e67` |
| `0x1401e424` | `0x1e424` | reader hero (зеркало `0x1401e690`) | вызов из save reader |
| `0x1401d8bc` | `0x1d8bc` | reader-зеркало `0x1401d84c` | вызов из `FUN_1401e424` |
| `0x140499ac` | `0x499ac` | разбор equipped-контейнера в 2 набора item id по маскам | вызов из `FUN_1401e424` |
| `0x0041d504` | `0x1d504` | exe: чтение заголовка `Saves\slot%d.sav` | ref на `"Goldenland II save "` |

Цепочка writer: `FUN_14031b64 → FUN_1401e690 → {FUN_1405c9d8→FUN_1404c7a8→FUN_1404f4bc, FUN_1401d84c} → FUN_1401e340`.

## 3. `BT_HERO` в decompressed body

Формат блока: 8-байтный тег (`BT_HERO ` — с пробелом, **без NUL**) + u32 size;
тело начинается с `+12`.

`slot0.sav`:

| Параметр | Значение |
|---|---|
| file size | 642409 |
| header | 112 байт, zlib с offset 112 |
| decompressed length | 905051 |
| `BT_HERO` offset (decompressed) | 874651 (`0xD589B`) |
| block size | 30388 (`0x76B4`) |
| body start / end | 874663 / 905051 |
| body size | 30388 = block size |

`BT_HERO` — последний блок в slot0, поэтому body заканчивается ровно на конце
decompressed-потока.

`slot2.sav` (второй контрольный слот, title `go end local`):

| Параметр | Значение |
|---|---|
| file size | 630410 |
| decompressed length | 910169 |
| `BT_HERO` offset (decompressed) | 875145 (`0xD5A89`) |
| block size | 35012 (`0x88C4`) |
| body start | 875157 |

## 4. Layout тела `BT_HERO`

Фиксированный заголовок (body-relative):

| offset | size | смысл |
|---|---|---|
| `0x000` | 12 | 3×u32 (в slot0: `123, 131, 1`) |
| `0x00C` | `0x11C` | stat/hero block (копируется в `hero+0xFF0`) |
| `0x128` | 12 | 3×u32 (в slot0: `263, 0, 5`) |
| `0x134` | `0x600` | блок 0x600 (копируется в `hero+0x1510`) |
| `0x734` | … | **3 item-контейнера** |
| после контейнеров | 8 | 2×u32 (slot0: `0, 0`) |
| далее | `0x80` | 4×0x20 (записывает `FUN_1401d84c`) |
| далее | 4 | u32 extra_count |
| далее | `0x14*extra_count` | extra-записи (у 27 слотов 0; у slot16 1) |
| далее | 4 | u32 list_count (`FUN_1401e340`) |
| далее | `list_count × item record` | отдельный список; **во всех 28 save list_count = 0** |
| конец | | ровно `body+size` |

Контейнеры (в порядке записи `hero+0x0C`, `hero+0x10`, `hero+0x109C`):

| # | hero offset | формат | slot0 | slot2 | смысл (наблюдение) |
|---|---|---|---|---|---|
| 0 | `+0x0C` | u32 id, u32 count, count×record | 8 | 9 | расходники/рецепты и т.п. |
| 1 | `+0x10` | u32 id, u32 count, count×record | 117 | 140 | инвентарь |
| 2 | `+0x109C` | u32 id, u32 count, count×record | **11** | **11** | **equipped-набор** |

У всех контейнеров id = `1`; count может быть 0 (например, slot21: `0 / 2 / 0`).
Контейнер №2 — именно экипировка: его читает `FUN_1401e424` в `hero+0x109C`,
затем `FUN_140499ac` раскладывает item id по слотам; в slot0 там лежат
подтверждённые пользователем armor/elbarms/helmet `318/397/280`.

## 5. Item record

`FUN_1404f4bc` пишет «сырой» in-memory item (0xC0 байт), затем u32 subcount,
затем `subcount × 0x14` (20) байт.

```
record_size = 0xC0 + 4 + subcount * 0x14
```

> **Поправка:** в plan/brief указано `12*subcount`. В бинарнике и в реальных
> данных stride = `0x14` (20), а не 12. Проверено на 1650 записях: конец каждой
> записи совпадает со следующей записью/концом контейнера.

Поля записи (offset от начала record):

| offset | size | поле | Проверка |
|---|---|---|---|
| `0x000` | u32 | runtime class/vtable ptr (`0x14092248` в этих save) | не семантика save |
| `0x004` | u32 | 0 |  |
| `0x008` | u32 | **type** = ITM `type` | сверено с ITM |
| `0x00C` | u32 | **flags / equip mask** (биты 12..23) | см. §6 |
| `0x010` | u32 | price = ITM `price` | сверено |
| `0x014` | u32 | weight_x10 = ITM `weight_x10` | сверено |
| `0x018` | u32 | unknown_a = ITM `unknown_a` | сверено |
| `0x020` | u32 | **style** = ITM `unknown_b[1]` | сверено на 22 item |
| `0x030` | u32 | material = ITM `material` | сверено |
| `0x0A4` | u32 | instance uid (последовательный в слоте) | 157…167 в slot0 |
| `0x0A8` | u32 | **item id** = ITM `description_id` / `items/data/<id>.itm` | `318/397/280` |
| `0x0AC` | u32 | константа слота (slot0=14, slot2=656); смысл не установлен |  |
| `0x0B0` | u32 | stack count (в equipped = 1) |  |
| `0x0B4` | u32 | durability/condition (100, реже 91/95) |  |
| `0x0C0` | u32 | **subcount** |  |
| `0x0C4` | `0x14*subcount` | subrecords: 5×u32, семантика не разобрана (похоже на effect/modifier) |  |

`item id` всегда лежит по `record + 0xA8`; начало записи = `item_id_offset − 0xA8`.

## 6. Equip-маска `+0x0C`

`FUN_140499ac` берёт `mask = *(item + 0x0C) & 0xFFF000` и раскладывает item id
в два набора `param_2`/`param_3` (по 6 слотов):

| mask | куда | категория (по типу/наблюдению) |
|---|---|---|
| `0x001000` | `param_2[1]` | weapon, набор A |
| `0x002000` | `param_3[1]` | weapon, набор B |
| `0x004000` | `param_2[4]` и `param_3[4]` | helmet (type 11) |
| `0x100000` | `param_2[3]` и `param_3[3]` | armor (type 12) |
| `0x200000` | `param_2[2]` и `param_3[2]` | elbarms/leggings (type 14) |
| `0x400000` | `param_2[5]` при type==13 | shield, набор A |
| `0x800000` | `param_3[5]` при type==13 | shield, набор B |
| `0x008000` | не используется `FUN_140499ac` | amulet (type 16) |
| `0x010000`, `0x020000` | не используется | rings (type 17) |
| `0x040000`, `0x080000` | не используется | bracelets (type 15) |

`param_2`/`param_3` — два набора оружия/щита (A/B); armor/helmet/elbarms
общие для обоих. Reader кладёт результат первого набора в `hero+0x5E0`;
логика переключения активного набора в этой задаче не прослежена.

## 7. Slot0: equipped-записи (body-relative)

`record_start`, id, type, style (+0x20), material, mask, subcount:

| record | id | type | style | material | mask | sub |
|---|---|---|---|---|---|---|
| `0x6B10` | 72 | 15 bracelet | 11 | 9 | `0x040000` | 7 |
| `0x6C60` | **397** | 14 elbarms | 11 | 9 | `0x200000` | 2 |
| `0x6D4C` | **280** | 11 helmet | 12 | 3 | `0x004000` | 1 |
| `0x6E24` | **318** | 12 armor | 13 | 0 | `0x100000` | 3 |
| `0x6F24` | 80 | 15 bracelet | 12 | 3 | `0x080000` | 2 |
| `0x7010` | 233 | 17 ring | 10 | 10 | `0x020000` | 6 |
| `0x714C` | 554 | 0 sword | 8 | 6 | `0x002000` | 4 |
| `0x7260` | 141 | 16 amulet | 7 | 5 | `0x008000` | 1 |
| `0x7338` | 486 | 1 axe | 13 | 0 | `0x001000` | 4 |
| `0x744C` | 353 | 13 shield | 10 | 10 | `0x800000` | 1 |
| `0x7524` | 245 | 17 ring | 14 | 11 | `0x010000` | 3 |

Item-id offsets: `397` @ body `0x6D08` (decomp `902575`), `280` @ `0x6DF4`
(`902811`), `318` @ `0x6ECC` (`903027`). Это ровно `record + 0xA8`.

## 8. Slot2: equipped-записи (body-relative)

| record | id | type | style | material | mask | sub |
|---|---|---|---|---|---|---|
| `0x7DFC` | 486 | 1 axe | 13 | 0 | `0x001000` | 4 |
| `0x7F10` | 359 | 13 shield | 12 | 3 | `0x800000` | 2 |
| `0x7FFC` | 554 | 0 sword | 8 | 6 | `0x002000` | 4 |
| `0x8110` | 66 | 15 bracelet | 10 | 10 | `0x040000` | 1 |
| `0x81E8` | 278 | 11 helmet | 11 | 9 | `0x004000` | 2 |
| `0x82D4` | 317 | 12 armor | 12 | 3 | `0x100000` | 2 |
| `0x83C0` | 108 | 16 amulet | 2 | 4 | `0x008000` | 1 |
| `0x8498` | 396 | 14 elbarms | 11 | 9 | `0x200000` | 3 |
| `0x8598` | 99 | 15 bracelet | 15 | 12 | `0x080000` | 1 |
| `0x8670` | 236 | 17 ring | 11 | 9 | `0x010000` | 2 |
| `0x875C` | 243 | 17 ring | 14 | 11 | `0x020000` | 1 |

## 9. Проверка 0/N

| Проверка | N | Расхождений |
|---|---|---|
| slot0: sequential parse всех item record, конец = следующий record / конец body | 136 | **0** |
| slot2: то же | 160 | **0** |
| все 28 save: sequential parse, финальный offset = размер body | 1650 | **0** |
| все save: начало каждого распознанного record содержит маркер `0x14092248` | 1650 | **0** |
| equipped-записи (контейнер №2) во всех слотах | 216 | **0** |
| slot0+slot2: поля item vs `items/data/<id>.itm` (type, style, price, weight, unknown_a, material) | 132 | **0** |

Дополнительно: в slot24/slot25 байты `48 22 09 14` встречаются ещё по одному разу
внутри subrecord (`slot24 @ body 0x1390`, `slot25 @ body 0x12CC`) — это не начала
записей; sequential parse от них не зависит. Во всех 28 файлах count/id/концы
контейнеров сходятся без остатка.

## 10. Item → tech/group — решено (детали в §13)

Task 2 закрыт. Коротко:

- tech-строка (`ARM_1_13_1`, `HLM_1_12_1`, `ELB_1_11_2`, …) берётся из
  `sdb/items/tech_names.sdb` по item id; она не собирается из
  `type`/`style`/`material`.
- группа и wear-набор — из `scripts/herowear.scr`;
  `+0x20 style` коррелирует со 2-м числом tech, но движок его для выбора
  группы не читает.
- активный при чтении save набор — A; packed-формула, id групп и проверка
  0/N — в §13.

## 11. Рецепт парсера для P5a

1. Проверить save header (112 байт), распаковать zlib с offset 112.
2. Найти тег `b"BT_HERO "`; прочитать u32 size; взять body `[off+12, off+12+size)`.
3. Пропустить `0x734` байт фиксированного заголовка.
4. Прочитать 3 контейнера: `u32 id`; если `id != 0xFFFFFFFF`, `u32 count`, затем
   `count` item record по формуле `0xC0 + 4 + u32(+0xC0)*0x14`.
5. Взять третий контейнер (`hero+0x109C`) — equipped-записи.
6. Для каждой записи: `item_id = u32(+0xA8)`, `type = u32(+0x08)`,
   `style = u32(+0x20)`, `mask = u32(+0x0C) & 0xFFF000`.
7. Категория для outfit — по `type` (и `mask` для набора A/B); item→tech/group —
   по §13.

## 12. Открытые вопросы / residual risk

- Переключение активного набора A/B в live-игре после загрузки save не
  прослежено: reader использует только A (§13.6). Это residual risk для P5a.
- Семантика `+0xAC` (константа на слот) и subrecord (5×u32) не разобрана
  (к outfit не относится).
- Групповые id зависят от порядка `group` в `herowear.scr`; их нельзя
  хардкодить — парсить из данных (§13.3).
- Item id вне `tech_names.sdb` или TECH вне `herowear.scr` для визуальной
  категории → naked-fallback по spec; в 28 save таких записей нет.
- Mask-биты аксессуаров (`0x008000/0x010000/0x020000/0x040000/0x080000`)
  по-прежнему не обрабатываются `FUN_140499ac` (визуальных слоёв нет);
  связь с type 15/16/17 подтверждена только по совпадению.
- `shlem_cherny` из spec в данных отсутствует; см. §13.8.

## 13. Item → tech/group → wear set (Task 2)

Источник: `Server.dll` SHA-256
`418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`,
`GoldenLand.exe` SHA-256
`7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF`.
Проекты Ghidra: `%TEMP%\ghidra_server`, `%TEMP%\ghidra_glexe`.

### 13.1 Цепочка вызовов

| VA | RVA | Модуль | Роль |
|---|---|---|---|
| `0x1401e424` | `0x1e424` | Server.dll | reader hero: `FUN_140499ac` → vtable `+0x98` → `+0x0C` |
| `0x140499ac` | `0x499ac` | Server.dll | разбор equipped в наборы A (`param_2`) и B (`param_3`) |
| `0x1405cbf0` | `0x5cbf0` | Server.dll | `GetServerAPI`: `DAT_140891bc = arg1` |
| `0x0042c624` | `0x2c624` | GoldenLand.exe | загрузчик DLL; вызов `GetServerAPI(0x46c200, 0x46d820)` @ `0x42c6bd` |
| `0x0042c350` | `0x2c350` | GoldenLand.exe | init объекта `0x46c200`: `+0x88→DAT_00469d58` @ `0x42c604`, `+0x98→DAT_0046c1e8` @ `0x42c497` |
| `0x0042ca7c` | `0x2ca7c` | GoldenLand.exe | создаёт оба singleton-объекта |
| `0x0042c098` | `0x2c098` | GoldenLand.exe | ctor visual factory (size `0x54`; vtable `0x4852d8`/`0x4852fc`) |
| `0x004255bc` | `0x255bc` | GoldenLand.exe | ctor wear manager (size `0xC460`; vtable `0x484400`) |
| `0x004253ac` | `0x253ac` | GoldenLand.exe | создаёт 5 пустых SDB + карты, затем парсит herowear |
| `0x00422eec` | `0x22eec` | GoldenLand.exe | парсер `scripts\herowear.scr` |
| `0x00422e70` | `0x22e70` | GoldenLand.exe | vtable `0x484400[0]`: weapon id → group id → base set idx |
| `0x00423888` | `0x23888` | GoldenLand.exe | vtable `0x484400[2]`: сборка packed visual-кода |
| `0x00423244` | `0x23244` | GoldenLand.exe | обратная распаковка packed → массив item id |
| `0x0042bd34` | `0x2bd34` | GoldenLand.exe | vtable `0x4852d8[3]`: вызывает `0x423888`, пишет packed в `hero+8`, возвращает visual-объект |

`DAT_140891bc` — 32-битный указатель на объект `GoldenLand.exe`
`0x46c200`. `[0x46c298] = DAT_0046c1e8`; у этого объекта vtable
`0x4852d8`, метод `[3]` = `FUN_0042bd34`. `[0x46c288] = DAT_00469d58`
(wear manager, vtable `0x484400`). Поэтому `FUN_140499ac` в хвосте
вызывает `FUN_00422e70`, а reader — `FUN_0042bd34`.

### 13.2 Tech-строка: источник данных, не формула

`FUN_00422eec` на строке `item "<TECH>"` ищет TECH в
`DAT_0046c358` — SDB `sdb\items\tech_names.sdb` (загружается в
`FUN_0042ca7c`; строка пути @ `0x485cd4`), получает u32 item id и
кладёт `map[item_id] = group_id` (`FUN_004027e0`).

Значит `item id → TECH` — это та же таблица в обратном направлении
(`glpak decode-sdb` печатает `<id>\t<TECH>`), а не сборка из
`+0x08 type`, `+0x20 style`, `+0x30 material`:

| item id | TECH | группа | gid |
|---|---|---|---|
| 318 | `ARM_1_13_1` | Dospeh_almaz | 1 |
| 397 | `ELB_1_11_2` | Ponogi_goluboy | 3 |
| 280 | `HLM_1_12_1` | Shlem_goluboy | 6 |
| 317 | `ARM_1_12_2` | Dospeh_cherny | 5 |
| 278 | `HLM_1_11_1` | Shlem_goluboy | 6 |
| 396 | `ELB_1_11_1` | Ponogi_goluboy | 3 |
| 486 | `AXE_1_13_2` | Axe_goluboy2 | 31 |
| 554 | `SWR_1_08_1` | Sword_platina | 69 |
| 353 | `SHL_1_10_1` | Shit_bio | 2 |
| 359 | `SHL_1_12_2` | Shit_cherny | 5 |

`+0x20 style` совпадает со 2-м числом TECH: `318→13`, `397→11`,
`280→12`, `486→13`, `554→8`, `353→10`, `359→12`. Это проверено
на всех 22 equipped-записях slot0+slot2 (0 расхождений) и на 113
визуальных записях 28 save (0). Но движок `style` при выборе группы
не читает: `FUN_00423888` использует только item id. Исключение —
ammo `AMO_1_0_1` (type 8): у него TECH не следует шаблону и нет
wear-слоя.

### 13.3 Group id и wear group

`FUN_004253ac` создаёт по категории пустую SDB (без загрузки файла)
и карту. Порядок категорий — массив `0x445260`:
`weapon(0)`, `armor(1)`, `shield(2)`, `helmet(3)`, `elbarms(4)`.

`FUN_00422eec` при первом вхождении `group "<G>"` выдаёт
`id = число_записей_SDB + 1`, повторное имя переиспользует id.
Карта `item id → group id` лежит в wear manager по
`+0x18 + 4*category_index`.

Число групп в текущем `herowear.scr`: weapon 88, armor 15,
shield 15, helmet 8, elbarms 5. id 0 = категория отсутствует
(в packed остаются нули).

### 13.4 Base wear set

Только у `category weapon` у группы есть второй аргумент. Массив
`0x4452ac` (порядок проверен по бинарнику):

```
0 noweapon_thrw   1 onehand_wpn   2 two_swords   3 two_axes_maces
4 two_spears_staffs   5 bows   6 xbows_guns
```

Индекс base set сохраняется в карту `+0x2c` по group id;
`FUN_00422e70` возвращает его для оружия, иначе 0. Порядок в
p4d (§herowear) был указан неверно.

### 13.5 Packed visual-код

`FUN_00423888` собирает u32 (bit 31 — флаг packed):

```
packed = 0x80000000
       | (armor_gid   & 0x0F)
       | (shield_gid  & 0x0F) << 4
       | (helmet_gid  & 0x0F) << 8
       | (elbarms_gid & 0x0F) << 12
       | (weapon_gid  & 0xFF) << 16
       | (base_idx    & 0x0F) << 24
```

Нет предмета → gid 0; нет оружия → `base_idx = 0`. Обратная
распаковка — `FUN_00423244` (для чтения save не нужна: item id уже
есть в записи).

### 13.6 Активный набор A/B

`FUN_140499ac`:

| mask | набор | категория |
|---|---|---|
| `0x001000` | A (`param_2`/`local_48`) | weapon |
| `0x002000` | B (`param_3`/`local_60`) | weapon |
| `0x004000` | A и B | helmet |
| `0x100000` | A и B | armor |
| `0x200000` | A и B | elbarms |
| `0x400000` + type 13 | A | shield |
| `0x800000` + type 13 | B | shield |

`FUN_1401e424` передаёт в `FUN_0042bd34` только `local_48`; `local_60`
не используется. `FUN_0042bd34` пишет packed в `hero+8` и кладёт
возвращённый visual-объект в `hero+0x5E0`. Следствие: при чтении save
активен набор A. Переключение A/B в live-игре после загрузки не
прослежено — residual risk (§12).

Для slot0/slot2 packed: A `0x831f3601`/`0x831f3605`,
B `0x81453621`/`0x81453655` (порядок: armor, shield, helmet,
elbarms, weapon, base_idx).

### 13.7 Проверка slot0 + slot2

Все визуальные слои (A и B, 18 путей) существуют в `wear.pak`:

| slot | set | cat | id | TECH | group | gid | base | asset |
|---|---|---|---|---|---|---|---|---|
| 0 | A | weapon | 486 | AXE_1_13_2 | Axe_goluboy2 | 31 | two_axes_maces | `wear/two_axes_maces/weapon/axe_goluboy2/axe_goluboy2.iad` |
| 0 | A | armor | 318 | ARM_1_13_1 | Dospeh_almaz | 1 | two_axes_maces | `wear/two_axes_maces/armor/dospeh_almaz/dospeh_almaz.iad` |
| 0 | A | helmet | 280 | HLM_1_12_1 | Shlem_goluboy | 6 | two_axes_maces | `wear/two_axes_maces/helmet/shlem_goluboy/shlem_goluboy.iad` |
| 0 | A | elbarms | 397 | ELB_1_11_2 | Ponogi_goluboy | 3 | two_axes_maces | `wear/two_axes_maces/elbarms/ponogi_goluboy/ponogi_goluboy.iad` |
| 0 | B | weapon | 554 | SWR_1_08_1 | Sword_platina | 69 | onehand_wpn | `wear/onehand_wpn/weapon/sword_platina/sword_platina.iad` |
| 0 | B | armor | 318 | ARM_1_13_1 | Dospeh_almaz | 1 | onehand_wpn | `wear/onehand_wpn/armor/dospeh_almaz/dospeh_almaz.iad` |
| 0 | B | shield | 353 | SHL_1_10_1 | Shit_bio | 2 | onehand_wpn | `wear/onehand_wpn/shield/shit_bio/shit_bio.iad` |
| 0 | B | helmet | 280 | HLM_1_12_1 | Shlem_goluboy | 6 | onehand_wpn | `wear/onehand_wpn/helmet/shlem_goluboy/shlem_goluboy.iad` |
| 0 | B | elbarms | 397 | ELB_1_11_2 | Ponogi_goluboy | 3 | onehand_wpn | `wear/onehand_wpn/elbarms/ponogi_goluboy/ponogi_goluboy.iad` |
| 2 | A | weapon | 486 | AXE_1_13_2 | Axe_goluboy2 | 31 | two_axes_maces | `wear/two_axes_maces/weapon/axe_goluboy2/axe_goluboy2.iad` |
| 2 | A | armor | 317 | ARM_1_12_2 | Dospeh_cherny | 5 | two_axes_maces | `wear/two_axes_maces/armor/dospeh_cherny/dospeh_cherny.iad` |
| 2 | A | helmet | 278 | HLM_1_11_1 | Shlem_goluboy | 6 | two_axes_maces | `wear/two_axes_maces/helmet/shlem_goluboy/shlem_goluboy.iad` |
| 2 | A | elbarms | 396 | ELB_1_11_1 | Ponogi_goluboy | 3 | two_axes_maces | `wear/two_axes_maces/elbarms/ponogi_goluboy/ponogi_goluboy.iad` |
| 2 | B | weapon | 554 | SWR_1_08_1 | Sword_platina | 69 | onehand_wpn | `wear/onehand_wpn/weapon/sword_platina/sword_platina.iad` |
| 2 | B | armor | 317 | ARM_1_12_2 | Dospeh_cherny | 5 | onehand_wpn | `wear/onehand_wpn/armor/dospeh_cherny/dospeh_cherny.iad` |
| 2 | B | shield | 359 | SHL_1_12_2 | Shit_cherny | 5 | onehand_wpn | `wear/onehand_wpn/shield/shit_cherny/shit_cherny.iad` |
| 2 | B | helmet | 278 | HLM_1_11_1 | Shlem_goluboy | 6 | onehand_wpn | `wear/onehand_wpn/helmet/shlem_goluboy/shlem_goluboy.iad` |
| 2 | B | elbarms | 396 | ELB_1_11_1 | Ponogi_goluboy | 3 | onehand_wpn | `wear/onehand_wpn/elbarms/ponogi_goluboy/ponogi_goluboy.iad` |

0/N:

| Проверка | N | Расхождений |
|---|---|---|
| slot0+slot2: item id → TECH | 22 | **0** |
| slot0+slot2: TECH prefix category == save `type` | 22 | **0** |
| slot0+slot2: `style == TECH[2]` | 22 | **0** |
| slot0+slot2: визуальный TECH найден в herowear | 12 | **0** |
| slot0+slot2: gid влезает в 4/8-битные поля (4 набора × 5) | 20 | **0** |
| slot0+slot2: asset существует (A+B) | 18 | **0** |
| все 28 save: визуальные item → TECH/herowear | 113 | **0** |
| все 28 save: asset визуальных слоёв (A+B) | 193 | **0** |

### 13.8 Отсутствующие ассеты

- В slot0/slot2 отсутствующих ассетов нет: 18/18 путей существуют.
- В 28 save — 0 отсутствующих среди 193 проверенных визуальных слоёв.
- Структурно в `wear.pak` каталог `shield/` есть только у base set
  `noweapon_thrw` и `onehand_wpn` (по 15 групп); у `two_swords`,
  `two_axes_maces`, `two_spears_staffs`, `bows`, `xbows_guns` его
  нет вовсе. Это согласуется с тем, что щит носится с одноручным
  оружием; в slot0/slot2 щиты лежат в наборе B при `onehand_wpn`.
  Если встретится щит при двуручном base set — слой считать
  отсутствующим, а не ошибкой парсера.
- `shlem_cherny` нет ни в `wear.pak`, ни в `herowear.scr`;
  `HLM_1_12_1`/`HLM_1_11_1` → `Shlem_goluboy` (ассет есть). Пример
  из spec не воспроизводится на этих save.

### 13.9 Рецепт для P5a

1. Из equipped-записи взять `id=+0xA8`, `type=+0x08`,
   `mask=+0x0C & 0xFFF000`.
2. Собрать набор A (§6, §13.6).
3. `TECH = reverse(tech_names.sdb)[id]` (id → строка).
4. `(category, group, gid, base_idx) = herowear[TECH]`.
5. `base = weapon ? base_idx : 0` (`noweapon_thrw`).
6. `asset = wear/<base>/<category>/<lower(group)>/<lower(group)>.iad`.
7. Нет TECH/group/asset → naked для категории + warning (spec §8).
8. `BRL`/`AML`/`RNG` и ammo (`AMO`, type 8) читать/хранить, но
   wear-слой не создавать.
