# P4b: движение NPC по маршрутам — план

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** NPC на `l1_1` ходят по маршрутам SEF (`MOVED`, `MOVED_FLIP`, `RANDOM_RADIUS`, `RANDOM`) с паузами, 16-направленной `_go`-анимацией и возвратом в `stay`.

**Architecture:** `gl_assets` расширяется `cell_groups`; `gl_game` получает чистые route/direction helpers и `LevelSimulation` (SDL-free, детерминированный, шаг 1/60); `gl_app::LevelScreen` владеет симуляцией, грузит `_go`-листы и рисует.

**Tech Stack:** C++20, CMake+Ninja, doctest, SDL2, LLVM-MinGW.

## Global Constraints

- C++20, `-Wall -Wextra -Werror`, без лишних комментариев.
- `gl_assets` и `gl_game` не зависят от SDL.
- Слои только вниз: `app → game → assets`.
- Скорость: `movement_x/y` PAD — px за кадр `_go`; `px/s = movement * 1000 / delay` (D-026).
- RNG детерминированный (xorshift32), seed от уровня/имени/индекса.
- Данные игры не коммитить; не менять `third_party`.
- Полные прогоны: `$env:GL_GAME_DIR = "I:\Games\Златогорье-2"`; тесты без переменной скипаются.
- Toolchain PATH:
  `$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"`

---

### Task 1: SEF `cell_groups`

**Files:**
- Modify: `src/assets/include/gl/assets/sef.hpp`
- Modify: `src/assets/src/sef.cpp`
- Test: `tests/assets/test_sef.cpp`
- Test: `tests/assets/test_real_persons.cpp`

**Interfaces:**
- Consumes: `Lexer`, `next_non_semicolon`, `to_utf8`, `format_error_at` (уже есть в `sef.cpp`).
- Produces: `struct SefCellGroup { std::string name; std::vector<std::pair<std::uint16_t,std::uint16_t>> cells; };` и `SefData::cell_groups`.

- [ ] **Step 1: Write failing synthetic tests**

В `tests/assets/test_sef.cpp` добавить:

```cpp
TEST_CASE("SEF parses cell_groups") {
    const std::string text =
        "pack: \"L1_1\"\n"
        "cell_groups:\n{\n"
        "  name: \"track1\"\n  {\n"
        "    cell_00 161 127\n"
        "    cell_01 144 134\n"
        "  }\n"
        "  name: \"empty\"\n  {\n  }\n"
        "}\n";
    auto sef = gl::assets::parse_sef(as_bytes(text));
    REQUIRE(sef.has_value());
    REQUIRE(sef->cell_groups.size() == 2);
    CHECK(sef->cell_groups[0].name == "track1");
    REQUIRE(sef->cell_groups[0].cells.size() == 2);
    CHECK(sef->cell_groups[0].cells[0].first == 161);
    CHECK(sef->cell_groups[0].cells[0].second == 127);
    CHECK(sef->cell_groups[1].name == "empty");
    CHECK(sef->cell_groups[1].cells.empty());
}
```

В реальный тест (`tests/assets/test_real_persons.cpp`) добавить проверку `l1_1`
(понадобится `#include "gl/assets/sef.hpp"`):

```cpp
auto sef_bytes = vfs.value().read("levels/single/l1_1/l1_1.sef");
REQUIRE(sef_bytes.has_value());
auto sef = gl::assets::parse_sef(sef_bytes.value());
REQUIRE(sef.has_value());
CHECK(sef->cell_groups.size() == 9);  // 5 box* + 4 track*
auto find = [&](std::string_view name) -> const gl::assets::SefCellGroup* {
    for (const auto& group : sef->cell_groups)
        if (group.name == name) return &group;
    return nullptr;
};
REQUIRE(find("track1") != nullptr);
CHECK(find("track1")->cells.size() == 9);
REQUIRE(find("track4") != nullptr);
CHECK(find("track4")->cells.size() == 7);
REQUIRE(find("track2") != nullptr);
CHECK(find("track2")->cells.size() == 22);
```

- [ ] **Step 2: Run tests, verify failure**

```powershell
cmake --build build
build\bin\gl_assets_tests.exe --test-case="*cell_groups*"
```
Ожидаемо: compile error — нет `cell_groups`.

- [ ] **Step 3: Add types**

В `sef.hpp`:

```cpp
struct SefCellGroup {
    std::string name;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> cells;
};
```
и в `SefData` после `persons`:

```cpp
std::vector<SefCellGroup> cell_groups;
```
Добавить `#include <utility>`.

- [ ] **Step 4: Parse the section**

В `sef.cpp` (анонимный namespace, рядом с `parse_persons`) добавить:

```cpp
VoidResult parse_cell_group_fields(Lexer& lexer, SefCellGroup& group) {
    while (true) {
        const Token key = next_non_semicolon(lexer);
        if (lexer.failed())
            return format_error_at("unterminated SEF cell group", key.line);
        if (key.kind == TokenKind::end)
            return format_error_at("unterminated SEF cell group", key.line);
        if (key.kind == TokenKind::rbrace)
            return VoidResult{std::monostate{}};
        if (key.kind != TokenKind::word)
            return format_error_at("unexpected token in SEF cell group", key.line);

        Token value = lexer.next();
        if (lexer.failed())
            return format_error_at("unterminated SEF cell group", key.line);
        if (value.kind == TokenKind::colon)
            value = lexer.next();
        if (key.text == "name") {
            if (value.kind != TokenKind::string)
                return format_error_at("SEF cell group name is not a string", value.line);
            group.name = to_utf8(value.text);
            continue;
        }
        if (key.text.rfind("cell_", 0) == 0) {
            if (value.kind != TokenKind::word)
                return format_error_at("bad SEF cell coordinate", value.line);
            const Token second = lexer.next();
            int x = 0;
            int y = 0;
            if (!parse_int(value.text, x) || second.kind != TokenKind::word ||
                !parse_int(second.text, y))
                return format_error_at("bad SEF cell coordinate", value.line);
            if (x < 0 || y < 0 || x > 0xFFFF || y > 0xFFFF)
                return format_error_at("SEF cell coordinate out of range", value.line);
            group.cells.emplace_back(static_cast<std::uint16_t>(x),
                                     static_cast<std::uint16_t>(y));
            continue;
        }
        if (value.kind == TokenKind::lbrace) {
            lexer.skip_balanced();
            if (lexer.failed())
                return format_error_at("unterminated SEF cell group", value.line);
        }
    }
}

Result<std::vector<SefCellGroup>> parse_cell_groups(Lexer& lexer) {
    const Token open = next_non_semicolon(lexer);
    if (open.kind != TokenKind::lbrace)
        return format_error_at("SEF cell_groups is not a block", open.line);
    std::vector<SefCellGroup> groups;
    while (true) {
        const Token key = next_non_semicolon(lexer);
        if (lexer.failed())
            return format_error_at("unterminated SEF cell_groups", key.line);
        if (key.kind == TokenKind::end)
            return format_error_at("unterminated SEF cell_groups", key.line);
        if (key.kind == TokenKind::rbrace)
            return groups;
        if (key.kind != TokenKind::word || key.text != "name")
            return format_error_at("expected name in SEF cell_groups", key.line);
        Token colon = lexer.next();
        if (colon.kind != TokenKind::colon)
            return format_error_at("expected ':' after SEF cell group name", colon.line);
        const Token name = lexer.next();
        if (name.kind != TokenKind::string)
            return format_error_at("SEF cell group name is not a string", name.line);
        const Token brace = lexer.next();
        if (brace.kind != TokenKind::lbrace)
            return format_error_at("SEF cell group is not a block", brace.line);
        SefCellGroup group;
        group.name = to_utf8(name.text);
        auto status = parse_cell_group_fields(lexer, group);
        if (!status)
            return status.error();
        groups.push_back(std::move(group));
    }
}
```

В `parse_sef`, перед `persons`:

```cpp
if (key.text == "cell_groups") {
    auto groups = parse_cell_groups(lexer);
    if (!groups)
        return groups.error();
    sef.cell_groups = std::move(groups.value());
    continue;
}
```

- [ ] **Step 5: Run tests**

```powershell
cmake --build build
build\bin\gl_assets_tests.exe
$env:GL_GAME_DIR="I:\Games\Златогорье-2"; build\bin\gl_assets_real_tests.exe
```
Ожидаемо: все проходят; реальный SEF-sweep 261/261.

- [ ] **Step 6: Commit**

```powershell
git add src/assets/include/gl/assets/sef.hpp src/assets/src/sef.cpp tests/assets/test_sef.cpp tests/assets/test_real_persons.cpp
git commit -m "feat(assets): parse SEF cell_groups"
```

---

### Task 2: Route and direction helpers

**Files:**
- Create: `src/game/include/gl/game/person_movement.hpp`
- Create: `src/game/src/person_movement.cpp`
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_person_movement.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Produces:
  `enum class RouteType : std::uint8_t { stay=0, stay_rotate=1, moved=2, moved_flip=3, random_radius=4, random=5 };`
  `RouteType parse_route_type(std::string_view) noexcept;`
  `std::uint8_t direction16_from_velocity(float vx, float vy) noexcept;`
  `std::uint8_t direction16_to8(std::uint8_t) noexcept;`
  `std::uint8_t direction8_to16(std::uint8_t) noexcept;`
  `float iso_speed(float vx, float vy, float move_x, float move_y) noexcept;`

- [ ] **Step 1: Write failing tests**

`tests/game/test_person_movement.cpp`:

```cpp
#include "gl/game/person_movement.hpp"
#include <doctest/doctest.h>

using namespace gl::game;

TEST_CASE("route type names") {
    CHECK(parse_route_type("STAY") == RouteType::stay);
    CHECK(parse_route_type("STAY_ROTATE") == RouteType::stay_rotate);
    CHECK(parse_route_type("MOVED") == RouteType::moved);
    CHECK(parse_route_type("MOVED_FLIP") == RouteType::moved_flip);
    CHECK(parse_route_type("RANDOM_RADIUS") == RouteType::random_radius);
    CHECK(parse_route_type("RANDOM") == RouteType::random);
    CHECK(parse_route_type("???") == RouteType::stay);
}

TEST_CASE("16-way direction from velocity") {
    CHECK(direction16_from_velocity(0.0f, -1.0f) == 0);
    CHECK(direction16_from_velocity(-0.40f, -0.90f) == 1);
    CHECK(direction16_from_velocity(-0.70f, -0.60f) == 2);
    CHECK(direction16_from_velocity(-0.95f, -0.30f) == 3);
    CHECK(direction16_from_velocity(-1.0f, 0.0f) == 4);
    CHECK(direction16_from_velocity(-0.95f, 0.30f) == 5);
    CHECK(direction16_from_velocity(-0.70f, 0.60f) == 6);
    CHECK(direction16_from_velocity(-0.40f, 0.90f) == 7);
    CHECK(direction16_from_velocity(0.0f, 1.0f) == 8);
    CHECK(direction16_from_velocity(0.40f, 0.90f) == 9);
    CHECK(direction16_from_velocity(0.70f, 0.60f) == 10);
    CHECK(direction16_from_velocity(0.95f, 0.30f) == 11);
    CHECK(direction16_from_velocity(1.0f, 0.0f) == 12);
    CHECK(direction16_from_velocity(0.95f, -0.30f) == 13);
    CHECK(direction16_from_velocity(0.70f, -0.60f) == 14);
    CHECK(direction16_from_velocity(0.40f, -0.90f) == 15);
}

TEST_CASE("16/8 direction mapping") {
    const std::uint8_t to8[16] = {0,1,1,1,2,3,3,3,4,5,5,5,6,7,7,7};
    for (std::uint8_t i = 0; i < 16; ++i)
        CHECK(direction16_to8(i) == to8[i]);
    const std::uint8_t to16[8] = {0,2,4,6,8,10,12,14};
    for (std::uint8_t i = 0; i < 8; ++i)
        CHECK(direction8_to16(i) == to16[i]);
}

TEST_CASE("isometric speed blend") {
    CHECK(iso_speed(1.0f, 0.0f, 6.0f, 5.0f) == doctest::Approx(6.0f));
    CHECK(iso_speed(0.0f, 1.0f, 6.0f, 5.0f) == doctest::Approx(5.0f));
    CHECK(iso_speed(1.0f, 1.0f, 6.0f, 5.0f) == doctest::Approx(5.5f));
}
```

