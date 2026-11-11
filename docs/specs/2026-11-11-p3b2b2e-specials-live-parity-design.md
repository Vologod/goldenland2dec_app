# Phase 1b2b2e — Specials live parity (design)

- Дата: 2026-11-11
- Статус: implemented/partial (part 1); Stage 2b (merge/stacking) и Stage 4
  (price CW capture) — follow-up
- Предыдущая фаза: `2026-11-10-p3b2b2d-specials-implementation-design.md` (`partial`)
- Связанные документы: `docs/STATUS.md` (1b2b2d), `docs/DECISIONS.md` (D-055,
  D-058–D-061), `docs/research/p3b2b2c-npc-loot-specials.md` (§10),
  `docs/research/p3b2b2-npc-loot-server.md` (§15),
  `tools/live/README.md`, `docs/plans/2026-11-11-p3b2b2e-specials-live-parity.md`

## 1. Проблема

Phase 1b2b2d реализовала specials, но live-гейты OPEN/BLOCKED:
live NPC-mod oracle не совпал (`items captured=19 / port=31, matched=1,
mismatched=48; mods captured=1 / port=0`), а live x87 CW = `0x007F`
(PC=24, single precision, RC=nearest), не `0x027F` (PC=53), который
использовался как extraction-assumption аналитического oracle.

Цель фазы — доказать и закрыть паритет NPC-specials на `l1_1` против
запущенного оригинала, не ослабляя тесты.

## 2. Доказательная база (проверено при подготовке design)

| Факт | Доказательство |
|---|---|
| Container map резолвится ключом, не индексом массива | `Server.dll` `FUN_1404bf8c` → `FUN_14004f14(manager+0xc)` → `FUN_14004a20`: count `+0`, entries `+4`, stride 8 (key `+0`, value `+4`). Незакоммиченная правка tool корректна. |
| Live item flags = ITM flags | `items/data/812.itm` flags `0xC0000000`, `372` `0xC0000284`, `933` `0x8000000C`, `975` `0x40000028` — совпадают с capture строками. Runtime-битов, требующих маски, нет. |
| «Inherited» mods = ITM effects, копируемые как есть | `933` effects `7/1/0x50` + `8/-1/0x50` = live mods; `418` effect `27/5/0x50` = live mod. |
| `level->persons` уже в SEF-порядке | `load_persons` итерирует `sef->persons` без сортировки; гипотеза «port sorts persons» из STATUS неверна. |
| RNG state читается из правильного места | `_rand` (RVA `0x6c09f`): `pDVar1 = FUN_1406ec31(); uVar2 = pDVar1[5]*0x343FD+0x269EC3`; `pDVar1[5]` = `+0x14`, т.е. DR1-сайт `0x6c0a4` верен. |
| Генерируемый live mod в scope один | `P389 item 372 mod 28/2/0x41`; у `372` `effects=0`. Остальные mod-строки — inherited effects. |

Следствия для дизайна:

- Маска item flags **не нужна**: oracle сравнивает raw flags (строже).
- Inherited effects входят в scope: реализуем копирование `Item.effects` в
  `InventoryItem.mods` без RNG.
- Основная нерешённая причина mismatch — provenance/порядок RNG-потока
  и, возможно, precision модель; их доказываем стадиями.

## 3. Цели и не-цели

**Цели.**

1. Live-захват `l1_1` с доказуемым provenance: порядок вызовов
   `FUN_1404b1c0`, состояние RNG перед первым draw, реальный `loot_level`
   из памяти.
2. 0/N совпадение NPC main-контейнеров (`item` и `mod` rows) на `l1_1`
   с live-захватом при старте из captured `rng_state`.
3. Модель inherited ITM effects в порте (value/flags, без RNG).
4. Доказанная precision-модель specials и `TradePrice` (ожидается
   x87 PC=24) и её реализация независимо от host FPU.
5. Обязательный live CW у `FUN_1206E8D0` (цена) и обновлённый FPU-гейт.

**Не-цели.**

- Жёсткий гейт на других уровнях (второй уровень — только если дёшево,
  как smoke, не критерий приёмки).
- Save/сериализация `mods`.
- UI отображения модов.
- Restock/повторная генерация NPC после загрузки уровня.
- Изменение поведения `gv_addon`/perks и прочих не смоделированных
  price-факторов.

## 4. Стадии

### Stage 0 — tool correctness (без live)

- Закоммитить/оформить незакоммиченные правки
  `tools/live/gl_live_capture.cpp`: map-резолв контейнера и `--scan-only`.
- Обновить `tools/live/README.md` (новый резолв, `--scan-only`, формат
  вывода, ограничения).
- Проверка: i686-сборка без диагностик, `--help`, `--scan-only` на
  недоступном PID даёт понятную ошибку.
- Код игры не меняется.

