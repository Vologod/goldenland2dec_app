# P4 — RNG parity (D-053 implementation) design

Дата: 2026-11-07. Статус: approved (пользователь, 2026-11-07).

## 1. Цель

Привести `gl::game::MsvcRandom` к семантике `Server.dll`, доказанной в
`docs/research/p3b2b2-npc-loot-server.md` §9 и D-053:

- `range(lo,hi)` — усечение (`fistp`, RC=`0xC00`), а не округление;
- `coin(p)` — float-арифметика и float-сравнения;
- production seed — из `timeGetTime`-эквивалента, а не жёсткий `1`.

## 2. Контекст

- `Server.dll` v1.3, SHA-256
  `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- D-030 заменён D-053: оригинал вызывает `srand(FUN_1406c092(timeGetTime()))`
  в `FUN_1402fdc0` (`decompiled.c:34129`); `FUN_140061e4` (`0x61e4`)
  усекает; `FUN_14006288` (`0x6288`) считает в float.
- Текущий порт (`src/game/src/msvc_random.cpp`):
  - `range` — `std::nearbyint`;
  - `coin` — `double`;
  - production seed — `1` (`src/app/src/level_screen.cpp:449`).
- RNG используется движением NPC (`FUN_1401c2a0`), `STAY_ROTATE`
  (`rand(10,25)`) и loot quantity.

## 3. Scope

### 3.1 `MsvcRandom`

```
range(lo,hi):
    if hi <= lo: return lo
    return lo + trunc(((hi-lo) + 0.99999) * (double)next() * (1.0/32767))

coin(p):
    pf = (float)p
    if pf <= 0.0f: return false
    if pf >  1.0f: return true
    return (float)next() * (1.0f/32767.0f) < pf
```

- `next()`/`fraction()` не меняются.
- `range` использует double-константы; `static_cast<int32_t>` даёт
  усечение к нулю (значение всегда неотрицательное).
- `coin` использует float-константы; `p` приводится к float.
- `static std::uint32_t production_seed() noexcept` — миллисекунды
  `std::chrono::steady_clock` (эквивалент `timeGetTime`), приведённые к
  `uint32_t`.

### 3.2 Production seed

- `LevelScreen` создаёт `LevelSimulation::create(level,
  MsvcRandom::production_seed())` вместо `1`.
- Тесты продолжают передавать явный seed для детерминизма.

### 3.3 Ограничения (фиксируются в D-054)

- Оригинальный `timeGetTime` недетерминирован между запусками; точное
  совпадение последовательностей с оригиналом не является целью.
- Оригинальный RNG — глобальный поток, переживающий загрузку уровней; в
  порте `LevelSimulation` создаётся заново и получает новый seed.
  Непрерывность потока между уровнями не реализуется в этой задаче.
- NPC-лут specials/`DAT_14088df4`, Transfer/Trade — вне scope.
- Port-side anti-livelock guard (Task 2, D-054): occupancy-abort движущегося
  `RANDOM`/`RANDOM_RADIUS` сбрасывает цель, ставит паузу ≥500 мс и делает
  дополнительный `range()` draw. Это защита порта, **не паритет**: поведение
  occupancy-abort в `Server.dll` не извлечено; D-032/D-034 этот случай не
  покрывают. Запись расширяется RNG seed/stream ограничениями в Task 4.

## 4. Non-goals

- NPC `.inv` генерация и specials.
- Точное воспроизведение seed по времени.
- Изменение формул движения/поведения сверх RNG.

## 5. Тестирование

Unit (`tests/game/test_msvc_random.cpp`):

- `range` — эталонная формула с truncation на фиксированном seed
  (несколько пар, включая `hi == lo`, `hi < lo`, границы `32767`);
- `coin` — `p<=0`, `p>1`, `p=1` (вырожденный случай), float-поведение;
- `production_seed` — не падает, возвращает `uint32_t` (не проверять
  конкретное значение).

Regression:

- обновить ожидания `test_level_simulation.cpp` / `test_person_movement`
  и `STAY_ROTATE` под truncation (не ослаблять проверки — пересчитать
  ожидаемые значения по эталонной формуле);
- full `ctest` с `GL_GAME_DIR`.

## 6. Критерии приёмки

1. `MsvcRandom` совпадает с research §9.2/§9.3 на фиксированном seed.
2. Production seed — clock-based; тесты детерминированы.
3. Обновлённые movement/STAY_ROTATE тесты и full `ctest` pass.
4. D-054 + `STATUS`/`PROJECT`/`parity-matrix` обновлены; ограничения
   seed/stream записаны.
5. Документы UTF-8 без BOM/NUL.

## 7. Риски

- Truncation меняет ожидаемые значения существующих movement-тестов;
  пересчёт должен опираться на эталонную формулу, а не подгонку.
- `production_seed` на разных платформах даёт разные значения — это
  ожидаемо (как `timeGetTime`).
- `coin(p=1)` может вернуть false при `rand()==32767` — это поведение
  оригинала, а не баг.
