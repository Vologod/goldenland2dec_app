# P7b3 — Character sheet text layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reproduce the original character-sheet text rectangles/centering exactly (labels left/top at code coords; values centered in their original boxes) so the sheet stops "поехали".

**Architecture:** Add one `draw_text_rect` primitive that mirrors `FUN_12030a88/b4c/a04` (center inside a box, `-1` = left/top). Replace `draw_row`/`draw_rows`/`draw_centered` and drive every section from exact rect tables taken from `FUN_120661cc` and its `.data` arrays (see spec `docs/specs/2026-11-24-p7b3-character-sheet-text-layout-design.md`). No game-logic changes.

**Tech Stack:** C++20, SDL2 software renderer in tests, doctest, CMake/Ninja.

## Global Constraints

- Toolchain PATH + `$env:GL_GAME_DIR="I:\Games\Златогорье-2"` (see AGENTS.md); close `glgame.exe` before building.
- Docs UTF-8 no BOM/NUL. Do not commit game assets. Commit only on explicit user request.
- Only `src/app/{include/gl/app,src}/character_overlay.*` and `tests/app/test_character_overlay.cpp` change.

---

### Task 1: `draw_text_rect` primitive + tabs/name/headers

**Files:**
- Modify: `src/app/include/gl/app/character_overlay.hpp:56-66`
- Modify: `src/app/src/character_overlay.cpp:284-343` (helpers), `:448-476` (tabs/headers)
- Test: `tests/app/test_character_overlay.cpp:286-355`

**Interfaces:**
- Produces: `void CharacterOverlay::draw_text_rect(SDL_Renderer*, const gl::render::Font*, std::string_view, float x, float y, float w, float h, gl::render::Color)` — if `w>=0` and `measure(text).x < w`: `x += (w-measure)/2`; if `h>=0` and `text_height() < h`: `y += (h-text_height())/2`; then `font->draw_text(x,y)`.
- Removes: `draw_centered`, `draw_rows`, `draw_row`.

- [ ] **Step 1: Write the failing test (sheet header/tab centres)**

In `tests/app/test_character_overlay.cpp`, replace the `TEST_CASE("character overlay aligns row text to the original coordinates" ...)` body with helpers that measure each value/label bbox and assert centres. For this task assert only tabs/name/headers (full body is completed in Task 3); use `make_sheet()` strings. Example assertions after rendering characteristics with `inventory_secondary` background:

```cpp
// tabs: "ХАРАКТЕРИСТИКИ" centre 464, "НАВЫКИ" centre 833, top ~16
auto tab1 = diff_bbox(surface, secondary.value(), 265, 0, 615, 35);
auto tab2 = diff_bbox(surface, secondary.value(), 615, 0, 1024, 35);
REQUIRE(tab1.has_value()); REQUIRE(tab2.has_value());
CHECK(std::abs((tab1->x0 + tab1->x1) - 2 * 464) <= 8);
CHECK(std::abs((tab2->x0 + tab2->x1) - 2 * 833) <= 8);
// state header centre 466
auto state_hdr = diff_bbox(surface, secondary.value(), 344, 50, 588, 82);
REQUIRE(state_hdr.has_value());
CHECK(std::abs((state_hdr->x0 + state_hdr->x1) - 2 * 466) <= 8);
```

(The mojibake in `make_sheet()` is ASCII-safe for these boxes because `characteristics_title`/`skills_title` are non-empty UTF-8; the width used is the real measured width.)

- [ ] **Step 2: Run test — expect FAIL**

Run:
```powershell
cmake --build build; build\bin\character_overlay_tests.exe --test-case="*original coordinates*"
```
Expected: FAIL (compile error: helper not yet used / centre assertions off), or assertion failure on centres.

- [ ] **Step 3: Implement the primitive and route tabs/name/headers through it**

Header (`character_overlay.hpp`): delete the three old declarations; add:
```cpp
void draw_text_rect(SDL_Renderer *renderer, const gl::render::Font *font,
                    std::string_view text, float x, float y, float w, float h,
                    gl::render::Color color);
```