### Stage 1 — provenance live-захвата

Расширить `tools/live/gl_live_capture.cpp` (read-only, без
`WriteProcessMemory`/injection):

1. Логировать каждый DR0-hit: порядковый номер, thread id, имя `.inv`
   из 4-го аргумента `FUN_1404b1c0` (стек, cp1251→UTF-8 для лога),
   `hot`/`main` container id (аргументы 2/3). Эти строки — provenance
   (в oracle-файл только как `#`-комментарии, парсер их игнорирует).
2. Захватывать состояние перед первым и вторым `_rand` draw на
   generation thread; проверить LCG-переход
   `s2 == s1*0x343FD+0x269EC3 (mod 2^32)`; несовпадение → warning и
   partial/диагностика, не фабрикация.
3. Читать реальный `loot_level` из `BT_INFO+0x26c4` (+`1`, контракт
   research §15/§5.2) и писать его в `loot_level=`; CLI `--loot-level`
   остаётся fallback только при недоступности памяти и помечается
   provenance-комментарием.
4. Писать provenance-комментарии: источник `rng_state`, `loot_level`,
   CW, число DR0-hit'ов.

Acceptance Stage 1 (одна пользовательская live-сессия):

- `gl_live_capture.exe` стартует до загрузки `l1_1`, ловит generation
  thread, пишет валидный `format=gl-specials-live-2`.
- LCG double-draw check проходит.
- В логе есть полный упорядоченный список вызовов с именами `.inv`.
- `loot_level` совпадает с ожидаемым из памяти; значение зафиксировано.
- Отчёт в `docs/research/p3b2b2c-npc-loot-specials.md` §11: порядок
  вызовов против SEF-порядка порта, provenance RNG/loot_level/CW,
  обновлённый diff порта.

### Stage 2 — модель по доказанному

Выполняется только по результатам Stage 1.

1. **Порядок/scope генерации.** Привести `generate_npc_inventories`
   (`src/game/src/npc_loot.cpp`) к live-порядку, если он отличается от
   `level.persons` (SEF): какие persons вызываются, в каком порядке,
   один ли раз, когда именно (до симуляции). Изменение — только по
   live-логу; без «подгонки».
2. **Inherited effects.** При создании записи копировать
   `ItemTemplate.effects` (id=`type`, value=`int32(amount)`,
   flags=`flags`) в `InventoryItem.mods` / `LootItem.mods`; `mod_count =
   template effects + generated mods`. RNG не потребляется. Это
   заменяет count-only поведение 1b2b2d и снимает исключение inherited
   rows из oracle.
3. **Merge/stacking.** Если live-diff показывает слияние/разбиение
   стеков, отличное от порта, восстановить `FUN_1404e3a4` по decomp и
   смоделировать; иначе не трогать (merge остаётся `unverified`, но
   oracle обязан совпасть — при расхождении это блокер Stage 2).

Acceptance Stage 2:

- Unit-тесты: copied effects (на реальных `933`/`418` effects, если
  доступны через catalog-фикстуру), `mod_count`, отсутствие RNG-draw,
  порядок persons.
- Real-data тест: oracle без исключения inherited rows.
- Повторный gate-run с новым захватом: diff сокращён; оставшиеся
  расхождения классифицированы (precision/порядок/иное) и записаны.

### Stage 3 — precision модель

1. По asm `FUN_14047284` и `FUN_1206E8D0` подтвердить, что арифметика
   x87 и что при PC=24 результат каждой операции округляется до
   binary32; RC=nearest; финальный `ROUND` — trunc (`fistp`).
2. Реализовать эмуляцию: после каждой float-операции приводить
   результат к `float` (хелпер `f32()` в `special_engine.cpp` /
   `trade_price.cpp`); не полагаться на x87/MXCSR host'а (Android/ARM).
3. `SpecialEngine`/`random_special_count`: baseval/mod_val/products,
   `fval*level`, сравнения и half — в binary32-модели.
4. `TradePrice`: перевести с `assume_pc53`/double на binary32-модель;
   пересчитать trade oracle и обновить D-058; сохранить требование
   RC=nearest.
5. Обновить аналитический Python-референс (`loot_ref.py`) на
   binary32-модель и перегенерировать `l1_1` oracle (draw count,
   final state, events).

Acceptance Stage 3:

- Unit-тесты на границы binary32 (значения, где single и double дают
  разные `int(fval*level)`), оба прохода specials, цена.
- Deterministic `l1_1` oracle обновлён и проходит.
- Trade-тесты и screenshot/overlay оракулы пересчитаны (если цена
  изменилась); полный `ctest` — без регрессий.

### Stage 4 — приёмка

1. Новый live-захват `l1_1` (generation) + отдельный захват CW у
   `FUN_1206E8D0` (client price site); значения записываются в
   FPU-CW файл `generation=<live> price=<live>`.
