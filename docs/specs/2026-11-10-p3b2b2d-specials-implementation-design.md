# Phase 1b2b2d — NPC loot specials implementation (design)

Дата: 2026-11-10. Статус: approved (пользователь, 2026-11-10).

## 1. Цель

Реализовать random-specials NPC-лута по `specials.scr` и
`FUN_14047284`: парсер БД, R4–R10 RNG, выбор модификаторов, запись
mod/value/flags в предмет, интеграция в `NpcLootGenerator`.

## 2. Контекст и гейты

- Research `docs/research/p3b2b2c-npc-loot-specials.md`; D-059.
- DB: `scripts/item_class_specials/specials.scr` (58 `special_desc`,
  15 `itemclass`, 9 слотов×0x10; hardcoded `IDSPEC_*`/`SPECF_*`/`T_*`).
- Алгоритм: identity `[0..57]` ∩ type-pool (ascending); R6
  `pool_size-nspec` removals `range(0,current_count-1)`; R7 value-group;
  R8 time-group (retail n2=1); R9/R10 value; `flags=tag1|tag2`;
  `item+0xc |= 0x200`; stride `0x14`.
- R4/R5: `flags&0x40` → 0.09/(9-k)*0.01 (limit 9), иначе
  0.45/(0.45-0.15k) (limit 3); условия `chunk==1`, `flags&4`,
  `item+0x78==0`.
- **Обязательные acceptance-гейты (до заявки на full parity):**
  1. live FPU-CW до `FUN_14047284` (generation) и `FUN_1206E8D0` (price);
     ожидается `0x027F` (PC=53/RC=nearest); иначе generation/price
     `blocked`.
  2. live NPC-mod oracle: NPC-контейнеры `+0x10`, mod-записи
     (`+0x78/+0x7c`, stride 0x14), mod id/value/flags, `flags&0x200`;
     0/N сравнение.
- Реализацию можно начинать до гейтов; без них — `partial`, не full
  parity.

## 3. Scope

### 3.1 `gl::assets`

- `parse_specials_scr(span) -> Result<SpecialDatabase>`:
  - `SpecialDef { id, random_sign, value_slots[3] { tag, access, value, price },
    time_slots[6] { tag, access, value, price } }` (в файле — 9 слотов:
    3 value + 6 time);
  - `item_classes: map<ItemType, vector<SpecialId>>` (T_* → type);
  - hardcoded enum maps (IDSPEC/SPECF/T) из `GoldenLand.exe` (research
    §2.3); неизвестное имя → format error.
- `SpecialDatabase` без SDL/ОС; путь
  `scripts/item_class_specials/specials.scr`.

### 3.2 `gl::game`

- `SpecialMod { id, value, flags }`.
- `random_special_count(flags, rng)` (R4/R5).
- `SpecialEngine::apply(db, item_type, nspec, loot_level, rng) ->
  vector<SpecialMod>` (FUN_14047284) с R6–R10; `nspec==0` → пусто;
  pool ascending; `flags|=0x200` ставит вызывающий item.
- `InventoryItem` расширяется `std::vector<SpecialMod> mods`;
  `mod_count` = размер (для загруженных save mods — как минимум count).
- RNG — тот же `MsvcRandom` (D-053), double/PC=53, `-ffp-contract=off`;
  `loot_level = party/hero level + 1`.

### 3.3 Интеграция

- `NpcLootGenerator`, при создании чанка `chunk==1` и
  `flags&4 && item.mod_count==0`:
  1. `nspec = random_special_count(flags, rng)` (R4/R5);
  2. если `nspec != 0` → `apply(...)` (R6–R10), записать mods,
     `flags |= 0x200`.
- Порядок RNG: R4/R5 → R6–R10 на каждый qualifying item в порядке
  pass1/pass2 (research §5.1); per-person nesting сохраняется.
- Existing no-specials oracle (`174/218/323`) получает
  `no_specials` режим/пометку `superseded`; tests разделяются.
- Price/trade: items with `mod_count>0` остаются `unsupported` для
  цены/торговли (D-058); modifier value не используется как trade price.

## 4. Non-goals

- Trade price модифицированных предметов (live CW/price gate).
- `random_sign` application site, time-slots 4–6, flag0x40 semantics.
- UI отображения модификаторов (отдельная задача).
- Save/сериализация mods.

## 5. Архитектура

- `src/assets/.../specials.hpp/.cpp` — парсер/БД/enum maps.
- `src/game/.../special_engine.hpp/.cpp` — R4–R10/apply.
- `NpcLootGenerator` использует engine; `InventoryItem.mods`.
- `gl_assets` не зависит от SDL; `gl_game` не зависит от SDL.

## 6. Тестирование

Unit:

- `specials.scr`: 58 defs, 9 slots, 15 itemclasses, pool sizes §2.4;
  enum maps; malformed.
- Engine: pool identity∩type ascending; nspec=0/all; R6 removal bounds;
  R7 MAX_HEALTH/MAX_ENERGY; R8 n2=1; R9/R10 conditions; flags/0x200;
  exact fixed-seed vectors.
- R4/R5: 0.45/0.30/0.15 and 0.09…0.01, limits 3/9, no-draw cases.

Integration:

- `l1_1`, seed 1, loot level 1: 18 candidates/9 triggers/12 mods,
  1329 draws, final `0x48CAA370`; per-person nesting regression
  (`Kolbasens 2×87`); first divergence `Crying_Guardian`.
- no-specials mode still matches `174/218/323`.

Live gates:

- FPU-CW capture (generation+price) — test/skip if unavailable; record.
- NPC-mod 0/N differential (user-assisted) — acceptance gate.

## 7. Критерии приёмки

1. Parser/engine/integration tests pass.
2. Deterministic specials oracle (corrected) matches 0/N.
3. No-specials mode regression passes.
4. Live FPU-CW and NPC-mod oracle captured and match 0/N (full parity);
   без них — `partial` с D-060.
5. Price/trade modifiers remain unsupported; full `ctest`.
6. D-060/parity/STATUS/PROJECT; UTF-8 без BOM/NUL.

## 8. Риски

- Modifier DB/enum mapping ошибки → parser tests/real 58/15.
- Live gates могут быть недостижимы; тогда merge с `partial`, не
  подгонять.
- RNG/FPU-CW отличия → values ±1; gate.
- Merge/stackable specials — engine применяется только к chunk==1.
