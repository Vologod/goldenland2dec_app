# P5e — Potions/food use (right-click) — дизайн

Дата: 2026-11-27. Статус: proposed. Решение: **D-116**.
Research: `docs/research/p5e-potions-food.md`.

## 1. Цель

Продюсер эффектов «зелья/еда»: по right-click на слоте инвентаря
применить предмет к герою ровно как `Server.dll FUN_14057cf0`
(RVA `0x57cf0`): прямые base-статы, timed-эффекты в P5-канал, лечение
HP/EP, расход 1 шт.; изменения переживают save/load.

## 2. Scope / Non-goals

**Scope:** типы 19 (potion) и 20 (food); hero self-use; прямое изменение
9 статов; append эффектов; лечение; расход из конкретного слота
(bottom/hot); запись стат-блока в BT_HERO; right-click триггер.

**Non-goals:** книги (21), свитки (22), рецепты (26); использование на
другом члене партии (left-click `0x43` target-select); NPC-использование;
`scripts/useitem/*.scr`; клиентская 2-секундная очередь (`gv_item_using_timeout`)
— порт применяет сразу (D-116); UI-анимации/звук.

## 3. Правило (parity)

Для `item.type ∈ {potion, food}` и валидного hero:

1. Для каждого `item.effects` (порядок файла):
   - если `(flags & 0x41) == 0x41` и `type` в `{6,7,8,9,10,11,12,28}` —
     **прямое** изменение base-стата:
     `6 str, 7 con, 8 per, 9 dex, 10 int, 11 wis, 12 luck`, `28 reputation`;
     атрибуты clamp `1..30` (`ch_max_primary_value` default 30);
     reputation clamp `-50..50` (порт; оригинал — границы менеджера).
   - иначе — `PersonEffect{type, value, flags, word3, -1}`, где
     `word3 = duration.hi | duration.lo<<8 | delay.hi<<16 | delay.lo<<24`;
     добавить в `PersonRuntime` героя (P5 aging/фильтр применяются как для
     любых эффектов).
   `value` — signed int32 (`0xFFFFFFFF` = −1).
2. Лечение: `HP = min(max_life, current_life + healing_health)`,
   `EP = min(max_energy, current_energy + healing_mana)`; применяется даже
   если heal = 0 (запись текущего clamp-значения).
3. Расход: из выбранного слота `count -= 1`; при `count == 0` запись
   удаляется. Применяется после шагов 1–2.
4. Неверный тип/пустой слот/нет каталога — no-op (без расхода).

## 4. Компоненты

### 4.1 `gl/game/item_use.hpp` + `src/game/src/item_use.cpp` (SDL-free)

```cpp
enum class HeroItemContainer { main, hot };

struct HeroUseResult {
    bool used = false;
    bool consumed = false;
    int effects_added = 0;
};

bool is_consumable(gl::assets::ItemType type);

HeroUseResult use_hero_item(const gl::assets::Item &item, HeroItemContainer container,
                            std::size_t index, gl::assets::HeroStats &stats,
                            PersonRuntime &runtime, InventoryRuntime &inventory);
```

- `use_hero_item` проверяет тип, индекс, затем применяет правило §3;
  прямые статы пишет в `stats` и `runtime.set_raw_parameter("Hero", ...)`;
  лечение — в `runtime` и `stats.life/energy`; эффекты — `runtime.add_effect`;
  расход — `inventory.consume_hero_item`.
- Хелпер `classify_effect` (direct id или нет) — отдельная функция для тестов.

### 4.2 `PersonRuntime`

- `void add_effect(std::string_view person, const PersonEffect &effect)`.
- `void set_raw_parameter(std::string_view person, PersonParameter, double)` —
  пишет значение без RS-специфики (reputation light/dark, clamp): нужен для
  прямых эликсиров и уже clamp-нутых HP/EP.

### 4.3 `InventoryRuntime`

- `bool consume_hero_item(HeroItemContainer container, std::size_t index)`:
  уменьшает `count`, при 0 удаляет запись из `hero_inventory_.items`/`.hot`.
- glgame после успеха: `pending_inventory_ = inventory_runtime_.hero_inventory();`
  и переоткрывает `InventoryOverlay` (чтобы индексы/иконки обновились).

### 4.4 UI-триггер (right-click)

- `InventoryOverlay::use_request_at(int x, int y) const` →
  `std::optional<InventoryUseRequest>` c `{HeroItemContainer, std::size_t index}`:
  - `bottom_slot` → `main`, `visible_indices_[page+slot]`;
  - `hot_slot` → `hot`, найденный по маске слота (как в `handle_mouse`);
  - прочие регионы → `nullopt`.
