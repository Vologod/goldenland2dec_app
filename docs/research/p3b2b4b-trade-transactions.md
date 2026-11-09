# Phase 1b2b4b: trade transactions research (Client.dll/Server.dll)

Дата: 2026-11-09. Задача: Task 1 плана
`docs/plans/2026-11-09-p3b2b4b-trade-transactions.md`. Только research,
production-код не менялся.

## 0. Вердикт

**`implementable` (scoped).**

- Транзакции **не клиент-локальные**: клиент только раскладывает товар по
  staging-контейнерам и посылает одну команду `0x0E`; сервер переносит
  предметы и деньги (`FUN_1402f0a8`, `FUN_14048518`, `FUN_140484c8`).
  Порядок: **сначала предметы, затем деньги**; деньги — единый net-delta.
- Базовая цена, режимы 0/1 (buy/sell), skill `SKILL_TRADE` (id 16),
  durability, min 1, reciprocal, count-multiply, truncation и
  per-unit-деление в UI — восстановлены. **Цена implementable under
  x87 PC=53** (double); нужен live FPU-CW verification (§4.5/§8).
- Реальный oracle (boat trader, `l13_1`, slot0/slot2) восстановлен:
  выбранный предмет `POT_1_00_27` (id 953), buy/sell цены, сток/деньги
  до/после, узел диалога/ответы и детерминированный путь — §6.
  **Before-state — real/validated; after-state — analytic (computed),
  not observed** (§6.4/§6.5/§7.5).
- **x87 PC.** `_controlfp`/`_control87` не импортируются ни
  `Client.dll`, ни `Server.dll`, ни `GoldenLand.exe`; Windows default
  CW = `0x027F` (PC=53/double). `FUN_1206E8D0` ставит только RC=trunc
  (`or 0xC00`), PC не меняет. Порт обязан считать в **IEEE double**
  (не extended): `fstp qword` округляет до double (nearest), `fistp`
  усекает. При другом PC/RC цена `blocked` (§4.3/§4.5).
- **Ограничение scope:** modifier-блок цены (`rec+0x78 != 0` и
  `(flags & 0x80) == 0`) структурно декодирован, но значения множителей
  находятся в engine/magic-менеджере (`DAT_12110ed8`, передан хостом,
  не в `Client.dll`) и **не восстановлены**. Это затрагивает 25/29
  Hero main записей slot0/slot2 (у всех текущих modifier-записей
  `0x80` снят). Поэтому:
  - **buy** — цена точная только при PC=53 и `mod_count == 0`; при
    `mod_count != 0` — `unsupported` (guard для buy и sell, §9).
    NPC-сток без модификаторов доказан только для slot0/slot2.
  - **sell** предметов без модификаторов (например `POT_1_00_27`) —
    цена точная (PC=53);
  - **sell** предметов с модификаторами — `unsupported` до расшифровки
    magic-def таблиц; **не угадывать** (D-029).
  `gv_addon`/perks (`0x10/0x400`) должны быть сняты до заявки на
  паритет. Полный buy/sell-паритет для magic-предметов — `blocked` на
  этом пункте (§8.2); транзакционный flow и oracle от этого не
  блокируются.

## 1. Provenance / метод

