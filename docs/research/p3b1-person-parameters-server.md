# Research: person parameter natives (`Server.dll` v1.3)

Дата: 2026-11-06. Задача: Phase 1b1, Task 1 (research only).
Источник истины — `Server.dll` v1.3; skill-name → id разрешён по
`GoldenLand.exe` (тем же объектом, который читает `Server.dll`).

## 1. Provenance и метод

| Файл | SHA-256 | Image base |
|---|---|---|
| `I:\Games\Златогорье-2\Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` | `0x14000000` |
| `I:\Games\Златогорье-2\GoldenLand.exe` | `7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF` | `0x00400000` |

- Корпус (gitignored): `.local/decomp/server/decompiled.c` (1108 функций),
  `.local/decomp/server/natives/natives.c` (65 обработчиков).
- Дизассемблер: `llvm-objdump.exe` (`-d -M intel`) из
  `llvm-mingw-20261006-ucrt-x86_64`.
- Байты данных читались напрямую из PE по `RVA → file offset`:
  `.data1` — RVA `0x8c000`, raw `0x80000`; `.data`/`.data1` exe — см.
  temp-скрипты (`dump_p3b1_consts.py`, `decode_slot_stats.py`,
  `dump_skill_names.py`; в git не входят).
- Строки/таблицы: decompiled-корпус + точечный `objdump` там, где
  псевдокод Ghidra теряет x87/поля.

Пути `.cs`/`.scr` ниже — внутри `Data/scripts.pak` / `Data/scripts.update.3.pak`;
они не коммитятся.

## 2. Native-таблица (ID → handler)

Строки скопированы из `docs/research/p3-vm-natives-server.md` §5;
usage — из spec §2 (`scripts.pak`, 556 `.cs`).

| ID | name | handler RVA | arity | arg types | return | side effects | usage |
|---|---|---|---|---|---|---|---|
| `0x4000000` | `RS_GetPersonParameterI` | `0x404f0` | 2 | `3,3` (string,string) | computed double | поиск персоны `FUN_1402914c`, затем getter по имени | 470 вызовов, все `"Hero"` |
| `0x4000001` | `RS_SetPersonParameterI` | `0x409d4` | 3 | `3,3,0` (string,string,int) | `0.0` (`DAT_140913f8`) | поиск персоны; setter по имени | 236 вызовов; `"Hero"/"REPUTATION"` 225, `"Hero"/"LIFE"` 1, `"L2_2.P71_Ivlis"/"LIFE"` 5, `"L0.P738_artist3"/"LIFE"` 1 |
| `0x400000C` | `RS_GetPersonSkillI` | `0x40944` | 2 | `3,3` (string,string) | computed double | поиск персоны; skill-name→id (`DAT_140891bc+0x78` vtable `+0x2c`), затем `FUN_1400cb64` | в поставляемых `.cs` не вызывается |

Распределение get-вызовов по именам: `REPUTATION` 444, `INTELLIGENCE` 10,
`LIFE` 4, `MAXIMUM LIFE` 4, `EXPERIENCE` 4, `REPUTATIUON` 2 (опечатка),
`WISDOM` 1, `CONSTITUTION` 1.

Аргумент `0` setter'а — int: VM (`FUN_14038fa8`, `0x38fa8`) усекает
double-аргумент к нулю (`fistp` с truncate) и кладёт int32 в
`DAT_14089750` до вызова обработчика (p3-vm research §3.5). Поэтому все
setter'ы получают уже усечённый `int`.

`RS_GetPersonParameterI` при ошибке arity/типов возвращает
`DAT_140913d8`; при неизвестном имени — `DAT_140913e0`
(`natives.c:545-549`). `RS_SetPersonParameterI` всегда возвращает
`DAT_140913f8` (`natives.c:877-880`, `1120`). `RS_GetPersonSkillI` при
ошибке arity — `DAT_140913e8`, при неизвестной персоне/навыке —
`DAT_140913f0` (`natives.c:1590-1604`).

Поиск персоны `FUN_1402914c` (`decompiled.c:29742-29797`) принимает только
entity с `(entity+0x14 & 0x3E) == 2` и `entity+0x150 == 0`; иначе `0`.
В getter'е результата поиска **нет null-проверки**: при валидном имени
параметра и отсутствующей персоне оригинал разыменовывает `null+4`
(undefined/AV; в поставляемых `.cs` не наблюдаемо). Spec §7 выбирает для
порта default вместо AV — это осознанное отклонение, не поведение
оригинала.

