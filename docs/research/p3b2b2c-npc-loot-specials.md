# Phase 1b2b2c — NPC loot specials (modifiers): research

Дата: 2026-11-10. Задача: Task 1 плана
`docs/plans/2026-11-10-p3b2b2c-npc-loot-specials-research.md`.
Research-only; production-код не менялся.

## 0. Вердикт

**`implementable` (scoped).**

- Источник базы модификаторов найден: `scripts/item_class_specials/specials.scr`
  + жёстко зашитые в `GoldenLand.exe` таблицы `IDSPEC_*`, `SPECF_*`, `T_*`.
  Это **не** `.itm`-поля, не `sdb/items/itemdescparts.sdb` и не `magic.pak`.
- `FUN_14047284` (RVA `0x47284`) полностью восстановлен: пересечение
  type-pool с identity-списком `.inv`, выбор подмножества, значения,
  флаги, запись mod-массива.
- `roll_special_count` (R4/R5) восстановлен точно; условия
  (`assigned==1`, `flags&4`, `item+0x78==0`) и значения совпадают в
  `FUN_140489ec` и `FUN_1404b1c0`.
- Полный RNG-порядок special-path восстановлен. Specials **нельзя
  пропускать**: даже неудачный R4 (`nspec==0`) потребляет draw и сдвигает
  последующий лут. На `l1_1`, seed 1, loot level 1: 18 кандидатов,
  9 срабатываний, 12 модификаторов; 1057 → 1329 draw (+272), первое
  RNG-расхождение — `L1_1.P389_Crying_Guardian` pass 1, первое
  изменение выдачи — его pass 2.
- Модификаторные множители цены (`class/type`) — это те же поля
  `special_desc`-записи (`+0x74/+0x84/+0x94`, `+0x14…+0x64`), а
  client-manager `DAT_12110ed8` — тот же объект, что server
  `DAT_14088df4` (host `+0x84`). Это закрывает основной missing-gap
  **`p3b2b4b` §8 п.2** структурно; остаётся live FPU-CW проверка.
- **Не доказано live:** фактический захват NPC-модов из запущенной игры и
  live FPU control word. Это acceptance-гейты следующей фазы, а не
  блокеры реализации: DB и алгоритм доказаны статически, а независимый
  Python-референс без specials точно воспроизводит принятый oracle
  (`174/218/323`, `P678` пуст, `P744` = `ELB_1_03_1`/`HLM_1_04_2`).

## 1. Provenance / метод

| Файл | SHA-256 |
|---|---|
| `I:\Games\Златогорье-2\Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `I:\Games\Златогорье-2\GoldenLand.exe` | `7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF` |
| `I:\Games\Златогорье-2\Client.dll` | `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16` |
| `Data\items.pak` | `37BF69C8351556486762399048915D4A03BB83949CD5EDBD50A3D162C234B9E0` |
| `Data\scripts.pak` | `25C2CF44DC3B45B6A1CC5CF0FF768EFA9B4B76032FA590BC99018E6DAA5AAB46` |
| `scripts/item_class_specials/specials.scr` | `4F29A2AC464B86A9B4092736888E8F15623C2964BA2F8AC940AB46E5BA913B79` |
| `scripts/item_class_specials/init.scr` | `F4E7167FA6B4F1372530FE4CBBA94746F05E78990D0BA233235E911C29153C91` |
| `sdb/items/tech_names.sdb` | `BACFC79409DFEA4FAD1AB85D21235C2E4D7F37C4525B69C87F7838634F391796` |

Метод:

- `Server.dll`: Ghidra 12.1.4, корпус `.local/decomp/server/decompiled.c`
  (номера строк ниже — по корпусу) + `llvm-objdump -d -M intel`.
  Image base `0x14000000`; RVA = VA − `0x14000000`; для `.text`
  file offset == RVA.
- `GoldenLand.exe`: импорт/анализ Ghidra 12.1.4 в отдельный проект
  (в репозитории нет корпуса), `llvm-objdump`. Image base `0x400000`;
  `.text` file offset == VA − `0x400000`. VA и RVA указаны ниже.
- Реальные данные: `glpak extract` (`items.pak` → 1848 `.itm`,
  `scripts.pak` → 238 `.inv` + 2 `.scr`, `sdb.pak` → `tech_names.sdb`,
  `levels.pak` → `levels/single/l1_1/l1_1.sef`). Временные копии —
  вне репозитория.
- Python 3.9 — независимый референс (вне репозитория). Он реализует
  только формулы из этого документа; значения не взяты из C++-порта.

## 2. Источник базы модификаторов

### 2.1 Цепочка инициализации

1. `GoldenLand.exe` `FUN_0042ca7c` (VA `0x42ca7c`, file offset `0x2ca7c`)
   создаёт объект `DAT_00469d54` (300 / `0x12c` байт), вызывает
   конструктор `FUN_00422398` и загружает:
   - `(*vtable+0x10)( "scripts\\item_class_specials\\specials.scr" )`
     (VA `0x485d7c`; table entry `+0x10` = `FUN_004216ac`);
   - `(*vtable+0xc)( "scripts\\item_class_specials\\init.scr" )`
     (VA `0x485da8`; table entry `+0xc` = `FUN_004205a0`).
   Порядок: **specials.scr, затем init.scr**.
2. `GoldenLand.exe` `FUN_0042c350` (VA `0x42c350`) заполняет host-структуру
   `0x46c200`; `DAT_0046c284` (= host `+0x84`) = `DAT_00469d54`.
3. `GoldenLand.exe` `FUN_0042c624` (VA `0x42c624`) вызывает
   `GetClientAPI(0x46c200, 0x46d820)` (VA `0x42c6ad`) и
   `GetServerAPI(0x46c200, 0x46d840)` (VA `0x42c6bd`).
4. `Server.dll` `GetServerAPI` (`decompiled.c:63133-63183`) читает
   `DAT_14088df4 = *(host+0x84)` (`decompiled.c:63169`), т.е. ровно
   `DAT_00469d54`.
5. `Client.dll` `DAT_12110ed8` также читается как `*(host+0x84)`
   (`p3b2b4b` §4.4). Следовательно client- и server-менеджер — **один и
   тот же объект**; таблицы цен модификаторов и таблицы specials — общие.

Вывод: база модификаторов — **текстовый `specials.scr` + hardcoded
enum-таблицы в `GoldenLand.exe`**. `init.scr` содержит только
литературные имена/suffix/desc для IDSPEC и для генерации не нужен.
`items.pak`/`.itm` дают только per-item prototype mods (ITM `effects`);
`itemdescparts.sdb` — тексты описаний; `magic.pak` — spell/effect `.mdf`
и визуалы. Ни один из них не является базой random-specials.

### 2.2 Vtable менеджера (`PTR_LAB_004839b0`, VA `0x4839b0`)

| offset | GoldenLand VA | RVA | Роль |
|---:|---:|---:|---|
| `+0x04` | `FUN_004221e4` | `0x221e4` | `pool[item_type][index]` → special id; `-1` при `index >= count` |
| `+0x08` | `FUN_00422228` | `0x22228` | `pool[item_type].count` (`0` если type отсутствует) |
| `+0x0c` | `FUN_004205a0` | `0x205a0` | парсер `init.scr` (display-данные) |
| `+0x10` | `FUN_004216ac` | `0x216ac` | парсер `specials.scr` |
| `+0x14` | `FUN_00422760` | `0x22760` | `IDSPEC_token → id` (map `+0xcc`) |
| `+0x1c` | `FUN_004203f4` | `0x203f4` | `id → def record` (array `+0xc4`, count `+0xc8`, stride `0x98`) |
| `+0x28` | `FUN_00422258` | `0x22258` | добавить `(item_type, special_id)` в pool-map `+0x04` (без дубликатов) |

`+0x04/+0x08` вызываются из `FUN_14047284` (`decompiled.c:48374`,
`48385`); `+0x1c` — `decompiled.c:48457`; `+0x14` — `.inv`-парсер
`decompiled.c:48810`.

### 2.3 Hardcoded enum-таблицы `GoldenLand.exe`

| Таблица | Функция (VA / RVA) | Содержимое |
|---|---|---|
| `IDSPEC_* → id` | `FUN_0041fd20` (`0x41fd20` / `0x1fd20`) | 78 имён, id `0..0x4d`. Первые 58 (`0..0x39`) используются в itemclass-пулах; `.inv`-identity — `[0..57]`. |
| `SPECF_* → tag` | `FUN_0042016c` (`0x42016c` / `0x2016c`) | `TIME_PERMANENT=1, DAY=2, NIGHT=4, INTERVAL=8, IMMEDIATE=0x10, DELAYED=0x20; VALUE_CHANGE_ABSOLUTE=0x40, RELATIVE=0x80, REPLACE=0x100; SPECF_ANTI_MONSTER_SPECIFIED=0x200`. |
| `T_* → item type` | `FUN_00420208` (`0x420208` / `0x20208`) | `T_SWORD=0, T_AXE=1, … T_STONE=28` (полный список — в функции). |

`SPECF_ANTI_MONSTER_SPECIFIED` (0x200) есть в hardcoded map, но в
retail `specials.scr` не встречается; на генерацию и цену не влияет.

### 2.4 `specials.scr`: грамматика и запись

Текст (cp1251, `//`-комментарии). Строки:

```
special_desc IDSPEC_X [random_sign]
{
    SPECF_*   <+|- access>   <value>   <price>
    ... 9 строк ...
}

itemclass T_*
    special IDSPEC_Y
    ...
```

- `special_desc` читает `FUN_004216ac`:
  - id (`IDSPEC_*`) → `+0x00`;
  - `random_sign` (если есть) → `+0x04 = 1`;
  - 9 слотов по `0x10` байт: `tag` (`+0`), `access` (`+4`, `1` для `+`,
    `0` для `-`), `value` (`+8`, float), `price` (`+12`, float).
- Первые 3 слота (`SPECF_VALUE_*`) пишутся в value-group: record
  `+0x68`, `+0x78`, `+0x88` (tag), `+0x6c/+0x7c/+0x8c` (access),
  `+0x70/+0x80/+0x90` (value/baseval), `+0x74/+0x84/+0x94`
  (price/baseprice).
- Следующие 6 слотов (`SPECF_TIME_*`) пишутся в time-group: record
  `+0x08/+0x18/+0x28/+0x38/+0x48/+0x58` (tag),
  `+0x0c/+0x1c/+0x2c/+0x3c/+0x4c/+0x5c` (access),
  `+0x10/+0x20/+0x30/+0x40/+0x50/+0x60` (value/mod_val),
  `+0x14/+0x24/+0x34/+0x44/+0x54/+0x64` (price/mod_price).
- Полный размер записи: `0x98` = 8 (header) + 9×`0x10`.
  `FUN_14047284` читает `+0x6c/+0x7c/+0x8c` (value-group access) и
  `+0xc/+0x1c/+0x2c` (**первые три** time-slot access). Time-slot
  `+0x3c…+0x64` в генерации не читаются; в retail у них access `0`,
  поэтому поведение не меняется.
- `itemclass` (`FUN_004203c4`, VA `0x4203c4` / RVA `0x203c4`) резолвит
  `T_*` в item type через map `+0x10c`; `special` резолвит id через
  map `+0xcc`; `(*vtable+0x28)` добавляет id в pool без дубликатов.

Размеры пулов в retail (item type: count):

| type | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 11 | 12 | 13 | 14 | 15 | 16 | 17 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| count | 30 | 29 | 28 | 32 | 32 | 32 | 43 | 28 | 22 | 21 | 18 | 21 | 33 | 33 | 33 |

Типы без `itemclass` (8–10, 18–28) имеют пустой пул.

### 2.5 `.inv` `special` и prototype mods

- `.inv`-парсер `FUN_14047784` читает `special` через vtable `+0x14`,
  добавляет id в локальный список, но после парсинга **безусловно**
  заменяет его identity `[0..57]` (`p3b2b2` §4.2;
  `decompiled.c:48943-48961`). В retail активных `special` нет;
  на генерацию они не влияют.
- `FUN_14047284` берёт именно `this+0x48/+0x4c` (identity `[0..57]`) и
  пересекает с type-pool.
- Per-item prototype mods приходят из ITM `effects` (host item DB):
  `FUN_1404f4fc` (`decompiled.c:54528-54653`) копирует
  `proto+0x74 → item+0x78` (mod count) и `proto+0x78 → item+0x7c`
  (mod array). Кросс-проверка `p3b2b4b` §7.2: item 411 (`effects=1`)
  → save `modn=1`; item 932 (`effects=9`) → `modn=9`; item 953
  (`effects=0`) → `modn=0`. Random-specials выполняются только при
  `item+0x78 == 0`.

## 3. `FUN_14047284` — точный алгоритм

Сигнатура по asm: `ECX = template` (`.inv`-шаблон, `+0x48/+0x4c`),
`[esp+0x68] = item`, `[esp+0x6c] = nspec` (запрошенное число).
`nspec == 0` → return 0 без работы (`decompiled.c:48363-48365`).

Псевдокод (номера строк — `decompiled.c`):