`character_overlay.cpp`: delete `draw_centered`/`draw_rows`/`draw_row`; add:
```cpp
void CharacterOverlay::draw_text_rect(SDL_Renderer *renderer,
                                      const gl::render::Font *font,
                                      std::string_view text, float x, float y,
                                      float w, float h, gl::render::Color color) {
    if (font == nullptr || text.empty()) return;
    if (w >= 0.0f) {
        const float text_w = font->measure(text).x;
        if (text_w < w) x += (w - text_w) * 0.5f;
    }
    if (h >= 0.0f) {
        const float cell_h = font->text_height();
        if (cell_h < h) y += (h - cell_h) * 0.5f;
    }
    font->draw_text(renderer, text, x, y, color);
}
```

In `draw_tabs`, replace the three `draw_centered` calls:
```cpp
draw_text_rect(renderer, title, sheet_.name, 15.0f, 10.0f, 250.0f, 26.0f, kNameColor);
draw_text_rect(renderer, title, sheet_.characteristics_title, 292.0f, 16.0f, 344.0f, -1.0f,
               characteristics ? kActiveTab : kInactiveTab);
draw_text_rect(renderer, title, sheet_.skills_title, 661.0f, 16.0f, 344.0f, -1.0f,
               characteristics ? kInactiveTab : kActiveTab);
```
Remove the underline `SDL_SetRenderDrawColor/FillRectF` block (original has none).

In `draw_content`, replace section header `draw_centered` calls:
```cpp
draw_text_rect(renderer, title, sheet_.state_title,       352.0f,  57.0f, 228.0f, -1.0f, kTextColor);
draw_text_rect(renderer, title, sheet_.combat_title,      352.0f, 307.0f, 228.0f, -1.0f, kTextColor);
draw_text_rect(renderer, title, sheet_.resistance_title,  709.0f,  57.0f, 228.0f, -1.0f, kTextColor);
draw_text_rect(renderer, title, sheet_.immunity_title,    713.0f, 527.0f, 228.0f, -1.0f, kTextColor);
```
and group header:
```cpp
draw_text_rect(renderer, title, entry.title, 535.0f, kSkillGroupY[group], 230.0f, -1.0f, kTextColor);
```

- [ ] **Step 4: Run test — expect PASS**

Run: `cmake --build build; build\bin\character_overlay_tests.exe --test-case="*original coordinates*"`
Expected: PASS for tab/header centres.

- [ ] **Step 5: Commit (only if user asked)**

```powershell
git add src/app/include/gl/app/character_overlay.hpp src/app/src/character_overlay.cpp tests/app/test_character_overlay.cpp
git commit -m "feat: p7b3 character-sheet centered-rect text primitive"
```

---

### Task 2: Left panel — attributes + experience values centered

**Files:**
- Modify: `src/app/src/character_overlay.cpp` (constants block near top; `draw_content` left-panel section)
- Test: `tests/app/test_character_overlay.cpp`

**Interfaces:**
- Consumes: `draw_text_rect(...)` from Task 1.
- Produces: exact attribute value boxes `x=197,w=30,h=18`, `y={60,87,115,143,171,198,225}`; experience `x=216,w=38,h=18,y=261`.

- [ ] **Step 1: Write failing test**

In the layout test add:
```cpp
auto primary_value = diff_bbox(surface, secondary.value(), 196, 55, 228, 78); // row 1
REQUIRE(primary_value.has_value());
CHECK(std::abs((primary_value->x0 + primary_value->x1) - 2 * 212) <= 6); // centre 197+15
auto exp_value = diff_bbox(surface, secondary.value(), 214, 255, 256, 282);
REQUIRE(exp_value.has_value());
// experience box centre 216+19=235
CHECK(std::abs((exp_value->x0 + exp_value->x1) - 2 * 235) <= 8);
```
(`make_sheet()` primary value = "1"; experience value = "0".)

- [ ] **Step 2: Run — expect FAIL** (currently right-aligned at 221/251).

Run: `cmake --build build; build\bin\character_overlay_tests.exe --test-case="*original coordinates*"`

- [ ] **Step 3: Implement**

