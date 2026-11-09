# Phase 1b2b4a — read-only trade overlay (design)

Дата: 2026-11-09. Статус: approved (пользователь, 2026-11-09).

## 1. Цель

Открыть торговую панель по `RS_PassToTradePanel` и показать read-only
списки NPC ↔ игрок, деньги, фильтры/скролл. Покупка/продажа — отдельная
фаза (1b2b4b).

## 2. Контекст

- `Server.dll`/`Client.dll` v1.3; SHA-256 зафиксированы в research.
- `RS_PassToTradePanel` (`0x700000D`, `117440525`) вызывает
  `FUN_14028404`, ставит `DAT_14088de8+0x5528=1` (закрыть диалог),
  возвращает `0.0`. В порте — `unsupported`.
- `engineres/trade/*.bmp` — полный набор ассетов панели
  (`inv_left`, `inv_right`, `filter1..7`, `afilter1..7`, `left*`,
  `lbig*`, `lm*`, `exit`, `drop`, …).
- NPC main-инвентарь — `InventoryRuntime` (1b2b2b); Hero — save;
  `ItemCatalog` даёт icon/name/price (ITM).
- `PassToTradePanel` в поставляемых `.cs` вызывается 129 раз; trade
  Add/Remove (`0x5000006/7`) — 0 раз.

## 3. Scope

### 3.1 Native

- `0x700000D` → `supported` (16/65).
- `NativeServices` получает `trade_panel` callback; handler вызывает его,
  возвращает `double{0.0}`; пустой callback → лог + `0.0`.
- `GameScriptHost`/`glgame`: callback закрывает текущий диалог и
  открывает `TradeOverlay` с именем NPC (speaker).

### 3.2 Overlay

- `gl::app::TradeOverlay`: letterbox 1024×768; layout/ассеты — из
  research (Task 1); два списка (NPC/игрок), деньги, фильтры, скролл,
  exit.
- Данные: NPC items (main), Hero items, `ItemCatalog` (icon/name/price),
  money (Hero).
- Read-only: клик по предмету только выделяет (детали/подсказка), покупка/
  продажа/подтверждение отсутствуют.
- Ввод: клавиатура/мышь/тач как в `InventoryOverlay`; Esc/exit закрывает
  панель и возвращает в уровень.

### 3.3 Источник данных

- Research Task 1 обязан доказать: клиент читает NPC main `entity+0x10`
  или trade `entity+0xC`; как формируются цены (ITM `price`, buy/sell,
  `SKILL_TRADE`); какие фильтры/категории.
- Если источник/раскладка недоказуемы — verdict `blocked`, код не
  пишется.

## 4. Non-goals

- Покупка/продажа, transfer/money mutations, trade confirm.
- `RS_PersonAddItemToTrade`/`RemoveItemToTrade` (`0x5000006/7`),
  trade-контейнер `+0xC`.
- Скидки/навык торговли, save.
- Полный оригинальный tooltip (как P5b1 — фиксированная панель).

## 5. Архитектура

- `gl::app::TradeOverlay` + `TradeLayout` (константы из research).
- `gl::game::InventoryRuntime`: accessor списка items персоны
  (`person_items(name)`) для overlay.
- `gl::script::NativeServices::trade_panel` (`std::function<void()>`);
  handler 0x700000D.
- `glgame`: при callback — `close_dialog()` + `trade_overlay_.open(...)`;
  блокировка ввода уровня как у inventory overlay.
- `glscript`: пустой callback → 0.

## 6. Research (Task 1)

1. `engineres/trade/*.bmp`: размеры, позиции, слои; GUI-скрипт
   (`trade_panel`/`inv_gui.scr`/аналог).
2. Client.dll: открытие/закрытие trade panel, источники списков/цен,
   фильтры, layout (RVA).
3. `PassToTradePanel`/`FUN_14028404`: семантика, close-флаг.
4. Реальный торговец для E2E (boat trader/weapon trader), ожидаемый
   список/цены/деньги для slot0/slot2.
5. Verdict `implementable`/`blocked`.

## 7. Тестирование

Unit:

- `TradeLayout`/hit-test/filters/scroll (синтетические списки).
- Overlay render snapshot (1024×768) с реальными ассетами.
- Native `PassToTradePanel`: callback вызван, return 0.0, registry 16/65.

Real:

- Реальный торговый диалог → `trade_panel` → overlay; ожидаемые NPC/игрок
  items/деньги из research.
- `glgame --level l1_1 --click-npc 13` без регрессии.
- Full `ctest`.

## 8. Критерии приёмки

1. Research: layout/assets/source/prices + real trader oracle.
2. `0x700000D` supported (16/65); overlay открывается по нативу.
3. Overlay показывает NPC/Hero items, деньги, фильтры; read-only.
4. Unit + real + screenshot tests; full `ctest`.
5. D-057/parity/STATUS/PROJECT; UTF-8 без BOM/NUL.

## 9. Риски

- Trade panel может быть клиентским GUI-скриптом, а не простыми BMP;
  research gate.
- NPC trade-контейнер `+0xC` может быть источником, а не main; если так
  и он не генерируется — overlay может быть пустым/blocked.
- Цены/скидки зависят от `SKILL_TRADE`; read-only показывает базовую
  цену, отклонения фиксируются.
