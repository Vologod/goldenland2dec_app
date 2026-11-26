# P5c2b — Day/night stat modifiers in the character sheet (design spec)

Дата: 2026-11-26. Статус: **implemented** (2026-11-26). Подпроект: **P5**
(game systems), UI-часть — **P7**. Продолжение P5c2 (D-113).

Источник фактов: `docs/research/p5c2b-residual-conditions.md`
(`Client.dll` SHA-256 `FBF213B4…AB5C16`).

## 1. Цель

Применять в листе персонажа условные модификаторы статов «день»/«ночь»
(ITM `effect.flags` биты `0x2`/`0x4`) по текущему фактору день/ночь, как
в оригинале (`FUN_12020e68` + `FUN_1201b1c4`). Сейчас такие модификаторы
всегда пропускаются.

## 2. Не входит в scope

- Интервальные модификаторы по времени суток (`0x8`) и `0x200`: в
  шипнутых данных не встречаются (0/2142), остаются «skip».
- Режимные флаги `gv_addon`/`ch_KrepkajaSpina`/капы: дефолты уже верны
  (0/0/15/30), код не меняется.
- Бонусные поля `this+0x16c..`/навыки `this+0x198`: для загруженного
  сейва 0; levелап-UI/баффы — отдельная задача.
- Активный набор A/B — остаётся A (инвентарный UI).
- Стихийный урон: оригинал не проверяет условия — без изменений.

## 3. Модель

`apply_modifiers(base, stat_id, equipped, condition)` — для каждого
эффекта надетого предмета (`mods`), помимо проверки `item.flags & 0x80`:
```
if (flags & 0x200) skip
if (flags & 0x8)   skip                      // интервал: не используется
if (flags & 0x2) {
    if (!condition.day_night_known || condition.day_night != 1.0) skip
}
if (flags & 0x4) {
    if (!condition.day_night_known || condition.day_night != 0.0) skip
}
// далее как раньше: 0x40 flat, 0x80 percent, иначе override
```
`day_night` — `gl::game::day_night_factor(calendar.now())` (double):
`1.0` в 8..20 ч, `0.0` при hour<7/>=22, дробное на 7/21 (условие не
срабатывает). Сравнение точное (`==`), как `float`-сравнение в оригинале.

## 4. Изменения API

`src/game/include/gl/game/combat_stats.hpp`:
```cpp
struct StatCondition {
    bool day_night_known = false;
    double day_night = 0.0;
};
struct StatInputs {
    ...
    StatCondition condition;
};
```
Поведение по умолчанию (`day_night_known=false`) — как сейчас: условные
эффекты пропускаются. Это сохраняет все существующие unit-тесты.

`combat_stats.cpp`: `apply_modifiers` получает `const StatCondition&`;
проводка во все вызовы. `elemental_damage` не меняется.

## 5. Wiring `glgame`

`open_character()` (tools/glgame/main.cpp): при наличии календаря
(`has_calendar_`) задать
`inputs.condition.day_night_known = true;`
`inputs.condition.day_night = gl::game::day_night_factor(calendar_.now());`
Иначе — оставить unknown (условные эффекты не применяются).

## 6. Проверка

Unit (`tests/game/test_combat_stats.cpp`):
- `flags 0x42` (+5 armor): unknown → 9; day(1.0) → 14; night(0.0) → 9;
  sunrise(0.5) → 9.
- `flags 0x44` (+5 armor): night(0.0) → 14; day(1.0) → 9.
- `flags 0x82` percent day: day → applied; night → skipped.
- `flags 0x8`/`0x200` — всегда skip даже при known.
- default `StatCondition` — прежнее поведение (регресс на существующий
  тест «conditional armor modifiers are skipped»).
Real slot0 — без изменений (`cond` отсутствует у экипировки slot0).

Команды:
```powershell
$env:PATH="I:\goldenland2dec\.local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin;I:\goldenland2dec\.local\toolchains\ninja;I:\goldenland2dec\.local\toolchains\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*combat stats*,*character sheet*"
ctest --test-dir build --output-on-failure
```

## 7. Критерии приёмки

- Unit day/night зелёные; существующие combat/character-sheet тесты зелёные.
- Real slot0 оракул не изменился (armor 15, weight 132/204.6, AP 43,
  crit 7, acc 90, урон/сопротивления как прежде).
- Полный `ctest` с `GL_GAME_DIR` — без регрессий.
- Документы обновлены (`STATUS`, research); ручной гейт — пользователь.