2. FPU-гейт: проверяет **измеренные** значения и биты PC=24
   (`cw & 0x0300 == 0`) и RC=nearest (`cw & 0x0C00 == 0`). Ожидание
   `0x027F` заменяется доказанным live-значением; это исправление
   reference, а не ослабление.
3. Live NPC-mod oracle: `mismatched_items == 0`, `mismatched_mods == 0`,
   `captured == port`, 0/N.
4. Docs: research §11, D-061 update или новый D-062, STATUS/PROJECT,
   parity matrix, plan.

## 5. Компоненты и файлы

| Компонент | Файлы | Изменение |
|---|---|---|
| Live tool | `tools/live/gl_live_capture.cpp`, `tools/live/README.md` | map-резолв, `--scan-only`, call-order/loot_level/двойной draw, provenance |
| Oracle gate | `tests/game/test_special_gates.cpp` | raw flags, inherited rows в scope, live loot_level/CW, диагностика |
| Генератор лута | `src/game/src/npc_loot.cpp`, `include/gl/game/npc_loot.hpp` | порядок/scope, inherited effects, merge (по evidence) |
| Specials | `src/game/src/special_engine.cpp`, `include/gl/game/special_engine.hpp` | binary32-модель |
| RNG | `src/game/src/msvc_random.cpp` | только double-draw self-check (поведение не меняется) |
| Item model | `src/game/include/gl/game/*` (ItemTemplate/InventoryItem), `src/game/src/inventory_runtime.cpp` | effects → mods при создании/transfer |
| Price | `src/game/src/trade_price.cpp` | binary32-модель, D-058 |
| Production wiring | `tools/glgame/main.cpp` | lookup передаёт effects; loot generation order |
| Reference | `.superpowers/sdd/2026-11-10-p3b2b2c-npc-loot-specials-research/loot_ref.py` | binary32-модель, новый oracle |

## 6. Формат capture

- `format=gl-specials-live-2` сохраняется; новые данные — только
  `#`-комментарии и корректные значения существующих ключей.
- `loot_level` — из памяти (`BT_INFO+0x26c4 + 1`); CLI только fallback
  с комментарием.
- `item`/`mod` rows — raw ITM flags (без маски).
- `# fpu_cw=` — generation/first-draw/scan-time; price CW — отдельный
  файл для `GL_SPECIALS_FPU_CW`.
- Partial capture (`*-partial`) gate по-прежнему отвергает.

## 7. Ошибки и деградация

- Нет generation thread / RNG state / LCG-перехода → partial +
  ненулевой exit; oracle не запускается.
- Нет `loot_level` в памяти → fallback CLI + provenance warning; gate
  помечает это сокращённым доказательством.
- Нет price CW → price-гейт OPEN; full parity не заявляется.
- Любой оставшийся mismatch → статус `partial`, факты в research;
  тесты не ослабляются.

## 8. Проверка (команды)

```powershell
# tool (i686)
I:\goldenland2dec\.local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin\i686-w64-mingw32-g++.exe `
    -std=c++20 -O2 -Wall -Wextra -static tools/live/gl_live_capture.cpp `
    -o tools/live/gl_live_capture.exe -lpsapi -lshell32
.\tools\live\gl_live_capture.exe --help

# build + gates
cmake --build build
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
$env:GL_SPECIALS_LIVE="I:\goldenland2dec\.local\live\special-l1_1-entry.txt"
$env:GL_SPECIALS_FPU_CW="<generation/price file>"
build\bin\gl_game_tests.exe --test-case="*special*gate*" --success

# full
ctest --test-dir build --output-on-failure
```

Live-сессии запускает пользователь; агент не подтверждает manual pass.

## 9. Критерии готовности (DoD)

1. Stage 1 evidence report опубликован (порядок, provenance, diff).
2. Inherited effects смоделированы и покрыты тестами.
3. Precision-модель доказана asm+live и реализована; oracle/цена
   пересчитаны.
4. Live NPC oracle 0/N на `l1_1`; FPU-CW гейт на измеренных значениях.
5. Полный `ctest` с `GL_GAME_DIR` — зелёный.
6. STATUS/PROJECT/research/DECISIONS/parity обновлены; коммит — по
   указанию пользователя.

## 10. Открытые вопросы

Вынесены в `docs/STATUS.md` → Open Questions (1b2b2e), чтобы не
оставлять TBD в spec:

- Точный порядок вызовов `FUN_1404b1c0` и соответствие SEF/entity.
- Нужен ли merge/stacking для 0/N на `l1_1`.
- Live CW у `FUN_1206E8D0` (значение и модель цены).
- Причина отсутствия портируемого generated mod (`372`) при captured
  state: порядок, precision или capture provenance.
