# Research: inventory transfer natives (`Server.dll` v1.3)

Дата: 2026-11-08. Задача: Phase 1b2b3, Task 1 (research only,
production-код не менялся). Источник истины — `Server.dll` v1.3 и
реальные `Saves/slot*.sav`, `scripts.pak`, `items.pak`.

## 1. Provenance и метод

| Файл | SHA-256 |
|---|---|
| `I:\Games\Златогорье-2\Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `I:\Games\Златогорье-2\Saves\slot0.sav` | `8AC545FE9715EF11C68B1CCA8617BEB03003ECA360F07F71C26E3FC5E5C73442` |
| `I:\Games\Златогорье-2\Saves\slot2.sav` | `5DB504F57BDFFB66A8B977256AD53EE8E1D6D283E1EDFCD75092FFFD3DB30296` |
| `I:\Games\Златогорье-2\Saves\slot21.sav` | `2544BDBC8C9271478F39D38350A242D795269D107641AF9D6D1C720FBFE934A4` |

- Image base `Server.dll` = `0x14000000`; RVA = VA − `0x14000000`.
  Для `.text` (RVA `0x1000`–`0x749b6`) file offset == RVA; для
  `.data1` (RVA `0x8c000`, file offset `0x80000`) file offset =
  RVA − `0xC000`.
- Декомпил-корпус (gitignored): `.local/decomp/server/decompiled.c`
  (Ghidra 12.1.4), обработчики — `.local/decomp/server/natives/natives.c`.
- Дизассемблер: `llvm-objdump -d -M intel` из
  `llvm-mingw-20261006-ucrt-x86_64` (VA `0x14xxxxxx`).
- Реальные `.cs` — `glpak disasm-cs`; разрешённые аргументы вызовов —
  `%TEMP%\person_calls_resolved.txt` (284 `TransferItemI`, 0
  `TransferAllItemsI`, 111 уникальных файлов).
- Saves: Python `zlib.decompress(raw[112:])`, `BT_HERO `, body =
  `[off+12, off+12+size)`; контейнеры от `body+0x734`, запись
  `0xC4 + u32(+0xC0)*0x14`.
- NPC-лут `l1_1`: значения из `p3b2b2-npc-loot-server.md` §15.6
  (независимый Python-референс, seed `1`, `loot level 1`, main = pass 2).

## 2. Нативы и соглашение вызова

| ID | name | handler RVA | signature | args | return |
|---|---|---|---|---|---|
| `0x5000001` | `RS_PersonTransferItemI` | `0x401dc` | `3,3,3,0` | `source, dest, TECH, count` | `1.0`/`0.0` |
| `0x5000003` | `RS_PersonTransferAllItemsI` | `0x40fb0` | `3,3` | `source, dest` | `1.0`/`0.0` |

Сигнатура подтверждена ассемблером обработчиков:

```
140401e3: push -0x1          ; sentinel
140401e5: mov  eax, 0x3
140401ea: push 0x0            ; num
140401ec: push eax            ; string
140401ed: push eax            ; string
140401ee: push eax            ; string
140401ef: call FUN_14038c6c   ; (3,3,3,0,-1)
```

`RS_PersonTransferAllItemsI`:

```
14040fb5: mov  eax, 0x3
14040fba: push -0x1
14040fbc: push eax
14040fbd: push eax
14040fbe: call FUN_14038c6c   ; (3,3,-1)
```

Marshalling (`FUN_14039054`-подобный, `decompiled.c:40120-40187`):
аргументы лежат в `DAT_14089740` (arg0), `DAT_14089748` (arg1),
`DAT_14089750` (arg2), `DAT_14089758` (arg3); `3` = string, `0` = num.
Отсюда порядок `source, dest, TECH, count`. `count` — `(int)ROUND(double)`
(`40173-40175`), в `FUN_14048790` читается как `uint`.

Обработчики (`natives.c:1644-1771`, `:1797-1839`):

- Персона ищется по точному имени (`entity+0x4c`), тип
  `(entity+0x14 & 0x3e) == 2` или `== 0x10` (TransferItem — один проход
  по сущностям; TransferAll — `FUN_1402914c` + fallback `FUN_14029270`).
- Контейнер — `entity+0x10` (`FUN_1404bf8c`); в `FUN_14048790`/
  `FUN_14048518` передаётся id контейнера `container+4`.
- При успехе (`result != 0`) оба контейнера получают write-only маркер
  `+0x18 = 1/2`; при `0.0` маркер не пишется.
- Возврат: `int` 0/1 загружается как `float10` (`fild`). Константы
  отказа: `0x140913a8` = `double 0.0` (`TransferItem`, сравнение
  `fcom qword`), `0x14091450` = `double 0.0` (сигнатура
  `TransferAll`), `0x14091458` = `float 0.0` (`TransferAll`,
  `fcom dword`). Успех — `1.0`.
- `TransferItem` перед переносом вызывает
  `FUN_1402ee60(DAT_14088dc4, sourceContainerId, destContainerId)`:
  для всех сущностей с `entity+0x10 == sourceContainerId` пишет
  `entity+0x4dc = destContainerId`. Потребитель этого поля не
  установлен (`unverified`); на счётчики не влияет. `TransferAll`
  этот вызов не делает.

Отличия в проверке отсутствующей персоны:

- `TransferItem`: после поиска проверяет `source == 0` и `dest == 0` и
  возвращает `0.0` (`natives.c:1745-1750`) — без AV.
- `TransferAll`: проверки `0` нет; `FUN_1404bf8c(manager,
  *(int*)(0+0x10))` в оригинале даёт AV. Порт: `0.0`/no-op (deviation,
  как D-051/D-052).

## 3. `FUN_14048790` — точная семантика

`decompiled.c:49329-49408`, RVA `0x48790`, `size=602`,
`__thiscall(this, param_1, param_2, param_3, param_4)`.
`this` = `DAT_14088df8` (manager), `param_1` = TECH-строка,
`param_2` = id source-контейнера, `param_3` = id dest-контейнера,
`param_4` = запрошенное количество (`uint`).

### 3.1 Псевдокод

```
FUN_14048790(this, tech, srcId, dstId, count):
    if (srcId == dstId) return 1
    src = container_lookup(this, srcId); if (!src) return 0
    dst = container_lookup(this, dstId); if (!dst) return 0
    FUN_1402ee60(world, srcId, dstId)

    remaining = count
    if ((int)count < 0) remaining = 1000000
    moved_item_id = -1

    loop:
        if ((int)remaining < 1):
            notify(srcId, dstId, moved_item_id, count - remaining)
            return 1

        uid = FUN_1404c690(src, tech)          # first matching record
        rec = item_pool(uid)
        if (rec == 0):
            if (remaining == count) return 0   # nothing was moved
            notify(srcId, dstId, moved_item_id, count - remaining)
            return 1

        if (moved_item_id == -1) moved_item_id = rec->id   # +0xA8
        cur = rec->count                                  # +0xB0
        if ((int)remaining < cur):
            rec->count = cur - remaining
            FUN_1404cd28(dst, tech, remaining)            # add/merge
            remaining = 0
            notify(srcId, dstId, moved_item_id, count - remaining)
            return 1

        FUN_1404a04c(-1, uid, srcId, dstId, 0, -1)        # move whole record
        remaining -= cur