| Файл | SHA-256 |
|---|---|
| `I:\Games\Златогорье-2\Client.dll` | `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16` |
| `I:\Games\Златогорье-2\Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |

Client base `0x12000000`, Server base `0x14000000`. Для Client `.text`
file offset == RVA; `.data1` RVA `0x122000`, raw `0x107000` (offset =
`0x107000 + (RVA-0x122000)`). Для Server `.data1` file offset =
RVA − `0xC000`.

- Client decomp: `.local/decomp/client/decompiled.c` (Ghidra 12.1.4).
- Server decomp: `.local/decomp/server/decompiled.c`, нативы —
  `.local/decomp/server/natives/natives.c`.
- Дизассемблер: `llvm-objdump -d -M intel` из
  `.local/toolchains/llvm-mingw-20261006-ucrt-x86_64`.
- Реальные данные: `Saves/slot0.sav`, `Saves/slot2.sav` (zlib,
  `BT_HERO `, контейнеры от `body+0x734`), `items/data/953.itm`,
  `Data/scripts.pak` (`l13_1.p517_boat_trader.d517.age.cs`).
- Принятый oracle 1b2b4a: `tests/game/test_inventory_runtime_real.cpp`
  (slot0/slot2 trade lists), повторно прогнан — §7.

## 2. Client: staging и confirm

### 2.1 Открытие и контейнеры

`FUN_1203CFD0` case `0xF` (client decompiled.c:46778-46833):

| screen field | источник | смысл |
|---|---|---|
| `+0x190` | local-player Hero entity `+0x10` | **Hero main** (local player) |
| `+0x194` | `local_player+0x66C` | Hero equip |
| `+0x198` | selected person `entity+0xC` (через `FUN_1206EC98`, callers=0) | NPC stock/trade |
| `+0x1A0` | local-player Hero entity `+0xC` | **Hero hot** (local player) |
| `+0x1A4` | `local_player+0x6F8` | **sell staging** (низ) |
| `+0x1A8` | `local_player+0x6FC` | **buy staging** (середина) |
| `+0x700` | `local_player+0x700` | третья staging-ёмкость (в transaction не участвует) |

`+0x6F8/+0x6FC/+0x700` — id ёмкостей, полученные клиентом с сервера
(vtable-setter `0x120C33D8`: пишет эти три поля, `client decompiled.c`
их не содержит; asm `0x120C33D8-0x120C34xx`). На сервере у каждой
person-сущности три staging-контейнера создаются в ctor через
`FUN_1404bc04`: `+0x4E4`, `+0x4EC`, `+0x4E8` (server decompiled.c:
37934-37941), освобождаются при удалении сущности
(`FUN_1404bb9c`, :36523-36532).

### 2.2 Drag/staging (`uVar8 == 0x49`)

`FUN_1207C41C` (RVA `0x7C41C`, client decompiled.c:80796), ветка
`0x49` (:81033-81134):

| найден в списке | вызов | смысл |
|---|---|---|
| Hero main (`+0x184`) | `FUN_120a38e8(uid, +0x190, +0x1A4, 0)` | main → **sell staging** (вся запись) |
| NPC stock (`+0x18C`) | `FUN_120a38e8(uid, +0x198, +0x1A8, 0)` | NPC → **buy staging** (вся запись) |
| sell staging (`+0x1B0`) | `FUN_120a38e8(uid, +0x1A4, +0x190, 0)` | вернуть из sell staging |
| buy staging (`+0x1B8`) | `FUN_120a38e8(uid, +0x1A8, +0x198, 0)` | вернуть из buy staging |
| equip (`+0x188`) | `FUN_120a38e8(uid, +0x194, +0x190, 6)` | снять экипировку |

`FUN_120a38e8` (RVA `0xA38E8`, :106851) ставит move-команду в очередь
контейнер-менеджера (`this+0x20/0x24`, запись `0x18`: uid, mode, 0,
srcId, dstId, count); `count` = вся запись (`+0xB0`), т.е. **переносится
весь стек**, выбора количества нет. Сериализация очереди —
`FUN_120a2b04` (RVA `0xA2B04`, :106319); точная сетевая привязка
`FUN_120a38e8`/`FUN_120a2b04` не трассирована (та же категория gap, что
`FUN_1206EC98` в 1b2b4a), но id staging-контейнеров приходят с сервера.

**Условный single-item путь (known case):** если `count > 1` и
UI-запрос `(*(DAT_12110f48+0x0C))(10)` или `(0xB)` возвращает
nonzero, `FUN_120a38e8` уходит в pending-capture ветку
(`this+0x68 = 1`, `FUN_1209a03c`, `return 1`) вместо постановки
full-stack move (:107119-107136). Это **не** общий quantity selector:
по умолчанию переносится весь стек; условный путь (10/0xB) — отдельный
known case, в Task 2 пока не реализуется (research-only note).

Ошибки staging:

- **Строка заполнена:** перед переносом проверяется число used-записей
  целевого staging-контейнера через `FUN_1205b9cc` (RVA `0x5B9CC`,
  :62016): `if (count > 0xF)` → error-sound `0xa7`, переноса нет
  (:81065-81072 для buy, :81088-81095 для sell). Т.е. максимум **16
  записей** в строке.
- **Деньги/квест нельзя ставить:** hero-main запись типа `0x12`
  (money, type 18) не переносится (:81118-81126); в UI ячейки типа
  `0x12`/`0x19` вообще не рисуются (см. `FUN_120723E8` §4.6), поэтому
  NPC-деньги/квесты недоступны для drag.
- Stock-ошибки на этом шаге нет: переносится существующая запись.

### 2.3 Итоги и cache `+0x254`

`FUN_12071014` (RVA `0x71014`, function start :75577; totals
:75825-75875):

- `+0x24C` = Σ `price(uid, 0)` по **buy staging** (mode 0);
- `+0x250` = Σ `price(uid, 1)` по **sell staging** (mode 1);
- после пересчёта `+0x254 = 1` (:75874); при любом изменении staging
  списков drag-обработчики ставят `+0x254 = 0` (:81060, :81076, :81083,
  :81121).
- На экране: `+0x24C`/`+0x250` — итоги; разница `sell − buy` рисуется
  на y=386 (asm `0x1207176A-0x1207177D`); NPC money y=126, Hero money
  y=612.

### 2.4 Confirm (Exchange) — `FUN_1206DBB0`

Кнопка obj **44** (`trade.scr` `lbig2`, top-left rect 579,320,110×43) → GUI-команда
`uVar8 == 0x27`, `*param_1 == 0x2C` (:81268-81273) →
`FUN_1206dbb0` (RVA `0x6DBB0`, :72319-72357):

```
if (+0x254 == 0) return;                 // нечего обменивать
sell = +0x250; buy = +0x24C;
if (buy <= sell) {                       // игрок получает разницу
    npc_money = money(NPC container +0x198);
    if (npc_money < sell-buy) { error(+8=1,+4=npc_money); return; }
    send 0x0E(delta = sell-buy); +0x254=0; return;
}
hero_money = money(Hero main +0x190);
if (hero_money + sell < buy) { error(+8=-1); return; }
send 0x0E(delta = sell-buy); +0x254=0;   // delta < 0
```

Ошибка — звук/сообщение через `DAT_121109a4+0x30` и
`DAT_12110cd0+4`; в этой же функции команда не посылается, cache не
сбрасывается (исходный quirk `+8/+4` см. §8.7). Отдельного диалога
подтверждения нет: Exchange выполняет обмен сразу после проверки денег.

### 2.5 Команда `0x0E`

`FUN_120C7358` (RVA `0xC7358`, :130158-130167) — только пишет
pending-команду в `DAT_12110f28`: `+0x424 = type`, `+0x428 = arg0`,
`+0x42C = arg1`, `+0x430 = arg2`. Для Exchange:
`type=0x0E, arg0=1, arg1=0, arg2 = sell − buy` (знаковая разница;
положительная — NPC платит Hero, отрицательная — Hero платит NPC).

### 2.6 Ошибки/границы (client)

- **Нет денег у Hero** (buy > sell): error, команды нет.
- **Нет денег у NPC** (sell > buy): error, команды нет.
- **Stock**: на confirm не проверяется (предметы уже в staging;
  сервер делает transfer-all того, что там есть).
- **Нет слота**: проверяется только лимит staging-строки (16 записей,
  `FUN_1205b9cc`, §2.2). У приёмного контейнера лимита слотов нет —
  **inherited proof** из p3b2b3 §3.2 (динамический add/merge/split по
  `max_stack`), live не перепроверялось.
- **Тип 0x12/0x19**: не отображаются/не stage-ятся (money/quest).

## 3. Server: исполнение транзакции

### 3.1 Диспетчер команд

`FUN_140294a0` (RVA `0x294A0`, server decompiled.c:29927-29928):
`(this, entityIndex, cmd, arg0, arg1, arg2)`; `switch(cmd)`.
`case 0xE` (:30292-30294):

```
FUN_1402f0a8(entityIndex, arg0, arg2);
```

Для Exchange клиент присылает `arg0 = 1`, `arg2 = sell − buy`, поэтому
сервер вызывает `FUN_1402f0a8(heroIdx, 1, delta)`. Тот же `case 0xE`
используется сервисами ремонта/опознания/зарядки/снятия проклятия
(`param_2 = 0/2/3/4`).

### 3.2 Trade (`FUN_1402F0A8`, `param_2 == 1`)

RVA `0x2F0A8`, :33449; trade-ветка :33844-33873:

```
hero = entityList[heroIdx];             // (entity+0x14 & 2) обязателен
heroMain = hero+0x10;                   // id контейнера
npcTrade = hero+0x4D4;                  // = NPC entity+0xC (ставится при открытии)
FUN_14048518(hero+0x4E8, heroMain, 0);  // все buy-staged записи -> Hero main
FUN_14048518(hero+0x4E4, npcTrade, 0);  // все sell-staged записи -> NPC trade
if (delta < 1)
    FUN_140484c8(heroMain, npcTrade, -delta);  // Hero платит NPC
