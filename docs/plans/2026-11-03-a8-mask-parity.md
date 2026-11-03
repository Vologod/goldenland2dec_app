# A8-parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Заменить baseline-приближение A8 точной engine-моделью `ApplyCellMask`: маска применяется по `LVL tile.mask`/BLK_MDSC и колонкам клетки, а не по `mask.y+height`.

**Architecture:** `LVL tile.mask` переносится в `gl_game::Level` как `TileMaskGrid` + mapping index→BLK_MDSC number. Чистая SDL-free функция `select_cell_mask` воспроизводит `Client.dll FUN_1202BC54`. `LevelScreen` рисует маску только для выбранных клеток/колонок и убирает baseline-merge.

**Tech Stack:** C++20, CMake/Ninja, doctest, SDL2; Ghidra headless и `ReadProcessMemory` для research; данные `I:\Games\Златогорье-2`.

## Global Constraints

- C++20; слои только вниз; `gl_assets`/`gl_game` без SDL.
- Оригинал — источник истины; без RVA/дампа в research код не менять.
- Никаких «TBD/TODO» в коде; неизвестное — в `STATUS` Open Questions.
- Ассеты не коммитить; real-тесты — только с `GL_GAME_DIR` (skip 77).
- Формат/поведение сверять с `Client.dll` `FUN_1202BC54`/`FUN_1202F17C`.
- После задач: сборка, целевые тесты; в конце полный `ctest`.

---

### Task 1: Research — точная семантика ApplyCellMask (gate, без кода)

**Files:**
- Create: `docs/research/a8-mask-parity.md`
- Modify: `docs/STATUS.md` (только после результата)

**Interfaces:**
- Produces: точные RVA, биты flags, размер/per-column layout, mapping
  `tile.mask`→BLK_MDSC, решения для hero `(180,24)` и стража у ворот.
  Без этого Task 2–4 не начинать.

- [ ] **Step 1: Зафиксировать текущие факты**

Записать в `docs/research/a8-mask-parity.md`: SHA-256 `Client.dll`
`FBF213B4…`, `GoldenLand.exe` `7854019E…`; `FUN_1202BC54`, `FUN_1202F17C`,
`FUN_1202E370`, host `DAT_12110F3C` vtable `+0x5C/+0x44`; grid
`renderer+0x8344` (0x28 байт/клетка, 4 subcell, mask id `+0x10`, флаги
`+0x00..0x0C`); вывод `+0x839C`/`+0x838C`.

- [ ] **Step 2: Декомпилировать все ветки**

Команды (headless, уже импортированный проект):
```powershell
$env:JAVA_HOME="I:\goldenland2dec\.local\toolchains\jdk-21.0.12.1+1"
$proj="C:\Users\volog\AppData\Local\Temp\opencode\p4d\ghidra_proj"
& .local\toolchains\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat `
  $proj glclient -process "Client.dll" -noanalysis `
  -scriptPath "$env:TEMP\opencode\ghidra-scripts" -postScript "DecompFuncs.java" `
  1202bc54 1202f17c 1202e370 1202b608
