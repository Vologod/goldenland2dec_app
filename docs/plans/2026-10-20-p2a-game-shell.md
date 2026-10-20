# P2a — Game Shell and Console Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development or executing-plans. Steps use `- [ ]`.

**Goal:** Add `glgame` — a game shell over the existing level viewer: read/write `GoldenLand.ini`, show a menu, load a level, open the `~` console, execute engine commands, and keep a `ServerCore`/`ClientCore` command/event boundary.

**Architecture:** New SDL-free `gl_core` (Config, commands, console buffer, state machine, server/client skeleton). `gl_render` gains ASCII text. A new SDL `gl_app` extracts the level screen from `glview` so `glgame` and `glview` share it. `glgame` wires menu/console/states.

**Tech Stack:** C++20, SDL2 2.32.10, stb (image, easy_font), doctest, miniz.

## Global Constraints

- C++20; `-Wall -Wextra -Werror`.
- `gl_assets`/`gl_game`/`gl_core` SDL-free; SDL only in `platform`/`render`/`app`/tools.
- Config is cp1251 on disk; API strings UTF-8; preserve comments/unknown lines/binds.
- No game assets committed; game dir via `--game-dir`/`GL_GAME_DIR`.
- P2a text UI is ASCII; Russian UI is P7.
- Every task ends with a commit.

## File Structure

```
third_party/stb/stb_easy_font.h              (add, pinned)
src/core/include/gl/core/{config,command,console,state,server}.hpp
src/core/src/{config,command,console,state,server}.cpp
src/render/include/gl/render/text.hpp
src/render/src/text.cpp
src/app/include/gl/app/level_screen.hpp
src/app/src/level_screen.cpp
src/app/CMakeLists.txt
tools/glgame/main.cpp, CMakeLists.txt
tests/core/{CMakeLists.txt,test_main.cpp,test_config.cpp,test_command.cpp,test_console.cpp,test_state.cpp,test_server.cpp}
tests/app/{CMakeLists.txt,test_glgame_screenshot.cpp}
```

---

### Task 1: `gl_core` + Config

**Files:** `src/core/**`, `tests/core/**`, root CMake.

**Interfaces:**
```cpp
namespace gl::core {
class Config {
public:
    static Result<Config> load(const std::filesystem::path& path);
    Result<void> save(const std::filesystem::path& path) const;
    std::optional<std::string> get(std::string_view key) const;
    std::optional<int> get_int(std::string_view key) const;
    std::optional<double> get_float(std::string_view key) const;
    bool get_bool(std::string_view key, bool fallback) const;
    void set(std::string_view key, std::string_view value);
    void set_int(std::string_view key, int value);
    const std::vector<std::string>& raw_lines() const;
};
}
```
Line model: keep `raw` (cp1251 bytes), parse `key`, `value`, `kind` (key/bind/comment/blank). `set` replaces the value in the raw line (preserving the key, whitespace prefix, comment suffix) or appends `key value`. Binds preserved as raw. Add `utf8_to_cp1251` to `gl_assets` (reverse table, `?` for unmappable). `load`/`save` use raw bytes; `get` returns UTF-8.
Tests: comments, binds, user name with Cyrillic, int/float/bool, round-trip byte-identical for untouched files, set preserves comments/order, missing file → empty config / save creates.

- [x] TDD, implement, test, commit.

---

### Task 2: Command registry and console

**Files:** `src/core/{command,console}.*`, `tests/core/{test_command,test_console}.cpp`.

**Interfaces:**
```cpp
using Args = std::span<const std::string_view>;
using Handler = std::function<Result<std::string>(Args)>;
struct CommandInfo { std::string name, help; Handler handler; };
class CommandRegistry {
public:
    void add(std::string name, std::string help, Handler);
    Result<std::string> execute(std::string_view line) const;
    std::string help() const;
    std::optional<std::string> complete(std::string_view prefix) const;
    const std::vector<CommandInfo>& commands() const;
};
std::vector<std::string> split_command_line(std::string_view line); // quotes, escapes
class ConsoleBuffer {
public:
    void push(std::string line);
    const std::deque<std::string>& lines() const;
    std::string history_prev(); std::string history_next();
    void add_history(std::string line);
};
```
Tests: quoted args, empty line, unknown command, help, completion, history navigation.

- [x] TDD, implement, commit.

---

### Task 3: ASCII text renderer

**Files:** add `third_party/stb/stb_easy_font.h` (pinned stb commit), `src/render/{include/gl/render/text.hpp,src/text.cpp}`, `tests/render/test_text.cpp`.

**Interfaces:**
```cpp
namespace gl::render {
struct Color { std::uint8_t r,g,b,a; };
void draw_text(SDL_Renderer*, std::string_view ascii, float x, float y, float scale, Color);
float text_width(std::string_view ascii, float scale);
float text_height(float scale);
}
```
Use `stb_easy_font_print` to produce quads, convert to `SDL_Vertex` and draw with `SDL_RenderGeometry` (triangles). Non-ASCII bytes render as `?`.
Tests: text_width > 0; a dummy/software renderer produces non-empty pixels for a string; empty string is a no-op.

- [x] TDD, implement, commit.

---

### Task 4: Extract `LevelScreen`