Add constants near the top of `character_overlay.cpp`:
```cpp
constexpr float kPrimaryValueX = 197.0f;
constexpr float kPrimaryValueW = 30.0f;
constexpr std::array<float, 7> kPrimaryValueY{60.0f, 87.0f, 115.0f, 143.0f, 171.0f, 198.0f, 225.0f};
constexpr float kExperienceValueX = 216.0f;
constexpr float kExperienceValueW = 38.0f;
constexpr float kRowValueH = 18.0f;
```
Replace the left-panel `draw_rows`/`draw_row` calls in `draw_content` with:
```cpp
for (std::size_t i = 0; i < sheet_.primary.size() && i < 7; ++i) {
    draw_text_rect(renderer, font_, sheet_.primary[i].label, kPrimaryLabelX,
                   kPrimaryY[i], -1.0f, -1.0f, kTextColor);
    draw_text_rect(renderer, font_, sheet_.primary[i].value, kPrimaryValueX,
                   kPrimaryValueY[i], kPrimaryValueW, kRowValueH, kTextColor);
}
draw_text_rect(renderer, font_, sheet_.experience_points.label, kExperienceLabelX,
               kExperienceY, -1.0f, -1.0f, kTextColor);
draw_text_rect(renderer, font_, sheet_.experience_points.value, kExperienceValueX,
               kExperienceY, kExperienceValueW, kRowValueH, kTextColor);
```
(Drop now-unused `kExperienceValueRight` if it becomes unused; keep if still referenced.)

- [ ] **Step 4: Run — expect PASS**

Run: `cmake --build build; build\bin\character_overlay_tests.exe --test-case="*original coordinates*"`

- [ ] **Step 5: Commit (only if user asked)** — same files, message `feat: p7b3 left-panel value centering`.

---

### Task 3: Characteristics page values (state/combat/resistance/immunity)

**Files:**
- Modify: `src/app/src/character_overlay.cpp` (constants; `draw_content` characteristics branch)
- Test: `tests/app/test_character_overlay.cpp`

**Interfaces:**
- Produces: `TextRect{float x,y,w,h}` value tables (see spec):
  - state: `{588,103,41,18},{471,133,159,18},{588,161,41,18},{480,212,150,16},{480,238,150,16},{572,265,56,16}`
  - combat: ids19-26 `{582,348,46,15}…{582,537,46,15}`; armor `{582,565,46,15}`; weight `{497,597,130,18}`
  - resistance: `x=950,w=47,h=17,y={103,136,168,220,251,282,312,343,373,430,456,482}`
  - immunity: col1 `x=774`, col2 `x=950`, `w=43,h=16,y={570,596,622}`

- [ ] **Step 1: Write failing test** — extend layout test:
```cpp
auto state_value = diff_bbox(surface, secondary.value(), 580, 96, 640, 130);   // level box center 608
REQUIRE(state_value.has_value());
CHECK(std::abs((state_value->x0 + state_value->x1) - 2 * 608) <= 8);
auto resist_value = diff_bbox(surface, secondary.value(), 940, 96, 1005, 130); // center 973.5
REQUIRE(resist_value.has_value());
CHECK(std::abs((resist_value->x0 + resist_value->x1) - 2 * 974) <= 8);
auto imm1 = diff_bbox(surface, secondary.value(), 760, 562, 830, 595);          // center 795.5
auto imm2 = diff_bbox(surface, secondary.value(), 935, 562, 1005, 595);         // center 971.5
REQUIRE(imm1.has_value()); REQUIRE(imm2.has_value());
CHECK(std::abs((imm1->x0 + imm1->x1) - 2 * 796) <= 8);
CHECK(std::abs((imm2->x0 + imm2->x1) - 2 * 972) <= 8);
```
(`make_sheet()` state values "1"; resistance/immunity values "—". The em-dash width is fine; centre assertion uses ±8.)

- [ ] **Step 2: Run — expect FAIL** (current right-align 627/995/816/991).

- [ ] **Step 3: Implement**