## 3. Default-константы (`DAT_*`)

Все значения прочитаны из PE; `.data1` raw offset = RVA − `0x8c000` + `0x80000`.

| symbol | RVA | file offset | 8 байт (hex) | double |
|---|---|---|---|---|
| `DAT_140913d8` | `0x913d8` | `0x853d8` | `00 00 00 00 00 00 00 00` | `0.0` |
| `DAT_140913e0` | `0x913e0` | `0x853e0` | `00 00 00 00 00 00 00 00` | `0.0` |
| `DAT_140913f0` | `0x913f0` | `0x853f0` | `00 00 00 00 00 00 00 00` | `0.0` |
| `DAT_14091400` | `0x91400` | `0x85400` | `00 00 00 00 00 00 00 00` | `0.0` |

Дополнительно (для полноты fallback-путей):

| symbol | RVA | 8 байт (hex) | double | роль |
|---|---|---|---|---|
| `DAT_140913e8` | `0x913e8` | `00 00 00 00 00 00 00 00` | `0.0` | skill arity fallback |
| `DAT_140913f8` | `0x913f8` | `00 00 00 00 00 00 00 00` | `0.0` | setter return |
| `DAT_14091408` | `0x91408` | `00 00 00 00 00 00 00 00` | `0.0` | `RS_GetItemCountI` fallback |

Вывод: все четыре требуемые константы и три дополнительных — ровно `0.0`
(8 нулевых байт).

## 4. Getter'ы: таблица и формулы

### 4.1 Базовый блок и accessor

Все 12 getter'ов работают через глобальный stat-manager `DAT_14088e00`
(объект с vtable `PTR_FUN_140928c0`, RVA `0x928c0`; конструктор —
`FUN_140584f4`, `decompiled.c:60032-60039`).

- `FUN_14007e20` (`decompiled.c:6101-6116`) при смене `param_1` (person id)
  копирует **0x47 dword (0x11C байт)** из записи персоны в `manager+0x50`.
- vtable `+0x04` = `FUN_140508cc` (`decompiled.c:55303-55328`) возвращает
  `entity+0xFF0` — это и есть stat-блок `BT_HERO` (см. §6).
- Следствие: **block offset `k` = `manager + 0x50 + k`**, а поля
  `manager+0x16C..0x184` — отдельный кеш модификаторов, не входящий в
  `0x11C`-блок.

Общие helper'ы:

- `FUN_14007fb0(value: float, index: int) -> float` (`decompiled.c:6212`,
  RVA `0x7fb0`) — применяет глобальную таблицу модификаторов по `index`.
  Если таблица пуста/выключена (`in_stack_00002c78[0x12] == 0` или
  `[6] == 0`), возвращает `value` без изменений (`decompiled.c:6273-6277`).
  Содержимое непустой таблицы загружается в рантайме; точные числовые
  множители для индексов ниже — `unverified`.
- `FUN_1400619c(x: float) -> int` (`decompiled.c:4329`, RVA `0x619c`) —
  округление «half away from zero»:
  `trunc(x + (x > 0 ? +0.5 : -0.5))`; disasm `0x140061ac..0x140061d8`
  (CW `or 0xc00` = truncate; константы `0x8d7a0 = +0.5f`,
  `0x8d7a4 = -0.5f`, `0x8d7a8 = 0.0f`).

Конфиг-пределы:

- `DAT_1407fb64 + 0x8c` = `ch_max_primary_value`, default **30**
  (`decompiled.c:2801`; допустимый конфиг 0x1E..0x25).
- `DAT_1407fb68 + 0x8c` = `ch_max_skill_value`, default **15**
  (`decompiled.c:2800`).
- Границы репутации: `manager+4` / `manager+8` = **−50 / +50** при
  `*(DAT_1407fc64+0x8c) == 0`, иначе **−100 / +100**
  (`decompiled.c:10867-10878`).

### 4.2 Таблица 12 параметров

`getter RVA` — функция, `setter RVA` — callee натива (или inline).
`field` — block offset / `manager` offset.

