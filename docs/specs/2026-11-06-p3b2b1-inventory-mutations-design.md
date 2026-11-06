# Phase 1b2b1 — Hero inventory mutations (design)

Дата: 2026-11-06. Статус: approved (пользователь, 2026-11-06).

## 1. Цель

Реализовать мутирующие inventory-нативы для Hero:

- `RS_PersonAddItem` (`0x5000004`);
- `RS_PersonRemoveItem` (`0x5000005`).

Семантика — из `Server.dll`; состояние — runtime-копия `HeroInventory` из
save. Save не пишется.

## 2. Контекст

- `Server.dll` v1.3, SHA-256
  `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Native-таблица: `docs/research/p3-vm-natives-server.md` §5.
- Реальные данные (556 `.cs`): `AddItem` 287, `RemoveItem` 250; почти все
  `"Hero"`, деньги — `MON_1_0_1` (item id 812, type 18).
- `item_name_index` / `tech_names.sdb`: 1848 уникальных имён; TECH→id.
- `HeroInventory.items` (`parse_hero_inventory`): `item_id` (`+0xA8`),
  `count` (`+0xB0`), `type` (`+0x08`), `flags` (`+0x0C`), `style` (`+0x20`).
- `Server.dll`:
  - `RS_PersonAddItem(person, tech, count)`: `FUN_1404bf8c(entity+0x10)`
    (fallback `FUN_14029270` для EF type `0x10`), при `count != 0`
    `FUN_1404cd28(container, tech, count)`, затем container state `+0x18`
    (`0→1`, иначе `2`), и `FUN_140482e8(-1, person_res, item_id, count)`;
  - `RS_PersonRemoveItem`: `FUN_1404cae8(container, tech, count)`, state
    `+0x18`, `FUN_140482e8(person_res, -1, item_id, count)`;
  - оба возвращают `0.0`.

## 3. Scope

### 3.1 Поддерживаемые нативы

| ID | Имя | Поведение порта |
|---|---|---|
| `0x5000004` | `RS_PersonAddItem` | добавить/stack предмет, вернуть `0.0` |
| `0x5000005` | `RS_PersonRemoveItem` | удалить/уменьшить count, вернуть `0.0` |

Остальные inventory ID остаются без изменений.

### 3.2 Семантика

Точные правила add/remove (stacking, лимиты, частичное удаление, `count
== 0`, отрицательный count, неизвестный TECH, отсутствующая персона,
state `+0x18`) фиксируются research (Task 1) и реализуются буквально.

Базовые правила (подтверждаются research):

- Person — только точное `"Hero"`; иначе no-op.
- TECH-имя — exact case-sensitive; неизвестное → no-op.
- `count == 0` → no-op (натив проверяет `!= 0`).
- Отрицательный `count`: в поставляемых `.cs` не встречается; оригинал
  передаёт его как `uint` (поведение не доказано). Порт делает no-op и
  фиксирует это отклонение в D-052.
- Add: если запись с `item_id` есть — увеличить `count` с учётом
  `max_stack` из ITM (split на несколько записей, если `max_stack >= 2`);
  иначе добавить запись с `count`, `type/flags/style` из ITM. `max_stack
  == 0` → add no-op (защитно; в оригинале цикл).
- Remove: уменьшить `count`; при нуле удалить запись; поведение при
  `count > available` и частичном удалении — по research.
- Money — обычная запись `MON_1_0_1` (type 18); `GetMoney`/`GetItemCount`
  видят новое значение.
- Runtime — единственный источник для UI: `InventoryOverlay` при
  следующем открытии видит изменения.
- Save и `.scr` не пишутся.

### 3.3 NPC и прочее

- NPC/EF type `0x10` контейнеры не моделируются → no-op; limitation в
  D-052/parity.
- Уведомление `FUN_140482e8` (сеть/UI) не реализуется; runtime-состояние
  обновляется напрямую.

## 4. Non-goals

- `RS_PersonTransferItemI`, `RS_PersonTransferAllItemsI`.
- Trade-варианты (`0x5000006`/`0x5000007`), `RS_PassToTradePanel`.
- NPC-лут из `.inv`.
- Запись save, сеть, торговый UI.
- Вес/цена/переполнение контейнера сверх доказанного research.

## 5. Архитектура

### 5.1 `gl::game::InventoryRuntime`

- Хранит `gl::assets::HeroInventory hero_inventory_` (копия save; live
  `items` вектор).
- `add_item(std::string_view person, std::string_view tech,
  std::int32_t count) -> bool`;
  `remove_item(...) -> bool`.
- `count`/`money` пересчитываются по live-вектору (или поддерживаются
  инкрементально).
- `const HeroInventory& hero_inventory() const` — для overlay.
- Для нового item id — `std::function<std::optional<ItemTemplate>(std::uint32_t)>
  item_template` в `create`, где `ItemTemplate { type, flags, style,
  max_stack }`; glgame передаёт lookup через `ItemCatalogEntry`/`Item`
  (`max_stack` — из ITM). Без шаблона add — no-op.
- `access()` расширяется callbacks add/remove.

### 5.2 `gl::script`

- `InventoryAccess`:
  - `std::function<void(std::string_view, std::string_view, std::int32_t)>
    add_item;`
  - `... remove_item;`
- handlers `0x5000004`/`0x5000005`: arity 3, типы `3,3,0`; return
  `double{0.0}`; неверные аргументы → warning + `0.0`.
- Registry: 2 ID → `supported` (итого 13/65).

### 5.3 `glgame`

- handlers мутируют `inventory_runtime_`; `open_inventory`/overlay
  используют `hero_inventory()` вместо статического `pending_inventory_`
  (или синхронизируют его после мутаций).
- Save не пишется.

## 6. Research deliverables (Task 1)

`docs/research/p3b2b1-inventory-mutations-server.md`:

1. `FUN_1404cd28`/`FUN_1404cae8` (`decompiled.c`) — точные правила add /
   remove: stacking, max count, partial/full remove, `count == 0`,
   negative, unknown item, item type/flags/style.
2. Container state `+0x18` и `FUN_140482e8` — что означают; что можно не
   моделировать без потери поведения.
3. Type/flags/style нового item id через ITM/ItemCatalog.
4. Реальный `.cs` с Add/Remove и наблюдаемым результатом (count/money/
   ветка); node id и ожидания для slot0/slot2.
5. Real oracle: add/remove `MON_1_0_1` и одного предмета на slot0/slot2.
6. NPC/box limitation.

## 7. Обработка ошибок

- Неизвестная/не-Hero персона → no-op.
- Неизвестный TECH/не найден item id → no-op.
- Отрицательный count → no-op (D-052; в оригинале поведение не доказано).
- Некорректный count по остальной семантике оригинала → no-op/без
  изменений.
- Неверная арность/тип → warning + `0.0`.
- Нет save/пустой инвентарь → add создаёт запись, remove — no-op/0.

## 8. Тестирование

Unit:

- add нового предмета, add существующего (stack), money add;
- remove частичный, полный, больше available, отсутствующего;
- count 0/negative, unknown TECH/person;
- `hero_inventory()` отражает изменения; `GetItemCount`/`GetMoney` видят
  мутации; `access()` callbacks.
- handlers arity/типы/return; registry 13 supported.

Real:

- slot0/slot2: add/remove `MON_1_0_1` и предмета, expected counts.
- Реальный диалог из research через `GameScriptHost` + `InventoryRuntime`.
- `glgame` overlay после мутации (если применимо) и `--click-npc 13` без
  регрессии.

Полный `ctest` с `GL_GAME_DIR`.

## 9. Критерии приёмки

1. Research содержит точные правила add/remove и real oracle.
2. 2 ID → supported (13/65).
3. Runtime мутации + overlay view покрыты unit/real тестами.
4. Real-диалог доказывает путь `ScriptVm → GameScriptHost →
   InventoryRuntime`.
5. Targeted + full `ctest` pass; числа в `STATUS`.
6. `STATUS`/`PROJECT`/`parity-matrix`/`DECISIONS` (D-052) обновлены.

## 10. Риски

- `FUN_1404cd28`/`FUN_1404cae8` могут иметь неочевидные лимиты/побочные
  эффекты; недоказанное остаётся за scope, а не угадывается.
- Type нового предмета без ITM может исказить money-логику; research
  проверяет ItemCatalog.
- UI overlay может хранить снимок; нужно явно переключить на live view.