```
FUN_14047284(template, item, nspec):
    if nspec == 0: return 0

    # 1. Пересечение identity-списка template с type-pool.
    pool = alloc(template.count * 4); zero(pool)
    type = item+0x08
    n_avail = vtable+8(type)                  # 48374
    for i in 0 .. template.count-1:           # template+0x4c = identity [0..57]
        sid = template.list[i]
        for j in 0 .. n_avail-1:
            if sid == vtable+4(type, j):      # 48385
                pool[k++] = sid; break
    count = k

    # 2. R6: случайное подмножество размера nspec.
    pool_size = count
    if nspec < pool_size:
        for r in 0 .. pool_size-nspec-1:
            idx = range(0, count-1)           # 48405; count = текущий pool
            remove pool[idx] (shift left); count -= 1
    # count == min(nspec, pool_size); R6 сделан pool_size-nspec раз

    # 3. Замена mod-массива item.
    free(item+0x7c)
    item+0x78 = count
    mods = alloc(count * 0x14); zero(mods)
    item+0x7c = mods

    # 4. Для каждого выбранного id (в порядке оставшегося pool).
    for i in 0 .. count-1:
        sid = pool[i]
        rec = vtable+1c(sid, sid, 0, 0)       # 48457; запись 0x98
        fval = (double)float(0.0)             # _DAT_14091e1c = 0.0f
        tag1 = 0
        tag2 = 0

        # 4a. value-group: slots +0x6c/+0x7c/+0x8c с access != 0.
        n1 = present(rec+0x6c) + present(rec+0x7c) + present(rec+0x8c)
        i = range(0, n1-1) if n1 > 1 else 0   # R7; при n1<=1 draw нет
        walk slots 0..2, decrement i on present; at 0:
            tag1 = rec+0x68+slot*0x10
            fval = (double)rec+0x70+slot*0x10  # baseval (float)

        # 4b. time-group: только первые 3 слота +0xc/+0x1c/+0x2c.
        n2 = present(rec+0x0c) + present(rec+0x1c) + present(rec+0x2c)
        i = range(0, n2-1) if n2 > 1 else 0   # R8; в retail n2==1
        walk slots 0..2, decrement i on present; at 0:
            tag2 = rec+0x08+slot*0x10
            fval = fval * (double)rec+0x10+slot*0x10  # mod_val (float)
        flags = tag1 | tag2

        # 4c. Значение: level = DAT_14088dc4+0x26c4 + 1.
        v = (int)ROUND(fval * (level))         # fild level; fmulp; fistp RC=trunc
        half = v >> 1

        if fval < 0.0:                         # знак baseval/mod_val
            if v >= 0: v = -1
            if half >= 0: half = -1
            v = range(half, v)                 # R9; при half>=v draw нет
            if (flags & 0x80) == 0:
                if v == -1: v -= range(0,1)    # R10
            else:
                if v > -3: v -= range(0,3)     # R10
        else:
            if v < 1: v = 1
            if half < 1: half = 1
            v = range(half, v)                 # R9
            if (flags & 0x80) == 0:
                if v == 1: v += range(0,1)     # R10
            else:
                if v < 3: v += range(0,3)      # R10

        # 5. Запись mod: stride 0x14.
        mods[i] = { id: sid, value: v, flags: flags, 0, 0 }

    item+0xc |= 0x200                          # 48568
    free(pool)
    return count
```

Ключевые факты:

- **Порядок pool**: пересечение идёт по identity `[0..57]`, поэтому
  pool отсортирован по возрастанию id, а не по порядку `specials.scr`.
- **R6**: `pool_size - nspec` draw'ов; при `nspec >= pool_size` draw'ов
  нет, и модов будет ровно `pool_size`.
- **R7**: draw только если в value-group >1 слота с access `+`.
  В retail это только `IDSPEC_MAX_HEALTH` (22) и `IDSPEC_MAX_ENERGY`
  (23), у которых `ABSOLUTE +` и `RELATIVE +`.
- **R8**: для reachable ids (входят в itemclass-пулы) time-group = 1
  слот с access `+` (`TIME_PERMANENT`) → draw нет. `n2=0` только у
  `IDSPEC_HEALTH_CURRENT` (48)/`IDSPEC_ENERGY_CURRENT` (49) (все time
  access `-`), но они не входят в пулы → для генерации недостижимы.
  Time-слоты 4–6 не читаются.
- **R9**: для отрицательного `fval` `half >= v` (floor), поэтому
  `range(half,v)` возвращает `half` без draw; для положительного —
  draw, только если `v > half` (например, `v<=1` → нет draw).
- **R10**: как в псевдокоде; `flags & 0x80` — это `RELATIVE` tag,
  не item-флаг.
- `ROUND` = trunc: asm `0x140475b4-0x140475d4` (`fnstcw`, `or 0xC00`,
  `fldcw`, `fistp`, restore). `fval` (group products) и
  `fval*loot_level` (`loot_level = +0x26c4+1`) считаются `fld/fmul` под
  **входящим** CW; `or 0xC00` действует только на финальный `fistp`
  (trunc). Windows default CW = `0x027F` (PC=53/double, RC=nearest), как
  в `p3b2b4b` §4.5. Поэтому значения specials условны для default
  `0x027F`; `fval` хранится в double (`fstp qword`), произведение —
  double.
- Mod-запись: `+0x00 id`, `+0x04 value`, `+0x08 flags`, `+0x0c = 0`,
  `+0x10 = 0` (asm `0x140476a0-0x140476bf`). `item+0xc |= 0x200`
  ставится **безусловно**, даже если `count == 0` (пустой pool).
- Константы: `0x91e1c = 0.0f` (initial fval), `0x91ee0 = double 0.01`,
  `0x91ee8 = 1.0f`, `0x91eec = 0.01f`, `0x91ef0 = -0.15f`,
  `0x91ef4 = 0.45f`, `0x91ef8 = 0.0f`, `0x91f00 = 0.0f`.

## 4. `roll_special_count` (R4/R5)

Одинаковая логика в:

- `FUN_140489ec` (hero/иной путь): `decompiled.c:49560-49583`;
- `FUN_1404b1c0` (SEF NPC, pass 1): `decompiled.c:51189-51209`;
- `FUN_1404b1c0` (SEF NPC, pass 2): `decompiled.c:51329-51363`.

Условие (на каждый созданный чанк):

```
assigned == 1
&& (item+0xc & 0x04) != 0        # item flags: specials-capable
&& (item+0x78 == 0)              # нет prototype mods (ITM effects == 0)
```

В `FUN_140489ec` `assigned` — это `chunk = min(qty, max_stack)`
(`decompiled.c:49550-49556`); в `FUN_1404b1c0` создание идёт с
`FUN_1404e3a4(name,0,1)`, затем `item+0xb0 = min(remaining, item+0x34)`
и проверяется уже это значение (`51183-51189`, `51323-51329`).
Поэтому для `max_stack == 1` (оружие/броня) specials проверяются на
каждом экземпляре.

Точные значения:

```
if (item+0xc & 0x40) == 0:
    n = 0
    if coin(0.45):
        while True:
            n += 1
            if n >= 3: break
            if not coin(n * -0.15 + 0.45): break
else:
    n = 0
    if coin(0.09):
        while True:
            n += 1
            if n >= 9: break
            if not coin((9 - n) * 0.01): break
return n
```

- `flags & 0x40` — item flag (не mod flag). В ITM таких 99/1848;
  семантика бита (название) `unverified`, поведение точное.
- Даже при `n == 0` R4 уже потреблён. Если у item type пустой pool
  (например, type 19 potion), `FUN_14047284` всё равно вызывается при
  `n != 0`, возвращает 0 и ставит `item+0xc |= 0x200`, но R6–R10 не
  потребляет.