| Параметр | getter RVA | setter RVA | field | формула (кратко) |
|---|---|---|---|---|
| `STRENGTH` | `0xd678` | `0xb824` | block `0x00` / `manager+0x50` | `round(curve(raw,6)) + (gate ? +0x16C : 0)`, clamp `[1,max_primary]` |
| `CONSTITUTION` | `0xd610` | `0xb5e8` | block `0x04` / `manager+0x54` | `round(curve(raw,7)) + (gate ? +0x170 : 0)`, clamp `[1,max_primary]` |
| `DEXTERITY` | `0xd540` | `0xb3ac` | block `0x08` / `manager+0x58` | `round(curve(raw,9)) + (gate ? +0x174 : 0)`, clamp `[1,max_primary]` |
| `PERCEPTION` | `0xd5a8` | `0xb170` | block `0x0C` / `manager+0x5C` | `round(curve(raw,8)) + (gate ? +0x178 : 0)`, clamp `[1,max_primary]` |
| `INTELLIGENCE` | `0xd4c0` | `0xacf8` | block `0x10` / `manager+0x60` | `round(curve(raw,10)) + (gate ? +0x17C : 0)`; skill `0x15` > 9 → `+2`; clamp `[1,max_primary]` |
| `WISDOM` | `0xd3d8` | `0xaf34` | block `0x14` / `manager+0x64` | `round(curve(raw,11)) + (gate ? +0x180 : 0)`; skill `0x15` > 4 → `+1`; clamp `[1,max_primary]` |
| `LUCK` | `0xd458` | `0xaabc` | block `0x18` / `manager+0x68` | `round(curve(raw,12)) + (gate ? +0x184 : 0)`, clamp `[1,max_primary]` |
| `REPUTATION` | `0xcc0c` | `0xaa18` | block `0x1C` / `manager+0x6C` | clamp raw к `[rep_min,rep_max]`, `round(curve(raw,0x1C))`, clamp снова |
| `EXPERIENCE` | `0xcc84` | — (нет ветки) | block `0x20` / `manager+0x70` | raw, clamp `[0,500000]` |
| `MAXIMUM LIFE` | `0xc280` out2 + `0xc304` | — (нет ветки) | computed | см. §4.3 |
| `LIFE` | `0xc280` out1 + vtable `+0x34` | `0x19674` | `entity+0x14FC` (body `0x128`) | `clamp(current,0,max_life)`; getter пишет clamp обратно |
| `ENERGY` | `0xc1fc` out1 + vtable `+0x38` | inline `0x40cf1` в `0x409d4` | `entity+0x1500` (body `0x12C`) | `clamp(current,0,max_energy)`; getter пишет clamp обратно |

`gate` = `*(manager+0xC) != 0`: только при нём добавляются поля
`manager+0x16C..0x184`. `manager+0xC` = 0 в конструкторе
(`FUN_140584f4`, `decompiled.c:60036-60038`); путь установки флага не
прослежен (`unverified`), но именно он включает бонусы.

Skill `0x15` = `SKILL_TALKING` (см. §7): `INTELLIGENCE` получает `+2` при
`SKILL_TALKING > 9`, `WISDOM` — `+1` при `SKILL_TALKING > 4`
(`decompiled.c:10382-10385`, `10323-10326`).

### 4.3 `LIFE` / `MAXIMUM LIFE` / `ENERGY`

`FUN_1400c280` (`decompiled.c:9416-9444`) — out-параметры:
`out1` (`param_2`) = текущая жизнь, `out2` (`param_3`) = максимум.

```
max_life = FUN_1400c304(manager, id)          // effective STR/CON/level
current  = vtable+0x34(manager, id)           // entity+0x14FC
current  = clamp(current, 0, max_life)
vtable+0x40(manager, id, current)             // -> FUN_14019674(entity,current)
*out1 = current; *out2 = max_life
```

`FUN_1400c1fc` (`decompiled.c:9382-9410`) устроен так же:
`out1` = текущая энергия, `out2` = максимум; читает `vtable+0x38`
(`entity+0x1500`), clamp `[0,max]`, пишет `vtable+0x44`.