- [ ] **Step 2: Run, verify failure**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*direction*"
```
Ожидаемо: ошибка компиляции (нет заголовка).

- [ ] **Step 3: Implement header**

`person_movement.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <string_view>

namespace gl::game {
enum class RouteType : std::uint8_t {
    stay = 0, stay_rotate = 1, moved = 2, moved_flip = 3, random_radius = 4, random = 5
};
RouteType parse_route_type(std::string_view type) noexcept;
std::uint8_t direction16_from_velocity(float vx, float vy) noexcept;
std::uint8_t direction16_to8(std::uint8_t direction16) noexcept;
std::uint8_t direction8_to16(std::uint8_t direction8) noexcept;
float iso_speed(float vx, float vy, float move_x, float move_y) noexcept;
}
```

- [ ] **Step 4: Implement helpers**

`person_movement.cpp`:

```cpp
#include "gl/game/person_movement.hpp"
#include <array>
#include <cmath>

namespace gl::game {
namespace {
constexpr float kVertical = 0.9863f;
constexpr float kMostlyVertical = 0.8616f;
constexpr float kDiagonal = 0.5878f;
constexpr float kMostlyHorizontal = 0.2079f;
}

RouteType parse_route_type(std::string_view type) noexcept {
    if (type == "STAY_ROTATE") return RouteType::stay_rotate;
    if (type == "MOVED") return RouteType::moved;
    if (type == "MOVED_FLIP") return RouteType::moved_flip;
    if (type == "RANDOM_RADIUS") return RouteType::random_radius;
    if (type == "RANDOM") return RouteType::random;
    return RouteType::stay;
}

std::uint8_t direction16_from_velocity(float vx, float vy) noexcept {
    const float length = std::sqrt(vx * vx + vy * vy);
    if (length <= 0.0f) return 0;
    const float x = std::fabs(vx / length);
    const bool downward = vy > 0.0f;
    if (vx < 0.0f) {
        if (downward) {
            if (x <= kMostlyHorizontal) return 8;
            if (x <= kDiagonal) return 7;
            if (x <= kMostlyVertical) return 6;
            if (x <= kVertical) return 5;
            return 4;
        }
        if (x <= kMostlyHorizontal) return 0;
        if (x <= kDiagonal) return 1;
        if (x <= kMostlyVertical) return 2;
        if (x <= kVertical) return 3;
        return 4;
    }
    if (downward) {
        if (x <= kMostlyHorizontal) return 8;
        if (x <= kDiagonal) return 9;
        if (x <= kMostlyVertical) return 10;
        if (x <= kVertical) return 11;
        return 12;
    }
    if (x <= kMostlyHorizontal) return 0;
    if (x <= kDiagonal) return 15;
    if (x <= kMostlyVertical) return 14;
    if (x <= kVertical) return 13;
    return 12;
}

std::uint8_t direction16_to8(std::uint8_t direction16) noexcept {
    static constexpr std::array<std::uint8_t, 16> kMap{0,1,1,1,2,3,3,3,4,5,5,5,6,7,7,7};
    return kMap[direction16 % kMap.size()];
}

std::uint8_t direction8_to16(std::uint8_t direction8) noexcept {
    static constexpr std::array<std::uint8_t, 8> kMap{0,2,4,6,8,10,12,14};
    return kMap[direction8 % kMap.size()];
}

float iso_speed(float vx, float vy, float move_x, float move_y) noexcept {
    const float length2 = vx * vx + vy * vy;
    if (length2 <= 0.0f) return move_y;
    const float fx2 = (vx * vx) / length2;
    return move_y + (move_x - move_y) * fx2;
}
}
```

- [ ] **Step 5: Wire CMake and run**

В `src/game/CMakeLists.txt` добавить `src/person_movement.cpp`;
в `tests/game/CMakeLists.txt` — `test_person_movement.cpp`.

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*direction*,*route*,*speed*"
```

- [ ] **Step 6: Commit**

```powershell
git add src/game tests/game
git commit -m "feat(game): add route type and 16-way direction helpers"
```

---

### Task 3: Load routes and `_go` metadata

**Files:**
- Modify: `src/game/include/gl/game/person.hpp`
- Modify: `src/game/src/person.cpp`
- Test: `tests/game/test_person_loader.cpp`
- Test: `tests/game/test_real_person_loader.cpp`

**Interfaces:**
- Produces in `PersonInstance`:
  `std::vector<std::pair<std::int32_t,std::int32_t>> route_cells;`
  `std::optional<gl::assets::PadAnimationType> go_type;`
  `float go_move_x = 0; float go_move_y = 0; std::int32_t go_delay_ms = 0;`

- [ ] **Step 1: Failing real test**

Добавить в `test_real_person_loader.cpp`:

```cpp
TEST_CASE("real data: l1_1 routes and go animations" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto persons = gl::game::load_persons(vfs.value(), "l1_1");
    REQUIRE(persons.has_value());

    int movers = 0;
    for (const auto& person : *persons) {
        if (person.route_type == "STAY") {
            CHECK(person.route_cells.empty());
            continue;
        }
        ++movers;
        CHECK(!person.route_cells.empty());
        REQUIRE(person.go_type.has_value());
        CHECK(person.go_delay_ms > 0);
        CHECK(person.go_move_x > 0.0f);
        CHECK(person.go_move_y > 0.0f);
    }
    CHECK(movers == 6);
    CHECK(persons->at(1).route_cells.size() == 7);   // track4
    CHECK(persons->at(9).route_cells.size() == 9);   // track1
    CHECK(persons->at(12).route_cells.size() == 8);  // track3
    CHECK(persons->at(13).route_cells.size() == 22); // track2
}
```

- [ ] **Step 2: Run, verify failure** — нет полей `route_cells`/`go_type`.

- [ ] **Step 3: Extend `PersonInstance`**

```cpp
#include <optional>
#include <utility>

std::vector<std::pair<std::int32_t, std::int32_t>> route_cells;
std::optional<gl::assets::PadAnimationType> go_type;
float go_move_x = 0.0f;
float go_move_y = 0.0f;
std::int32_t go_delay_ms = 0;
```

- [ ] **Step 4: Resolve routes and go in `load_persons`**

Перед циклом собрать `std::map<std::string, const SefCellGroup*>` с ключом `ascii_lower(name)`
(использовать существующий `lower_path` после нормализации). В цикле после PAD:

```cpp
auto route_it = groups.find(lower_path(source.route));
if (route_it != groups.end()) {
    for (const auto& cell : route_it->second->cells)
        person.route_cells.emplace_back(cell.first, cell.second);
}

const gl::assets::PadAnimation* go = nullptr;
for (const auto& animation : person.pad) {
    if (animation.type == gl::assets::PadAnimationType::rt_go) { go = &animation; break; }
}
if (go == nullptr) {
    for (const auto& animation : person.pad) {
        if (animation.type == gl::assets::PadAnimationType::tb_go) { go = &animation; break; }
    }
}
if (go != nullptr) {
    person.go_type = go->type;
    person.go_move_x = go->movement_x;
    person.go_move_y = go->movement_y;
    person.go_delay_ms = go->delay;
}
```

Если `route_type != "STAY"` и (`route_cells.empty()` или `!go_type`), писать
`gl::log(warning, ...)`; симуляция оставит NPC стоять.

- [ ] **Step 5: Synthetic loader test**

Добавить в `test_person_loader.cpp` помощник (рядом с `write_sef`):

```cpp
void write_sef_with_cells(const fs::path& root, std::string_view level,
                          std::string_view persons, std::string_view cell_groups) {
    write_text(root / "levels" / "single" / std::string(level) /
                   (std::string(level) + ".sef"),
               "pack: \"" + std::string(level) +
                   "\"\npersons:\n{\n" + std::string(persons) +
                   "\n}\ncell_groups:\n{\n" + std::string(cell_groups) + "\n}\n");
}
```

Новый тест:

```cpp
TEST_CASE("person loader: resolves route cells from cell_groups") {
    TempDir dir;
    write_sef_with_cells(
        dir.path, "test_level",
        "name: \"Walk.One\"\n{\n  position 10 10\n  direction \"DOWN\"\n"
        "  route_type \"MOVED\"\n  route \"track1\"\n  radius 15\n"
        "  delay_min 0\n  delay_max 9000\n}\n",
        "name: \"track1\"\n{\n  cell_00 10 10\n  cell_01 13 10\n}\n");
    write_script(dir.path, "Walk.One", "L0.Walk_One");
    write_person_assets(dir.path, "L0.Walk_One");
    write_shadow(dir.path, "L0.Walk_One");

    auto vfs = gl::assets::GameVfs::mount(dir.path.string());
    REQUIRE(vfs.has_value());
    auto persons = gl::game::load_persons(vfs.value(), "test_level");
    REQUIRE(persons.has_value());
    REQUIRE(persons->size() == 1);
    REQUIRE(persons->front().route_cells.size() == 2);
    CHECK(persons->front().route_cells[0] == std::pair<std::int32_t, std::int32_t>{10, 10});
    CHECK(persons->front().route_cells[1] == std::pair<std::int32_t, std::int32_t>{13, 10});
    CHECK_FALSE(persons->front().go_type.has_value()); // make_pad has no go animation
}
```

- [ ] **Step 6: Run**

```powershell
cmake --build build
build\bin\gl_game_tests.exe
$env:GL_GAME_DIR="I:\Games\Златогорье-2"; build\bin\gl_game_tests.exe
```

- [ ] **Step 7: Commit**

```powershell
git add src/game tests/game
git commit -m "feat(game): load person routes and go animation metadata"
```

---

### Task 4: `LevelSimulation`

**Files:**
- Create: `src/game/include/gl/game/level_simulation.hpp`
- Create: `src/game/src/level_simulation.cpp`
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_level_simulation.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Consumes: `Level`, `PersonInstance`, `RouteType`, helpers Task 2.
- Produces:

```cpp
struct PersonMovement {
    float x = 0, y = 0;
    float target_x = 0, target_y = 0;
    std::uint8_t direction8 = 0;
    std::uint8_t direction16 = 0;
    bool moving = false;
    bool waiting = false;
    std::uint64_t wait_until_ms = 0;
    std::uint64_t state_started_ms = 0;
};

class LevelSimulation {
public:
    static LevelSimulation create(const Level& level, std::uint64_t seed);
    void advance(double dt_seconds);
    const std::vector<PersonMovement>& persons() const;
    bool can_move(std::size_t index) const;
    std::uint64_t time_ms() const;
};
```

- [ ] **Step 1: Failing tests**

`test_level_simulation.cpp`:

```cpp
#include "gl/game/level_simulation.hpp"
#include <doctest/doctest.h>

using namespace gl::game;

namespace {
Level make_level(std::string route_type, std::vector<std::pair<std::int32_t,std::int32_t>> cells,
                 std::int32_t delay_min, std::int32_t delay_max) {
    Level level;
    level.name = "test";
    level.width = 240 * 12;
    level.height = 240 * 9;
    PersonInstance person;
    person.name = "person";
    person.tile_x = 10;
    person.tile_y = 10;
    person.route_type = std::move(route_type);
    person.radius = 3;
    person.delay_min = delay_min;
    person.delay_max = delay_max;
    person.route_cells = std::move(cells);
    person.go_type = gl::assets::PadAnimationType::rt_go;
    person.go_move_x = 6.0f;
    person.go_move_y = 5.0f;
    person.go_delay_ms = 100;
    level.persons.push_back(std::move(person));
    return level;
}
}

TEST_CASE("MOVED follows route cells in order") {
    Level level = make_level("MOVED", {{10,10},{13,10},{13,13}}, 0, 0);
    auto sim = LevelSimulation::create(level, 1);
    CHECK(sim.can_move(0));
    for (int i = 0; i < 600; ++i) sim.advance(1.0 / 60.0);
    CHECK(sim.persons()[0].x > 10.0f * 12 + 6);
}

TEST_CASE("delay under 500 ms does not stop") {
    Level level = make_level("MOVED", {{10,10},{13,10}}, 0, 400);
    auto sim = LevelSimulation::create(level, 1);
    sim.advance(3.0);
    CHECK(sim.persons()[0].x == doctest::Approx(13.0f * 12 + 6).epsilon(0.01));
}

TEST_CASE("delay over 500 ms waits") {
    Level level = make_level("MOVED", {{10,10},{13,10}}, 1000, 1000);
    auto sim = LevelSimulation::create(level, 1);
    sim.advance(0.5);
    CHECK(sim.persons()[0].x < 10.0f * 12 + 6 + 1.0f);
    sim.advance(2.0);
    CHECK(sim.persons()[0].x > 10.0f * 12 + 6 + 4.0f);
}

TEST_CASE("RANDOM_RADIUS stays within radius") {
    Level level = make_level("RANDOM_RADIUS", {}, 0, 0);
    auto sim = LevelSimulation::create(level, 3);
    for (int i = 0; i < 600; ++i) {
        sim.advance(1.0 / 60.0);
        const auto& p = sim.persons()[0];
        CHECK(std::fabs(p.x - (10.0f * 12 + 6)) <= 3 * 12 + 6);
        CHECK(std::fabs(p.y - (10.0f * 9 + 4)) <= 3 * 9 + 4);
    }
}

TEST_CASE("simulation is deterministic") {
    Level level = make_level("MOVED_FLIP", {{10,10},{13,10},{13,13},{10,13}}, 0, 900);
    auto a = LevelSimulation::create(level, 7);
    auto b = LevelSimulation::create(level, 7);
    for (int i = 0; i < 120; ++i) {
        a.advance(1.0 / 60.0);
        b.advance(1.0 / 60.0);
    }
    CHECK(a.persons()[0].x == b.persons()[0].x);
    CHECK(a.persons()[0].y == b.persons()[0].y);
}

TEST_CASE("person without go cannot move") {
    Level level = make_level("MOVED", {{10,10},{13,10}}, 0, 0);
    level.persons[0].go_type.reset();
    auto sim = LevelSimulation::create(level, 1);
    CHECK_FALSE(sim.can_move(0));
    sim.advance(5.0);
    CHECK(sim.persons()[0].x == doctest::Approx(10.0f * 12 + 6));
}
```

- [ ] **Step 2: Run, verify failure**

```powershell
cmake --build build
```
Ожидаемо: нет `level_simulation.hpp`.

- [ ] **Step 3: Implement header and logic**

`level_simulation.hpp`:

```cpp
#pragma once
#include "gl/game/level.hpp"
#include <cstdint>
#include <vector>

namespace gl::game {
struct PersonMovement {
    float x = 0.0f;
    float y = 0.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;
    std::uint8_t direction8 = 0;
    std::uint8_t direction16 = 0;
    bool moving = false;
    bool waiting = false;
    std::uint64_t wait_until_ms = 0;
    std::uint64_t state_started_ms = 0;
};

class LevelSimulation {
public:
    static LevelSimulation create(const Level& level, std::uint64_t seed);
    void advance(double dt_seconds);
    const std::vector<PersonMovement>& persons() const { return persons_; }
    bool can_move(std::size_t index) const;
    std::uint64_t time_ms() const { return time_ms_; }

private:
    struct State {
        PersonMovement movement;
        std::vector<std::pair<std::int32_t, std::int32_t>> route;
        RouteType route_type = RouteType::stay;
        std::int32_t radius = 13;
        std::int32_t delay_min = 0;
        std::int32_t delay_max = 0;
        float move_x = 0.0f;
        float move_y = 0.0f;
        std::int32_t go_delay_ms = 0;
        float spawn_x = 0.0f;
        float spawn_y = 0.0f;
        std::int32_t route_index = 0;
        std::uint32_t rng = 1;
        bool can_move = false;
    };
    void choose_target(State& state);
    std::uint32_t next_random(State& state);
    std::int32_t random_range(State& state, std::int32_t low, std::int32_t high);
    std::vector<State> states_;
    std::vector<PersonMovement> persons_;
    std::uint64_t time_ms_ = 0;
    double accumulator_ = 0.0;
    float width_ = 0.0f;
    float height_ = 0.0f;
};
}
```

Добавить `#include "gl/game/person_movement.hpp"` и `<utility>`.

`level_simulation.cpp` — ключевая логика:

