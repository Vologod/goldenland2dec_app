# Phase 1b2b4b — trade transactions (design)

Дата: 2026-11-09. Статус: approved (пользователь, 2026-11-09).

## 1. Цель

Сделать trade overlay интерактивным: покупка/продажа предметов по цене
оригинала с обновлением денег/стоков и подтверждением.

## 2. Контекст

- `Server.dll`/`Client.dll` v1.3; read-only overlay 1b2b4a уже открывается
  по `RS_PassToTradePanel`; NPC trade-сток = pass-1 `entity+0xC`
  (`loot.hot`), Hero main = `entity+0x10`; два money (NPC/Hero).
- Цены: research `p3b2b4a-trade-panel.md` §6: ITM `price`,
  durability-факторы `0.0025`/`0.75`, skill `-0.1`/`2.5`, truncation,
  buy/sell reciprocal, min 1, count multiply.
- Деньги — обычные `MON_1_0_1` (type 18) в контейнерах.
- `RS_PersonAddItemToTrade`/`RemoveItemToTrade` (`0x5000006/7`) в
  поставляемых `.cs` не вызываются; trade-сток NPC и так `+0xC`.

## 3. Scope

### 3.1 Транзакции (staging + Exchange)

Оригинал: предметы (весь стек) перетаскиваются в buy/sell staging-ряды,
затем **Exchange** (obj 44, `FUN_1206DBB0`) выполняет обмен и переводит
**чистую разницу** денег; отдельного подтверждения нет.

- `InventoryRuntime::exchange_trade(npc, hero, buy_items, sell_items)`:
  - `buy_items` — записи NPC trade (`+0xC`), `sell_items` — Hero main;
  - `buy_total`/`sell_total` по ценам (Task 1/2);
  - если `buy <= sell`: NPC money ≥ `sell-buy`, delta NPC→Hero;
  - иначе: Hero money + sell ≥ buy, delta Hero→NPC (отрицательный);
  - перенести buy-items NPC trade → Hero main, sell-items Hero main →
    NPC trade (весь стек, merge), затем net money;
  - ошибка (нет денег/предмета/цены/модификатора) → без мутаций.
- `InventoryRuntime::buy_trade(npc, hero, tech, price)` — **без
  `count`**: примитив, переносит весь стек NPC trade → Hero main и
  money Hero→NPC (для staged/тестов).
- `InventoryRuntime::sell_trade(...)`: весь стек Hero main → NPC trade;
  money NPC → Hero по sell-цене.
- **Modifier guard:** если `mod_count != 0 && (flags&0x80)==0` —
  `unsupported` для buy и sell (точная цена невозможна, research §8.2-
  8.3). `gv_addon`/perks (`0x10/0x400`) снять до паритета; цена — только
  при x87 PC=53 (research §4.3/§4.5).
- **Количество:** общего quantity selector нет; whole-stack per action.
  Условный single-item путь (`count>1` + query `10|0xB`, research §2.2) —
  known case, не реализуется без подтверждения.
- Инварианты: суммарное количество предмета/денег сохраняется между
  контейнерами; merge по item_id; source deletion; ошибки не мутируют.
- Runtime-only: save не пишется.

### 3.2 Цены

- `TradePrice { buy, sell }` из ITM/durability/skill по research;
  min 1; count multiply; truncation/rounding — строго по research
  (PC=53/double; `fstp qword` округляет store, `fistp` truncates).
- Конкретная формула и входы (durability, skill TRADE, NPC/hero)
  фиксируются Task 1; неподтверждённое остаётся `unsupported`/blocked.

### 3.3 UI

- Выбор NPC/Hero предмета; цена (buy/sell) в панели; кнопки Buy/Sell
  (whole selected stack) или Exchange-подтверждение (по research);
  обновление списков/денег после транзакции; ошибки (нет денег/стока) —
  сообщение/лог, без мутации.
- Количество: одна операция — **весь выбранный стек**; отдельного
  quantity-выбора нет. Условный single-item путь (`count>1` + query
  `10|0xB`, research §2.2) — known case, вне scope до подтверждения.
- Цена: buy/sell только при PC=53 и `modn==0`; иначе «—»/`unsupported`
  (modifier guard §3.1).
- Read-only фильтры/скролл/exit сохраняются.

## 4. Non-goals

- Quantity slider/selector/negotiation/bargaining, trade-навык UI
  (одна операция — весь выбранный стек).
- `RS_PersonAddItemToTrade`/`RemoveItemToTrade` natives.
- Save, сеть, specials NPC-лута.
- Полный оригинальный tooltip/анимации.

## 5. Архитектура

- `gl::game::TradePrice`/price helpers в `gl_game` (ITM/durability/skill
  injection).
- `InventoryRuntime` transaction methods (buy/sell) + errors
  (`Result<...>` или enum).
- `TradeOverlay` получает callbacks `buy`/`sell`/`price`; UI-only,
  без прямой мутации.
- `glgame` связывает callbacks с runtime; обновляет overlay snapshot
  после действия; input blocking.

## 6. Research (Task 1)

1. Buy/sell функции Client/Server: RVA, порядок transfer/money, count,
   confirm, ошибки, rounding.
2. Точная price-формула и входы (durability/skill/NPC), min 1, buy/sell
   reciprocal, count.
3. Деньги NPC/Hero: какой контейнер/item, как меняется при сделке.
4. Реальный торговец: цены/стоки/деньги slot0/slot2, buy/sell oracle.
5. Verdict `implementable`/`blocked`.

## 7. Тестирование

Unit:

- buy/sell success (весь выбранный стек); insufficient money/stock;
  merge; price min 1; no mutation on error; invariants.
- Price formula per research (durability/skill cases) под PC=53/double;
  modifier guard (buy и sell) → `unsupported`.

Real:

- Boat trader slot0/slot2: buy/sell E2E через production path;
  ожидаемые деньги/стоки/цены; screenshot.
- `l1_1 --click-npc 13` regression.
- Full `ctest`.

## 8. Критерии приёмки

1. Research: transaction/price semantics + real oracle.
2. Buy/sell работают в overlay; деньги/стоки обновляются; ошибки
   безопасны.
3. Unit + real + screenshots; full `ctest`.
4. D-058/parity/STATUS/PROJECT; UTF-8 без BOM/NUL.
5. Нет save-записи; specials/trade natives вне scope.

## 9. Риски

- Оригинальный trade UI может использовать staged-контейнер `+0x4D4`/
  `+0xC` и подтверждение; неточность → blocked.
- Цена зависит от durability/skill/NPC и x87 PC; неподтверждённые входы
  нельзя угадывать. Точная цена — только при PC=53/double; иначе
  `unsupported`.
- Modifier-предметы (`mod_count != 0`, `(flags&0x80)==0`) — `unsupported`
  для buy и sell; NPC-stock без модификаторов доказан только slot0/slot2.
- Покупка может требовать свободного слота/лимита; лимит слотов
  приёмного контейнера — inherited proof (p3b2b3); research gate.