else
    FUN_140484c8(npcTrade, heroMain, delta);   // NPC платит Hero
```

- `+0x4E8` — **buy staging** (то, что игрок покупает): после обмена уходит
  в Hero main.
- `+0x4E4` — **sell staging** (то, что игрок продаёт): после обмена
  уходит в NPC trade container (`+0x4D4`).
- `+0x4D4` выставляется при открытии панели в `FUN_14028404`
  (:29303-29309): `hero[0x135] = NPC entity + 0xC`, иначе `-1`.
- Порядок: **оба transfer-all, затем деньги**. Ошибки add/remove не
  проверяются.

### 3.3 Деньги (`FUN_140484C8`)

RVA `0x484C8`, :49211-49226:

```
src = container_lookup(srcId); dst = container_lookup(dstId);
if (dst) FUN_1404cd28(dst, "MON_1_0_1", amount);  // add/merge (split по max_stack)
if (src) FUN_1404cae8(src, "MON_1_0_1", amount);  // remove (partial/over -> удалить найденное)
return 1;
```

- Сначала **добавляет dst**, затем **уменьшает src**; результат обеих
  операций игнорируется.
- Если у src меньше `amount`, `FUN_1404cae8` удаляет найденное
  (p3b2b1 §3) → в теории возможно «создание» денег; в штатном UI
  недостижимо, потому что клиент проверяет деньги до команды (§2.4).
  Это исходный quirk; порт добавляет guard — deviation D-058.
- Money item — обычный `MON_1_0_1` (type 18) в контейнерах:
  Hero main `+0x10`, NPC trade `+0x4D4 = NPC+0xC`.

### 3.4 Transfer-all / merge

`FUN_14048518` (RVA `0x48518`) — все used-записи source в slot-order
переносятся в dest через `FUN_1404a04c` (mode 0, count −1) →
`FUN_1404cd28`: merge по item id, лимит `max_stack` (`+0x34`), split на
новые записи; source-записи удаляются; ошибки не проверяются
(p3b2b3 §3-5). Для `POT_1_00_27` `max_stack = 10000`, поэтому Hero
51+14 = 65 сливается в одну запись.

### 3.5 Отмена/закрытие (rollback)

`FUN_1402ABD8` (RVA `0x2ABD8`), `param_2 == 0xD` (:30853-30872):

```
FUN_14048518(hero+0x4E4, heroMain +0x10, 0);      // sell staging -> Hero main
FUN_14048518(hero+0x4E8, hero+0x4D4, 0);          // buy staging  -> NPC trade
```

Т.е. при завершении/отмене трейда staged-предметы возвращаются:
`+0x4E4` → Hero main, `+0x4E8` → NPC trade. Точный клиентский триггер
действия `0xD` (команда `0x05` / смена action-состояния) не трассирован
(§8.6).

### 3.6 Жизненный цикл staging-контейнеров

- 3 контейнера `+0x4E4/+0x4E8/+0x4EC` создаются в ctor каждой
  person-сущности (:37934-37941) и удаляются вместе с сущностью
  (:36523-36532). Они не создаются заново на каждый трейд и должны быть
  пусты между сделками (exchange/rollback их опустошает).
- `+0x4D8` — field, сбрасываемый при открытии трейда/диалога
  (:29189, :29237); в transaction не участвует.
- `+0x4EC` — третья ёмкость; в trade-ветке не используется.

## 4. Цена

### 4.1 Входы и поля записи

`FUN_1206E8D0` (RVA `0x6E8D0`, client decompiled.c:72885-72993).

| поле | смещение | смысл |
|---|---:|---|
| type | `+0x08` | тип (18 money, 19 potion, 25 quest) |
| flags | `+0x0C` | bit `0x80` — gate modifier-блока; bit `0x2` — quest-display в addon |
| ITM price | `+0x10` | базовая цена (`price` из `items/data/<id>.itm`) |
| mod count | `+0x78` | число modifier-записей |
| mod array | `+0x7C` | указатель на записи stride `0x14` |
| uid | `+0xA4` | instance uid |
| item id | `+0xA8` | prototype id |
| count | `+0xB0` | количество в записи |
| durability | `+0xB4` | 0..100 |

Skill берётся **Hero/local player**: `FUN_120c54dc(DAT_12110f28)` →
`FUN_1201fab4(DAT_12110efc, idx, 0x10)` (id `0x10 = SKILL_TRADE`,
RVA `0x1FAB4`, :24195). Значение в save: `BT_HERO body+0x038+4*16`
(slot0=1, slot2=5; §7). NPC-навык в формулу не входит.

### 4.2 Константы (Client.dll, `_DAT_12128fd0..129000`)

| RVA | file off | double | роль |
|---|---:|---:|---|
| `0x128FD0` | `0x10DFD0` | `0.85` | addon perk `0x400` |
| `0x128FD8` | `0x10DFD8` | `-1.0` | addon: `(f-1)` |
| `0x128FE0` | `0x10DFE0` | `1.0` | min factor/base 1 |
| `0x128FE8` | `0x10DFE8` | `-0.1` | skill slope |
| `0x128FF0` | `0x10DFF0` | `2.5` | skill intercept |
| `0x128FF8` | `0x10DFF8` | `0.0025` | durability slope |
| `0x129000` | `0x10E000` | `0.75` | durability intercept, addon perk `0x10` |
| `0x129008` | `0x10E008` | `0.0` | default modifier multiplier |

`0x129010/0x129014` — unsigned-int→float correction (`0.0f`/`2^32f`),
не часть формулы.

### 4.3 Формула (PC=53)

Asm `0x1206E967-0x1206EC5C`; арифметика x87. **Точное integer-значение
зависит от x87 precision control (PC).** `Client.dll`, `Server.dll` и
`GoldenLand.exe` не импортируют `_controlfp`/`_control87` (0 вхождений
в импортах и строках), поэтому действует Windows default CW = `0x027F`:
PC=53 (double), RC=nearest. `FUN_1206E8D0` перед каждым `fistp` делает
`fnstcw; or 0xC00; fldcw` (это ставит RC=trunc и **не меняет PC**), затем
восстанавливает CW. Значит промежуточные значения — IEEE double (не
extended): `fstp qword` округляет до double (nearest), `fistp` усекает.
`ITM.price` и `durability` трактуются как unsigned int. При PC=64
промежуточные были бы extended и могли бы изменить trunc на границе;
поэтому PC обязателен. Цена **implementable under PC=53**; live FPU-CW
verification — §4.5/§8.

```
// PC=53: промежуточные — IEEE double; fstp qword rounds, fistp trunc
base0 = trunc( (durability * 0.0025 + 0.75) * ITM.price )
base  = base0
if ((flags & 0x80) == 0 && mod_count != 0) {
    for each mod in mod[0..mod_count):
        base = trunc(base + mod_value_contribution(mod))   // §4.4
}
if (base < base0 / 3) base = base0 / 3        // integer division (trunc)
skill = Hero SKILL_TRADE                      // id 0x10, clamp getter
f = 2.5 - 0.1 * skill
if (gv_addon != 0) {                          // DAT_12106bc0+0x8C
    if (perk & 0x10)  f = (f - 1.0) * 0.75 + 1.0
    if (perk & 0x400) f = (f - 1.0) * 0.85 + 1.0
}
if (f < 1.0) f = 1.0
if (mode != 0) f = 1.0 / f                    // sell: reciprocal (double)
unit = trunc(f * base)
if (unit < 1) unit = 1
return unit * count
```

- `mode == 0` — **buy** (игрок платит): `f = 2.5 − 0.1*skill`.
- `mode != 0` — **sell** (игрок получает): `1/f`.
- `durability = 100` → `base0 = ITM.price` (свежий NPC-лут всегда 100;
  в slot0/slot2 все Hero main записи имеют `+0xB4 = 100` — §7).
- min 1 применяется к **unit**, затем умножение на count; умножение —
  целочисленное, после trunc.
- Reciprocal **не точен**: buy 1200 / sell 208 при base 500 (skill 1);
  1000 / 250 (skill 5).
- Порт обязан считать в C++ `double` (не `long double`/extended);
  окончательное int-преобразование — trunc; double-store (`fstp qword`)
  округляет к nearest. При live PC != 53 цена `blocked` (§4.5/§8).

### 4.4 Modifier-блок (частично декодирован; значения — missing)

Если `(flags & 0x80) == 0` и `mod_count(+0x78) != 0`, для каждой
записи `mod` (stride `0x14`):

| mod field | смещение | смысл |
|---|---:|---|
| id | `+0x00` | ключ magic/effect def |
| value | `+0x04` | signed значение |
| flags | `+0x08` | `&0x1c0` — class selector; `&0x3f` — type selector |
| ? | `+0x0C` | не декодировано |
| ? | `+0x10` | `-1` в текущих данных |

Asm `0x1206E9C0-0x1206EB19`:

```
def = (*DAT_12110ed8 + 0x1c)(mod.id);           // engine magic manager
cls = mod.flags & 0x1c0; typ = mod.flags & 0x3f;
classMul = [def+0x74, def+0x84, def+0x94][index of cls in {def+0x68,def+0x78,def+0x88}] else 0.0
typeMul  = [def+0x14,0x24,0x34,0x44,0x54,0x64][index of typ in {def+0x08,0x18,0x28,0x38,0x48,0x58}] else 0.0
base = trunc(base + mod.value * classMul * typeMul)   // один fistp
```

Внутреннего trunc нет: произведение `mod.value*classMul*typeMul`
складывается с `base` в x87 (PC=53/double) и только затем один `fistp`
с RC=trunc на итерацию.

**Чего не хватает:** содержимого def-таблиц (`classMul`/`typeMul`) и
семантики селекторов. `DAT_12110ed8` приходит из host API
(`GetClientAPI` exported, client decompiled.c:131927-131987;
`DAT_12110ed8 = *(iVar5+0x84)`), т.е. это engine/magic-менеджер
(`GoldenLand.exe`/загружаемые magic-данные), а не `Client.dll`. Без его
расшифровки точную цену modifier-предметов получить нельзя.

Факты по save (Python, §7): slot0 — 25 из 117 Hero main записей имеют
`+0x78 != 0`; slot2 — 29 из 140; у всех них `(flags & 0x80) == 0`,
поэтому блок активен. NPC-сток (`pass 1`) имеет `+0x78 = 0` у всех
записей (modifier-блок не выполняется). Примеры: item 953 `modn=0`;
item 411 `modn=1` (`id=7,value=1,flags=0x1`); item 932 `modn=9`.

### 4.5 Округление / FPU control word

- `base0`, `base` после каждого модификатора, `unit` — `fistp` с
  RC=trunc (`or 0xC00` перед каждым `fistp`).
- `f` — double; reciprocal перед trunc. `fstp qword [esp+0x20]`
  сохраняет `f` в double с RC=nearest (default CW) — это округление
  **store**, не финального int.
- `base0/3` — integer division (asm magic `0x55555556`), trunc.
- Round-to-nearest на **финальном int-преобразовании нет** (там trunc),
  но double-store (`fstp qword`) округляет к nearest, а промежуточная
  арифметика — IEEE double только при **PC=53**.
- **Live FPU-CW verification (обязательный шаг):** при открытии trade
  прочитать CW; ожидается PC=53 (bits 8-9 = `10`) и RC=nearest **до**
  входа в `FUN_1206E8D0`; функция ставит только RC=trunc и
  восстанавливает CW. Если live PC/RC иные — цена `blocked` (не
  считать, не угадывать).

### 4.6 Per-unit и UI

`FUN_120723E8` (RVA `0x723E8`, :76241): per-unit = `price(uid,mode) /
count` **integer division**; затем число рисуется в ячейке. Пропускаются
типы `0x12` (money) и `0x19` (quest): non-addon — всегда; addon — quest
показывается только при `(flags & 2) == 0` (:76284-76294). Поэтому
деньги/квесты нельзя выбрать в trade-строках.

Режимы по спискам: NPC stock (`FUN_12070198`) → mode 0 (buy);
Hero main (`FUN_1206FBC0`) → mode 1 (sell); equip/hot цену не считают
(1b2b4a §4.3).

## 5. Деньги/контейнеры и инварианты

- Money item — `MON_1_0_1` (type 18), обычные записи контейнеров.
- NPC money = сумма `MON_1_0_1` в NPC trade `entity+0xC` (`+0x4D4`).
- Hero money = сумма `MON_1_0_1` в Hero main `entity+0x10`.
- Buy: item `+0x4E8 → Hero main`; money `Hero main → NPC trade`.
- Sell: item `+0x4E4 → NPC trade`; money `NPC trade → Hero main`.
- Money — **net-delta**, не две транзакции: `sell_total − buy_total`;
  положительный — NPC→Hero, отрицательный — Hero→NPC.
- Инварианты (при достаточных деньгах):
  - сумма item count по `Hero main + NPC trade + staging` сохраняется;
  - сумма money по `Hero main + NPC trade` сохраняется;
  - staging пуст после exchange/rollback;
  - `max_stack` merge/split сохраняет суммарное count;
  - при недостатке денег клиент не посылает команду → мутаций нет.
- Сервер не проверяет деньги/сток: add-then-remove с игнором ошибок;
  при недостатке src деньги могут «создаться» (недостижимо штатным UI).
  Порт должен валидировать до мутации — D-058.

## 6. Реальный oracle: `L13_1.P517_Boat_Trader` (`l13_1`)

### 6.1 Детерминированный путь / node / answers

- Level `l13_1`, person index **3** (`--click-npc 3`) —
  `L13_1.P517_Boat_Trader` (`scr_dialog =
  l13_1\l13_1.p517_boat_trader.d517.age`, `scr_inv = inv013_barman`).
- Диалог (`glpak disasm-cs`, `scripts/dialogs/l13_1/
  l13_1.p517_boat_trader.d517.age.cs`):
  - node 132-145: `D_Say(8868)`; ответы node 146 `3498`, node 154 `4214`;
  - `LastAnswer == 3498` (node 119-124) → node 125-131:
    `result = RS_PassToTradePanel()` (bytecode 127, `0x700000D`),
    затем `D_CloseDialog(0)` (bytecode 130-131).
  - Ответ **индекс 0** = `3498`; `4214` — вторая ветка (покупка лодки,
    `L13_Tarvik_BuyBoat`, `RS_PersonRemoveItem(Hero,MON,200)`).
- `glscript --run --answer 0`: `dialogs: 8868 3498 4214`,
  `answers: 2`, `status: end`, native `RS_PassToTradePanel` вызван.
- Детерминированный вход: `--level l13_1 --equip-save 0|2 --seed 1`,
  `--trade-npc L13_1.P517_Boat_Trader` (или click NPC 3 → answer 0).
- В UI: NPC-стек → средний ряд (buy staging), Hero-стек → нижний ряд
  (sell staging), затем obj 44 (Exchange).

### 6.2 Исходное состояние (до сделки)

| ключ | slot0 | slot2 |
|---|---:|---:|
| loot level / seed | 41 / 1 | 37 / 1 |
| NPC money (`+0xC`, `MON_1_0_1`) | **42142** | **48908** |
| Hero money (`+0x10`) | **43875** | **122008** |
| `SKILL_TRADE` (id 16) | **1** | **5** |
| NPC `POT_1_00_27` (id 953) count | **14** | **17** |
| Hero `POT_1_00_27` count | **51** | **25** |
| NPC records / Hero main records | 16 / 117 | 17 / 140 |
| gv_addon | не снят; oracle — 0 (retail default) | то же |

**Before-state — real/validated** (save + принятый NPC-loot + ITM),
проверено real-тестом `tests/game/test_inventory_runtime_real.cpp`
(slot0 69 assertions, slot2 71 assertions, pass) и `glgame --trade-npc`
(`npc_items=16/42142`, `17/48908`).

### 6.3 Выбранный предмет и цены

`POT_1_00_27` (id 953): type 19, flags `0x8000000C`, `max_stack 10000`,
`ITM price = 500`, durability 100, `modn = 0` — modifier-блок не
участвует. Цены ниже — **analytic (computed) under PC=53/double**;
live FPU-CW verification ещё не снят (§4.5/§8).

| | slot0 (skill 1) | slot2 (skill 5) |
|---|---:|---:|
| base | 500 | 500 |
| buy unit (`trunc((2.5−0.1s)*500)`) | **1200** | **1000** |
| sell unit (`trunc(500/(2.5−0.1s))`) | **208** | **250** |
| buy whole stack | 14×1200 = **16800** | 17×1000 = **17000** |
| sell whole stack | 51×208 = **10608** | 25×250 = **6250** |

### 6.4 Buy (весь NPC-стек 953)

**After-state — analytic (computed), not observed.** Строки «до ...» —
real/validated before-state; «после» и суммы — расчёт по формуле §4.3
(PC=53) и модели переноса §3.2, не чтение памяти игры.

| | slot0 | slot2 |
|---|---:|---:|
| до: Hero money / NPC money | 43875 / 42142 | 122008 / 48908 |
| до: Hero 953 / NPC 953 | 51 / 14 | 25 / 17 |
| цена | 16800 | 17000 |
| после: Hero money / NPC money | **27075 / 58942** | **105008 / 65908** |
| после: Hero 953 / NPC 953 | **65 / 0** | **42 / 0** |
| сумма item / money | 65 / 86017 | 42 / 170916 |

Merge: Hero 51+14 (slot0) / 25+17 (slot2) в одну запись (max_stack
10000); NPC-запись удаляется (count 0).

### 6.5 Sell (весь Hero-стек 953)

**After-state — analytic (computed), not observed.** Before-state —
real/validated; «после»/суммы — расчёт (PC=53), не live-наблюдение.

| | slot0 | slot2 |
|---|---:|---:|
| до: Hero money / NPC money | 43875 / 42142 | 122008 / 48908 |
| до: Hero 953 / NPC 953 | 51 / 14 | 25 / 17 |
| цена | 10608 | 6250 |
| после: Hero money / NPC money | **54483 / 31534** | **128258 / 42658** |
| после: Hero 953 / NPC 953 | **0 / 65** | **0 / 42** |
| сумма item / money | 65 / 86017 | 42 / 170916 |

Hero-запись удаляется; NPC получает стек и сливает с существующим
(14+51 / 17+25).

### 6.6 Проверяемые ошибки/границы

| сценарий | ожидание |
|---|---|
| buy при Hero money < buy_total | нет команды/мутаций; error |
| sell при NPC money < sell_total | нет команды/мутаций; error |
| staging-строка уже 16 записей | error-sound `0xa7`, запись не stage-ится |
| money (type 18 / `0x12`) / quest (type 25 / `0x19`) | не рисуется/не выбирается |
| повторный Exchange без staging | cache `+0x254 = 0` → no-op |
| buy, когда NPC-запись уже удалена | сервер молча переносит что есть; порт — guard/no-op, D-058 |

## 7. Проверка (фактический вывод)

### 7.1 Хэши/константы

```
Client.dll FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16
Server.dll 418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837
0x10DFD0 0.85   0x10DFD8 -1.0   0x10DFE0 1.0
0x10DFE8 -0.1   0x10DFF0 2.5    0x10DFF8 0.0025
0x10E000 0.75   0x10E008 0.0
```

FPU: byte-scan Client.dll/Server.dll/GoldenLand.exe —
`_controlfp`/`_control87`/`control87`/`controlfp` = 0 вхождений;
`FUN_1206E8D0` меняет только RC (`or 0xC00`), PC не меняет. Windows
default CW `0x027F` (PC=53/double, RC=nearest). Live CW в игре не
снят — обязательный шаг верификации (§4.5/§8.1).

### 7.2 ITM / save

```
items/data/953.itm: type=19 flags=2147483660 max_stack=10000 price=500
slot0: BT_HERO @0xD589B size=0x76B4; containers 8/117/11;
       money=43875; SKILL_TRADE(base+0x038+4*16)=1;
       id953 (count=51,price=500,dur=100,flags=0x8000000C,mod=0);
       mod records 25/117; all (flags&0x80)==0; dur histogram {100:117}
