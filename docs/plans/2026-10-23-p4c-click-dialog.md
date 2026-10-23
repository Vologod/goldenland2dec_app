# P4c: клик → hero → диалог — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Добавить hero-маркер, подход к talkable NPC по клику и локальный dialog overlay поверх уровня.

**Architecture:** `gl::game::HeroController` хранит позицию/путь и использует `Level::walkable` + текущие позиции NPC как occupancy. `LevelScreen` делает screen→world/hit-test, рисует маркер и обновляет hero. `DialogOverlay` вынесен из `glgame` и рисуется поверх уровня; путь диалога берётся из `scr_dialog`.

**Tech Stack:** C++20, SDL2, CMake/Ninja, doctest, существующие `gl::assets`/`gl::game`/`gl::render`/`gl::script`.

## Global Constraints

- Не коммитить без явной просьбы пользователя. Шаги `Commit` выполнять только по указанию.
- Hero — маркер `kHeroMarkerSize = 20` px; точный PAD Вертаса вне P4c (D-038).
- Остановка — AABB маркера и PAD-кадра NPC; не фиксированные 90 px.
- Диалог — локальный overlay; server action `0x12` не реализуется.
- Кодировки: данные cp1251/ASCII; на границе API UTF-8.
- Пути VFS регистронезависимы, `\` нормализуется в `/`.
- `GL_GAME_DIR`-тесты без каталога игры возвращают 77; CMake `SKIP_RETURN_CODE 77`.
- Команды проверки: `cmake --build build`, `ctest --test-dir build --output-on-failure`.

---

### Task 1: SEF `points_entrance`

**Files:**
- Modify: `src/assets/include/gl/assets/sef.hpp`
- Modify: `src/assets/src/sef.cpp`
- Modify: `src/game/include/gl/game/level.hpp`
- Modify: `src/game/src/level_loader.cpp`
- Test: `tests/assets/test_sef.cpp`

**Interfaces:**
- Produces: `struct gl::assets::SefEntrance { std::string name; std::int32_t x; std::int32_t y; std::uint8_t direction; };`
- Produces: `SefData::entrances`, `Level::entrances`.

- [ ] **Step 1: Write failing parser test**

Add to `tests/assets/test_sef.cpp`:

```cpp
TEST_CASE("sef points_entrance parses names, positions, directions") {
    const std::string text =
        "points_entrance:\n"
        "{\n"
        "  name: \"L1_1_e_L1_1_1\"\n"
        "  {\n"
        "    position 75 55\n"
        "    direction \"DOWN_LEFT\"\n"
        "  }\n"
        "  name: \"GM\"\n"
        "  {\n"
        "    position 175 206\n"
        "    direction \"UP\"\n"
        "  }\n"
        "}\n";
    const auto data = as_bytes(text);
    const auto sef = gl::assets::parse_sef(data);
    REQUIRE(sef);
    REQUIRE(sef->entrances.size() == 2);
    CHECK(sef->entrances[0].name == "L1_1_e_L1_1_1");
    CHECK(sef->entrances[0].x == 75);
    CHECK(sef->entrances[0].y == 55);
    CHECK(sef->entrances[0].direction == 3); // DOWN_LEFT in existing SEF map
    CHECK(sef->entrances[1].name == "GM");
    CHECK(sef->entrances[1].x == 175);
    CHECK(sef->entrances[1].y == 206);
    CHECK(sef->entrances[1].direction == 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target gl_assets_tests; ctest --test-dir build -R gl_assets_tests --output-on-failure`

Expected: compile error: `SefData` has no member `entrances`.

- [ ] **Step 3: Add struct and parser**

In `sef.hpp`, add `SefEntrance` and `std::vector<SefEntrance> entrances;` to `SefData`.

In `sef.cpp`, after `cell_groups` parsing, detect top-level `points_entrance`:

```cpp
if (key == "points_entrance") {
    if (!lexer.consume('{')) return error("points_entrance: expected '{'");
    while (!lexer.consume('}')) {
        SefEntrance e;
        if (!lexer.read_string_key(e.name)) return error("points_entrance: expected name");
        if (!lexer.consume('{')) return error("points_entrance: expected '{'");
        while (!lexer.consume('}')) {
            const auto field = lexer.read_key();
            if (field == "position") {
                if (!lexer.read_int(e.x) || !lexer.read_int(e.y)) {
                    return error("points_entrance: bad position");
                }
            } else if (field == "direction") {
                std::string dir;
                if (!lexer.read_string(dir) || !parse_direction(dir, e.direction)) {
                    return error("points_entrance: bad direction");
                }
            } else {
                lexer.skip_value();
            }
        }
        data.entrances.push_back(std::move(e));
    }
    continue;
}
```

Use existing lexer helpers and `parse_direction` (the same string map as SEF persons).

- [ ] **Step 4: Store in Level**

In `level.hpp` add `std::vector<gl::assets::SefEntrance> entrances;`.

In `level_loader.cpp`, when SEF is read (`level.pack = sef->pack;`), also
`level.entrances = sef->entrances;`.

- [ ] **Step 5: Add real SEF test**

In `tests/assets/test_sef.cpp` add GL_GAME_DIR-gated:

```cpp
TEST_CASE("real l1_1 has five points_entrance") {
    const char* dir = std::getenv("GL_GAME_DIR");
    if (!dir) { doctest::skip(true); return; }
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs);
    auto bytes = vfs->read("levels/single/l1_1/l1_1.sef");
    REQUIRE(bytes);
    auto sef = gl::assets::parse_sef(*bytes);
    REQUIRE(sef);
    REQUIRE(sef->entrances.size() == 5);
    CHECK(sef->entrances[0].name == "L1_1_e_L1_1_4");
    CHECK(sef->entrances[0].x == 180);
    CHECK(sef->entrances[0].y == 21);
    CHECK(sef->entrances[3].name == "L1_1_e_L1_2");
    CHECK(sef->entrances[3].x == 36);
    CHECK(sef->entrances[3].y == 60);
    CHECK(sef->entrances[4].name == "GM");
    CHECK(sef->entrances[4].x == 175);
    CHECK(sef->entrances[4].y == 206);
}
```

- [ ] **Step 6: Run tests**

Run: `cmake --build build; ctest --test-dir build -R "gl_assets_tests" --output-on-failure`

Expected: PASS.

- [ ] **Step 7: Commit only by explicit request**

```bash
git add src/assets/include/gl/assets/sef.hpp src/assets/src/sef.cpp src/game/include/gl/game/level.hpp src/game/src/level_loader.cpp tests/assets/test_sef.cpp
git commit -m "feat(assets): parse SEF points_entrance"
```

---

### Task 2: Tile/world helpers and Hero core

**Files:**
- Create: `src/game/include/gl/game/hero.hpp`
- Create: `src/game/src/hero.cpp`
- Modify: `src/game/CMakeLists.txt`
- Modify: `src/game/include/gl/game/pathfinder.hpp`
- Test: `tests/game/test_hero.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Produces: `gl::game::kTileWidth = 12`, `kTileHeight = 9`, `kHeroMarkerSize = 20`.
- Produces: `Tile tile_at(float x, float y)`, `float tile_center_x(std::int32_t)`, `float tile_center_y(std::int32_t)`.
- Produces: `struct Hero`, `class HeroController` with `spawn_at_first_entrance()`, `spawn_at_tile(Tile)`, `set_hero_world(float,float)`, `set_goal(Tile)`, `advance(double)`, `hero()`.

- [ ] **Step 1: Write failing tile/hero test**

Create `tests/game/test_hero.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/game/hero.hpp"
#include "gl/game/level.hpp"

TEST_CASE("tile_at round-trips tile centers") {
    using namespace gl::game;
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 50; ++x) {
            const Tile t = tile_at(tile_center_x(x), tile_center_y(y));
            CHECK(t.x == x);
            CHECK(t.y == y);
        }
    }
}

TEST_CASE("hero spawns at first walkable entrance") {
    gl::game::Level level;
    level.walkable.width = 10;
    level.walkable.height = 10;
    level.walkable.walkable.assign(100, 1);
    level.entrances.push_back({"blocked", 1, 1, 0});
    level.entrances.push_back({"free", 2, 2, 0});
    level.walkable.walkable[1 * 10 + 1] = 0;
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_first_entrance();
    CHECK(hero.hero().tile.x == 2);
    CHECK(hero.hero().tile.y == 2);
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build --target gl_game_tests; ctest --test-dir build -R gl_game_tests --output-on-failure`

Expected: `hero.hpp` not found.

- [ ] **Step 3: Implement helpers and Hero skeleton**

`pathfinder.hpp` add inline helpers:

```cpp
inline constexpr std::int32_t kTileWidth = 12;
inline constexpr std::int32_t kTileHeight = 9;
inline constexpr std::int32_t kHeroMarkerSize = 20;
inline Tile tile_at(float x, float y) {
    return {static_cast<std::int32_t>(std::lround((x - 6.0f) / 12.0f)),
            static_cast<std::int32_t>(std::lround((y - 4.0f) / 9.0f))};
}
inline float tile_center_x(std::int32_t x) { return x * 12.0f + 6.0f; }
inline float tile_center_y(std::int32_t y) { return y * 9.0f + 4.0f; }
```

`hero.hpp`:

```cpp
#pragma once
#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/game/pathfinder.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace gl::game {
struct Hero {
    float x = 0.0f, y = 0.0f;
    Tile tile{};
    Tile goal{};
    bool has_goal = false;
    std::vector<Tile> path;
    std::size_t path_index = 0;
    std::uint8_t direction = 0;
};

class HeroController {
public:
    HeroController(const Level& level, const LevelSimulation* simulation);
    void spawn_at_first_entrance();
    void spawn_at_tile(Tile tile);
    void set_hero_world(float x, float y);
    bool set_goal(Tile goal);
    void advance(double dt_seconds);
    bool can_interact(const PersonInstance& person, const PersonMovement& movement) const;
    const Hero& hero() const { return hero_; }
private:
    const Level& level_;
    const LevelSimulation* simulation_;
    Hero hero_;
};
}
```

`hero.cpp` implement spawn and basic advance along path; `set_goal` may return false until Task 3.

- [ ] **Step 4: Run tests**

Run: `cmake --build build; ctest --test-dir build -R gl_game_tests --output-on-failure`

Expected: PASS for tile/spawn.

- [ ] **Step 5: Commit only by explicit request**

```bash
git add src/game/include/gl/game/hero.hpp src/game/src/hero.cpp src/game/include/gl/game/pathfinder.hpp src/game/CMakeLists.txt tests/game/test_hero.cpp tests/game/CMakeLists.txt
git commit -m "feat(game): add hero controller and tile helpers"
```

---

### Task 3: Hero path to NPC and interaction AABB

**Files:**
- Modify: `src/game/src/hero.cpp`
- Test: `tests/game/test_hero.cpp`

**Interfaces:**
- Consumes: `find_path`, `WalkGrid`, `PersonInstance::pad`, `PersonMovement`.
- Produces: `HeroController::set_goal` nearest-free-cell fallback; `can_interact`.

- [ ] **Step 1: Write failing path test**

Add synthetic level with a 3×3 open grid, NPC at (2,2), hero at (0,2):

```cpp
TEST_CASE("hero paths to a free cell next to npc") {
    gl::game::Level level;
    level.walkable.width = 8;
    level.walkable.height = 8;
    level.walkable.walkable.assign(64, 1);
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 2});
    CHECK(hero.set_goal({2, 2}));
    CHECK_FALSE(hero.hero().path.empty());
    CHECK(hero.hero().path.back() != gl::game::Tile{2, 2});
    CHECK(hero.hero().path.back().y == 2);
}
```

- [ ] **Step 2: Run to verify failure**

Run: `ctest --test-dir build -R gl_game_tests --output-on-failure`

Expected: `spawn_at_tile`/path logic missing or path ends on NPC cell.

- [ ] **Step 3: Implement path and AABB**

- `blocked` lambda: out of bounds or NPC tile (from `simulation_->persons()`, except target tile); if `simulation_ == nullptr`, no NPC blocking.
- If target tile is occupied by an NPC, search radius 1..3 in 8 directions for
  walkable/unoccupied; call `find_path` to that cell. If the target tile itself
  is non-walkable, reject the goal (no fallback path for wall clicks).
- `advance`: move along `path[path_index]` with `iso_speed` from `person_movement.hpp`; on reaching segment, increment; when done clear path/has_goal.
- `can_interact`: compute NPC frame from `person.pad` matching `person.stay_type` (`frame_width`, `frame_height`, `anchor_x`, `anchor_y`); build world rect `{movement.x - anchor_x, movement.y - anchor_y, frame_width, frame_height}`; hero rect `{hero_.x - 10, hero_.y - 10, 20, 20}`; return `SDL_HasIntersection` equivalent (write plain AABB helper, no SDL in game layer).
- Add `spawn_at_tile(Tile)` public for tests.

- [ ] **Step 4: Add can_interact test**

```cpp
TEST_CASE("can_interact uses npc frame extents") {
    gl::game::PersonInstance p;
    p.stay_type = gl::assets::PadAnimationType::rt_stay;
    gl::assets::PadAnimation a;
    a.frame_width = 40;
    a.frame_height = 60;
    a.anchor_x = 20;
    a.anchor_y = 55;
    p.pad.push_back(a);
    gl::game::PersonMovement m;
    m.x = 100;
    m.y = 100;
    gl::game::Level level;
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 0});
    hero.set_hero_world(125.0f, 100.0f);
    CHECK(hero.can_interact(p, m));
}
```

Add `set_hero_world(float,float)` test seam.

- [ ] **Step 5: Run tests**

Run: `cmake --build build; ctest --test-dir build -R gl_game_tests --output-on-failure`

Expected: PASS.

- [ ] **Step 6: Commit only by explicit request**

```bash
git add src/game/src/hero.cpp tests/game/test_hero.cpp
git commit -m "feat(game): hero pathing and interaction range"
```

---

### Task 4: `scr_dialog` resolver

**Files:**
- Create: `src/game/include/gl/game/dialog_path.hpp`
- Create: `src/game/src/dialog_path.cpp`
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_dialog_path.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Produces: `std::optional<std::string> dialog_script_path(const PersonInstance&);`

- [ ] **Step 1: Write failing tests**

```cpp
#include <doctest/doctest.h>
#include "gl/game/dialog_path.hpp"

TEST_CASE("dialog path maps scr_dialog") {
    gl::game::PersonInstance p;
    p.dialog = "l1_1\\l1.p621_svet_3circusworkerv.d621.age";
    CHECK(gl::game::dialog_script_path(p) ==
          std::optional<std::string>{"scripts/dialogs/l1_1/l1.p621_svet_3circusworkerv.d621.age.cs"});
}

TEST_CASE("empty dialog is not talkable") {
    gl::game::PersonInstance p;
    CHECK_FALSE(gl::game::dialog_script_path(p));
}

TEST_CASE("existing .cs is not duplicated") {
    gl::game::PersonInstance p;
    p.dialog = "demon.d1.age.cs";
    CHECK(gl::game::dialog_script_path(p) ==
          std::optional<std::string>{"scripts/dialogs/demon.d1.age.cs"});
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build; ctest --test-dir build -R gl_game_tests --output-on-failure`

Expected: `dialog_path.hpp` not found.

- [ ] **Step 3: Implement**

```cpp
std::optional<std::string> dialog_script_path(const PersonInstance& person) {
    if (person.dialog.empty()) return std::nullopt;
    std::string p = person.dialog;
    for (char& c : p) if (c == '\\') c = '/';
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    std::string out = "scripts/dialogs/" + p;
    if (out.size() < 3 || out.substr(out.size() - 3) != ".cs") out += ".cs";
    return out;
}
```

No VFS check in this pure function; `glgame` checks `vfs.exists`.

- [ ] **Step 4: Run tests**

Run: `cmake --build build; ctest --test-dir build -R gl_game_tests --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Commit only by explicit request**

```bash
git add src/game/include/gl/game/dialog_path.hpp src/game/src/dialog_path.cpp src/game/CMakeLists.txt tests/game/test_dialog_path.cpp tests/game/CMakeLists.txt
git commit -m "feat(game): resolve scr_dialog paths"
```

---

### Task 5: LevelScreen click, hit-test, marker

**Files:**
- Modify: `src/app/include/gl/app/level_screen.hpp`
- Modify: `src/app/src/level_screen.cpp`
- Test: `tests/app/test_level_click.cpp`
- Modify: `tests/app/CMakeLists.txt`

**Interfaces:**
- Consumes: `HeroController`, `tile_at`, `place_person_px`.
- Produces: `Tile screen_to_tile(const gl::render::Camera&, int, int)`, `ClickTarget`, `ClickTarget click_target(int,int)`, `bool world_to_tile(...)`, `HeroController& hero()`.
- `LevelScreenOptions` gets `draw_hero` (default true).

- [ ] **Step 1: Write failing coordinate test**

```cpp
TEST_CASE("screen_to_tile maps camera and zoom") {
    gl::render::Camera camera;
    camera.x = 120.0f;
    camera.y = 90.0f;
    camera.zoom = 1.0f;
    const auto t = gl::app::screen_to_tile(camera, 0, 0);
    CHECK(t.x == 10);
    CHECK(t.y == 10);
}
```

Add free function `Tile screen_to_tile(const gl::render::Camera&, int sx, int sy)` in
`level_screen.hpp`; `LevelScreen::world_to_tile` delegates to it.

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build --target person_render_tests; ctest --test-dir build -R person_render_tests --output-on-failure`

Expected: `world_to_tile` missing.

- [ ] **Step 3: Implement click/hit-test/marker**

- Add `ClickTarget` and methods to `LevelScreen`.
- `world_to_tile`: inverse camera (`camera.x + sx / zoom`, `camera.y + sy / zoom`), `tile_at`.
- `person_at_screen`: for each `level_.persons[i]`, get animation `stay_type`; compute `place_person_px`; draw rect from frame; hit-test. Ignore invalid PAD.
- `draw_hero`: marker diamond at `(hero_.hero().x, hero_.hero().y)` via `SDL_RenderDrawLine` or filled `SDL_Rect` (20×20, alpha 180), drawn in the body pass at baseline `hero.y`.
- Add `HeroController& hero()` and initialize `hero_` in `load` (`spawn_at_first_entrance`).

- [ ] **Step 4: Add marker/sort test**

Synthetic `LevelScreen` with two persons; ensure `hero` baseline placed between them by stable sort; test `person_at_screen` for a known frame returns the person index.

- [ ] **Step 5: Run tests**

Run: `cmake --build build; ctest --test-dir build -R "person_render_tests|glview" --output-on-failure`

Expected: PASS.

- [ ] **Step 6: Commit only by explicit request**

```bash
git add src/app/include/gl/app/level_screen.hpp src/app/src/level_screen.cpp tests/app/test_level_click.cpp tests/app/CMakeLists.txt
git commit -m "feat(app): level click hit-test and hero marker"
```

---

### Task 6: DialogOverlay component

**Files:**
- Create: `src/app/include/gl/app/dialog_overlay.hpp`
- Create: `src/app/src/dialog_overlay.cpp`
- Modify: `src/app/CMakeLists.txt`
- Test: `tests/app/test_dialog_overlay.cpp`
- Modify: `tests/app/CMakeLists.txt`

**Interfaces:**
- Produces: `DialogOverlay` with `set_content`, `clear`, `active`, `hit_test`, `render`, `selected`, `move_selection`.

- [ ] **Step 1: Write failing layout test**

```cpp
TEST_CASE("dialog overlay hit-tests answers") {
    gl::app::DialogOverlay o;
    o.set_content("Стражник", "Больше не подходи ко мне!", {"Уйти", "Спросить"});
    REQUIRE(o.active());
    CHECK(o.hit_test(80, 600) == std::optional<std::size_t>{0});
    CHECK(o.hit_test(80, 660) == std::optional<std::size_t>{1});
}
```

Use the same geometry as current `glgame::dialog_answer_rect` (panel bottom, answer rows 36 px).

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build; ctest --test-dir build -R person_render_tests --output-on-failure`

Expected: `dialog_overlay.hpp` not found.

- [ ] **Step 3: Implement overlay**

Move answer-rect math from `glgame/main.cpp:721-727` into `DialogOverlay`; store `speaker_`, `text_`, `answers_`, `selected_`. `render` draws panel (`SDL_SetRenderDrawColor(12,14,20,230)`), speaker/text via `Font::draw_text`/`wrap`, answers with highlight. `hit_test` returns answer index. `move_selection` wraps.

- [ ] **Step 4: Run tests**

Run: `cmake --build build; ctest --test-dir build -R person_render_tests --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Commit only by explicit request**

```bash
git add src/app/include/gl/app/dialog_overlay.hpp src/app/src/dialog_overlay.cpp src/app/CMakeLists.txt tests/app/test_dialog_overlay.cpp tests/app/CMakeLists.txt
git commit -m "feat(app): reusable dialog overlay"
```

---

### Task 7: glgame integration

**Files:**
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp`

**Interfaces:**
- Consumes: `HeroController`, `ClickTarget`, `DialogOverlay`, `dialog_script_path`.
- Produces: level state keeps `level_` while dialog open; overlay rendered after level.

- [ ] **Step 1: Add failing screenshot test**

Add a case that runs:

```cpp
glgame --level l1_1 --click-npc <first-talkable-index> --screenshot <png>
```

Resolve `<first-talkable-index>` from SEF persons in the test (first non-empty
`scr_dialog`). Expect exit 0, PNG 1280×720, and at least one non-background pixel
in the dialog panel area (bottom 220 px). `--click-world 180 21` must not open
a panel.

- [ ] **Step 2: Run to verify failure**

Run: `ctest --test-dir build -R glgame_screenshot_tests --output-on-failure`

Expected: unknown flag or no overlay.

- [ ] **Step 3: Implement integration**

- Add `HeroController hero_` to `Game` or `LevelScreen`; update after `LevelSimulation::advance`.
- Click handling: record `mouse_down`; on `MOUSEBUTTONUP` if distance <4 px call `screen.click_target`; if person:
  - if `!person.dialog.empty()` and `can_interact` → open dialog;
  - else set goal to person tile.
- `open_dialog` stays; after start fill `DialogOverlay`.
- `render`: if `level_` exists, render level always; if `dialog_` active, render overlay.
- Dialog input priority: `dialog_overlay.hit_test`, `handle_dialog_key`; `Esc` closes overlay and session, state remains `level`.
- Add `--click-npc <index>` and `--click-world <x> <y>` test flags: apply after level load, run 2 s headless, then screenshot/exit.
- `close_dialog` clears overlay; do not reset `level_`.

- [ ] **Step 4: Run targeted tests**

Run: `cmake --build build; ctest --test-dir build -R "glgame_screenshot_tests|glview_screenshot_tests" --output-on-failure`

Expected: PASS; dialog screenshot shows level under panel.

- [ ] **Step 5: Commit only by explicit request**

```bash
git add tools/glgame/main.cpp tests/app/test_glgame_screenshot.cpp
git commit -m "feat(glgame): click-to-npc and dialog overlay"
```

---

### Task 8: Real l1_1 acceptance and docs

**Files:**
- Modify: `tests/app/test_glgame_screenshot.cpp`
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`

**Interfaces:**
- Consumes all previous tasks.

- [ ] **Step 1: Add real click test**

Resolve `L1.P621_Svet_3CircusWorkerV` index from `LevelScreen::level().persons`; run:

```cpp
glgame --level l1_1 --click-npc <index> --screenshot <png>
```

Expect exit 0, overlay active, `npc_text()` contains a non-empty string from
`scripts/dialogs/l1_1/l1.p621_svet_3circusworkerv.d621.age.cs`.

- [ ] **Step 2: Run full test suite**

Run: `cmake --build build; ctest --test-dir build --output-on-failure`

Expected: all tests pass; `GL_GAME_DIR` real tests not skipped when set.

- [ ] **Step 3: Update docs**

In `STATUS.md` add P4c result: hero marker, spawn, click/path, overlay, tests,
manual gate status. In `PROJECT.md` mark P4c `[~]` or `[x]` only after manual
screen check; otherwise leave `[ ]` and note automated pass.

- [ ] **Step 4: Manual gate**

Пользователь проверяет в `glgame`: клик по NPC → подход → диалог поверх
уровня; Esc → уровень; клик по земле; NPC без диалога. Без этого P4c не
считать принятым.

- [ ] **Step 5: Commit only by explicit request**

```bash
git add tests/app/test_glgame_screenshot.cpp docs/STATUS.md docs/PROJECT.md
git commit -m "test: P4c real level click and dialog acceptance"
```

---

## Self-review

- Spec §2 behavior: Tasks 1–8.
- Spec §4.1 SEF: Task 1.
- Spec §4.2 Hero/path/AABB: Tasks 2–3.
- Spec §4.3 click/hit-test/marker: Task 5.
- Spec §4.4 dialog path/overlay: Tasks 4, 6.
- Spec §5 errors: Tasks 1–7.
- Spec §7 tests: Tasks 1–8.
- Spec §8 acceptance: Task 8.
- Spec §9 risks: covered by D-038 and manual gate.

No placeholders; commit steps conditional on explicit user request.
