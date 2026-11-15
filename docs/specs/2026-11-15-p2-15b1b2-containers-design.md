# P2-15b1b2 — Restore person containers from save Design

Status: approved (2026-11-15)
Phase: P2-15b1b2 (entity-container decoder + restore; research-backed)
Layers: `assets` (shared item-record parser) + `game` (person decoder,
restore, `InventoryRuntime` replace APIs) + `tools/glgame`

## 1. Goal

Декодировать сериализованные контейнеры type-2 персон (`c0/+0xc`,
`c1/+0x10`, `c2/+0x109c`) и person-tail; применить c0 к main-контейнеру,
c1 — к trade-контейнеру matched persons в порядке `BT_ENTS`; c2 — только
структура. Доказать O4-оракул (19 pinned tuples, 14/0/80 item records,
115,405/115,405 definition ids). NPC loot gate `+0x26c0` и порядок RNG —
`P2-15b1b3`, не входят.

## 2. Evidence

`docs/research/p2-15b1b-restore.md`:

- Type-2 reader `FUN_1401e758` читает три контейнера через `FUN_1405ca18`
  (reader lines 22076-22081), затем tail через `FUN_1401d8bc` (22082).
  Контейнеры установлены в теле персоны **до** chance-gated override
  `FUN_1404b1c0` (22083-22086).
- Save-формат: `container := u32 flag | u32 count + count*item_record`;
  `item_record := raw(0xC0) + u32 sub + sub*0x14`. Ведущий `u32` — литерал
  `1` presence / `0` absent (writer `FUN_1405c9d8` VA `0x5c9e7`/`0x5ca09`;
  reader тестирует только non-zero, VA `0x5ca24`). Absent (0) не имеет
  count-слова.
- Item-record layout идентичен `BT_HERO`: marker `0x14092248` 115,405/115,405;
  `+0x78` == trailing subcount 115,405/115,405. Offsets: `+0x08` type,
  `+0x0C` flags, `+0x20` style, `+0xA8` definition id, `+0xB0` count,
  `+0xB4` durability, `+0xC0` subcount, `+0xC4 + i*0x14` модификаторы
  (5×u32; семантика unverified). `+0xA4` instance id — diagnostic only,
  на load переаллоцируется, remap не нужен.
- Tail: `raw(4*0x20) + u32 c + raw(0x14*c)` (`FUN_1401d8bc` 21261-21295);
  `c == 0` во всех corpus body.
- Corpus: 43,521 контейнеров = 3×14,507 type-2; 42,192 present +
  1,329 absent; 5,150 zero-count; item records c0 82,628 / c1 18,621 /
  c2 14,156; 115,405/115,405 definition id резолвятся в
  `items.pak`/`tech_names.sdb`.
- Роли c0/c1/c2 inferred по содержимому (general/trade/money/NPC_* weapon),
  consumer'ом не доказаны; c2 никогда не регенерируется `FUN_1404b1c0`.
- Duplicates: 81 группа имён (max 6). Оригинал не матчит, а создаёт объект
  на каждую запись; порт ключует runtime по имени — см. §Risks.
- O4 pin (28-save corpus): slot0 14 item records, slot2 0, slot21 80;
  legacy combined digest `9DDA…`/`E3B0…`/`72A8…` (slot0/2/21).

Port на момент спеки: `decode_save_person` читает поля до `+0x1630` и
**не** читает контейнеры/tail; `parse_hero_inventory` содержит приватную
копию item-record layout; `set_person_items`/`set_person_trade_items`
используют `try_emplace` и не перекрывают существующий ключ.

## 3. Scope

Входит:

- `assets::SaveItemRecord`/`SaveItemModifier`/`parse_item_record` — общий
  декодер записи `0xC0 + u32 sub + 0x14*sub`; рефактор
  `parse_hero_inventory` на него без изменения поведения.
- `game::SaveContainer`/`SavePersonRecord.containers[3]`/`tail_count`;
  `decode_save_person` дочитывает контейнеры и tail и требует точного
  consume тела.
- `InventoryRuntime::replace_person_items`/`replace_person_trade_items`
  (overwrite-семантика).
- `restore_save_persons(..., InventoryRuntime *inventory = nullptr)`:
  c0 → main, c1 → trade для matched persons в `BT_ENTS`-порядке;
  last-wins при дубликатах имени + warning.
- `glgame`: передаёт `&inventory_runtime_`; отдельный лог контейнеров.
- Tests: assets unit, game unit, real O4/pins, E2E `--dump-npc`.
- Docs: D-073, STATUS/PROJECT/parity.

Не входит:

- `+0x26c0` gate/chance-regeneration/RNG-порядок (`P2-15b1b3`).
- Type-2-only synthesis (3 записи), SEF-only removal.
- AP, `+0x40/+0x44/+0x654/+0x658/+0x6cc/+0xff0`, tail-семантика.
- NPC equipment/outfit из c2; c2 runtime-таргета нет.
- Occurrence-квалифицированные runtime-ключи (`name#k`).
- Семантика модификаторов (5×u32) и `+0xB8` type-6.
- World item instance id (`+0xA4`) remap; save write (P2-15c).