**Files:** `src/app/**` (new `gl_app`), refactor `tools/glview/main.cpp`, `tests/render` unchanged.

**Interfaces:**
```cpp
namespace gl::app {
struct LevelScreenOptions { bool background_only=false; bool debug_overlay=false; };
class LevelScreen {
public:
    static Result<LevelScreen> load(SDL_Renderer*, gl::assets::GameVfs&, std::string_view level);
    void render(SDL_Renderer*, const LevelScreenOptions&, std::uint64_t elapsed_ms);
    void set_viewport(int screen_w, int screen_h);
    void pan(float world_dx, float world_dy);
    void pan_input(int x_axis, int y_axis, float delta_seconds);
    void zoom_at(float factor, float screen_x, float screen_y);
    void reset_camera();
    const gl::render::Camera& camera() const;
    const gl::game::Level& level() const;
};
}
```
`elapsed_ms` determines animation frame timing; there is no separate frame count. The stored
viewport set by `set_viewport` is the single source for culling/clamping. Move
loading/texture upload/draw/camera from `glview` into `LevelScreen`; `glview` keeps its CLI and
input, sets the viewport on init/resize, and calls the screen. No behavior change; screenshot
test still passes.

- [x] TDD/refactor, verify existing tests, commit.

---

### Task 5: State machine + ServerCore/ClientCore skeleton

**Files:** `src/core/{state,server}.*`, `tests/core/{test_state,test_server}.cpp`.

**Interfaces:**
```cpp
enum class AppState { boot, menu, level, exit };
class StateMachine { bool transition(AppState); AppState state() const; };

enum class EventType { paused_changed, config_changed, log };
struct Event { EventType type; bool flag=false; std::string text; };
class ServerCore {
public:
    void submit(Command);
    void tick(double dt);
    bool paused() const;
    std::vector<Event> drain_events();
};
class ClientCore { public: void on_event(const Event&); const std::vector<Event>& events() const; };
```
Tests: allowed/forbidden transitions; submit `gv_pause` toggles pause only on tick; events drained once; config_changed event.

Event payload convention (Task 6 relies on this):

- `paused_changed`: `flag` = new pause state; `key`/`value`/`text` empty.
- `config_changed`: `key` = setting name, `value` = new value, `text` = `value`;
  `flag` = bool value for `r_windowed`, `false` for string settings (`cl_username`).
- `log`: `text` = message; `key`/`value` empty.
- `ServerCore` never writes files; the shell owns `Config` and applies `config_changed`.
- `submit(Command)` is the parsed path; `submit(std::string_view)` is a parsing convenience.
- Extra arguments to `gv_pause`/`r_windowed`/`cl_username` log a usage error and change nothing.
- `level → level` is a reload transition at the state-machine level only: `ServerCore` is not
  reset; reload/world reset is owned by the shell.

- [x] TDD, implement, commit.

---

### Task 6: `glgame` shell (menu, console, commands, config)

**Files:** `tools/glgame/main.cpp`, `tools/glgame/CMakeLists.txt`, tests `tests/app/**`.

- Args: `--game-dir` (required unless screenshot/menu), `--config`, `--level`, `--width/--height`, `--menu`, `--screenshot`, `--exec` (repeatable).
- Load config from `<game-dir>/GoldenLand.ini` (or `--config`); apply `r_windowed`, `r_resolution`, `cl_username`.
- States: Menu (title + New Game/Continue(disabled)/Exit, arrows/Enter/mouse), Level (LevelScreen), Console overlay (`~`, text input, history, completion).
- Commands: `help`, `exit`, `d_console`, `gv_pause`, `gv_free_camera`, `r_resolution <w> <h>`, `r_windowed <0/1>`, `cl_username <name>`, `game_save <slot>` (stub).
- Config persistence: `r_windowed`/`r_resolution`/`cl_username` update Config; save on exit and after setters.
- `--exec` runs commands at boot; `--screenshot` renders menu (with `--menu`) or level.
- Tests: screenshot menu + level 1280×720 ≥2 colors; `--exec help`/unknown don't crash; config round-trip after `--exec "cl_username Test"` (inspect INI).
- [x] Implement, tests, commit.
- [x] Manual acceptance: run menu/console on a normal desktop (New Game `l1_1`, `~`,
  `help`, `gv_pause`, `exit`). Подтверждено пользователем 2026-10-20.

---

### Task 7: Acceptance + docs

- [x] `cmake --build build` and full `ctest` with `GL_GAME_DIR`: 8/8 passed.
- [x] `--screenshot` menu + level 1280×720 with exit 0 (non-uniform PNGs).
- [x] Update STATUS/PROJECT/research; record P2a automated acceptance; next P2b.
  DECISIONS updated: D-019 (`Result<void>`) and D-020 (missing config = defaults +
  warning, `save` creates it).
- [x] Commit.
- [x] Manual desktop acceptance: `glgame --game-dir I:\Games\Златогорье-2`, menu,
  New Game `l1_1`, `~`, `help`, `gv_pause`, `exit`. Подтверждено пользователем 2026-10-20.

## Self-Review

- Spec coverage: config, console/commands, states, menu, level, Server/Client skeleton, tests.
- Deferred: full `.sav`, full command set, Russian UI, Android.
