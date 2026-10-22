# P4b2: коллизии и pathfinding NPC — план

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** NPC на `l1_1` не проходят сквозь препятствия и друг друга: сетка проходимости из LVL, прямая ходьба при свободной линии, иначе A*-обход.

**Architecture:** `gl::game::WalkGrid` в `Level` (строит `load_level` из `LvlData`); `gl::game::Pathfinder` (line-of-sight + A*); `LevelSimulation` использует препятствия и occupancy; `gl_app` даёт `--walkable-overlay`.

**Tech Stack:** C++20, CMake+Ninja, doctest, SDL2, LLVM-MinGW.

## Global Constraints

- C++20, `-Wall -Wextra -Werror`, без лишних комментариев.
- `gl_assets`/`gl_game` SDL-free; слои только вниз.
- Проходимость (D-028): `word = chunk[0].relief | (chunk[0].sound<<16)`,
  `sub = (x&1) + 2*(y&1)`, `blocked = (word & (0x100u<<sub)) != 0`; чанки читаются
  column-major (`cx*chunk_height+cy`); пустая сетка = препятствий нет.
- A* 8 направлений, диагональ √2, без срезания углов (диагональ только если обе
  ортогональные клетки свободны); занятые NPC клетки — препятствие.
- Детерминизм и шаг 1/60 сохраняются; P4b-тесты остаются зелёными.
- Данные игры не коммитить; `third_party` не менять.
- Полные прогоны: `$env:GL_GAME_DIR = "I:\Games\Златогорье-2"`.
- Toolchain PATH:
  `$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"`

---

### Task 1: WalkGrid и заполнение из LVL

**Files:**
- Create: `src/game/include/gl/game/walk_grid.hpp`
- Create: `src/game/src/walk_grid.cpp`
- Modify: `src/game/include/gl/game/level.hpp`
- Modify: `src/game/src/level_loader.cpp`
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_walk_grid.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
struct WalkGrid {
    std::int32_t width = 0, height = 0;
    std::vector<std::uint8_t> walkable;   // 1 = проходимо
    bool is_walkable(std::int32_t x, std::int32_t y) const;
    bool empty() const { return width <= 0 || height <= 0; }
};
WalkGrid build_walk_grid(const gl::assets::LvlData& level);
```
- `gl::game::Level` получает `WalkGrid walkable;`.

- [x] **Step 1: Failing tests**

`tests/game/test_walk_grid.cpp`:

```cpp
#include "gl/assets/lvl.hpp"
#include "gl/game/walk_grid.hpp"
#include <doctest/doctest.h>

using namespace gl::game;

TEST_CASE("walk grid maps chunk tiles to world tiles") {
    gl::assets::LvlData lvl;
    lvl.chunk_width = 2;
    lvl.chunk_height = 1;
    lvl.chunks.resize(2);
    // Chunk 0: first tile relief bits 9 and 11 -> sub-tiles 1 and 3 blocked.
    lvl.chunks[0][0].relief = 0x0A00;
    // The remaining tiles of the chunk are ignored.
    lvl.chunks[0][1].relief = 0xFFFF;
    lvl.chunks[0][2].relief = 0xFFFF;
    lvl.chunks[0][3].relief = 0xFFFF;
    // Chunk 1: first tile relief bits 8..11 -> all four sub-tiles blocked.
    lvl.chunks[1][0].relief = 0x0F00;

    const WalkGrid grid = build_walk_grid(lvl);
    CHECK(grid.width == 4);
    CHECK(grid.height == 2);
    CHECK(grid.is_walkable(0, 0));
    CHECK_FALSE(grid.is_walkable(1, 0));
    CHECK(grid.is_walkable(0, 1));
    CHECK_FALSE(grid.is_walkable(1, 1));
    CHECK_FALSE(grid.is_walkable(2, 0));
    CHECK_FALSE(grid.is_walkable(3, 0));
    CHECK_FALSE(grid.is_walkable(2, 1));
    CHECK_FALSE(grid.is_walkable(3, 1));
    CHECK_FALSE(grid.is_walkable(-1, 0));
    CHECK_FALSE(grid.is_walkable(0, 2));
}

