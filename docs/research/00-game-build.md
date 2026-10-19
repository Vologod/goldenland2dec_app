# Исследование: целевая сборка игры

Дата: 2026-10-19. Источник: локальный каталог `I:\Games\Златогорье-2`.

## Файлы

| Файл | Размер | MD5 |
|---|---:|---|
| `GoldenLand.exe` | 372 736 | `6EE2462796FA41DC3796515009A10763` |
| `Client.dll` | 1 331 200 | `D6E519AA7214B406A60F24B75A8165AF` |
| `Server.dll` | 589 824 | `E3E0A262B1A60DE687523465AD5A36E8` |
| `dare.dll` | 208 896 | — |
| `protect.dll` | 0 | пустой |

- Версия: **1.3** (подтверждено `readme.update.3.txt` и наличием `*.update.3.pak`).
- PE: Win32 i386 (machine `0x014C`), MSVC (`MSVCR`, `Microsoft Visual C++`).
- PE timestamp: `0x40837534` (2004-04-19).
- `Client.dll` импортирует `DirectDraw`, `GDI32`, `USER32`, `WINMM`, `KERNEL32`.
- `protect.dll` отсутствует по содержимому; StarForce не участвует в запуске.
- Для сравнения: в `I:\Games\Златогорье2 ХН` те же размеры `Client.dll`/`Server.dll`,
  но другие MD5 (`C23D...`/`2ADD...`) — это сборка аддона, не целевая.

## Архивы `Data\`

| Архив | Размер | Файлов | Суммарный распакованный размер |
|---|---:|---:|---:|
| `engineres.pak` | 49 247 901 | 1041 | — |
| `engineres.update.3.pak` | 2 498 181 | — | — |
| `items.pak` | 4 044 708 | 3 786 | 28 847 595 |
| `items.update.3.pak` | 1 978 | — | — |
| `levels.pak` | 402 004 345 | — | — |
| `levels.update.3.pak` | 2 305 | — | — |
| `magic.pak` | 39 700 439 | — | — |
| `music.pak` | 22 206 000 | — | — |
| `persons.pak` | 327 370 036 | — | — |
| `scripts.pak` | 3 000 694 | 4 485 | 9 324 686 |
| `scripts.update.3.pak` | 1 358 131 | — | — |
| `sdb.pak` | 542 847 | 45 | 1 411 188 |
| `sdb.update.3.pak` | 473 504 | — | — |
| `sounds.pak` | 284 164 587 | — | — |
| `wear.pak` | 72 613 381 | — | — |

`scripts.pak` по расширениям: `.ssh` 2502, `.scr` 826, `.cs` 555, `.prs` 277,
`.inv` 238, `.inf` 54, `.dsc` 26, `.bin` 3, `.ini` 2, `.pssh` 1, `.h` 1.
`items.pak`: `.bmp` 1938, `.itm` 1848.
`sdb.pak`: 45 `.sdb` (в т.ч. `dialogsphrases.sdb` 854 634 байта).

## Наблюдения о движке

- Движок **клиент-серверный даже в одиночной игре**: логика в `Server.dll`,
  представление в `Client.dll`, хост/цикл/консоль в `GoldenLand.exe`.
- 2D-изометрия на спрайтах; графика через DirectDraw и GDI, звук через `dare.dll`
  (обёртка над DirectSound, судя по экспортам `_DARE_*`).
- Скрипты: `AgeScript` (`.age.cs`), компилированный байткод `.cs`, серверные `.ssh`,
  предметы `.itm`, персонажи `.prs`, уровни `.lvl/.pad`, спрайты `.csx`.
- В `Client.dll` найдены строки `Goldenland II save`, `Saves\slot%d.sav`,
  `engineres\interface\...`, `software\Burut CT\ColdHeaven` (последнее — в сборке ХН).
- `Server.dll` и `Client.dll` не упакованы; код доступен для Ghidra/IDA.

## Инструменты сообщества (вторичные источники)

- `DarkContact/GoldenLandEditor` (C++, SDL3, ImGui): парсеры LVL/PAD/CSX/SDB/ITM/MDF/SEF/LAO,
  документация AgeScript, опкодов CS, консольных команд. Лицензия не указана — использовать
  как справочник, не копировать код без проверки лицензии.
- `fersatgit/Goldenland-2-Cold-Heaven` (Pascal, Unlicense): описание PAK/CSX/SDB/ITM,
  Kaitai-схемы, плагины Total Commander/XnView.
- `AlexKimov/goldenland-file-formats` (C++): инструменты форматов.
- `devion-user/csharp-goldenland2-decoding` (C#): извлечение ресурсов.
- `pakompom/SpaceRangersHD_FPC`: референс организации кроссплатформенного порта
  (FPC + SDL-слой + Android), но **не** источник кода для GoldenLand.

## Ограничения

- Восстановленного исходника GoldenLand нет; искать/использовать утёкший код запрещено.
- Данные игры не коммитить.
