# Phase 1b2b2b — core NPC `.inv` loot (no specials) design

Дата: 2026-11-08. Статус: approved (пользователь, 2026-11-08).

## 1. Цель

Реализовать стартовый NPC-инвентарь из `scripts/inventory/*.inv`:
парсер таблиц, генератор базовых предметов/денег и NPC-контейнеры в
`InventoryRuntime`. Специальные модификаторы (`FUN_14047284`/
`DAT_14088df4`) не генерируются — это осознанное ограничение с
документированным RNG-desync.

## 2. Контекст

- `Server.dll` v1.3, SHA-256
  `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Research: `docs/research/p3b2b2-npc-loot-server.md` §3–§8, §11–§13.
- `.inv`: заголовок `regenerate_chance`, `chance_decrease`,
  `quantity_decrease`, `level_offset`; строки `item "<TECH>" <minlvl>
  <maxlvl> <chance> <qty_min> <qty_max>`; 4-number строки пропускаются
  (362 в корпусе); `special` в реальных данных не активен.
- **Реальный SEF-путь NPC — `FUN_1404b1c0`**, а не `FUN_140489ec`
  (см. research §15). Один проход по таблице на контейнер, всего два
  прохода:
  - pass 1 → `+0xc` (hot): окно
    `min+offset <= level <= max+offset`, R2 `coin(0.01*n3)`, R3
    `range(n4,n5)`, создание `FUN_1404e3a4(name,0,1)` и прямая
    установка `item+0xb0 = min(qty, max_stack)`; `remaining -= min`.
    `this+4=1` только очищает item-флаг `0x80` (`53919-53921`), не
    гейтит merge; pass-1 merge возможен/`unverified`. Chunk-loop нет.
    Порог метаданных `100000` практически недостижим — **4-stack exit
    отсутствует**.
  - pass 2 → `+0x10` (**main**; его читают
    `TestPersonHasItem`/`GetItemCountI`): chance
    `(0.01*n3)/chance_decrease` (при `n3==0` coin не бросается),
    qty `n4/n5`, масштабированные `1/quantity_decrease`
    (trunc + `+1` при дробной части) → `range`; создание с
    `this+4=0` и прямой установкой count; порог `100000`.
  - R1 `coin(regenerate_chance)` — всегда (SEF вызывает с
    `param_4=1`, поэтому R1 не отменяет генерацию).
- `FUN_140489ec` (caller-квирки `qty -= chunk`, early exit `>=4`) —
  другой путь (hero/иные источники); merge `+1` — shared
  `FUN_1404e3a4` (pass-1/2 merge `unverified`). Caller-квирки к
  `FUN_1404b1c0` **не** применять.
- `level = *(DAT_14088dc4+0x26c4) + 1`; `+0x26c4` читается из `BT_INFO`
  save (`FUN_1403210c`, `decompiled.c:35638`).
- Спец-модификаторы: `roll_special_count` (R4/R5) + `FUN_14047284`
  (R6–R10) — **вне scope**.
- Порт: `MsvcRandom` (D-053/D-054), `ItemNameIndex`, `ItemCatalog`
  (`Item` даёт type/flags/max_stack/style=`unknown_b[1]`),
  `InventoryRuntime` (Hero).

## 3. Scope

### 3.1 Парсер `.inv`

`gl::assets::InvTable`:

```cpp
struct InvItem {
    std::string tech_name;
    std::int32_t min_level = 0;
    std::int32_t max_level = 0;
    double chance_percent = 0.0;
    std::int32_t qty_min = 0;
    std::int32_t qty_max = 0;
};
struct InvTable {
    double regenerate_chance = 0.0;
    double chance_decrease = 0.0;
    double quantity_decrease = 0.0;
    std::int32_t level_offset = 0;
    std::vector<InvItem> items;
};
Result<InvTable> parse_inv(std::span<const std::uint8_t>);
Result<InvTable> load_inv(GameVfs&, std::string_view name);
```

- 4-number `item` — пропуск (как оригинал), warning один раз.
- `special` — читается и игнорируется (в данных нет); без догадок.
- Комментарии `//` (whitespace-separated) и кавычки — как в оригинале.

### 3.2 Генератор

