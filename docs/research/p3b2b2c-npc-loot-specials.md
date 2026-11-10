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
  последующий лут. На `l1_1`, seed 1, loot level 1: 17 кандидатов,
  7 срабатываний, 9 модификаторов; 1057 → 1272 draw (+215), первое
  расхождение — `L1_1.P389_Crying_Guardian`.
- Модификаторные множители цены (`class/type`) — это те же поля
  `special_desc`-записи (`+0x74/+0x84/+0x94`, `+0x14…+0x64`), а
  client-manager `DAT_12110ed8` — тот же объект, что server
  `DAT_14088df4` (host `+0x84`). Это закрывает основной missing-gap
  **D-058 §8.2** структурно; остаётся live FPU-CW проверка.
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
| `SPECF_* → tag` | `FUN_0042016c` (`0x42016c` / `0x2016c`) | `TIME_PERMANENT=1, DAY=2, NIGHT=4, INTERVAL=8, IMMEDIATE=0x10, DELAYED=0x20; VALUE_CHANGE_ABSOLUTE=0x40, RELATIVE=0x80, REPLACE=0x100; ANTI_MONSTER=0x200`. |
| `T_* → item type` | `FUN_00420208` (`0x420208` / `0x20208`) | `T_SWORD=0, T_AXE=1, … T_STONE=28` (полный список — в функции). |

`ANTI_MONSTER` в retail `specials.scr` не встречается; парсер
`FUN_004216ac` принял бы его за ошибку (выход из парсинга), т.к. в
special_desc допустимы только value-tags и time-tags.

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
    if nspec < count:
        for r in 0 .. count-nspec-1:
            idx = range(0, count-1)           # 48405
            remove pool[idx] (shift left); count -= 1
    # count == min(nspec, pool_size); R6 сделан count-nspec раз

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
- **R8**: в retail time-group всегда 1 слот с access `+`
  (`TIME_PERMANENT`); draw нет. Time-слоты 4–6 не читаются.
- **R9**: для отрицательного `fval` `half >= v` (floor), поэтому
  `range(half,v)` возвращает `half` без draw; для положительного —
  draw, только если `v > half` (например, `v<=1` → нет draw).
- **R10**: как в псевдокоде; `flags & 0x80` — это `RELATIVE` tag,
  не item-флаг.
- `ROUND` = trunc: asm `0x140475b4-0x140475d4` (`fnstcw`, `or 0xC00`,
  `fldcw`, `fistp`, restore). PC не меняется; Windows default CW =
  `0x027F` (PC=53/double), как в `p3b2b4b` §4.5. `fval` хранится в
  double (`fstp qword`), произведение — double.
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

На каждого NPC (в порядке `persons:` SEF) и каждый проход:

| # | Вызов | Аргумент | Когда |
|---|---|---|---|
| R1 | `coin` | `regenerate_chance` | всегда, до проходов |
| R2 | `coin` | `0.01*n3` (pass 1) | для записи в окне |
| R3 | `range` | `n4,n5` (pass 1) | если R2 != 0 и `n3 != 0` |
| R4 | `coin` | `0.45` или `0.09` | на каждый чанк с `assigned==1`, `flags&4`, `mods==0` |
| R5 | `coin` | `0.30/0.15` или `0.08…0.01` | пока R4/R5 успешны |
| R6 | `range` | `(0, pool_size-1)` × (`pool_size-nspec`) | внутри `FUN_14047284`, до значений |
| R7 | `range` | `(0, n1-1)` | на каждый special, если `n1>1` |
| R8 | `range` | `(0, n2-1)` | на каждый special, если `n2>1` (в retail нет) |
| R9 | `range` | `(half, v)` | на каждый special, если `v>half` (positive) |
| R10 | `range` | `(0,1)` или `(0,3)` | на каждый special при условии flags/знака |
| R2' | `coin` | `(0.01*n3)/chance_decrease` | pass 2, только `n3 != 0` |
| R3' | `range` | scaled `n4,n5` | pass 2 |
| R4–R10 | — | — | pass 2, те же условия |

Порядок на один special: **R6 (все удаления) → [R7, R8, R9, R10] для
каждого выбранного id**. R6 идёт до значений, потому что сначала
формируется подмножество pool.

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
| кандидатов (`assigned==1`, `flags&4`, `mods==0`) | **17** |
| срабатываний (`nspec != 0`) | **7** |
| созданных модов (pass1+pass2) | **9** |
| RNG draw без specials | **1057** |
| RNG draw со specials | **1272** (+215) |
| первое расхождение | `L1_1.P389_Crying_Guardian` |

Первое расхождение — не специальный предмет, а **неудачный R4**:
`L1_1.P389_Crying_Guardian` pass 1, `ELB_1_04_1` (type 14,
flags `0xC0000004`) — `coin(0.45)` вернул 0, `nspec=0`, но один draw
потреблён (state `75413397 → 3257732100`). Этого достаточно, чтобы
сдвинуть весь последующий pass 2.

События special-path (analytic reference; pass1 hot + pass2 main):

