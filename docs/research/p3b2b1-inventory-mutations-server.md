# Research: inventory mutation natives (`Server.dll` v1.3)

Дата: 2026-11-06. Задача: Phase 1b2b1, Task 1 (research only).
Источник истины — `Server.dll` v1.3; layout записей/ITM — по уже
принятым research (`p5-save-equipment.md`, `p5b1-inventory-ui.md`), где
это указано явно.

## 1. Provenance и метод

| Файл | SHA-256 |
|---|---|
| `I:\Games\Златогорье-2\Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `I:\Games\Златогорье-2\Saves\slot0.sav` | `8AC545FE9715EF11C68B1CCA8617BEB03003ECA360F07F71C26E3FC5E5C73442` |
| `I:\Games\Златогорье-2\Saves\slot2.sav` | `5DB504F57BDFFB66A8B977256AD53EE8E1D6D283E1EDFCD75092FFFD3DB30296` |

- Image base `Server.dll` = `0x14000000`; RVA = VA − `0x14000000`.
- Корпус (gitignored): `.local/decomp/server/decompiled.c`,
  `.local/decomp/server/natives/natives.c` (65 обработчиков).
- Дизассемблер: `llvm-objdump.exe -d -M intel` из
  `llvm-mingw-20261006-ucrt-x86_64` (файл `Server.dll`).
- Save: Python `zlib.decompress(raw[112:])`, поиск `BT_HERO `, body =
  `[off+12, off+12+size)`; контейнеры от `body+0x734`, запись
  `0xC0 + 4 + u32(+0xC0)*0x14`.
- ITM: `glpak decode-itm`; `.cs`: `glpak disasm-cs`; вызовы с
  разрешёнными аргументами — `%TEMP%\person_calls_resolved.txt`.

Пути `.cs`/`.scr`/`.itm` ниже — внутри `scripts.pak`/`items.pak`; они не
коммитятся.

## 2. Нативы `0x5000004` / `0x5000005`

| ID | name | handler RVA | signature | return | side effects |
|---|---|---|---|---|---|
| `0x5000004` | `RS_PersonAddItem` | `0x41320` | 3 args: `string,string,num` (`3,3,0`) | `0.0` (`DAT_14091498`) | add/stack + container state + notify |
| `0x5000005` | `RS_PersonRemoveItem` | `0x41478` | 3 args: `string,string,num` (`3,3,0`) | `0.0` (`DAT_140914a8`) | remove + container state + notify |

Оба обработчика:

```
if (!signature(3,3,0,-1)) return 0.0;       // FUN_14038c6c, push -1,0,3,3
p = find_person(person);                     // FUN_1402914c (type 2, +0x150==0)
if (p == 0) p = find_entity_0x10(person);    // FUN_14029270 (type (…&0x3E)==0x10)
c = lookup_container(manager, p->+0x10);     // FUN_1404bf8c; p не проверяется на 0
count = arg[2] as uint (DAT_14089750);       // int32 из VM (ROUND(double))
if (count != 0) {
    add/remove(c, tech, count);              // FUN_1404cd28 / FUN_1404cae8
    c->+0x18 = c->+0x18 == 0 ? 1 : 2;        // write-only state marker
    if (tech != 0 && tech_id_lookup(tech, &id)) {
        notify(…, id, count);                // FUN_140482e8
    }
}
return 0.0;
```

Ключевые отличия add/remove от query-нативов (`p3b2a`): порядок
аргументов — `(person, tech, count)`, третий аргумент — число; person
и container **не проверяются на 0** (отсутствующая персона/контейнер —
AV в оригинале, см. §9).

Константы возврата прочитаны из PE (RVA → file offset = RVA − `0xC000`):

```
0x91498 off=0x85498 bytes=00 00 00 00 00 00 00 00 double=0.0  (Add)
0x914a8 off=0x854a8 bytes=00 00 00 00 00 00 00 00 double=0.0  (Remove)
```

### 2.1 Порядок аргументов и `count`

`FUN_14039054`-подобный marshalling (`decompiled.c:40120-40187`) кладёт
аргументы в `DAT_14089740` (arg0, string), `DAT_14089748` (arg1,
string), `DAT_14089750` (arg2). Для `num`-узла (`+0x3c == 0x18`) значение
вычисляется как `(int)ROUND(double)` (`40173-40175`), т.е. отрицательный
double даёт отрицательный `int32`, который нативы читают как `uint`.
Следствие: `count` в `FUN_1404cd28`/`FUN_1404cae8` — **беззнаковый**;
поведение отрицательного count — см. §3.4/§4.5, это не «no-op».

## 3. Add: `FUN_1404cd28` (`decompiled.c:52606-52739`, RVA `0x4cd28`)

Подпись по дизассемблеру: `__thiscall(container, tech, count, extra)`,
где `extra` всегда `0` из натива (`push 0` на `0x1404137b`). `count`
читается как `uint`.

### 3.1 Алгоритм

```
if (count == 0) return -1;
if (!tech_id_lookup(tech, &id)) return -1;
for each used slot i in container (0 .. container->+0x10-1):
    rec = item_record(container->ids[i]);          // global record pool
    if (rec && rec->+0xA8 == id):
        max = rec->+0x34;                          // stack limit
        if (max < 2) break;                        // never stack non-stackables
        cur = rec->+0xB0;
        if (cur >= max) continue;                  // this stack is full
        n = min(count_remaining, max - cur);
        rec->+0xB0 = cur + n;
        count_remaining -= n;
        if (count_remaining == 0) return rec->+0xA4;
