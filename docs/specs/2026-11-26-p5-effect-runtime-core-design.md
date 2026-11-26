# P5 — Effect runtime core (aging + stats + save) — дизайн

Дата: 2026-11-26. Статус: proposed. Решение: **D-115**.
Research: `docs/research/p5-effect-runtime.md`.

## 1. Цель

Замоделировать общий канал активных эффектов персоны (5×int32,
`person+0x110C/+0x14F4`): старение по игровому времени, клиентский фильтр
«ещё не наступившей задержки», применение к статам листа и персистентность
в сейве (BT_HERO и type-2 tail). Продюсеры (зелья/еда/магия) — отдельные
подпроекты.

## 2. Scope / Non-goals

**Scope:**

- ядро: `PersonEffect`, `effect_delay_pending`, `age_effects`, часы
  (минуты между `GameDatetime`);
- `PersonRuntime`: хранение/восстановление эффектов, per-person часы,
  `active_effect_mods`;
- `combat_stats`: эффекты как дополнительный источник модификаторов;
- save restore/write: герой (BT_HERO) и NPC (type-2 tail);
- glgame: загрузка эффектов героя, тик старения, wiring в лист.

**Non-goals:** создание эффектов (зелья/еда/магия/`LE_CastMagic`),
`LE_CastEffect`/`LE_DelEffect` (сущности уровня), интервал `0x8`,
бонусные поля/навыки левелапа, набор B, UI-индикация эффектов.

## 3. Модель и формулы

`PersonEffect { std::array<std::int32_t,5> words; }` — `{id,value,flags,cond,extra}`.

`effect_delay_pending(e)` ⇔ `(flags & 0x20) && (cond & 0xffff) != 0`
(клиентский skip, research §3).

`age_effects(effects, elapsed_minutes)` — точная копия `FUN_1405719c`
(research §2), для каждой записи по индексу:

1. `flags & 0x10`:
   `dur = (cond&0xff)*60 + ((cond>>8)&0xff)`.
   `elapsed >= dur` → удалить запись, индекс не увеличивать.
   Иначе `cond = (rem/60) | ((rem%60)<<8)` (весь dword).
2. `flags & 0x20` (на обновлённом `cond`):
   - `(cond&0xffff)!=0`: `delay = (cond&0xff)*60 + ((cond>>8)&0xff)`.
     `elapsed >= delay` → `cond &= 0xffff0000`; иначе
     `cond = (cond & 0xffff0000) | (rem/60) | ((rem%60)<<8)`.
   - иначе: `dur = ((cond>>16)&0xff)*60 + ((cond>>24)&0xff)`.
     `elapsed >= dur` → удалить; иначе
     `cond = ((rem/60)<<16) | ((rem%60)<<24)`.

`elapsed == 0` → no-op. Часы: `total_minutes(GameDatetime) =
days_from_civil(y,m,d)*1440 + hour*60 + minute`; `delta =
max(0, now − prev)`; невалидное время (`year == 0`) → 0.

## 4. Компоненты и wiring

### 4.1 `gl/game/effect.hpp` (+ `src/game/src/effect.cpp`)

- `struct PersonEffect`;
- `bool effect_delay_pending(const PersonEffect&)`;
- `void age_effects(std::vector<PersonEffect>&, std::uint32_t elapsed_minutes)`;
- `std::int64_t total_minutes(const GameDatetime&)`;
- `std::uint32_t elapsed_minutes(const GameDatetime& prev, const GameDatetime& now)`.
- `SPECIAL`-конверсия: `std::vector<gl::assets::SpecialMod> effect_mods(...)`.

`PersonEffect` переносится из `person_runtime.hpp` в этот заголовок;
`person.hpp` и `person_runtime.hpp` включают его. `set_injured_records`
остаётся в `person_runtime`.

### 4.2 `PersonInstance` (`gl/game/person.hpp`)

```cpp
std::vector<PersonEffect> saved_effects;
std::optional<GameDatetime> effect_clock;
```

### 4.3 `PersonRuntime` (`person_runtime.hpp/.cpp`)

- `PersonState`: `std::optional<GameDatetime> effect_clock`.
- `create`: копирует `saved_effects`/`effect_clock` персон.
- `set_effects(name, effects)`, `effects(name)`; `set_effect_clock`,
  `effect_clock`.
- `age_effects(const GameDatetime& now)`: для каждой персоны
  `delta = elapsed_minutes(clock.value_or(now), now)`, `gl::game::age_effects`,
  `clock = now`.
- `active_effect_mods(name)`: слова → `SpecialMod{id=w0,value=w1,flags=w2,
  word3=w3,word4=w4}`, исключая `effect_delay_pending`.

### 4.4 `combat_stats`

- `StatInputs` + `std::vector<gl::assets::SpecialMod> effect_mods`.
- `compute_armor_weight`: локальная копия `equipped`; при непустых
  `effect_mods` добавить псевдо-предмет `{flags=0,type=0,mods=effect_mods}`;
  все `apply_modifiers`/`attribute_value` получают этот список. Псевдо-предмет
  не выбирается щитом/оружием (маски не стоят).
