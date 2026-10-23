# P4b: движение NPC по маршрутам — дизайн

## 1. Цель

NPC на уровне самостоятельно перемещаются по маршрутам из SEF, как в оригинале:
`MOVED`, `MOVED_FLIP`, `RANDOM_RADIUS` (и `RANDOM`), с паузами, корректным
направлением, `_go`-анимацией и возвратом в `stay`. Без pathfinding, коллизий,
AI, триггеров и клика (P4c).

## 2. Область

Входит:

- парсинг `cell_groups` из SEF;
- `gl::game::LevelSimulation` — детерминированная симуляция движения;
- загрузка `_go`-листов и рендер движущихся NPC (16 направлений);
- интеграция в `LevelScreen`/`glgame`/`glview`, `--no-persons`;
- тесты, docs, автоматическая приёмка.

Не входит: pathfinding/обходимые препятствия, NPC-NPC коллизии, AI/триггеры,
клик → диалог, бой/торговля/глобальная карта. `STAY`/`STAY_ROTATE` реализованы
в A4 (см. §4.2).

## 3. Данные (подтверждено)

Подробности и RVA — `docs/research/p4b-person-movement.md`.

- `route_type`: `STAY`=0, `STAY_ROTATE`=1, `MOVED`=2, `MOVED_FLIP`=3,
  `RANDOM_RADIUS`=4, `RANDOM`=5.
- `l1_1`: 10 `STAY`, 1 `MOVED`, 3 `MOVED_FLIP`, 2 `RANDOM_RADIUS`.
- Маршрут — `cell_groups` из `l1_1.sef`: `name` + `cell_NN x y` (тайлы).
- Позиция цели: `(x*12+6, y*9+4)` px; прибытие при `|dx|<4 && |dy|<4` px.
- Пауза после прибытия: `d = rand(delay_min..delay_max)`; `d < 500` мс — без паузы.
- Скорость: `movement_x/movement_y` из `_go` PAD (пиксели за обновление),
  `iso_speed = (1-fx²)*move_y + fx²*move_x`, `fx = dx/len`; интервал обновления
  `go.delay * 0.8` (live-измерение, D-033), т.е.
  `px/s = iso_speed * 1000 / (go.delay * 0.8)`. Порт интегрирует шагом 1/60 с;
  оригинал квантует обновления к 35 Гц.
- Направление: 16 кодов 10..25 по порогам `0.2079/0.5878/0.8616/0.9863`;
  row — `kPadPersonRows16[code-10]`, зеркало — как в P4a. В покое — 8 направлений.
- `radius == 0` → 13 тайлов (как в оригинале).

## 4. Архитектура

```
assets: SEF cell_groups
game:   LevelSimulation (SDL-free), PersonState, route/direction helpers
app:    LevelScreen владеет симуляцией, грузит stay+go, рисует
```

### 4.1 `gl_assets`

`SefCellGroup { name; vector<pair<uint16_t,uint16_t>> cells; }`,
`SefData::cell_groups`. Парсер принимает `cell_00..cell_NN` в любом порядке,
значения — тайлы; пустая группа допустима.

### 4.2 `gl_game`

```cpp
enum class RouteType : uint8_t { stay=0, stay_rotate=1, moved=2, moved_flip=3,
                                 random_radius=4, random=5 };
RouteType parse_route_type(std::string_view);        // неизвестное -> stay (warning отложен)

struct PersonMovement {
    float x = 0, y = 0;          // px
    float target_x = 0, target_y = 0;
    std::uint8_t direction8 = 0;
    std::uint8_t direction16 = 0;
    bool moving = false;
    std::uint64_t wait_until_ms = 0;
    bool waiting = false;
    std::uint64_t state_started_ms = 0;   // начало текущего go/stay
};

class LevelSimulation {
public:
    static LevelSimulation create(const Level&, std::uint64_t seed);
    void advance(double dt_seconds);                    // шаг 1/60 с
    const std::vector<PersonMovement>& persons() const;
};
```