```cpp
constexpr float kTileW = 12.0f;
constexpr float kTileH = 9.0f;
constexpr float kArrive = 4.0f;

std::uint32_t hash_name(std::string_view text) {
    std::uint32_t hash = 2166136261u;
    for (unsigned char c : text) hash = (hash ^ c) * 16777619u;
    return hash;
}

LevelSimulation LevelSimulation::create(const Level& level, std::uint64_t seed) {
    LevelSimulation sim;
    sim.width_ = static_cast<float>(level.width);
    sim.height_ = static_cast<float>(level.height);
    sim.states_.reserve(level.persons.size());
    for (std::size_t i = 0; i < level.persons.size(); ++i) {
        const PersonInstance& person = level.persons[i];
        State state;
        state.route_type = parse_route_type(person.route_type);
        state.route = person.route_cells;
        state.radius = person.radius > 0 ? person.radius : 13;
        state.delay_min = std::max(0, person.delay_min);
        state.delay_max = std::max(state.delay_min, person.delay_max);
        state.go_delay_ms = person.go_delay_ms;
        state.move_x = person.go_move_x;
        state.move_y = person.go_move_y;
        state.spawn_x = person.tile_x * kTileW + 6.0f;
        state.spawn_y = person.tile_y * kTileH + 4.0f;
        state.movement.x = state.spawn_x;
        state.movement.y = state.spawn_y;
        state.movement.direction8 = person.direction;
        state.movement.direction16 = direction8_to16(person.direction);
        state.rng = static_cast<std::uint32_t>(seed) ^ hash_name(level.name) ^
                    hash_name(person.name) ^ (static_cast<std::uint32_t>(i) * 0x9E3779B9u);
        if (state.rng == 0) state.rng = 0x12345678u;
        state.can_move = person.go_type.has_value() && state.go_delay_ms > 0 &&
                         (!state.route.empty() || state.route_type == RouteType::random_radius ||
                          state.route_type == RouteType::random);
        if ((state.route_type == RouteType::moved || state.route_type == RouteType::moved_flip) &&
            state.route.empty()) {
            state.can_move = false;
        }
        if (state.route_type == RouteType::stay || state.route_type == RouteType::stay_rotate) {
            state.can_move = false;
        }
        sim.states_.push_back(std::move(state));
    }
    sim.persons_.resize(sim.states_.size());
    for (std::size_t i = 0; i < sim.states_.size(); ++i) sim.persons_[i] = sim.states_[i].movement;
    return sim;
}
```

`advance`:

```cpp
void LevelSimulation::advance(double dt_seconds) {
    if (!(dt_seconds > 0.0)) return;
    accumulator_ += dt_seconds;
    while (accumulator_ + 1e-12 >= 1.0 / 60.0) {
        accumulator_ -= 1.0 / 60.0;
        time_ms_ += 1000 / 60;
        for (State& state : states_) {
            if (!state.can_move) continue;
            PersonMovement& m = state.movement;
            if (m.moving) {
                const float dx = m.target_x - m.x;   // target_x/y добавить в State/PersonMovement
                const float dy = m.target_y - m.y;
                const float distance = std::sqrt(dx * dx + dy * dy);
                const float speed = iso_speed(dx, dy, state.move_x, state.move_y) *
                                    1000.0f / static_cast<float>(state.go_delay_ms);
                const float step = speed * (1.0f / 60.0f);
                if (distance <= step || (std::fabs(dx) < kArrive && std::fabs(dy) < kArrive)) {
                    m.x = m.target_x;
                    m.y = m.target_y;
                    m.moving = false;
                    const std::int32_t delay = random_range(state, state.delay_min, state.delay_max);
                    m.state_started_ms = time_ms_;
                    if (delay < 500) {
                        choose_target(state);
                    } else {
                        m.waiting = true;
                        m.wait_until_ms = time_ms_ + static_cast<std::uint64_t>(delay);
                    }
                } else {
                    m.x += dx / distance * step;
                    m.y += dy / distance * step;
                    m.direction16 = direction16_from_velocity(dx, dy);
                    m.direction8 = direction16_to8(m.direction16);
                }
            } else if (m.waiting) {
                if (time_ms_ >= m.wait_until_ms) {
                    m.waiting = false;
                    choose_target(state);
                }
            } else {
                choose_target(state);
            }
            persons_[&state - states_.data()] = m;
        }
    }
}
```

`PersonMovement` дополнить `float target_x = 0, target_y = 0;`.

`choose_target`:

```cpp
void LevelSimulation::choose_target(State& state) {
    PersonMovement& m = state.movement;
    if (state.route_type == RouteType::stay || state.route_type == RouteType::stay_rotate) {
        state.can_move = false;
        return;
    }
    std::int32_t tile_x = 0;
    std::int32_t tile_y = 0;
    if (state.route_type == RouteType::moved || state.route_type == RouteType::moved_flip) {
        const std::int32_t count = static_cast<std::int32_t>(state.route.size());
        if (state.route_index < 0 || state.route_index >= count) state.route_index = 0;
        tile_x = state.route[static_cast<std::size_t>(state.route_index)].first;
        tile_y = state.route[static_cast<std::size_t>(state.route_index)].second;
        if (state.route_type == RouteType::moved) {
            state.route_index = (state.route_index + 1) % count;
        } else {
            state.route_index += ((next_random(state) & 1u) != 0) ? 1 : -1;
        }
    } else {
        const float base_x = state.route_type == RouteType::random_radius
            ? state.spawn_x : m.x;
        const float base_y = state.route_type == RouteType::random_radius
            ? state.spawn_y : m.y;
        for (int attempt = 0; attempt < 64; ++attempt) {
            const std::int32_t candidate_x = static_cast<std::int32_t>(
                std::lround(base_x / kTileW)) + random_range(state, -state.radius, state.radius);
            const std::int32_t candidate_y = static_cast<std::int32_t>(
                std::lround(base_y / kTileH)) + random_range(state, -state.radius, state.radius);
            if (candidate_x >= 0 && candidate_y >= 0 &&
                candidate_x * kTileW + 6.0f < width_ &&
                candidate_y * kTileH + 4.0f < height_) {
                tile_x = candidate_x;
                tile_y = candidate_y;
                break;
            }
            tile_x = static_cast<std::int32_t>(std::lround(base_x / kTileW));
            tile_y = static_cast<std::int32_t>(std::lround(base_y / kTileH));
        }
    }
    m.target_x = tile_x * kTileW + 6.0f;
    m.target_y = tile_y * kTileH + 4.0f;
    m.moving = true;
    m.state_started_ms = time_ms_;
}
```