```

### 3.2 Ключевые факты

- **Same container** (`srcId == dstId`): `return 1` до любых изменений.
- **Отсутствующий source/dest контейнер**: `return 0`.
- **Отсутствующий предмет** (`FUN_1404c690 == 0xFFFFFFFF`):
  - `count > 0`: `return 0` (`remaining == count`), без notify;
  - `count == 0`: `return 1`, notify с `0`, без изменений;
  - `count < 0`: `remaining = 1000000 != count` → notify и **`return 1`**
    (оригинальный quirk; изменений нет).
- **`count == 0`**: цикл не делает ни одного lookup; `return 1`, notify с
  количеством `0`. Контейнерный маркер `+0x18` в обработчике всё равно
  ставится (результат `1.0`).
- **`count < 0`**: `remaining = 1000000` (не «всё»: если предметов
  больше 1 000 000, переносится ровно 1 000 000).
- **Partial** (`0 < count < cur`): `rec->count -= count`, затем
  `FUN_1404cd28(dst, tech, count)`; source-запись остаётся.
- **Exact/over** (`count >= cur`): запись source целиком переносится
  через `FUN_1404a04c(..., mode=0, count=-1)`; source-запись удаляется.
  При `count > доступно` переносятся все найденные записи, остаток
  молча игнорируется; возврат всё равно `1`.
- **Несколько записей одного TECH**: каждая итерация заново вызывает
  `FUN_1404c690`, который возвращает **первую used-запись** в порядке
  слотов; после удаления она перестаёт находиться, и обрабатывается
  следующая. Порядок — slot order.
- **Merge/append в dest**: `FUN_1404cd28` (RVA `0x4cd28`,
  `decompiled.c:52606-52739`): слияние по item id (`rec+0xA8`), лимит
  `rec+0x34` (`max_stack`); при `max_stack < 2` слияние не делается, а
  создаются новые записи; остаток делится чанками по `max_stack`.
- **Поля новой dest-записи**: `FUN_1404e3a4`/`FUN_1404f4fc` берут
  `type/flags/style/max_stack` из **ITM-шаблона** (`proto+4/+8/+0x1c/+0x30`),
  а не из source-записи. Source runtime-биты flags (например `0x200` в
  save `POT_1_00_27`) в dest не копируются. Это расхождение с spec
  §3.2/§5.1 («из source-записи») — для Task 2 следовать коду
  `Server.dll`.
- **Ошибка add в dest**: `FUN_1404a04c`/`FUN_1404cd28` результат не
  проверяется; source уже уменьшен/удалён, функция всё равно вернёт `1`
  (если не сработал ранний `return 0` по отсутствию предмета).
- **Source-поля**: partial не трогает flags; full-path `FUN_1404a04c`
  case 0 снимает у source `flags & 0x00FFF000` (строка `50813`) перед
  удалением; на итоговый dest это не влияет.
- **Notify** `FUN_140482e8(srcId, dstId, moved_item_id, moved)`
  (RVA `0x482e8`) — серверная/сетевая нотификация; в одиночном порте
  не моделируется. При отсутствии предмета и `count > 0` notify нет.
- **`+0x18`** — write-only маркер контейнера; в `Server.dll` чтений вне
  mutation-хелперов не найдено (p3b2b1 §5.1). Порт может не моделировать.

### 3.3 UID / id / count

`FUN_1404c690` (RVA `0x4c690`, `decompiled.c:52312-52356`):

```
if (!tech) return -1
item_id = item_manager_vtable +0x2c(tech)     # TECH -> item id
if (!item_id) return -1
for each used slot in container, slot order:
    rec = record_by_uid(container->ids[slot]) # FUN_1404dbec
    if (rec && rec->+0xA8 == item_id) return rec->+0xA4
