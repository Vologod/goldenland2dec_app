# P5e — Potions/food use: server rule, client trigger, data

Дата: 2026-11-27. Статус: research done.

Источники: `Server.dll` SHA-256
`418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`
(`ImageBase = 0x14000000`), `Client.dll` SHA-256
`FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`
(`ImageBase = 0x12000000`); `llvm-objdump`, `.local/decomp/{server,client}`,
ITM-корпус (1848 файлов). Закрывает продюсер эффектов «зелья/еда» из
`docs/research/p5-effect-runtime.md` §6.

## 1. Серверное правило — `FUN_14057cf0` (RVA `0x57cf0`)

`__thiscall undefined4(this, uint owner, uint item_slot, uint target)`.
`callers=0` в декомпиляции (вызов через сетевой диспетчер). Ветка
`0x12 < item_type < 0x15` — зелья (`0x13` = 19) и еда (`0x14` = 20).

Порядок для валидной цели (type-2, живая, не `+0x5e0+0x244==1`):

1. `FUN_14007e20(manager, target)` — обновить кэш статов.
2. Цикл по эффектам предмета (`item+0x78` count, `item+0x7c` массив 5×u32):
   - если `(flags & 0x41) == 0x41` и `id` в switch:
     `6,7,8,9,10,11,12` → прямой base-стат (см. таблицу);
     `0x1c` → reputation;
     изменение пишется в кэш менеджера, clamp, write-back
     (`vtable+8`, `FUN_1400e020`).
   - иначе (включая `default` switch) → `FUN_1401792c(person, effect)`:
     запись **5 слов** добавляется в список эффектов персоны
     (`P5`-канал; `word3` = `duration_hi | lo<<8 | delay_hi<<16 | delay_lo<<24`).
3. Лечение (`item+0x80` HP, `item+0x84` EP):
   `FUN_1400c280` / `FUN_1400c1fc` возвращают текущее и максимум; текущее
   clamp `0..max`; затем `current += heal`, clamp `max`, запись
   (`FUN_14019674` для HP, `+0x1500` для EP).
4. Расход: если `item+0xB0 > 1` → `-1`, return 0; иначе return 1
   (запись удаляет вызывающая сторона).
5. `FUN_140507a4(item_slot)` запускает `scripts/useitem/<name>.scr`, если
   `item+0x64 != 0`; в retail это поле 0, поэтому no-op.

### 1.1 Прямые base-статы

| effect id | manager offset | параметр | clamp |
|---:|---|---|---|
| 6 | `+0x50` | strength | 1..`ch_max_primary_value` (30) |
| 7 | `+0x54` | constitution | то же |
| 8 | `+0x5c` | perception | то же |
| 9 | `+0x58` | dexterity | то же |
| 10 | `+0x60` | intelligence | то же |
| 11 | `+0x64` | wisdom | то же |
| 12 | `+0x68` | luck | то же |
| 0x1c (28) | `+0x6c` | reputation | `manager+4..manager+8` |

`FUN_1400df94(new, min, max)` = clamp. Значения `value` — signed int32
(в ITM хранится u32; `0xFFFFFFFF` = −1).

## 2. Клиентский триггер — right-click

- `FUN_12041520` (RVA `0x41520`) обновляет input: `+0xb0` = left release
  (`vtable+0x30`), `+0xb4` = right release (`+0x34`), `+0x68` = `+0x38`
  (left down), `+0x6c` = `+0x3c`, `+0x70` = `+0x40` (right down);
  `+0x74/+0x78/+0x7c` = предыдущие значения.
- `FUN_120407d8` (RVA `0x407d8`, экран инвентаря): при edge `+0x70`
  (right down) и курсоре над слотом (`FUN_12047c74 == 0x34`,
  `FUN_12047ccc` = item под курсором) ставит в очередь код **`0x4b`**
  (`FUN_12040c28`).
- `FUN_1205625c` (RVA `0x5625c`, обработчик очереди) для `0x4b`:
  `FUN_12044478(...) == 6` (регион инвентаря), страница `0/4`, тип
  `0x13/0x14` → `FUN_120c7358(local_player, 0xc, player, 0, slot)`.
  `FUN_120c7358` (RVA `0xc7358`) пишет pending-команду
  `{cmd=0xc, owner, 0, slot}` в `+0x424..+0x430`.
- Левый клик (edge `+0x68`) ставит код **`0x43`**; для зелий/еды он
  включает режим выбора цели (`DAT_12110f28+0x6f0 = 8`), т.е. «применить
  на другом» — отдельный сценарий.
- `FUN_120976a8` (RVA `0x976a8`) — предикат «можно использовать»:
  `0x12 < type && (type < 0x16 || type == 0x1a)`; вызывается из
  inventory/UI перед постановкой действия.
- `gv_item_using_timeout` (2000 мс) — клиентская очередь/анти-спам
  (`+0x40` записи 0x14 c timestamp), к правилу эффекта не относится.

В порте: **right-click по слоту bottom/hot** → use self; left-click
target-select и книги/свитки/рецепты — вне scope.

## 3. ITM-данные (корпус 1848)

- 44 зелья (`type 19`), 15 еды (`type 20`).
- Прямые стат-эффекты (`flags 0x41`): 12 у зелий (id 6..11) + 1 у еды
  (id 28). Пример: `948.itm` — `{6,+1}`, `{10,−1}`; `958` `{10,+1}`,`{6,−1}`.
- Лечение (`+0x80/+0x84`): 9 зелий + 2 еды. Пример: `953.itm` HP+100
  (зелье есть в `slot0`, count 51 по p3b2b1); `957` HP+20/EP+20;
  `970` HP+100/EP+100.
- Timed-эффекты: `408.itm` (еда) id 30 +100 flags `0x50` duration hi=3
  (3 ч); `409` id 4 +10 flags `0x50` 30 мин; `416` id 22/23 flags `0x90`.
- ITM `duration.hi/lo` и `delay.hi/lo` упаковываются в `cond` как
  часы/минуты — совпадает с aging `FUN_1405719c`.

## 4. Открытые/вне scope

- Цель — другой член партии (`0x43` + click): нужен UI выбора цели.
- Книги (`0x15`), свитки (`0x16`), рецепты (`0x1a`).
- `scripts/useitem/*.scr` (поле-триггер 0 в retail).
- Клиентская 2-секундная очередь (анти-спам) — порт применяет сразу;
  фиксируется в DECISIONS.
