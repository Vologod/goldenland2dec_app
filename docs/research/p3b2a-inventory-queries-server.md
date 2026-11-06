# Research: inventory query natives (`Server.dll` v1.3)

Дата: 2026-11-06. Задача: Phase 1b2a, Task 1 (research only).
Источник истины — `Server.dll` v1.3; имена TECH и type-таблица — по
`GoldenLand.exe`/`Client.dll` там, где это указано явно.

## 1. Provenance и метод

| Файл | SHA-256 |
|---|---|
| `I:\Games\Златогорье-2\Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `I:\Games\Златогорье-2\Saves\slot0.sav` | `8AC545FE9715EF11C68B1CCA8617BEB03003ECA360F07F71C26E3FC5E5C73442` |
| `I:\Games\Златогорье-2\Saves\slot2.sav` | `5DB504F57BDFFB66A8B977256AD53EE8E1D6D283E1EDFCD75092FFFD3DB30296` |

- Image base `Server.dll` = `0x14000000`; RVA = VA − `0x14000000`.
- Корпус (gitignored): `.local/decomp/server/decompiled.c` (1108 функций),
  `.local/decomp/server/natives/natives.c` (65 обработчиков),
  `.local/decomp/server/functions.csv`.
- Дизассемблер: `llvm-objdump.exe -d -M intel` из
  `llvm-mingw-20261006-ucrt-x86_64`.
- `tech_names.sdb`: `glpak decode-sdb`, затем проверка чисел независимым
  Python-разбором (id/name, уникальность, регистр).
- Save: Python `zlib.decompress(raw[112:])`, поиск `BT_HERO `, body =
  `[off+12, off+12+size)` (тот же контракт, что у
  `gl::assets::decode_save_body`). Числа сверены с P5b1
  (`docs/research/p5b1-inventory-ui.md` §8.2/§8.3).
- `.cs`: `glpak disasm-cs`; вызовы с разрешёнными аргументами —
  `%TEMP%\person_calls_resolved.txt` (скрипт `%TEMP%\opencode\scan_person.ps1`).

Пути `.cs`/`.scr`/`.inv` ниже — внутри `scripts.pak`; они не коммитятся.

## 2. Native semantics

| ID | name | handler RVA | arity | arg types | return | fallback | lookup | side effects |
|---|---|---|---|---|---|---|---|---|
| `0x5000000` | `RS_TestPersonHasItem` | `0x40144` | 2 | `3,3` (string,string) | `1.0` если count > 0, иначе `0.0` | `0.0` (`DAT_140913a0`) | `FUN_1402914c` (EF_PERSON), затем `FUN_14029270` (type `0x10`) | нет (read-only) |
| `0x5000002` | `RS_GetItemCountI` | `0x40d40` | 2 | `3,3` (string,string) | `count` (int как double) | `0.0` (`DAT_14091408`) только при неверной сигнатуре | только `FUN_1402914c` (EF_PERSON), fallback нет | нет (read-only) |
| `0x5000008` | `RS_GetMoney` | `0x41120` | 0 | `[]` | сумма `count` записей type `0x12` | `0.0` (`DAT_14091478`) | текущая player-персона `FUN_14027f7c` | нет (read-only) |

Проверка сигнатуры (`FUN_14038c6c`, RVA `0x38c6c`): Test/Count передают
`(3,3)` (два string-аргумента), GetMoney — `(-1)` (без аргументов).
Дизассемблер:

- Test `0x1404014d..0x14040156`: `push -1; mov eax,3; push eax; push eax;
  call 0x14038c6c`; при `eax == 0` — `fld qword [0x140913a0]` (`0.0`).
- Count `0x14040d43..0x14040d4c`: то же `(3,3)`; при `eax == 0` —
  `fld qword [0x14091408]` (`0.0`).
- Money `0x1404112f..0x14041136`: `push -1; call 0x14038c6c`; при
  `eax == 0` — `fld qword [0x14091478]` (`0.0`).

`FUN_14038c6c` проверяет только перечисленные позиции и игнорирует лишние
аргументы (`docs/research/p3-vm-natives-server.md` §5, п. 5); `arity` в
таблице — минимальное число проверяемых типов.

### 2.1 `RS_TestPersonHasItem` (`natives.c:1609-1640`)

```
if (!signature(3,3)) return 0.0;
p = find_person(name);                 // FUN_1402914c: type==2 && +0x150==0
if (p == 0) p = find_entity_0x10(name); // FUN_14029270: type==0x10
if (p == 0) return 0.0;
c = lookup_container(manager, p->+0x10); // FUN_1404bf8c
if (c == 0) return 0.0;
n = count_by_tech(c, tech_name);        // FUN_1404c574
return n > 0 ? 1.0 : 0.0;
```

### 2.2 `RS_GetItemCountI` (`natives.c:1775-1793`)

```
if (!signature(3,3)) return 0.0;
p = find_person(name);                 // FUN_1402914c, без fallback
c = lookup_container(manager, p->+0x10); // p не проверяется на 0
return count_by_tech(c, tech_name);    // FUN_1404c574
```

Важное отличие: при отсутствующей персоне оригинал разыменовывает
`p->+0x10` (AV), а не возвращает `0.0`. Порт по spec §7 возвращает `0`;
это осознанное отклонение (см. §8).

### 2.3 `RS_GetMoney` (`natives.c:1991-2028`)

```
if (!signature(-1)) return 0.0;
idx = current_player_index(manager);        // FUN_14027f7c
if (idx invalid) e = 0;
else if (!entity_bitmap_test(idx)) e = 0;
else e = entity_list[idx];
if (e != 0 && (e->+0x14 & 2) != 0) {       // player-сущность
    c = lookup_container(manager, e->+0x10);
    if (c != 0) money = sum_type18(c);     // FUN_1404c494
}
return money;
```

`FUN_14027f7c` (`decompiled.c:29080-29138`) возвращает индекс текущей
персоны; `+0x2564` — массив сущностей, `+0x2568/+0x256c` — bitmap и его
размер. Возврат — сумма `count` (`record+0xB0`) всех записей контейнера
с `type == 0x12`.

## 3. Default-константы

`.data1` в `Server.dll`: VMA `0x1408c000`, raw pointer `0x80000`
(RVA `0x8c000`). Значения прочитаны напрямую из PE.

| symbol | RVA | file offset | 8 байт (hex) | double | роль |
|---|---|---|---|---|---|
| `DAT_14091398` | `0x91398` | `0x85398` | `00 00 00 00 00 00 f0 3f` | `1.0` | Test: count > 0 |
| `DAT_140913a0` | `0x913a0` | `0x853a0` | `00 00 00 00 00 00 00 00` | `0.0` | Test: default/ошибка |
| `DAT_14091408` | `0x91408` | `0x85408` | `00 00 00 00 00 00 00 00` | `0.0` | Count: ошибка сигнатуры |
| `DAT_14091478` | `0x91478` | `0x85478` | `00 00 00 00 00 00 00 00` | `0.0` | Money: default/ошибка |

## 4. Разрешение контейнера и предмета

### 4.1 `FUN_1404c574` — TECH → item id + сумма count

`decompiled.c:52248-52304`, RVA `0x4c574`.

```
int count_by_tech(container, tech_name) {
    if (tech_name == 0) return 0;
    id = 0;
    if (!item_name_lookup(tech_name, &id)) return 0; // DAT_14088dfc vtable+0x2c
    total = 0;
    for (idx = first_used(container+8); idx != -1; idx = next_used(...)) {
        rec = container_record(container, container->ids[idx]); // FUN_1404dbec
        if (rec->+0xA8 == id) total += rec->+0xB0;
    }
    return total;
}
```

Дизассемблер подтверждает поля и порядок: `0x1404c590..0x1404c5a3`
(`vtable+0x2c(name, &id)`), `0x1404c640..0x1404c646` (`rec+0xA8 == id`),
`0x1404c64c..0x1404c650` (`total += rec+0xB0`). В декомпиляции локальная
`iStack_41c` — это фактически `id` по адресу `[esp+0xc]`; на дизассемблере
видно сравнение `cmp edx, [esp+0xc]`.

Item record (P5b1 layout): `+0x08` type, `+0xA8` item id, `+0xB0` count.
`FUN_1404dbec` (`decompiled.c:53278-53345`) возвращает указатель на запись
по id элемента контейнера.

### 4.2 `FUN_1404c494` — сумма type `0x12`

`decompiled.c:52201-52244`, RVA `0x4c494`.

```
int sum_type18(container) {
    total = 0;
    for (idx = first_used(container+8); idx != -1; idx = next_used(...)) {
        rec = container_record(container, container->ids[idx]);
        if (rec->+0x08 == 0x12) total += rec->+0xB0;
    }
    return total;
}
```

Дизассемблер: `0x1404c52a..0x1404c52d` (`mov edx,[eax+8]; cmp edx,0x12`),
`0x1404c532..0x1404c536` (`add edx,[eax+0xB0]`).

### 4.3 `FUN_1404bf8c` и `entity+0x10`

`decompiled.c:51908-51920`, RVA `0x4bf8c`:

```
undefined4 lookup_container(manager, id) {
    int out;
    if (FUN_14004f14(manager+0xC, id, &out)) return out;
    return 0;
}
```

`FUN_14004f14` (`decompiled.c:3238-3256`) — map-lookup: при успехе пишет
найденный указатель в `out` и возвращает `1`, иначе `out = 0`, `0`.
Декомпиляция `FUN_1404bf8c` теряет возврат (`return 0` в обеих ветках), но
дизассемблер `0x1404bf9f..0x1404bfac` показывает `mov eax,[esp]` в
success-ветке. Это артефакт Ghidra, не поведение.

Почему `entity+0x10` — основной инвентарь:

- `FUN_1401e424` (`decompiled.c:21780-21874`, reader `BT_HERO`) читает три
  id контейнеров из тела save в порядке `entity+0x0C`,
  `entity+0x10`, `entity+0x109C` (`decompiled.c:21824-21829`).
- P5 (`docs/research/p5-save-equipment.md` §4) отождествляет эти три id с
  контейнерами 0/1/2; `+0x10` — контейнер 1, «инвентарь» (117 записей
  slot0, 140 slot2), `+0x109C` — экипировка (11/11).
- P5b1 (`docs/research/p5b1-inventory-ui.md` §3) независимо: экран
  `+0x188 = hero+0x10` = container 1 (основной инвентарь, нижняя полоса).

Следствие: Test/Count/Money читают **основной инвентарь** (`+0x10`), а не
hot slots (`+0x0C`) и не экипировку (`+0x109C`).

### 4.4 Item-name index (`DAT_14088dfc`, vtable `+0x2c`)

`DAT_14088dfc` инициализируется из объекта `GoldenLand.exe`
(`decompiled.c:63146-63163`, `DAT_14088dfc = *(iVar1+0x24)`).
`vtable+0x2c` — lookup TECH-имени в id:

- `decompiled.c:49163`: `(vtable+0x2c)(s_MON_1_0_1_140920a8, &local_8)`,
  затем `param_3 == local_8` — сравнение с item id.
- `decompiled.c:52271`: тот же вызов в `FUN_1404c574`.
- `decompiled.c:54538`: `(vtable+0x2c)(param_2, this+0xA8)` — запись id в
  поле записи.

Значит `FUN_1404c574` получает именно item id (не type/flags).

## 5. `sdb/items/tech_names.sdb`

Формат SDB (`src/assets/src/sdb.cpp`): каждая запись — `u32 id`,
`u32 len`, `len` байт строки; без сигнатуры `SDB ` байты XOR `0xAA`.
`glpak decode-sdb` выдал 1848 строк `id<TAB>name`.

| Проверка | Значение |
|---|---|
| записей | **1848** |
| id | `0..1847`, непрерывно, уникальны (1848) |
| имена | уникальны (1848), без case-insensitive дубликатов |
| длина имени | 7..73 |
| charset | `0-9 A-Z _` + `aegilnorstux` (158 имён смешанного регистра) |
| регистр | имена хранятся как есть; `mon_1_0_1` в таблице отсутствует (exact lookup не найдёт) |
| `MON_1_0_1` | **812** |
| `QST_1_0_159` | **1101** |
| `ARM_1_08_2` | **307** |

Смешанный регистр встречается в именах вида
`NPC_TWO_HANDED_AXE_AC_HPMax_...` и `not_using`; для трёх целевых имён
регистр — ровно как в таблице. Поскольку имена уникальны, reverse-map
`name → id` однозначен; дубликатов (spec §5.1) в реальных данных нет.

## 6. Реальный oracle (`slot0.sav`, `slot2.sav`)

Метод: `zlib.decompress(raw[112:])`; `BT_HERO ` найден, body =
`[off+12, off+12+size)`; от `body+0x734` три контейнера:
`u32 id; u32 n; n × record`, где
`record_size = 0xC0 + 4 + u32(record+0xC0) * 0x14`. После третьего
контейнера остаётся ровно 144 байта (8 + `0x80` + 4 + 4) — совпадает с
P5-разметкой (2×u32, 4×0x20, extra_count=0, list_count=0).

| Слот | raw | decompressed | `BT_HERO ` | size | container end | c0/c1/c2 |
|---|---|---|---|---|---|---|
| slot0 | 642409 | 905051 | `0xD589B` | `0x76B4` (30388) | `0x7624` | 8 / **117** / 11 |
| slot2 | 630410 | 910169 | `0xD5A89` | `0x88C4` (35012) | `0x8834` | 9 / **140** / 11 |

Счётчики контейнеров 8/117/11 и 9/140/11 совпадают с P5b1 §8.2,
деньги — с P5b1 §8.3 (`43875` / `122008`).

### 6.1 Oracle-значения (container 1 = `entity+0x10`)

| query | TECH | item id | slot0 | slot2 |
|---|---|---:|---:|---:|
| `RS_GetItemCountI("Hero", …)` / `RS_TestPersonHasItem` | `MON_1_0_1` | 812 | **43875** | **122008** |
| `RS_GetItemCountI("Hero", …)` / `RS_TestPersonHasItem` | `QST_1_0_159` | 1101 | **0** | **0** |
| `RS_GetItemCountI("Hero", …)` / `RS_TestPersonHasItem` | `ARM_1_08_2` | 307 | **0** | **0** |
| `RS_GetMoney` (sum type 18) | — | — | **43875** | **122008** |

Совпадающие записи (полная запись контейнера 1, index — порядковый номер):

| Слот | index | item id | count | type | TECH |
|---|---:|---:|---:|---:|---|
| slot0 | 0 | 812 | 43875 | 18 | `MON_1_0_1` |
| slot2 | 0 | 812 | 122008 | 18 | `MON_1_0_1` |

В обоих save ровно одна запись type 18, поэтому `money == count(MON_1_0_1)`.
`QST_1_0_159` и `ARM_1_08_2` в container 1 отсутствуют (0 записей) —
ожидаемый Test = `0.0`, Count = `0`, GetMoney от них не зависит.

### 6.2 Полные списки container 1

Приведены в приложении A (exact `item_id:count`, в порядке записей).
Container 0/2 (для полноты декодирования, в oracle не участвуют):

```
slot0 c0: 1554:1,1554:1,1521:1,953:1,1575:1,1579:1,953:1,953:1
slot0 c2: 72:1,397:1,280:1,318:1,80:1,233:1,554:1,141:1,486:1,353:1,245:1
slot2 c0: 953:1,953:1,1532:1,1532:1,953:1,1575:1,953:1,953:1,953:1
slot2 c2: 486:1,359:1,554:1,66:1,278:1,317:1,108:1,396:1,99:1,236:1,243:1
```

## 7. Реальный диалог для проверки

Выбран:
`scripts/dialogs/l13_1/l13_1.p517_boat_trader.d517.age.cs`
(2484 байта в `scripts.pak`).

Почему он:

- `RS_GetItemCountI("Hero","MON_1_0_1")` находится **на начальном пути**
  VM, без зависимости от ответов игрока, случайностей и других переменных.
- Результат присваивается глобальной переменной `money`, которую тест
  может прочитать после `wait_user`.
- Оба save дают ненулевой, различимый count (`43875` vs `122008`), поэтому
  подмена/пустой инвентарь сразу ломает ожидание.

Дизассемблер (`glpak disasm-cs`), ключевые узлы:

```
1:   ==     | 2 3 132 4          ; LastPhrase == 0 -> 132
2:   str_var| LastPhrase
3:   num    | 0
132: jmp    | 133 133
133: assign | 134 135 137 137
134: str_var| result
135: func   | ... D_Say(136)
136: num    | 8868
137: assign | 138 139 142 142
138: str_var| money
139: func   | ... 83886082 RS_GetItemCountI(140, 141)
140: str    | "Hero"
141: str    | "MON_1_0_1"
142: assign | 143 144 146 146
143: str_var| result
144: func   | ... D_Answer(145)
145: num    | 3498
```

`person_calls_resolved.txt` (строка 1340) независимо подтверждает:
`node 139 → 83886082 RS_GetItemCountI :: str "Hero" | str "MON_1_0_1"`.

Ожидание после `ScriptVm` до `wait_user`:

| Слот | `vm.variable("money")` |
|---|---:|
| slot0 | **43875** |
| slot2 | **122008** |

Детерминизм: `ScriptVm` инициализирует `LastPhrase = 0`
(`src/script/src/script_vm.cpp:234`), поэтому узел 1 всегда ведёт на 132;
до узла 139 нет ветвлений по ответам/переменным, только `D_Say` и
присваивание. `L13_Tarvik_BuyBoat` (default 0 в `zlato_vars.scr`) влияет
только на выбор второго `D_Answer` (узел 146 → 4214/8765), но не на
`money`. Первый ответ 3498 предлагается всегда.

Более «сильная» альтернатива (результат выбирает ветку):
`scripts/dialogs/l9_1/l0.p448_life_potion_trader.d448.age.cs`, узлы
104 (`money = GetItemCountI`), 109 (`money > 149`), 118/112
(`D_Answer 8780` / `8772`). Она требует `L9_SeaCity_n3_TeachMage == 4`
(в `zlato_vars.scr` default 0) и ответа 4872, поэтому для базового real-
теста выбран boat trader; p448 можно использовать как второй сценарий.

## 8. Ограничения: NPC и box/trigger

- **NPC.** `FUN_1402914c` находит `EF_PERSON` (`type 2`, `+0x150 == 0`),
  после чего читается `entity+0x10`. Для NPC контейнер `+0x10`
  загружается из `scripts/inventory/%s.inv` (конструктор
  `FUN_140233d4`, `decompiled.c:25993-26017`; строка
  `s_scripts_inventory__s_inv_1408e57c`). То есть оригинал **умеет**
  отвечать по NPC-луту. Порт не моделирует NPC-инвентарь/`.inv`
  (Phase 1b2b/c), поэтому NPC-запросы возвращают `0`.
- **`FUN_14029270`.** `decompiled.c:29801-29858`, RVA `0x29270`: поиск
  сущности по имени с `(entity+0x14 & 0x3E) == 0x10`; `+0x150` не
  проверяется. Это fallback только у Test (и у transfer/add/remove); у
  Count его нет, у Money — нет.
- **Type-лейбл `0x10`.** В `Client.dll` таблица имён типов
  (`docs/research/p4b2-render-order.md` §, строки 91-97) даёт:
  `2 EF_PERSON`, `4 EF_MAGIC`, `8 EF_INVCONTAINER`, `0x10 EF_TRIGGER`,
  `0x20 EF_DOOR`. `Server.dll` использует те же числа: person `2`,
  door `0x20` (`FUN_14029388`, `decompiled.c:29893`; вызывается из
  `RS_SetDoorState`), trigger `0x10` (`FUN_14028138`,
  `decompiled.c:29172-29175`, `s_bOpenTrigger`). Поэтому утверждение
  spec/brief «`FUN_14029270` = EF_INVCONTAINER boxes» **не подтверждено**:
  по коду это lookup type `0x10` (в Client — `EF_TRIGGER`), а
  `EF_INVCONTAINER` — type `8`. Порт не моделирует ни type `0x10`, ни
  type `8`, поэтому fallback-запросы возвращают `0`.
- В поставляемых `.cs` первый аргумент inventory-нативов — имена персон
  (68 уникальных, 926 вызовов `"Hero"`); имён box/trigger в скане не
  найдено. То есть fallback `FUN_14029270` на реальных `.cs` кампании,
  вероятно, не наблюдаем, но это не доказательство его ненужности.
- **Count без персоны.** `RS_GetItemCountI` не проверяет результат
  `FUN_1402914c` и не имеет fallback: отсутствующая персона в оригинале —
  AV. Порт возвращает `0` (spec §7; deviation).
- **Money.** Работает только для текущей player-сущности
  (`entity+0x14 & 2`); NPC/box не участвуют.

## 9. Unverified / open questions

1. **Семантика `FUN_14029270`/type `0x10`.** Код однозначно матчит
   `0x10`, но связь этого типа с «ящиками»/контейнерами и с item-нативами
   не установлена; Client называет `0x10` `EF_TRIGGER`. Нужен live/SEF
   разбор конкретного объекта, если понадобится 1b2b/c.
2. **`entity+0x150`** в person-lookup: условие `== 0` подтверждено, смысл
   поля не установлен.
3. **NPC `.inv`-лут** детально не разбирался (только путь/загрузчик);
   counts и формат — Phase 1b2b/c.
4. **Live-оракул не снимался.** Числа получены из save и перекрёстно
   сверены с P5b1; запуск оригинала для чтения памяти не выполнялся.
5. **`FUN_1404c574` `callers=0` в `decompiled.c`** — артефакт Ghidra
   (indirect/direct call из натива); сам вызов виден в `natives.c` и
   дизассемблере.
6. **Case-sensitivity.** Код сравнения — побайтовый `strcmp`-подобный
   цикл; отдельный тест на смешанный регистр в lookup не снимался (все
   три целевых имени — точный uppercase).

## 10. Проверка (фактический вывод)

Константы (`Server.dll`, RVA → file offset, 8 байт):

```
0x91398 off=0x85398 bytes=00 00 00 00 00 00 f0 3f double=1.0
0x913a0 off=0x853a0 bytes=00 00 00 00 00 00 00 00 double=0.0
0x91408 off=0x85408 bytes=00 00 00 00 00 00 00 00 double=0.0
0x91478 off=0x85478 bytes=00 00 00 00 00 00 00 00 double=0.0
```

`tech_names.sdb`:

```
tech: entries=1848 unique_ids=1848 unique_names=1848
tech: id_min=0 id_max=1847 contiguous=True
tech_lookup MON_1_0_1    -> 812
tech_lookup QST_1_0_159  -> 1101
tech_lookup ARM_1_08_2   -> 307
tech_lookup mon_1_0_1    -> None
names with lowercase 158; case-insensitive duplicates 0
```

Save decode + counts:

```
slot0: raw=642409 decompressed=905051 bt_hero_off=0xD589B size=30388(0x76B4) container_end=0x7624
  c0=8 c1=117 c2=11
  MON_1_0_1 id=812 total=43875 entries=[(0, 812, 43875, 18)]
  QST_1_0_159 id=1101 total=0
  ARM_1_08_2 id=307 total=0
  money type18 total=43875