```
Записать: какой бит flags означает «клетка в маске»/«перекрывать»;
направления сканов (вправо/вниз), границы (`+0x95A0/+0x95A4`), сколько
колонок на клетку (`0x3C/4`), как renderer использует `+0x839C`/`+0x838C`.

- [ ] **Step 3: Найти builder сетки**

Проследить host-объект `DAT_12110F3C` (vtable `+0x5C`) и/или найти в
`GoldenLand.exe` функцию, заполняющую 0x28-записи из `LVL tile.mask`.
Команды: Ghidra `RefsTo.java` по `DAT_12110F3C`, поиск `0x8344`/`0x95A0`
в `GoldenLand.exe` (`analyzeHeadless ... glexe`). Записать, откуда берутся
flags и как `tile.mask` индекс превращается в `mask_id`.

- [ ] **Step 4: Live-сверка**

Запустить оригинал, загрузить `l1_1` (slot0), прочитать `renderer+0x8344`
для клетки hero `(180,24)` и соседей; сравнить с `tile.mask` индексом 6 и
BLK_MDSC (`lvl.masks[6].number=8`). Записать grid-запись (flags/id) и
решение `ApplyCellMask` для hero. Повторить для стража у ворот (найти
entity/координаты). Инструмент — `ReadProcessMemory` (не debugger).

- [ ] **Step 5: Gate**

В research есть RVA, биты, layout колонок, mapping и два live-кейса.
`STATUS` обновлён: «research gate passed». Если live-сверка невозможна —
`BLOCKED` с точными вопросами; Task 2–4 не начинать.

- [ ] **Step 6: Commit**

```bash
git add docs/research/a8-mask-parity.md docs/STATUS.md
git commit -m "docs(a8): derive ApplyCellMask semantics for parity"
```

---

### Task 2: `TileMaskGrid` в Level

**Files:**
- Modify: `src/game/include/gl/game/level.hpp`
- Modify: `src/game/src/level_loader.cpp`
- Test: `tests/game/test_tile_mask.cpp`, `tests/game/CMakeLists.txt`
- Test: `tests/assets/test_a8_mask_real.cpp`, `tests/assets/CMakeLists.txt`

**Interfaces:**
- Produces:
```cpp
struct TileMaskGrid {
    int width = 0;
    int height = 0;
    std::vector<std::uint16_t> indices;
    std::uint16_t at(int x, int y) const;
};
```
`Level` получает `TileMaskGrid tile_masks;` и
`std::vector<std::uint32_t> mask_numbers;` (`mask_numbers[i] = lvl.masks[i].number`).
`tile_masks.at` вне границ возвращает `0xFFFF`.

- [ ] **Step 1: Failing tests**

`tests/game/test_tile_mask.cpp`:
```cpp
#include "gl/game/level.hpp"
#include <doctest/doctest.h>

TEST_CASE("TileMaskGrid returns none outside and stored index inside") {
    gl::game::TileMaskGrid grid;
    grid.width = 2;
    grid.height = 2;
    grid.indices = {0, 1, 2, 0xFFFF};
    CHECK(grid.at(0, 0) == 0);
    CHECK(grid.at(1, 0) == 1);
    CHECK(grid.at(1, 1) == 0xFFFF);
    CHECK(grid.at(-1, 0) == 0xFFFF);
    CHECK(grid.at(2, 1) == 0xFFFF);
}
```
`tests/assets/test_a8_mask_real.cpp`:
```cpp
TEST_CASE("real l1_1 tile mask index maps to BLK_MDSC" * doctest::skip(game_dir_missing())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    CHECK(level->mask_numbers.size() == 62);
    CHECK(level->tile_masks.at(180, 24) == 6);
    REQUIRE(level->mask_numbers.size() > 6);
    CHECK(level->mask_numbers[6] == 8);
}
```
`game_dir_missing()` — локальный helper по образцу `test_real_data.cpp`.

- [ ] **Step 2: Run, verify fail**

```powershell
cmake --build build
build\tests\game\gl_game_tests.exe --test-case="*TileMaskGrid*"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\tests\assets\gl_assets_real_tests.exe --test-case="*tile mask*"
```
Expected: compile error / missing fields.

- [ ] **Step 3: Implement**

`level.hpp`: add `TileMaskGrid` (above `Level`) and fields.
`level_loader.cpp` после `level.walkable = build_walk_grid(...)`:
```cpp
level.mask_numbers.reserve(lvl->masks.size());
for (const auto& mask : lvl->masks) level.mask_numbers.push_back(mask.number);

level.tile_masks.width = level.walkable.width;
level.tile_masks.height = level.walkable.height;
level.tile_masks.indices.assign(
    static_cast<std::size_t>(level.tile_masks.width) * level.tile_masks.height, 0xFFFF);
for (std::uint32_t cx = 0; cx < lvl->chunk_width; ++cx) {
    for (std::uint32_t cy = 0; cy < lvl->chunk_height; ++cy) {
        const auto& chunk = lvl->chunks[static_cast<std::size_t>(cx) * lvl->chunk_height + cy];
        for (std::uint32_t t = 0; t < 4; ++t) {
            const std::uint32_t tx = cx * 2 + (t & 1u);
            const std::uint32_t ty = cy * 2 + (t >> 1u);
            level.tile_masks.indices[static_cast<std::size_t>(ty) * level.tile_masks.width + tx] =
                chunk[t].mask;
        }
    }
}
```
Add out-of-range warning once after fill: iterate indices, if `value != 0xFFFF && value >= mask_numbers.size()` warn with cell.

`TileMaskGrid::at` in `level.cpp` (create if none) or inline:
```cpp
std::uint16_t TileMaskGrid::at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) return 0xFFFF;
    return indices[static_cast<std::size_t>(y) * width + x];
}
```

- [ ] **Step 4: Run tests**

Expected: both PASS.

- [ ] **Step 5: Commit**

```bash
git add src/game/include/gl/game/level.hpp src/game/src/level.cpp src/game/src/level_loader.cpp tests/game/test_tile_mask.cpp tests/game/CMakeLists.txt tests/assets/test_a8_mask_real.cpp tests/assets/CMakeLists.txt
git commit -m "feat(a8): expose LVL tile mask grid in the game level"
```

---

### Task 3: `select_cell_mask` (engine rule)

**Files:**
- Create: `src/game/include/gl/game/mask_selection.hpp`
- Create: `src/game/src/mask_selection.cpp`
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_mask_selection.cpp`, `tests/game/CMakeLists.txt`

