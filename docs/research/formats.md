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
  size     : uint32  (размер данных в архиве)
  offset   : uint32  (абсолютное смещение от начала файла)
}
```

Данные по `offset`:

```
compressed   : uint32
if compressed == 0:
    unpacked_size : uint32
    data          : zlib-поток
else:
    data          : raw (проверить в P0)
```

Подтверждение на реальных заголовках: `engineres.pak` = `PAK \0\0\0\0`, count `0x3CE` (974),
первый `name_len` = `0x18` (24).

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