TEST_CASE("walk grid stores chunks column-major") {
    gl::assets::LvlData lvl;
    lvl.chunk_width = 2;
    lvl.chunk_height = 2;
    lvl.chunks.resize(4);
    // File order is column-major: index = cx * chunk_height + cy.
    lvl.chunks[0][0].relief = 0x0000; // (cx=0, cy=0)
    lvl.chunks[1][0].relief = 0x0100; // (cx=0, cy=1) -> world (0, 2)
    lvl.chunks[2][0].relief = 0x0000; // (cx=1, cy=0)
    lvl.chunks[3][0].relief = 0x0000; // (cx=1, cy=1)

    const WalkGrid grid = build_walk_grid(lvl);
    CHECK(grid.width == 4);
    CHECK(grid.height == 4);
    CHECK_FALSE(grid.is_walkable(0, 2));
    CHECK(grid.is_walkable(2, 0));
    CHECK(grid.is_walkable(1, 2));
    CHECK(grid.is_walkable(0, 3));
    CHECK(grid.is_walkable(3, 3));
}

TEST_CASE("empty lvl produces empty grid") {
    gl::assets::LvlData lvl;
    CHECK(build_walk_grid(lvl).empty());
}
```

Real test (`tests/game/test_real_person_loader.cpp` или новый):

```cpp
TEST_CASE("real data: l1_1 walk grid" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    CHECK(level->walkable.width == 230);
    CHECK(level->walkable.height == 224);
    std::size_t blocked = 0;
    for (std::uint8_t value : level->walkable.walkable)
        if (value == 0) ++blocked;
    CHECK(blocked == 18028);
}
```

- [x] **Step 2: Run, verify failure** (нет заголовка/поля).

- [x] **Step 3: Implement**

`walk_grid.hpp`:

```cpp
#pragma once
#include "gl/assets/lvl.hpp"
#include <cstdint>
#include <vector>

namespace gl::game {
struct WalkGrid {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::vector<std::uint8_t> walkable;
    bool is_walkable(std::int32_t x, std::int32_t y) const;
    bool empty() const { return width <= 0 || height <= 0; }
};
WalkGrid build_walk_grid(const gl::assets::LvlData& level);
}
```

`walk_grid.cpp`:

```cpp
#include "gl/game/walk_grid.hpp"

namespace gl::game {
bool WalkGrid::is_walkable(std::int32_t x, std::int32_t y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    return walkable[static_cast<std::size_t>(y) * width + x] != 0;
}

WalkGrid build_walk_grid(const gl::assets::LvlData& level) {
    WalkGrid grid;
    if (level.chunk_width == 0 || level.chunk_height == 0) return grid;
    grid.width = static_cast<std::int32_t>(level.chunk_width) * 2;
    grid.height = static_cast<std::int32_t>(level.chunk_height) * 2;
    grid.walkable.assign(static_cast<std::size_t>(grid.width) * grid.height, 0);
    for (std::uint32_t cx = 0; cx < level.chunk_width; ++cx) {
        for (std::uint32_t cy = 0; cy < level.chunk_height; ++cy) {
            const auto& chunk =
                level.chunks[static_cast<std::size_t>(cx) * level.chunk_height + cy];
            const std::uint32_t word = static_cast<std::uint32_t>(chunk[0].relief) |
                                       (static_cast<std::uint32_t>(chunk[0].sound) << 16);
            for (std::uint32_t t = 0; t < 4; ++t) {
                const std::uint32_t tx = cx * 2 + (t & 1u);
                const std::uint32_t ty = cy * 2 + (t >> 1u);
                const std::uint32_t sub = (tx & 1u) + 2u * (ty & 1u);
                const bool blocked = (word & (0x100u << sub)) != 0;
                grid.walkable[static_cast<std::size_t>(ty) * grid.width + tx] =
                    blocked ? 0 : 1;
            }
        }
    }
    return grid;
}
}
```

В `level.hpp`: `#include "gl/game/walk_grid.hpp"` и поле `WalkGrid walkable;`.
В `level_loader.cpp` после разбора LVL: `level.walkable = build_walk_grid(lvl);`.

