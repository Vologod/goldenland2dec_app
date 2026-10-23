# A8: cell mask / foreground occlusion — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Перекрывать персонажей foreground-масками уровня как в оригинале (`mask_45` / `Shot0001`).

**Architecture:** `gl::game::Level` хранит `LevelMask`; `app/mask_render` собирает foreground-текстуру «фон через alpha=0-силуэт»; `LevelScreen` вставляет маски в Y-отсортированные проходы теней/тел по baseline.

**Tech Stack:** C++20, CMake+Ninja, doctest, SDL2, LLVM-MinGW.

## Global Constraints

- C++20, `-Wall -Wextra -Werror`, `.clang-format`.
- Ассеты не коммитить; маски читаются из VFS во время выполнения.
- `gl_assets`/`gl_game` SDL-free; композиция маски — чистая функция в `app`/`assets`, без SDL.
- Маска: alpha=0 — occluding foreground; координаты — top-left мира.
- Baseline маски = `y + height`; tie-break — `x + width/2`.
- Полный `ctest` — с `$env:GL_GAME_DIR = "I:\Games\Златогорье-2"`.
- Toolchain PATH:
  `$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"`

---

### Task 1: `LevelMask` + загрузка масок

**Files:**
- Modify: `src/game/include/gl/game/level.hpp`
- Modify: `src/game/src/level_loader.cpp`
- Test: `tests/game/test_real_person_loader.cpp` (real l1_1) или новый `tests/assets/test_real_levels.cpp`

**Interfaces:**
- Produces:

```cpp
struct LevelMask {
    std::uint32_t number = 0;
    std::int32_t x = 0, y = 0;
    gl::assets::Image image;
};
// Level::masks
```

- [ ] **Step 1: Failing test**

Добавить в real-level тест (`tests/assets/test_real_levels.cpp` или
`tests/game/test_real_person_loader.cpp` — где уже загружается l1_1):

```cpp
TEST_CASE("real l1_1 loads mask descriptors") {
    // ... существующая загрузка level ...
    CHECK(level.masks.size() == 62);
    const auto it = std::find_if(level.masks.begin(), level.masks.end(),
                                 [](const gl::game::LevelMask& m) { return m.number == 45; });
    REQUIRE(it != level.masks.end());
    CHECK(it->x == 885);
    CHECK(it->y == 1275);
    CHECK(it->image.width == 687);
    CHECK(it->image.height == 741);
}
```

- [ ] **Step 2: Run fails**

```powershell
cmake --build build --target gl_assets_real_tests; build\bin\gl_assets_real_tests.exe --test-case="*mask descriptors*"
```
Expected: compile error `Level::masks`/`LevelMask` missing.

- [ ] **Step 3: Implement**

`level.hpp`:

```cpp
struct LevelMask {
    std::uint32_t number = 0;
    std::int32_t x = 0, y = 0;
    gl::assets::Image image;
};
// внутри Level:
std::vector<LevelMask> masks;
```

`level_loader.cpp` после загрузки background:

```cpp
level.masks.reserve(lvl->masks.size());
for (const LvlMask &mask : lvl->masks) {
    const std::string path = pack_path(
        level.pack, "bitmaps/masks/mask_" + std::to_string(mask.number) + ".csx");
    auto data = vfs.read(path);
    if (!data) {
        warn("level '" + level.name + "': " +
             contextual(data.error(), "skipping mask", path).message);
        continue;
    }
    auto csx = decode_csx(data.value());
    if (!csx) {
        warn("level '" + level.name + "': " +
             contextual(csx.error(), "skipping mask", path).message);
        continue;
    }
    LevelMask object;
    object.number = mask.number;
    object.x = static_cast<std::int32_t>(mask.x);
    object.y = static_cast<std::int32_t>(mask.y);
    object.image = to_image(csx.value());
    level.masks.push_back(std::move(object));
}
```

- [ ] **Step 4: Run passes**

```powershell
cmake --build build --target gl_assets_real_tests; build\bin\gl_assets_real_tests.exe --test-case="*mask descriptors*"
```

- [ ] **Step 5: Commit**

```powershell
git add src/game/include/gl/game/level.hpp src/game/src/level_loader.cpp tests/assets/test_real_levels.cpp
git commit -m "feat(game): load level foreground masks (A8)"
```

---

### Task 2: `compose_mask_foreground` + unit tests

**Files:**
- Create: `src/app/include/gl/app/mask_render.hpp`
- Create: `src/app/src/mask_render.cpp`
- Modify: `src/app/CMakeLists.txt`
- Test: `tests/app/test_mask_render.cpp` (или в `test_person_render.cpp`)
- Modify: `tests/app/CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
gl::assets::Image compose_mask_foreground(const gl::assets::Image& background,
                                          const gl::assets::Image& mask,
                                          int x, int y);
```

- [ ] **Step 1: Failing test**