slot2: BT_HERO @0xD5A89 size=0x88C4; containers 9/140/11;
       money=122008; SKILL_TRADE=5;
       id953 (count=25,price=500,dur=100,flags=0x8000000C,mod=0);
       mod records 29/140; all (flags&0x80)==0; dur histogram {100:140}
```

### 7.3 Реальный oracle / тесты

```
gl_game_tests --test-case="real data: l13_1 slot0 trade lists match the research oracle"
  [doctest] test cases: 1 | 1 passed | 0 failed | 235 skipped
  [doctest] assertions: 69 | 69 passed | 0 failed
gl_game_tests --test-case="real data: l13_1 slot2 trade lists match the research oracle"
  [doctest] test cases: 1 | 1 passed | 0 failed | 235 skipped
  [doctest] assertions: 71 | 71 passed | 0 failed
```

### 7.4 glgame / glscript

```
slot0: trade: opened npc='L13_1.P517_Boat_Trader' npc_items=16 npc_money=42142
       hero_items=117 hero_money=43875
slot2: trade: opened npc='L13_1.P517_Boat_Trader' npc_items=17 npc_money=48908
       hero_items=140 hero_money=122008
glscript: dialogs: 8868 3498 4214 / answers: 2 / status: end
          [warning] script native RS_PassToTradePanel: no trade panel handler