slot2: raw=630410 decompressed=910169 bt_hero_off=0xD5A89 size=35012(0x88C4) container_end=0x8834
  c0=9 c1=140 c2=11
  MON_1_0_1 id=812 total=122008 entries=[(0, 812, 122008, 18)]
  QST_1_0_159 id=1101 total=0
  ARM_1_08_2 id=307 total=0
  money type18 total=122008
```

`glpak disasm-cs` (boat trader) — ключевые строки:

```
137: assign | 138 139 142 142
138: str_var | money
139: func | -1 -1 | 83886082 RS_GetItemCountI(140, 141)
140: str | "Hero"
141: str | "MON_1_0_1"
142: assign | 143 144 146 146
```

`person_calls_resolved.txt`:

```
scripts/dialogs/l13_1/l13_1.p517_boat_trader.d517.age.cs :: 139 83886082 RS_GetItemCountI :: str "Hero" | str "MON_1_0_1"
```

Документ: UTF-8 без BOM, `nul=0` (проверка отдельной командой; см.
`STATUS`/report).

## Приложение A. Полные списки container 1 (`item_id:count`)

slot0 (117 записей):

```
812:43875,444:6,1013:1,1579:2,411:19,416:10,413:16,433:8,450:11,952:21,
953:51,451:24,449:15,410:5,1580:1,1541:2,989:1,424:3,448:22,434:16,
439:20,447:11,1575:10,414:14,431:16,1534:4,1107:1,932:3,943:3,440:16,
1497:1,1542:3,1530:2,1028:1,1068:1,1046:1,961:3,446:61,1037:1,441:5,
1583:7,1108:1,1072:1,419:7,437:4,443:4,1037:1,1037:1,407:55,1577:6,
445:6,951:4,1110:1,1065:1,936:2,969:6,934:1,438:1,925:2,940:3,
942:3,1560:2,1008:1,1031:1,1059:1,998:1,1060:1,417:4,926:1,963:1,
949:1,1002:1,930:3,967:2,1054:1,970:1,1037:1,1037:1,1037:1,1016:1,
1518:2,1567:1,1591:2,1592:1,408:3,442:46,1016:1,1537:1,962:2,1552:1,
1009:1,1056:1,928:1,1083:1,1040:1,415:5,420:5,1017:1,409:4,1045:1,
436:15,412:5,428:13,937:1,1556:1,1561:1,1588:1,1532:1,1572:1,1581:2,
1553:1,1587:1,1590:1,1016:1,1016:1,1016:1,1037:1
```

slot2 (140 записей):

```
812:122008,429:1,411:16,449:5,441:4,408:5,444:1,422:2,410:13,435:5,
423:1,445:13,446:25,419:6,414:5,1544:4,1548:4,1533:3,1562:4,1567:2,
1569:6,1531:2,1582:10,420:8,440:4,430:4,442:10,427:7,960:1,426:2,
432:4,1523:7,23:1,1571:3,1585:3,413:5,424:2,431:5,1517:2,447:11,
1530:2,1570:5,448:22,1545:2,1551:1,952:26,1541:4,1547:3,1558:3,
1584:1,407:13,443:3,1043:1,1522:1,1065:1,1580:2,1581:1,434:13,
1586:3,1037:1,433:8,436:12,1557:2,1559:2,1573:1,416:3,450:1,961:4,
1037:1,1516:1,1519:3,1534:2,1543:2,1563:6,1537:2,1546:1,1589:1,
1518:3,1528:1,1536:3,1550:1,1572:1,417:3,953:25,1037:1,1037:1,
1552:1,1521:3,1529:2,1568:1,76:1,1013:3,1031:1,998:1,1560:1,1542:1,
1564:1,1524:1,1591:1,1002:1,1525:1,1578:1,962:3,415:2,1056:1,
1083:1,1040:1,933:1,949:1,947:1,1526:1,428:4,1535:1,439:1,957:1,
939:1,1583:1,1592:1,943:1,940:1,948:1,950:2,1051:1,1588:1,1052:1,
421:3,418:3,930:1,938:1,955:2,969:1,931:1,959:1,963:1,1515:1,1540:2,
1561:1,1520:1,1539:1,1566:1
```