`gl::game::NpcLootGenerator` реализует initial spawn `FUN_1404b1c0`
(два прохода, тот же порядок записей и RNG, что в оригинале):

```cpp
struct LootItem {
    std::uint32_t item_id;
    std::int32_t count;
    std::uint32_t type, flags, style;
};
struct NpcLoot {
    std::vector<LootItem> hot;   // pass 1 -> +0xc
    std::vector<LootItem> main;  // pass 2 -> +0x10 (queries)
};
NpcLoot generate(const gl::assets::InvTable&,
                 std::int32_t loot_level,   // party_level + 1
                 const gl::assets::ItemNameIndex&,
                 std::function<std::optional<ItemTemplate>(std::uint32_t)>,
                 MsvcRandom&);
```

- R1 `coin(regenerate_chance)` всегда; SEF вызывает с `param_4=1`,
  поэтому R1 не отменяет генерацию. Контейнеры очищаются.
- **Pass 1 (`hot`, `+0xc`)**: записи в порядке файла; окно уровней;
  R2 `coin(0.01*n3)`; `n3==0` — пропуск (защитный дубль); R3
  `range(n4,n5)`; `qty<1` — пропуск; цикл:
  `FUN_1404e3a4(name,0,1)` с `this+4=1` (только clears item-флаг
  `0x80`, не gate merge), затем `item+0xb0 = min(remaining,
  max_stack)`, `remaining -= min`. Merge возможен/`unverified`;
  chunk-loop нет. Раннего выхода `>=4` нет; порог метаданных
  `100000` практически недостижим.
- **Pass 2 (`main`, `+0x10`)**: окно уровней; если `n3==0` — chance
  coin не бросается, иначе R2'
  `coin((0.01*n3)/chance_decrease)`; R3' — `n4/n5`, умноженные на
  `1/quantity_decrease`, trunc + `+1` при дробной части, затем
  `range`; создание `FUN_1404e3a4(name,0,1)` с `this+4=0` (item-флаг
  `0x80` сохраняется, merge search от него не зависит) и прямой
  установкой `item+0xb0 = min(remaining, max_stack)`;
  `remaining -= min`. Merge возможен/`unverified`. Порог метаданных
  `100000`, restock-флаг `+0x1c = 0`. Деление на
  `chance_decrease`/`quantity_decrease` не защищено; в корпусе нулей
  нет, поведение при 0 — `unverified`/D-055.
- Прототип предмета: `type/flags/style/max_stack` из ITM; отсутствует —
  пропуск записи (документируется).
- Specials: **не вызывать** R4–R10. RNG-поток после первого
  special-способного предмета расходится с оригиналом — запись в D-055
  (Task 5).
- `FUN_140489ec` (caller-квирки `qty -= chunk`, early exit `>=4`) не
  используется для NPC SEF; merge `+1` — shared `FUN_1404e3a4`,
  pass merge `unverified`; caller-квирки на `FUN_1404b1c0` не
  переносить.

### 3.3 NPC-контейнеры

- `InventoryRuntime` расширяется: `std::map<std::string, PersonInventory>`
  (Hero + NPC). Hero — из save; NPC — из генератора.
- Генерация при загрузке уровня: для каждого `PersonInstance` с непустым
  `inventory` (SEF `scr_inv`) в порядке SEF; путь
  `scripts/inventory/<inventory>.inv`; уровень — `party_level + 1`
  (`+0x26c4 + 1`, Task 1).
- **Контейнеры:** main (`+0x10`) — обязателен: именно его читают
  `TestPersonHasItem`/`GetItemCountI` (research `p3b2a` §2.1/§2.2/§4.3).
  Hot (`+0xc`) — pass 1; если runtime хранит только один контейнер,
  hot допускается не сохранять, но **RNG pass 1 всё равно должен быть
  выполнен** для выравнивания потока перед pass 2 (документируется как
  divergence). Если runtime хранит оба — сохранять оба.
- `TestPersonHasItem`/`GetItemCount` видят NPC-предметы из main.
- `Add/Remove` для NPC остаются no-op (1b2b1 scope) — документируется.
- Transfer/Trade — вне scope.

### 3.4 Уровень и seed