- `FUN_140489ec` использует те же значения (`0.45`, `-0.15`, `0.09`,
  `0.01`), но другие константы-адреса (`0x91e40/0x91e3c/0x91e38`).

## 5. Полный RNG-порядок special-path и влияние на последующие записи

### 5.1 Порядок для `FUN_1404b1c0` (SEF NPC)

Порядок обхода (точно как `FUN_1404b1c0`/`generate_npc_inventories`):
для каждого SEF person в порядке `persons:` — **R1; pass 1 (все записи);
pass 2 (все записи); затем следующий person**. Hot (`+0xc`) и main
(`+0x10`) эмитируются per person; **не** «все pass1, потом все pass2».
Это существенно: при неверной вложенности special в pass 1 одного NPC
сдвигает pass 2 предыдущего NPC.

`FUN_14006288` (coin) не вызывает `_rand` при `p <= 0` (return 0) и
`p > 1` (return 1); `FUN_140061e4` (range) не вызывает `_rand` при
`hi <= lo` (return lo). Поэтому «вызов» и «draw» — разные вещи; ниже
указан фактический draw. Все числа draw в §5.2 — реальные вызовы
`_rand`/`next`; ранние return не считаются.

| # | Вызов | Аргумент | Когда | Фактический `_rand` draw |
|---|---|---|---|---|
| R1 | `coin` | `regenerate_chance` | per person, до проходов | `p==0` → нет; `0<p<=1` → да; `p>1` → нет (header 0..1) |
| R2 | `coin` | `0.01*n3` (pass 1) | запись в окне | `n3==0` → `p=0`, draw нет + skip; `0<n3<=100` → да |
| R3 | `range` | `n4,n5` (pass 1) | R2 != 0 и `n3 != 0` | `n4>=n5` → нет (return `n4`); иначе да |
| R4 | `coin` | `0.45` или `0.09` | кандидат (`assigned==1`, `flags&4`, `mods==0`) | всегда да |
| R5 | `coin` | `0.30/0.15` или `0.08…0.01` | пока R4/R5 успешны | всегда да |
| R6 | `range` | `(0, current_count-1)` × (`pool_size-nspec`) | внутри `FUN_14047284`, до значений | всегда да (`hi>lo`; `current_count` уменьшается) |
| R7 | `range` | `(0, n1-1)` | на special, если `n1>1` | да; `n1<=1` → нет |
| R8 | `range` | `(0, n2-1)` | на special, если `n2>1` | да; `n2<=1` → нет |
| R9 | `range` | `(half, v)` | на special | да только при `v>half` (positive `v>=2`); negative/`v<=1` → нет |
| R10 | `range` | `(0,1)` или `(0,3)` | на special при условии flags/знака | всегда да |
| R2' | `coin` | `(0.01*n3)/chance_decrease` | pass 2, только `n3 != 0` | `p<=0` → нет; `p>1` → нет (return 1); `0<p<=1` → да |
| R3' | `range` | scaled `n4,n5` | pass 2 | `lo>=hi` → нет; иначе да |
| R4–R10 | — | — | pass 2, те же условия | как выше |

Порядок на один special: **R6 (все удаления) → [R7, R8, R9, R10] для
каждого выбранного id**. R6 идёт до значений, потому что сначала
формируется подмножество pool.

- **R6**: число удалений = `pool_size − nspec`; **каждое** удаление
  использует `range(0, current_count-1)`, где `current_count`
  уменьшается от `pool_size` до `nspec+1` — это **не** постоянный
  `pool_size-1`.
- Time-group: `n2=1` для всех id, реально входящих в itemclass-пулы
  (`TIME_PERMANENT` — единственный с access `+`). `n2=0` только у
  `IDSPEC_HEALTH_CURRENT` (48)/`IDSPEC_ENERGY_CURRENT` (49) (все time
  access `-`), но эти id не входят ни в один pool → для генерации
  недостижимы. Поэтому «R8=0 в retail» — только про reachable ids.

### 5.2 `l1_1`, seed 1, loot level 1 — фактический прогон референса

Независимый Python-референс без specials точно совпал с принятым
oracle `p3b2b2` §15.6:

- `P387 QST_1_0_5 = 1`;
- `Kolbasens MON_1_0_1 = 174`;
- `Crying_Guardian MON_1_0_1 = 218`;
- `P711_citizen8 MON_1_0_1 = 323`;
- `P678_citizen4` — пуст;
- `P744_oficer1` (первый) — `ELB_1_03_1`, `HLM_1_04_2`, money `0`.

С specials (те же seed/level/оба прохода):

| показатель | значение |
|---|---:|
| кандидатов (`assigned==1`, `flags&4`, `mods==0`) | **18** |
| срабатываний (`nspec != 0`) | **9** |
| созданных модов (pass1+pass2) | **12** |
| RNG draw без specials | **1057** |
| RNG draw со specials | **1329** (+272) |
| первое RNG-расхождение | `L1_1.P389_Crying_Guardian` pass 1 (`ELB_1_04_1`) |
| первое изменение выдачи | `L1_1.P389_Crying_Guardian` pass 2 (`MON 218→287`) |
| persons до divergence | `P387`, `P678`, `Kolbasens` — hot/main идентичны, включая stack structure |
| final RNG state | `0x48CAA370` (per-person nesting §5.1) |

Первое RNG-расхождение — **неудачный R4**:
`L1_1.P389_Crying_Guardian` pass 1, `ELB_1_04_1` (type 14,
flags `0xC0000004`) — `coin(0.45)` вернул 0, `nspec=0`, но один draw
потреблён (state `75413397 → 3257732100`). `ELB_1_04_1` — последняя
запись pass 1, поэтому **hot pass 1 P389 ещё идентичен**; сдвиг
проявляется в pass 2 этого же NPC (`218 → 287`). Persons #1–#3
(`P387`, `P678`, `Kolbasens`) полностью идентичны.

Регрессия вложенности (no-specials vs specials; `(item,count)`):

| person | pass 1 | pass 2 |
|---|---|---|
| `L1_1.P387_Poison_Plotnik` | `QST_1_0_5=1` | `QST_1_0_5=1` |
| `L0.P678_citizen4` | `MON 79+81+128`, `FOD_1_7_1=1`, `FOD_1_14_1=1` | empty |
| `L0.P7_Kolbasens_Rodstvennik1` | `MON 62+72+129`, `RGT_1_0_8=1`, `RGT_1_0_13=1` | `MON 87+87` |

`Kolbasens` — **2×87 в обоих прогонах**; ранее ошибочно указывалась
смена stack structure (эффект неверной вложенности «все pass1, потом
все pass2»). При правильной вложенности `P387/P678/Kolbasens` не
меняются. `P693` также пуст в обоих.

События special-path (analytic reference; pass1 hot + pass2 main):