- [x] **Step 4: Run tests**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*walk grid*"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"; build\bin\gl_game_tests.exe --test-case="*l1_1 walk grid*"
```

- [x] **Step 5: Commit**

```powershell
git add src/game tests/game
git commit -m "feat(game): build walk grid from LVL chunk flags"
```

---

### Task 2: Pathfinder (line of sight + A*)

**Files:**
- Create: `src/game/include/gl/game/pathfinder.hpp`
- Create: `src/game/src/pathfinder.cpp`
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_pathfinder.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Consumes: `WalkGrid`.
- Produces:

```cpp
struct Tile { std::int32_t x = 0, y = 0; };
bool line_of_sight(const WalkGrid& grid, Tile from, Tile to,
                   const std::vector<std::uint8_t>& blocked);
std::vector<Tile> find_path(const WalkGrid& grid, Tile from, Tile to,
                            const std::vector<std::uint8_t>& blocked);
```

- [x] **Step 1: Failing tests**

```cpp
#include "gl/game/pathfinder.hpp"
#include <doctest/doctest.h>

using namespace gl::game;

namespace {
WalkGrid grid_with_wall() {
    WalkGrid grid;
    grid.width = 7;
    grid.height = 5;
    grid.walkable.assign(static_cast<std::size_t>(grid.width) * grid.height, 1);
    for (std::int32_t y = 0; y < 4; ++y)
        grid.walkable[static_cast<std::size_t>(y) * grid.width + 3] = 0;
    return grid;
}
const std::vector<std::uint8_t> kFree;
}

TEST_CASE("line of sight is blocked by walls") {
    const WalkGrid grid = grid_with_wall();
    CHECK(line_of_sight(grid, {0, 4}, {6, 4}, kFree));
    CHECK_FALSE(line_of_sight(grid, {0, 0}, {6, 0}, kFree));
    CHECK(line_of_sight(grid, {2, 2}, {2, 2}, kFree));
}

TEST_CASE("find_path goes around a wall") {
    const WalkGrid grid = grid_with_wall();
    const auto path = find_path(grid, {0, 0}, {6, 0}, kFree);
    REQUIRE_FALSE(path.empty());
    CHECK(path.front() == Tile{0, 0});
    CHECK(path.back() == Tile{6, 0});
    for (const Tile& tile : path)
        CHECK(grid.is_walkable(tile.x, tile.y));
    CHECK(path.size() > 7); // обход длиннее прямой
}

TEST_CASE("find_path returns empty for unreachable target") {
    WalkGrid grid = grid_with_wall();
    for (std::int32_t x = 0; x < grid.width; ++x)
        grid.walkable[static_cast<std::size_t>(4) * grid.width + x] = 0; // вторая стена
    CHECK(find_path(grid, {0, 0}, {0, 4}, kFree).empty());
}

TEST_CASE("blocked cells are obstacles but the start cell is free") {
    const WalkGrid grid = grid_with_wall();
    std::vector<std::uint8_t> blocked(static_cast<std::size_t>(grid.width) * grid.height, 0);
    blocked[static_cast<std::size_t>(2) * grid.width + 1] = 1;
    blocked[static_cast<std::size_t>(2) * grid.width + 2] = 1;
    CHECK_FALSE(line_of_sight(grid, {0, 2}, {4, 2}, blocked));
    const auto path = find_path(grid, {0, 2}, {4, 2}, blocked);
    REQUIRE_FALSE(path.empty());
    for (const Tile& tile : path)
        CHECK(blocked[static_cast<std::size_t>(tile.y) * grid.width + tile.x] == 0 ||
              tile == Tile{0, 2});
}
```

- [x] **Step 2: Run, verify failure.**

- [x] **Step 3: Implement**

`pathfinder.hpp`:

```cpp
#pragma once
#include "gl/game/walk_grid.hpp"
#include <cstdint>
#include <vector>

