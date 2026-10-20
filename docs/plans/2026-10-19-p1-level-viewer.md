# P1 — Level Viewer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Load a real Goldenland 2 level from the original `.pak` data and render it in an SDL2 window: pre-rendered background, static sprites, animated sprites, camera pan/zoom, and a screenshot mode.

**Architecture:** Extend `gl_assets` with `LVL`/`LAO`/`SEF` parsers and CSX frame slicing. Add `platform` (SDL2), `render` (textures/camera), `game` (LevelLoader/Level), and a new `glview` executable. `gl_assets` stays SDL-free.

**Tech Stack:** C++20, CMake+Ninja, LLVM-MinGW, SDL2 `release-2.32.10` (FetchContent), miniz/stb (already vendored), doctest.

## Global Constraints

- C++20, no extensions; `-Wall -Wextra -Werror` for first-party code.
- `gl_assets` must not include SDL/Windows/Android headers; only `platform`/`render`/`app` may.
- No game assets committed; read from `GL_GAME_DIR` / `--game-dir`.
- Level paths per spec: `levels/single/<name>/<name>.sef`, `levels/lvl/<pack>.lvl`,
  `levels/pack/<pack>/bitmaps/{layer.jpg,static/static_N.csx,animated/anim_i.csx,masks/,triggers/}`,
  `levels/pack/<pack>/data/animated/<pack>.lao`.
- Sprite positions are world pixels, drawn from the top-left corner.
- Tile 12×9 px, chunk 24×18 px, chunks are 2×2 tiles.
- Windows x64 target; no Android in P1.
- Every task ends with a commit.

## File Structure

```
third_party/sdl2/                  (FetchContent at configure; not committed)
cmake/SDL2.cmake
src/assets/include/gl/assets/{lvl,lao,sef,csx_frames}.hpp
src/assets/src/{lvl,lao,sef,csx_frames}.cpp
src/platform/include/gl/platform/{sdl_context,paths}.hpp
src/platform/src/{sdl_context,paths}.cpp
src/render/include/gl/render/{texture,camera}.hpp
src/render/src/{texture,camera}.cpp
src/game/include/gl/game/{level,level_loader}.hpp
src/game/src/{level,level_loader}.cpp
tools/glview/main.cpp
tests/assets/{test_lvl,test_lao,test_sef,test_csx_frames,test_real_levels}.cpp
tests/render/{CMakeLists.txt,test_glview_screenshot.cpp}
```

---

### Task 1: SDL2 + platform skeleton + glview window

**Files:**
- Create: `cmake/SDL2.cmake`, `src/platform/**`, `tools/glview/main.cpp`, `tools/glview/CMakeLists.txt`
- Modify: root `CMakeLists.txt`

**Interfaces:**
- Produces `gl::platform::SdlContext` (RAII SDL init/quit), `gl::platform::Window` (SDL_Window+SDL_Renderer),
  `glview` with `--help` and a window that closes on Esc.
- Produces `cmake/SDL2.cmake` exposing `SDL2::SDL2` and `SDL2::SDL2main`.

- [ ] **Step 1:** Add `cmake/SDL2.cmake` with pinned FetchContent:
  ```cmake
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                       GIT_TAG release-2.32.10 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL2)
  ```
  Root CMake includes it and adds `gl_platform` (links `SDL2::SDL2`), `gl_render`, `gl_game`, `tools/glview`.
- [ ] **Step 2:** Implement `SdlContext` (SDL_Init(SDL_INIT_VIDEO|TIMER), SDL_Quit) and
  `Window` (1280×720 resizable, `SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC`,
  `SDL_HINT_RENDER_SCALE_QUALITY=0`).
- [ ] **Step 3:** `glview --help` prints usage and exits 0. Without `--help`, open the window,
  pump events, close on Esc/quit. Build and run.
- [ ] **Step 4:** Commit `feat(platform): add SDL2 context and glview window`.

---

### Task 2: LVL parser

**Files:** `src/assets/include/gl/assets/lvl.hpp`, `src/assets/src/lvl.cpp`,
`tests/assets/test_lvl.cpp`, `tests/assets/test_real_levels.cpp`, CMake lists.