| person | pass | item | type | nspec | mods (id, value, flags) |
|---|---:|---|---:|---:|---|
| `L1_2.P404_Sister_Of_Mercy2` | 1 | `BRL_1_01_5` | 15 | 1 | `(25, -1, 0x81)` |
| `L0.P785_witch3` | 1 | `BRL_1_04_4` | 15 | 2 | `(16, 2, 0x41)`, `(43, 2, 0x41)` |
| `L0.P785_witch3` | 1 | `AML_1_03_1` | 16 | 2 | `(44, 2, 0x41)`, `(46, 1, 0x41)` |
| `L0.P785_witch3` | 2 | `RNG_1_01_2` | 17 | 1 | `(18, 2, 0x41)` |
| `L0.P693_citizen6` | 1 | `POT_1_00_30` | 19 | 1 | — (пустой pool; `flags |= 0x200`) |
| `L0.P744_oficer1` | 1 | `SWR_1_03_3` | 0 | 1 | `(32, 3, 0x81)` |
| `L0.P744_oficer1` | 1 | `SHL_1_04_1` | 13 | 2 | `(11, 2, 0x41)`, `(24, 2, 0x41)` |

`+215` draw'ов: **208** — сам special-path, остальные 7 — уже
изменившиеся последующие chance/quantity-ветки. Внутри 208:
**27** — R4/R5 (17 кандидатов × R4 + 10 успешных R5), **171** — R6
(`pool_size - nspec` на каждое срабатывание: type15 `33-1`, `33-2`;
type16 `33-2`; type17 `33-1`; type0 `30-1`; type13 `18-2`), **10** —
R9/R10; R7/R8 для этих id не срабатывали (`MAX_HEALTH/MAX_ENERGY` не
выбраны; time-group = `PERMANENT`).

Влияние на main (pass 2) по персонам (no-specials → specials, только
изменившиеся агрегаты):

| person | было | стало |
|---|---|---|
| `L1_1.P389_Crying_Guardian` | `MON=218` | `MON=287` |
| `L1_2.P404_Sister_Of_Mercy2` | `AML_1_04_2=1` | `BRL_1_01_2=1` |
| `L0.P785_witch3` | `BRL_1_01_2=1` | `MON=108`, `RNG_1_01_2=1`, `SCR_4_0_3=1` |
| `L1_2.P403_Sister_Of_Mercy1` | `MON=119` | `MON=159` |
| `L0.P711_citizen8` | `MON=323` | `MON=93` |
| `L0.P692_citizen5` | `FOD_1_9_1=2`, `MON=54` | `MON=60` |
| `L0.P744_oficer1` (1) | `ELB_1_03_1=1`, `HLM_1_04_2=1`, `MON=0` | `ARM_1_04_1=1`, `ELB_1_04_1=1`, `MON=317` |
| `L0.P744_oficer1` (2) | `MON=90` | `MON=177` |
| `L0.P640_children2` | `MON=85` | `MON=183`, `RGT_1_0_12=1`, `STN_1_0_2=3` |
| `L1.P621_Svet_3CircusWorkerV` | `FOD_1_13_1=3`, `MON=104` | `MON=179` |
| `L0.P733_artist1` | `MON=268` | `FOD_1_13_1=1` |
| `L1.P620_Svet_3CircusWorker2` | `FOD_1_11_1=1`, `MON=82` | `FOD_1_2_1=1`, `MON=318` |

`Kolbasens` total остался `174`, но структура изменилась (`1×174` →
`2×87`). `P387`, `P678`, `P693` не изменились.

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
| **special-capable** (`type` в pool, `flags&4`, `effects==0`, `max_stack==1`) | **522** |
| из них `flags&0x80 == 0` (price-guard active) | **20** |
| special-capable со `max_stack > 1` | **0** |

Special-capable по типам: type0 56, 1 37, 2 26, 3 11, 4 11, 5 7, 6 31,
7 56, 11 26, 12 26, 13 26, 14 26, 15 76, 16 76, 17 31.

`max_stack > 1` встречается у type0/1/2 (14/15/15), но ни один из них не
является special-capable (`effects != 0` или `flags&4 == 0`). Поэтому
merge-путь `FUN_1404e3a4` для random-specials не задействован —
`unverified` merge не блокирует specials.

### 6.2 Цена/торговля и D-058

- Item получает `flags |= 0x200` и `mod_count > 0` (если pool непуст).
- Price-блок клиента выполняется при
  `(flags & 0x80) == 0 && mod_count != 0` (`p3b2b4b` §4.3-§4.4).
  D-058 guard (`mod_count != 0 && (flags & 0x80) == 0` → `unsupported`)
  срабатывает для **20** special-capable ITM (см. §6.1), если special
  выпал. У остальных 502 special-capable `flags&0x80 != 0`, поэтому
  случайные моды на цену не влияют.
- Пример реального specials-предмета: `SWR_1_03_3` (id 536, type 0,
  flags `0x104`, price 35). В `l1_1` specials-run он получил
  `mod id=32` (`IDSPEC_TRANSFER_ENERGY`), `value=3`,
  `flags=0x81` (`RELATIVE|PERMANENT`). После special:
  `flags=0x304`, `mod_count=1`, price-guard active.