| person | pass | item | type | nspec | mods (id, value, flags) |
|---|---:|---|---:|---:|---|
| `L1_2.P404_Sister_Of_Mercy2` | 1 | `BRL_1_01_5` | 15 | 1 | `(25, -1, 0x81)` |
| `L0.P785_witch3` | 1 | `BRL_1_04_4` | 15 | 2 | `(16, 2, 0x41)`, `(43, 2, 0x41)` |
| `L0.P785_witch3` | 1 | `AML_1_03_1` | 16 | 2 | `(44, 1, 0x41)`, `(46, 2, 0x41)` |
| `L0.P785_witch3` | 2 | `RNG_1_03_1` | 17 | 1 | `(18, 2, 0x41)` |
| `L0.P744_oficer1` | 1 | `HLM_1_03_1` | 11 | 1 | `(20, 2, 0x41)` |
| `L0.P744_oficer1` | 2 | `SWR_1_04_2` | 0 | 1 | `(10, 2, 0x41)` |
| `L0.P744_oficer1` | 1 | `HLM_1_01_2` | 11 | 2 | `(12, 2, 0x41)`, `(25, -4, 0x81)` |
| `L0.P744_oficer1` | 1 | `SHL_1_01_2` | 13 | 1 | `(7, 2, 0x41)` |
| `L0.P744_oficer1` | 2 | `ARM_1_04_2` | 12 | 1 | `(12, 1, 0x41)` |

`+272` draw'ов (net): **275** — сам special-path, downstream — минус 3.
Внутри 275: **30** — R4/R5 (18 R4 + 12 coin-вызовов R5; сумма `nspec`
= 12, последний coin в последовательности может быть неуспешным),
**233** — R6 (`pool_size − nspec`; каждое удаление — `range(0,
current_count-1)`; type15 `33-1`, `33-2`; type16 `33-2`; type17 `33-1`;
type11 `22-1`, `22-2`; type0 `30-1`; type13 `18-1`; type12 `21-1`),
**12** — R9/R10; R7/R8 для этих id не срабатывали
(`MAX_HEALTH/MAX_ENERGY` не выбраны; time-group = `PERMANENT`).

Per-mod trace (12 модов, 12 R9/R10 draw'ов; `v0 = trunc(fval*level)`,
`half` — после clamp/negative fix):

| person | pass | item | mod | fval | v0 | half | v | R9 | R10 |
|---|---:|---|---:|---:|---:|---:|---:|---:|---:|
| `P404` | 1 | `BRL_1_01_5` | 25 | -0.5 | 0 | -1 | -1 | 0 | 1 |
| `P785` | 1 | `BRL_1_04_4` | 16 | 1.0 | 1 | 1 | 2 | 0 | 1 |
| `P785` | 1 | `BRL_1_04_4` | 43 | 0.5 | 0 | 1 | 2 | 0 | 1 |
| `P785` | 1 | `AML_1_03_1` | 44 | 1.0 | 1 | 1 | 1 | 0 | 1 |
| `P785` | 1 | `AML_1_03_1` | 46 | 0.5 | 0 | 1 | 2 | 0 | 1 |
| `P785` | 2 | `RNG_1_03_1` | 18 | 0.5 | 0 | 1 | 2 | 0 | 1 |
| `P744` | 1 | `HLM_1_03_1` | 20 | 0.25 | 0 | 1 | 2 | 0 | 1 |
| `P744` | 2 | `SWR_1_04_2` | 10 | 0.025 | 0 | 1 | 2 | 0 | 1 |
| `P744` | 1 | `HLM_1_01_2` | 12 | 0.025 | 0 | 1 | 2 | 0 | 1 |
| `P744` | 1 | `HLM_1_01_2` | 25 | -0.5 | 0 | -1 | -4 | 0 | 1 |
| `P744` | 1 | `SHL_1_01_2` | 7 | 0.025 | 0 | 1 | 2 | 0 | 1 |
| `P744` | 2 | `ARM_1_04_2` | 12 | 0.025 | 0 | 1 | 1 | 0 | 1 |

Итого R9=0, R10=12: в этом прогоне R9 не потребовался (все `v0<=1`
для positive и `v0=0` для negative); каждый мод — ровно один R10 draw.
Старого случая «10 draw'ов на 9 модов» в corrected run нет.

Влияние на main (pass 2) по персонам (no-specials → specials, только
изменившиеся агрегаты):

| person | было | стало |
|---|---|---|
| `L1_1.P389_Crying_Guardian` | `MON=218` | `MON=287` |
| `L1_2.P404_Sister_Of_Mercy2` | `AML_1_04_2=1` | `BRL_1_01_2=1` |
| `L0.P785_witch3` | `BRL_1_01_2=1` | `RNG_1_03_1=1`, `SCR_5_0_1=1` |
| `L1_2.P403_Sister_Of_Mercy1` | `MON=119` | `MON=251` |
| `L0.P711_citizen8` | `MON=323` | `MON=0` |
| `L0.P692_citizen5` | `FOD_1_9_1=2`, `MON=54` | `MON=72` |
| `L0.P744_oficer1` (1) | `ELB_1_03_1=1`, `HLM_1_04_2=1`, `MON=0` | `HLM_1_04_1=1`, `SWR_1_04_2=1`, `MON=161` |
| `L0.P744_oficer1` (2) | `MON=90` | `ARM_1_04_2=1`, `FOD_1_9_1=3`, `SWR_1_03_2=1`, `MON=73` |
| `L0.P640_children2` | `MON=85` | `MON=83` |
| `L1.P621_Svet_3CircusWorkerV` | `FOD_1_13_1=3`, `MON=104` | `FOD_1_10_1=1`, `MON=281` |
| `L0.P733_artist1` | `MON=268` | `MON=78` |
| `L1.P620_Svet_3CircusWorker2` | `FOD_1_11_1=1`, `MON=82` | `FOD_1_11_1=3`, `MON=0` |

`P387`, `P678`, `Kolbasens`, `P693` не изменились; у `Kolbasens`
hot/main и stack structure идентичны (pass 2 — `2×87` в обоих прогонах).

**Следствие:** принятый oracle `p3b2b2` §15.6 — это **no-specials**
референс. При true parity он недействителен; его значения нужно либо
заменить specials-oracle, либо явно помечать `superseded` в
implementation-фазе.

## 6. Реальные данные

### 6.1 ITM (1848 файлов, `items.pak`)

| показатель | значение |
|---|---:|
| всего `.itm` | 1848 |
| `flags & 0x4` | 922 |
| `flags & 0x40` | 99 |
| `flags & 0x80` | 565 |
| `effects > 0` (prototype mods) | 583 |
| `max_stack > 1` | 231 |
| **modifier-capable** (`type` в pool, `flags&4`, `effects==0`) | **522** (все `max_stack==1`) |
| из них `flags&0x80 == 0` (price-guard active) | **20** |
| **RNG candidates** (`flags&4`, `effects==0`; возможен `assigned==1`) | **533** |
| RNG candidates в pool (= modifier-capable) | 522 |
| RNG candidates с пустым pool (type19/20) | **11** (9 type19 + 2 type20), все `max_stack>1` |
| modifier-capable со `max_stack > 1` | **0** |

Modifier-capable по типам: type0 56, 1 37, 2 26, 3 11, 4 11, 5 7, 6 31,
7 56, 11 26, 12 26, 13 26, 14 26, 15 76, 16 76, 17 31.