namespace gl::game {
struct Tile { std::int32_t x = 0, y = 0; };
inline bool operator==(Tile a, Tile b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(Tile a, Tile b) { return !(a == b); }

bool line_of_sight(const WalkGrid& grid, Tile from, Tile to,
                   const std::vector<std::uint8_t>& blocked);
std::vector<Tile> find_path(const WalkGrid& grid, Tile from, Tile to,
                            const std::vector<std::uint8_t>& blocked);
}
```

`pathfinder.cpp`:

```cpp
#include "gl/game/pathfinder.hpp"
#include <algorithm>
#include <cmath>
#include <queue>

namespace gl::game {
namespace {

bool blocked_at(const WalkGrid& grid, const std::vector<std::uint8_t>& blocked, Tile tile) {
    if (!grid.empty() && !grid.is_walkable(tile.x, tile.y)) return true;
    const std::size_t index = static_cast<std::size_t>(tile.y) * grid.width + tile.x;
    return index < blocked.size() && blocked[index] != 0;
}

bool step_allowed(const WalkGrid& grid, const std::vector<std::uint8_t>& blocked,
                  Tile from, std::int32_t dx, std::int32_t dy) {
    const Tile next{from.x + dx, from.y + dy};
    if (blocked_at(grid, blocked, next)) return false;
    if (dx != 0 && dy != 0) {
        if (blocked_at(grid, blocked, Tile{from.x + dx, from.y})) return false;
        if (blocked_at(grid, blocked, Tile{from.x, from.y + dy})) return false;
    }
    return true;
}

float heuristic(Tile a, Tile b) {
    const std::int32_t dx = std::abs(a.x - b.x);
    const std::int32_t dy = std::abs(a.y - b.y);
    const std::int32_t min = std::min(dx, dy);
    return static_cast<float>(dx + dy) + (1.41421356f - 2.0f) * static_cast<float>(min);
}

} // namespace

bool line_of_sight(const WalkGrid& grid, Tile from, Tile to,
                   const std::vector<std::uint8_t>& blocked) {
    if (from == to) return true;
    std::int32_t x = from.x;
    std::int32_t y = from.y;
    const std::int32_t dx = std::abs(to.x - from.x);
    const std::int32_t dy = std::abs(to.y - from.y);
    const std::int32_t sx = from.x < to.x ? 1 : -1;
    const std::int32_t sy = from.y < to.y ? 1 : -1;
    std::int32_t error = dx - dy;
    while (x != to.x || y != to.y) {
        const std::int32_t e2 = error * 2;
        if (e2 > -dy) { error -= dy; x += sx; }
        if (e2 < dx) { error += dx; y += sy; }
        if (blocked_at(grid, blocked, Tile{x, y})) return false;
    }
    return true;
}

std::vector<Tile> find_path(const WalkGrid& grid, Tile from, Tile to,
                            const std::vector<std::uint8_t>& blocked) {
    if (from == to) return {from};
    if (blocked_at(grid, blocked, to)) return {};
    const std::size_t width = static_cast<std::size_t>(grid.width);
    const std::size_t height = static_cast<std::size_t>(grid.height);
    const auto index = [width](Tile t) { return static_cast<std::size_t>(t.y) * width + t.x; };
    std::vector<float> cost(width * height, -1.0f);
    std::vector<Tile> parent(width * height);
    struct Node { float f; Tile tile; };
    const auto worse = [](const Node& a, const Node& b) { return a.f > b.f; };
    std::priority_queue<Node, std::vector<Node>, decltype(worse)> open(worse);
    cost[index(from)] = 0.0f;
    parent[index(from)] = from;
    open.push({heuristic(from, to), from});
    constexpr std::int32_t kSteps[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    while (!open.empty()) {
        const Node node = open.top();
        open.pop();
        if (node.tile == to) break;
        if (node.f > cost[index(node.tile)] + heuristic(node.tile, to) + 0.0001f) continue;
        for (const auto& step : kSteps) {
            if (!step_allowed(grid, blocked, node.tile, step[0], step[1])) continue;
            const Tile next{node.tile.x + step[0], node.tile.y + step[1]};
            const float step_cost = (step[0] != 0 && step[1] != 0) ? 1.41421356f : 1.0f;
            const float next_cost = cost[index(node.tile)] + step_cost;
            if (cost[index(next)] < 0.0f || next_cost < cost[index(next)]) {
                cost[index(next)] = next_cost;
                parent[index(next)] = node.tile;
                open.push({next_cost + heuristic(next, to), next});
            }
        }
    }
    if (cost[index(to)] < 0.0f) return {};
    std::vector<Tile> path;
    Tile current = to;
    while (!(current == from)) {
        path.push_back(current);
        current = parent[index(current)];
    }
    path.push_back(from);
    std::reverse(path.begin(), path.end());
    return path;
}
}
```

Примечание: `blocked_at` для непустой сетки проверяет `is_walkable`, поэтому
`grid.width/height` валидны; для пустой сетки `index` считается по нулевым
размерам — вызывать `find_path` на пустой сетке не нужно (симуляция обходит
проверкой `grid_.empty()`).

- [x] **Step 4: Run tests**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*path*,*line of sight*"
```

- [x] **Step 5: Commit**

```bash
git add src/game tests/game
git commit -m "feat(game): add line-of-sight and A* pathfinding"
```

---

### Task 3: Интеграция в `LevelSimulation`

**Files:**
- Modify: `src/game/include/gl/game/level_simulation.hpp`
- Modify: `src/game/src/level_simulation.cpp`
- Test: `tests/game/test_level_simulation.cpp`

**Interfaces:**
- Consumes: `WalkGrid`, `Pathfinder`, существующие `PersonMovement`.
- Produces: `PersonMovement::path` (очередь waypoint-ов) и поведение обхода.

- [x] **Step 1: Failing tests**

Добавить в `test_level_simulation.cpp`:

```cpp
namespace {
Level make_walled_level() {
    Level level;
    level.name = "wall";
    level.width = 14 * 12;
    level.height = 10 * 9;
    level.walkable.width = 14;
    level.walkable.height = 10;
    level.walkable.walkable.assign(14 * 10, 1);
    for (int y = 0; y < 8; ++y)
        level.walkable.walkable[static_cast<std::size_t>(y) * 14 + 7] = 0; // стена
    PersonInstance person;
    person.name = "walker";
    person.tile_x = 2;
    person.tile_y = 2;
    person.route_type = "MOVED";
    person.route_cells = {{12, 2}};
    person.go_type = gl::assets::PadAnimationType::rt_go;
    person.go_move_x = 6.0f;
    person.go_move_y = 5.0f;
    person.go_delay_ms = 100;
    level.persons.push_back(person);
    return level;
}
}

TEST_CASE("walker goes around a wall and never enters blocked tiles") {
    auto sim = LevelSimulation::create(make_walled_level(), 1);
    for (int i = 0; i < 60 * 60; ++i) {
        sim.advance(1.0 / 60.0);
        const auto& p = sim.persons()[0];
        const int tile_x = static_cast<int>((p.x - 6.0f) / 12.0f + 0.5f);
        const int tile_y = static_cast<int>((p.y - 4.0f) / 9.0f + 0.5f);
        CHECK(make_walled_level().walkable.is_walkable(tile_x, tile_y));
    }
    CHECK(sim.persons()[0].x > 11 * 12);
}

TEST_CASE("two walkers never occupy the same tile") {
    Level level = make_walled_level();
    PersonInstance second = level.persons[0];
    second.name = "walker2";
    second.tile_x = 12;
    second.tile_y = 2;
    second.route_cells = {{2, 2}};
    level.persons.push_back(second);
    auto sim = LevelSimulation::create(level, 5);
    for (int i = 0; i < 60 * 30; ++i) {
        sim.advance(1.0 / 60.0);
        const auto& a = sim.persons()[0];
        const auto& b = sim.persons()[1];
        const int ax = static_cast<int>((a.x - 6.0f) / 12.0f + 0.5f);
        const int ay = static_cast<int>((a.y - 4.0f) / 9.0f + 0.5f);
        const int bx = static_cast<int>((b.x - 6.0f) / 12.0f + 0.5f);
        const int by = static_cast<int>((b.y - 4.0f) / 9.0f + 0.5f);
        CHECK_FALSE((ax == bx && ay == by));
    }
}
```

- [x] **Step 2: Run, verify failure.**

- [x] **Step 3: Implement**

В `PersonMovement` добавить:

```cpp
std::vector<Tile> path;
std::size_t path_index = 0;
```

В `LevelSimulation` (private) добавить `WalkGrid grid_;` и
`std::vector<std::uint8_t> occupancy_;`; в `create` — `grid_ = level.walkable;`.

Хелперы (анонимный namespace или private static):

```cpp
Tile tile_of(const PersonMovement& movement) {
    return Tile{static_cast<std::int32_t>(std::lround((movement.x - 6.0f) / 12.0f)),
                static_cast<std::int32_t>(std::lround((movement.y - 4.0f) / 9.0f))};
}

void LevelSimulation::rebuild_occupancy() {
    occupancy_.assign(static_cast<std::size_t>(grid_.width) * grid_.height, 0);
    if (occupancy_.empty()) return;
    for (const State& state : states_) {
        const Tile tile = tile_of(state.movement);
        if (tile.x < 0 || tile.y < 0 || tile.x >= grid_.width || tile.y >= grid_.height) continue;
        occupancy_[static_cast<std::size_t>(tile.y) * grid_.width + tile.x] = 1;
    }
}

std::vector<std::uint8_t> LevelSimulation::blocked_for(std::size_t index) const {
    std::vector<std::uint8_t> blocked = occupancy_;
    if (blocked.empty()) return blocked;
    const Tile self = tile_of(states_[index].movement);
    if (self.x >= 0 && self.y >= 0 && self.x < grid_.width && self.y < grid_.height)
        blocked[static_cast<std::size_t>(self.y) * grid_.width + self.x] = 0;
    return blocked;
}
```

В начале каждого тика `advance`: `rebuild_occupancy();`.

`choose_target(State&)` заменяется на `plan_target(std::size_t index)`:

```cpp
bool LevelSimulation::plan_target(std::size_t index) {
    State& state = states_[index];
    PersonMovement& m = state.movement;
    // ... прежний выбор goal (tile_x/tile_y) для MOVED/MOVED_FLIP/RANDOM* ...
    // RANDOM*: candidate принимается только если is_walkable и occupancy == 0.
    const Tile from = tile_of(m);
    const std::vector<std::uint8_t> blocked = blocked_for(index);
    std::vector<Tile> path;
    if (grid_.empty() || line_of_sight(grid_, from, goal, blocked)) {
        path = {from, goal};
    } else {
        path = find_path(grid_, from, goal, blocked);
    }
    if (path.size() < 2) return false; // нет пути -> пауза
    m.path = std::move(path);
    m.path_index = 1;
    m.target_x = m.path[1].x * 12.0f + 6.0f;
    m.target_y = m.path[1].y * 9.0f + 4.0f;
    m.moving = true;
    m.state_started_ms = time_ms_;
    if (!occupancy_.empty()) {
        const Tile reserved = m.path.back();
        if (reserved.x >= 0 && reserved.y >= 0 && reserved.x < grid_.width &&
            reserved.y < grid_.height)
            occupancy_[static_cast<std::size_t>(reserved.y) * grid_.width + reserved.x] = 1;
    }
    return true;
}
```

Прибытие на waypoint в `advance`:

```cpp
if (m.moving) {
    const Tile waypoint = m.path[m.path_index];
    m.target_x = waypoint.x * 12.0f + 6.0f;
    m.target_y = waypoint.y * 9.0f + 4.0f;
    const bool occupied_now = !grid_.empty() && m.path_index + 1 < m.path.size() &&
        occupancy_[static_cast<std::size_t>(m.path[m.path_index + 1].y) * grid_.width +
                   m.path[m.path_index + 1].x] != 0;
    if (occupied_now && !line_of_sight(grid_, tile_of(m), m.path.back(), blocked_for(index))) {
        m.moving = false;                       // replan в следующий тик
    } else if (/* достигли waypoint (|dx|<4 && |dy|<4) */) {
        if (m.path_index + 1 < m.path.size()) {
            ++m.path_index;
        } else {
            m.moving = false;
            /* прежняя логика паузы delay_min..max (P4b) */
        }
    }
}
```

Если `plan_target` вернул false — пауза `delay_min..max`, как при прибытии
(`waiting = true`, `wait_until_ms = time_ms_ + delay`), повтор в следующий раз.
Движение по прямой (путь из 2 клеток) не меняет P4b-поведение, когда `grid_.empty()`.

`RANDOM*`: `candidate` принимается, если
`(grid_.empty() || grid_.is_walkable(candidate)) &&` (occupancy пуст или клетка
свободна); retry 64, затем пауза.

- [x] **Step 4: Run tests**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*walker*,*simulation*,*MOVED*,*RANDOM*"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"; build\bin\gl_game_tests.exe
```

- [x] **Step 5: Commit**

```bash
git add src/game tests/game
git commit -m "feat(game): avoid obstacles and other NPCs in level simulation"
```

---

### Task 4: `--walkable-overlay`

**Files:**
- Modify: `src/app/include/gl/app/level_screen.hpp`
- Modify: `src/app/src/level_screen.cpp`
- Modify: `tools/glview/main.cpp`
- Modify: `tools/glgame/main.cpp`
- Test: `tests/render/test_glview_screenshot.cpp`

**Interfaces:**
- `LevelScreenOptions` получает `bool walkable_overlay = false;`.
- `--walkable-overlay` в обоих инструментах.

- [x] **Step 1: Failing screenshot test**

```cpp
TEST_CASE("glview --walkable-overlay changes the frame") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    TempDir dir;
    const fs::path plain = dir.path / "walk_plain.png";
    const fs::path overlay = dir.path / "walk_overlay.png";
    REQUIRE(run_glview(glview_arguments(game_dir, plain, {"--frames", "5"})) == 0);
    REQUIRE(run_glview(glview_arguments(game_dir, overlay,
                                        {"--frames", "5", "--walkable-overlay"})) == 0);
    int aw = 0, ah = 0, ac = 0, bw = 0, bh = 0, bc = 0;
    const StbiImage a = load_png(plain, aw, ah, ac);
    const StbiImage b = load_png(overlay, bw, bh, bc);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(aw == 1280);
    REQUIRE(ah == 720);
    std::uint64_t differing = 0;
    for (int y = 0; y < ah; ++y)
        for (int x = 0; x < aw; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * aw + x) * 4;
            if (a.get()[i] != b.get()[i] || a.get()[i + 1] != b.get()[i + 1] ||
                a.get()[i + 2] != b.get()[i + 2])
                ++differing;
        }
    CAPTURE(differing);
    CHECK(differing > 10000);
}
```

- [x] **Step 2: Implement overlay**

В `render` после отрисовки анимаций, если `options.walkable_overlay`:
для каждого тайла `(x,y)` с `!walkable.is_walkable(x,y)` нарисовать
`SDL_FRect{ x*12, y*9, 12, 9 }` цветом `SDL_SetRenderDrawBlendMode(renderer,
SDL_BLENDMODE_BLEND)` + `SDL_SetRenderDrawColor(renderer, 255, 0, 0, 96)` +
`SDL_RenderFillRectF`. Вне видимой области — пропускать (`rect_visible`).

В `glview`/`glgame`: флаг `--walkable-overlay` → `options.walkable_overlay = true`.

- [x] **Step 3: Run**

```powershell
cmake --build build
$env:GL_GAME_DIR="I:\Games\Златогорье-2"; build\bin\glview_screenshot_tests.exe --test-case="*walkable*"
```

- [x] **Step 4: Commit**

```bash
git add src/app tools tests
git commit -m "feat(app): add walkable overlay for collision verification"
```

---

### Task 5: Приёмка и документы

**Files:**
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`, `docs/plans/2026-10-22-p4b2-collisions-pathfinding.md`

- [x] **Step 1: Full suite**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
ctest --test-dir build --output-on-failure
```
Записать число тестов/assertions и время.

- [x] **Step 2: Evidence**

- Real-тест `l1_1` 60 с: 0 заходов в заблокированные тайлы, 0 совпадений тайлов,
  детерминизм; path не пуст для movers.
- `--walkable-overlay` PNG 1280×720; ручная сверка с фоном.
- Логи без ошибок pathfinding.

- [x] **Step 3: Docs**

- `STATUS.md`: P4b2 Tasks 1–5, automated passed, **manual gate OPEN**; далее P4c.
- `PROJECT.md`: P4 статус.
- План: отметить чекбоксы.

- [x] **Step 4: Commit**

```bash
git add docs
git commit -m "docs(p4b2): record automated acceptance"
```

- [ ] **Step 5: Manual gate (OPEN, user)**

```powershell
build\bin\glgame.exe --game-dir "I:\Games\Златогорье-2" --level l1_1
build\bin\glview.exe --game-dir "I:\Games\Златогорье-2" --level l1_1 --walkable-overlay
```

Проверить: NPC обходят дома/заборы и друг друга; overlay совпадает с фоном.
Только после подтверждения пользователя — фиксировать приёмку и мержить.