// remaining > 0: create new records
while (count_remaining != 0):
    tpl = item_template(tech);                     // factory -> ITM definition
    max = tpl->+0x30;                              // same value as rec->+0x34
    n = min(count_remaining, max);
    new_rec = FUN_1404e3a4(container, tech, 0, n);
    if (new_rec == -1) return -1;
    count_remaining -= n;
return new_rec or -1;
```

### 3.2 Stacking и лимит

- Стекинг — **по item id** (`rec+0xA8 == id`, id из `tech_names.sdb` через
  vtable `DAT_14088dfc+0x2c`, exact/case-sensitive).
- Лимит стека — `rec+0x34`. Save-проверка (§7.1): у money `812`
  `+0x34 = 100000000`, у potion `953`/reagent `444`/food `407`
  `+0x34 = 10000`; эти же числа — ITM `max_stack`. То есть `+0x34` — это
  скопированный из item definition `max_stack`.
- Если `max < 2` (0 или 1), add **не** стекает в существующую запись, а
  уходит в создание новых записей. При `max == 1` каждая единица count
  становится отдельной записью; при `count > 1` создаётся несколько
  записей.
- Если существующая запись заполнена (`cur >= max`), скан продолжается —
  может найтись другая запись того же id с местом (в save деньги/расходники
  обычно одной записью).
- `max == 0` в creation-loop даёт `n = 0` и потенциально незавершающийся
  цикл; для целевых предметов `max > 0` (см. §10).

### 3.3 Новая запись: поля (`FUN_1404e3a4` + `FUN_1404f4fc`)

`FUN_1404e3a4` (RVA `0x4e3a4`) аллоцирует `0xC0` байт, вызывает
`FUN_1404f4fc(record, id, tech)` (RVA `0x4f4fc`) и добавляет id записи в
контейнер. `FUN_1404f4fc`:

- `tech_id_lookup(tech, &rec->+0xA8)`; `rec->+0xA4 = id` (оба поля = item id).
- Копирует item definition/template в запись:
  - `rec+0x08 = tpl+0x04` — **type**;
  - `rec+0x0C = tpl+0x08` — **flags**;
  - `rec+0x10 = tpl+0x0C` (price), `rec+0x14 = tpl+0x10` (weight),
    `rec+0x18 = tpl+0x14`, `rec+0x1C = tpl+0x18`,
    `rec+0x20 = tpl+0x1C` — **style**, `rec+0x30 = tpl+0x2C` (material),
    `rec+0x34 = tpl+0x30` — **max_stack**;
  - далее — поля по типу (jump table `0x140921a0`, type `0..0x1C`:
    damage/spell/charge/healing/recipe и т.п.).
- `rec+0xB0 = 1` (затем `FUN_1404e3a4` перезаписывает на фактический
  chunk), `rec+0xB4 = 100`, `rec+0xB8 = rec+0xA0` только при `type == 6`,
  иначе `0`.
- `FUN_1404e3a4` после инициализации: `rec+0xAC = container+4`
  (id/owner контейнера; в save это `14`/`656`, см. §7.1), `rec+0xB0 =
  chunk`, и если `DAT_14088df8+4 != 0`, то `rec+0x0C &= ~0x80`
  (`0x1404e792..0x1404e7a6`).
- Вторичный индекс контейнера (`+0x12C/+0x130/+0x134`) и used-bitmap
  обновляются; состояние контейнера `+0x18` ставится `1/2`.

Источник type/flags/style для порта: `ItemCatalogEntry::item` (`ITM`):
`item.type`, `item.flags`, `item.unknown_b[1]` (= style, P5 evidence:
`p5-save-equipment.md` §5, 0/N на 22 equipped item). `max_stack` —
`item.max_stack` (ITM header). `MON_1_0_1` (id `812`): type `18`
(money), flags `0xC0000000`, `max_stack = 100000000`, style `1`.

### 3.4 Zero / negative / unknown

| Вход | Поведение оригинала |
|---|---|
| `count == 0` | натив не вызывает add; внутри add тоже `return -1`; изменений/нотификации нет |
| `count < 0` | как `uint` (`-100` → `0xFFFFFF9C`): add пытается добавить ~4.29 млрд единиц чанками по `max_stack` (для 10000 — сотни тысяч записей). Оригинал не клампит и не проверяет знак; практически это patho/hang/OOM |
| неизвестный TECH | `tech_id_lookup` fail → `return -1`, изменений нет |
| TECH есть, item definition/ITM нет | создание записи не удаётся (`FUN_1404e3a4 == -1`); уже выполненный стекинг в существующие записи при этом остаётся |
| `count != 0`, но все стеки заполнены | создаются новые записи на остаток |

## 4. Remove: `FUN_1404cae8` (`decompiled.c:52500-52597`, RVA `0x4cae8`)

```
if (!tech_id_lookup(tech, &id)) return;           // null tech / unknown -> no-op
cap = container->+0x10;                            // snapshot
remaining = count;
for i in 0 .. cap-1:
    if (!used(i)) continue;
    rec = FUN_1404dbec(container, container->ids[i]);
    if (!rec || rec->+0xA8 != id) continue;
    cur = rec->+0xB0;
    if (remaining < cur) { rec->+0xB0 = cur - remaining; return; }
    if (remaining == cur) { FUN_1404dd6c(); return; }   // delete record
    remaining -= cur;
    FUN_1404dd6c();                                     // delete record, continue