Vtable `PTR_FUN_140928c0` (file offset `0x868c0` в `Server.dll`):
`+0x04 = 0x508cc` (`entity+0xFF0`), `+0x34 = 0x511d0`
(`entity+0x14FC`), `+0x38 = 0x51294` (`entity+0x1500`),
`+0x40 = 0x51cfc` (`FUN_14019674`), `+0x44 = 0x51dc0`
(inline `entity+0x1500 = value`).
Функции: `FUN_140511d0` (`decompiled.c:55692`), `FUN_14051294`
(`55727`), `FUN_14051cfc` (`56212`), `FUN_14051dc0` (`56248`).

`MAXIMUM LIFE` — `FUN_1400c304` (`decompiled.c:9452-9476`):

```
L = FUN_1400cbc0(manager, id)                 // level from clamped EXP
v = ((STR_eff >> 1) * 3 + CON_eff * 3) * (L * 0.02f + 1.0f)
v = FUN_14007fb0((float)v, 0x16)
n = FUN_1400619c((float)v)
n = n < 2 ? 1 : (n > 998 ? 999 : n)
```

Максимум энергии — `FUN_1400c3b0` (`decompiled.c:9484-9508`):

```
L = FUN_1400cbc0(manager, id)
v = ((INT_eff >> 1) * 3 + WIS_eff * 3) * (L * 0.01f + 1.0f)
v = FUN_14007fb0((float)v, 0x17)
n = FUN_1400619c((float)v)
n = n < 1 ? 0 : (n > 998 ? 999 : n)
```

Коэффициенты: `0x8dbd4 = 0.02f`, `0x8dbd8 = 1.0f`,
`0x8dbdc = 0.01f`, `0x8dbe0 = 1.0f`. `>> 1` — арифметический сдвиг int32.

Level `FUN_1400cbc0` (`decompiled.c:9837-9860`, использует
`FUN_1400cc84` = clamped EXP):

```
L = 1
if (exp > 99) {
    step = 50
    do { step += 50; L += 1 } while (L * step + step <= exp)
}
if (L > 98) L = 99
return L
```

Точное значение `FUN_14007fb0` для индексов `0x16/0x17` при непустой
таблице модификаторов — `unverified`; при пустой таблице — identity.

### 4.4 Точные формулы getter'ов (декомпиляция)

- `FUN_1400d678` STRENGTH (`decompiled.c:10480-10501`): index `6`,
  base `+0x50`, bonus `+0x16C`.
- `FUN_1400d610` CONSTITUTION (`10453-10474`): index `7`, `+0x54`,
  bonus `+0x170`.
- `FUN_1400d540` DEXTERITY (`10399-10420`): index `9`, `+0x58`,
  bonus `+0x174`.
- `FUN_1400d5a8` PERCEPTION (`10426-10447`): index `8`, `+0x5C`,
  bonus `+0x178`.
- `FUN_1400d4c0` INTELLIGENCE (`10367-10393`): index `10`, `+0x60`,
  bonus `+0x17C`, затем `FUN_1400cb64(id, 0x15) > 9 → +2`.
- `FUN_1400d3d8` WISDOM (`10308-10334`): index `11`, `+0x64`,
  bonus `+0x180`, затем `FUN_1400cb64(id, 0x15) > 4 → +1`.
- `FUN_1400d458` LUCK (`10340-10361`): index `12`, `+0x68`,
  bonus `+0x184`.
- `FUN_1400cc0c` REPUTATION (`9866-9896`): base `+0x6C`; clamp stored к
  `[manager+4, manager+8]`; index `0x1C`; clamp результата к тем же
  границам.
- `FUN_1400cc84` EXPERIENCE (`9902-9918`): base `+0x70`, clamp
  `[0,500000]`.

## 5. Setter'ы: семантика

Все stat-setter'ы вызываются нативом как
`FUN_X(DAT_14088e00, person_id, value, 1)`. Последний аргумент **всегда
1** в нативном пути.

### 5.1 Общий stat-setter (STR/CON/DEX/PER/INT/WIS/LUCK)

Для каждого `FUN_1400b824` / `0xb5e8` / `0xb3ac` / `0xb170` / `0xacf8` /
`0xaf34` / `0xaabc` (`decompiled.c:8937`, `8821`, `8705`, `8589`,
`8357`, `8473`, `8241`):

```
if (value > max_primary || value < 1) return 0;
if (id != manager+0x14) FUN_14007e20(manager, id);
own_bonus = manager[B_i]          // 0x16C / 0x170 / 0x174 / 0x178 / 0x17C / 0x180 / 0x184
base      = manager[b_i]          // 0x50 / 0x54 / 0x58 / 0x5C / 0x60 / 0x64 / 0x68
if (value == base + own_bonus) return 1;
delta = value - (base + own_bonus);
manager[0x16C] += delta;          // <-- ВСЕГДА +0x16C
return 1;
```