## 4. API

`src/assets/include/gl/assets/save_item.hpp` (new):

```cpp
namespace gl::assets {
struct SaveItemModifier { std::array<std::uint32_t, 5> words{}; };
struct SaveItemRecord {
    std::uint32_t item_id = 0;    // +0xA8
    std::uint32_t count = 0;      // +0xB0
    std::uint32_t durability = 0; // +0xB4
    std::uint32_t type = 0;       // +0x08
    std::uint32_t flags = 0;      // +0x0C
    std::uint32_t style = 0;      // +0x20
    std::uint32_t mod_count = 0;  // +0xC0
    std::vector<SaveItemModifier> modifiers;
};
struct ItemRecordRead { SaveItemRecord record; std::size_t size = 0; };
Result<ItemRecordRead> parse_item_record(std::span<const std::uint8_t> data);
} // namespace gl::assets
```

`parse_item_record` требует `data.size() >= 0xC4`, читает subcount
`+0xC0`, проверяет `0xC4 + 0x14*sub <= data.size()` (иначе
`ErrorCode::format`), заполняет `size`; trailing bytes после записи не
проверяет (границу держит вызывающий). `parse_hero_inventory` использует
эту функцию (`reader.remaining()` + `reader.skip(read.size)`), внешний
контракт и порядок контейнеров не меняются.

`src/game/include/gl/game/save_person.hpp`:

```cpp
struct SaveContainer {
    bool present = false;
    std::vector<gl::assets::SaveItemRecord> items;
};
struct SavePersonRecord {
    // b1b1 fields unchanged: name, current/anchor tiles, direction,
    // route_type, route_name, radius, delay_min/max, tribe_id, dialog,
    // inventory, hp, energy, route_index.
    std::array<SaveContainer, 3> containers;
    std::uint32_t tail_count = 0;
};
```

`decode_save_person` после route index читает 3 контейнера: `u32 flag`,
`flag != 0` → `present`, затем `u32 count` и `count` item records
(`parse_item_record`), затем tail: skip `0x80`, `u32 tail_count`, skip
`0x14*tail_count`. В конце требует пустой `reader.remaining()`; любое
усечение/хвост → `ErrorCode::format`.

`src/game/include/gl/game/inventory_runtime.hpp`:

```cpp
void replace_person_items(std::string name,
                          std::vector<gl::assets::InventoryItem> items);
void replace_person_trade_items(std::string name,
                                std::vector<gl::assets::InventoryItem> items);
```

Overwrite-семантика (`insert_or_assign`): существующий ключ перекрывается.
`replace_person_items` игнорирует Hero (как `set_person_items`),
`replace_person_trade_items` допускает Hero (как `set_person_trade_items`).
Сами методы не логируют; диагностику ведёт restore.

`src/game/include/gl/game/save_restore.hpp`:

```cpp
struct RestoreCounts {
    std::size_t matched = 0;
    std::size_t sef_only = 0;
    std::size_t type2_only = 0;
    std::size_t skipped = 0;
    std::size_t containers_applied = 0;  // c0/c1 replace for matched records
    std::size_t duplicate_containers = 0; // name replaced a previous applied one
    std::size_t c2_items = 0;             // decoded c2 item records
};
RestoreResult restore_save_persons(Level &level,
                                   const gl::assets::SaveTailEntry &tail,
                                   const gl::assets::SaveTribeTable *tribes,
                                   bool drop_sef_only = false,
                                   InventoryRuntime *inventory = nullptr);
```

Конверсия `SaveItemRecord → InventoryItem`: `item_id`, `count`,
`durability`, `type`, `flags`, `style`, `mod_count`; `mods` остаётся
пустым (raw-модификаторы живут только в `SaveItemRecord` и O4-диагностике).

## 5. Restore algorithm

1. Обход `tail.entities` в сохранённом порядке (как b1b1); type != 2 →
   `skipped`; ошибка декода → `skipped` + warning.
2. Occurrence-match по имени (k-я saved-запись → k-я SEF-персона); нет
   match → `type2_only`, skip.
3. Поля персоны применяются как в b1b1 (без изменений).
4. Если `inventory != nullptr`: `replace_person_items(name, c0.items)` и
   `replace_person_trade_items(name, c1.items)`; `present == false` даёт
   пустой вектор (оригинал не создаёт контейнер). `containers_applied++`.
   Если имя уже применялось в этом restore — warning
   `save restore: duplicate containers for '<name>' occurrence <k>; last record wins`,
   `duplicate_containers++`, замена всё равно выполняется.
5. `c2_items += c2.items.size()`; c2 не применяется.
6. Порядок применения — `BT_ENTS`; SEF-only persons не трогаются; item
   records внутри контейнера сохраняются по одному, в исходном порядке,
   без merge/dedup по definition id (item duplicates — отдельные записи).

## 6. glgame wiring

