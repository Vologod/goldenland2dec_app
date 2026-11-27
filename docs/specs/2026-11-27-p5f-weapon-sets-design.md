# P5f — Weapon/shield sets A/B — дизайн

Дата: 2026-11-27. Статус: implemented (2026-11-27). Решение: **D-117**.
Research: `docs/research/p5f-weapon-sets.md`.

## 1. Цель

Смоделировать runtime-состояние активного набора оружия/щита A/B
(`local_player+0x6E4`), переключение (Z и клик по неактивному слоту) и
применение набора к боевым статам, листу персонажа, инвентарю и кукле
героя на уровне.

## 2. Scope / Non-goals

**Scope:** `active_weapon_set` 0/1 (default A, не сейвится, сброс при
load/new game); Z; клик по неактивному weapon/shield слоту в инвентаре и
листе; активная пара для `combat_stats`, `resolve_hero_outfit`,
`CharacterOverlay`, `InventoryOverlay`; CLI `--weapon-set` для
скриншотов.

**Non-goals:** сохранение набора в сейв; серверная боевая логика атак
(вне текущего scope); анимации смены; отдельные арт-кнопки оригинала
(клик по слоту — port UX, D-117).

## 3. Модель

Новый `gl/game/weapon_set.hpp` (SDL-free):

```cpp
enum class WeaponSet : std::uint8_t { a = 0, b = 1 };
constexpr std::uint32_t kWeaponMaskA = 0x001000u;
constexpr std::uint32_t kWeaponMaskB = 0x002000u;
constexpr std::uint32_t kShieldMaskA = 0x400000u;
constexpr std::uint32_t kShieldMaskB = 0x800000u;
struct WeaponSetMasks { std::uint32_t weapon; std::uint32_t shield; };
WeaponSetMasks weapon_set_masks(WeaponSet set); // A→A/A, B→B/B
```

- `active_weapon_set` живёт в glgame (`WeaponSet`, default A); **не**
  сериализуется. Сброс в A в `apply_hero_from_body` (load) и при
  New Game.
- Закрытые наборы используют **только** активные маски, без fallback
  (slot0 A armor=15 — оракул).
- Модификаторы предметов (`FUN_120b0278`) применяются из **всех** надетых
  предметов, без фильтра набора; поэтому primary-атрибуты одинаковы для
  A и B (slot0 perception=13 в обоих). Набор влияет только на выбор
  weapon/shield для armor/damage/outfit.

## 4. Потребители

### 4.1 `combat_stats`

- `StatInputs` + `bool use_b_set = false;`.
- `compute_armor_weight`: `masks = weapon_set_masks(use_b_set ? B : A)`;
  shield-поиск по `masks.shield`, weapon-поиск по `masks.weapon`.
- `open_character` выставляет `inputs.use_b_set = active_weapon_set_ == B`.

### 4.2 `resolve_hero_outfit`

- Сигнатура: `resolve_hero_outfit(equipment, tech, wear, bool use_b_set = false)`.
- Weapon/shield маски из `weapon_set_masks`; никакого fallback: если
  активного предмета нет — соответствующая часть outfit пустая.
- Все call-sites (glgame resolve/load/apply) передают активный набор.

### 4.3 CharacterOverlay

- `open(..., bool use_b_set)` уже есть — передавать активный набор.
- Добавить `void set_use_b_set(bool)` (смена без пересоздания) и
  `bool take_weapon_set_toggle()`.
- Оба weapon/shield слота (`0x001000/0x002000/0x400000/0x800000`)
  существуют как click-target; **иконка** неактивной пары не рисуется
  (parity prop `0x57`); активная пара — рисуется.
- Клик по неактивному weapon/shield слоту → toggle request; клик по
  активному — прежнее поведение (лист read-only → ничего).
- Кукла/overlay-слои — только активный набор (`doll_layer_skipped` уже
  использует `use_b_set_`).

### 4.4 InventoryOverlay

- `open(..., bool use_b_set = false)`; `set_use_b_set(bool)`;
  `take_weapon_set_toggle()`.
- Weapon/shield A/B слоты всегда в `hit_test`; иконка неактивной пары
  не рисуется; активная — рисуется и получает рамку (port UX, 1 px).
- Клик по неактивному weapon/shield слоту → toggle request; по
  активному — прежний выбор equipped-item.