- Task 1: `+0x26c4` доказан как сохранённый **party level**;
  `loot_level = value + 1`. Источник: `BT_INFO` save (если save —
  источник сессии), иначе runtime party level / `0` на новом старте.
- RNG: тот же `MsvcRandom`, что у `LevelSimulation` (production seed
  D-053; тесты — явный seed). Лут (оба прохода `FUN_1404b1c0`)
  потребляется в SEF-порядке до создания `LevelSimulation`; тот же
  advanced state передаётся в симуляцию без reseed. Если точный
  tick-order недоказуем — генерировать детерминированно от seed и
  документировать расхождение.

## 4. Non-goals

- Specials-модификаторы (`FUN_14047284`/`DAT_14088df4`).
- Restock/regenerate во времени после первого поколения
  (`regenerate_chance` в тиках); при этом `chance_decrease` и
  `quantity_decrease` **в scope** — они применяются pass 2
  `FUN_1404b1c0` уже при initial spawn.
- Хранение hot-контейнера (`+0xc`, pass 1), если runtime хранит только
  main; RNG pass 1 всё равно выполняется (см. §3.3).
- `FUN_140489ec` (hero/иной путь) и его caller-квирки
  (`qty -= chunk`, early exit `>=4`); merge `+1` — shared
  `FUN_1404e3a4`, pass merge `unverified`.
- Transfer/Trade/PassToTradePanel.
- Box/trigger контейнеры.
- Точный pre-level RNG-order, если он недоказуем.

## 5. Тестирование

- `parse_inv`: все 238 реальных файлов; 4-number skip; комментарии;
  synthetic special.
- Генератор: фиксированный seed/level, эталонные значения из
  псевдокода (не из реализации); R1 всегда; pass 1 — split по
  `max_stack` (merge shared-helper `unverified`) без 4-stack exit;
  pass 2 — chance `/chance_decrease`, qty `1/quantity_decrease` +
  trunc/`+1`, `n3==0` без chance coin; порядок двух проходов и RNG;
  unknown ITM; пустая таблица. Caller-квирки (`qty -= chunk`,
  4-stack) — отдельный тест для `FUN_140489ec`. Merge behavior
  (pass 1/2) is `unverified` — не утверждать/не тестировать; тесты
  покрывают только proven R1/R2/R3, split/count set.
- Real: один `.inv` + реальный уровень; oracle — **main** (pass 2)
  item ids/counts (hardcoded).
- InventoryRuntime: NPC main count/money; hot — если хранится; Hero
  не меняется.
- E2E `glgame --level l1_1` без регрессии; real-data stability.
- Full `ctest`.

## 6. Критерии приёмки

1. Task 1 verdict: `+0x26c4`/BT_INFO доказан (или явный BLOCKED без кода).
2. `parse_inv` 238/238; generator tests pass.
3. NPC-контейнеры заполняются; queries видят предметы.
4. RNG-desync specials задокументирован (D-055), specials не вызываются.
5. Targeted + full `ctest` pass; STATUS/PROJECT/parity обновлены.
6. Документы UTF-8 без BOM/NUL.

## 7. Риски

- `+0x26c4` зависит от save/party; Task 1 gate (доказан: средний
  уровень партии).
- **Нет 4-stack exit:** `FUN_1404b1c0` не останавливается на 4 стеках;
  порог метаданных `100000` практически недостижим. Не добавлять
  port-side cap, иначе количество предметов будет занижено.
- Merge `+1` — shared `FUN_1404e3a4`; caller-квирки (`qty -= chunk`,
  4-stack exit) — только `FUN_140489ec`. Pass-1/2 merge в
  `FUN_1404b1c0` — `unverified`; не добавлять port-side force-new.
- Деление на `chance_decrease`/`quantity_decrease` не защищено; в
  корпусе нулей нет, поведение при 0 — `unverified`/D-055 (Task 5).
- **Контейнеры/второй проход:** main — `+0x10` (pass 2), hot — `+0xc`
  (pass 1). Если runtime хранит только main, pass 1 RNG всё равно
  обязателен; иначе pass 2 рассогласован. Проверять oracle по main.
- Pass 2 использует `chance_decrease`/`quantity_decrease` уже на
  spawn; это не restock.
- RNG-desync specials влияет на последующие записи; документируется в
  D-055 (Task 5), не маскируется.