- glgame: в `SDL_MOUSEBUTTONDOWN` при `button == SDL_BUTTON_RIGHT` и
  видимом инвентаре — `use_request_at(x,y)`; если есть — `use_hero_item`
  с `ItemCatalog::find(item_id)`. Левый клик не меняется.
- Триггер соответствует оригиналу: right edge (`+0x70`) → код `0x4b`.

### 4.5 Save (BT_HERO stat block)

- `HeroLiveState` + `std::optional<gl::assets::HeroStats> stats`.
- `make_hero_save_body`: при наличии `stats` пишет в `body.stat_block`:
  - `+0x00..0x20` — 9 статов (str, con, dex, per, int, wis, luck, rep, exp);
  - `+0x2C + 4*id` — 27 навыков;
  остальные байты блока сохраняются.
- glgame при записи сейва: `hero.stats = pending_hero_stats_`.
- Timed-эффекты уже пишутся `extra` (P5); HP/EP — существующие `hp/ep`.

## 5. Данные/поток

`right-click` → overlay request → `ItemCatalog::find(id).item` →
`use_hero_item`:
`HeroStats` (прямые статы) + `PersonRuntime` (raw значения, эффекты,
HP/EP) + `InventoryRuntime` (расход) → glgame синхронизирует
`pending_hero_stats_`/`pending_inventory_`, переоткрывает инвентарь,
пишет лог. При save — stat block из `pending_hero_stats_`, эффекты/HP/EP
из runtime.

## 6. Ошибки и края

- Тип не 19/20 → `used=false`, без изменений/расхода.
- Индекс вне контейнера/нет записи → no-op.
- Нет ITM в каталоге → no-op + warning (нельзя применить правило).
- Повторное использование при полном HP/EP: эффекты/расход всё равно
  применяются (как оригинал; лечение clamp).
- Атрибуты: значение может уйти ниже 1/выше 30 → clamp 1..30;
  reputation −50..50.
- Стек: расход ровно из выбранного слота; удаление при 0.
- Эффект с задержкой (`0x20`) попадает в список и не действует до
  истечения задержки (P5-фильтр).

## 7. Тестирование

**Unit (`tests/game/test_item_use.cpp`):**

- `is_consumable` для 19/20 и отрицание для оружия/книги.
- Timed appendix: synthetic effect `{30,100,0x50,dur 3/0}` → запись
  `word3=0x00000003`, `word4=-1`; `active_effect_mods` учитывает.
- Direct stats: `{6,+1},{10,−1}` (как `948.itm`) → str+1/int−1;
  clamp на 30 и 1; reputation `{28,−1}` clamp −50.
- Лечение: current 10, max 100, heal 100 → 100; HP+EP с `957`.
- Расход: count 3→2; count 1→удаление; неверный тип не трогает.
- Комбинация direct + timed + heal.

**Unit app (`tests/app/test_inventory_overlay.cpp`):**

- `use_request_at` для bottom/hot возвращает контейнер+индекс; для
  puppet/filter/пустого — `nullopt`.

**Unit save (`tests/game/test_hero_save_write.cpp`):**

- `make_hero_save_body` c `HeroStats` пишет 9 статов и навыки; байты вне
  известных полей сохраняются.

**Real (`tests/game/test_item_use_real.cpp`, `GL_GAME_DIR`):**

- `items/data/953.itm` (HP+100): slot0 hero имеет 953 count 51;
  use → count 50, HP `min(max, cur+100)`.
- `items/data/948.itm` (`{6,+1},{10,−1}`): synthetic HeroStats 10/10 →
  str 11, int 9.
- `items/data/408.itm` (food, timed id 30): эффект попадает в runtime
  с `word3=0x00000003`.

**Интеграция:** `cmake --build build`; focused
`gl_game_tests --test-case="*item use*,*inventory overlay*,*hero save*"`;
`glgame_screenshot_tests --test-case="*inventory*"`; полный `ctest`
с `GL_GAME_DIR`.

## 8. Критерии приёмки

1. Right-click по зелью/еде в инвентаре применяет правило §3; расход
   ровно 1 шт. из слота.
2. Прямые эликсиры меняют base-статы (clamp) и сохраняются в BT_HERO
   stat block; timed-эффекты видны в листе и старятся; лечение clamp.
3. Real-оракулы 953/948/408 совпадают.
4. Полный `ctest` с `GL_GAME_DIR` зелёный; docs/PROJECT/STATUS/D-116.

## 9. Открытые вопросы

Нет блокирующих. Left-click target-select, книги/свитки/рецепты и
клиентская 2с-очередь — отдельные задачи (D-116).