return -1
```

- `+0xA4` — **instance uid** (индекс в глобальном пуле `DAT_14088df8+0x18`);
  его возвращает `FUN_1404c690`.
- `+0xA8` — **item/prototype id** (для сравнения с TECH-id и notify).
- `+0xB0` — текущее количество записи.
- `+0x34` — `max_stack`; `+0xC` — flags.
- `uid` далее разрешается в объект записи через
  `FUN_1404dbec(container, uid)`; в `FUN_14048790` — прямым индексом
  пула `*(DAT_14088df8+0x18)[uid]`.

## 4. `FUN_14048518` — точная семантика (`TransferAll`)

`decompiled.c:49235-49321`, RVA `0x48518`, `size=629`,
`__thiscall(this, param_1, param_2, param_3)`:
`param_1` = source container id, `param_2` = dest container id,
`param_3` = mode. Натив всегда передаёт `param_3 = 0`.

### 4.1 Псевдокод

```
FUN_14048518(this, srcId, dstId, mode):
    if (srcId == dstId) return 1
    src = container_lookup(srcId); if (!src) return 0
    dst = container_lookup(dstId); if (!dst) return 0

    extra = -1
    if (mode == 1) extra = FUN_1402617c(world, dstId)

    n = src->used_count                     # snapshot +0x10
    i = 0
    while (i < n):
        if (used(src, i)):
            uid = src->ids[i]               # +0xC
            if (mode == 1):
                rec = item_pool(uid)
                if ((rec == 0 || rec->type != 0x19)):
                    if (FUN_1404a04c(extra, uid, srcId, dstId, 1, -1) == 0)
                        return 0
            else:
                FUN_1404a04c(-1, uid, srcId, dstId, mode, -1)
        i += 1
    return 1
