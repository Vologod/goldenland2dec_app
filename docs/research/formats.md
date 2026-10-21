# Исследование: форматы данных

Дата: 2026-10-19. Вторичные источники: `fersatgit/Goldenland-2-Cold-Heaven` (README,
Kaitai, XnView plugin), `DarkContact/GoldenLandEditor`, `AlexKimov/goldenland-file-formats`.
Все факты подлежат проверке на файлах целевой сборки в P0.

## Кодировки

- Строки в данных — cp1251 (Windows-1251). ASCII-имена путей — ASCII.
- На границе API — UTF-8.

## PAK

```
magic      : 8 байт  = "PAK \0\0\0\0"
count      : uint32  (little-endian)
FAT[count] : {
  name_len : uint32
  name     : name_len байт, cp1251, путь с обратными слэшами
  size     : uint32  (распакованный размер данных)
  offset   : uint32  (абсолютное смещение от начала файла)
}
```

Данные по `offset`:

```
mode         : uint32  (0 — raw, иначе — zlib)
if mode == 0:
    data     : raw, ровно `size` байт
else:
    stored   : uint32  (длина zlib-потока в архиве)
    data     : zlib-поток, разворачивается в `size` байт
```

Проверено 2026-10-19 на всех `.pak` в `I:\Games\Златогорье-2\Data` (mode 0 / 1,
`offset + заголовок + data` не выходит за файл): `scripts.pak` count `0x1185` (4485),
`sdb.pak` 45, `items.pak` 3786, `engineres.pak` `0x411` (1041). Сумма `size` по `items.pak`
совпадает с распакованным итогом 28 847 595.

## CSX (спрайт с палитрой)

```
color_count      : uint32
transparent_color: uint32 (BGR)
palette          : color_count * uint32 (BGR)
width            : uint32
height           : uint32
row_offsets      : (height + 1) * uint32 (смещения строк в compressed_data;
                    последний элемент = размер compressed_data)
compressed_data  : RLE по строкам
```

Точный RLE — в `XnView plugin/Xcsx.dpr`; проверить и покрыть тестами.

Проверено 2026-10-19 на целевой сборке: 19 704 непустых `.csx` декодируются,
максимум 1400×41 700 = 58 380 000 пикселей; одна пустая запись
(`levels/pack/tl41/bitmaps/minimap.csx`, size 0).

## SDB (строковая база)

```
signature : uint32 = "SDB "  (может отсутствовать)
entries   : {
  id    : uint32
  value : string  (uint32 len + cp1251 bytes; len==0 — пустая строка)
}
```

Если сигнатуры нет — текстовые байты зашифрованы XOR `0xAA`.

## ITM (предмет)

```
magic        : uint32 = 4
description_id : uint32 (id в sdb\items\descriptions.sdb)
item_type    : uint32 (0..28)
item_flags   : uint32 (битовые флаги)
max_stack    : uint32
puppet_name  : string (путь к bmp)
icon_name    : string (путь к bmp)
unknown_qword: uint64 (0 или 1)
price        : uint32
weight_x10   : uint32
...
```

Полный список полей и условных блоков — `fersatgit/Goldenland-2-Cold-Heaven` README
(раздел ITM). Парсер должен корректно пропускать отсутствующие опциональные блоки.

## LVL (уровень)

Подтверждено на всех 175 `levels/lvl/*.lvl` из `levels.pak` (`I:\Games\Златогорье-2`):
12 блоков строго в порядке `BLK_LVER`, `BLK_MPSZ`, `BLK_MHDR`, `BLK_MDSC`, `BLK_SDSC`,
`BLK_ADSC`, `BLK_TDSC`, `BLK_CGRP`, `BLK_SENV`, `BLK_WTHR`, `BLK_DOOR`, `BLK_LFLS`.
Заголовок блока: 8 байт ASCII-имени + `u32 size` (LE), затем payload ровно `size` байт;
лишних байт после `BLK_LFLS` нет. `BLK_LVER` = `u16 major=3`, `u16 minor=1` во всех файлах.