**Interfaces:**
```cpp
namespace gl::assets {
struct LvlVersion { std::uint16_t major=0, minor=0; };
struct LvlMapSize { std::uint32_t pixel_width=1, pixel_height=1; };
struct LvlTile { std::uint16_t relief=0, sound=0, mask=0xFFFF; };
struct LvlDescription { std::uint16_t param1=0, param2=0; std::uint32_t number=0;
                        std::int32_t x=0, y=0; std::string name; };
struct LvlMask { std::uint32_t number=0, x=0, y=0; };
struct LvlCellGroup { std::string name; std::vector<std::pair<std::uint16_t,std::uint16_t>> cells; };
struct LvlData {
    LvlVersion version; LvlMapSize map_size;
    std::uint32_t chunk_width=0, chunk_height=0;
    std::vector<std::array<LvlTile,4>> chunks;
    std::vector<LvlMask> masks;
    std::vector<LvlDescription> statics, animations, triggers;
    std::vector<LvlCellGroup> cell_groups;
    std::uint32_t level_floors=0;
};
Result<LvlData> parse_lvl(std::span<const std::uint8_t> data);
}
```
Parse the 12 blocks in order (8-byte name, u32 size, payload); decode MPSZ, MHDR, MDSC, SDSC,
ADSC, TDSC, CGRP, LFLS; validate the rest (SENV/WTHR/DOOR) and skip their contents. Strings are
`u32 len + cp1251`. Reject bad block names/sizes and out-of-range counts.

- [x] **Step 1:** Synthetic test with a minimal valid 12-block LVL + bad magic/size cases.
- [x] **Step 2:** Implement `parse_lvl`.
- [x] **Step 3:** Integration: iterate `levels/lvl/*.lvl` in `levels.pak`, parse all 175, assert 0 failures.
- [x] **Step 4:** Commit.

---

### Task 3: LAO + SEF parsers

**Files:** `lao.hpp/cpp`, `sef.hpp/cpp`, tests `test_lao.cpp`, `test_sef.cpp`; extend `test_real_levels.cpp`.

**Interfaces:**
```cpp
struct LaoInfo { std::uint32_t height=0, delay=0; };
Result<std::vector<LaoInfo>> parse_lao(std::span<const std::uint8_t> data);

struct SefData { std::string pack; bool internal_location=false; bool exit_to_global_map=false; int weather=-1; };
Result<SefData> parse_sef(std::span<const std::uint8_t> data);
```
LAO: file size % 8 == 0; pairs `(height, delay)`. SEF: text, lines may have `//` comments;
extract `pack: "..."`, `internal_location: N`, `exit_to_global_map: N`, `weather: N`.

- [ ] **Step 1:** Unit tests for both (including CRLF/tabs/comments).
- [ ] **Step 2:** Implement.
- [ ] **Step 3:** Integration: parse all `data/animated/*.lao` (0 failures) and `levels/single/l1_1/l1_1.sef`
  (`pack == "l1_1"`).
- [ ] **Step 4:** Commit.

---

### Task 4: CSX frame slicing

**Files:** `csx_frames.hpp/cpp`, `test_csx_frames.cpp`.

**Interfaces:**
```cpp
Result<std::vector<Image>> slice_csx_frames(const CsxImage& image, std::uint32_t frame_height);
```
Reject `frame_height == 0` or `frame_height > image.height`. Produce `ceil(height/frame_height)`
frames; the last may be shorter. Each frame copies its RGBA rows.

- [x] **Step 1:** Unit test with a synthetic 2×6 image, frame_height 2 → 3 frames, correct pixels.
- [x] **Step 2:** Implement.
- [x] **Step 3:** Commit.

---

### Task 5: LevelLoader + Level model

**Files:** `src/game/**`, `tests/assets/test_level_loader.cpp` (integration-gated).