- Маршрутные клетки копируются в `PersonInstance::route_cells` при загрузке.
- Скорость на тик: `iso_speed(...) * 1000 / (go.delay * 0.8)`; шаг интеграции
  фиксирован 1/60 с (D-033).
- RNG вынесен в `gl::game::MsvcRandom` (`src/game/include/gl/game/msvc_random.hpp`):
  `next()`, `fraction()`, `range(min,max)`, `coin(p)`; `LevelSimulation` хранит его
  приватно, публичных `next_random`/`random_range` нет.
- RNG — один глобальный поток MSVC CRT `rand` (`state = state*0x343FD + 0x269EC3`,
  `(state>>16)&0x7FFF`); `create(..., seed)` задаёт начальное состояние, в production
  `seed = 1` (начальное состояние CRT без `srand`). `range(min,max)`:
  `max<=min -> min`, иначе `min + round((max-min+0.99999)*rand/32767.0)` (возможен
  `max+1` при `rand=32767`); `coin(p)` — `p<=0 -> false`, `p>1 -> true`, иначе
  `fraction() < p`.
- Порядок вызовов: при создании сначала бросается пауза (`range(delay_min,delay_max)`);
  только при `<500` выбирается цель. На прибытии/после паузы — та же последовательность:
  пауза до выбора цели. `MOVED_FLIP` делает `index += coin(0.5) ? +1 : -1` после
  выбора клетки; `RANDOM*` — две выборки на попытку.
- Индекс маршрута (`MOVED`/`MOVED_FLIP`) сдвигается в момент выбора цели, до
  планирования пути и движения: `MOVED` — `(index+1) % count`, `MOVED_FLIP` —
  `coin(0.5) ? +1 : -1` после нормализации `index` в `[0,count)`. `consume_goal`
  индекс не меняет. Если планирование к только что выбранной клетке не удалось —
  `has_goal` сбрасывается и следующая попытка берёт следующую клетку (заблокированная
  клетка пропускается); mid-path replan к занятой клетке сохраняет цель (D-031).
- Для `RANDOM*` неудачное планирование пути (первая же неудача) сбрасывает
  `has_goal`, и следующая попытка выбирает новую случайную цель: счётчика из
  3 неудач нет (D-032).
- `STAY` (A4, D-035): после гейта паузы, если текущая клетка равна spawn — ничего
  не делает (без случайного блуждания); если смещена — строит путь к spawn через
  LOS/A*/occupancy и по прибытии снова `STAY`. Проверка «в клетке spawn» — по
  тайлам (`+0x604/+0x608` vs `+0x5f8/+0x5fc`).
- `STAY_ROTATE` (A4, D-035): после гейта при `d >= 500` мс ждёт; иначе обновляет
  направление из `rng_.range(10,25)`: `direction16 = min(d,25)-10` (0..15),
  `direction8 = direction16_to8(...)`, и ставит паузу **500 мс** порта
  (`kRotatePauseMs`). NPC не двигается. Длительность действия/анимация
  `SET_DIRECTION` в оригинале не извлечена; `+0x600` не моделируется. Код 26
  (`range` может дать `max+1`) ограничивается до 25.
- Нет клеток у `MOVED*` или нет `_go` у route-типа — `STAY` + warning; `RANDOM*`
  маршрутных клеток не требуют.
- `radius <= 0` → 13; для `RANDOM*` выборка `rand(-r..r)` по обеим осям до
  клетки в границах карты и проходимой по `WalkGrid` (занятость другими NPC не
  учитывается), без лимита попыток. Базовая клетка вне границ возвращается без
  изменений (без выборок). Технический предохранитель — 1'000'000 выборок с
  warning и откатом на базовую клетку (защита от вырожденной карты, не
  поведение оригинала).
- `delay_min > delay_max` — обмен; отрицательные → 0.