```

### 7.5 Python-референс цены/сделки

**Analytic (computed), not observed.** Before-state — real/validated
(§6.2); after-state — расчёт по §4.3 под PC=53/double. Live FPU-CW не
снят; Python `float` — IEEE double, что соответствует PC=53.

```
slot0 loot=41 skill=1 npc_953=14 hero_953=51 hero_money=43875 npc_money=42142
  base=500 buy_unit=1200 sell_unit=208 buy_total=16800 sell_total=10608
  BUY  stack 14: hero_money 43875->27075 npc_money 42142->58942
                 hero_953 51->65 npc_953 14->0 item_total=65 money_total=86017
  SELL stack 51: hero_money 43875->54483 npc_money 42142->31534
                 hero_953 51->0 npc_953 14->65 item_total=65 money_total=86017
slot2 loot=37 skill=5 npc_953=17 hero_953=25 hero_money=122008 npc_money=48908
  base=500 buy_unit=1000 sell_unit=250 buy_total=17000 sell_total=6250
  BUY  stack 17: hero_money 122008->105008 npc_money 48908->65908
                 hero_953 25->42 npc_953 17->0 item_total=42 money_total=170916
  SELL stack 25: hero_money 122008->128258 npc_money 48908->42658
                 hero_953 25->0 npc_953 17->42 item_total=42 money_total=170916