`max_stack > 1` (231) по типам: 0:14, 1:15, 2:15, 8:1, 9:3, 10:3,
18:1, 19:44, 20:15, 22:78, 23:15, 24:15, 27:9, 28:3. Из них
modifier-capable — **0**.

Отдельно **11 stackable RNG candidates** (type19: ids 951–957, 969, 970;
type20: ids 407, 415): `flags&4`, `effects==0`, `max_stack=10000`, но
type19/20 **не входят в itemclass-пулы**. При `assigned==1` они
вызывают R4/R5 (draw), `FUN_14047284` возвращает 0 (pool пуст), модов
нет, но `flags |= 0x200`. Merge для них не меняет набор модов (их нет);
общий merge-путь `FUN_1404e3a4` остаётся `unverified`.

### 6.2 Цена/торговля и D-058

- Item получает `flags |= 0x200` и `mod_count > 0` (если pool непуст).
- Price-блок клиента выполняется при
  `(flags & 0x80) == 0 && mod_count != 0` (`p3b2b4b` §4.3-§4.4).
  D-058 guard (`mod_count != 0 && (flags & 0x80) == 0` → `unsupported`)
  срабатывает для **20** modifier-capable ITM (см. §6.1), если special
  выпал. У остальных 502 modifier-capable `flags&0x80 != 0`, поэтому
  случайные моды на цену не влияют.
- Пример corrected-run: `HLM_1_03_1` (id 256, type 11,
  flags `0x104`, price 40). В `l1_1` specials-run он получил
  `mod id=20` (`IDSPEC_CRUSHING_RES`), `value=2`,
  `flags=0x41` (`ABSOLUTE|PERMANENT`). После special:
  `flags=0x304`, `mod_count=1`, price-guard active.
- **p3b2b4b §8 п.2 (modifier multipliers) теперь раскрыт структурно.**
  Ценовые `classMul/typeMul` — это поля той же `special_desc`-записи:
  - value-group (class, `flags & 0x1c0`): `+0x68/+0x78/+0x88` (tag),
    `+0x74/+0x84/+0x94` (price); `0x40=ABS, 0x80=REL, 0x100=REPL`;
  - time-group (type, `flags & 0x3f`): `+0x08/+0x18/…/+0x58` (tag),
    `+0x14/+0x24/…/+0x64` (price); `1=PERM, 2=DAY, 4=NIGHT,
    8=INTERVAL, 0x10=IMMEDIATE, 0x20=DELAYED`.
  - Client `DAT_12110ed8` и server `DAT_14088df4` — один объект
    (host `+0x84` = `DAT_00469d54`), поэтому таблицы совпадают.
  - Save-записи, пришедшие из ITM `effects`, могут иметь `+0x10 = -1`
    (`p3b2b4b` §4.4); generated specials пишут `+0x0c = +0x10 = 0`.
    Price-код читает только `id/value/flags`, не `+0x0c/+0x10`.
- Для `HLM_1_03_1` при PC=53: `base0 = trunc((100*0.0025+0.75)*40)
  = 40`; `classMul = def(id 20)+0x74 = 40` (ABSOLUTE);
  `typeMul = def+0x14 = 1` (PERMANENT); **item value после модификаторов**
  (`base`/local_430) = `40 + 2*40*1 = 120`. Это **не** финальная
  trade-цена: далее применяются `f = 2.5 − 0.1*SKILL_TRADE` (buy) или
  reciprocal (sell), `trunc` и `× count` (`p3b2b4b` §4.3). Точное целое
  зависит от live FPU-CW (PC=53 — обязательное условие, как в
  `p3b2b4b` §4.5); без live CW цена остаётся `blocked`. Прежний пример
  `35 + 3*300*2 = 1835` был item value после модификаторов из другого
  прогона, не финальной trade-ценой, и здесь заменён.
- Итог для D-058: структура modifier-цены больше не missing; остаётся
  live FPU-CW и реализация. До реализации 20 guard-active предметов
  должны оставаться `unsupported` (иначе — угадывание, D-029).

## 7. Verdict, missing evidence, oracle/acceptance

### 7.1 Verdict

**`implementable` (scoped).** DB, алгоритм, RNG-порядок и поля записи
восстановлены из оригинала. Реализация random-specials возможна без
догадок. Полный parity sign-off требует live-проверки (ниже).

### 7.2 Missing evidence / open questions

1. **Live-оракул specials не снят.** Нет захвата NPC-модов из
   запущенной игры при известном RNG-state. Это acceptance-гейт, не
   блокер алгоритма.
2. **Live FPU-CW не снят** ни для генерации, ни для цены. В
   `FUN_14047284` `fval` и `fval*level` считаются `fld/fmul` под
   входящим CW (default `0x027F`: PC=53, RC=nearest); `fnstcw/or 0xC00`
   действует только на финальный `fistp` (trunc). Поэтому значения
   specials условны для default `0x027F`; при другом PC/RC возможны
   отличия до trunc. Цена — то же (`p3b2b4b` §4.5). Нужен live CW перед
   `FUN_14047284` и `FUN_1206E8D0`.
3. **`random_sign` (`def+0x04`)** парсится; комментарий
   `specials.scr:10-11` говорит о 50% смене знака `<baseval>`; однако
   `FUN_14047284` и price-код поле не читают. Где применяется —
   `unverified`.
4. **Item flag `0x40`** (режим 0..9 specials) поведение точное,
   смысловое имя — `unverified`.
5. **Time-slots 4–6** (`+0x3c…+0x64`) в генерации не читаются
   (только первые 3); в retail у них access `0`, поэтому эффекта нет.
   Причина/семантика — `unverified`.
6. **Mod fields `+0x0c/+0x10`** для generated specials всегда `0`
   (asm `0x140476b3-0x140476bf`); live-проверка не снималась.
7. **Pass-1 hot** в порте отбрасывается; specials в pass 1 обязательны
   для RNG, даже если hot не хранится. Это уже зафиксировано в
   `p3b2b2` §15.3; для specials — тем более.
8. **Merge** `FUN_1404e3a4`: modifier-capable stackable ITM = 0;
   11 type19/20 stackable RNG candidates имеют пустой pool и не
   получают модов. Merge не влияет на mod-набор, но остаётся
   `unverified` в общем случае.

### 7.3 Oracle / acceptance для следующей фазы

**Реализацию можно начинать сейчас.** Live NPC-mod oracle и live FPU-CW
— обязательные acceptance-гейты **до заявки на full parity**, а не
блокеры старта implementation spec/plan.

1. **Unit: DB/parse.**
   - `specials.scr`: 58 `special_desc`, 9 слотов, 15 `itemclass`,
     pool sizes §2.4; `IDSPEC/T_*/SPECF` maps совпадают с
     `GoldenLand.exe`.
   - `FUN_14047284` pool: identity `[0..57]` ∩ type-pool, ascending;
     `nspec >= pool` → все pool; `nspec == 0` → пусто.
   - R4/R5: точные значения/лимиты 3 и 9 для `flags&0x40` 0/1.
   - R6: `pool_size-nspec` удалений, каждое `range(0,current_count-1)`;
     R7 только MAX_HEALTH/MAX_ENERGY; R8=0 для reachable ids (`n2=1`);
     R9/R10 условия §3.
