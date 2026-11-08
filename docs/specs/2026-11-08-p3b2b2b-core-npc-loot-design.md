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
- Генератор `FUN_140489ec`: порядок записей файла, окно
  `min+offset <= level <= max+offset`, `coin(0.01*chance)`,
  `qty = range(qmin,qmax)`, split по `max_stack`, merge по item_id,
  ранний выход при `>= 4` стеках с restock-метаданными, R1
  `coin(regenerate_chance)` всегда.
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

`gl::game::NpcLootGenerator`:

```cpp
struct LootItem {
    std::uint32_t item_id;
    std::int32_t count;
    std::uint32_t type, flags, style;
};
std::vector<LootItem> generate(const gl::assets::InvTable&,
                               std::int32_t level,
                               const gl::assets::ItemNameIndex&,
                               std::function<std::optional<ItemTemplate>(std::uint32_t)>,
                               MsvcRandom&);
```

- R1 `coin(regenerate_chance)` всегда (даже для пустого контейнера).
- Записи в порядке файла; окно уровней; `coin(0.01*chance)`;
  `qty=range(qmin,qmax)`; `qty<1` — пропуск.
- Прототип предмета: `type/flags/style/max_stack` из ITM; отсутствует —
  пропуск записи (документируется).
- Split: `chunk = min(qty, max_stack)`; merge по item_id (count += chunk;
  оригинальный merge-path +1 — исследовать/учесть в Task 1).
- Ранний выход при `>=4` стеках: остановить генерацию и сохранить
  restock-метаданные (путь/индекс) в NPC-контейнере; фактический restock
  не реализуется (non-goal), но поведение первого поколения сохраняется.
- Specials: **не вызывать** R4–R10. RNG-поток после первого
  special-способного предмета расходится с оригиналом — D-055.

### 3.3 NPC-контейнеры

- `InventoryRuntime` расширяется: `std::map<std::string, PersonInventory>`
  (Hero + NPC). Hero — из save; NPC — из генератора.
- Генерация при загрузке уровня: для каждого `PersonInstance` с непустым
  `inventory` (SEF `scr_inv`) в порядке SEF; путь
  `scripts/inventory/<inventory>.inv`; уровень — из `+0x26c4` (Task 1).
- `TestPersonHasItem`/`GetItemCount` видят NPC-предметы.
- `Add/Remove` для NPC остаются no-op (1b2b1 scope) — документируется.
- Transfer/Trade — вне scope.

### 3.4 Уровень и seed

- Task 1 обязан доказать `+0x26c4` (BT_INFO) и способ получить его из
  save. Если не доказано — implementation **blocked**, код не пишется.
- RNG: тот же `MsvcRandom`, что у `LevelSimulation` (production seed
  D-053; тесты — явный seed). Порядок потребления RNG до генерации
  (movement init и т.п.) должен быть зафиксирован в Task 1; если точный
  порядок недоказуем — генерировать детерминированно от seed и
  документировать расхождение.

## 4. Non-goals

- Specials-модификаторы (`FUN_14047284`/`DAT_14088df4`).
- Restock/regenerate во времени (`regenerate_chance` после первого
  поколения, `chance_decrease`, `quantity_decrease`).
- Transfer/Trade/PassToTradePanel.
- Box/trigger контейнеры.
- Точный pre-level RNG-order, если он недоказуем.

## 5. Тестирование

- `parse_inv`: все 238 реальных файлов; 4-number skip; комментарии;
  synthetic special.
- Генератор: фиксированный seed/level, эталонные значения из псевдокода
  (не из реализации); split/max_stack, merge, ранний выход, unknown ITM,
  пустая таблица.
- Real: один `.inv` + реальный уровень; ожидаемые item ids/counts
  (hardcoded).
- InventoryRuntime: NPC count/money; Hero не меняется.
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

- `+0x26c4` может зависеть от save/party; Task 1 gate.
- Ранний выход при 4 стеках может давать мало предметов; следовать
  research, не «улучшать».
- Merge-path +1 и порядок вставки могут отличаться; Task 1 фиксирует.
- RNG-desync specials влияет на последующие записи; документируется, не
  маскируется.
