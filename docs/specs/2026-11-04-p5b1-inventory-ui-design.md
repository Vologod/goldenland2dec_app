# P5b1 — Read-only inventory UI (design)

Дата: 2026-11-04. Статус: design approved; research gate перед планом.

Источники: `Client.dll` SHA-256
`FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`;
`docs/research/p5-save-equipment.md`; `docs/research/formats.md` (ITM/SDB);
ресурсы `scripts/ui/inv_gui.scr`, `engineres/newinv/*`,
`engineres/interface/bag*.csx`.

## 1. Цель

Показать read-only инвентарь героя из выбранного save в оригинальном
интерфейсе: три контейнера `BT_HERO`, сетка/скролл/фильтры, выбор
предмета и правая панель с именем/описанием/характеристиками.

## 2. Scope

- Три контейнера `BT_HERO`: 0 — расходники/рецепты (нижняя панель),
  1 — основной инвентарь (сетка), 2 — экипировка (puppet слева).
- Оригинальный визуал: `engineres/newinv/inventory_secondary.bmp`,
  `inventory_skills.bmp`, `filter1..7`, `drop`, `exit`, `inv_left/right`,
  `weapon_slot1`, `cell`.
- Логический холст 1024×768, letterbox 4:3; touch/мышь в логических
  координатах.
- Сетка + пагинация/скролл + фильтры 1..7 + выбор предмета; правая
  панель: имя (`lit_names.sdb`), описание (`descriptions.sdb`), базовые
  ITM-характеристики (тип, цена, вес; damage/armor — если есть).
- Открытие: `I` desktop / inventory button touch; закрытие:
  `exit`/Esc/Android back. Skills-вкладка видна, но disabled до P5c.
- Источник данных — выбранный save-слот (P5a); при отсутствии —
  пустые контейнеры.

## 3. Non-goals

- use/drop/equip/перемещение предметов, изменение save.
- Skills UI/параметры (P5c), магия, торговля (P6), сундуки/NPC.
- Полный world load из `.sav` (P2c).
- Android-полировка/тач-раскладка вне letterbox-масштабирования (P1b).

## 4. Архитектура

```
selected save
  → gl::assets::parse_hero_inventory(body)  // 3 контейнера
  → gl::assets::ItemCatalog (lazy ITM + lit_names/descriptions + icon)
  → gl::game::InventoryModel / InventoryView (filter/page/selection)
  → gl::app::InventoryOverlay (SDL, logical 1024×768)
  → glgame input/hotkey
```

Слои вниз: `assets` без SDL; `game` без SDL; SDL только `app`/`tools`.
P5a `HeroEquipment`/outfit сохраняется; экипировка берётся из контейнера 2.

## 5. Данные и API

### 5.1 `gl::assets`

```cpp
struct InventoryItem {
    std::uint32_t item_id = 0;
    std::uint32_t count = 1;
    std::uint32_t flags = 0;
    std::uint32_t type = 0;
    std::uint32_t style = 0;
};
struct HeroInventory {
    std::vector<InventoryItem> consumables; // container 0
    std::vector<InventoryItem> items;       // container 1
    std::vector<InventoryItem> equipped;    // container 2
};
Result<HeroInventory> parse_hero_inventory(std::span<const std::uint8_t> body);
```

Parser использует тот же layout, что P5a (`0x734`, 3 контейнера,
`0xC0 + 4 + subcount*0x14`); `count` из record `+0xB0` (в equipped = 1).

```cpp
struct ItemInfo {
    std::uint32_t item_id = 0;
    std::string name;        // lit_names.sdb[description_id]
    std::string description; // descriptions.sdb[description_id]
    std::string icon_path;   // ITM icon_name
    std::uint32_t type = 0;
    std::uint32_t price = 0;
    std::uint32_t weight_x10 = 0;
    bool has_damage = false;
    std::uint32_t damage = 0;
};
class ItemCatalog {
  public:
    static Result<ItemCatalog> load(const GameVfs&);
    const ItemInfo* find(std::uint32_t item_id) const;
};
```

`load` читает `items/data/<id>.itm` лениво/кэширует; SDB
`lit_names`/`descriptions` и ITM `description_id` дают тексты; иконка —
`icon_name` BMP из `items.pak`; при ошибке — fallback `bag.csx`.