return;                                                 // missing / exhausted
```

`FUN_1404dbec` (RVA `0x4dbec`) — поиск записи по id слота; `FUN_1404dd6c`
(RVA `0x4dd6c`) — удаление записи: очищает used-bit контейнера и глобального
pool, вызывает деструктор записи, чистит вторичный индекс, ставит
`container+0x18 = 1/2`, возвращает `1`.

### 4.1 Правила

| Ситуация | Поведение |
|---|---|
| `count < cur` | частичное: `cur -= count`, запись остаётся, выход |
| `count == cur` | запись удаляется (count становится 0 ⇒ удаление) |
| `count > cur` | запись удаляется, остаток `count - cur` идёт в следующие записи того же id |
| `count > всего доступно` | удаляются **все** записи этого id; остаток молча игнорируется (не ошибка, отрицательного count не возникает) |
| предмет отсутствует | скан заканчивается без изменений |
| `count == 0` | натив не вызывает remove; при прямом вызове первый же match даёт `0 < cur` → no-op и выход |
| `count < 0` | как `uint` (огромное число) ⇒ `count >= cur` для всех записей ⇒ удаляются **все** записи этого id |
| неизвестный TECH | lookup fail → no-op |
| несколько записей одного id | удаляются по порядку слотов, пока не кончится count/записи |

Порядок обхода — по индексам контейнера (slot order), не по item id.

## 5. Container `+0x18` и `FUN_140482e8`

### 5.1 `container+0x18`

Поле пишется в add/remove/create/delete по одной схеме:

```
if (container->+0x18 == 0) container->+0x18 = 1;
else                       container->+0x18 = 2;
```

В `Server.dll` нет чтения этого поля вне самих mutation-хелперов
(проверка по всему `decompiled.c`: единственные `if (…+0x18 == 0)` стоят
там же, где и запись). В save контейнер хранит только id/count/records —
`+0x18` не сериализуется. Смысл — write-only маркер изменения (0 =
нетронут, 1 = первое изменение, 2 = последующие); потребитель в
`Server.dll` не найден (возможно, сетевой/клиентский слой). **Порт может
не моделировать.**

### 5.2 `FUN_140482e8` (RVA `0x482e8`, `decompiled.c:49126-49187`)

```
if (count <= 0 || item_id == -1) return;
if (DAT_14088de8+0x2CC == 0) return;              // feature/network gate
e = current_player_entity();                       // same lookup as RS_GetMoney
if (!e) return;
if (e->+0x10 == param_2) match = true;
else if (e->+0x10 != param_1) return;
if (!tech_id_lookup("MON_1_0_1", &money_id)) return;
if (match)  event = (item_id == money_id) ? 0xB4 : 0xB6;  // add money/item
else        event = (item_id == money_id) ? 0xB5 : 0xB7;  // remove money/item
FUN_140528d8(event, player_name/tag_string);       // RVA 0x528d8
```

`FUN_140528d8` собирает пакет (`10`, `2`, event, строка) и вызывает
server API `DAT_140891bc+0xC4` — это серверная нотификация/отправка
текущему игроку. Add передаёт `(−1, e->+0x10, id, count)`, remove —
`(e->+0x10, −1, id, count)`; отсюда коды `0xB4/0xB6` (add money/item) и
`0xB5/0xB7` (remove money/item).

Нотификация срабатывает для валидного TECH и count != 0 даже если
фактически ничего не изменилось (например, remove отсутствующего
предмета). Для одиночного in-process порта runtime — единственный
источник истины; **порт может безопасно не вызывать notify**, а UI
читать живой `InventoryRuntime`.

### 5.3 Что моделировать, что опустить

Моделировать: `item_id`, `count`, `type`, `flags`, `style`, `max_stack`
(для стекинга/сплита), порядок записей (для overlay).

Опустить без потери игрового поведения: `container+0x18`,
`FUN_140482e8`/`FUN_140528d8`, used-bitmap/free-list/вторичный индекс,
record ids (`+0xA4` instance uid, `+0xAC` owner/container id), durability
`+0xB4`, type-6 `+0xB8`, template-conditional blocks (damage/spell/…),
NPC/`.inv` контейнеры.

## 6. NPC / box / trigger limitation

- `FUN_1402914c` ищет `EF_PERSON` (`type 2`, `+0x150 == 0`); fallback
  `FUN_14029270` ищет сущность с `(entity+0x14 & 0x3E) == 0x10`
  (`p3b2a` §8: в Client это `EF_TRIGGER`, не `EF_INVCONTAINER` = type 8;
  связь с «ящиками» не подтверждена).
- Оба mutation-натива имеют fallback `0x10`, но не моделируют NPC/box
  контейнеры в порте. NPC-инвентарь `entity+0x10` загружается из
  `scripts/inventory/%s.inv` (конструктор `FUN_140233d4`; `p3b2a` §8),
  это Phase 1b2b/c.
- Add/Remove **не проверяют** результат person-lookup: отсутствующая
  персона → чтение `[0x10]` → AV. `FUN_1404bf8c` может вернуть 0 →
  `c == 0` → AV на `c->+0x18`. Порт по spec §7 делает no-op (deviation,
  как у `RS_GetItemCountI` в D-051).
- `FUN_140482e8` работает только для текущей player-сущности
  (`entity+0x14 & 2`); NPC/box в нотификации не участвуют.

## 7. Реальный oracle (slot0/slot2)

### 7.1 Save-записи и поля

Контейнер 1 (`entity+0x10`) — основной инвентарь; деньги — одна запись
`MON_1_0_1` (id `812`, type `18`). Дополнительно проверены поля записи:

| Слот | item | count | type | flags (save) | style | `+0x30` | `+0x34` |
|---|---|---:|---:|---|---:|---:|---:|
| slot0 | 812 money | 43875 | 18 | `0xC0000000` | 1 | `0xFFFFFFFF` | 100000000 |
| slot0 | 953 potion | 51 | 19 | `0x8000020C` | 1 | `0xFFFFFFFF` | 10000 |
| slot0 | 444 reagent | 6 | 24 | `0x40000008` | 1 | `0xFFFFFFFF` | 10000 |
| slot0 | 407 food | 55 | 20 | `0x8000000C` | 1 | `0xFFFFFFFF` | 10000 |
| slot2 | 812 money | 122008 | 18 | `0xC0000000` | 1 | `0xFFFFFFFF` | 100000000 |
| slot2 | 953 potion | 25 | 19 | `0x8000020C` | 1 | `0xFFFFFFFF` | 10000 |
| slot2 | 444 reagent | 1 | 24 | `0x40000008` | 1 | `0xFFFFFFFF` | 10000 |
| slot2 | 407 food | 13 | 20 | `0x8000020C` | 1 | `0xFFFFFFFF` | 10000 |

ITM (`decode-itm`):

| id | type | flags | max_stack | unknown_b[1]=style |
|---:|---:|---|---:|---:|
| 812 | 18 money | `0xC0000000` | 100000000 | 1 |
| 953 | 19 potion | `0x8000000C` | 10000 | 1 |
| 444 | 24 reagent | `0x40000008` | 10000 | 1 |
| 407 | 20 food | `0x8000000C` | 10000 | 1 |

Вывод: `rec+0x34` = ITM `max_stack`; `rec+0x08` = ITM `type`;
`rec+0x20` = ITM `unknown_b[1]` (style); `rec+0x0C` содержит ITM flags,
но в save встречаются дополнительные runtime-биты (например `0x200` у
953/407), происхождение которых в этой задаче не разбиралось.

### 7.2 Ожидания add/remove

| Операция | slot0 | slot2 |
|---|---|---|
| старт `MON_1_0_1` count / `GetMoney` | 43875 | 122008 |
| `AddItem("Hero","MON_1_0_1",100)` | **43975** | **122108** |
| затем `RemoveItem("Hero","MON_1_0_1",100)` | **43875** | **122008** |
| старт `953` count | 51 | 25 |
| `AddItem("Hero","TECH(953)",10)` | **61** | **35** |
| затем `RemoveItem("Hero","TECH(953)",10)` | **51** | **25** |

Для 953 стек уже существует и `max_stack = 10000`, поэтому add идёт в
существующую запись (не создаёт новую); remove частичный.

Дополнительный случай новой записи: item `307` (`ARM_1_08_2`) в обоих
save отсутствует; ITM: type `12`, flags `0xC0000084`, `max_stack = 1`,
style `8`. `AddItem("Hero","ARM_1_08_2",1)` создаёт запись
`{id=307, count=1, type=12, flags=0xC0000084 (возможно `~0x80` при
`DAT_14088df8+4 != 0`), style=8}`; `RemoveItem(...,1)` её удаляет. При
`count > 1` и `max_stack == 1` создаются отдельные записи по 1.

## 8. Реальный диалог

Выбран: `scripts/dialogs/unique/gypsy.age.cs` (unique NPC, без
зависимости от переменных мира).

Почему он:

- Remove money `2800` выполняется детерминированно после двух ответов,
  без случайностей и без условий на `zlato_vars.scr`;
- результат (уменьшение money) наблюдаем в `InventoryRuntime` сразу после
  прохода;
- путь проверен `glscript` (см. §11): узел Remove достигается, дальше
  идут `RS_DelPerson`, `RS_AddExp`, `D_CloseDialog`.

`glpak disasm-cs` (ключевые узлы):

```
72: jmp | 73 73
75: func | -1 -1 | 16777220 D_Say(76)
76: num | 5039
79: func | -1 -1 | 16777222 D_Answer(80)
80: num | 5040
83: func | -1 -1 | 16777222 D_Answer(84)
84: num | 5084
61: jmp | 62 62
62: == | 63 64 67 65          ; LastPhrase == 5039 -> 67
67: jmp | 68 68
68: assign | 69 70 66 66
70: func | -1 -1 | 16777221 D_CloseDialog(71)
...
46: jmp | 47 47
47: == | 48 49 52 50          ; LastAnswer == 5084 -> 52
52: jmp | 53 53
55: func | -1 -1 | 16777220 D_Say(56)
56: num | 5051
59: func | -1 -1 | 16777222 D_Answer(60)
60: num | 568
21: jmp | 22 22
22: == | 23 24 27 25          ; LastAnswer == 568 -> 27
27: jmp | 28 28
28: assign | 29 30 34 34
30: func | -1 -1 | 83886085 RS_PersonRemoveItem(31, 32, 33)
31: str | "Hero"
32: str | "MON_1_0_1"
33: num | 2800
36: func | -1 -1 | 67108870 RS_DelPerson(37)
40: func | -1 -1 | 67108869 RS_AddExp(41)
44: func | -1 -1 | 16777221 D_CloseDialog(45)
```

`person_calls_resolved.txt`:

```
scripts/dialogs/unique/gypsy.age.cs :: 30 83886085 RS_PersonRemoveItem :: str "Hero" | str "MON_1_0_1" | num 2800
```

Последовательность ответов (индексы 0-based в `DialogSession::choose`):
первый экран — ответы `[5040, 5084]`, выбрать **индекс 1** (5084);
второй экран — ответ `[568]`, выбрать **индекс 0**.

Ожидание после Remove (деньги — только эта запись, других изменений
money в пути нет):

| Слот | money до | после −2800 |
|---|---:|---:|
| slot0 | 43875 | **41075** |
| slot2 | 122008 | **119208** |

## 9. Отклонения/ограничения порта (рекомендация)

- Персона — только точное `"Hero"`; missing/не-`Hero` → no-op (в
  оригинале AV; deviation как D-051).
- Контейнер не найден / NPC/`0x10`/`.inv` → no-op (Phase 1b2b/c).
- `count == 0` → no-op (совпадает с нативом).
- `count < 0` → рекомендуется no-op (оригинал: uint-wrap; add —
  патологический цикл, remove — удаление всех записей). Это осознанное
  отклонение ради безопасности, зафиксировать в D-052/spec §7.
- Неизвестный TECH → no-op; известный TECH без ITM и без существующей
  записи → no-op (оригинал не создаёт запись).
- Notify и `+0x18` не моделируются; runtime — источник истины.
- Save не пишется.

## 10. Unverified / open questions

1. **Смысл `container+0x18`.** Поле пишется `1/2`, но чтений вне
   mutation-хелперов в `Server.dll` не найдено; точный потребитель
   (сеть/клиент/serialization) не установлен.
2. **`DAT_14088df8+4`.** При `!= 0` новый record теряет бит `0x80` в
   flags; смысл глобального флага не установлен. Для money (`0xC0000000`)
   не влияет; для предметов с `0x80` в ITM flags — влияет.
3. **Смысл event-кодов `0xB4..0xB7`.** Код однозначно различает add/remove
   money/item и шлёт через `DAT_140891bc+0xC4`; конкретный клиентский
   обработчик/звук/сообщение не разбирались.
4. **`max_stack == 0`.** Теоретически даёт `chunk = 0` и незавершающийся
   creation-loop; ITM с нулём в целевых данных не проверялись. Порт
   должен защититься (no-op).
5. **Отрицательный count** исполнен только статически (uint-wrap);
   живой прогон не выполнялся — исход зависит от памяти/лимитов.
6. **Runtime-биты `rec+0x0C`** (например `0x200` в save 953/407) не
   объяснены; при создании новой записи движок копирует template flags,
   затем, возможно, снимает `0x80`. Порт не должен выдумывать остальные
   биты.
7. **Live-оракул не снимался.** Все числа — из save + статического
   дизассемблера; чтение памяти запущенной игры не выполнялось.
8. **Порядок новых записей.** Оригинал кладёт запись в первый свободный
   слот контейнера; порт с вектором естественно добавляет в конец. Для
   save-loaded контейнера (capacity == числу записей) это совпадает, но
   live-проверка порядка после нескольких add/remove не делалась.

## 11. Проверка (фактический вывод)

Константы возврата (`Server.dll`, RVA → file offset, 8 байт):

```
0x91498 off=0x85498 bytes=00 00 00 00 00 00 00 00 double=0.0
0x914a8 off=0x854a8 bytes=00 00 00 00 00 00 00 00 double=0.0
```

ITM:

```
812: type=18 flags=3221225472 max_stack=100000000 unknown_b=0,1,0,0,0
953: type=19 flags=2147483660 max_stack=10000     unknown_b=10,1,5,0,0
444: type=24 flags=1073741832 max_stack=10000     unknown_b=0,1,0,0,0
407: type=20 flags=2147483660 max_stack=10000     unknown_b=10,1,5,0,0
307: type=12 flags=3221225604 max_stack=1         unknown_b=0,8,0,0,0
```

Save-поля записей (парсер: `zlib`, `BT_HERO `, `body+0x734`, stride
`0xC0+4+sub*0x14`):

```
slot0: c1 id=812 count=43875 type=18 flags=0xc0000000 style=1 +0x30=0xffffffff +0x34=100000000
slot0: c1 id=953 count=51   type=19 flags=0x8000020c style=1 +0x34=10000
slot2: c1 id=812 count=122008 type=18 flags=0xc0000000 style=1 +0x34=100000000
slot2: c1 id=953 count=25   type=19 flags=0x8000020c style=1 +0x34=10000
```

`glscript` (путь gypsy; текущий билд без mutation runtime):

```
> glscript --game-dir "I:\Games\Златогорье-2" --file "scripts/dialogs/unique/gypsy.age.cs" --run --answer 1 --answer 0
[info] script native RS_PersonRemoveItem (0x5000005): unimplemented - requires item runtime state
status: wait_user
steps: 16
dialogs: 5039 5040 5084
answers: 2
status: wait_user
steps: 37
dialogs: 5051 568
answers: 1
status: end
steps: 55
dialogs:
answers: 0
[info] script native RS_DelPerson (0x4000006): unimplemented - requires person runtime state
[info] script native RS_AddExp (0x4000005): unimplemented - requires person runtime state
```

Документ: UTF-8 без BOM, `nul=0` (проверка отдельной командой; см.
отчёт Task 1).