Add a small struct + tables in the anonymous namespace:
```cpp
struct TextRect { float x; float y; float w; float h; };
constexpr std::array<TextRect, 6> kStateValueRect{{
    {588,103,41,18},{471,133,159,18},{588,161,41,18},
    {480,212,150,16},{480,238,150,16},{572,265,56,16}}};
constexpr std::array<TextRect, 10> kCombatValueRect{{
    {582,348,46,15},{582,375,46,15},{582,402,46,15},{582,430,46,15},
    {582,456,46,15},{582,483,46,15},{582,510,46,15},{582,537,46,15},
    {582,565,46,15},{497,597,130,18}}};
constexpr std::array<float, 12> kResistanceValueY{103,136,168,220,251,282,312,343,373,430,456,482};
constexpr std::array<float, 3> kImmunityValueY{570,596,622};
```
Replace the characteristics branch value drawing with paired calls:
```cpp
for (std::size_t i = 0; i < 6 && i < sheet_.state.size(); ++i) {
    draw_text_rect(renderer, font_, sheet_.state[i].label, kCenterLabelX, kStateY[i], -1,-1, kTextColor);
    const auto &r = kStateValueRect[i];
    draw_text_rect(renderer, font_, sheet_.state[i].value, r.x, r.y, r.w, r.h, kTextColor);
}
for (std::size_t i = 0; i < 10 && i < sheet_.combat.size(); ++i) {
    const float label_x = (i == 9) ? kWeightLabelX : kCenterLabelX;
    draw_text_rect(renderer, font_, sheet_.combat[i].label, label_x, kCombatY[i], -1,-1, kTextColor);
    const auto &r = kCombatValueRect[i];
    draw_text_rect(renderer, font_, sheet_.combat[i].value, r.x, r.y, r.w, r.h, kTextColor);
}
for (std::size_t i = 0; i < 12 && i < sheet_.resistance.size(); ++i) {
    draw_text_rect(renderer, font_, sheet_.resistance[i].label, kResistanceLabelX, kResistanceY[i], -1,-1, kTextColor);
    draw_text_rect(renderer, font_, sheet_.resistance[i].value, 950.0f, kResistanceValueY[i], 47.0f, 17.0f, kTextColor);
}
for (std::size_t col = 0; col < 2; ++col) {
    const float vx = (col == 0) ? 774.0f : 950.0f;
    const float lx = (col == 0) ? kImmunityLeftX[0] : kImmunityRightX[0];
    for (std::size_t row = 0; row < 3; ++row) {
        const auto &r = sheet_.immunity[col * 3 + row];
        draw_text_rect(renderer, font_, r.label, lx, kImmunityY[row], -1,-1, kTextColor);
        draw_text_rect(renderer, font_, r.value, vx, kImmunityValueY[row], 43.0f, 16.0f, kTextColor);
    }
}
```
Delete the now-unused `kCenterValueRight`/`kResistanceValueRight`/`kImmunityLeftValueRight`/`kImmunityRightValueRight` constants.

- [ ] **Step 4: Run — expect PASS**

- [ ] **Step 5: Commit (only if user asked)** — message `feat: p7b3 characteristics value centering`.

---

### Task 4: Skills page values + skill points

**Files:**
- Modify: `src/app/src/character_overlay.cpp` (skills branch)
- Test: `tests/app/test_character_overlay.cpp`

**Interfaces:**
- Produces: skill labels at `x={295,544,790}`, `y=kSkillRowY[group*3+row]`; skill values `x={460,706,952}`, `y={123,156,188,310,342,374,494,527,559}[group*3+row]`, `w=31,h=18`; skill points label `x=526,y=626`; value `{739,623,45,23}`.

- [ ] **Step 1: Write failing test** — in skills section:
```cpp
auto skill_label = diff_bbox(surface, skills.value(), 285, 118, 440, 150);
auto skill_value = diff_bbox(surface, skills.value(), 450, 118, 500, 150); // center 475.5
REQUIRE(skill_label.has_value()); REQUIRE(skill_value.has_value());
CHECK(skill_label->y0 >= 124); CHECK(skill_label->y0 <= 126);
CHECK(std::abs((skill_value->x0 + skill_value->x1) - 2 * 476) <= 8);
auto points_value = diff_bbox(surface, skills.value(), 735, 620, 790, 650); // center 761.5
REQUIRE(points_value.has_value());
CHECK(std::abs((points_value->x0 + points_value->x1) - 2 * 762) <= 10);
```

