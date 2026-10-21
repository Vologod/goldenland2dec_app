# Research: P3a `.cs` bytecode

Дата: 2026-10-21. Источник данных: `I:\Games\Златогорье-2\Data\scripts.pak`.
Вторичный источник: `DarkContact/GoldenLandEditor`
(`src/parsers/CS_Parser.cpp`, `src/enums/CsOpcodes.cpp`, `src/enums/CsFunctions.cpp`).

## Формат

```
u32 fileSize (== размер файла)
повторять до fileSize:
  i32 opcode
  payload:
    0..20, 50: i32 a, b, c, d
    21, 24:    f64 value
    22, 23:    C-string cp1251, NUL-terminated
    48:        i32 c, i32 d, f64 functionId, до 9 × i32 args (терминатор -1)
    49:        i32 c, i32 d
```

Валидные опкоды: `0..24`, `48`, `49`, `50`. Имена опкодов и функций совпадают с
таблицами GoldenLandEditor (`CsOpcodes.cpp`, `CsFunctions.cpp`); для id `0` имя
`unknown` (в редакторе была особая строка `RS_GetPersonParameterI (0)`, это UI-артефакт).

## Легаси-обфускация шести записей

В `scripts.pak` **555** файлов `.cs`. 549 из них — обычный формат выше. Шесть записей
хранятся с побайтовым XOR тела (первые 4 байта — размер — не шифруются):

```
plain[i] = data[i] ^ ((i + 0x55) & 0xFF),  i >= 4
```

Затронутые пути (базовый `scripts.pak`):

- `scripts/dialogs/l2_1/l2.p588_kor_aferist.d588.age.cs`
- `scripts/dialogs/l6_1/l29_1.p478_escort_leader.d478.age.cs`
- `scripts/dialogs/l6_1/l6_1.p461_sfinks.d461.age.cs`
- `scripts/dialogs/l9_1/l9.p610_5alivecitizen.d610.age.cs`
- `scripts/dialogs/l9_1/l9_3.p433_docks_ghost.d433.age.cs`
- `scripts/dialogs/zlato_czen_1/l50.p166_varuh.d166.age.cs`

Подтверждение: в `scripts.update.3.pak` лежат те же скрипты по путям без подкаталога
(например, `scripts/dialogs/l9.p610_5alivecitizen.d610.age.cs`). Для трёх пар совпадают
размеры и содержимое после расшифровки: `l9.p610` — 2132 из 2138 байт, `l6_1.p461` —
12446 из 12448, `l29_1.p478` — 5504 из 5510; оставшиеся байты — реальные различия версий
скриптов. Все шесть после XOR-декодирования разбираются как обычный `.cs`.

`parse_cs` сначала пробует обычный разбор; при ошибке повторяет разбор на копии с
XOR-декодированием тела. Это не DRM/StarForce: `protect.dll` пустой, обфускация —
свойство самих старых записей архива.

## Проверка

- `tests/assets/test_cs.cpp`: синтетические узлы всех payload-типов, ошибки формата,
  имена опкодов/функций, XOR-вариант.
- `tests/assets/test_real_cs.cpp`: 555/555 `.cs` из `scripts.pak`, 0 ошибок,
  338 977 узлов (`ctest -R gl_assets`, 412.83 s).