- **D-058 §8.2 (modifier multipliers) теперь раскрыт структурно.**
  Ценовые `classMul/typeMul` — это поля той же `special_desc`-записи:
  - value-group (class, `flags & 0x1c0`): `+0x68/+0x78/+0x88` (tag),
    `+0x74/+0x84/+0x94` (price); `0x40=ABS, 0x80=REL, 0x100=REPL`;
  - time-group (type, `flags & 0x3f`): `+0x08/+0x18/…/+0x58` (tag),
    `+0x14/+0x24/…/+0x64` (price); `1=PERM, 2=DAY, 4=NIGHT,
    8=INTERVAL, 0x10=IMMEDIATE, 0x20=DELAYED`.
  - Client `DAT_12110ed8` и server `DAT_14088df4` — один объект
    (host `+0x84` = `DAT_00469d54`), поэтому таблицы совпадают.
- Для примера `SWR_1_03_3` при PC=53: `base0 = trunc((100*0.0025+0.75)*35)
  = 35`; `classMul = def(id 32)+0x84 = 300` (RELATIVE); `typeMul =
  def+0x14 = 2` (PERMANENT); один mod даёт `35 + 3*300*2 = 1835`.
  Точное целое зависит от live FPU-CW (PC=53 — обязательное условие,
  как в `p3b2b4b` §4.5); без live CW цена остаётся `blocked`.
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
2. **Live FPU-CW** для modifier-цены не снят. Цена specials/mods
   реализуема только под PC=53/double; при другом CW — `blocked`
   (наследуется из `p3b2b4b` §4.5).
3. **`random_sign` (`def+0x04`)** парсится, но `FUN_14047284` и
   price-код его не читают. Семантика (применение к стату/эффекту)
   вне scope — `unverified`.
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
8. **Merge** `FUN_1404e3a4` не задействован (0 special-capable
   stackable ITM), но остаётся `unverified` в общем случае.

### 7.3 Oracle / acceptance для следующей фазы

1. **Unit: DB/parse.**
   - `specials.scr`: 58 `special_desc`, 9 слотов, 15 `itemclass`,
     pool sizes §2.4; `IDSPEC/T_*/SPECF` maps совпадают с
     `GoldenLand.exe`.
   - `FUN_14047284` pool: identity `[0..57]` ∩ type-pool, ascending;
     `nspec >= pool` → все pool; `nspec == 0` → пусто.
   - R4/R5: точные значения/лимиты 3 и 9 для `flags&0x40` 0/1.
   - R6: `pool-nspec` draw'ов; R7 только MAX_HEALTH/MAX_ENERGY;
     R8=0 в retail; R9/R10 условия §3.
2. **Deterministic integration `l1_1`, seed 1, loot level 1.**
   - events §5.2 (7 срабатываний, 9 модов; `RNG_1_01_2` main);
   - final pass2 aggregates §5.2; 1272 draw, final state
     `0xE66E2199` (референс; C++ state может отличаться только при
     другом порядке — он не должен);
   - first divergence `Crying_Guardian`/`ELB_1_04_1` (§5.2).
3. **Live/differential gate (обязателен до заявки на full parity).**
   - Запустить оригинал, зафиксировать seed/RNG-state (attach/debug,
     без DRM-обхода), снять NPC-контейнеры `+0x10` и mod-записи
     (`+0x78/+0x7c`, stride `0x14`) для `l1_1`/иного уровня.
   - Сравнить 0/N: item id/count, mod id/value/flags, `flags&0x200`.
   - Для цены — снять FPU CW до входа в `FUN_1206E8D0`; ожидается
     PC=53. Если CW иной — price `blocked`.
4. **Regression:** при включении specials принятые no-specials
   значения `174/218/323` помечены `superseded`; тесты обновляются на
   new oracle (или явно разделяются `no_specials`/`specials` режимы).

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
#   max_stack>1=231 special-capable=522 guard-active=20 stackable-special=0
# pool sizes: 0:30 1:29 2:28 3:32 4:32 5:32 6:43 7:28
#   11:22 12:21 13:18 14:21 15:33 16:33 17:33

# l1_1 seed=1 level=1 Python reference:
#   no-specials: P387 QST=1; Kolbasens MON=174; Crying MON=218;
#     P711 MON=323; P678 empty; P744 first ELB_1_03_1/HLM_1_04_2 MON=0
#   specials: candidates=17 triggered=7 mods=9 draws 1057->1272
#     special-path draws=208 (R4/R5=27, R6=171, R9/R10=10, R7/R8=0);
#     remaining 7 = downstream divergence
#     first divergence Crying_Guardian (ELB_1_04_1 nspec=0, 1 draw)
#     events: BRL_1_01_5(25,-1,0x81); BRL_1_04_4(16,2,0x41)(43,2,0x41);
#             AML_1_03_1(44,2,0x41)(46,1,0x41); RNG_1_01_2(18,2,0x41);
#             POT_1_00_30(empty pool); SWR_1_03_3(32,3,0x81);
#             SHL_1_04_1(11,2,0x41)(24,2,0x41)
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