**Interfaces:**
```cpp
struct LevelStatic { std::int32_t x=0, y=0; Image image; };
struct LevelAnimation { std::int32_t x=0, y=0; std::uint32_t delay_ms=0; std::vector<Image> frames; };
struct Level {
    std::string name, pack; std::uint32_t width=0, height=0;
    Image background;
    std::vector<LevelStatic> statics;
    std::vector<LevelAnimation> animations;
};
Result<Level> load_level(GameVfs& vfs, std::string_view level_name);
```
Loader:
1. read `levels/single/<name>/<name>.sef` → `pack`.
2. read `levels/lvl/<pack>.lvl` → parse.
3. read `levels/pack/<pack>/bitmaps/layer.jpg` → `decode_image`.
4. statics: for each `SDSC` description, read `bitmaps/static/static_<number>.csx`, `decode_csx`,
   convert to `Image` (store RGBA), position from description; missing file → log warning, skip.
5. animations: read `data/animated/<pack>.lao`; for `i` in `[0, min(ADSC.size, lao.size))`,
   read `bitmaps/animated/anim_<i>.csx`, `slice_csx_frames(image, lao[i].height)`, position from ADSC
   sorted by `number`; delay = lao[i].delay; missing → warning, skip.
- [x] **Step 1:** Integration test `l1_1`: width/height 2760×2016, statics 2, animations 2,
  animation frame counts equal `image.height / lao.height`.
- [x] **Step 2:** Implement.
- [x] **Step 3:** Commit.

---

### Task 6: Renderer, camera, background, screenshot

**Files:** `src/render/**`, `tools/glview/main.cpp`, `tests/render/**`.

**Interfaces:**
```cpp
class Texture { public: static Result<Texture> from_image(SDL_Renderer*, const Image&);
                 SDL_Texture* get() const; int width() const; int height() const; };
struct Camera { float x=0, y=0, zoom=1.0f; };
SDL_FRect to_screen(const Camera&, float world_x, float world_y, float w, float h);
void draw_background(SDL_Renderer*, const Texture&, const Camera&, int screen_w, int screen_h);
```
`glview`: parse `--game-dir`, `--level` (default `l1_1`), `--width/--height` (default 1280×720),
`--screenshot <png>`, `--frames N`. Loop: read input, update camera, draw background, present.
Screenshot: `SDL_RenderReadPixels` → RGBA → `stbi_write_png`.
- [x] **Step 1:** Render test: `SDL_VIDEODRIVER=dummy`, `SDL_RENDER_DRIVER=software`,
  `glview --level l1_1 --screenshot out.png --width 1280 --height 720`; assert PNG exists,
  1280×720, and has ≥2 distinct pixel colors.
- [x] **Step 2:** Implement window/background/camera/screenshot.
- [x] **Step 3:** Commit.

---

### Task 7: Draw statics and animations

**Files:** `tools/glview/main.cpp`, `src/game/**`, `tests/render/**`.

- Load statics/animations into `Texture`s at load time.
- Draw statics at `(x,y)` top-left, then animations; animation frame = `(now - start) / delay` looped.
- Optional `--frames N` advances N frames for deterministic screenshots.
- [ ] **Step 1:** Extend the screenshot test: capture after 5 frames; assert the image differs from
  a background-only render (sprite pixels present) and dimensions are correct.
- [ ] **Step 2:** Implement.
- [ ] **Step 3:** Commit.

---

### Task 8: Input, debug overlay, acceptance

**Files:** `tools/glview/main.cpp`, docs.

- Camera: WASD/arrows `600/zoom` px/s, LMB drag, wheel zoom to cursor (0.25…4), `Home` reset,
  `Esc` quit, `F1` debug rectangles.
- Clamp camera to the map.
- Status: `gl::log` info level prints level size/counts on load.
- [ ] **Step 1:** Manual run `glview --game-dir I:\Games\Златогорье-2 --level l1_1`; pan/zoom.
- [ ] **Step 2:** Run screenshot + ctest (unit/integration/render) green.
- [ ] **Step 3:** Update `docs/STATUS.md` (P1 accepted), `docs/PROJECT.md` P1 `[x]`,
  `docs/research/` with level/anchor facts.
- [ ] **Step 4:** Commit.

---

## Self-Review

- Spec coverage: SEF/LVL/LAO/CSX-frames, loader, background/static/animation rendering,
  camera/input, screenshot, tests — all mapped to tasks.
- Types consistent (`Image`, `CsxImage`, `LvlData`, `Level`, `Camera`).
- Deferred: masks/depth, tiles, hero, audio, Android — explicitly out of P1.