**Interfaces:**
- Consumes: `TileMaskGrid`, `mask_numbers` (Task 2).
- Produces:
```cpp
struct CellMaskSelection {
    std::uint16_t mask_index = 0xFFFF;
    std::uint32_t mask_number = 0;
    bool applies = false;
    std::vector<std::uint8_t> columns; // per engine column, size from research
};
CellMaskSelection select_cell_mask(const TileMaskGrid& grid,
                                   std::span<const std::uint32_t> mask_numbers,
                                   int x, int y);
```
`columns` size and contents are defined by Task 1 research; `columns[i] != 0`
means mask covers that column.

- [ ] **Step 1: Failing tests**

Synthetic grid 3×3, one mask index `0`.
```cpp
TEST_CASE("select_cell_mask ignores cells without a mask") {
    // index 0xFFFF => applies=false, columns empty
}
TEST_CASE("select_cell_mask rejects an edge cell without continuation") {
    // index 0 at (1,1), no index 0 at right/down => applies=false (per research)
}
TEST_CASE("select_cell_mask accepts an interior cell") {
    // 3x3 all index 0 => applies=true, mask_number from mapping, columns per research
}
TEST_CASE("select_cell_mask ignores out-of-range mask index") {
    // index 99, mask_numbers size 1 => applies=false
}
```
Concrete assertions are filled from Task 1 research (column count, which
cells apply); until then tests compile but are skipped behind a research
constant.

- [ ] **Step 2: Run, verify fail**

```powershell
cmake --build build
build\tests\game\gl_game_tests.exe --test-case="*select_cell_mask*"
```
Expected: missing symbols.

- [ ] **Step 3: Implement exactly from research**

Код повторяет `FUN_1202BC54` для одной клетки:
- читает `mask_index = grid.at(x,y)`;
- проверяет `mask_index < mask_numbers.size()`;
- выполняет scans right/down и flags-биты, зафиксированные в
  `docs/research/a8-mask-parity.md`;
- заполняет `columns` в том же размере, что engine (`+0x839C` layout);
- `mask_number = mask_numbers[mask_index]`.

Никаких baseline/`mask.y+height` в функции нет.

- [ ] **Step 4: Run tests**

