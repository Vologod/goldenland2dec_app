# P5a — save → hero outfit (design)

Дата: 2026-11-04. Статус: design approved; research gate перед планом.

Источники: `docs/research/p2c-save-slots.md`, `p4b2-render-order.md`
(BT_HERO/equipped items), `p4d-hero-wear.md` (item→tech, HAD/IAD/SEQ),
`docs/DECISIONS.md` D-041/D-042/D-043.

## 1. Цель

Выбор save-слота в существующем меню применяет к герою **экипировку из
этого слота** (внешний вид), без загрузки мира/позиции/квестов. Hero
рисуется теми же wear-слоями, что в оригинале для данного save.

## 2. Scope

- Читать equipped-записи из `BT_HERO` выбранного `Saves/slot<N>.sav`:
  **все equipped item instances**, не только визуальные.
- Хранить их в модели героя; для рендера использовать 5 визуальных
  категорий: `weapon`, `shield`, `elbarms`, `helmet`, `armor`.
- Маппинг item instance → tech/group → `HeroOutfit` через ITM,
  `items.pak`, `scripts/herowear.scr`; источник истины — оригинальный
  save/движок.
- Меню save slots: выбор валидного слота запоминает `HeroEquipment` и
  применяет его к текущему уровню, если уровень открыт; иначе — при
  следующем `load_level` (`New Game`/`--level`). Мир не загружается.
- Fallback: ошибка чтения/неизвестный item/tech → **naked hero**
  (`wear_set = noweapon_thrw`, все слои пусты) + warning; меню/уровень
  продолжают работать.
- Acceptance: `slot0` + один другой слот; real-data тесты; screenshot
  naked vs slot0; пользовательский visual gate против оригинала.

## 3. Non-goals

- Полная загрузка мира/позиции/NPC/квестов из `.sav` (P2c).
- Inventory UI, use/drop/экипировка через интерфейс, tooltips (P5b+).
- Stats/skills/magic и влияние предметов на параметры.
- Отрисовка аксессуаров (bracelet/amulet/rings): они читаются и
  хранятся в модели, но не имеют wear-слоя в этом срезе.
- Android/UI-полировка.

## 4. Архитектура

```
Saves/slot<N>.sav
  → gl::assets: parse save header/body + BT_HERO equipped records
  → gl::assets::HeroEquipment (item_id, type, raw fields, slot)
  → gl::game: resolve_hero_outfit(equipment, ItemCatalog, HeroWearCatalog)
  → HeroOutfit
  → gl::app::LevelScreen::apply_hero_outfit(outfit)
  → HeroVisual/Texture → SDL
```

Слои только вниз: `assets` (бинарные форматы) не зависит от SDL/game;
`game` — маппинг и `HeroOutfit`; `app` — загрузка текстур и меню.

## 5. Данные и API

### 5.1 `gl::assets`

```cpp
struct EquippedItem {
    std::uint32_t item_id = 0;
    std::uint16_t type = 0;
    std::uint16_t slot = 0;
    std::array<std::uint8_t, 16> raw{}; // поля, нужные для item→tech
};

struct HeroEquipment {
    std::vector<EquippedItem> items; // все equipped-записи BT_HERO
};

Result<HeroEquipment> parse_hero_equipment(std::span<const std::uint8_t> save_body);
```

Точные offsets/размер item-записи и equipped-слотов извлекаются на
research-этапе (writer `0x1401e690`, item writer `0x1404f4bc`) и
фиксируются в `docs/research/p5-save-equipment.md` с RVA/offset.
Парсер не привязывается к UI и не читает `wear.pak`.

### 5.2 `gl::game`

```cpp
struct HeroEquipmentVisual {
    std::string wear_set = "noweapon_thrw";
    std::optional<std::string> weapon, shield, elbarms, helmet, armor;
    std::vector<EquippedItem> other_equipped; // bracelet/amulet/rings и т.п.
};

Result<HeroOutfit> resolve_hero_outfit(const HeroEquipment& equipment,
                                       const ItemCatalog& items,
                                       const HeroWearCatalog& wear);
```

`HeroOutfit` остаётся текущим типом P4d. Naked outfit — `HeroOutfit{}` с
пустыми слоями и `wear_set = "noweapon_thrw"`.

### 5.3 `gl::app`

```cpp
void LevelScreen::apply_hero_outfit(const HeroOutfit& outfit);
```

Перезагружает `HeroVisual`, body/shadow textures и headers без
перезагрузки уровня. Ошибка загрузки → naked outfit + warning.

## 6. Поток меню

1. Меню `Load Game` показывает существующие save slots (`SaveManager`).
2. Выбор валидного слота: читается файл, парсится header/body и
   equipped-список.
3. `HeroEquipment` сохраняется в `Game` (`pending_equipment_`).
4. Если уровень уже открыт — сразу `apply_hero_outfit`.
5. Если уровня нет — outfit применяется при следующем `load_level`
   (`New Game`/`--level`).
6. Сообщение в меню/console: применён слот, либо naked-fallback.
7. Ошибка чтения/парсинга не блокирует меню и не меняет текущий уровень;
   outfit сбрасывается в naked согласно выбранному поведению слота.

## 7. Research gate (до реализации)

1. Найти точное начало/размер equipped item records в `BT_HERO`
   (`GoldenLand.exe` writer/reader + live save).
2. Подтвердить на `slot0` IDs `318/397/280` (Almaz/Goluboy/Cherny) и
   найти второй слот с другим набором.
3. Извлечь item→tech/group (ITM `unknown_b`, `herowear.scr`, каталоги
   `wear.pak`) и проверить 5 визуальных категорий.
4. Записать offsets/RVA/наблюдения в `docs/research/p5-save-equipment.md`.
   Только после 0/N совпадений на двух слотах — реализация.

## 8. Ошибки и fallback

- Save header/body invalid → naked outfit + warning, слот не применяется
   к миру.
- Equipped record не читается → naked outfit для всех категорий +
  warning.
- Item/tech не найден → naked outfit для соответствующей категории,
  остальные категории применяются.
- `apply_hero_outfit` не смог загрузить wear → naked outfit + warning;
  при повторной ошибке — hero marker (существующее поведение P4d).

## 9. Тесты и acceptance

- Unit: synthetic save-body parser; item→tech/group mapping; naked
  fallback; выбор слотов.
- Real: `slot0` даёт `318/397/280` → Almaz/Goluboy/Cherny; второй слот —
  другой набор; несовпадения — ошибка теста.
- App: выбор слота в меню запоминает outfit; `apply_hero_outfit`
  перестраивает текстуры без reload уровня; screenshot naked vs slot0.
- Manual gate: пользователь сравнивает hero в порте с оригиналом на
  том же save (slot0 + второй слот).
- Полный `ctest` с `GL_GAME_DIR`; без game data real-тесты skip.

## 10. Риски

- Точное расположение equipped-списка в `BT_HERO` не извлечено; research
  может потребовать Ghidra/live-память.
- Item→tech может быть неполным для некоторых item ID; fallback naked
  фиксирован, но визуальный parity будет неполным.
- Некоторые wear-ассеты отсутствуют (пример: `shlem_cherny`); категория
  остаётся naked, это отражается в warning.
