# Phase 1b2b3 — inventory transfers (design)

Дата: 2026-11-08. Статус: approved (пользователь, 2026-11-08).

## 1. Цель

Реализовать перенос предметов между персонами:

- `RS_PersonTransferItemI` (`0x5000001`);
- `RS_PersonTransferAllItemsI` (`0x5000003`).

Источник — `InventoryRuntime` (Hero + NPC-контейнеры после 1b2b2b).

## 2. Контекст

- `Server.dll` v1.3, SHA-256
  `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Native-таблица: `docs/research/p3-vm-natives-server.md` §5.
- Реальные `.cs`: `TransferItemI` 284, `TransferAllItemsI` — 0 (в поставляемых
  скриптах не вызывается; реализуется для полноты и будущего trade).
- `FUN_14048790` (`decompiled.c:49329`): перенос до `count` единиц
  TECH-предмета из контейнера A в B; `count<0` → 1 000 000; partial;
  отсутствующий предмет → `0`; merge в B; `param_2==param_3` → `1`.
- `FUN_14048518`: перенос всех записей A→B (merge).
- `FUN_1404c690`: TECH name → instance uid → item id; `+0xA8` id,
  `+0xB0` count.
- `InventoryRuntime` (1b2b2b): Hero + NPC `main` контейнеры; `add/remove`
  Hero-only.

## 3. Scope

### 3.1 Нативы

| ID | Имя | arity | Возврат |
|---|---|---|---|
| `0x5000001` | `RS_PersonTransferItemI` | 4 (`3,3,3,0`) | `1.0`/`0.0` |
| `0x5000003` | `RS_PersonTransferAllItemsI` | 2 (`3,3`) | `1.0`/`0.0` |

Registry: 2 ID → `supported` (15/65).

### 3.2 Семантика

- Person lookup — точное имя; Hero и NPC.
- TECH — exact case-sensitive; неизвестный: `count > 0` → `0.0`;
  `count == 0` / `count < 0` → `1.0` (absent-item quirk, без изменений).
- TransferItem:
  - `count == 0` → успех без изменений (`1.0`);
  - `count < 0` → до 1 000 000 (все);
  - source без предмета (matrix из `FUN_14048790`): `count > 0` →
    `0.0`; `count == 0` → `1.0`; `count < 0` → `1.0` (quirk:
    `remaining = 1 000 000 != count`, изменений нет);
  - перенос до `count` единиц; partial при нехватке; origin пустые
    записи удаляются; в dest merge по item_id (count += moved); новая
    запись создаётся из **ITM-шаблона** (`type/flags/style/max_stack`),
    а не копирует source-запись; при merge в существующую запись
    сохраняются её flags;
  - same person → `1.0` без изменений.
- TransferAll:
  - перенос всех записей source → dest с merge; source очищается;
  - пустой source → `1.0`.
- Save не пишется; notify/container-state не моделируются.
- Trade-контейнер (`entity+0xC`) и `0x5000006/7` — вне scope.

## 4. Non-goals

- Trade UI/`RS_PassToTradePanel`, trade-контейнеры.
- NPC add/remove (остаются no-op; transfer мутирует контейнеры напрямую).
- Save, сеть, restock.

## 5. Архитектура

### 5.1 `gl::game::InventoryRuntime`

```cpp
bool transfer_item(std::string_view from, std::string_view to,
                   std::string_view tech_name, std::int32_t count);
bool transfer_all(std::string_view from, std::string_view to);
```

- Работает с контейнерами `from`/`to` (Hero/NPC); новые dest-записи
  берут `type/flags/style/max_stack` из ITM-шаблона (как
  `FUN_1404cd28`); source runtime-поля не копируются. Merge в
  существующую dest-запись сохраняет flags этой записи.
- Merge в dest по item_id; порядок записей — append (как в 1b2b1).
- Отсутствующая персона/контейнер: `false`/no-op; в оригинале
  `TransferItem` missing person/container и missing container в обоих
  хелперах дают `0`; AV только `TransferAll` при missing person (null
  `entity+0x10`) — deviation, D-056. Both
  containers резолвятся до short-circuit `from == to`, поэтому
  одинаковые имена отсутствующей персоны → `false`.
- Порт-guard: `transfer_all` при source-записи без ITM template
  возвращает `false` без мутаций (оригинал add-ошибку не проверяет и
  продолжил бы) — deviation, D-056.
- Порт-guard: `max_stack == 0` — `false`/no-op (в оригинале возможен
  нетерминирующий цикл создания, research §9.7) — deviation, D-056.
- `access()` расширяется callbacks (`transfer_item`, `transfer_all`).

### 5.2 `gl::script`

- `InventoryAccess`:
  - `std::function<bool(std::string_view, std::string_view,
    std::string_view, std::int32_t)> transfer_item;`
  - `std::function<bool(std::string_view, std::string_view)> transfer_all;`
- handlers: валидация arity/типов; return `double{1.0}`/`0.0`;
  пустой callback → `0.0`.

### 5.3 `glgame`

- `access()` уже подключается; новые callbacks заработают без отдельной
  проводки (проверить).
- Реальный диалог/сценарий из research.

## 6. Research (Task 1)

1. Точная семантика `FUN_14048790`/`FUN_14048518`: partial/over/absent/
   negative/zero/merge/порядок/RVA.
2. Real oracle: slot0/slot2 Hero↔NPC transfers (counts до/после) +
   `.inv`-сгенерированный NPC; expected values.
3. Реальный `.cs` с transfer и наблюдаемым результатом (node ids).
4. Trade-контейнер — подтвердить вне scope.

## 7. Обработка ошибок

- Неизвестная/отсутствующая персона, TECH, item id → `0.0`/no-op.
- Некорректные аргументы → warning + `0.0`.
- Пустой callback → `0.0`.

## 8. Тестирование

Unit:

- TransferItem: partial/full/over/absent(+/-/0)/zero/negative/same
  person; merge; NPC↔Hero; Hero↔NPC; новая dest-запись берёт
  type/flags/style из ITM-шаблона; merge сохраняет flags dest;
  удаление опустевших source-записей.
- TransferAll: все/пустой source/merge.
- handlers: arity/типы/return; registry 15/65.

Real:

- slot0/slot2 Hero↔NPC oracle counts; каждая строка oracle — свежий
  `InventoryRuntime` в базовом состоянии, строки не накапливаются.
- Реальный `.cs` (если найден) через production path.
- `glgame --level l1_1 --click-npc 13 --screenshot` без регрессии.
- Full `ctest`.

## 9. Критерии приёмки

1. Research семантика + real oracle + (если есть) real dialog.
2. 2 ID → supported (15/65).
3. Unit + real tests; full `ctest`.
4. D-056/parity/STATUS/PROJECT обновлены.
5. Docs UTF-8 без BOM/NUL.

## 10. Риски

- Merge/порядок в dest может отличаться от оригинала (как в 1b2b1);
  фиксируется research/D-056.
- NPC-контейнеры генерируются без specials (RNG-desync) — transfer
  работает на том, что есть.
- Real dialog с `TransferAll` может отсутствовать; тогда unit/oracle.
