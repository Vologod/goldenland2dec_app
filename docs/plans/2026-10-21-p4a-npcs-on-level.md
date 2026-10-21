# P4a — NPCs on the Level Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development or executing-plans. Steps: `- [ ]`.

**Goal:** Load the 16 NPCs of `l1_1` from SEF/person scripts and draw their standing (`rt_stay`) animation with direction.

**Architecture:** `gl_assets` parses PAD and person `.scr`, and extends SEF with `persons`. `gl_game` resolves `res_name` → sprite dir and loads PAD + `rt_stay` textures. `gl_app::LevelScreen` renders them.

**Tech Stack:** C++20, existing `gl_assets`/`gl_game`/`gl_render`/SDL2, doctest.

## Global Constraints

- C++20; `-Wall -Wextra -Werror`; `gl_assets`/`gl_game` SDL-free.
- Game data only via `GameVfs`; no assets committed.
- Missing NPC resources → warning + skip, never fatal.
- Positions: tile `(x,y)` → pixels `(x*12, y*9)`; direction 0..7 per SEF; frame from PAD delay.
- Every task ends with a commit.

## File Structure

```
src/assets/include/gl/assets/pad.hpp
src/assets/src/pad.cpp
src/assets/include/gl/assets/person_script.hpp
src/assets/src/person_script.cpp
src/assets/include/gl/assets/sef.hpp        (extend: persons)
src/assets/src/sef.cpp
src/game/include/gl/game/person.hpp
src/game/src/person.cpp
tests/assets/{test_pad.cpp,test_person_script.cpp,test_real_persons.cpp}
src/app/src/level_screen.cpp                (render persons)
tests/app/test_glgame_screenshot.cpp        (NPC screenshot)
```

---

### Task 1: PAD parser (`gl_assets`)

Transcribe the format from `GoldenLandEditor/src/parsers/PAD_Parser.cpp` (secondary source).

**Interface:**
```cpp
namespace gl::assets {
enum class PadAnimationType : std::uint32_t { rt_stay=1, rt_fun=2, tb_stay=4, tb_fun=8,
    tb_go=0x10, rt_go=0x20, cast=0x40, suffer=0x80, die=0x100, ss_attack=0x400,
    hits0=0x10000, hits1=0x20000, hits2=0x40000, hits3=0x80000 };
struct PadCrop { std::int16_t x,y,w,h; };
struct PadAnimation {
    PadAnimationType type; std::int32_t delay, frames_per_row, frame_width, frame_height;
    std::int32_t anchor_x, anchor_y; float movement_x, movement_y;
    std::vector<std::vector<PadCrop>> crops;          // rows × frames
    std::int32_t shadow_width, shadow_height, shadow_anchor_x, shadow_anchor_y;
    std::vector<std::vector<PadCrop>> shadow_crops;
};
Result<std::vector<PadAnimation>> parse_pad(std::span<const std::uint8_t> data);
bool pad_is_go(PadAnimationType);
}
```
- Magic `PAD `, `u32 dataSize`, `u32 masks`; each animation: type, size, delay,
  framesPerRow, frameWidth/Height, anchors, movementX/Y, crops table, shadow frame,
  shadow crops; exact payload consumption.
- Direction tables (person/shadow, 8/16) as constants; helpers for row/mirror.
- Tests: synthetic one-animation PAD + corrupt sizes; all real `.pad` in `persons.pak`
  parse with 0 failures (gated `GL_GAME_DIR`), count > 0.

- [x] TDD, implement, commit.

---

### Task 2: Person script + SEF persons

**Person `.scr`:** simple `key: value` + blocks `{ ... }` with nested `key value` lines;
comments `//`; strings quoted. Expose:
```cpp
struct PersonDef {
    std::string res_name;
    std::int32_t level=0;
    std::map<std::string, std::string> values;   // raw key→value (stats/skills/etc.)
};
Result<PersonDef> parse_person_script(std::span<const std::uint8_t> data);
```
(At least `res_name` + `level`; keep raw values for P4b/P5.)
Tests: synthetic; all 15 distinct `l1_1` person scripts parse; missing `res_name` → format.

**SEF persons:** extend `SefData`:
```cpp
struct SefPerson {
    std::string name;
    std::int32_t x=0,y=0;                 // tiles
    std::string direction;                // "DOWN_RIGHT", ...
    std::string route_type, route;
    std::int32_t radius=0, delay_min=0, delay_max=0;
    std::string tribe, dialog, inventory;
};
std::vector<SefPerson> persons;
```
Parse the `persons: { name: "..."; { position x y; direction "..."; ... } }` text.
Tests: `l1_1` → 16 persons; known first/last fields; synthetic.

- [x] TDD, implement, commit.

---

### Task 3: Person loader (`gl_game`)

```cpp
struct PersonTexture { gl::assets::Image image; }; // CPU image for now
struct PersonInstance {
    std::string name;
    std::int32_t tile_x=0, tile_y=0;
    std::uint8_t direction=0;             // 0..7
    std::string dialog, inventory;
    std::string res_name;
    gl::assets::PersonDef def;
    std::vector<gl::assets::PadAnimation> pad;
    gl::assets::Image sprite;             // rt_stay sheet
    gl::assets::Image shadow;             // optional
};
Result<std::vector<PersonInstance>> load_persons(GameVfs&, const std::string& level_name);
```
- Read `levels/single/<level>/<level>.sef`; for each person derive
  `scripts/persons/<lower(name)>.scr`; parse; `res_name` → `persons/<lower(res_name)>/`
  (`<res>.pad`, `animation/rt_stay.csx`, `shadows/rt_stay.csx`); decode CSX; keep PAD.
- Missing script/sprite/PAD → warning + skip.
- Tests: `l1_1` → 16 instances; each has a PAD with `rt_stay`; sprite size matches
  `frame_width*frames_per_row × frame_height*8` (or the sheet dimensions); direction mapping.

- [x] TDD, implement, commit.

---

### Task 4: Render NPCs (`gl_app`)

- `LevelScreen` loads persons (Task 3) and uploads standing textures (and shadows if cheap);
  prefer `rt_stay`, fall back to `tb_stay` via `PersonInstance::stay_type` (real data: 6/16).
- Draw after background/static/animations (P4a order; Y-sorting in P4b), at
  `(tile_x*12, tile_y*9)` with PAD anchor: screen position = world pos − anchor.
- Frame: `(simulation_time_ms / delay) % frames_per_row`; row from direction via PAD
  tables; mirror when the direction maps to a mirrored row (swap U).
- Optional shadow from `shadows/rt_stay.csx` with `shadowFrame` anchors; behind sprite.
- Screenshot test: `l1_1` with NPCs vs a debug flag `--no-persons` (add to glview/glgame)
  → images differ, 1280×720; NPC count 16 in logs.
- Tests: existing screenshots updated; new no-persons comparison.

- [x] Implement, tests, commit.

---

### Task 5: Acceptance + docs

- Full `ctest` with `GL_GAME_DIR`; NPC screenshot; count 16; all `.pad` parse.
- Manual: `glgame --game-dir ... --level l1_1` shows NPCs; Esc/menu.
- Docs: STATUS/PROJECT/plan/research; P4a accepted; next P4b.
- Commit.

## Self-Review

- Covers PAD, person scripts, SEF persons, loader, rendering, tests, docs.
- Deferred: movement/pathfinding/AI (P4b), click→dialog (P4c), combat (P5).
