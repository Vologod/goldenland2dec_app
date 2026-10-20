# Design: P2b — Deterministic tick and Server/Client boundary

Статус: **утверждён**.
Дата: 2026-10-20.
Базируется на: `docs/specs/2026-10-20-p2a-game-shell-design.md`, `docs/DECISIONS.md`.

## 1. Цель

Сделать время и симуляцию детерминированными: `ServerCore` тикает с фиксированным шагом,
хранит номер тика и время симуляции, применяет команды на границе тика; `ClientCore`
получает снимок состояния; `glgame` рисует анимации по времени сервера, а пауза
останавливает симуляцию.

P2b — фундамент для игровой логики P3+ (NPC, бой, скрипты), где важен детерминизм.

## 2. Границы

Входит:

- Фиксированный шаг **60 Гц** (`1/60 с`), аккумулятор, ограничение `dt` (0.25 с) и
  максимум шагов за кадр (15) против «spiral of death».
- `ServerCore`: `tick_index()`, `simulation_time_ms()`, `snapshot()`; команды применяются
  в начале тика; пауза замораживает время.
- `ClientCore`: приём `ServerSnapshot`, доступ к нему для рендера/UI.
- `glgame`: цикл использует `server.tick(dt)` и время сервера для анимаций; убрать
  ручной `animation_ms_`.
- Тесты детерминизма/паузы/границ.

Не входит:

- Игровые системы/скрипты (P3+), сеть/сокеты, интерполяция движения (появится с
  движущимися сущностями), save/load (P2c).

## 3. Модель времени

- `kTickHz = 60`, `kFixedDt = 1.0/60`, `kMaxFrameDt = 0.25`, `kMaxStepsPerTick = 15`.
- `tick(dt)`:
  1. обработать накопленные команды (один раз за внешний тик);
  2. если пауза — `accumulator = 0`, выйти (команда `gv_pause 0` уже применена);
  3. `dt = clamp(dt, 0, kMaxFrameDt)`; `accumulator += dt`;
  4. `while (accumulator >= kFixedDt && steps < kMaxStepsPerTick)`: `++tick_index_`,
     `accumulator -= kFixedDt`, `++steps`; при достижении лимита `accumulator = 0`.
- `simulation_time_ms = tick_index_ * 1000 / 60` (целочисленно, детерминированно).
- `ServerSnapshot { bool paused; bool free_camera; uint64_t tick_index; uint64_t simulation_time_ms; }`.
- `ClientCore::on_snapshot(const ServerSnapshot&)`, `snapshot()`.

## 4. Интеграция в glgame

- Цикл: `delta_ms` (кап 250) → `server_.tick(dt)`; `client_.on_snapshot(server_.snapshot())`;
  обработка событий из `client_.events()`.
- Анимации уровня: `LevelScreen::render(..., client_.snapshot().simulation_time_ms)`.
- Пауза: `gv_pause` → сервер; при паузе время не растёт → анимации стоят.
- `--screenshot`: детерминированное время `frames * 16` (как в P1), независимо от tick.
- `gv_free_camera`: остаётся runtime-флагом в snapshot.

## 5. Тестирование

Unit (`gl_core`):

- `dt=0.25` → 15 тиков, `simulation_time_ms=250`; `dt=1.0` клампится к 0.25 и даёт 15 тиков.
- дробные `0.01 × 60` (суммарно 0.6 с) → 36 тиков (накопление, без потерь внутри лимита).
- `dt > 0.25` → не более 15 тиков и сброс аккумулятора.
- команда, отправленная до `tick`, не применяется до тика; события — один раз.
- детерминизм: одинаковая последовательность `dt` → одинаковые `tick_index/simulation_time`.
- `ClientCore` snapshot mirror.

Integration (`glgame`): существующие screenshot-тесты зелёные; пауза останавливает
`simulation_time_ms` (unit-проверка); ручной прогон — анимации идут/стоят.

## 6. Критерии готовности

1. `ctest` зелёный (unit + real-data + app).
2. `glgame`: анимации идут; `gv_pause 1` останавливает, `gv_pause 0` продолжает.
3. Детерминизм-тесты проходят.
4. `gl_core` SDL-free; `glgame` использует время сервера.
5. Документы обновлены (`STATUS`, `PROJECT`, `DECISIONS` D-021 для 60 Гц/лимитов).

## 7. Риски

- Аккумулятор с `double` может давать 59/61 тик на границах — тесты используют
  допуск/точные последовательности; шаг и время считаются от `tick_index`.
- Пауза и команды: команда `gv_pause 0` должна применяться до проверки паузы.
- Длинные кадры: ограничение 15 шагов и сброс аккумулятора.

## 8. Отложено

Интерполяция, сети, P3+ системы, P2c save/load.
