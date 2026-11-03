# A8-parity: `ApplyCellMask` и `tile.mask`

Дата: 2026-11-03. Статус: **static analysis done; live gate pending**
(см. Task 1 плана). Код не менялся.

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
