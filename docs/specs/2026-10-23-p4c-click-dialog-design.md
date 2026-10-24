# P4c: клик → hero → диалог — дизайн

## 1. Цель

Добавить в порт минимального управляемого hero и первый интерактивный сценарий:
клик по talkable NPC → подход → локальный диалог поверх уровня.

Оригинал: `Client.dll` v1.3 (`FBF213B4…AB5C16`), `Server.dll` v1.3
(`E3E0A262…`). Основание — `docs/research/p4b2-render-order.md` (P4c),
`docs/STATUS.md`, live-проверка: hero «Вертас» — отдельная сущность, визуал
собирается генератором/пресетами, отдельного `persons\hero` PAD нет.

## 2. Поведение

- При загрузке `l1_1` hero появляется на первом walkable `points_entrance`.
- Hero рисуется маркером (ромб/круг) и участвует в Y-sort и foreground-масках
  как обычный мировой объект.
- ЛКМ по NPC:
  - если `can_interact(hero, npc)` уже true — сразу открывается локальный диалог;
  - иначе строится путь к **текущей** клетке NPC (`movement.x/y` через
    `tile_at`, не SEF spawn); если NPC сместился до прибытия, цель
    пересчитывается; диалог открывается, когда `can_interact` становится true.
- ЛКМ по земле: hero идёт в выбранную свободную клетку.
- ПКМ или клик по недостижимой клетке: цель сбрасывается.
- Диалог — overlay: уровень остаётся отрисованным, панель диалога рисуется
  сверху. `Esc` закрывает диалог и возвращает в уровень.
- В диалоге доступны ответы, навигация Up/Down/Enter и мышь; логика —
  существующий `DialogSession`. Пока диалог/save slots активны, WASD/стрелки
  и wheel не панорамируют/не зумируют уровень: ввод принадлежит overlay.
- NPC без непустого `scr_dialog` не является talkable: клик по нему не ставит
  цель и не открывает диалог (только warning в debug-логе).

## 3. Границы

Входит:

- парсинг SEF `points_entrance` и хранение в `Level`;
- `Hero`/`HeroController` в `gl::game` (позиция, путь, цель, `can_interact`);
- hit-test NPC и клик в `LevelScreen`/`glgame`;
- маркер hero, Y-sort и маски;
- resolver `scr_dialog` → `scripts/dialogs/<...>.cs`;
- `DialogOverlay` поверх уровня;
- unit/synthetic/real/screenshot-тесты и docs.

Не входит:

- серверный action `0x12`/`OpenDialog` и сетевой слой;
- бой, торговля, инвентарь, скиллы, save/load hero;
- точный PAD/генератор внешности Вертаса (отдельная задача);
- camera-follow (камера остаётся управляемой как сейчас);
- точная 6-осевая формула `CanInteract` оригинала (см. §6).

## 4. Архитектура

### 4.1 SEF `points_entrance`

`gl::assets::SefData` получает:

```cpp
struct SefEntrance {
    std::string name;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint8_t direction = 0;
};
std::vector<SefEntrance> entrances;
```

Парсер читает top-level `points_entrance`, для каждой именованной записи —
`position X Y` и `direction` (строки направлений уже отображаются в 0..7).
Остальные поля записи пропускаются. `Level` получает
`std::vector<SefEntrance> entrances`.

Hero spawn: первый `entrance`, чья клетка `walkable.is_walkable(x,y)`; если
таких нет — первый entrance; если нет и entrance — центр walkable-области.
Отсутствие `points_entrance` — warning, уровень продолжает грузиться.

### 4.2 Hero и pathfinding

Новый `gl::game::Hero`:

```cpp
struct Hero {
    float x = 0, y = 0;
    Tile tile{};
    Tile goal{};
    bool has_goal = false;
    std::vector<Tile> path;
    std::size_t path_index = 0;
    std::uint8_t direction = 0;
};
```

`HeroController`:

```cpp
HeroController(const Level&, const LevelSimulation&);
void spawn_at_first_entrance();
void set_goal(Tile);
void advance(double dt_seconds);
bool can_interact(const PersonInstance&, const PersonMovement&) const;
const Hero& hero() const;
```

