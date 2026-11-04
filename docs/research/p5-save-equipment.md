# P5a: equipped-записи `BT_HERO` в save-файлах

Дата: 2026-11-04. Статус: **research DONE** (layout извлечён и проверен на 28 save).
Задача: `docs/plans/2026-11-04-p5a-save-outfit-research.md`, Task 1.
Игровой код не менялся; коммитится только этот документ.

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

## 10. Что ещё нужно для item → tech/group

Уже подтверждено:

- `+0x08` type → категория (`11` helmet, `12` armor, `13` shield, `14` elbarms;
  `15/16/17` bracelets/amulet/ring; `0/1/…` оружие).
- `+0x20` = ITM `unknown_b[1]` (style-номер): `318→13`, `280→12`, `397→11`.
- `+0xA8` = id файла `items/data/<id>.itm`; ITM даёт `puppet_name` (например,
  `puppet\Bronya\Almaz\00.bmp` для 318) и `material`.
- `+0x30` = ITM `material`.

Не извлечено (следующий шаг):

1. Точная сборка tech-строки (`ARM_1_13_1`, `HLM_1_12_1`, `ELB_1_11_*`): какой
   компонент даёт последнее число/вариант и как выбирается группа.
2. Функция, превращающая item id из `FUN_140499ac` в PersonVisual. В reader
   после `FUN_140499ac` вызывается vtable-метод
   `DAT_140891bc+0x98 → +0x0C(local_48, hero+8)`; это наиболее вероятная точка
   item→tech/group, но её реализация не декомпилирована.
3. Сопоставление tech/group с `scripts/herowear.scr` (и/или `puppet_name`) для
   5 визуальных категорий.
4. Выбор активного набора A/B (`param_2`/`param_3`), если он не всегда первый.

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
   по §10.

## 12. Открытые вопросы

- Семантика `+0xAC` (константа на слот) и subrecord (5×u32).
- Активный weapon/shield set A/B и момент переключения.
- Точная функция item id → tech/group и tech → wear set.
- Являются ли mask-биты аксессуаров (`0x008000/0x010000/0x020000/0x040000/0x080000`)
  именно слотами: в `FUN_140499ac` они не обрабатываются (визуальных слоёв нет),
  связь установлена только по совпадению с type 15/16/17.