Порядок `on_level_loaded` не меняется: `generate_npc_inventories` →
`restore_save_persons(..., &inventory_runtime_)` (`tools/glgame/main.cpp`
2104-2115). Существующая строка
`save restore: matched=... sef_only=... type2_only=...` остаётся без
изменений (E2E substring). Добавляется отдельный info-лог:
`save containers: applied=<n> duplicates=<n> c2=<n>`.

## 7. Tests

Assets unit (`tests/assets/test_save_item.cpp`, new):

- record с `sub=0` и `sub=2`: поля/offsets, raw 5×u32, `size`;
- усечённый header (`<0xC4`), subcount за границей, `size` под
  `0xC4+0x14*sub` → `format`;
- `parse_hero_inventory` после рефактора: существующие hero-тесты
  зелёные, порядок/значения не меняются.

Game unit (`tests/game/test_save_restore.cpp`):

- decode: absent (`flag 0`) без count; present+count 0; present с
  произвольным non-zero flag; порядок items и дубликаты definition id;
  tail `0x80 + u32 c + 0x14*c`; trailing byte/усечения
  (flag/count/item/subrecord/tail) → `format`;
- restore: `inventory=nullptr` — без мутаций; c0→main, c1→trade (money
  MON), absent очищает ранее сгенерированный loot; c2 не применяется
  (`c2_items` считается); SEF-only не трогается; drop gate не затронут;
- duplicates: две saved-записи с одним именем → last wins, счётчик
  `duplicate_containers == 1`, warning;
- `replace_*` unit: перекрывает существующий ключ; Hero-гарды как у
  `set_*`.

Real (`tests/game/test_save_restore_real.cpp`, + новый O4 test-case):

- pinned O4: slot0 14 item records, slot2 0, slot21 80; tuple
  `name\toccurrence\tc0\tc1\tc2`, контейнер `flag:def*count,def*count,...`
  (empty — пустая строка), 19/19 строк равны research-данным;
- all-section: 28 saves, 2,599 tail sections, 14,507 type-2 records,
  115,405 item records, каждый definition id есть в `tech_names.sdb`
  (`Sdb::find`) и `items/data/<id>.itm` в VFS; любой mismatch → `d/N`;
- независимая сверка: реконструированная legacy combined строка
  (`name, cur_x, cur_y, anchor_x, anchor_y, direction, route_type, route,
  c0, c1, c2`) даёт research SHA-256 `9DDA…`/`E3B0…`/`72A8…` для
  slot0/2/21; split container-tuple digest пинится как regression после
  первого прогона.

App (`tests/app/test_glgame_screenshot.cpp`):

- `--load-save 0 --dump-npc L93_2.P292_Chernobogs_Guardian2`:
  `npc dump ... container='main'` = последняя saved-запись
  (`812:688`, `919:50`…), `container='trade'` = `812:561`; лог
  `save containers: applied=3 duplicates=1 c2=3`.

## 8. Acceptance

1. `decode_save_person` дочитывает тело до конца: все 2,599 tail-секций /
   14,507 type-2 records consume exactly, 0 mismatches.
2. O4: 19/19 pinned tuples; slot counts 14/0/80; 115,405/115,405 defs
   резолвятся; legacy combined digest 3/3 равен research-пинам.
3. Runtime: c0/c1 применены в `BT_ENTS`-порядке с overwrite; absent →
   пустой контейнер; дубликаты имён last-wins + счётчик; c2 decode-only;
   SEF-only нетронуты.
4. Item records сохраняют порядок и дубликаты (нет merge/dedup).
5. `--load-save 0` даёт восстановленные контейнеры в `--dump-npc`.
6. Полный `ctest --test-dir build --output-on-failure` с
   `GL_GAME_DIR=I:\Games\Златогорье-2` — зелёный.
7. Docs: D-073, `STATUS.md`, `PROJECT.md`, `parity-matrix.md`.

## 9. Risks

- Runtime ключуется по имени: дубликаты персон схлопываются (last wins,
  documented gap); occurrence-ключи — отдельная задача (нативы/скрипты/
  торговля/UI). Decoder и O4 от этого не страдают.
- Absent-контейнер очищает loot matched person — это состояние
  оригинала; b1b3 должен выполнять regeneration вместо/после replacement
  без двойного применения.
- Роль c1 (trade/money) inferred; применение к `trades_` соответствует
  текущей trade-модели порта.
- Семантика модификаторов неизвестна: raw только в `SaveItemRecord`, в
  цену/specials не попадает.
- Неизвестный definition id не отвергается декодером (структурная
  проверка только); резолв проверяется оракулом.
- Рефактор `parse_hero_inventory` прикрыт существующими hero-тестами.
- Стоимость all-section теста: 28 saves/2,599 секций декодируются
  полностью; допустимо для real-теста, но не для unit-прогонов.

## 10. References

- `docs/research/p2-15b1b-restore.md` §Items/§Container walk/§Restore/
  §Oracle.
- `docs/research/p2-15b1-save-entities.md` §Body layout.
- `docs/specs/2026-11-14-p2-15b1b1-restore-fields-design.md`;
  `docs/research/p5-save-equipment.md` (`0xC0` layout).
- D-071, D-072.
