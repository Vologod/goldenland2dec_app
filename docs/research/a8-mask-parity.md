# A8-parity: `ApplyCellMask` и `tile.mask`

Дата: 2026-11-03 (resolved 2026-11-04). Статус: **parity PASSED**
(hook + manual gate). См. раздел RESOLVED.

Источники: `Client.dll` SHA-256
`FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`
(v1.3); `GoldenLand.exe` `7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF`.

## Функции

- `Client.dll 0x1202BC54` — `ApplyCellMask(renderer, cell_x, cell_y, cell_index)`:
  заполняет per-subcell решения/ids для клетки персонажа.
- `Client.dll 0x1202F17C` — рендер маски для клетки: использует
  `+0x838C` (per-subcell «применять») и `+0x839C` (id масок).
- `Client.dll 0x1202E370` — инициализация: `renderer+0x8344 = host vtable
  +0x5C`, размеры `+0x95A0/+0x95A4 = host vtable +0x44`.
- `Client.dll 0x1202B608` — caller `FUN_1202E370`; grid приходит из host,
  не строится в Client.
- `GetClientAPI` (`0x120CA88C`) пишет указатель host-объекта в
  `DAT_12110F3C`.

## Grid `renderer+0x8344`

- Указатель на массив указателей по `cell_x`; запись клетки — **0x28
  байт** на `cell_y`.
- 4 subcell (2×2): `flags` — 4 × u32 в `+0x00..0x0C`; `mask_id` — 4 × i32
  в `+0x10..0x1C` (`-1` = нет маски).
- `FUN_1202BC54` для каждого subcell:
  - `id = grid[x][y].mask_id[sub]`;
  - `flags = grid[x][y].flags[sub]`;
  - вправо: пока `flags & 1` и `mask_id` тот же; если `flags & 2` и
    `x >= renderer+0x9588` → найдено;
  - вниз: пока `flags & 1` и id тот же; если `flags & 2` → найдено;
  - маска применяется к subcell только если найдено **и вправо, и вниз**;
  - результат: `renderer+0x838C[sub] = applies`;
    `renderer+0x839C[sub*0x3C + cell_index*4] = mask_id`.
- `FUN_1202F17C` для subcell: `id = grid.mask_id[sub]`; если `id == -1` —
  маски нет; для shadow-pass (`param_4 != 0`) дополнительно требует
  `+0x838C[sub] != 0` и `id == +0x839C[sub*0x3C + cell_index*4]`.
  Значит columns = 4 subcell (2×2), индекс персонажа — `cell_index`.

## `tile.mask`

- LVL `LvlTile.mask` (`u16`, 0xFFFF = нет) — **0-based индекс в списке
  `BLK_MDSC`** (`lvl.masks[i].number` — номер маски). Проверено на `l1_1`:
  клетка `(180,24)` → 6 → `lvl.masks[6].number == 8`; mask 8 — здание
  `x=2054,y=-17`, bottom=218.
- Origin-тайлы маски не всегда принадлежат своей маске (маски
  перекрываются), поэтому проверка по origin ненадёжна; mapping
  подтверждается на клетке hero и на live-grid.

## Что осталось (gate)

1. **Live:** прочитать `renderer+0x8344` для клетки hero `(180,24)` и
   соседей; получить `flags/mask_id` по subcell и решение
   `ApplyCellMask`. Сравнить `mask_id` с `tile.mask` индексом 6 /
   BLK_MDSC number 8.
2. **Builder:** где host строит `flags` (биты 0/1) из LVL. Гипотеза:
   `flags & 1` = subcell принадлежит региону маски, `flags & 2` =
   перекрывающий subcell; требует live/статического подтверждения.
   Без этого `select_cell_mask` не кодировать.
3. Повторить live для стража у ворот (клетка/entity) — второй кейс
   приёмки.

## Предварительный вывод

- Baseline `mask.y+height` неверен: движок решает по per-cell/subcell
  `tile.mask` и непрерывности региона вправо/вниз.
- Для hero `(180,24)` `tile.mask=6` (mask 8), но subcell-соседи справа/вниз
  могут не иметь того же id → маска не применится; это и объясняет ложное
  перекрытие в двери.
- После live-дампа можно реализовать `select_cell_mask` точно (4 subcell
  columns) без baseline и без угадывания.

## Live-дамп (2026-11-03)

Процесс `GoldenLand` PID 1980, `Client.dll` base `0x12000000`,
renderer `0x12111360`, grid `0x4037188`, размер `115x112` клеток
(24×18 px). Чтение — `ReadProcessMemory` (не debugger).

- Клетка hero `(90,12)` (мир `(2166,216)`): ids `[6,-1,40,-1]`,
  flags `[0x1,0x0,0x1,0x0]`; клетка `(90,10)` (мир y≈193):
  ids `[6,60,40,-1]`, flags `[0x1,0x1,0x1,0x0]`.
- Скан `ApplyCellMask` вниз от `(90,12)` сразу упирается в `id=-1`
  (`(90,13)`), вправо идёт по `id=6`, но без `flags&2`; вниз-условие
  ложно → маска 8 к hero **не применяется**. Это доказывает, что текущий
  baseline A8 («bottom 218 > y 216») даёт ложное перекрытие.
- Район башни (`mask 45`) — cells x30..52,y65..85: ids `17/42/41/12/...`;
  для ids `17/42/41` в subcell есть `flags&2` (например `(31,65)`
  flags `0x7`, `(34,75)` flags `0x3`). То есть `flags&2` отличает
  «перекрывающие» subcell от фоновых.