### 5.2 `gl::game`

```cpp
struct InventoryViewState {
    std::size_t page = 0;
    std::uint32_t filter_mask = 0; // фильтры 1..7, mapping — research
    std::optional<std::size_t> selected;
};
std::vector<const InventoryItem*> visible_items(const HeroInventory&,
                                                InventoryViewState&);
```

View-логика (фильтр, страницы, выбор) без SDL, тестируемая.

### 5.3 `gl::app`

`InventoryOverlay`:
- `open/close/toggle`, `render(renderer, viewport)`, `handle_mouse/touch/key`.
- хранит `ItemCatalog`, `HeroInventory`, `InventoryViewState`, текстуры
  фона/иконок; логические координаты; letterbox-масштаб.
- `glgame` хранит выбранный save (`HeroInventory`) рядом с P5a outfit и
  передаёт overlay.

## 6. UI

- Холст 1024×768; фон `inventory_secondary`/`inventory_skills`;
  letterbox по центру, hit-test с обратным масштабом.
- Координаты объектов — из `scripts/ui/inv_gui.scr`: puppet-контейнер
  `(8,296,270×320)`; нижняя панель `(138,696,730×66)`; scroll
  `(110,702,27×55)` и `(870,703,26×54)`; exit `(920,694,75×68)`;
  drop `(19,690,81×75)`; фильтры `y=672/673`, ширина 56, высота 23;
  skills-вкладка `(280,4,368×40)`, inventory-вкладка `(654,4,360×40)`.
- Сетка/слоты: координаты брать из оригинального UI-кода/фона
  `inventory_secondary` (research gate), не подбирать на глаз.
- Правая панель: имя, описание, тип/цена/вес, damage/armor при наличии;
  перенос строк по ширине панели.
- Пагинация: scroll-кнопки и drag/колесо; фильтры toggle; skills
  disabled.
- Пустой контейнер/нет save — пустые слоты и текст «(пусто)».

## 7. Input и Android

- Desktop: `I` toggle, Esc/exit close, mouse click/scroll; фильтры click.
- Touch: tap = click; drag по сетке = scroll; on-screen inventory button
  (заглушка до P1b); Android back = close.
- Hit-тесты по логическим rect; минимальный touch-таргет не уменьшать
  ниже оригинального (letterbox только масштабирует).
- Никакого hover-зависимого поведения: выбор сразу показывает панель.

## 8. Research gate (до реализации)

1. Точные rects слотов сетки/страницы и соответствие контейнеров
   (`consumables/items/equipped`) панелям UI из `inv_gui.scr` + Client.dll.
2. Семантика фильтров 1..7 (какие item type/флаги включает каждый).
3. Формат/пути иконок ITM (`icon_name`) и fallback для отсутствующих.
4. Тексты `lit_names`/`descriptions` по `description_id`; поведение для
   пустых/неизвестных id.
5. Записать offsets/observations в `docs/research/p5b1-inventory-ui.md`.

## 9. Ошибки

- Save/parser error → пустой inventory + warning; overlay открывается.
- ITM/SDB/icon missing → item отображается с fallback-иконкой и
  техническим именем `item <id>`; остальные работают.
- Некорректный view state (page/filter/selection) → clamp/сброс.

## 10. Тесты и acceptance

- Unit: parser 3 контейнеров (0/N slot0/slot2 counts 8/117/11 и
  9/140/11); item name/description lookup; icon fallback; filter/page/
  selection state.
- App: overlay screenshot 1024×768 и letterbox 16:9/portrait; touch
  hit-test; open/close; фильтры/страницы.
- Real: slot0/slot2 inventory items resolve to ITM/SDB; missing assets
  fallback.
- Manual gate: сравнить overlay с `inventory_secondary`/`inventory_skills`
  и оригинальным UI; пользователь подтверждает.
- Полный `ctest` с `GL_GAME_DIR`.

## 11. Риски

- Точная геометрия сетки/фильтров может потребовать дизассемблера
  Client.dll; research gate обязателен.
- Оригинальные иконки могут быть BMP с палитрой/прозрачностью; нужен
  decode/fallback.
- На маленьких экранах текст правой панели требует переноса/скролла;
  в P5b1 допускается вертикальный скролл панели.