```powershell
build\tests\game\gl_game_tests.exe --test-case="*select_cell_mask*"
```
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/game/include/gl/game/mask_selection.hpp src/game/src/mask_selection.cpp src/game/CMakeLists.txt tests/game/test_mask_selection.cpp tests/game/CMakeLists.txt
git commit -m "feat(a8): implement engine cell mask selection"
```

---

### Task 4: Integrate into LevelScreen

**Files:**
- Modify: `src/app/include/gl/app/level_screen.hpp`
- Modify: `src/app/src/level_screen.cpp`
- Modify: `tests/render/test_glview_screenshot.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp` (если кадры меняются)

**Interfaces:**
- Consumes: `select_cell_mask`, `Level::tile_masks/mask_numbers`.
- Produces: маски рисуются только для выбранных клеток/колонок; baseline
  `MaskOverlay.baseline/center_x` и глобальный `mask_order_` удалены.

- [ ] **Step 1: Failing screenshot test**

`tests/render/test_glview_screenshot.cpp`:
```cpp
TEST_CASE("glview applies masks only to masked cells") {
    // hero at l1_1 (180,24): original shows body+shadow fully (from research gate)
    // compare glview --hero-wear at camera clamped 1480,0 with/without masks:
    // masked frame must equal unmasked at the hero pixels; guard cell still masks.
}
```
Тест использует эталон из Task 1 (пиксели/координаты); до research
ожидаемые значения не фиксируются.

- [ ] **Step 2: Run, verify fail**

```powershell
cmake --build build
build\tests\render\glview_screenshot_tests.exe --test-case="*masked cells*"
```
Expected: FAIL (current baseline clips hero).

- [ ] **Step 3: Implement**

`level_screen.hpp`:
- удалить `MaskOverlay::baseline/center_x`, `mask_order_`;
- добавить `void draw_cell_mask(SDL_Renderer*, std::size_t person_instance,
  PersonPass) const;`
- маски остаются `std::vector<MaskOverlay{texture,x,y,number}>`.

`level_screen.cpp`:
- удалить построение `mask_order_` и baseline-merge в `draw_pass`;
- в ветке person/hero после отрисовки тела/тени вызвать
  `draw_cell_mask` для текущей клетки; функция:
  - `selection = select_cell_mask(level_.tile_masks, level_.mask_numbers, tile.x, tile.y)`;
  - если `!selection.applies` — вернуть;
  - найти `MaskOverlay` по `selection.mask_number`;
  - нарисовать foreground с clipping по `selection.columns` (колонки из
    research; если research даёт cell-level, рисовать весь foreground);
  - для shadow pass и body pass — одинаково.
- для hero: клетка `tile_at(hero.x,hero.y)`, после `draw_hero_wear`;
- для NPC: клетка `tile_at(movement.x,movement.y)`.

- [ ] **Step 4: Update A8 tests**

Заменить baseline-ожидания A8 на engine-модель; сохранить проверку
`mask_45`/`mask_1` там, где решения совпадают. Полный `glview_screenshot_tests`
и `glgame_screenshot_tests` — зелёные.

- [ ] **Step 5: Run**

```powershell
cmake --build build
build\tests\render\glview_screenshot_tests.exe
build\tests\app\glgame_screenshot_tests.exe
build\tests\game\gl_game_tests.exe
```
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/app/include/gl/app/level_screen.hpp src/app/src/level_screen.cpp tests/render/test_glview_screenshot.cpp tests/app/test_glgame_screenshot.cpp
git commit -m "fix(a8): apply masks by engine cell selection"
```

---

### Task 5: Real parity evidence + docs + manual gate

**Files:**
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`, `docs/DECISIONS.md`,
  `docs/research/a8-mask-parity.md`
- Maybe modify: existing A8 spec (superseded) with link.

- [ ] **Step 1: Full ctest**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```
Record counts/duration.

- [ ] **Step 2: Visual parity**

```powershell
build\bin\glview.exe --game-dir "I:\Games\Златогорье-2" --level l1_1 `
  --hero-wear --no-persons --camera-x 1480 --camera-y 0 `
  --screenshot "$env:TEMP\a8_hero_door.png" --sim-ms 0
build\bin\glview.exe --game-dir "I:\Games\Златогорье-2" --level l1_1 `
  --camera-x 1480 --camera-y 0 --screenshot "$env:TEMP\a8_guard_gate.png" --sim-ms 0
```
Сравнить с оригиналом в тех же точках (live/скриншоты): hero у двери не
должен ложно перекрываться; тень стража не обрезана.

- [ ] **Step 3: Docs**

- `STATUS`: A8-parity implemented/automated passed/manual gate status;
  P4d manual gate можно закрыть после A8 (если других дефектов нет).
- `PROJECT`: A8 parity status.
- `DECISIONS`: D-044 — engine cell mask model, tile.mask index, baseline
  removed; ссылка на research/spec.
- `research`: итоговые RVA/биты/дамп.

- [ ] **Step 4: Commit**

```bash
git add docs/STATUS.md docs/PROJECT.md docs/DECISIONS.md docs/research/a8-mask-parity.md
git commit -m "docs(a8): record cell mask parity and acceptance"
```

---

## Self-Review

- Spec coverage: точная модель (`ApplyCellMask`) → Task 1/3; `tile.mask`
  grid/mapping → Task 2; интеграция/Y-sort → Task 4; тесты/гейт/доки →
  Task 5.
- Placeholder scan: research-зависимые значения явно помечены как результат
  Task 1 gate; код задач 2/4 полностью конкретен. Task 3/4 не начинать до
  gate.
- Type consistency: `TileMaskGrid`, `mask_numbers`, `CellMaskSelection`,
  `select_cell_mask`, `draw_cell_mask` используются одинаково.