- `BLK_MHDR`: `u32 chunkW`, `u32 chunkH`; `size == 8 + chunkW*chunkH*24` (чанк = 4 тайла
  по 6 байт: `u16 relief`, `u16 sound`, `u16 mask`, маска `0xFFFF` — пусто).
- `BLK_MDSC`: `size == 4 + count*16`; запись `(u32 pad, u32 number, u32 x, u32 y)`.
- `BLK_SDSC`/`BLK_ADSC`/`BLK_TDSC`: запись `(u16 p1, u16 p2, u32 number, i32 x, i32 y,
  string)`; string = `u32 len` + cp1251. Payload расходуется ровно.
- `BLK_CGRP`: payload начинается с `u32 count`; далее на группу `string name`,
  `u32 cellsCount`, `cellsCount*(u16 x,u16 y)`. Payload расходуется ровно.
- `BLK_SENV`: минимум 32 байта; заголовок `i32`, `float×3`, `u32 soundCount`, затем
  3 строки; далее звуки (в P1 не моделируются).
- `BLK_WTHR`: ровно 4 байта (`u16 type`, `u16 intensity`).
- `BLK_DOOR`: `u32 count`, затем `count*6` строк; payload расходуется ровно.
- `BLK_LFLS`: ровно 4 байта (`u32 levelFloors`).

## LAO и SEF (метаданные уровня)

Подтверждено на `levels.pak` (`I:\Games\Златогорье-2`):

- `.lao`: 33 записи `levels/pack/<pack>/data/animated/*.lao`, ровно по одной на
  33 pack-каталога. Формат — последовательность LE-пар `(u32 height, u32 delay)`;
  размер файла кратен 8. Пустой файл (0 пар) допустим.
- `levels/single/l1_1/l1_1.sef` (9534 байта, cp1251): верхнеуровневые ключи `version`,
  `pack: "L1_1"`, `internal_location`, `exit_to_globalmap`, `weather`; далее секции
  `persons`, `cell_groups`, `triggers`, `points_entrance`. Реальный ключ называется
  `exit_to_globalmap` (без `_map`); парсер принимает оба написания. `pack` нормализуется
  в нижний регистр (`L1_1` → `l1_1`), т.к. VFS регистронезависим. Комментарии `//`
  до конца строки, CRLF и табы допускаются; блок-комментариев в файле нет.

### Person-скрипты и SEF `persons:` (P4a, 2026-10-21)

- В `scripts.pak` 782 файла `scripts/persons/*.scr`; у всех `res_name` и `level` в
  ожидаемом формате, 15 реальных скриптов `l1_1` разобраны без ошибок. Текст cp1251,
  комментарии `//`, верхнеуровневые `key: value` или
  `key value`, строки в кавычках. Вложенные пары из блоков (`weapon`, `skills`, `magic`,
  `ai`, `trade_panel`) сохраняются в `PersonDef::values` с dot-префиксом
  (`skills.SKILL_HEALING`, `ai.life_escape`, `weapon.NPC_...`); значение — остаток
  строки как есть (`weapon.NPC_...` → `"18 200"`). `res_name` обязателен; `level` —
  целое, по умолчанию 0.
- `levels/single/l1_1/l1_1.sef`: в `persons:` **16** записей; при этом во всём файле
  **41** блок `name:` — остальные 25 находятся в `points_entrance`, `cell_groups` и
  `triggers`. Поля записи: `name`, `position X Y` (тайлы), `literary_name`, `direction`,
  `route_type`, `route`, `radius`, `delay_min/max`, `tribe`, `scr_dialog`, `scr_inv`;
  `route` присутствует только у движущихся NPC.
- 15 уникальных имён `l1_1` (одно повторяется) соответствуют 15 файлам
  `scripts/persons/<lower(name)>.scr`; все найдены и разобраны. Первый:
  `L1_1.P387_Poison_Plotnik` → `L0.P1131_Inhabitans03`; последний:
  `L1.P620_Svet_3CircusWorker2` → `L0.P1158_Clowns_Male01`.

## PAD (анимационные листы персонажей)