- `set_goal` ищет путь через `gl::game::find_path(level.walkable, from, to,
  blocked)`, где `blocked` — клетки NPC из `simulation.persons()` (кроме
  целевой клетки NPC) и границы уровня.
- Если цель занята NPC, ищется ближайшая свободная клетка вокруг неё
  (радиус 1..3, порядок 8 направлений, затем по возрастанию расстояния).
- Если целевая клетка сама непроходима (`WalkGrid`), цель отклоняется:
  hero не ищет обход для клика по стене/непроходимой земле.
- Если цель проходима, но `find_path` не нашёл путь (изолированная область),
  цель отклоняется. Fallback-поиск соседней клетки применяется только когда
  цель занята NPC, а не для произвольного отсутствия пути.
- Для клика по NPC цель — текущая клетка `PersonMovement`; при изменении
  клетки NPC до прибытия pending talk пересчитывает цель, а при неудаче
  сбрасывается.
- Движение — по сегментам пути, скорость как у NPC (`iso_speed`); `advance`
  обновляет `x/y`, `tile`, `direction`, `path_index`.
- `can_interact` — AABB-проверка: мировой прямоугольник NPC (PAD-кадр текущей
  анимации: `frame_width/frame_height`, anchor) пересекается с квадратом
  маркера hero (`kHeroMarkerSize = 20 px`).
- Порядок кадра: сначала `LevelSimulation::advance` (NPC), затем
  `HeroController::advance`; occupancy и `can_interact` используют уже
  обновлённые позиции NPC.
- При завершении пути (`path.empty()`) hero остаётся в последней клетке.
  Если после прибытия `can_interact` всё ещё false (цель была занята/обход),
  контроллер делает ещё один шаг к ближайшей свободной клетке в радиусе 1;
  если и это не дало контакта — цель сбрасывается, warning, диалог не
  открывается.

### 4.3 Клик и hit-test

`LevelScreen` получает:

```cpp
struct ClickTarget {
    std::optional<std::size_t> person;
    std::optional<Tile> tile;
};
ClickTarget click_target(int sx, int sy) const;
bool world_to_tile(int sx, int sy, Tile& out) const;
```

- `click_target` — единая точка входа: сначала `person_at_screen`, затем
  `world_to_tile`; `glgame` выбирает NPC или клетку.
- `world_to_tile`: `world = camera + screen / zoom`, затем `tile_at`.
- `person_at_screen`: для каждого NPC берётся его PAD-кадр (`stay_type`,
  `place_person_px`), строится экранный прямоугольник; проверяется точка.
  Приоритет — ближайший к курсору/больший baseline; стабильный порядок.
- `glgame` различает клик и drag: нажатие запоминает точку; если движение
  меньше `4 px`, на отпускании выполняется click, иначе — pan.
- Клик по NPC: `hero_.set_goal` не нужен, если `can_interact`; иначе
  `HeroController` получает цель = клетка NPC.
- Клик по земле: `hero_.set_goal(tile)`.

### 4.4 Диалог

`dialog_script_path`:

```cpp
std::optional<std::string> dialog_script_path(const PersonInstance&);
```

