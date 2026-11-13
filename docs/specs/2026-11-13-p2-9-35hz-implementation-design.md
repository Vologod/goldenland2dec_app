# P2-9 — 35 Hz tick implementation (combined P2-9a/P2-9b) Design

Status: implemented (2026-11-13)
Phase: P2-9a + P2-9b (combined parity pass)
Layers: `core` (ServerCore) + `game` (LevelSimulation) + `app`/`tools` (loops, fixtures)

## 1. Goal

Перевести тик и симуляцию с 60 Hz accumulator на оригинальную модель:
порог **28 ms** (`trunc(1000/35)`), один тик за host-кадр, без
accumulator/catch-up/clamp; `LevelSimulation` делает ровно один шаг на тик.
Обновить детерминированные оракулы из независимой модели, не подгонкой.

## 2. Evidence

- `docs/research/p2-9-35hz.md`:
  - `sv_fps=35`, `sv_game_speed=0`; host gate `0x4184ac`: signed
    `1000/fps` (`idivl`) = 28 ms; `elapsed >= interval`; один tick pass,
    `last_ms = current sample`; нет accumulator/catch-up/clamp.
  - Эффективный период квантуется host frame; точные 35 Hz не заявляются
    (U1).
  - `LevelSimulation` P10/P11: один integer-ms шаг; delta — фактический
    tick delta, не 1/60.
  - D-033 `0.8` — приближение; точная формула `anim_word0 + gv_anim_speed - 17`
    (U-C1); в этом пассе `gv_anim_speed=0`.
- D-021 (60 Hz) — исторический порт-дизайн; D-067 фиксирует оригинальный
  gate и supersede как parity target.

## 3. Model

### 3.1 `ServerCore`

Константы:

```cpp
inline constexpr std::int32_t kDefaultSvFps = 35;
inline constexpr std::int32_t kDefaultSvGameSpeed = 0;
inline constexpr std::uint32_t kTickIntervalMs = 28; // 1000 / 35, trunc
inline constexpr std::int32_t kNominalTickHz = 35;    // display only
std::uint32_t tick_interval_ms(std::int32_t sv_fps); // 0 => tick every frame
```

`tick_interval_ms`: `sv_fps == 0` → 0 (gate skipped); `sv_fps < 0` → 0
(original negative interval ticks every frame); `sv_fps > 1000` → 0;
`1..1000` → `1000 / sv_fps` (integer trunc; 1000 → 1).

Состояние: `clock_ms_`, `last_ms_`, `tick_index_`, `simulation_time_ms_`.
`accumulator_`, `kFixedDt`, `kMaxFrameDt`, `kMaxStepsPerTick` удаляются.

`tick(double dt)`:

1. Применить pending commands (как сейчас, до gate/паузы).
2. `dt` non-finite/≤0 → 0; иначе `advance_ms = llround(dt*1000)`.
3. `clock_ms_ += advance_ms` (без clamp).
4. Если `paused_`: `last_ms_ = clock_ms_`; вернуть (порт-выбор: без backlog;
   оригинальная pause-семантика `unverified`).
5. `interval = tick_interval_ms(kDefaultSvFps)`.
6. Если `interval == 0 || clock_ms_ - last_ms_ >= interval`:
   `last_ms_ = clock_ms_`; `++tick_index_`; `simulation_time_ms_ = clock_ms_`.
   **Один тик максимум**, без цикла.
7. `sv_game_speed` fixed `0`: не влияет на interval; документируется, не
   читается из config (plumbing вне scope).

`simulation_time_ms` — latch absolute clock (не `tick_index*28`), как
P8/T4. `tick_index` — число тиков.

### 3.2 `LevelSimulation`

- `advance(double dt_seconds)`: вычислить `delta_ms = llround(dt*1000)`;
  `delta_ms <= 0` → return; `time_ms_ += delta_ms`; **ровно один шаг**.
- Удалить `kFixedStep`, `accumulator_`, `time_ms_exact_`.
- Движение: `step = speed * (delta_ms / 1000.0)`; `speed` — прежняя
  px/s формула (`iso_speed * 1000 / (go_delay * 0.8)`), без изменения
  `0.8` в этом пассе (уточнение `-17` — отдельный U-C1 follow-up).
- Все таймеры (`wait_until_ms`, `state_started_ms`, `kMinBackoffMs`,
  `kRotatePauseMs`) остаются ms и сравниваются с `time_ms_`.