Подтверждено на всех 276 `.pad` из `persons.pak` (`I:\Games\Златогорье-2`, 2026-10-21):
2450 анимаций, 0 ошибок. Формат (LE), вторичный источник — `DarkContact/GoldenLandEditor`
`src/parsers/PAD_Parser.cpp`:

```
magic           : 4 байта = "PAD "
dataSize        : uint32  (абсолютный offset конца области анимаций)
animationMasks  : uint32  (битовая маска ожидаемых типов)
animations[..]  : {
  type              : uint32 (один бит из animationMasks)
  size              : int32  (включает само поле size; конец = offset(size) + size)
  delay             : int32
  framesPerRow      : int32
  frameWidth        : int32
  frameHeight       : int32
  anchorX           : int32
  anchorY           : int32
  movementX         : float
  movementY         : float
  cropsTableSize    : uint32 (байты; % (8*framesPerRow) == 0)
  crops             : rows × framesPerRow × (int16 x,y,w,h)
  shadowFrame       : int32 width, height, anchorX, anchorY
  shadowTableSize   : uint32 (аналогично cropsTableSize)
  shadowCrops       : shadowRows × framesPerRow × (int16 x,y,w,h)
}
```

- `rows = cropsTableSize / 8 / framesPerRow`, `shadowRows` — аналогично.
  У person-таблиц реально 5 уникальных строк (8 направлений, 5..7 — mirror) или
  9 строк (16 направлений `_go`, 9..15 — mirror); у shadow-таблиц 8/16 строк
  (identity). Индекс строки всегда проверяется на границу `crops.size()`.
- `type` — ровно один бит: `0x1 rt_stay`, `0x2 rt_fun`, `0x4 tb_stay`, `0x8 tb_fun`,
  `0x10 tb_go`, `0x20 rt_go`, `0x40 cast`, `0x80 suffer`, `0x100 die`,
  `0x400 ss_attack`, `0x10000..0x80000 hits0..hits3`. Все 14 битов вместе дают
  `0xF05FF`; иные биты в `animationMasks` — ошибка формата.
- `dataSize` не является длиной payload от offset 12: для файлов из `persons.pak`
  он указывает абсолютный конец области анимаций. После него в файле наблюдаются
  trailer-таблицы длиной 12/76/116 байт: `u32 count` + `count` пар `(i32,i32)`.
  Байты после `dataSize` парсер не читает.
- Один файл, `persons/l0.p1081_interwizards07/l0.p1081_interwizards07.pad`, содержит
  внутри `dataSize` после всех masked-анимаций хвост: trailer-таблицу (`count=9`) и два
  полных повторных animation record (`ss_attack`, `hits1`, побайтно совпадают с первыми).
  Контракт парсера: после исчерпания всех битов `animationMasks` допускается только
  `u32 tailCount` + `tailCount` пар `(i32,i32)` (tailCount ≤ 4096) и затем полные
  повторные animation records; любой другой хвост — `format`-ошибка. Возвращаются
  только анимации из маски.
- `delay` не валидируется и сохраняется как есть (включая 0/отрицательные);
  потребитель обязан защитить деление при вычислении кадра.
- Направления: person 8 — `{0,1,2,3,4,3,2,1}` (5..7 mirror), person 16 для `_go` —
  `{0..8,7..1}` (9..15 mirror); shadow-таблицы — identity, тени не зеркалируются.

## Прочие форматы (следующие подпроекты)

- `LVL`, `MDF`, `SEF`, `LAO` — уровни/маски/метаданные (`DarkContact/GoldenLandEditor`).
- `ANI` — анимации персонажей.
- `PRS`, `SSH`, `INF`, `INV`, `DSC` — персонажи/скрипты/инвентарь.
- `SCR`, `CS`, `AGE.CS` — скрипты; `CS` — компилированный байткод,
  опкоды/функции задокументированы в `DarkContact/GoldenLandEditor/docs` и `CsOpcodes.h`.

## Правила проверки

- Любой формат считается подтверждённым только после успешного разбора **всех**
  файлов соответствующего типа из целевых архивов.
- Расхождения с вторичными источниками фиксируются здесь с примером файла и offset.
