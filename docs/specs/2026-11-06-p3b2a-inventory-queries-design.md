# Phase 1b2a — Hero inventory query natives (design)

Дата: 2026-11-06. Статус: approved (пользователь, 2026-11-06).

## 1. Цель

Снять часть `P3-9` для inventory: реализовать read-only запросы инвентаря
персоны на состоянии порта:

- `RS_TestPersonHasItem` (`0x5000000`);
- `RS_GetItemCountI` (`0x5000002`);
- `RS_GetMoney` (`0x5000008`).

Источник данных — `HeroInventory` из выбранного save (контейнер
`entity+0x10`); предметы идентифицируются по TECH-имени из
`sdb/items/tech_names.sdb`.

## 2. Контекст

- `Server.dll` v1.3, SHA-256
  `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Native-таблица: `docs/research/p3-vm-natives-server.md` §5.
- Реальные данные (556 `.cs`):
  - `RS_TestPersonHasItem`: 321 вызов, почти все `"Hero"` + TECH-имя
    (`QST_1_0_*`, `ARM_1_08_2`, `MON_1_0_1`, …);
  - `RS_GetItemCountI`: 313 вызовов, 289 — `"Hero"/"MON_1_0_1"`;
  - `RS_GetMoney`: в поставляемых `.cs` не вызывается.
- `tech_names.sdb`: 1848 записей, имена уникальны; `MON_1_0_1 → 812`,
  `ARM_1_08_2 → 307`, `QST_1_0_159 → 1101`.
- `HeroInventory::items` (`parse_hero_inventory`) хранит `item_id`
  (record `+0xA8`), `count` (`+0xB0`), `type` (`+0x08`); money — записи
  с `type == 0x12` (18).
- `Server.dll`:
  - `FUN_1404c574(container, tech_name)` разрешает TECH-имя в item id
    (vtable `DAT_14088dfc+0x2c`) и суммирует `count` записей с этим id
    (`decompiled.c:52252-52304`);
  - `FUN_1404c494(container)` суммирует `count` записей с `type == 0x12`
    (`decompiled.c:52205-52244`);
  - `RS_TestPersonHasItem` возвращает `1.0` при `count > 0`, иначе `0.0`
    (константы `DAT_14091398 = 1.0`, `DAT_140913a0 = 0.0`);
  - `RS_GetItemCountI` возвращает `count` (default `DAT_14091408 = 0.0`);
  - `RS_GetMoney` возвращает сумму money-записей текущей персоны (default
    `DAT_14091478 = 0.0`);
  - если персона не найдена среди EF_PERSON, натив пробует
    `FUN_14029270` — поиск среди сущностей типа `0x10` (контейнеры/ящики
    по имени), затем возвращает default.

## 3. Scope

### 3.1 Поддерживаемые нативы

| ID | Имя | Поведение порта |
|---|---|---|
| `0x5000000` | `RS_TestPersonHasItem` | `1.0`, если count > 0, иначе `0.0` |
| `0x5000002` | `RS_GetItemCountI` | count (double) |
| `0x5000008` | `RS_GetMoney` | сумма count записей type 18 |

Остальные inventory-нативы (`Transfer`, `Add/Remove`, `Trade`) остаются
`unsupported`.

### 3.2 Семантика

- Person lookup — точное case-sensitive имя; `"Hero"` — специальная
  запись, всегда присутствующая.
- Hero-контейнер — `HeroInventory::items` из save; без save — пустой.
- TECH-имя разрешается через `ItemNameIndex` (точное сравнение,
  case-sensitive). Неизвестное имя → `0`.
- `count(person, tech)` — сумма `count` записей контейнера с
  `item_id == ItemNameIndex.find(tech)`.
- `money()` — сумма `count` записей Hero-контейнера с `type == 18`.
- NPC и контейнеры-ящики: имена находятся, но контейнер не моделируется
  (генерируемый лут/`.inv` — Phase 1b2b/c), поэтому count = `0`.
  Это ограничение фиксируется в `DECISIONS.md` и `parity-matrix`.
- `RS_GetMoney` работает на текущей персоне; в порте текущая — `"Hero"`.

## 4. Non-goals

- `RS_PersonTransferItemI`, `RS_PersonTransferAllItemsI`.
- `RS_PersonAddItem`, `RS_PersonRemoveItem`.
- Trade-варианты (`0x5000006`/`0x5000007`).
- Генерация NPC-лута из `scripts/inventory/*.inv`.
- Контейнеры-ящики (`EF_INVCONTAINER`, `.inv` для boxes).
- Изменение save, UI-инвентаря, торговли, веса/цены.

## 5. Архитектура

### 5.1 `gl::assets`

- `class ItemNameIndex` (`item_name_index.hpp/.cpp`):
  - `static Result<ItemNameIndex> load(const GameVfs&);`
  - `std::optional<std::uint32_t> find(std::string_view tech_name) const;`
  - читает `sdb/items/tech_names.sdb` через `Sdb::parse`, строит reverse
    map `name → id`; дубликаты (если появятся) — первая запись + warning.
- Не менять `ItemCatalog` (P5b1).

### 5.2 `gl::script`

- `struct InventoryAccess { std::function<std::int32_t(std::string_view
  person, std::string_view tech_name)> item_count;
  std::function<std::int32_t()> money; };`
- `NativeServices` получает поле `InventoryAccess inventory;`.
- `native_registry.cpp`: три handler'а; arity 2/2/0; типы `3,3` у
  Test/Count; отсутствие callback'а → `0.0`; неверные аргументы →
  warning + `0.0`.
- ID `0x5000000`, `0x5000002`, `0x5000008` → `supported`.

### 5.3 `gl::game`

- `class InventoryRuntime`:
  - `static InventoryRuntime create(const HeroInventory&,
    const ItemNameIndex&);`
  - `std::int32_t count(std::string_view person, std::string_view tech_name)
    const;`
  - `std::int32_t money() const;`
  - `gl::script::InventoryAccess access();`
  - хранит `std::map<std::uint32_t, std::int32_t>` count по item_id и
    `money_`; имя персоны сравнивается с `"Hero"` (остальные → 0).
- `InventoryRuntime` не зависит от SDL.

### 5.4 Проводка

- `glgame`: `ItemNameIndex` загружается при boot; `InventoryRuntime`
  перестраивается после успешного `apply_equip_save` (или пустой при
  отсутствии save); `open_dialog` прокидывает
  `context->host.services().inventory = inventory_runtime_.access();`.
- `glscript`: пустые callbacks → нули.

## 6. Research deliverables (Task 1)

`docs/research/p3b2a-inventory-queries-server.md`:

1. Подтвердить RVA/константы Test/Count/Money и типы аргументов.
2. Подтвердить `FUN_1404c574` (TECH→id + sum count) и `FUN_1404c494`
   (type 18 sum) с RVA/file:line.
3. Подтвердить `entity+0x10 = HeroInventory.items` (container 1) по
   `FUN_1404bf8c`/P5b1 layout.
4. Проверить `tech_names.sdb` (1848, уникальность, case) и записать
   маппинг для реально используемых имён.
5. Посчитать реальные counts/money для slot0/slot2 (MON_1_0_1, QST_*,
   ARM_1_08_2) и записать как oracle.
6. Найти реальный `.cs`, ветка которого зависит от
   `GetItemCount/TestPersonHasItem` (кандидат для real-теста).
7. Зафиксировать `FUN_14029270` (EF_INVCONTAINER boxes) и NPC-лут как
   вне scope; limitation.

## 7. Обработка ошибок

- Персона не найдена / не Hero → `0` (Test/Count/Money).
- TECH-имя не найдено → `0`.
- Нет save/пустой инвентарь → `0`.
- Неверная арность/тип → warning + `0`.
- Отсутствие callback'а (glscript) → `0`.

## 8. Тестирование

Unit:

- `ItemNameIndex`: реальный sdb, точный поиск, регистр, неизвестное имя;
  synthetic duplicate.
- `InventoryRuntime`: counts по item_id, money type 18, отсутствующая
  персона/tech, пустой инвентарь, `access()` callbacks.
- `gl_script`: arity/типы, Test=1.0/0.0, Count, Money, пустые callbacks.

Real:

- slot0/slot2: counts для `MON_1_0_1`, `QST_1_0_159`, `ARM_1_08_2` и
  money совпадают с research oracle.
- Реальный диалог из research через `GameScriptHost` + `InventoryRuntime`
  выбирает ожидаемую ветку.
- `glgame --level l1_1 --click-npc 13` без регрессии.

Полный `ctest` с `GL_GAME_DIR`.

## 9. Критерии приёмки

1. Research содержит RVA/константы, `tech_names→id`, real counts/money
   slot0/slot2 и выбранный real-диалог.
2. `NativeRegistry` 3 ID → `supported`; supported всего 11/65.
3. `ItemNameIndex` + `InventoryRuntime` покрыты unit + real тестами.
4. Real-диалог доказывает путь `ScriptVm → GameScriptHost →
   InventoryRuntime`.
5. Targeted + полный `ctest` pass; числа в `STATUS`.
6. `STATUS`/`PROJECT`/`parity-matrix`/`DECISIONS` (D-051) обновлены.

## 10. Риски

- TECH-имена в `.sdb` и `.cs` могут отличаться регистром; research
  проверяет точное совпадение.
- Money может быть представлен несколькими type-18 item id; сумма по
  type 18 покрывает это.
- NPC/box запросы вернут `0` при наличии лута в оригинале — ограничение
  документируется, а не маскируется.
- `GetMoney` использует текущую персону; при отсутствии Hero в сцене
  вернётся `0`.