- `LevelScreen`: продолжает вызывать `advance((animation_ms-last)/1000)`;
  теперь каждый server tick даёт один шаг с фактическим delta.

### 3.3 `glgame` / headless

- Интерактивный цикл: убрать `delta_ms > 250` clamp; `server_.tick(dt)`
  получает фактический delta; `animation_ms = simulation_time_ms`.
- Headless `--sim-ms`: declared frame schedule **16 ms**; каждый frame —
  `server.tick(0.016)`; оракулы пересчитать по расписанию (20 s → 1250
  frames; ticks на 28 ms gate при 16 ms сетке → каждые 32 ms → 625).
  Если конкретный тест использует иную сетку, она фиксируется в тесте.
- `glview` headless: та же 16 ms сетка; `differing > 1000` пересчитать из
  расписания (A/R), не из нового вывода.
- Анимации: `animation_frame_index` остаётся ms-based; меняются только
  входные `animation_ms` (следствие тика). Отдельная миграция анимаций —
  вне scope.

## 4. Testing / oracle strategy

- `core`: `tick_interval_ms` edge cases (35→28, 0→0, <0→0, 1000→1, >1000→0);
  gate 16/28/56/250 ms → 0/1/2/8 ticks (один тик за вызов: 250 ms = 1 тик);
  no catch-up (2×16 ms → 1 тик, 2×28 ms → 2 тика); `simulation_time_ms`
  latch; pause behavior (no backlog, resume after 28 ms).
- `game`: каждый `advance` = один шаг; `0.028` delta; movement distances/
  arrival/STAY_ROTATE/occupancy; пересчёт из аналитической модели
  (`speed * 0.028`), не из порта.
- `app`/`render`: screenshot/click/dialog headless tests — пересчитать
  ожидаемые tick counts/кадры по 16 ms schedule; `differing` thresholds —
  из расписания.
- Real tests: NPC loot/save не зависят от tick; если full ctest выявит
  зависимость — фиксируется отдельно.
- Full `ctest` с `GL_GAME_DIR` — обязательный гейт.

## 5. API/compat

- `server.hpp`: удалить `kFixedDt`/`kMaxFrameDt`/`kMaxStepsPerTick`; добавить
  `kTickIntervalMs`, `kNominalTickHz`, `tick_interval_ms`, `kDefaultSvFps`,
  `kDefaultSvGameSpeed`.
- `ServerCore`: `tick(double)` сохраняется; добавляется `clock_ms()`
  (для тестов/диагностики), `simulation_time_ms()` как latch.
- `LevelSimulation`: `advance(double)` сохраняется, семантика «один шаг за
  вызов»; `time_ms()` — integer.

## 6. Out of scope

- Config/console plumbing `sv_fps`/`sv_game_speed`/`gv_anim_speed` (fixed
  defaults 35/0).
- Точная формула `anim_word0 + gv_anim_speed - 17` (U-C1) и non-zero
  `sv_game_speed`.
- Миграция всех анимаций на 28/16 ms сетку сверх входного `animation_ms`.
- Save/load, RNG, AI/restock cadence.

## 7. Acceptance

1. `ServerCore` gate: 28 ms threshold, один тик/вызов, без clamp/catch-up;
   `simulation_time_ms` = clock latch; edge cases `tick_interval_ms`.
2. `LevelSimulation`: один шаг/вызов, integer `time_ms`, движение
   `speed*delta`; оракулы пересчитаны аналитически.
3. Интерактивный/headless glgame: 16 ms schedule; 20 s → 625 ticks
   (если тест объявляет 16 ms); glview NPC-move oracle обновлён.
4. Полный `ctest` с `GL_GAME_DIR` — 100% passed.
5. Docs: D-068, STATUS/PROJECT/parity, research update; D-021/D-067 история.

## 8. Risks

- U1 host frame time: порт использует переданный delta; parity по гейту,
  не по точному wall-clock.
- Pause semantics не доказана; текущий «no backlog» — port choice,
  документируется.
- Float/step oracle accumulation: пересчёт только A/R/L; никаких
  «подогнать под порт».
- Много тестов/скриншотов: план разбивает на core → game → app/render,
  full ctest в конце.

## 9. References

- `docs/research/p2-9-35hz.md` (T1–T9, C1–C8, P1–P29, impact matrix).
- `docs/specs/2026-11-12-p2-9-35hz-research-design.md`.
- `docs/DECISIONS.md` D-021/D-033/D-067.