- [ ] **Step 2: Run — expect FAIL** (skills right-aligned 488/733/978; points 779).

- [ ] **Step 3: Implement**

Add constants:
```cpp
constexpr std::array<float, 3> kSkillValueX{460.0f, 706.0f, 952.0f};
constexpr std::array<float, 9> kSkillValueY{123,156,188,310,342,374,494,527,559};
constexpr float kSkillValueW = 31.0f, kSkillValueH = 18.0f;
```
Replace skill label/value drawing in the skills branch:
```cpp
for (std::size_t group = 0; group < sheet_.skill_groups.size() && group < 3; ++group) {
    const auto &entry = sheet_.skill_groups[group];
    draw_text_rect(renderer, title, entry.title, 535.0f, kSkillGroupY[group], 230.0f, -1.0f, kTextColor);
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t row = 0; row < 3; ++row) {
            const std::size_t index = column * 3 + row;
            if (index >= entry.rows.size()) continue;
            const auto &r = entry.rows[index];
            draw_text_rect(renderer, font_, r.label, kSkillColumnX[column],
                           kSkillRowY[group * 3 + row], -1.0f, -1.0f, kTextColor);
            draw_text_rect(renderer, font_, r.value, kSkillValueX[column],
                           kSkillValueY[group * 3 + row], kSkillValueW, kSkillValueH, kTextColor);
        }
    }
}
draw_text_rect(renderer, font_, sheet_.skill_points_label, 526.0f, 626.0f, -1.0f, -1.0f, kTextColor);
draw_text_rect(renderer, font_, std::to_string(sheet_.skill_points), 739.0f, 623.0f, 45.0f, 23.0f, kTextColor);
```
Delete unused `kSkillValueRight`/`kSkillPointsValueRight`/`kSkillPointsLabelX`/`kSkillPointsY`.

- [ ] **Step 4: Run — expect PASS**

- [ ] **Step 5: Commit (only if user asked)** — message `feat: p7b3 skills value centering`.

---

### Task 5: Full verification + docs

**Files:**
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`, `docs/research/` (new `p7b3-character-sheet-text-layout.md`)

- [ ] **Step 1: Build + focused**

```powershell
cmake --build build
build\bin\character_overlay_tests.exe
build\bin\gl_game_tests.exe --test-case="*character sheet*"
build\bin\glgame_screenshot_tests.exe --test-case="*character*"
```
Expected: all green.

- [ ] **Step 2: Screenshot for manual gate**

```powershell
build\bin\glgame.exe --game-dir "I:\Games\Златогорье-2" --character --load-save 0
```
Capture characteristics + `--character-tab skills`; attach for the user (manual gate stays OPEN; agent does not confirm manual pass).

- [ ] **Step 3: Full ctest with GL_GAME_DIR**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
ctest --test-dir build --output-on-failure
```
Expected: `21/21 passed` (record duration).

- [ ] **Step 4: Update docs**

- `docs/research/p7b3-character-sheet-text-layout.md`: record `FUN_120661cc`/`FUN_12030a88/b4c/a04/30980` RVAs, helper semantics, the `.data` arrays, and the value rect table, with Client.dll SHA-256.
- `docs/STATUS.md`: new `P7b3` section (done/verified/next; manual gate open).
- `docs/PROJECT.md`: P7 row note.

- [ ] **Step 5: Commit (only if user asked)** — `docs: record p7b3 character-sheet text layout`.

## Self-Review

- Spec coverage: helper semantics (Task 1), left panel (Task 2), characteristics values (Task 3), skills values (Task 4), non-text parity (underline removal in Task 1), verification (Task 5). No gaps.
- Placeholders: none; all code shown.
- Type consistency: `draw_text_rect(renderer, font, text, x, y, w, h, color)` and `TextRect{x,y,w,h}` used consistently across tasks.