Проверено декомпиляцией всех семи и дизассемблером `0x1400acf8`
(`add [ebx+0x16c], ebp` @ `0x1400aeb5`) и `0x1400b5e8`
(`add [ebx+0x16c], ebp` @ `0x1400b7a5`).

Следствие (verified, engine quirk): для `STRENGTH` поле `+0x16C` — это
именно его bonus, поэтому set работает «как ожидается» (при identity
`FUN_14007fb0`). Для остальных шести параметров delta попадает в
**bonus силы**, а собственный bonus и base не меняются. В поставляемых
`.cs` эти шесть setter-веток не вызываются (только `LIFE`/`REPUTATION`),
поэтому квирк не наблюдаем в кампании. Port spec §5.2 сохраняет значение
в runtime; это расхождение с оригиналом для шести неиспользуемых
параметров нужно учитывать в Task 4 (см. §8).

`param_3 == 0` (не используется нативом) — ветка перераспределения
очков: меняет base `+0x5x` и пул `+0x194`; для натива неактуальна.

Setter пишет **кеш manager'а**, а не `entity+0xFF0`. Кеш перезагружается
из `entity+0xFF0` при смене person id (`FUN_14007e20`), поэтому эффект
stat-setter'а для NPC/Hero не персистентен между доступами к другим
персонам (flush в нативном пути не вызывается). `LIFE`/`ENERGY` пишут
entity напрямую.

### 5.2 `REPUTATION` — `FUN_1400aa18` (`decompiled.c:8195-8235`)

```
target = clamp(value, manager+4, manager+8)   // -50..50 по умолчанию
if (id != manager+0x14) FUN_14007e20(manager, id);
current = FUN_1400cc0c(manager, id);          // effective reputation
delta = target - current;
if (delta > 0) {
    if (FUN_1400cb64(id, 2) > 4) delta *= 2;              // SKILL_LGHTMAG
    if (*(DAT_1407fc64+0x8c) != 0 && (manager+0xF8 & 0x400000)) delta >>= 1;
}
if (delta < 0 && FUN_1400cb64(id, 3) > 4) delta *= 2;     // SKILL_DARKMAG
if (delta != 0) {
    vtable+0x4C(manager, id);                 // hook (семантика unverified)
}
manager[0x6C] += delta;                       // base reputation (always)
vtable+0x08(manager, id);                     // always; hook/recompute (unverified)
return 1;
```

Только `vtable+0x4C` условен. Обновление `manager[0x6C]` и вызов
`vtable+0x08` безусловны: они лежат на `LAB_1400aa93`
(`decompiled.c:8231-8233`), который достигается и при `delta == 0`
(ветки `goto LAB_1400aa87`/`LAB_1400aa93` на `8223/8228`). Поэтому
flush/recompute происходит даже при нулевой дельте.

`return 1` игнорируется нативом (наружу всегда `0.0`). Из-за
`round(curve(...))` в getter'е итоговое effective-значение может
отличаться от `target` на величину округления/кривой.

### 5.3 `LIFE` — `FUN_14019674` (`decompiled.c:18436-18447`)

```
if (entity+0xF58 == 0) entity[0x14FC] = value;
else if (entity[0x14FC] <= value) entity[0x14FC] = value;
```

Клапа нет. При `entity+0xF58 != 0` понижение жизни игнорируется
(«только повышение»). Семантика поля `+0xF58` не установлена
(`unverified`); наблюдаемое поведение зафиксировано.

### 5.4 `ENERGY` — inline в `0x409d4`

`natives.c:901-903`; дизассемблер: `mov edx, [DAT_14089750]` /
`mov [ebp+0x1500], edx` @ `0x14040cf1`. Прямая запись int32, без клапа и
пересчётов.

### 5.5 `EXPERIENCE`, `MAXIMUM LIFE`

Веток в `RS_SetPersonParameterI` нет (`natives.c:865-1121`): no-op,
возврат `0.0`. `MAXIMUM LIFE` вычисляется только getter'ом.

## 6. `BT_HERO` stat-блок