```cpp
TEST_CASE("mask foreground copies background through alpha zero silhouette") {
    gl::assets::Image bg; bg.width = 4; bg.height = 4; bg.rgba.assign(4*4*4, 0);
    for (int i = 0; i < 16; ++i) { bg.rgba[i*4+0]=10; bg.rgba[i*4+1]=20; bg.rgba[i*4+2]=30; bg.rgba[i*4+3]=255; }
    gl::assets::Image mask; mask.width = 2; mask.height = 2; mask.rgba.assign(2*2*4, 255);
    // alpha=0 только в (0,0) и (1,1)
    mask.rgba[0*4+3]=0; mask.rgba[3*4+3]=0;
    const auto out = gl::app::compose_mask_foreground(bg, mask, 1, 1);
    REQUIRE(out.width == 2); REQUIRE(out.height == 2);
    CHECK(out.rgba[0*4+0] == 10); CHECK(out.rgba[0*4+3] == 255);
    CHECK(out.rgba[1*4+3] == 0);
    CHECK(out.rgba[2*4+3] == 0);
    CHECK(out.rgba[3*4+0] == 10); CHECK(out.rgba[3*4+3] == 255);
}

TEST_CASE("mask foreground skips out of bounds") {
    gl::assets::Image bg; bg.width = 1; bg.height = 1; bg.rgba.assign(4, 0);
    gl::assets::Image mask; mask.width = 2; mask.height = 1; mask.rgba.assign(2*4, 255);
    mask.rgba[0*4+3]=0; mask.rgba[1*4+3]=0;
    const auto out = gl::app::compose_mask_foreground(bg, mask, 0, 0);
    CHECK(out.rgba[0*4+3] == 255);
    CHECK(out.rgba[1*4+3] == 0);
}
```

- [ ] **Step 2: Run fails** (missing header/function)

- [ ] **Step 3: Implement**

```cpp
gl::assets::Image compose_mask_foreground(const gl::assets::Image& background,
                                          const gl::assets::Image& mask,
                                          int x, int y) {
    gl::assets::Image out;
    if (mask.width == 0 || mask.height == 0 || mask.rgba.size() != mask.width*mask.height*4)
        return out;
    out.width = mask.width; out.height = mask.height;
    out.rgba.assign(static_cast<std::size_t>(mask.width)*mask.height*4, 0);
    for (std::uint32_t py = 0; py < mask.height; ++py) {
        for (std::uint32_t px = 0; px < mask.width; ++px) {
            const std::size_t mi = (static_cast<std::size_t>(py)*mask.width + px)*4;
            if (mask.rgba[mi+3] >= 128) continue;
            const int bx = x + static_cast<int>(px), by = y + static_cast<int>(py);
            if (bx < 0 || by < 0 || bx >= static_cast<int>(background.width) ||
                by >= static_cast<int>(background.height)) continue;
            const std::size_t bi = (static_cast<std::size_t>(by)*background.width + bx)*4;
            const std::size_t oi = mi;
            out.rgba[oi+0] = background.rgba[bi+0];
            out.rgba[oi+1] = background.rgba[bi+1];
            out.rgba[oi+2] = background.rgba[bi+2];
            out.rgba[oi+3] = 255;
        }
    }
    return out;
}
```

- [ ] **Step 4: Run passes**

- [ ] **Step 5: Commit**

---

### Task 3: Merge masks into shadow/body passes

**Files:**
- Modify: `src/app/include/gl/app/level_screen.hpp`
- Modify: `src/app/src/level_screen.cpp`
- Modify: `tools/glview/main.cpp` (`--camera-x/--camera-y`)
- Test: `tests/render/test_glview_screenshot.cpp` (real screenshot)

**Interfaces:**
- Produces: `LevelScreen::MaskOverlay` + `mask_order_`.
- `draw_person` signature unchanged; new `draw_mask_overlay`.

- [ ] **Step 1: Failing screenshot test**

В `test_glview_screenshot.cpp` добавить прогон с камерой:
```cpp
REQUIRE(run_glview({"--game-dir", game_dir, "--level", "l1_1",
                    "--camera-x", "885", "--camera-y", "1275",
                    "--screenshot", path}) == 0);
CHECK(png_non_uniform(path));
```
Expected: CLI отвергает `--camera-x` (exit 1) или кадр не тот.

- [ ] **Step 2: Implement overlay building**

```cpp
struct MaskOverlay {
    gl::render::Texture texture;
    std::int32_t x = 0, y = 0;
    float baseline = 0.0f;
    float center_x = 0.0f;
};
std::vector<MaskOverlay> masks_;
std::vector<std::size_t> mask_order_;
```

В `load`:
```cpp
for (const gl::game::LevelMask& mask : screen.level_.masks) {
    gl::assets::Image foreground = compose_mask_foreground(
        screen.level_.background, mask.image, mask.x, mask.y);
    if (foreground.rgba.empty()) continue;
    auto texture = gl::render::Texture::from_image(renderer, foreground);
    if (!texture) { warn; continue; }
    screen.masks_.push_back(MaskOverlay{std::move(texture.value()), mask.x, mask.y,
        static_cast<float>(mask.y + mask.image.height),
        static_cast<float>(mask.x) + static_cast<float>(mask.image.width) / 2.0f});
}
screen.mask_order_.resize(screen.masks_.size());
std::iota(...);
std::stable_sort(screen.mask_order_, compare baseline, center_x);
```