- В районе mask 8 (`id=6`) `flags&2` не встретился ни в одном subcell
  (x84..106,y4..16): mask 8 — фоновая/не перекрывающая для этой модели,
  несмотря на alpha=0-силуэт в `mask_8.csx`.

### Что всё ещё не извлечено (gate)

1. Как host-builder заполняет `flags` (`&1`, `&2`) и `mask_id`.
   Попытка сопоставить `flags&2` с alpha/центром subcell не дала корреляции;
   это не просто «alpha=0 в клетке».
2. Почему mask 8 (id 6) не имеет `flags&2`, а mask 45 (id 17/41/42) имеет:
   возможно, бит выдаётся по типу маски/слою в host, а не по CSX напрямую.
3. Точный mapping `mask_id` → `BLK_MDSC.number` в host-массиве (индекс 6
   соответствует региону mask 8 по геометрии, но общий порядок не доказан).

Без ответа на (1)/(2) `select_cell_mask` нельзя кодировать: получится
подгонка под два кейса. Нужен деcompile host-builder в `GoldenLand.exe`
(функция, отдаваемая vtable `+0x5C`) или дальнейший live-дамп с
классификацией всех 62 масок.

## Host builder найден (2026-11-03)

`GoldenLand.exe FUN_00426490` (class `PTR_LAB_00484A00`) строит grid
`object+0x330`; размеры `object+0x320/+0x324` берутся из `BLK_MHDR`
(chunk_width/chunk_height; для `l1_1` 115×112). Для каждого chunk `(x,y)` и
subcell `s=0..3` builder читает из LVL:

- `u32 flags = LvlTile.relief | (LvlTile.sound << 16)` → grid
  `+s*4` (ровно 4 байта);
- `u16 mask = LvlTile.mask` → grid `+0x10+s*4`; `0xFFFF` превращается в
  `-1`.

То есть `flags & 1`/`flags & 2` — это младшие биты `relief`, а не
производные от CSX. Дополнительные поля `+0x20/+0x24` обнуляются.

## Точная модель для порта

- `Level` хранит `MaskCellGrid` (chunk_width × chunk_height), в клетке —
  4×(u32 flags, u16 mask) из LVL.
- Для персонажа: `cell_x = pixel_x / 24`, `cell_y = pixel_y / 18`
  (`FUN_1202BF60`).
- Для каждого subcell:
  - `id = mask`; `0xFFFF` → пропуск;
  - вправо от `cell_x` до границы: пока `flags&1` и `mask==id`; если
    встретился `flags&2` — найдено;
  - вниз аналогично;
  - маска применяется к subcell только если найдено **и вправо, и вниз**.
- Renderer (`FUN_1202F17C`) для применённого subcell рисует foreground
  mask image, обрезанный прямоугольником клетки 24×18; позиция маски —
  `mask.x/y` из BLK_MDSC.
- Mask id — индекс в массиве масок (`LvlMask` order); `BLK_MDSC.number`
  даёт имя CSX.

## Multi-cell footprint и hero direction16 (2026-11-03)

- `FUN_1202BF60` строит список клеток персонажа из прямоугольника кадра
  (24×18 px): тело и тень могут занимать несколько клеток. Порт теперь
  применяет маски ко всем клеткам `draw_rect` кадра (тело и тень отдельно),
  а не только к клетке ног.
- Hero хранит `direction16` из `direction16_from_velocity`; рендер walk
  использует его вместо `direction8_to16(direction8)` (последний давал
  только чётные 16-направления, из-за чего диагональные тело/тень брали
  неверную строку). После правки полный `ctest` — 17/17 (1485.32 s).

## RESOLVED: exact cell-id selection via hook (2026-11-04)

Внешний read-only tracer (x86, Windows Debug API): hardware
execute-breakpoints на `FUN_1202BC54`/`FUN_1202F17C`, после срабатывания
выставляется `EFlags.RF` (иначе CPU повторяет первый вызов), процесс не
патчится, перед detach очищаются `Dr0..Dr7`. Требует 32-битной сборки
(`i686-w64-mingw32-clang++ -static`, `-lpsapi`).

Факты (PID 10672, `l1_1`; `Client.dll` SHA-256 `FBF213B4…5C16`):

- `ApplyCellMask` вызывается **для одной строки — `cell_y` персонажа**
  (`renderer+0x958c`), по x-диапазону кадра. Примеры: person `(74,18)`
  → Apply `(72..75,18)`; person `(66,6)` → Apply `(65..67,6)`.
- `FUN_1202F17C` (`MaskRenderer`) после этого рисует foreground **для
  всех клеток кадра, у которых grid `mask_id` совпадает с выбранным в
  строке персонажа**: person `(74,18)` → renderer `x=72..75`,
  `y=12..19`.
- Поэтому маска не «все клетки кадра» и не «только низ кадра»: выбор
  id — по строке `floor(person_y/18)`, отрисовка — по всем клеткам
  кадра с этим id.
- RVA: `FUN_1202BC54 = 0x2BC54`, `FUN_1202F17C = 0x2F17C` (при base
  `0x12000000`); person cell — `renderer+0x9588/+0x958C`; grid —
  `renderer+0x8344`.
- Реализация в порте: `LevelScreen::draw_rect_masks` собирает mask id
  в строке `person_y/18`, затем рисует foreground для клеток кадра с
  тем же id. Пользовательский gate: угол дома, место правее, полное
  перекрытие и дерево — **PASSED 2026-11-04**.

- **Проверка:** regression select_row_mask_ids collects ids across the person
  row; полный ctest — **17/17 passed**, 1155.42 s; пользовательский gate
  2026-11-04 — PASSED.