2. **Deterministic integration `l1_1`, seed 1, loot level 1.**
   - exact nesting §5.1: per person `R1 → pass1 → pass2`; regression
     `P387/P678/Kolbasens` hot/main identical including stack structure
     (`Kolbasens` pass2 `2×87` in both runs);
   - events §5.2 (9 срабатываний, 12 модов; `RNG_1_03_1` main) +
     per-mod R9/R10 trace (12 draw'ов, все R10);
   - final pass2 aggregates §5.2; 1329 draw, final state
     `0x48CAA370` (референс; C++ state может отличаться только при
     другом порядке — он не должен);
   - first RNG divergence `Crying_Guardian` pass1/`ELB_1_04_1`; first
     emitted change `Crying_Guardian` pass2 (`MON 218→287`).
3. **Live/differential gate (обязателен до заявки на full parity).**
   - Запустить оригинал, зафиксировать seed/RNG-state (attach/debug,
     без DRM-обхода), снять NPC-контейнеры `+0x10` и mod-записи
     (`+0x78/+0x7c`, stride `0x14`) для `l1_1`/иного уровня.
   - Сравнить 0/N: item id/count, mod id/value/flags, `flags&0x200`.
   - Снять FPU CW **до** `FUN_14047284` (генерация) и **до**
     `FUN_1206E8D0` (цена); ожидается default `0x027F`
     (PC=53/RC=nearest). Если CW иной — generation values и/или price
     `blocked`.
4. **Regression:** при включении specials принятые no-specials
   значения `174/218/323` помечены `superseded`; тесты обновляются на
   new oracle (или явно разделяются `no_specials`/`specials` режимы).
5. **Reference reproduction.** Полный Python-референс архивом:
   `.superpowers/sdd/2026-11-10-p3b2b2c-npc-loot-specials-research/loot_ref.py`
   (SHA-256 `BD123627FE120BB77FDD937C20FE4F53B9D6581813A36F074EE77B4B31A27F8F`);
   ключевые формулы/порядок — §3–§5. Подготовка (вне репозитория):
   `glpak extract <game> --archive items.pak --out <base>/items`;
   `--prefix scripts/inventory --out <base>/scripts`;
   `--file levels/single/l1_1/l1_1.sef --out <base>/levels`;
   `--file sdb/items/tech_names.sdb --out <base>`;
   `--prefix scripts/item_class_specials --out <base>/scripts`.
   Запуск: `set GL_SPEC_BASE=<base> && py loot_ref.py --specials`
   (без `--specials` — no-specials oracle). Скрипт содержит self-check
   `modifier_value(1.0,1)==1` для level contract.

## 8. Проверка (фактический вывод)

```
# SHA-256
Server.dll 418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837
GoldenLand.exe 7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF
Client.dll FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16
items.pak 37BF69C8351556486762399048915D4A03BB83949CD5EDBD50A3D162C234B9E0
scripts.pak 25C2CF44DC3B45B6A1CC5CF0FF768EFA9B4B76032FA590BC99018E6DAA5AAB46
specials.scr 4F29A2AC464B86A9B4092736888E8F15623C2964BA2F8AC940AB46E5BA913B79
init.scr F4E7167FA6B4F1372530FE4CBBA94746F05E78990D0BA233235E911C29153C91
tech_names.sdb BACFC79409DFEA4FAD1AB85D21235C2E4D7F37C4525B69C87F7838634F391796

# GoldenLand.exe vtable PTR_LAB_004839b0 @ 0x4839b0:
#   +0x04=0x4221e4  +0x08=0x422228  +0x0c=0x4205a0  +0x10=0x4216ac
#   +0x14=0x422760  +0x18=0x422744  +0x1c=0x4203f4  +0x20=0x422718
#   +0x24=0x422064  +0x28=0x422258  +0x2c=0x42277c
# host struct FUN_0042c350: 0x46c284 = DAT_00469d54
# API calls: 0x42c6ad GetClientAPI, 0x42c6bd GetServerAPI (host 0x46c200)

# Server.dll constants (file read, PE section map)
0x91e1c f32 0.0
0x91ee0 f64 0.01
0x91ee8 f32 1.0
0x91eec f32 0.01
0x91ef0 f32 -0.15
0x91ef4 f32 0.45
0x91ef8 f32 0.0
0x91f00 f32 0.0
0x91e38 f32 0.01   0x91e3c f32 -0.15   0x91e40 f32 0.45
0x91e60 f32 0.0

# asm FUN_14047284: R6 0x14047365-0x140473ff; R7 0x140474d8;
#   R8 0x1404754c; ROUND 0x140475b4-0x140475d4 (fistp, RC=0xC00);
#   R9 0x140475ff/0x14047651; R10 0x14047620/0x14047672/0x14047736/0x1404774b;
#   mod store 0x140476a0-0x140476bf (stride 0x14); flags|=0x200 0x140476e0.

# ITM stats (1848): flags4=922 flags40=99 flags80=565 effects>0=583
#   max_stack>1=231; modifier-capable=522 (all ms=1); RNG candidates=533
#   (522 in pool + 11 empty-pool type19/20, all ms>1); guard-active=20
#   modifier-capable stackable=0
# pool sizes: 0:30 1:29 2:28 3:32 4:32 5:32 6:43 7:28
#   11:22 12:21 13:18 14:21 15:33 16:33 17:33

# level contract: v0 = int(fval * loot_level); self-check fval=1.0,loot=1 -> 1
# l1_1 seed=1 level=1 Python reference (per-person R1,pass1,pass2 nesting):
#   no-specials: P387 QST=1; Kolbasens MON=174 (pass2 2x87); Crying MON=218;
#     P711 MON=323; P678 empty; P744 first ELB_1_03_1/HLM_1_04_2 MON=0
#   nesting regression: P387/P678/Kolbasens hot+main identical incl stacks
#   specials: candidates=18 triggered=9 mods=12 draws 1057->1329
#     special-path draws=275 (R4/R5=30, R6=233, R9/R10=12, R7/R8=0);
#     downstream -3; final state 0x48CAA370
#     per-mod R9/R10: R9=0, R10=12 (each mod one R10; no two-draw mod)
#     first RNG divergence Crying_Guardian pass1 ELB_1_04_1 (nspec=0, 1 draw);
#     first emitted change Crying_Guardian pass2 MON 218->287
#     events: BRL_1_01_5(25,-1,0x81); BRL_1_04_4(16,2,0x41)(43,2,0x41);
#             AML_1_03_1(44,1,0x41)(46,2,0x41); RNG_1_03_1(18,2,0x41);
#             HLM_1_03_1(20,2,0x41); SWR_1_04_2(10,2,0x41);
#             HLM_1_01_2(12,2,0x41)(25,-4,0x81); SHL_1_01_2(7,2,0x41);
#             ARM_1_04_2(12,1,0x41)
```

Документ: UTF-8 без BOM, `nul=0` (проверено после записи).

## 9. Ссылки

- `docs/research/p3b2b2-npc-loot-server.md` §8/§15 (special path, R4-R10,
  item record fields, `roll_special_count`, `FUN_1404b1c0`).
- `docs/research/p3b2b4b-trade-transactions.md` §4/§8 (modifier fields,
  price, D-058).
- `docs/research/p3b2a-inventory-queries-server.md`,
  `p3b2b1-inventory-mutations-server.md` (item records, containers).
- `docs/DECISIONS.md` D-058 (modifier price guard), D-029 (no guesses),
  D-055 (specials skipped → RNG desync).

## 10. Live capture 2026-11-10 (BLOCKED/PARTIAL)

- **Tool:** `tools/live/gl_live_capture.exe` (32-bit, read-only attach;
  hardware DR0 at `Server.dll+0x4b1c0`, DR1 at `+0x6c0a4`; scan-only mode).
- **Artifact (not committed):** `.local/live/special-l1_1-entry.txt`.
  `level=l1_1`, `rng_state=0x71df3143`, `loot_level=2`, 21 item / 4 mod.
- **FPU:** live x87 `ControlWord = 0x007F` at generation entry, first draw
  and scan time (PC=24, RC=nearest), not the expected `0x027F` (PC=53).
  This is a direct live measurement and contradicts the extraction
  assumption used by the analytic oracle.
- **Gate result (`GL_SPECIALS_FPU_CW=0x007F`, captured state):** CW check
  127 vs 639 fails; NPC oracle captured items=19 / port=31, matched=1 /
  mismatched=48; mods captured=1 / port=0. No test weakened.
- **Unresolved:** precision model (single vs double), common RNG stream
  order/scope (live entity/SEF order vs port sorted persons), inherited
  ITM `effects>0` scope. STATUS remains `partial`; live gates OPEN.
  Follow-up: separate live-parity subproject/spec (D-061).

## 11. Live provenance (2026-11-11)

- **Artifact (not committed):** `.local/live/special-l1_1-trace.txt`.
- **Header:** `level=l1_1`, `rng_state=0xad5bd7cd`,
  `loot_level=2`, `# loot_level_source=memory`, `# calls=16`.
- **RNG double-draw check:** `# rng_second=0xad5bd7cd check=fail`. This is a
  **tool artifact**, not a bad RNG state: the second DR1 hit re-triggered the
  same pre-draw instruction before the first draw executed, so the first
  captured state is still the pre-draw state. The tool was fixed afterwards
  (re-arm DR1 after the first draw); a future run is expected to show
  `check=ok`.
- **FPU CW comments:** `0x007f` at generation entry, first draw and scan time
  (PC=24, RC=nearest).
- **Scan:** entities=256, persons=15, items=22, mods=14.
- **Live call order (16 calls, all `tid=9392`):**
  `1 p387`, `2 inv012_rich_civil`, `3 inv018_garbage_reagent`,
  `4 inv001_swordsman`, `5 inv011_caster`, `6 inv011_caster`,
  `7 inv014_money`, `8 inv018_food_potion`, `9 inv014_money`,
  `10 inv012_civil`, `11 inv001_swordsman`, `12 inv001_swordsman`,
  `13 inv018_garbage_reagent`, `14 inv012_civil`, `15 inv012_civil`,
  `16 inv012_civil`.
- **Port order (`port order:` gate message):**
  `L1_1.P387_Poison_Plotnik(p387)`,
  `L0.P678_citizen4(inv012_rich_civil)`,
  `L0.P7_Kolbasens_Rodstvennik1(inv018_garbage_reagent)`,
  `L1_1.P389_Crying_Guardian(inv001_swordsman)`,
  `L1_2.P404_Sister_Of_Mercy2(inv011_caster)`,
  `L0.P785_witch3(inv011_caster)`,
  `L1_2.P403_Sister_Of_Mercy1(inv014_money)`,
  `L0.P693_citizen6(inv018_food_potion)`,
  `L0.P711_citizen8(inv014_money)`,
  `L0.P692_citizen5(inv012_civil)`,
  `L0.P744_oficer1(inv001_swordsman)`,
  `L0.P744_oficer1(inv001_swordsman)`,
  `L0.P640_children2(inv018_garbage_reagent)`,
  `L1.P621_Svet_3CircusWorkerV(inv012_civil)`,
  `L0.P733_artist1(inv012_civil)`,
  `L1.P620_Svet_3CircusWorker2(inv012_civil)`.
- **Order comparison:** live call order matches the port SEF order 1:1,
  including the duplicate `P744`. Person order/scope is **not** the cause of
  the oracle mismatch.
- **Gate (`GL_SPECIALS_LIVE`, no FPU-CW file):**
  `oracle compare: items captured=22 port=29 matched=1 mismatched=49;
  mods captured=14 port=7 matched=0 mismatched=21`.
- **First mismatches (verbatim):**
  `item L0.P678_citizen4 item=420 count=1 item_flags=0x8000000c
  mod_count=6 captured=1 port=0`;
  `item L0.P678_citizen4 item=812 count=78 item_flags=0xc0000000
  mod_count=0 captured=0 port=1`;
  `item L0.P678_citizen4 item=812 count=139 item_flags=0xc0000000
  mod_count=0 captured=1 port=0`;
  `item L0.P692_citizen5 item=812 count=67 ... captured=1 port=0`;
  `item L0.P692_citizen5 item=812 count=98 ... captured=0 port=1`;
  `item L0.P692_citizen5 item=812 count=127 ... captured=0 port=1`;
  `item L0.P693_citizen6 item=407 count=3 item_flags=0x8000000c
  mod_count=0 captured=0 port=1`;
  `item L0.P693_citizen6 item=938 count=1 item_flags=0xc mod_count=2
  captured=1 port=0`.
- **Inherited-effects evidence:** ITM `420` has 6 effects
  (`13..18 / 5 / 0x50`), exactly the 6 live mods; ITM `938` effects
  `11/1/0x50` + `9/-1/0x50` exactly the 2 live mods. This confirms the
  inherited-effects model. The port did not generate item 420/938 in its
  stream, so the remaining gap is generation/merge/window, not effects
  semantics.
- **Hypotheses (unverified):**
  (a) the original merges same-item stacks (`FUN_1404e3a4`), so live money
  139 may be merged stacks while the port emits separate stacks (78 + ...);
  (b) loot window/quantity or loot-level contract differences (live money
  139 exceeds one rich_civil money entry's 50..100 range at level 2);
  (c) a later RNG/specials divergence around P678/P693.
  Order and inherited effects are ruled out as causes.
- **Status: `partial`.** Not full parity. Stage 2b (merge/stacking +
  window/quantity) and Stage 4 (price CW) remain open. The precision/inherited
  model is recorded in D-062; the live price CW at `FUN_1206E8D0` is still
  unmeasured (generation CW `0x007F`/PC=24 is measured).