- Active-first lookup пропускает слот активного набора, если его предмет
  отсутствует: точка, где есть только неактивный слот (например, slot0
  shield A отсутствует в `(238,493)`), переключает набор, а не выбирает
  пустой equipped-item.

### 4.5 glgame

- Поле `gl::game::WeaponSet active_weapon_set_ = a;`.
- `toggle_weapon_set()`: flip; пересчитать outfit
  (`resolve_outfit_from_equipment(..., use_b)`), применить
  `level_->apply_hero_outfit`; обновить открытые overlays
  (`character_overlay_.set_use_b_set`, `inventory_overlay_.set_use_b_set`
  или переоткрыть лист с сохранением вкладки); log
  `weapon set: B/A`. При открытом листе персонажа он переоткрывается с
  сохранением вкладки, чтобы боевые числа обновились; у клавиши Z есть
  repeat guard (удержание не переключает набор многократно).
- Клавиша **Z** (`SDLK_z`/`'Z'`) в `handle_keydown`, если state level,
  нет dialog/save/console (overlays открыты — тоже работает).
- Клики: после `inventory_overlay_.handle_mouse` /
  `character_overlay_.handle_mouse` проверять `take_weapon_set_toggle()`
  и вызывать `toggle_weapon_set()`.
- CLI `--weapon-set <a|b>` (только для скриншотов/тестов): выставляет
  `active_weapon_set_` при загрузке.
- Сброс в A в `apply_hero_from_body` и при старте New Game.

## 5. UI-поведение

- В инвентаре/листе видны оба слота weapon A/B и shield A/B; иконка
  неактивной пары скрыта (пустой прямоугольник), активная — с рамкой.
- Клик по пустому/неактивному слоту переключает набор; после
  переключения иконки/doll/статы/outfit отражают новый набор.
- Z переключает из любого уровня (кроме диалога/меню/консоли).

## 6. Ошибки и края

- Набор B без B-weapon/shield: соответствующая часть пустая (без
  fallback); слот остаётся click-target.
- Нет `pending_equipment_`/уровня: state переключается, outfit не
  трогается; overlays обновляются.
- Toggle при открытом диалоге/меню/консоли игнорируется.
- Active set не пишется в сейв; load/new game → A.

## 7. Тестирование

**Unit game (`tests/game/test_weapon_set.cpp`):**

- `weapon_set_masks` A/B.
- `resolve_hero_outfit` (synthetic equipment + real/synthetic TECH/wear):
  A выбирает 0x001000/0x400000, B — 0x002000/0x800000; общие
  helmet/armor/elbarms не зависят от набора.
- `compute_armor_weight` на синтетике: B берёт B-щит/оружие.

**Real (`tests/game/test_combat_stats.cpp` или новый):**

- slot0: A `armor_class=15`; B `armor_class=35` (щит 353 +20),
  `primary[3]=13` (мод `{8,−2}`), `damage[0]=0`, `damage[1]>0`,
  `damage[2]>0`; вес `1320/2046` не меняется.
- slot0 resolver: A weapon group ≠ B weapon group; A shield пуст,
  B shield непуст.

**Unit app:**

- `inventory_overlay_tests`: A/B слоты hit-testable; иконка неактивной
  пары не рисуется (pixel diff); клик по неактивному → toggle request;
  клик по активному → selection; B active рисует B.
- `character_overlay_tests`: `use_b_set` меняет doll/slot; клик по
  неактивному → toggle request.

**Интеграция:**

- `glgame --inventory --weapon-set b --screenshot` и
  `--character --weapon-set b --screenshot` (скриншот-тесты) показывают
  B-оружие/щит; A-кейсы не меняются.
- `ctest` с `GL_GAME_DIR` — 21/21.

## 8. Критерии приёмки

1. Z и клик по неактивному слоту переключают A/B; состояние не
   сейвится, load/new game → A.
2. Combat-статы slot0: A armor=15, B armor=35, B damage-структура и
   perception=13; A-oracle не меняется.
3. Outfit/кукла/инвентарь/лист отражают активный набор; неактивная
   иконка скрыта, слот кликабелен.
4. Скриншоты A/B; полный `ctest` 21/21; docs/D-117.

## 9. Открытые вопросы

Нет блокирующих. Серверная боевая логика атак с активным набором —
отдельный подпроект (после появления боевой системы).
