# P2-9 — 35 Hz tick research (design)

Status: implemented (research complete 2026-11-13)
Phase: P2-9 research (docs-only; precedes the 35 Hz implementation)
Layers: research only

## 1. Goal

Доказать точную сетку тика оригинала и подготовить scope перевода порта с
60 Гц на 35 Гц: период тика, потребители шага, модули/тесты порта, которые
изменятся, и порядок реализации. Production-код не меняется.

## 2. Evidence / provenance

| Artifact | SHA-256 / value |
|---|---|
| `GoldenLand.exe` | `7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF` |
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `Client.dll` | `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |

Уже известные факты:

- D-021: порт сейчас на 60 Гц (`ServerCore`, `kFixedDt=1/60`,
  `simulation_time_ms = tick_index*1000/60`).
- D-033: live `GoldenLand.exe` v1.3, `l1_1`: `sv_fps = 35`,
  `sv_game_speed = 0`; движение — 1/35 с, delay `go` 80 ms → ~63 ms,
  85 ms → ~68 ms (тик 35: 56/84 ms); строка `sv_fps` в оригинале.
- `src/game/src/level_simulation.cpp`: `kFixedStep = 1.0/60.0`,
  `kMovementDelayFactor = 0.8`, `kMinBackoffMs = 500`,
  `kRotatePauseMs = 500`.
- `src/core` `ServerCore`: `kTickHz=60`, `kMaxFrameDt=0.25`,
  `kMaxStepsPerTick=15`.
- parity-matrix `P2-9`: `contradicted` (60 Гц vs `sv_fps=35`).

## 3. Research questions

1. **Точная сетка.** Где `sv_fps`/`sv_game_speed` читаются; главный цикл
   (`timeGetTime`/`Sleep`/accumulator); период = `1000/35` (28.571 ms),
   28 ms, 29 ms или иная дискретная сетка; какой `tick_index`/время
   передаются в симуляцию.
2. **`sv_game_speed`.** Что делает `0`; есть ли ускорение/замедление и как
   оно меняет шаг.
3. **Потребители шага в оригинале.** Movement/`_go` delay, `STAY_ROTATE`,
   анимации (кадры/`delay`), AI/таймеры, restock/спавн: что реально
   использует 35 Гц, а что ms/иные интервалы.
4. **Порт.** Все места с 60 Гц/`1.0/60`/`*1000/60`/15 steps: `ServerCore`,
   `LevelSimulation`, `LevelScreen`, screenshot sim, `animation_frame_index`,
   тесты/оракулы (`tick(0.25) → 15`, `0.01*60 → 36`, movement positions,
   `STAY_ROTATE` 500 ms, click/dialog timing).
5. **Детерминизм.** Фиксированные `1.0/35` + целочисленный tick index;
   нужен ли пересчёт `kMaxStepsPerTick` (15 при 35 Гц = 428 ms > 250 ms
   clamp) и `kMaxFrameDt`; как избежать накопления ошибки (`time_ms` из
   tick_index).
6. **Оракулы.** Какие тесты пересчитываются аналитически (не подгонкой),
   какие достаточно обновить по формуле `speed * 1/35`, где нужны
   независимые live-замеры.

## 4. Methods

- Ghidra headless/decompiled: xrefs строк `sv_fps`/`sv_game_speed` в
  `Client.dll`/`Server.dll`/`GoldenLand.exe`, главный цикл, формулы шага.
- Поиск по `.local/decomp/{client,server}/decompiled.c` + `functions.csv`.
- Live evidence: только если статики недостаточно — user-assisted
  read-only замеры (D-033 уже дал 35 Гц; повторять только при
  неоднозначности периода).
- Python/скрипты — в `%TEMP%`, не коммитить.

## 5. Deliverable

`docs/research/p2-9-35hz.md`:

1. Evidence table: строки/RVA/functions, главный цикл, период, формула.
2. Точная модель тика: период, накопление, ограничения кадра.
3. Потребители шага (оригинал) и что остаётся в ms.
4. Порт: полный список файлов/констант/тестов с текущими значениями и
   требуемыми изменениями.
5. Impact matrix: какие оракулы меняются и как пересчитываются.
6. Verdict + декомпозиция реализации (`P2-9a` core tick, `P2-9b`
   movement/animations/oracles) с acceptance.
7. Open Questions (без TBD).

## 6. Acceptance criteria

1. Точный период тика доказан (RVA/lines или live), `1000/35` vs
   дискретная сетка — явно.
2. `sv_fps`/`sv_game_speed` consumer(s) найдены; `sv_game_speed=0`
   объяснён.
3. Полный список потребителей шага и port-мест 60 Гц (grep + файлы).
4. Impact matrix по тестам/оракулам с методом пересчёта.
5. Verdict и декомпозиция с acceptance для реализации.
6. Docs-only: нет изменений `src/tests/tools`; документы UTF-8 без BOM/NUL.

## 7. Out of scope

- Реализация перевода на 35 Гц (отдельный spec/plan после research).
- Изменение анимаций/UI, save/load, RNG.
- Новые live-замеры, если статики достаточно.

## 8. Risks

- `sv_fps=35` может быть UI/конфиг-строкой, а фактический цикл — иной
  (28/29 ms или `timeGetTime`-квант); тогда решение принимается по
  фактическому циклу, а не по строке.
- Часть движений может использовать ms-интервалы, а не тики; слепой
  перевод всех констант на 1/35 — ошибка.
- Пересчёт оракулов может выявить скрытую зависимость от 60 Гц в
  накоплении float; фиксируется как отдельный риск.

## 9. References

- `docs/DECISIONS.md` D-021 (60 Гц), D-033 (`sv_fps=35`, движение).
- `docs/specs/2026-10-20-p2b-deterministic-tick-design.md`,
  `docs/plans/2026-10-20-p2b-deterministic-tick.md`.
- `docs/research/p4b-person-movement.md` (live movement evidence).
- `docs/research/parity-matrix.md` `P2-9` (`contradicted`).
