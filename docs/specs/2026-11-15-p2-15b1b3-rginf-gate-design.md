# P2-15b1b3 — RGINF gate and loot ordering Design

Status: approved (2026-11-15)
Phase: P2-15b1b3 (last P2-15b1b increment; research-backed)
Layers: `game` (rginf decision, loot gate, restore flags) + `tools/glgame`

## 1. Goal

Применить runtime-решение `+0x26c0` из `BT_RGINF`: вычислить due по
сохранённым latch/datetime, `BT_INFO.time` и интервалу `7` дней; на
load-save не запускать ungated `generate_npc_inventories` /
`consume_person_init_rng`; при due — chance-gated регенерация c0/c1 в
`BT_ENTS`-порядке для matched persons (c2 не трогается); SEF-only persons
получают порт-only fallback-генерацию. RNG-parity не заявляется.

## 2. Evidence

- Reader `FUN_140325fc` (decompiled 35812-35846): `BT_RGINF` -> `+0x26c0`
  (latch) и `+0x26a0` (datetime, `0x20` B). Если сохранённый latch `0`:
  `saved + gv_items_regenerate_interval` сравнивается с текущим clock; при
  due `+0x26c0=1` и `+0x26a0=now`; иначе `+0x26c0=0`. Сохранённый latch
  `!= 0` -> `+0x26c0=0`, datetime сохраняется. Сохранённая `1` — не
  «regenerate now».
- `BT_INFO` time blob применяется до `BT_RGINF` (35628-35639), поэтому
  `now` = `payload.info.time` (0x20 B, 8×u32 little-endian).
- Comparator `FUN_1405b490` (RVA `0x5b490`, decompiled 62070-62114):
  лексикографическое сравнение `year, month, day, hour, minute, second,
  millisecond`; `weekday` (index 3) не сравнивается; `a <= b` возвращает
  `1` (равенство = due).
- Add `FUN_1405b38c` (RVA `0x5b38c`, vtable `DAT_14088e14+0`): добавляет
  duration-поля, нормализуя ms/sec/min/hour, месяцы/дни — через
  `FUN_1405b140`; в `FUN_1405b140`/`FUN_1405b0c0` видны грегорианские
  правила `/4`, `/100`, `/400`. Due-duration: `interval / 0x1e` месяцев и
  `interval % 0x1e` дней (35828-35835) — для single-player `7` дней это
  `0` месяцев + `7` дней.
- Interval: `gv_items_regenerate_interval` = `DAT_1407fbcc+0x8c`,
  config init decompiled 2765-2774: `7` single-player, `5` multiplayer.
- Gate `FUN_1404b1c0` (RVA `0x4b1c0`): R1 `coin(this+0x40 =
  regenerate_chance)` at 51141; при `R1=0 && param_4==0` — return без
  регенерации (контейнеры остаются); иначе контейнеры очищаются и
  заполняются pass 1/2. Restore reader вызывает его с `param_4=0`
  (22083-22086); initial spawn `FUN_14018990` (18090) и trigger path
  (22454) — с `param_4=1` (R1 рисуется, но результат игнорируется).
  Текущий порт моделирует ровно `force=1`.
- Candidate condition: `+0x26c0 != 0`, c0/c1 valid (`!= -1`), non-empty
  `scr_inv` (`+0x2ab0`); c2 никогда не передаётся.
- Due на реальных данных (независимый Python-разбор 28 saves; скрипт не
  коммитится): slot0/2/21/3 not due, slot20 (`L18_2`, latch 0, 1 type-2)
  и slot23 (`L4_1`, latch 0, 24 type-2) due. Выбранные секции: latch `1`
  у 11 слотов, due-слотов ровно `{20, 23}`. Slot3 (`L93_4`, latch 1,
  3 type-2) — отложенный b1b2-пин — not due.
- Опорные даты (saved rginf -> BT_INFO.time): slot0 `1126-05-30
  16:17:25.120 -> 20:29:25.120`; slot20 `1124-12-16 06:58:51.660 ->
  1124-12-31 07:21:07.100`; slot23 `1124-11-20 06:06:32.000 ->
  1125-01-15 15:37:50.080`; slot2 `1126-03-31 05:01:11.400 ->
  05:48:09.300`; slot3 `1126-03-31 06:29:30.780 -> 08:30:20.640`;
  slot21 `1124-05-11 08:15:49.440 -> 17:05:55.380`.

## 3. Scope

Входит:

- `game::GameDatetime`, `add_days`, `datetime_le`, `decide_rginf`,
  константа `kSinglePlayerItemsRegenerateIntervalDays = 7`.
- `NpcLootGenerator::generate(..., bool force = true)` ->
  `std::optional<NpcLoot>`: R1 всегда рисуется; `force=false` + провал ->
  `nullopt` без дальнейших draw; `force=true` — текущее поведение.
- `game::regenerate_npc_inventories(runtime, persons, indices, vfs, names,
  lookup, loot_level, rng, specials, force)` с per-person заменой
  main/trade и счётчиками; `generate_npc_inventories` остаётся для New
  Game и разделяет внутренний helper.