- [ ] **Step 3: Merge in render**

```cpp
auto draw_pass = [&](PersonPass pass) {
    std::size_t pi = 0, mi = 0;
    while (pi < person_order_.size() || mi < mask_order_.size()) {
        bool person = mi >= mask_order_.size();
        if (!person && pi < person_order_.size()) {
            const PersonDrawKey& pk = person_keys_[person_order_[pi]];
            const MaskOverlay& mk = masks_[mask_order_[mi]];
            person = (pk.y < mk.baseline) ||
                     (pk.y == mk.baseline && pk.x <= mk.center_x);
        }
        if (person) {
            draw_person(renderer, persons_[person_order_[pi]], simulated_ms_, pass);
            ++pi;
        } else {
            const MaskOverlay& mk = masks_[mask_order_[mi]];
            gl::render::draw_texture(renderer, mk.texture, camera_,
                                     static_cast<float>(mk.x), static_cast<float>(mk.y),
                                     screen_w_, screen_h_);
            ++mi;
        }
    }
};
draw_pass(PersonPass::shadow);
draw_pass(PersonPass::body);
```
`draw_texture` работает с Texture; проверить сигнатуру в `camera.hpp`/`texture`/`level_screen` usage.

- [ ] **Step 4: Add glview verification flags**

В `tools/glview/main.cpp`: распарсить `--camera-x`/`--camera-y`,
`--place-person <index> <tile_x> <tile_y>`, `--no-masks`; после загрузки:
```cpp
if (options.place_person) {
    screen.value().simulation().place_person(
        options.place_person->index,
        gl::game::Tile{options.place_person->x, options.place_person->y});
}
screen.value().set_viewport(width, height);
if (options.camera_x && options.camera_y) {
    const auto& camera = screen.value().camera();
    screen.value().pan(*options.camera_x - camera.x, *options.camera_y - camera.y);
}
screen_options.draw_masks = !options.no_masks;
```
Обновить `--help`.

- [ ] **Step 5: Build/tests**

```powershell
cmake --build build
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\glview.exe --game-dir $env:GL_GAME_DIR --level l1_1 --camera-x 885 --camera-y 1275 --screenshot "$env:TEMP\a8_mask.png"
build\bin\glview_screenshot_tests.exe
```

- [ ] **Step 6: Commit**

---

### Task 4: Real mask tests, screenshots, docs

**Files:**
- Modify: `tests/assets/test_real_levels.cpp` (mask count/size/alpha count)
- Modify: `docs/research/p4b2-render-order.md`
- Modify: `docs/DECISIONS.md` (D-037)
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`

- [ ] **Step 1: Real mask test**

```cpp
CHECK(level.masks.size() == 62);
const auto& m45 = ...;
CHECK(compose_mask_foreground(level.background, m45.image, m45.x, m45.y)
          .rgba.size() == 687u*741u*4u);
std::size_t opaque = 0;
for (std::size_t i = 3; i < fg.rgba.size(); i += 4) if (fg.rgba[i]) ++opaque;
CHECK(opaque == 182090);
```

- [ ] **Step 2: Screenshot alignment + SHA**

```powershell
build\bin\glview.exe --game-dir "I:\Games\Златогорье-2" --level l1_1 --camera-x 885 --camera-y 1275 --screenshot "$env:TEMP\a8_tower.png"
Get-FileHash -Algorithm SHA256 "$env:TEMP\a8_tower.png"
```
Проверить: вышка не двоится; сохранить SHA-256 в STATUS.

- [ ] **Step 3: Full ctest**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
ctest --test-dir build --output-on-failure
```

- [ ] **Step 4: Decision D-037**

> A8: foreground occlusion — по `BLK_MDSC` (number,x,y) и
> `masks/mask_<number>.csx`; alpha=0 силуэт = occluding foreground, координаты
> top-left. Порт собирает текстуру «фон через alpha=0» и вставляет её в
> Y-отсортированные проходы теней/тел по baseline `y+height`; персонаж с
> меньшим baseline перекрывается, с большим — рисуется сверху. Это
> приближение `ApplyCellMask`; `r_masks_mode`/alt-маски не реализованы.

- [ ] **Step 5: Update docs/commit**

## Self-Review

- Spec coverage: загрузка масок (Task 1), композиция (Task 2), merge/CLI (Task 3), real/evidence (Task 4).
- Нет плейсхолдеров; сигнатуры/ключи заданы.
- Риск: `draw_texture` принимает `Texture` и camera — проверить сигнатуру;
  при несовпадении использовать `SDL_RenderCopyF` как в `level_screen.cpp`.