### 4.3 `gl_app`

- `LevelScreen` при загрузке: для каждого NPC грузит `_go` PAD/лист (`rt_go`,
  fallback `tb_go`) и тень; хранит текстуры stay/go отдельно.
- `render(renderer, options, simulation_time_ms)` продвигает симуляцию
  фиксированными шагами 1/60 до `simulation_time_ms` (накопленный остаток
  сохраняется); пауза `glgame` замораживает время.
- Движущийся NPC: позиция из `PersonMovement`; кадр
  `((sim_time_ms - state_started_ms) / go.delay) % frames_per_row`; row/mirror по
  `direction16`; тень — по `kPadShadowRows16`, не зеркалится.
- Стоящий: та же позиция симуляции (центр клетки `tile*12+6`, `tile*9+4`),
  stay-лист (`stay_type`) и 8 направлений (`direction8`); draw origin —
  `(x - anchor_x, y - anchor_y)` от центра клетки.
- `--no-persons`: не грузить `_go`-листы, не симулировать, не рисовать.
  `--sim-ms` влияет только на screenshot-режим, в интерактивном цикле игнорируется.

## 5. Тесты

Unit (`gl_game_tests`):

- `parse_route_type` — все 6 строк + неизвестная.
- SEF `cell_groups` — синтетика, порядок/пустая группа/битые клетки.
- Симуляция с фиксированным seed: `MOVED` цикл; `MOVED_FLIP` ±1; пауза <500 мс;
  пауза >=500 мс; `RANDOM_RADIUS`/`RANDOM` в границах и вокруг нужной точки;
  прибытие 4 px; диагональная скорость; 16→8 направления (все коды 10..25).
- `STAY`/`STAY_ROTATE` (A4): `STAY` в spawn не двигается; смещённый `STAY`
  возвращается точно в spawn и останавливается; `STAY_ROTATE` меняет
  `direction16/direction8` по `rand(10,25)` каждые 500 мс и никогда не двигается.
- `LevelSimulation` не зависит от SDL.

Real (`GL_GAME_DIR`):

- `l1_1`: 16 NPC; 6 не-`STAY` по типу (4 route-followers + 2 `RANDOM_RADIUS`),
  10 `STAY`; за 60 с все подвижные остаются в границах, у movers есть `_go`;
  детерминизм повторного прогона (одинаковые позиции). В конкретный момент часть
  подвижных может быть в паузе: при seed 1 в t=60 с 2 moving + 4 waiting; счётчики
  зависят от порядка бросков RNG (A1/D-030).
- Все `cell_groups` из `l1_1.sef` находятся.

Screenshots:

- `l1_1` t=0 vs t=10 с: изображения различаются, 1280x720, 16 persons,
  `--no-persons` статичен.

## 6. Критерии готовности

1. На `l1_1` 6 подвижных (4 route-followers: `MOVED`/`MOVED_FLIP` + 2
   `RANDOM_RADIUS`), 10 `STAY`; направления и анимации соответствуют данным.
2. Детерминизм: два прогона с одинаковым временем дают одинаковые позиции.
3. Скорость/паузы соответствуют формулам §3 (тесты).
4. `gl_game`/`gl_assets` SDL-free; полный `ctest` зелёный.
5. Ручной гейт: в `glgame --level l1_1` видно движение NPC; подтверждено
   пользователем.

## 7. Риски

- Частота обновления оригинала квантуется к 35 Гц; live-измерение дало
  множитель 0.8 к `_go.delay` (D-033). В порте шаг 1/60 с; при дальнейшей сверке
  учитывать квантование.
- У части NPC может не быть `_go`; они останутся стоять (warning).
- Память: +1–2 листа на NPC; для Android позже — ленивая загрузка.

## 8. Отложено

`RANDOM` (за пределами выбора цели), pathfinding/коллизии, AI, триггеры,
клик → диалог, Y-сортировка (P4c/позже).