```

### 4.2 Ключевые факты

- **Same container**: `return 1`, без изменений.
- **Отсутствующий source/dest контейнер**: `return 0`.
- **Пустой source** (`used_count == 0`): цикл не выполняется, `return 1`.
- **Native path** (`mode == 0`): каждая used-запись source переносится
  целиком (`FUN_1404a04c`, `count=-1`), с merge в dest через
  `FUN_1404cd28`; source-записи удаляются; порядок — slot order.
  Возврат `1` всегда (результат `FUN_1404a04c` не проверяется).
- **`mode == 1`** (`type 0x19` skip, проверка `return 0`) нативом не
  используется; в поставляемых `.cs` `TransferAll` не вызывается
  (0 вызовов), поэтому live-сценарий не наблюдался.
- Source-записи удаляются; после успеха source-контейнер пуст (при
  отсутствии ошибок add; ошибки не проверяются).
- Notify/`+0x18` — как у `FUN_14048790` (обработчик ставит маркер при
  `result != 0`); `FUN_1402ee60` не вызывается.

## 5. Вспомогательные функции

| Функция | RVA | `decompiled.c` | Роль в переносе |
|---|---|---|---|
| `FUN_1404bf8c` | `0x4bf8c` | 51910-51920 | id → container, `0` если нет |
| `FUN_1404c690` | `0x4c690` | 52312-52356 | TECH → uid первой записи |
| `FUN_1404dbec` | `0x4dbec` | 53279-… | uid → record, `0` если нет |
| `FUN_1404dd6c` | `0x4dd6c` | 53350-… | удалить record (used-bit + pool) |
| `FUN_1404cd28` | `0x4cd28` | 52606-52739 | add: merge по id/`max_stack`, split, ITM-поля |
| `FUN_1404cae8` | `0x4cae8` | 52500-52597 | remove (transfer **не** использует) |
| `FUN_1404a04c` | `0x4a04c` | 50361-50986 | generic move; case `mode=0` |
| `FUN_1404e3a4` | `0x4e3a4` | 53626-… | создать record из ITM |
| `FUN_1402ee60` | `0x2ee60` | 33363-33434 | `entity+0x4dc = dstId` (см. §2) |

`FUN_1404a04c` case 0 (`param_5 == 0`): `param_6 == -1` → `param_6 =
rec->count`; `rec->count -= param_6`; tech-имя получается через
vtable `DAT_14088dfc+0x28` (id → имя, asm `0x1404a852-0x1404a861`);
`FUN_1404cd28(dst, tech, param_6)`; если `rec->count == 0` →
`FUN_1404dd6c`. `param_5` 0x32/0x33/0x34 — другие режимы
trade/use, к нативам не относятся.

## 6. Реальные вызовы и диалоги

### 6.1 Статистика

- `RS_PersonTransferItemI`: **284** вызова в **111** уникальных `.cs`.
- `RS_PersonTransferAllItemsI`: **0** вызовов в поставляемых скриптах.
- Направления смешанные: `Hero → NPC`, `NPC → Hero`, деньги
  (`MON_1_0_1`, 40…5000) и quest-предметы (`QST_*`, 1…15).
- Возврат `TransferItemI` во всех 284 случаях присваивается scratch-
  переменной `result`; автоматический разбор 111 файлов не нашёл ни
  одного `==`/`<`/… по `result` в этих файлах. То есть скрипты **не
  ветвятся** по результату переноса; наблюдаемы только последующие
  счётчики/глобалы. Для `TransferAll` реального вызова нет.

### 6.2 Рекомендуемый real-диалог: `l1_1.p387_poison_plotnik`

Файл: `scripts/dialogs/l1_1/l1_1.p387_poison_plotnik.d387.age.cs`
(`glpak disasm-cs`). NPC `L1_1.P387_Poison_Plotnik` — из `l1_1` SEF,
генерируется 1b2b2b (`p387.inv`: `regenerate_chance 0`, одна запись
`QST_1_0_5` 1..100 100% 1 1). Поэтому его main **точно**:
`QST_1_0_5 = 1`, `QST_1_0_3 = 0`.

Детерминированный путь при начальных глобалах
`L1_Svetlograd_n3_GetElexir = 0`, `plotnik_povtor = 0` (проверено
`glscript`: без предмета у Hero выбирается ветка `1006/2191`) и
предзагруженном `Hero.QST_1_0_3 = 1`:

| node | операция |
|---:|---|
| 434 | `D_Say(1006)` |
| 439 | `item = RS_TestPersonHasItem("Hero","QST_1_0_3")` |
| 444 | `item == 1` → 453, иначе 447 |
| 453-457 | `D_Answer(2190)`; затем 448-452 `D_Answer(2191)` → экран 1006: `[2190, 2191]` |
| 266 | `LastAnswer == 2190` → 271 |
| 272 | `D_Say(1203)` |
| 276-282 | `result = RS_PersonTransferItemI("Hero","L1_1.P387_Poison_Plotnik","QST_1_0_3",1)` |
| 283-286 | `D_Answer(2194)` (единственный ответ на экране 1203) |
| 238-244 | `LastAnswer == 2194` → 244 |
| 245 | `L1_Svetlograd_n3_GetElexir = 5` |
| 248 | `plotnik_povtor = 2` |
| 251-260 | `RS_StageComplete("potion","potion_4")`, `RS_StageEnable("potion","potion_5")` |
| 261-264 | `D_CloseDialog(0)` |

Ответы: **индекс 0** на первом экране (`2190`; при наличии предмета
экран также содержит `2191` вторым) и **индекс 0** на втором
(`2194`). Ожидание после прогона (slot0 и slot2 одинаково, т.к.
дело не в save-деньгах):

| объект | до | после |
|---|---:|---:|
| `Hero.QST_1_0_3` | 1 (preload) | **0** (source-запись удалена) |
| `L1_1.P387_Poison_Plotnik.QST_1_0_3` | 0 | **1** (append; `max_stack=1`) |
| `L1_1.P387_Poison_Plotnik.QST_1_0_5` | 1 | **1** (не тронут) |
| `L1_Svetlograd_n3_GetElexir` | 0 | **5** |
| `plotnik_povtor` | 0 | **2** |

`QST_1_0_3` = item id `973`, `type 25`, `flags 0xC0000008`,
`max_stack 1`, `style 15`; итоговая dest-запись берёт эти ITM-поля.

Контроль без preload (Hero `QST_1_0_3 = 0`): ветка `1006` → `D_Answer
2191`; глобал `plotnik_povtor = 1` (node 231-233), переноса нет.
`glscript` подтвердил эту ветку: `dialogs: 1006 2191`, `answers: 1`,
`status: end`.

### 6.3 Второй real-диалог (secondary): `old_test.age.cs`

Файл: `scripts/dialogs/old_test.age.cs` (self-contained test dialog).
Детерминированный путь, ответы: первый экран `10814` с ответами
`[10819,10818,10817]` — **индекс 0** (`10819`); второй экран `10820` с
ответом `[568]` — **индекс 0**.

| node | операция |
|---:|---|
| 141-157 | `D_Say(10814)`, ответы `10819/10818/10817` |
| 119-125 | `LastAnswer == 10819` → 125 |
| 126-129 | `D_Say(10820)` |
| 130-136 | `result = RS_PersonTransferItemI("Hero","L0.M3_Baga","QST_1_0_57",1)` |
| 137-140 | `D_Answer(568)` |
| 108-115 | `D_CloseDialog(0)` |

`glscript --answer 0 --answer 0` реально доходит до node 132
(`[info] script native RS_PersonTransferItemI ... unimplemented`),
затем `status: end` (вывод — §11).

`L0.M3_Baga` есть в SEF `levels/single/l57/l57.sef` (дважды, scr_inv
`l0.m3_baga`); `l0.m3_baga.inv` содержит только `FOD_1_1_1` и
`MON_1_0_1`, **не** `QST_1_0_57`. Поэтому при preload
`Hero.QST_1_0_57 = 1` после диалога: `Hero.QST_1_0_57 = 0`,
`L0.M3_Baga.QST_1_0_57 = 1` (дубликат имени — port first-wins,
documented divergence; второй контейнер не обновляется).

## 7. Real oracle slot0/slot2 (transfer)

Вход: `InventoryRuntime` из save + `generate_npc_inventories` на
`l1_1` persons, seed `1`, loot level `1` (как 1b2b2b §15.6).
Счётчики — только **main** (`entity+0x10`); hot (`+0xC`) не входит.

Исходные значения (save scan + `l1_1` oracle):

| ключ | slot0 | slot2 |
|---|---:|---:|
| `Hero.MON_1_0_1` | 43875 | 122008 |
| `Hero.POT_1_00_27` (id 953) | 51 | 25 |
| `Hero.RGT_1_0_8` (id 444) | 6 | 1 |
| `Hero.QST_1_0_3` / `QST_1_0_5` / `QST_1_0_57` | 0 / 0 / 0 | 0 / 0 / 0 |
| `L0.P7_Kolbasens_Rodstvennik1.MON_1_0_1` | 174 | 174 |
| `L1_1.P387_Poison_Plotnik.QST_1_0_5` | 1 | 1 |
| `L1_1.P387_Poison_Plotnik.QST_1_0_3` | 0 | 0 |
| `L0.P678_citizen4` | empty | empty |

### 7.1 `transfer_item` (count/направление)

| вызов | slot0 | slot2 | return |
|---|---|---|---|
| `Hero→P7, MON_1_0_1, 100` | Hero 43875→**43775**; P7 174→**274** | Hero 122008→**121908**; P7 174→**274** | true |
| `P7→Hero, MON_1_0_1, 100` | P7 174→**74**; Hero 43875→**43975** | P7 174→**74**; Hero 122008→**122108** | true |
| `P7→Hero, MON_1_0_1, 1000` (over) | P7 174→**0** (record(s) deleted); Hero 43875→**44049** | P7 174→**0**; Hero 122008→**122182** | true |
| `Hero→P7, MON_1_0_1, -1` (negative) | Hero 43875→**0**; P7 174→**44049** | Hero 122008→**0**; P7 174→**122182** | true |
| `Hero→P7, MON_1_0_1, 0` | без изменений | без изменений | true |
| `Hero→P387, QST_1_0_5, 1` (absent) | без изменений | без изменений | **false** |
| `Hero→P678 (empty), POT_1_00_27, 10` | Hero POT 51→**41**; P678 0→**10** | Hero POT 25→**15**; P678 0→**10** | true |
| `P387→Hero, QST_1_0_5, 1` | P387 1→**0**; Hero 0→**1** | P387 1→**0**; Hero 0→**1** | true |
| real-dialog `Hero(preload 1)→P387, QST_1_0_3, 1` | Hero 1→**0**; P387 0→**1** | Hero 1→**0**; P387 0→**1** | true |
| `Hero→Hero, MON_1_0_1, 100` | без изменений | без изменений | true |

Замечания:

- В over/negative случаях у P7 могут быть несколько money-записей
  (`inv018_garbage_reagent.inv` содержит 15 `MON_1_0_1`); переносятся
  все до исчерпания, поэтому total точный, а число записей — нет.
- `POT_1_00_27` (id 953) в save имеет flags `0x8000020C`; новая запись
  в P678 получает ITM flags `0x8000000C` (runtime-бит `0x200` не
  копируется) — следствие §3.2.
- `QST_1_0_3`/`QST_1_0_5` имеют `max_stack=1`: запись не сливается с
  существующей, а добавляется отдельной (append в конец для порта).

### 7.2 `transfer_all`

| вызов | slot0 | slot2 | return |
|---|---|---|---|
| `P387→Hero` | P387 main **пуст**; Hero `QST_1_0_5` 0→**1** | то же | true |
| `P678→Hero` (empty source) | без изменений | без изменений | true |
| `Hero→P678` | Hero main **пуст** (0 записей; money 0, POT 0, RGT 0); P678: money **43875**, POT **51**, RGT **6** | Hero main **пуст** (140 записей → 0); P678: money **122008**, POT **25**, RGT **1** | true |
| `Hero→Hero` | без изменений | без изменений | true |

При `Hero→P678` все 117 (slot0) / 140 (slot2) main-записей Hero
переносятся в slot order; записи с `max_stack=1` остаются отдельными,
с `max_stack>=2` сливаются. Суммы по TECH сохраняются. Hot-контейнер
Hero не затрагивается. `money()` после переноса = 0 (money-запись
ушла из main); для P678 деньги считать через
`count(P678,"MON_1_0_1")`.

## 8. Trade-контейнеры / `0x5000006/7` — вне scope (подтверждено)

- `RS_PersonAddItemToTrade` (`0x5000006`, RVA `0x412a4`,
  `natives.c:1931-1957`) и `RS_PersonRemoveItemToTrade` (`0x5000007`,
  RVA `0x41400`, `natives.c:1961-1987`) берут контейнер
  `*(int *)(entity+0xC)` (`natives.c:1946`, `:1976`).
- `TransferItem`/`TransferAll` работают с `entity+0x10`
  (`natives.c:1752-1753`, `:1821-1822`).
- Общего состояния/вызовов нет; trade-контейнер не участвует в
  переносе. Реализация `0x5000006/7` и trade UI — вне scope 1b2b3.

## 9. Следствия для порта (Task 2)

1. `transfer_item(from,to,tech,count)`:
   - same person → `true`; отсутствующая/неизвестная персона/контейнер →
     `false`/no-op (deviation от AV для `TransferAll`/missing container);
   - `count == 0` → `true`, без изменений; `count < 0` → лимит
     `1 000 000`; положительный `count` при отсутствии предмета →
     `false`;
   - partial: уменьшить source-запись, добавить в dest;
   - exact/over: удалить source-записи целиком, остаток игнорировать,
     вернуть `true`.
2. `transfer_all(from,to)`: все main-записи source → dest в slot order;
   dest add/merge; source очищается; пустой source/тот же контейнер →
   `true`; missing person/container → `false`/no-op (deviation от AV).
3. Dest-запись создаётся из **ITM-шаблона** порта
   (`ItemTemplateLookup`): `type/flags/style/max_stack`, а не копия
   source-record (spec §3.2/§5.1 требует правки в Task 2).
4. Слияние — по `item_id`, лимит `max_stack` из текущего ITM; при
   `max_stack < 2` — новые записи (split по `max_stack`).
5. NPC add/remove natives остаются no-op, но transfer мутирует NPC main
   напрямую (как в оригинале `FUN_1404cd28`/`FUN_1404a04c`).
6. Не моделировать: notify `FUN_140482e8`, container `+0x18`,
   `entity+0x4dc` (`FUN_1402ee60`), trade-контейнер `+0xC`.
7. `max_stack == 0` в шаблоне — port guard (no-op), в оригинале
   возможен незавершающийся creation-loop (p3b2b1 §3.4).
8. Порядок новых записей: оригинал — первый свободный слот, порт
   vector append (уже documented deviation 1b2b1).

## 10. Unverified / open questions

1. Потребитель `entity+0x4dc`, записываемого `FUN_1402ee60` при
   `TransferItem` (вероятно, AI/квест/trade-target), не установлен.
2. Потребитель container `+0x18` и event-кодов notify `0xB4..0xB7`
   (p3b2b1 §5, §10) — не меняется.
3. Живой прогон negative-count и missing-container (AV) не выполнялся;
   все выводы — статический дикода + реальные данные.
4. Выбор сущности при **дубликате имени** (например, два `L0.M3_Baga`
   в `l57`) в `TransferItem`-скане не трассирован; порт использует
   first-wins контейнеров 1b2b2b.
5. Точный порядок записей в dest после переноса нескольких записей
   одного id через первый свободный слот оригинала не снят live; порт
   append (documented).
6. `TransferAll` `mode=1` (skip `type 0x19`, `FUN_1402617c`) нативом не
   используется и live не наблюдался.
7. Live-оракул (чтение памяти запущенной игры) не снимался; oracle —
   save + независимый Python-референс NPC-лута + реальные `.cs`.

## 11. Проверка (фактический вывод)

Сигнатуры (objdump):

```
140401e3 push -0x1 ; 140401e5 mov eax,0x3 ; push 0 ; push eax ; push eax ; push eax
140401ef call 0x14038c6c          ; TransferItemI: (3,3,3,0,-1)
14040fb5 mov eax,0x3 ; push -0x1 ; push eax ; push eax
14040fbe call 0x14038c6c          ; TransferAllItemsI: (3,3,-1)
14040353 push [0x14089758] ; push dest+4 ; push src+4 ; push [0x14089750]
1404036b call 0x14048790          ; (tech, srcContainerId, dstContainerId, count)
```

Константы возврата (`Server.dll`, file offset = RVA − `0xC000`):

```
0x913a8 off=0x853a8 bytes=00 00 00 00 00 00 00 00 double=0.0  (TransferItem miss)
0x91450 off=0x85450 bytes=00 00 00 00 00 00 00 00 double=0.0  (TransferAll sig)
0x91458 off=0x85458 bytes=00 00 00 00 00 00 00 00 float=0.0   (TransferAll cmp)
```

`FUN_1404c690` (asm): TECH → id via vtable `+0x2c`; сравнение
`rec+0xA8 == id`; возврат `rec+0xA4` (`0x1404c757-0x1404c794`),
иначе `0xFFFFFFFF`.

Save scan (Python, main-контейнер `+0x10`, `body+0x734`):

```
slot0: Hero money=43875 POT_1_00_27=51 RGT_1_0_8=6
       QST_1_0_3=0 QST_1_0_5=0 QST_1_0_57=0; main records=117