RNG:

```cpp
std::uint32_t LevelSimulation::next_random(State& state) {
    state.rng ^= state.rng << 13;
    state.rng ^= state.rng >> 17;
    state.rng ^= state.rng << 5;
    return state.rng;
}
std::int32_t LevelSimulation::random_range(State& state, std::int32_t low, std::int32_t high) {
    if (high <= low) return low;
    const std::uint32_t span = static_cast<std::uint32_t>(high - low) + 1u;
    return low + static_cast<std::int32_t>(next_random(state) % span);
}
```

`can_move(index)` возвращает `states_[index].can_move && !route_type stay`.

- [ ] **Step 4: Run tests**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*simulation*,*MOVED*,*delay*,*RANDOM*,*deterministic*"
```

- [ ] **Step 5: Commit**

```powershell
git add src/game tests/game
git commit -m "feat(game): add deterministic NPC route simulation"
```

---

### Task 5: Render moving NPCs in `LevelScreen`

**Files:**
- Modify: `src/app/include/gl/app/person_render.hpp`
- Modify: `src/app/src/person_render.cpp`
- Modify: `src/app/include/gl/app/level_screen.hpp`
- Modify: `src/app/src/level_screen.cpp`
- Modify: `tools/glview/main.cpp`
- Modify: `tools/glgame/main.cpp`
- Test: `tests/app/test_person_render.cpp`
- Test: `tests/render/test_glview_screenshot.cpp`
- Test: `tests/app/test_glgame_screenshot.cpp`

**Interfaces:**
- Consumes: `LevelSimulation` (Task 4).
- Produces: `place_person_px(animation, direction, px, py)`; `LevelScreen::load(renderer, vfs, level, bool load_persons = true)`.

- [ ] **Step 1: Failing helper test**

В `test_person_render.cpp`:

```cpp
TEST_CASE("px placement uses float position") {
    gl::assets::PadAnimation animation;
    animation.type = gl::assets::PadAnimationType::rt_go;
    animation.anchor_x = 20;
    animation.anchor_y = 77;
    auto placement = gl::app::place_person_px(animation, 12, 1610.0f, 1147.0f);
    CHECK(placement.draw_x == doctest::Approx(1590.0f));
    CHECK(placement.draw_y == doctest::Approx(1070.0f));
    CHECK(placement.row == 4);
    CHECK(placement.mirror);
}
```

- [ ] **Step 2: Implement `place_person_px`**

`person_render.hpp`:

```cpp
PersonPlacement place_person_px(const gl::assets::PadAnimation& animation,
                               std::uint8_t direction, float x, float y);
PersonShadowPlacement place_person_shadow_px(const gl::assets::PadAnimation& animation,
                                             std::uint8_t direction, float x, float y);
```

`person_render.cpp`: те же вычисления, но `x - anchor_x`, `y - anchor_y` без умножения
на тайлы; `direction` передаётся как есть (`pad_person_row` сам выбирает 16/8 таблицу).

- [ ] **Step 3: Extend `LevelScreen`**

В `level_screen.hpp`:

```cpp
struct Person {
    gl::render::Texture texture;
    std::optional<gl::render::Texture> shadow;
    std::optional<gl::render::Texture> go_texture;
    std::optional<gl::render::Texture> go_shadow;
    std::size_t instance = 0;
    std::size_t animation = 0;
    std::size_t go_animation = 0;
};
```
Добавить `#include <gl/game/level_simulation.hpp>`,
`static Result<LevelScreen> load(..., bool load_persons = true);`,
члены `gl::game::LevelSimulation simulation_;`, `std::uint64_t simulated_ms_ = 0;`,
`std::uint64_t last_simulation_ms_ = 0;`.

В `load`:
- после persons: `simulation_ = gl::game::LevelSimulation::create(level_, 0x5EED);`
- для каждого `PersonInstance` с `go_type`:

```cpp
const std::string directory = person_directory(instance.res_name);
const std::string go_sheet =
    *instance.go_type == gl::assets::PadAnimationType::rt_go ? "rt_go" : "tb_go";
for (std::size_t i = 0; i < instance.pad.size(); ++i)
    if (instance.pad[i].type == *instance.go_type) go_animation = i;
bool ok = decode_person_image(vfs,
    directory + "/animation/" + go_sheet + ".csx", go_image, ...);
if (ok) {
    go_texture = gl::render::Texture::from_image(renderer, go_image);
    decode_person_image(vfs, directory + "/shadows/" + go_sheet + ".csx",
                        go_shadow_image, ...);
    // опционально: Texture::from_image(renderer, go_shadow_image)
}
```
`decode_person_image` — локальный helper P4a в `level_screen.cpp`; использовать
его же. При ошибке `go_texture` остаётся пустым, пишется warning, NPC рисуется
как стоящий (симуляция при этом продолжает двигать координаты — это допустимо
только для повреждённых данных, все реальные movers `l1_1` имеют `_go`).

В `render`:
```cpp
const std::uint64_t target = elapsed_ms;
if (target > last_simulation_ms_) {
    simulation_.advance(static_cast<double>(target - last_simulation_ms_) / 1000.0);
    last_simulation_ms_ = target;
}
```
(если `options.draw_persons == false` — не продвигать).

