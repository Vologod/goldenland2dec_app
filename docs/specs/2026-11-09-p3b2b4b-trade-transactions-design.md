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

### 3.1 Транзакции

- `InventoryRuntime::buy_trade(npc, hero, tech, count, price)`:
  проверить Hero money ≥ price*count и NPC stock; перенести предмет
  NPC trade → Hero main; перевести money Hero → NPC (или уменьшить
  Hero money; точная форма — research); вернуть результат/ошибку.
- `InventoryRuntime::sell_trade(...)`: Hero main → NPC trade; money
  NPC → Hero по sell-цене.
- Инварианты: суммарное количество предмета/денег сохраняется между
  контейнерами; merge по item_id; source deletion; ошибки не мутируют.
- Runtime-only: save не пишется.

### 3.2 Цены

- `TradePrice { buy, sell }` из ITM/durability/skill по research;
  min 1; count multiply; truncation/rounding — строго по research.
- Конкретная формула и входы (durability, skill TRADE, NPC/hero)
  фиксируются Task 1; неподтверждённое остаётся `unsupported`/blocked.

### 3.3 UI

- Выбор NPC/Hero предмета; цена (buy/sell) в панели; кнопки Buy/Sell
  (или подтверждение — по research); обновление списков/денег после
  транзакции; ошибки (нет денег/стока) — сообщение/лог, без мутации.
- Количество: по умолчанию 1; quantity-выбор только если research
  докажет оригинальный механизм, иначе вне scope.
- Read-only фильтры/скролл/exit сохраняются.

## 4. Non-goals

- Quantity slider/negotiation/bargaining, trade-навык UI.
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

- buy/sell success; insufficient money/stock; merge; price min 1;
  count; no mutation on error; invariants.
- Price formula per research (durability/skill cases).

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
- Цена зависит от durability/skill/NPC; неподтверждённые входы нельзя
  угадывать.
- Покупка может требовать свободного слота/лимита; research gate.