- пустой `person.dialog` → `nullopt`;
- нормализовать `\` → `/`, убрать ведущий `/`;
- `scripts/dialogs/` + значение + `.cs` (если `.cs` уже есть — не дублировать);
- проверить `vfs.exists`; отсутствие — warning и `nullopt`.

`DialogOverlay` в `gl::app`:

```cpp
struct DialogOverlayStyle { int panel_x, panel_y, panel_w, panel_h; ... };
class DialogOverlay {
    void open(std::string speaker, std::string text,
              std::vector<std::string> answers);
    void close();
    bool active() const;
    std::optional<std::size_t> hit_test(int sx, int sy) const;
    void render(SDL_Renderer*, const DialogOverlayStyle&) const;
};
```

`glgame` при клике:

1. `open_dialog(path)` — существующий `DialogSession`.
2. Заполняет `DialogOverlay` `npc_text()`/`answers()`.
3. `render`: сначала `LevelScreen`, затем overlay; `render_dialog` больше не
   очищает экран и не заменяет уровень.
4. `Esc`/close: `DialogSession` уничтожается, `DialogOverlay::close`, состояние
   остаётся `level`.

## 5. Ошибки

- Нет `points_entrance`/walkable — hero в центре, warning.
- Нет пути — цель сбрасывается, warning; диалог не открывается.
- Нет `scr_dialog`/CS/SDB — NPC не talkable; warning, без краша.
- Битый PAD/кадр — NPC пропускается для hit-test; warning.
- Диалог не стартовал — overlay не открывается; ошибка в лог.
- Клик вне уровня/камеры — игнорируется.

## 6. Приближение `CanInteract`

Оригинал `Client.dll` `0x120b2fb8` проверяет не фиксированный радиус, а
габариты кадров: hero-кадр против порогов NPC `+0x34..+0x4c` по 6 осям.
Для маркера точные 6 направлений не воспроизводимы. Порт использует AABB
маркера и PAD-кадра NPC с допуском 0. Это **data-driven** приближение:
остановка зависит от размера спрайта NPC, а не от константы 90 px.
Отличие фиксируется в `docs/DECISIONS.md`; точная 6-осевая формула — отдельная
задача паритета при появлении hero PAD.

## 7. Тесты

### Unit

- `parse_sef` synthetic: `points_entrance` с 2 записями, `position`,
  `direction`, пустой блок.
- `parse_sef` real `l1_1`: 5 entrances, `L1_1_e_L1_1_1` = (75,55),
  `GM` = (175,206).
- `HeroController`: spawn на первом walkable; путь по прямой; обход
  заблокированной клетки; занятая цель → ближайшая свободная; `advance` до
  конца пути; `can_interact` true/false на AABB.
- `LevelScreen`/helper: `world_to_tile` round-trip; `person_at_screen` для
  известного кадра.
- `dialog_script_path`: пустой, обычный, с `\`, уже с `.cs`, отсутствующий файл.

### Synthetic / E2E

- `glgame --level l1_1 --click-npc <index> --screenshot <png>`:
  путь выполняется, overlay открывается, PNG 1280×720, exit 0.
- `--click-world <x> <y>`: hero идёт в клетку; без NPC overlay не открывается.
- `--no-dialog`/no path: цель сбрасывается, overlay отсутствует.
- Screenshot: уровень виден под панелью диалога (не заменён фоном).

### Real data

- `l1_1`: hero spawn; 16 NPC; путь к `L1.P621_Svet_3CircusWorkerV` и открытие
  `scripts/dialogs/l1_1/l1.p621_svet_3circusworkerv.d621.age.cs`.
- Отсутствующий/битый dialog script — warning, NPC не talkable.

### Команды

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R "p4c|real" --output-on-failure
```

`GL_GAME_DIR`-зависимые тесты возвращают 77 без каталога игры.

## 8. Приёмка

- `cmake --build build` — успех.
- Targeted P4c-тесты — зелёные.
- Полный `ctest` с `GL_GAME_DIR` — зелёный.
- Real `l1_1`: hero появляется, клик по `P621` ведёт к нему, overlay
  открывается; `Esc` возвращает в уровень.
- Screenshot: под панелью диалога виден уровень; координаты/масштаб не ломаются.

## 9. Риски и ограничения

- Точная 6-осевая формула `CanInteract` не воспроизведена; AABB может дать
  остановку чуть раньше/позже оригинала.
- Визуал hero — маркер, не PAD Вертаса; точный арт — отдельная задача.
- Оригинальный client-side `0x4f`/`0x52` и local dialog path подтверждены
  статически, но полный сетевой `0x12` не реализуется.
- Pathfinding hero использует текущие позиции NPC как препятствия; динамические
  двери/скриптовые препятствия вне P4c.
- Камера не следует за hero; при длинном пути пользователь панорамирует вручную.