```

## 8. Unverified / missing evidence

1. **x87 PC/CW (price gate).** Точное integer-значение цены зависит от
   x87 PC. `_controlfp`/`_control87` нигде не импортируются; Windows
   default CW = `0x027F` (PC=53/double, RC=nearest), функция цены
   меняет только RC (`or 0xC00`) и восстанавливает CW. Цена
   **implementable under PC=53**; нужен live FPU-CW verification
   (§4.5). Если live PC != 53 (или RC/PC иные до входа) — цена
   `blocked`, не считать.
2. **Modifier-цены (главный gap).** `classMul`/`typeMul` и def-таблицы
   magic-менеджера (`DAT_12110ed8`, engine/host, `vtable+0x1c`,
   `def+0x14/+0x74`) не расшифрованы. Затрагивает 25/29 Hero main
   записей. Без этого точный **sell** magic-предметов невозможен.
   Нужно: реверс magic-def менеджера в `GoldenLand.exe` (или
   загрузчика magic-данных) + live/память-оракул. До этого —
   `unsupported` (не угадывать).
3. **Buy scope / modifier guard.** Buy тоже должен возвращать
   `unsupported` при `mod_count != 0` (и `(flags&0x80)==0`), не только
   sell. NPC-сток без модификаторов доказан **только для slot0/slot2**
   (`L13_1.P517_Boat_Trader`); другие уровни/торговцы не сканировались.
   `gv_addon`/perks (`0x10/0x400`) должны быть сняты до заявки на
   паритет.
4. **Клиентская привязка staging id.** `local_player+0x6F8/+0x6FC`
   пишет vtable-setter `0x120C33D8` (asm), но caller/сетевой пакет и
   точный порядок трёх id (`+0x4E4/+0x4E8/+0x4EC`) не трассированы.
   Роли контейнеров доказаны на сервере (+0x4E4→NPC, +0x4E8→Hero) и
   на клиенте по totals (mode 1/0), поэтому на transaction не влияет.
5. **`gv_addon`** (`DAT_12106bc0+0x8C`): значение для slot0/slot2 не
   снято (в retail ожидается 0); при `!=0` включаются perk-модификаторы
   `0x10/0x400`. Oracle — для `gv_addon=0` (или Hero без этих perks);
   до capture паритет не заявлять.
6. **Точный триггер rollback `action 0xD`** и клиентская команда
   закрытия/отмены не трассированы; сам rollback доказан.
7. **Исходный quirk `+8/+4`** в `FUN_1206DBB0`: при ошибке деньги
   кладутся в `screen+8/+4`, которые `FUN_12070DAC` (:75503) может
   отправить как `0x0E` на следующем input-событии. Порт не должен
   воспроизводить этот баг; deviation — **D-058**.
8. **Stock/деньги на сервере не валидируются**; add-then-remove с
   игнором ошибок. Порт — guard до мутации; deviation — **D-058**.
9. **Условный single-item путь** (`count>1` + query `10|0xB`,
   §2.2) — known case; в Task 2 не реализуется, пока не подтверждён.
10. **Slot-limit claim (inherited proof).** Лимит staging-строки 16 —
    из `FUN_1205b9cc` (re-proven по коду); отсутствие лимита слотов у
    приёмного контейнера — **inherited proof** из p3b2b3 §3.2
    (dynamic add/merge/split по max_stack), live не перепроверялось.
11. **Live-оракул** (память запущенной игры) не снимался; oracle —
    save + принятый NPC-loot 1b2b4a + ITM + реальный `.cs` + Python;
    after-state — analytic, не observed.
12. **`+0x4EC`** третья staging-ёмкость и её роль в других action
    (drop/loot) не исследована; в trade не используется.

## 9. Следствия для Task 2

1. `TradePrice` (только при PC=53/double):
   - `base0 = trunc((dur*0.0025+0.75)*price)`;
   - `f = 2.5 − 0.1*skill`, `gv_addon`/perks, clamp 1;
   - buy `unit = trunc(f*base)`, sell `unit = trunc(base/f)`, min 1;
   - total = `unit * count`; per-unit UI = `total / count` (integer).
   - **guard (buy и sell):** если `mod_count != 0 && (flags&0x80)==0`
     → `unsupported`/`unpriced`, не считать modifier-блок; UI «—», без
     транзакции. `gv_addon`/perks снять до паритета.
   - если live FPU CW даёт PC != 53 (или RC/PC иные до входа) →
     `unsupported`/blocked; не считать.
2. `InventoryRuntime::buy_trade(npc, hero, tech, price)` — **без `count`**:
   - одна операция переносит **весь выбранный стек**; count берётся из
     runtime-записи (источника);
   - проверить Hero money ≥ total, NPC stock ≥ count;
   - NPC trade → Hero main (add/merge по max_stack), Hero money →
     NPC trade;
   - ошибки не мутируют.
3. `InventoryRuntime::sell_trade(...)`: весь стек Hero main → NPC trade,
   NPC money → Hero main; зеркально; только `modn==0` и PC=53.
4. Количество: whole-stack per action — подтверждено человеком; общего
   quantity selector нет. Условный single-item путь (`count>1` + query
   `10|0xB`, §2.2) — known case, в Task 2 не реализуется, пока не
   подтверждён. Staging-строки/лимит 16 — UI-деталь, не менять
   деньги/стоки.
5. Деньги — net-delta для стадии; для атомарной кнопки — просто
   `total` в одну сторону.
6. Не реализовывать: modifier-цены (buy+sell unsupported),
   `+8/+4`-quirk, серверный add-before-remove без проверки (в порту —
   guard), `RS_PersonAddItemToTrade/RemoveItemToTrade` (вне scope),
   save. D-058 — **recorded** (Task 5).

## 10. Приложение A. Python-референс (analytic, PC=53)

```python
def trunc(x): return int(x)  # toward zero

def unit_price(itm_price, durability, skill, mode):
    base = trunc((durability * 0.0025 + 0.75) * itm_price)
    f = 2.5 - 0.1 * skill
    if f < 1.0: f = 1.0
    if mode != 0: f = 1.0 / f
    u = trunc(f * base)
    return max(u, 1)
```

**Analytic (computed), not observed.** Python `float` — IEEE double;
это соответствует PC=53, но live FPU-CW в игре не снят (§4.5/§8.1).
Референс не учитывает modifier-блок (только `modn==0`); buy/sell
modifier-предметов — `unsupported` (guard §9.1), `gv_addon`/perks
(`0x10/0x400`) не моделируются. NPC-stock без модификаторов доказан
только slot0/slot2. Полный скрипт — в отчёте Task 1; он не входит в
репозиторий.

Документ: UTF-8 без BOM, `nul=0`.
