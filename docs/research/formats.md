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

## Прочие форматы (следующие подпроекты)

- `LVL`, `PAD`, `MDF`, `SEF`, `LAO` — уровни/маски/метаданные (`DarkContact/GoldenLandEditor`).
- `ANI` — анимации персонажей.
- `PRS`, `SSH`, `INF`, `INV`, `DSC` — персонажи/скрипты/инвентарь.
- `SCR`, `CS`, `AGE.CS` — скрипты; `CS` — компилированный байткод,
  опкоды/функции задокументированы в `DarkContact/GoldenLandEditor/docs` и `CsOpcodes.h`.

## Правила проверки

- Любой формат считается подтверждённым только после успешного разбора **всех**
  файлов соответствующего типа из целевых архивов.
- Расхождения с вторичными источниками фиксируются здесь с примером файла и offset.