### 6.1 Reader/writer (Server.dll)

`BT_HERO ` — 8-байтный тег без NUL + u32 size; body начинается с `+12`
(`docs/research/p5-save-equipment.md` §3).

Reader `FUN_1401e424` (`decompiled.c:21780-21874`):

- body `0x000,0x004,0x008` — 3×u32 (`FUN_1400129c` ×3);
- body `0x00C .. 0x127` — `0x11C` байт копируются в `entity+0xFF0`
  (`decompiled.c:21809`);
- body `0x128` → `entity+0x14FC` (life);
- body `0x12C` → `entity+0x1500` (energy);
- body `0x130` → `entity+0x1504` (третье u32; смысл `unverified`).

Writer `FUN_1401e690` (`decompiled.c:21880-21899`): 3×u32, затем
`(entity+0xFF0, 0x11C)`, затем 3×u32 (`+0x14FC`, `+0x1500`, `+0x1504`),
далее `0x600` из `entity+0x1510` и три item-контейнера. Писатель и
читатель зеркальны.

`FUN_1401d8bc` (`decompiled.c:21261-21295`) — это **не** stat-блок:
он читает 4×0x20 (`entity+0xF5C`, `+0xF7C`, `+0xF9C`, `+0xFBC`), затем
u32 count и `count × 0x14` записей в `entity+0x110C`. Упомянут для
полноты; к 12 параметрам не относится.

### 6.2 Layout (block-relative)

| block offset | поле | getter field | подтверждение |
|---|---|---|---|
| `0x000` | strength | `manager+0x50` | `FUN_140508cc` → `entity+0xFF0`; `FUN_14007e20` копирует 0x11C в `manager+0x50` |
| `0x004` | constitution | `manager+0x54` | то же |
| `0x008` | dexterity | `manager+0x58` | то же |
| `0x00C` | perception | `manager+0x5C` | то же |
| `0x010` | intelligence | `manager+0x60` | то же |
| `0x014` | wisdom | `manager+0x64` | то же |
| `0x018` | luck | `manager+0x68` | то же |
| `0x01C` | reputation | `manager+0x6C` | то же |
| `0x020` | experience | `manager+0x70` | то же |
| `0x02C + 4*id` | skills 0..26 | `manager+0x7C + 4*id` | `FUN_1400cb64` (`decompiled.c:9820`), см. §7 |

Body offset = `0x00C + block offset`. Три u32 после блока — body
`0x128/0x12C/0x130`.

### 6.3 Реальные save (`Saves/slot0.sav`, `slot2.sav`)

Метод: header 112 байт, `zlib.decompress(raw[112:])`, найти `BT_HERO `,
взять body `[off+12, off+12+size)`.

| поле (block offset) | slot0 | slot2 |
|---|---|---|
| `0x000` strength | 30 | 24 |
| `0x004` constitution | 25 | 21 |
| `0x008` dexterity | 18 | 22 |
| `0x00C` perception | 12 | 10 |
| `0x010` intelligence | 5 | 8 |
| `0x014` wisdom | 7 | 8 |
| `0x018` luck | 9 | 10 |
| `0x01C` reputation | −40 | −50 |
| `0x020` experience | 81099 | 65133 |
| body `0x128` life (u32) | 263 | 201 |
| body `0x12C` energy (u32) | 0 | 0 |
| body `0x130` extra (u32) | 5 | 51 |

Фактические размеры: slot0 — decompressed `905051`, `BT_HERO` @
`0xD589B`, block size `0x76B4` (30388); slot2 — decompressed `910169`,
`BT_HERO` @ `0xD5A89`, block size `0x88C4` (35012). Header body:
slot0 `[123,131,1]`, slot2 `[103,133,1]`.

Skill-значения из блока (`0x02C + 4*id`, id по §7), как дополнительное
подтверждение offset'а:

| id | skill | slot0 | slot2 |
|---|---|---|---|
| 0 | `SKILL_GODSMAG` | 0 | 0 |
| 1 | `SKILL_ELEMMAG` | 0 | 0 |
| 2 | `SKILL_LGHTMAG` | 0 | 0 |
| 3 | `SKILL_DARKMAG` | 0 | 0 |
| 4 | `SKILL_SHADMAG` | 0 | 0 |
| 5 | `SKILL_NATRMAG` | 0 | 0 |
| 6 | `SKILL_ATHLETIC` | 10 | 13 |
| 7 | `SKILL_WPN_CRUSH` | 0 | 0 |
| 8 | `SKILL_WPN_STAFF` | 0 | 0 |
| 9 | `SKILL_WPN_DIST` | 0 | 0 |
| 10 | `SKILL_WPN_SPEAR` | 0 | 0 |
| 11 | `SKILL_WPN_THROW` | 0 | 0 |
| 12 | `SKILL_WPN_SWORD` | 0 | 0 |
| 13 | `SKILL_WPN_AXE` | 15 | 15 |
| 14 | `SKILL_WPN_HAND` | 0 | 0 |
| 15 | `SKILL_SCOUT` | 0 | 1 |
| 16 | `SKILL_TRADE` | 1 | 5 |
| 17 | `SKILL_MAGICUSE` | 0 | 0 |
| 18 | `SKILL_STEAL` | 0 | 0 |
| 19 | `SKILL_HACK` | 1 | 1 |
| 20 | `SKILL_HEALING` | 14 | 1 |
| 21 | `SKILL_TALKING` | 0 | 1 |
| 22 | `SKILL_SMITH` | 1 | 5 |
| 23 | `SKILL_IDENTIFY` | 0 | 0 |
| 24 | `SKILL_CRITICAL_HIT` | 15 | 15 |
| 25 | `SKILL_ALCHEMY` | 0 | 0 |
| 26 | `SKILL_TACTIC` | 10 | 10 |

Все значения в `[0,15]` (`ch_max_skill_value`), что согласуется с
`FUN_1400cb64`.

## 7. Skill name → id

### 7.1 Механизм

`RS_GetPersonSkillI` (`natives.c:1598`) вызывает
`(**(code **)(**(int **)(DAT_140891bc + 0x78) + 0x2c))(name, &id)`.

- `DAT_140891bc` — указатель на объект `GoldenLand.exe` `0x46c200`.
- exe `0x42c5fa-0x42c613`: `[0x46c278] = [0x46c308]`; `[0x46c308]`
  создаётся в `FUN_0042e0e8` (`0x42e131-0x42e13e`).
- Объект skill-таблицы: vtable `0x47f808`; `+0x1c = 0x408f80`
  (add: name,id), `+0x2c = 0x408e84` (lookup: name → `record+4` = id),
  возвращает `1` при найденном имени.
- Регистрация (`0x42e14b..0x42e328`): пары `(id, name)` для id `0..0x1a`.

### 7.2 Таблица id → имя

Строки прочитаны из `GoldenLand.exe` (VA `0x485e6c..0x485ffc`):

| id | имя |
|---|---|
| 0 | `SKILL_GODSMAG` |
| 1 | `SKILL_ELEMMAG` |
| 2 | `SKILL_LGHTMAG` |
| 3 | `SKILL_DARKMAG` |
| 4 | `SKILL_SHADMAG` |
| 5 | `SKILL_NATRMAG` |
| 6 | `SKILL_ATHLETIC` |
| 7 | `SKILL_WPN_CRUSH` |
| 8 | `SKILL_WPN_STAFF` |
| 9 | `SKILL_WPN_DIST` |
| 10 | `SKILL_WPN_SPEAR` |
| 11 | `SKILL_WPN_THROW` |
| 12 | `SKILL_WPN_SWORD` |
| 13 | `SKILL_WPN_AXE` |
| 14 | `SKILL_WPN_HAND` |
| 15 | `SKILL_SCOUT` |
| 16 | `SKILL_TRADE` |
| 17 | `SKILL_MAGICUSE` |
| 18 | `SKILL_STEAL` |
| 19 | `SKILL_HACK` |
| 20 | `SKILL_HEALING` |
| 21 | `SKILL_TALKING` |
| 22 | `SKILL_SMITH` |
| 23 | `SKILL_IDENTIFY` |
| 24 | `SKILL_CRITICAL_HIT` |
| 25 | `SKILL_ALCHEMY` |
| 26 | `SKILL_TACTIC` |

### 7.3 Сверка с `.scr`

