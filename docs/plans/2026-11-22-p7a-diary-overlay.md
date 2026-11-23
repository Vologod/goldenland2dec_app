# P7a — Quest journal overlay implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Read-only quest journal overlay for the loaded save.

**Architecture:** SDL-free `DiaryRow` view model; `gl::app::DiaryOverlay`
renders the original diary frame and handles list/detail input; glgame
loads the lit SDB and toggles the overlay with `J`/`--diary`.

**Tech Stack:** C++20, SDL2, doctest, CMake/Ninja.

## Global Constraints

- SDL only in `app/tools`; `game` stays SDL-free.
- `AGENTS.md` naming; no unnecessary comments.
- Docs UTF-8 without BOM via `write`/`edit`.
- Full `ctest` with `GL_GAME_DIR`.

---

### Task 1: Diary view model

**Files:**
- Create: `src/game/include/gl/game/diary_view.hpp`
- Create: `src/game/src/diary_view.cpp`
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_diary_view.cpp` (new)
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Produces `DiaryRow`, `build_diary_rows`, `format_diary_date`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "gl/assets/sdb.hpp"
#include "gl/game/diary_view.hpp"
#include <doctest/doctest.h>
#include <string>
#include <vector>

TEST_CASE("diary rows map text ids and keep save order") {
    gl::assets::Sdb texts;
    // Constructing Sdb is private; load a tiny synthetic SDB instead.
}
```

Use the public `Sdb::parse` with a synthetic body. Inspect
`tests/assets/test_sdb.cpp` for the signature format and copy its helper
block.

Assertions:
- two entries with text ids 7, 9 map to values;
- an unknown id becomes `#12`;
- rows keep input order;
- `format_diary_date` yields `05.03.2024 07:08` for a synthetic datetime.

- [ ] **Step 2: Implement**

`diary_view.hpp/cpp` as in the spec; text lookup uses
`texts.find(entry.text_id)` and falls back to `#` + id.

- [ ] **Step 3: Run**

`cmake --build build; build\bin\gl_game_tests.exe --test-case="*diary row*"`

### Task 2: DiaryOverlay

**Files:**
- Create: `src/app/include/gl/app/diary_overlay.hpp`
- Create: `src/app/src/diary_overlay.cpp`
- Modify: `src/app/CMakeLists.txt`
- Test: `tests/app/test_diary_overlay.cpp` (new)
- Modify: `tests/app/CMakeLists.txt`

**Interfaces:**
- Consumes `DiaryRow`.
- Produces `DiaryOverlay` with `open`, `close`, `visible`, `render`,
  `set_font`, `handle_key`, `handle_wheel`, `handle_mouse`,
  `selected`, `detail_scroll`, `row_count`.

- [ ] **Step 1: Write the failing test**

Headless SDL software renderer (follow `test_inventory_overlay.cpp`):
- open with 3 rows and the real VFS/font;
- render and check non-background pixels in the left list, right detail
  and close regions;
- `handle_key(SDLK_DOWN)` selects row 1;
- `handle_wheel(1)` changes `detail_scroll`;
- click on close rect hides the overlay;
- empty rows render without crash.

- [ ] **Step 2: Implement**

Layout/constants as in the spec. Use `GameVfs::read` + `decode_image` +
`Texture::from_image` for `main.bmp`, `page1.bmp`, `close_l.bmp`.
Wrap/measure with `Font`; when font is null, skip text but still render
the frame. Clip the detail with `SDL_RenderSetClipRect`.

- [ ] **Step 3: Run**

`cmake --build build; build\bin\diary_overlay_tests.exe`

### Task 3: glgame integration

**Files:**
- Modify: `src/app/include/gl/app/input_gate.hpp`
- Modify: `tests/app/test_level_click.cpp`
- Modify: `tools/glgame/main.cpp`
- Test: `tests/app/test_glgame_screenshot.cpp`

**Interfaces:**
- `level_input_enabled(..., bool diary_active = false)`.

- [ ] **Step 1: Input gate**

Add `diary_active` and `CHECK_FALSE(level_input_enabled(false, false,
false, false, false, true));` in `test_level_click.cpp`.

- [ ] **Step 2: Overlay wiring**

Add members `gl::app::DiaryOverlay diary_overlay_;` and
`std::vector<gl::game::DiaryRow> diary_rows_;`.

`open_diary()`:
- require `loaded_payload_`;
- `decode_diary_entries(*loaded_payload_)`;
- read/parse `sdb/diary/single_quest_lit.sdb`;
- `diary_rows_ = build_diary_rows(entries, sdb)`;
- `diary_overlay_.open(vfs_, diary_rows_)`, `set_font(inventory_font())`.

`toggle_diary()` and `--diary` call it; `J`/Esc handled before level
input; mouse/wheel/touch route to the diary when visible; render after
inventory/trade.

- [ ] **Step 3: Screenshot test**

Add a `glgame --load-save --slot 0 --diary --screenshot ...` case that
checks the diary region differs from the bare level screenshot (or from
an empty frame) by a threshold.

- [ ] **Step 4: Run**

`cmake --build build; build\bin\diary_overlay_tests.exe;
build\bin\glgame_screenshot_tests.exe --test-case="*diary*"` (or the
suite's diary case name).

### Task 4: Docs and full verification

**Files:**
- Modify: `docs/DECISIONS.md` (D-110)
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`
- Modify: `docs/research/formats.md` (diary UI assets note)

- [ ] **Step 1: Docs**
  - D-110: data source, UI, controls, residuals.
  - STATUS: P7a done with commands/output.
  - PROJECT: P7 progress and P7a.
  - formats.md: `scripts/ui/diary.scr` tabs/button layout and
    `engineres/diary` assets.
- [ ] **Step 2: Full check**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```

## Follow-up from manual gate (2026-11-22, not implemented)

- Sort active (`end` absent) first, completed (`end` present) last;
  newest-first within groups.
- Strike completed stage paragraphs; expose all description text with
  correct clip/scroll/max and per-record stage state.
- Align the five tab labels to the `page1.bmp` strip.

## Self-review

- Spec coverage: view model, overlay, gating, CLI, tests, docs.
- No placeholders.
- Types consistent (`DiaryRow`, `build_diary_rows`, `format_diary_date`,
  `DiaryOverlay`).