- `RestoredPerson::loot_regenerate` (c0&&c1 present && non-empty
  inventory), вычисляется в `restore_save_persons`.
- `glgame` load-save: restore (b1b2) -> decision -> due: gated
  regeneration по `RestoreResult.persons` в `BT_ENTS`-порядке
  (`force=false`) -> fallback `force=true` для SEF-only. New Game path не
  меняется.
- Tests: O6 synthetic + real decisions, gate/draw unit, restore flags,
  E2E slot0/slot23.
- Docs: D-074, STATUS/PROJECT/parity/research.

Не входит:

- RNG-паритет исходной загрузки (spawn chain, weapon/spawn draws,
  call-order), restock/top-up, trigger path, multiplayer interval `5`,
  live oracle, c2 runtime, occurrence-ключи, modifier semantics.
- Регенерация для type-2-only записей (они пропускаются b1b1) и для
  SEF-only через `FUN_1404b1c0` (только port fallback).

## 4. API

`src/game/include/gl/game/rginf.hpp` (new):

```cpp
namespace gl::game {
struct GameDatetime {
    std::uint32_t year = 0, month = 0, day = 0, weekday = 0;
    std::uint32_t hour = 0, minute = 0, second = 0, millisecond = 0;
    static GameDatetime from_array(const std::array<std::uint32_t, 8> &value);
    static GameDatetime from_time_bytes(const std::array<std::uint8_t, 0x20> &value);
    std::array<std::uint32_t, 8> to_array() const;
};
struct RginfDecision {
    std::uint32_t runtime_latch = 0;
    GameDatetime timestamp;
    bool due = false;
};
constexpr std::int32_t kSinglePlayerItemsRegenerateIntervalDays = 7;
bool datetime_le(const GameDatetime &lhs, const GameDatetime &rhs);
GameDatetime add_days(const GameDatetime &value, std::int32_t days);
RginfDecision decide_rginf(const gl::assets::SaveRginf &saved, const GameDatetime &now,
                           std::int32_t interval_days);
} // namespace gl::game
```

`datetime_le` сравнивает year/month/day/hour/minute/second/millisecond
(weekday игнорируется), равенство — `true`. `add_days` переносит
грегорианские дни/месяцы/годы, сохраняет time-of-day и weekday (weekday
не участвует в сравнении и не перезаписывается при due — `timestamp=now`).
`decide_rginf`: `due = saved.latch == 0 && datetime_le(add_days(saved_ts,
interval), now)`; `runtime_latch = due ? 1 : 0`; `timestamp = due ? now :
saved_ts`.

`src/game/include/gl/game/npc_loot.hpp`:

```cpp
struct NpcInventoryCounts {
    std::size_t requested = 0;
    std::size_t populated = 0;
    std::size_t gate_failed = 0;
    std::size_t missing_table = 0;
};
NpcInventoryCounts regenerate_npc_inventories(
    InventoryRuntime &runtime, const std::vector<PersonInstance> &persons,
    const std::vector<std::size_t> &indices, gl::assets::GameVfs &vfs,
    const gl::assets::ItemNameIndex &names,
    InventoryRuntime::ItemTemplateLookup item_template, std::int32_t loot_level,
    MsvcRandom &rng, const gl::assets::SpecialDatabase *specials, bool force);
```

`NpcLootGenerator::generate` меняет возврат на `std::optional<NpcLoot>` и
получает `bool force = true`; существующие callers/tests обновляются на
optional.

`RestoredPerson` (save_restore.hpp) += `bool loot_regenerate = false;`.
`restore_save_persons` заполняет его из `decoded->containers[0].present &&
decoded->containers[1].present && !decoded->inventory.empty()`.

## 5. Algorithms

**Decision** (load-save, выбранная tail-секция):

```
now = GameDatetime::from_time_bytes(payload.info.time)
d = decide_rginf(selected.rginf, now, kSinglePlayerItemsRegenerateIntervalDays)
log "rginf: saved_latch=<l> due=<0|1> runtime_latch=<d.runtime_latch>"
```

**Load-save order** (`glgame` callback; заменяет текущий
`generate_npc_inventories` -> restore):

1. `restore_save_persons(...)` (b1b1/b1b2), serialized c0/c1 применены.
2. Decision (выше).
3. Если `d.due`: индексы `restore_result_.persons[i].level_index` с
   `loot_regenerate == true`, в порядке `persons` (это `BT_ENTS`-порядок);
   `regenerate_npc_inventories(..., force=false)`. `nullopt` (R1 fail) —
   сериализованные контейнеры остаются; success — main/trade заменяются
   сгенерированными (пустой результат очищает). c2 не трогается.
   Лог `npc regen: due=1 candidates=<n> regenerated=<n> gate_failed=<n>
   missing_table=<n>`.
4. SEF-only fallback: индексы `[restore_result_.counts.matched,
   level.persons.size())` (b1b1 кладёт matched первыми, SEF-only после),
   `regenerate_npc_inventories(..., force=true)`; лог
   `npc regen: sef_only=<n> populated=<n>`.