Person-скрипты (`scripts/persons/*.scr`, пример
`scripts/persons/l0.a11_cock1.scr:42-87`; `scripts/hero.scr:31-133`)
содержат `skills { SKILL_... value }`. Имена в точности совпадают с
таблицей §7.2 (побайтово, case-sensitive); в поставляемых `.scr`
встречаются 23 из 27 имён — `SKILL_TRADE`, `SKILL_STEAL`,
`SKILL_TALKING`, `SKILL_SMITH` не задаются и остаются `0`.
Порт-парсер (`PersonDef.values["skills.<NAME>"]`, см.
`tests/assets/test_person_script.cpp:54`) сохраняет те же ключи.

### 7.4 Вердикт

**`verified`.** Порядок id `0..26` доказан таблицей регистрации в
`GoldenLand.exe`; `+0x2c` — точный lookup по имени с записью id в
out-параметр. Соответствие ключам `skills.<NAME>` — побайтовое.
`RS_GetPersonSkillI` может быть `supported`.

`FUN_1400cb64` (`decompiled.c:9809-9831`) дополнительно:
`id > 0x1a → 0`; `base = block[0x2C + 4*id]`; при `manager+0xC != 0`
`base += manager[0x198 + 4*id]`; clamp `[0, ch_max_skill_value]` (15).
Наружу натив возвращает double от этого int.

## 8. Unverified / open questions

1. **`FUN_14007fb0` при непустой modifier-таблице.** Точные множители
   для индексов `6..0xC`, `0x15`, `0x16`, `0x17`, `0x1C` зависят от
   рантайм-таблицы; статически извлечь не удалось. При пустой таблице
   (наблюдаемый default) функция — identity. Значения getter'ов с
   активными модификаторами могут отличаться.
2. **`manager+0xC` gate и origin `+0x16C..0x184`.** Флаг включает
   добавление бонусов; где он выставляется и как бонусы считаются из
   экипировки/навыков — не прослежено.
3. **Stat-setter квирк.** Для CON/DEX/PER/INT/WIS/LUCK delta пишется в
   `manager+0x16C` (bonus силы), а не в собственный bonus. Это
   подтверждено декомпиляцией и дизассемблером; в поставляемых `.cs`
   эти сеттеры не вызываются, поэтому поведение не наблюдаемо в
   кампании. Task 4 должен либо воспроизвести квирк, либо зафиксировать
   отклонение в `DECISIONS.md`.
4. **`entity+0xF58`** (LIFE setter: запрет понижения) — семантика не
   установлена.
5. **`entity+0x1504` / body `0x130`** (третье u32) — смысл не
   установлен.
6. **Reputation hooks `vtable+0x4C` / `vtable+0x08`** — вызываются
   сеттером; их побочные эффекты не разобраны.
7. **`*(DAT_1407fc64+0x8c)`** — конфиг-флаг, переключающий границы
   репутации `[-50,50]` / `[-100,100]` и некоторые множители; точное
   имя/смысл флага не идентифицированы (вероятно multiplayer, но это
   не доказано).
8. **Нет null-проверки персоны в getter'е** — оригинал при валидном
   имени параметра и отсутствующей персоне делает AV; порт по spec §7
   возвращает default. Отклонение зафиксировано здесь.

## 9. Проверка (фактический вывод)

Константы:

```
0x913d8: bytes=00 00 00 00 00 00 00 00 as_double=0.0
0x913e0: bytes=00 00 00 00 00 00 00 00 as_double=0.0
0x913f0: bytes=00 00 00 00 00 00 00 00 as_double=0.0
0x91400: bytes=00 00 00 00 00 00 00 00 as_double=0.0
0x8d7a0: as_f32=0.5  0x8d7a4: as_f32=-0.5  0x8d7a8: as_f32=0.0
0x8dbd4: 0.02f  0x8dbd8: 1.0f  0x8dbdc: 0.01f  0x8dbe0: 1.0f
```

Real save:

```
slot0: 30 25 18 12 5 7 9 -40 81099; life=263 energy=0 extra=5
slot2: 24 21 22 10 8 8 10 -50 65133; life=201 energy=0 extra=51
```

Skill-id registration (GoldenLand.exe, `0x42e14b..0x42e328`):

```
0 SKILL_GODSMAG ... 26 SKILL_TACTIC   (полный список — §7.2)
```

Валидация документов (UTF-8/BOM/NUL) выполняется отдельной командой
(`$b=[IO.File]::ReadAllBytes(...); bom=...; nul=...`); результат —
`bom=False nul=False`.