- `modifier_applies` не меняется (0x2/0x4/0x8/0x200 уже обрабатываются).

### 4.5 glgame (`tools/glgame/main.cpp`)

- Загрузка: после `PersonRuntime::create` (строка ~2459) для BT_HERO
  вызвать `parse_hero_save_body` (если ещё не) и
  `set_effects("Hero", extra)` + `set_effect_clock("Hero", tail80[0x40..])`.
- Основной цикл: после `calendar_.advance(now)` (строка ~914) при
  `has_calendar_` вызвать `person_runtime_.age_effects(calendar_.now())`.
- `open_character`: `inputs.effect_mods = person_runtime_.active_effect_mods("Hero")`.
- Headless-путь `advance_scene`: если есть календарь — также вызывать
  `age_effects` после продвижения времени (если календарь там не
  продвигается, вызов безвреден).

## 5. Save restore/write

- NPC restore: `apply_record` (`save_restore.cpp`) заполняет
  `person.saved_effects` из `record.extra` (5×int32 LE) и
  `person.effect_clock` из `record.tail80[0x40..0x60]`
  (`GameDatetime::from_time_bytes`).
- Hero restore: glgame из `HeroSaveBody` (`extra`, `tail80`).
- NPC write: `TailPersonLive` + `std::optional<std::vector<PersonEffect>>
  effects;` и `std::optional<GameDatetime> effect_clock;`;
  `apply_live_save` заполняет из `input.persons` (только уникальные имена,
  как существующие live-поля); `build_selected_tail` перезаписывает
  `record.extra` и `record.tail80[0x40..0x60]`.
- Hero write: `HeroLiveState` + `effects`/`effect_clock`;
  `make_hero_save_body` пишет `body.extra` и `body.tail80[0x40..0x60]`.
- Кодек/формат не меняются (`encode_*` уже пишут `extra`/`tail80`).

## 6. Ошибки и края

- Невалидный/нулевой datetime → delta 0 (эффекты «замирают», не удаляются).
- `elapsed` с минутами, не секундами (как оригинал).
- Отрицательная дельта (часы назад) → 0.
- Пустой список/нет персоны → no-op.
- Фильтр delay применяется только к статам, не к хранению/старению.
- `extra` из сейва не ограничивается 50 (как `FUN_14018848`); при записи
  сохраняется как есть.

## 7. Тестирование

**Unit (`tests/game`):**

- `effect_delay_pending`: 0x20+delay → skip; 0x20+low0 → apply; 0x10/0x40/0x80 → apply.
- `age_effects`:
  - `0x10` длительность: 100→60 после 40 мин; `>=` удаляет; упаковка часов.
  - `0x20` задержка: low counts down; переход в активную фазу
    (`cond &= 0xffff0000`); затем старшее слово; удаление.
  - `0x10|0x20` обе ветки (fallthrough) — на синтетике.
  - elapsed 0 — без изменений; отрицательная дельта — 0.
- `compute_armor_weight` с `effect_mods`: flat/percent/override, DEX−3,
  clamp; delay-pending не влияет.
- `PersonRuntime::age_effects`: per-person clock, удаление, set/effects.
- Save: `HeroSaveBody` decode/encode c `extra`/`tail80`; `make_hero_save_body`
  подставляет эффекты; `build_selected_tail` перезаписывает `record.extra`.

**Real (`tests/..._real.cpp`, `GL_GAME_DIR`):**

- Скан 28 сейвов: hero `extra` — 27×0, `slot16` = `{9,-3,0x60,0x23000000,-1}`.
- `slot16`: `age_effects` 10 мин → `0x19000000` (25 мин), 35 мин → удалён.
- `slot16`: raw DEX (`BT_HERO` body `0x14`) = 10; `ArmorWeight.primary[2]`
  с эффектом = 7, без эффекта = 10 (flat `0x40`, clamp не срабатывает).
- Type-2 `extra`/`tail80` decode→encode byte-equal на реальном сейве.

**Интеграция:**

```powershell
$env:PATH="..."; $env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```

Полный `ctest` с `GL_GAME_DIR` — без регрессий (ожидается 21/21).

## 8. Критерии приёмки

1. Ядро старения совпадает с `FUN_1405719c` по упаковке/веткам/удалению
   (unit-тесты на все переходы).
2. `slot16` real: эффект восстанавливается, `primary[2]` DEX = 7 (raw 10 − 3),
   через 35 игровых минут эффект удаляется.
3. Save round-trip: эффекты и часы сохраняются/восстанавливаются для hero
   и NPC; чужие snapshot’ы не меняются.
4. Полный `ctest` с `GL_GAME_DIR` зелёный; документы обновлены
   (`STATUS`, `PROJECT`, research, D-115).

## 9. Открытые вопросы

Нет блокирующих. NPC-старение включается для всех персон; если на реальном
сейве обнаружится расхождение — фиксируется в `STATUS` как residual.
