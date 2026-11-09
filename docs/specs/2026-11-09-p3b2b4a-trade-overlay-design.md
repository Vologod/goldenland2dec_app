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
- NPC **trade**-инвентарь — pass 1 (`hot`) `.inv`-лута (1b2b2b) в
  `entity+0xC`; `InventoryRuntime` сейчас хранит только main `+0x10` и
  отбрасывает `hot`. Hero — save; `ItemCatalog` даёт icon/name/price
  (ITM).
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
  research (Task 1); два списка (NPC trade `entity+0xC` / hero), две
  суммы денег (`npc_money`, `hero_money`), фильтры, скролл, exit.
- Данные: NPC trade items (pass 1 `hot`), Hero main items, `ItemCatalog`
  (icon/name/price), две money (count `MON_1_0_1` в NPC trade и hero main).
- Read-only: клик по предмету только выделяет (детали/подсказка), покупка/
  продажа/подтверждение отсутствуют.
- NPC- и Hero-ряды фильтруются через `gl::game::item_matches_filter`
  (inventory-группировка P5b1: type 18 всегда, биты `0x01..0x20`) для
  видимого списка, clamp скролла и hit-test — **assumption** для P3d-4
  (partial), не доказанный trade-паритет (type→bit для trade не извлечён,
  research §8.1). mask 0 = «без фильтра»; «all» (mask 0) не подсвечивается
  до явного выбора.
- Ввод: клавиатура/мышь/тач как в `InventoryOverlay`; Esc/exit закрывает
  панель и возвращает в уровень.

### 3.3 Источник данных

- Research Task 1 (done) доказал:
  - NPC-список — trade-контейнер `entity+0xC` (pass 1 NPC-лута), **не**
    main `+0x10`; server/native-level (`FUN_14028404`, `0x5000006/7`);
    клиентский setter `FUN_1206EC98` (callers=0) не трассирован —
    `unverified` (research §8.2).
  - hero-список — main `entity+0x10` (hot/equip — опционально).
  - деньги — count `MON_1_0_1` раздельно для NPC trade и hero main.
  - цена — ITM `price`, `SKILL_TRADE` (id 16), buy `2.5−0.1*skill`,
    sell reciprocal, trunc; таблицы — research §6.4.
- Если источник/раскладка недоказуемы — verdict `blocked`, код не
  пишется.

## 4. Non-goals

- Покупка/продажа, transfer/money mutations, trade confirm.
- `RS_PersonAddItemToTrade`/`RemoveItemToTrade` (`0x5000006/7`) —
  мутации (read-only чтение `entity+0xC` — в scope).
- Дополнительные скидки/перки, кроме базовой формулы `SKILL_TRADE`; save.
- Полный оригинальный tooltip (как P5b1 — фиксированная панель).

## 5. Архитектура

- `gl::app::TradeOverlay` + `TradeLayout` (константы из research).
- `gl::game::InventoryRuntime`: сохраняет pass 1 NPC (`loot.hot`) как
  trade-контейнер; accessors `trade_items(name)`, `trade_money(name)`,
  hero main items/`money()`.
- `TradeOverlay::open(GameVfs&, const ItemCatalog&, string npc_name,
  const vector<InventoryItem>& npc_items, int32 npc_money,
  const vector<InventoryItem>& hero_items, int32 hero_money)`.
- `gl::script::NativeServices::trade_panel` (`std::function<void()>`);
  handler 0x700000D.
- `glgame`: при callback — `close_dialog()` + `trade_overlay_.open(...)`
  с NPC trade items/money и hero main items/money; блокировка ввода
  уровня как у inventory overlay.
- `glscript`: пустой callback → 0.

## 6. Research (Task 1)

1. `engineres/trade/*.bmp`: размеры, позиции, слои; GUI-скрипт
   (`trade_panel`/`inv_gui.scr`/аналог).
2. Client.dll: открытие/закрытие trade panel, источники списков/цен,
   фильтры, layout (RVA); NPC trade `entity+0xC` (server/native),
   client setter invocation — `unverified`.
3. `PassToTradePanel`/`FUN_14028404`: семантика, close-флаг.
4. Реальный торговец для E2E (boat trader), ожидаемые NPC trade
   items/`npc_money`, hero items/`hero_money`, цены для slot0/slot2.
5. Verdict `implementable`/`blocked`.

## 7. Тестирование

Unit:

- `InventoryRuntime` сохраняет `loot.hot` (pass 1) и отдаёт
  `trade_items(name)`/`trade_money(name)`; RNG main не меняется.
- `TradeLayout`/hit-test/filters/scroll (синтетические списки).
- NPC-фильтр меняет видимый список, сбрасывает и зажимает scroll, а
  hit-test выбирает предмет по отфильтрованному списку; геройский фильтр
  работает; «all» не pressed при открытии.
- Overlay render snapshot (1024×768) с реальными ассетами;
  `npc_money` и `hero_money` отображаются раздельно.
- Native `PassToTradePanel`: callback вызван, return 0.0, registry 16/65.

Real:

- Реальный торговый диалог → `trade_panel` → overlay; ожидаемые NPC trade
  items/`npc_money`, hero items/`hero_money` из research (slot0/slot2;
  счётчики/money + первые 12 hero main).
- `glgame --level l1_1 --click-npc 13` без регрессии.
- Full `ctest`.

## 8. Критерии приёмки

1. Research: layout/assets/source/prices + real trader oracle
   (NPC trade `entity+0xC` pass 1; два money).
2. `0x700000D` supported (16/65); overlay открывается по нативу.
3. Overlay показывает NPC trade items/Hero main items, `npc_money` и
   `hero_money`, фильтры; read-only.
4. Unit + real + screenshot tests; full `ctest`.
5. D-057/parity/STATUS/PROJECT; UTF-8 без BOM/NUL.

## 9. Риски

- Trade panel может быть клиентским GUI-скриптом, а не простыми BMP;
  research gate.
- NPC trade-контейнер `+0xC` **является** источником (server/native
  proven); 1b2b2b pass 1 (`hot`) сейчас не сохраняется — Task 2 обязан
  его сохранять, иначе NPC-список пуст.
- Цены/скидки зависят от `SKILL_TRADE`; read-only показывает базовую
  цену, отклонения фиксируются.
