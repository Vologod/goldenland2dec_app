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
клик → диалог, `STAY_ROTATE`, бой/торговля/глобальная карта.

## 3. Данные (подтверждено)

Подробности и RVA — `docs/research/p4b-person-movement.md`.

- `route_type`: `STAY`=0, `STAY_ROTATE`=1, `MOVED`=2, `MOVED_FLIP`=3,
  `RANDOM_RADIUS`=4, `RANDOM`=5.
- `l1_1`: 10 `STAY`, 1 `MOVED`, 3 `MOVED_FLIP`, 2 `RANDOM_RADIUS`.
- Маршрут — `cell_groups` из `l1_1.sef`: `name` + `cell_NN x y` (тайлы).
- Позиция цели: `(x*12+6, y*9+4)` px; прибытие при `|dx|<4 && |dy|<4` px.
- Пауза после прибытия: `d = rand(delay_min..delay_max)`; `d < 500` мс — без паузы.
- Скорость: `movement_x/movement_y` из `_go` PAD (px за кадр), пересчёт
  `px/s = movement * 1000 / go.delay`; диагональ
  `speed = (1-fx²)*move_y + fx²*move_x`, `fx = dx/len` (D-026).
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
RouteType parse_route_type(std::string_view);        // неизвестное -> stay + warning

struct PersonMovement {
    float x = 0, y = 0;          // px
    float target_x = 0, target_y = 0;
    std::uint8_t direction8 = 0;
    std::uint8_t direction16 = 0;
    bool moving = false;
    std::size_t route_index = 0;
    std::uint64_t wait_until_ms = 0;
    bool waiting = false;
    std::uint32_t rng = 1;       // детерминированный ГПСЧ (xorshift32)
};

class LevelSimulation {
public:
    static LevelSimulation create(const Level&, std::uint64_t seed);
    void advance(double dt_seconds);                    // шаг 1/60 с
    const std::vector<PersonMovement>& persons() const;
};
```

- Маршрутные клетки копируются в `PersonInstance::route_cells` при загрузке.
- RNG — xorshift32; seed = `hash(level.name) ^ hash(person.name) ^ index`.
  Начальное состояние ненулевое; детерминизм по уровню/индексу.
- `STAY` не двигается; если позиция смещена — возврат к spawn (на будущее).
- `STAY_ROTATE` в P4b = `STAY` + однократный warning на уровень.
- Нет маршрута/клеток у `MOVED*`, нет `_go` — `STAY` + warning.
- `radius <= 0` → 13; цель клампится в `[0,width-1] x [0,height-1]` клетки;
  для `RANDOM*` до 64 попыток, затем spawn/текущая позиция.
- `delay_min > delay_max` — обмен; отрицательные → 0.

### 4.3 `gl_app`

- `LevelScreen` при загрузке: для каждого NPC грузит `_go` PAD/лист (`rt_go`,
  fallback `tb_go`) и тень; хранит текстуры stay/go отдельно.
- `render(renderer, options, simulation_time_ms)` продвигает симуляцию
  фиксированными шагами 1/60 до `simulation_time_ms` (накопленный остаток
  сохраняется); пауза `glgame` замораживает время.
- Движущийся NPC: позиция из `PersonMovement`; кадр
  `(anim_ms / go.delay) % frames_per_row`; row/mirror по `direction16`; тень — по
  `kPadShadowRows16`, не зеркалится.
- Стоящий: прежняя логика P4a (`stay_type`, 8 направлений).
- `--no-persons`: не грузить `_go`-листы, не симулировать, не рисовать.

## 5. Тесты

Unit (`gl_game_tests`):

- `parse_route_type` — все 6 строк + неизвестная.
- SEF `cell_groups` — синтетика, порядок/пустая группа/битые клетки.
- Симуляция с фиксированным seed: `MOVED` цикл; `MOVED_FLIP` ±1; пауза <500 мс;
  пауза >=500 мс; `RANDOM_RADIUS`/`RANDOM` в границах и вокруг нужной точки;
  прибытие 4 px; диагональная скорость; 16→8 направления (все коды 10..25).
- `LevelSimulation` не зависит от SDL.

Real (`GL_GAME_DIR`):

- `l1_1`: 16 NPC; за 60 с 4 движущихся, 10 неподвижных; позиции в границах;
  у movers есть `_go`; детерминизм повторного прогона (одинаковые позиции).
- Все `cell_groups` из `l1_1.sef` находятся.

Screenshots:

- `l1_1` t=0 vs t=10 с: изображения различаются, 1280x720, 16 persons,
  `--no-persons` статичен.

## 6. Критерии готовности

1. На `l1_1` 4 NPC ходят по маршрутам, 10 стоят; направления и анимации
   соответствуют данным.
2. Детерминизм: два прогона с одинаковым временем дают одинаковые позиции.
3. Скорость/паузы соответствуют формулам §3 (тесты).
4. `gl_game`/`gl_assets` SDL-free; полный `ctest` зелёный.
5. Ручной гейт: в `glgame --level l1_1` видно движение NPC; подтверждено
   пользователем.

## 7. Риски

- Частота обновления оригинала статически не закреплена; принята единица
  «px за кадр анимации» (D-026). При сверке с оригиналом — один множитель.
- У части NPC может не быть `_go`; они останутся стоять (warning).
- Память: +1–2 листа на NPC; для Android позже — ленивая загрузка.

## 8. Отложено

`STAY_ROTATE`, `RANDOM` (за пределами выбора цели), pathfinding/коллизии, AI,
триггеры, клик → диалог, Y-сортировка (P4c/позже).