5. RNG: на load-save `consume_person_init_rng` не вызывается (save reader
   не вызывает `FUN_14018990`); New Game path остаётся
   `generate_npc_inventories` + person-init.

`regenerate_npc_inventories` для каждого индекса: skip `Hero`; пустой
`inventory` -> skip; `load_inv` (кэш по имени); ошибка -> `missing_table`,
контейнеры не меняются; иначе `NpcLootGenerator::generate(..., force)`;
`nullopt` -> `gate_failed`; success -> `set`-замена через существующие
replace-API (main/trade), `populated++`.

## 6. Tests

Unit `tests/game/test_rginf.cpp`:

- decision table: (latch 0, due) -> `{1, now}`; (latch 0, not due) ->
  `{0, saved}`; (latch 1, любой due) -> `{0, saved}`;
- comparator: equality -> `true`, weekday difference ignored, ms
  tie-break; lexicographic order across fields;
- `add_days`: same month, month end, leap Feb 29 1124 +7 -> Mar 7,
  Dec 31 +7 -> Jan 7 next year, time-of-day preserved;
- `from_time_bytes`/`to_array` round-trip.

Unit `tests/game/test_npc_loot.cpp` (или новый case):

- `generate(force=false)` при R1 fail -> `nullopt`, RNG advanced ровно
  одним draw (для `p>0`); `force=true` продолжает и возвращает loot;
- `regenerate_npc_inventories`: только переданные indices; gate_failed
  оставляет контейнеры, success заменяет; missing table не меняет;
  порядок draws соответствует порядку indices; SEF-only fallback с
  `force=true`.

Unit `tests/game/test_save_restore.cpp`:

- `loot_regenerate` = true только при c0&&c1 present и непустом
  inventory; absent/empty -> false; c2 присутствие не влияет.

Real `tests/game/test_rginf_real.cpp` (skip без `GL_GAME_DIR`):

- выбранные секции slots 0/2/3/20/21/23: pinned latch + due:
  `{0:0, 2:0, 3:0, 20:1, 21:0, 23:1}`;
- все 28 saves: latch `1` у 11 выбранных секций, due set ровно `{20,23}`.

App `tests/app/test_glgame_screenshot.cpp`:

- `--load-save 0`: `rginf: saved_latch=0 due=0 runtime_latch=0`; b1b2
  container pins (`save containers: applied=3 duplicates=1 c2=3`, main
  `[812:688:`, trade `[812:561:`) остаются без изменений;
- `--load-save 23`: `due=1`, `npc regen: due=1 candidates=<n>` с
  `regenerated + gate_failed == candidates` и `candidates > 0`; без
  предметных pin'ов (оригинального RNG-oracle нет).

## 7. Acceptance

1. O6: synthetic decision table проходит; real pinned decisions
   `{0:0, 2:0, 3:0, 20:1, 21:0, 23:1}`, due set `{20,23}`, latch-1 count
   `11`.
2. Gate: `force=false` fail -> `nullopt` после одного R1 draw; success
   заменяет main/trade; fail сохраняет serialized; c2 нетронут; порядок
   индексов = `BT_ENTS`.
3. Load-save: ungated `generate_npc_inventories`/`consume_person_init_rng`
   не вызываются; SEF-only fallback выполняется после гейта; New Game
   path не меняется.
4. `--load-save 0` не меняет b1b2 pins; `--load-save 23` даёт `due=1` и
   согласованные счётчики.
5. Полный `ctest` с `GL_GAME_DIR=I:\Games\Златогорье-2` зелёный.
6. Docs: D-074, STATUS/PROJECT/parity/research.

## 8. Risks

- Семантика add/compare выведена из дизассемблера (0x5b38c/0x5b490/
  0x5b140) и проверена на 28 saves; экзотические календарные случаи
  (невалидные day/month) не моделируются.
- Интервал жёстко `7` (single-player); `5`/config plumbing вне scope.
- RNG-parity не заявляется: due-регенерация детерминирована в порте, но
  не сверена с оригиналом; E2E для slot23 проверяет решение/счётчики, а
  не предметы.
- SEF-only fallback — port-only (оригинал их не создаёт); состояние
  документированно отклоняется.
- Due-case заменяет serialized containers у успешных персон; это
  ожидаемое поведение оригинала, но live-oracle не снят.
- Порядок `BT_ENTS` для гейта совпадает с `RestoredPerson`-порядком,
  только если b1b1 сохраняет matched-first (текущий контракт).

## 9. References

- `docs/research/p2-15b1b-restore.md` §Ordered restore/§RNG implications/
  §P2-15b1b3 + appended gate evidence (2026-11-15).
- `docs/research/p2-15b1-save-entities.md` §RGINF.
- `docs/research/p3b2b2-npc-loot-server.md` §15.2 (R1/force), §9.2
  (`coin`).
- Specs/plans `2026-11-14-p2-15b1b1`, `2026-11-15-p2-15b1b2`; D-071/D-072/
  D-073.