Отрисовка:
```cpp
for (const Person& person : persons_) {
    const gl::game::PersonMovement& movement = simulation_.persons()[person.instance];
    const bool moving = movement.moving;
    const auto& animation = moving ? level_.persons[person.instance].pad[person.go_animation]
                                   : level_.persons[person.instance].pad[person.animation];
    const auto& texture = moving ? person.go_texture : person.texture;
    if (!texture) continue;
    const auto placement = gl::app::place_person_px(
        animation, moving ? movement.direction16 : movement.direction8,
        movement.x, movement.y);
    const std::size_t frame = gl::app::person_frame_index(
        elapsed_ms - movement.state_started_ms, animation.delay, animation.frames_per_row);
    ... SDL_RenderCopyExF c source (col*frame, row*frame_h) ...
}
```
Тень: для `moving` — `go_shadow`, `place_person_shadow_px`, `pad_shadow_row(go_type, direction16)`.

- [ ] **Step 4: `--no-persons` / `--sim-ms`**

- `glview`: парсить `--sim-ms N`; в screenshot-режиме передавать `N` как `elapsed_ms`
  в `render`. `--no-persons` — передавать `load_persons=false` в `LevelScreen::load`.
- `glgame`: аналогично `--sim-ms` (для screenshot) и `--no-persons`; в обычном цикле
  `render(..., client_.snapshot().simulation_time_ms)` уже передаёт серверное время.

- [ ] **Step 5: Screenshot tests**

В `test_glview_screenshot.cpp`:

```cpp
TEST_CASE("glview NPCs move between t=0 and t=10s") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    TempDir dir;
    const fs::path first_png = dir.path / "move_first.png";
    const fs::path later_png = dir.path / "move_later.png";
    REQUIRE(run_glview(glview_arguments(game_dir, first_png, {"--sim-ms", "0"})) == 0);
    REQUIRE(run_glview(glview_arguments(game_dir, later_png, {"--sim-ms", "10000"})) == 0);

    int aw = 0, ah = 0, ac = 0, bw = 0, bh = 0, bc = 0;
    const StbiImage first = load_png(first_png, aw, ah, ac);
    const StbiImage later = load_png(later_png, bw, bh, bc);
    REQUIRE(first != nullptr);
    REQUIRE(later != nullptr);
    REQUIRE(aw == bw);
    REQUIRE(ah == bh);
    std::uint64_t differing = 0;
    for (int y = 0; y < ah; ++y) {
        for (int x = 0; x < aw; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * aw + x) * 4;
            if (first.get()[i] != later.get()[i] ||
                first.get()[i + 1] != later.get()[i + 1] ||
                first.get()[i + 2] != later.get()[i + 2])
                ++differing;
        }
    }
    CAPTURE(differing);
    CHECK(differing > 1000);
}
```
(Использовать существующие `TempDir`, `glview_arguments`, `load_png`, `StbiImage`
из этого файла.)

В `test_glgame_screenshot.cpp`:

```cpp
TEST_CASE("glgame NPCs move between t=0 and t=10s") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path first_png = temp.path / "move_first.png";
    const fs::path later_png = temp.path / "move_later.png";
    REQUIRE(run_glgame(screenshot_arguments(
        game_dir, config, first_png, {"--level", "l1_1", "--sim-ms", "0"})) == 0);
    REQUIRE(run_glgame(screenshot_arguments(
        game_dir, config, later_png, {"--level", "l1_1", "--sim-ms", "10000"})) == 0);

    int aw = 0, ah = 0, ac = 0, bw = 0, bh = 0, bc = 0;
    const StbiImage first = load_png(first_png, aw, ah, ac);
    const StbiImage later = load_png(later_png, bw, bh, bc);
    REQUIRE(first != nullptr);
    REQUIRE(later != nullptr);
    REQUIRE(aw == bw);
    REQUIRE(ah == bh);
    std::uint64_t differing = 0;
    for (int y = 0; y < ah; ++y) {
        for (int x = 0; x < aw; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * aw + x) * 4;
            if (first.get()[i] != later.get()[i] ||
                first.get()[i + 1] != later.get()[i + 1] ||
                first.get()[i + 2] != later.get()[i + 2])
                ++differing;
        }
    }
    CAPTURE(differing);
    CHECK(differing > 1000);
}
```

- [ ] **Step 6: Run**

```powershell
cmake --build build
build\bin\person_render_tests.exe
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\glview_screenshot_tests.exe
build\bin\glgame_screenshot_tests.exe
```

- [ ] **Step 7: Commit**

```powershell
git add src/app tools tests
git commit -m "feat(app): render moving NPCs from level simulation"
```

---

### Task 6: Acceptance and docs

**Files:**
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`
- Modify: `docs/plans/2026-10-22-p4b-person-movement.md`

- [ ] **Step 1: Full real suite**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
ctest --test-dir build --output-on-failure
```
Записать число тестов, assertions и время.

- [ ] **Step 2: Evidence**

- `glview --level l1_1 --screenshot out0.png --sim-ms 0` и `--sim-ms 10000`:
  diff > 1000 px, 1280×720.
- Лог: `16 persons`, у 4 движущихся загружены `go`-листы.
- Реальный тест: `l1_1` 60 с — 4 движущихся, 10 стоящих; повторный прогон
  совпадает.

- [ ] **Step 3: Docs**

- `docs/STATUS.md`: запись P4b Tasks 1–6, automated acceptance passed,
  **manual desktop gate OPEN**; следующая — P4c (клик → диалог).
- `docs/PROJECT.md`: P4 статус — P4a принят, P4b automated passed, manual pending.
- `docs/plans/...`: отметить чекбоксы.
- `docs/research/p4b-person-movement.md`: уточнить наблюдаемые значения,
  если появятся.

- [ ] **Step 4: Commit**

```powershell
git add docs
git commit -m "docs(p4b): record automated acceptance"
```

- [ ] **Step 5: Manual gate**

Команда для пользователя:

```powershell
build\bin\glgame.exe --game-dir "I:\Games\Златогорье-2" --level l1_1
```

Проверить: 4 NPC ходят, 10 стоят, направления/анимации корректны, `Esc`/меню.
Только после подтверждения пользователя — фиксировать приёмку и мержить.