slot2: Hero money=122008 POT_1_00_27=25 RGT_1_0_8=1
       QST_1_0_3=0 QST_1_0_5=0 QST_1_0_57=0; main records=140
slot21: Hero money=1000 QST_1_0_51=1; main records=2
```

ITM (id 973/975/1025/953/812/444): `QST_1_0_3` type 25 flags
`0xC0000008` max_stack 1 style 15; `QST_1_0_5` type 25 flags
`0x40000008` max_stack 1 style 15; `QST_1_0_57` type 25 flags
`0x28` max_stack 1 style 15; `POT_1_00_27` type 19 flags
`0x8000000C` max_stack 10000; `MON_1_0_1` type 18 flags
`0xC0000000` max_stack 100000000; `RGT_1_0_8` type 24 flags
`0x40000008` max_stack 10000.

`glscript` (old_test, ответы 0/0; реальный node 132 достигнут):

```
dialogs: 10814 10819 10818 10817
answers: 3
[info] script native RS_PersonTransferItemI (0x5000001): unimplemented - requires item runtime state
dialogs: 10820 568
answers: 1
status: end
```

`glscript` (p387, Hero без `QST_1_0_3`, контроль ветки):

```
dialogs: 1006 2191
answers: 1
status: end
```

`.cs`-статистика: `TransferItemI` 284, `TransferAllItemsI` 0,
111 уникальных файлов; автоматический разбор не нашёл ветвлений по
`result` в этих файлах.

Документ: UTF-8 без BOM, `nul=0` (проверка — отчёт Task 1).
