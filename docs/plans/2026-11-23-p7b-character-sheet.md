# P7b Character Sheet Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the read-only character sheet from `inventory_skills.bmp` (left attributes, five sub-tabs, 27 skills, dynamic equipped hero doll), opened from the inventory skills tab and `C`.

**Architecture:** SDL-free `gl::game::CharacterSheet` builds localized rows from `PersonRuntime` + `user_interface.sdb`; `gl::app::CharacterOverlay` renders the original frame/tabs; `gl::app::HeroPortrait` composes the equipped idle frame; `glgame` wires entries and input gating. Spec: `docs/specs/2026-11-23-p7b-character-sheet-design.md`.

**Tech Stack:** C++20, CMake + Ninja, SDL2, doctest, existing `gl_assets`/`gl_render`/`gl_game`/`gl_app`.

## Global Constraints

- C++20; no comments in code; `PascalCase` types, `snake_case` functions/variables/files.
- Layers only downward: `assets <- core/script <- game/render/ui/audio <- app <- platform`; SDL only in `app`/`render`/`tools`.
- Data references: `user_interface.sdb` ids per spec; skill order permutation `0x1212e0a0`; `inventory_skills.bmp` 1024x768; positions per spec.
- Text is UTF-8 at API boundaries; game data cp1251 is decoded by `Sdb`.
- Build/test with the toolchain:
  ```powershell
  $tools="I:\goldenland2dec\.local\toolchains"
  $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
  $env:GL_GAME_DIR="I:\Games\Златогорье-2"
  ```
- Commits: the user explicitly authorized one commit per task for P7b (2026-11-23), no push. Commit the spec/plan and each task's code/tests/docs with concise messages; stop and ask before any push or history rewrite.
- Docs UTF-8 without BOM; verify `bom=False nul=False` and no literal `\n` artifacts.

---

### Task 1: `CharacterSheet` model

**Files:**
- Create: `src/game/include/gl/game/character_sheet.hpp`
- Create: `src/game/src/character_sheet.cpp`
- Test: `tests/game/test_character_sheet.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Consumes: `gl::game::PersonRuntime` (`get_parameter`, `get_skill`), `gl::script::PersonParameter`, `gl::game::hero_level_from_experience`, `gl::assets::Sdb`.
- Produces:
  - `enum class gl::game::CharacterTab { personality, characteristics, skills, state, resistance };`
  - `struct CharacterRow { std::string label; std::string value; bool modeled = true; };`
  - `struct CharacterSheet { std::string name; std::vector<CharacterRow> primary, personality, characteristics, skills; std::int32_t skill_points = 0; };`
  - `CharacterSheet build_character_sheet(const PersonRuntime&, const std::function<std::string(std::uint32_t)>& ui_text, std::string_view person = "Hero");`
  - `CharacterSheet build_character_sheet(const PersonRuntime&, const gl::assets::Sdb&, std::string_view person = "Hero");`

- [ ] **Step 1: Write the failing unit test**

Create `tests/game/test_character_sheet.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/game/character_sheet.hpp"
#include "gl/game/person_runtime.hpp"
#include "gl/script/person_access.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace {
gl::game::PersonRuntime make_runtime() {
    gl::assets::HeroStats stats;
    stats.strength = 30;
    stats.constitution = 25;
    stats.dexterity = 18;
    stats.perception = 12;
    stats.intelligence = 5;
    stats.wisdom = 7;
    stats.luck = 9;
    stats.reputation = -40;
    stats.experience = 81099;
    stats.life = 200;
    stats.energy = 11;
    stats.skills[21] = 1;
    stats.skills[12] = 15;
    return gl::game::PersonRuntime::create({}, stats);
}

std::string ui_text(std::uint32_t id) { return "L" + std::to_string(id); }
} // namespace

TEST_CASE("character sheet maps primary attributes in UI order") {
    const auto runtime = make_runtime();
    const gl::game::CharacterSheet sheet =
        gl::game::build_character_sheet(runtime, ui_text);
    REQUIRE(sheet.primary.size() == 8);
    CHECK(sheet.primary[0].label == "L6");
    CHECK(sheet.primary[0].value == "30");
    CHECK(sheet.primary[1].label == "L7");
    CHECK(sheet.primary[1].value == "25");
    CHECK(sheet.primary[3].label == "L9");
    CHECK(sheet.primary[3].value == "12");
    CHECK(sheet.primary[4].label == "L10");
    CHECK(sheet.primary[4].value == "7");
    CHECK(sheet.primary[5].label == "L11");
    CHECK(sheet.primary[5].value == "5");
    CHECK(sheet.primary[7].label == "L16");
    CHECK(sheet.primary[7].value == "-40");
}

TEST_CASE("character sheet maps the 27 skills through the original permutation") {
    const auto runtime = make_runtime();
    const gl::game::CharacterSheet sheet =
        gl::game::build_character_sheet(runtime, ui_text);
    REQUIRE(sheet.skills.size() == 27);
    CHECK(sheet.skills[0].label == "L49");
    CHECK(sheet.skills[0].value == "15");  // SKILL_WPN_SWORD (id 12)
    CHECK(sheet.skills[20].label == "L69");
    CHECK(sheet.skills[20].value == "0");   // SKILL_HEALING (id 20)
    CHECK(sheet.skills[21].label == "L70");
    CHECK(sheet.skills[21].value == "1");   // SKILL_TALKING (id 21)
}

TEST_CASE("character sheet marks unmodeled characteristics and computes the level") {
    const auto runtime = make_runtime();
    const auto sheet = gl::game::build_character_sheet(runtime, ui_text);
    REQUIRE(sheet.characteristics.size() == 15);
    CHECK(sheet.characteristics[0].label == "L14");
    CHECK(sheet.characteristics[0].modeled);
    CHECK(sheet.characteristics[0].value ==
          std::to_string(gl::game::hero_level_from_experience(81099.0)));
    CHECK(sheet.characteristics[1].label == "L15");
    CHECK(sheet.characteristics[1].value == "81099");
    CHECK(sheet.characteristics[2].label == "L16");
    CHECK(sheet.characteristics[2].value == "-40");
    CHECK(sheet.characteristics[3].label == "L27");
    CHECK(sheet.characteristics[4].label == "L28");
    CHECK_FALSE(sheet.characteristics[5].modeled);
    CHECK(sheet.characteristics[5].value == "-");
    CHECK(sheet.characteristics[14].label == "L17");
    CHECK(sheet.skill_points == 0);
}

TEST_CASE("character sheet personality rows and unknown person") {
    const auto runtime = make_runtime();
    const auto hero = gl::game::build_character_sheet(runtime, ui_text);
    REQUIRE(hero.personality.size() == 4);
    CHECK(hero.name == "Hero");
    CHECK(hero.personality[0].label == "L5");
    CHECK(hero.personality[0].value == "Hero");
    CHECK(hero.personality[1].label == "L14");

    const auto missing = gl::game::build_character_sheet(runtime, ui_text, "missing");
    CHECK(missing.name == "missing");
    CHECK(missing.primary[0].value == "0");
    CHECK(missing.skills[0].value == "0");
}

TEST_CASE("character sheet falls back to #id for missing UI text") {
    const auto runtime = make_runtime();
    const auto sheet = gl::game::build_character_sheet(
        runtime, [](std::uint32_t) { return std::string{}; });
    CHECK(sheet.primary[0].label == "#6");
    CHECK(sheet.skills[0].label == "#49");
}
```

Add to `tests/game/CMakeLists.txt` in the `gl_game_tests` source list:
```
    test_character_sheet.cpp
```

- [ ] **Step 2: Build and confirm red**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*character sheet*"
```
Expected: compile error for missing `gl/game/character_sheet.hpp`.

- [ ] **Step 3: Add the header**

Create `src/game/include/gl/game/character_sheet.hpp`:

```cpp
#pragma once
#include "gl/assets/sdb.hpp"
#include "gl/game/person_runtime.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace gl::game {
enum class CharacterTab { personality, characteristics, skills, state, resistance };

struct CharacterRow {
    std::string label;
    std::string value;
    bool modeled = true;
};

struct CharacterSheet {
    std::string name;
    std::vector<CharacterRow> primary;
    std::vector<CharacterRow> personality;
    std::vector<CharacterRow> characteristics;
    std::vector<CharacterRow> skills;
    std::int32_t skill_points = 0;
};

CharacterSheet build_character_sheet(
    const PersonRuntime &persons,
    const std::function<std::string(std::uint32_t)> &ui_text,
    std::string_view person = "Hero");
CharacterSheet build_character_sheet(const PersonRuntime &persons,
                                     const gl::assets::Sdb &ui_texts,
                                     std::string_view person = "Hero");
} // namespace gl::game
```

- [ ] **Step 4: Implement the model**

Create `src/game/src/character_sheet.cpp`:

```cpp
#include "gl/game/character_sheet.hpp"
#include "gl/script/person_access.hpp"
#include <array>
#include <string>

namespace gl::game {
namespace {
using gl::script::PersonParameter;

constexpr std::array<std::uint32_t, 8> kPrimaryIds{6, 7, 8, 9, 10, 11, 12, 16};
constexpr std::array<PersonParameter, 8> kPrimaryParams{
    PersonParameter::strength,     PersonParameter::constitution,
    PersonParameter::dexterity,    PersonParameter::perception,
    PersonParameter::wisdom,       PersonParameter::intelligence,
    PersonParameter::luck,         PersonParameter::reputation};
constexpr std::array<std::uint32_t, 15> kCharacteristicIds{
    14, 15, 16, 27, 28, 18, 19, 20, 21, 22, 23, 24, 25, 26, 17};
constexpr std::array<std::uint32_t, 27> kSkillUiIds{
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
    63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75};
constexpr std::array<std::string_view, 27> kSkillNames{
    "SKILL_WPN_SWORD", "SKILL_WPN_AXE",   "SKILL_WPN_CRUSH", "SKILL_WPN_STAFF",
    "SKILL_WPN_DIST",  "SKILL_WPN_SPEAR", "SKILL_WPN_THROW", "SKILL_WPN_HAND",
    "SKILL_CRITICAL_HIT", "SKILL_SHADMAG", "SKILL_NATRMAG",  "SKILL_GODSMAG",
    "SKILL_ELEMMAG",   "SKILL_LGHTMAG",   "SKILL_DARKMAG",  "SKILL_MAGICUSE",
    "SKILL_ALCHEMY",   "SKILL_IDENTIFY",  "SKILL_TACTIC",   "SKILL_SCOUT",
    "SKILL_HEALING",   "SKILL_TALKING",   "SKILL_TRADE",    "SKILL_STEAL",
    "SKILL_HACK",      "SKILL_SMITH",     "SKILL_ATHLETIC"};

std::string label_of(const std::function<std::string(std::uint32_t)> &ui_text,
                     std::uint32_t id) {
    std::string text = ui_text(id);
    return text.empty() ? "#" + std::to_string(id) : text;
}

std::string integer_value(double value) {
    return std::to_string(static_cast<std::int32_t>(value));
}

CharacterRow row(const std::function<std::string(std::uint32_t)> &ui_text,
                 std::uint32_t id, std::string value, bool modeled = true) {
    return CharacterRow{label_of(ui_text, id), std::move(value), modeled};
}
} // namespace

CharacterSheet build_character_sheet(
    const PersonRuntime &persons,
    const std::function<std::string(std::uint32_t)> &ui_text,
    std::string_view person) {
    CharacterSheet sheet;
    sheet.name = std::string(person);
    sheet.skill_points = 0;
    for (std::size_t index = 0; index < kPrimaryIds.size(); ++index) {
        sheet.primary.push_back(row(ui_text, kPrimaryIds[index],
                                    integer_value(persons.get_parameter(person,
                                                                         kPrimaryParams[index]))));
    }

    const double experience =
        persons.get_parameter(person, PersonParameter::experience);
    const double reputation =
        persons.get_parameter(person, PersonParameter::reputation);
    sheet.personality.push_back(row(ui_text, 5, sheet.name));
    sheet.personality.push_back(row(
        ui_text, 14,
        integer_value(static_cast<double>(hero_level_from_experience(experience)))));
    sheet.personality.push_back(row(ui_text, 15, integer_value(experience)));
    sheet.personality.push_back(row(ui_text, 16, integer_value(reputation)));

    const std::array<std::string, 15> characteristic_values{
        integer_value(static_cast<double>(hero_level_from_experience(experience))),
        integer_value(experience),
        integer_value(reputation),
        integer_value(persons.get_parameter(person, PersonParameter::life)),
        integer_value(persons.get_parameter(person, PersonParameter::energy)),
        "-", "-", "-", "-", "-", "-", "-", "-", "-", "-"};
    const std::array<bool, 15> characteristic_modeled{
        true, true, true, true, true, false, false, false, false, false,
        false, false, false, false, false};
    for (std::size_t index = 0; index < kCharacteristicIds.size(); ++index) {
        sheet.characteristics.push_back(row(ui_text, kCharacteristicIds[index],
                                            characteristic_values[index],
                                            characteristic_modeled[index]));
    }

    for (std::size_t index = 0; index < kSkillUiIds.size(); ++index) {
        sheet.skills.push_back(row(
            ui_text, kSkillUiIds[index],
            integer_value(persons.get_skill(person, kSkillNames[index]))));
    }
    return sheet;
}

CharacterSheet build_character_sheet(const PersonRuntime &persons,
                                     const gl::assets::Sdb &ui_texts,
                                     std::string_view person) {
    return build_character_sheet(
        persons,
        [&ui_texts](std::uint32_t id) {
            const std::string *text = ui_texts.find(id);
            return text == nullptr ? std::string{} : *text;
        },
        person);
}
} // namespace gl::game
```

- [ ] **Step 5: Build and run**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*character sheet*"
```
Expected: 5 cases pass.

---

### Task 2: `CharacterOverlay` static frame, tabs and rows

**Files:**
- Create: `src/app/include/gl/app/character_overlay.hpp`
- Create: `src/app/src/character_overlay.cpp`
- Test: `tests/app/test_character_overlay.cpp`
- Modify: `tests/app/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 `CharacterSheet`/`CharacterTab`; `gl::assets::GameVfs`; `gl::render::Font`; `gl::render::Texture`; SDL.
- Produces:
  - `enum class CharacterRegion { none, tab, close };`
  - `class CharacterOverlay { bool open(const GameVfs&, CharacterSheet); void close(); bool visible() const; CharacterTab tab() const; void set_tab(CharacterTab); void set_font(const Font*); void render(SDL_Renderer*, int, int, const Font* = nullptr); bool handle_mouse(int,int,bool); bool handle_key(SDL_Keycode); CharacterRegion hit_test(int,int) const; };`

- [ ] **Step 1: Write the failing overlay tests**

Create `tests/app/test_character_overlay.cpp` using the `SoftwareRenderer` pattern from `tests/app/test_inventory_overlay.cpp`; include:
```cpp
#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/character_overlay.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/render/font.hpp>
#include <gl/render/texture.hpp>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

int main(int argc, char **argv) {
    doctest::Context context(argc, argv);
    return context.run();
}

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

struct SoftwareRenderer {
    SoftwareRenderer(int width, int height) {
        surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
        if (surface != nullptr) renderer = SDL_CreateSoftwareRenderer(surface);
    }
    ~SoftwareRenderer() {
        if (renderer != nullptr) SDL_DestroyRenderer(renderer);
        if (surface != nullptr) SDL_FreeSurface(surface);
    }
    SoftwareRenderer(const SoftwareRenderer &) = delete;
    SoftwareRenderer &operator=(const SoftwareRenderer &) = delete;
    SDL_Surface *surface = nullptr;
    SDL_Renderer *renderer = nullptr;
};

std::vector<std::uint8_t> snapshot(SDL_Renderer *renderer, SDL_Surface *surface) {
    SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                         surface->pixels, surface->pitch);
    const auto *pixels = static_cast<const std::uint8_t *>(surface->pixels);
    return {pixels, pixels + static_cast<std::size_t>(surface->pitch) * surface->h};
}

std::size_t region_difference(const std::vector<std::uint8_t> &a,
                              const std::vector<std::uint8_t> &b, int x, int y,
                              int w, int h) {
    std::size_t different = 0;
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) {
            const std::size_t offset =
                (static_cast<std::size_t>(py) * 1024 + static_cast<std::size_t>(px)) * 4;
            if (a[offset] != b[offset] || a[offset + 1] != b[offset + 1] ||
                a[offset + 2] != b[offset + 2])
                ++different;
        }
    }
    return different;
}

gl::game::CharacterSheet make_sheet() {
    gl::game::CharacterSheet sheet;
    sheet.name = "Hero";
    auto add = [](std::vector<gl::game::CharacterRow> &rows, const char *label,
                  const char *value, bool modeled = true) {
        rows.push_back(gl::game::CharacterRow{label, value, modeled});
    };
    for (int i = 0; i < 8; ++i) add(sheet.primary, "PRIMARY", std::to_string(i + 1).c_str());
    for (int i = 0; i < 4; ++i) add(sheet.personality, "PERSON", std::to_string(i).c_str());
    for (int i = 0; i < 15; ++i)
        add(sheet.characteristics, "CHAR", i < 5 ? "1" : "-", i < 5);
    for (int i = 0; i < 27; ++i) add(sheet.skills, "SKILL", std::to_string(i).c_str());
    return sheet;
}
} // namespace

TEST_CASE("character overlay opens, hits tabs and close" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    gl::app::CharacterOverlay overlay;
    CHECK(overlay.open(vfs.value(), make_sheet()));
    CHECK(overlay.visible());
    CHECK(overlay.tab() == gl::game::CharacterTab::skills);
    CHECK(overlay.hit_test(5, 10) == gl::app::CharacterRegion::tab);
    CHECK(overlay.hit_test(930, 700) == gl::app::CharacterRegion::close);
    CHECK(overlay.handle_mouse(5, 10, true));
    CHECK(overlay.tab() == gl::game::CharacterTab::personality);
    CHECK(overlay.handle_key(SDLK_3));
    CHECK(overlay.tab() == gl::game::CharacterTab::skills);
    CHECK(overlay.handle_key(SDLK_ESCAPE));
    CHECK_FALSE(overlay.visible());
}

TEST_CASE("character overlay renders different tab content" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    SoftwareRenderer software(1024, 768);
    REQUIRE(software.renderer != nullptr);
    auto font_data = vfs->read("engineres/fonts/ttf/arial.ttf");
    REQUIRE(font_data.has_value());
    auto font = gl::render::Font::load_from_memory(
        software.renderer, *font_data, 18.0f, "arial");
    REQUIRE(font.has_value());

    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(vfs.value(), make_sheet()));
    overlay.set_tab(gl::game::CharacterTab::skills);
    overlay.render(software.renderer, 1024, 768, &font.value());
    const auto skills = snapshot(software.renderer, software.surface);
    overlay.set_tab(gl::game::CharacterTab::characteristics);
    overlay.render(software.renderer, 1024, 768, &font.value());
    const auto characteristics = snapshot(software.renderer, software.surface);
    CHECK(region_difference(skills, characteristics, 289, 126, 140, 80) > 0);
}
```

Add to `tests/app/CMakeLists.txt`:
```
add_executable(character_overlay_tests test_character_overlay.cpp)
target_include_directories(character_overlay_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
target_link_libraries(character_overlay_tests PRIVATE gl_app)
gl_enable_warnings(character_overlay_tests)
add_test(NAME character_overlay_tests COMMAND character_overlay_tests)
set_tests_properties(character_overlay_tests PROPERTIES SKIP_RETURN_CODE 77)
```

- [ ] **Step 2: Build and confirm red**

```powershell
cmake -S . -B build
cmake --build build
build\bin\character_overlay_tests.exe
```
Expected: compile error for missing `character_overlay.hpp`.

- [ ] **Step 3: Add the overlay header**

Create `src/app/include/gl/app/character_overlay.hpp`:

```cpp
#pragma once
#include <gl/game/character_sheet.hpp>
#include <gl/render/font.hpp>
#include <gl/render/texture.hpp>
#include <SDL.h>
#include <map>
#include <string>
#include <vector>

namespace gl::assets { class GameVfs; }

namespace gl::app {
enum class CharacterRegion { none, tab, close };

class CharacterOverlay {
  public:
    bool open(const gl::assets::GameVfs &vfs, gl::game::CharacterSheet sheet);
    void close();
    bool visible() const { return visible_; }
    gl::game::CharacterTab tab() const { return tab_; }
    void set_tab(gl::game::CharacterTab tab) { tab_ = tab; }
    void set_font(const gl::render::Font *font) { font_ = font; }
    void render(SDL_Renderer *renderer, int screen_w, int screen_h,
                const gl::render::Font *font = nullptr);
    bool handle_mouse(int x, int y, bool click);
    bool handle_key(SDL_Keycode key);
    CharacterRegion hit_test(int x, int y) const;

  private:
    void update_transform(int screen_w, int screen_h);
    void reset_texture_cache(SDL_Renderer *renderer);
    gl::render::Texture *load_texture(std::string_view path);
    void draw_background(SDL_Renderer *renderer);
    void draw_tabs(SDL_Renderer *renderer);
    void draw_close(SDL_Renderer *renderer);
    void draw_rows(SDL_Renderer *renderer, const std::vector<gl::game::CharacterRow> &rows,
                   const int *y_positions, int count, float x, float w, float height,
                   bool right_align);
    void draw_content(SDL_Renderer *renderer);

    const gl::assets::GameVfs *vfs_ = nullptr;
    const gl::render::Font *font_ = nullptr;
    gl::game::CharacterSheet sheet_;
    gl::game::CharacterTab tab_ = gl::game::CharacterTab::skills;
    bool visible_ = false;
    bool hover_close_ = false;
    float scale_ = 1.0f;
    float offset_x_ = 0.0f;
    float offset_y_ = 0.0f;
    SDL_Renderer *texture_renderer_ = nullptr;
    std::map<std::string, gl::render::Texture> textures_;
};
} // namespace gl::app
```

- [ ] **Step 4: Implement the overlay**

Create `src/app/src/character_overlay.cpp` with the canvas constants and draw loops:

```cpp
#include "gl/app/character_overlay.hpp"
#include "gl/assets/image.hpp"
#include "gl/assets/vfs.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace gl::app {
namespace {
constexpr float kCanvasWidth = 1024.0f;
constexpr float kCanvasHeight = 768.0f;
constexpr std::string_view kBackground = "engineres/newinv/inventory_skills.bmp";
constexpr std::array<float, 5> kTabX{3.0f, 62.0f, 121.0f, 179.0f, 239.0f};
constexpr float kTabY = 4.0f;
constexpr float kTabW = 56.0f;
constexpr float kTabH = 25.0f;
constexpr std::array<float, 7> kPrimaryY{60.0f, 88.0f, 117.0f, 144.0f,
                                         172.0f, 199.0f, 226.0f};
constexpr std::array<float, 15> kCharacteristicsY{
    109.0f, 140.0f, 168.0f, 218.0f, 244.0f, 271.0f, 354.0f, 381.0f,
    408.0f, 435.0f, 462.0f, 489.0f, 516.0f, 543.0f, 570.0f};
constexpr std::array<float, 27> kSkillX{
    295.0f, 295.0f, 295.0f, 544.0f, 544.0f, 544.0f, 790.0f, 790.0f, 790.0f,
    295.0f, 295.0f, 295.0f, 544.0f, 544.0f, 544.0f, 790.0f, 790.0f, 790.0f,
    295.0f, 295.0f, 295.0f, 544.0f, 544.0f, 544.0f, 790.0f, 790.0f, 790.0f};
constexpr std::array<float, 27> kSkillY{
    130.0f, 162.0f, 194.0f, 130.0f, 162.0f, 194.0f, 130.0f, 162.0f, 194.0f,
    315.0f, 348.0f, 381.0f, 315.0f, 348.0f, 381.0f, 315.0f, 348.0f, 381.0f,
    500.0f, 532.0f, 564.0f, 500.0f, 532.0f, 564.0f, 500.0f, 532.0f, 564.0f};
constexpr gl::render::Color kTextColor{20, 18, 16, 255};

bool contains(float rx, float ry, float rw, float rh, float x, float y) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}
} // namespace

bool CharacterOverlay::open(const gl::assets::GameVfs &vfs,
                            gl::game::CharacterSheet sheet) {
    vfs_ = &vfs;
    sheet_ = std::move(sheet);
    tab_ = gl::game::CharacterTab::skills;
    visible_ = true;
    scale_ = 1.0f;
    offset_x_ = 0.0f;
    offset_y_ = 0.0f;
    update_transform(1024, 768);
    reset_texture_cache(nullptr);
    return true;
}

void CharacterOverlay::close() { visible_ = false; }

void CharacterOverlay::update_transform(int screen_w, int screen_h) {
    scale_ = std::min(static_cast<float>(screen_w) / kCanvasWidth,
                      static_cast<float>(screen_h) / kCanvasHeight);
    if (scale_ <= 0.0f) scale_ = 1.0f;
    offset_x_ = (static_cast<float>(screen_w) - kCanvasWidth * scale_) * 0.5f;
    offset_y_ = (static_cast<float>(screen_h) - kCanvasHeight * scale_) * 0.5f;
}

void CharacterOverlay::reset_texture_cache(SDL_Renderer *renderer) {
    textures_.clear();
    texture_renderer_ = renderer;
}

gl::render::Texture *CharacterOverlay::load_texture(std::string_view path) {
    if (vfs_ == nullptr || texture_renderer_ == nullptr) return nullptr;
    const auto cached = textures_.find(std::string(path));
    if (cached != textures_.end()) return &cached->second;
    auto data = vfs_->read(path);
    if (!data) return nullptr;
    auto decoded = gl::assets::decode_image(data.value());
    if (!decoded) return nullptr;
    auto created = gl::render::Texture::from_image(texture_renderer_, decoded.value());
    if (!created) return nullptr;
    return &textures_.emplace(std::string(path), std::move(created.value())).first->second;
}

void CharacterOverlay::render(SDL_Renderer *renderer, int screen_w, int screen_h,
                              const gl::render::Font *font) {
    if (renderer == nullptr || !visible_ || screen_w <= 0 || screen_h <= 0) return;
    if (texture_renderer_ != renderer) reset_texture_cache(renderer);
    if (font != nullptr) font_ = font;
    update_transform(screen_w, screen_h);

    float old_scale_x = 1.0f;
    float old_scale_y = 1.0f;
    SDL_RenderGetScale(renderer, &old_scale_x, &old_scale_y);
    SDL_Rect old_viewport{};
    SDL_RenderGetViewport(renderer, &old_viewport);
    SDL_BlendMode old_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);
    const SDL_Rect viewport{
        static_cast<int>(std::lround(offset_x_)),
        static_cast<int>(std::lround(offset_y_)),
        std::max(1, static_cast<int>(std::lround(kCanvasWidth * scale_))),
        std::max(1, static_cast<int>(std::lround(kCanvasHeight * scale_)))};
    SDL_RenderSetViewport(renderer, &viewport);
    SDL_RenderSetScale(renderer, scale_, scale_);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    draw_background(renderer);
    draw_tabs(renderer);
    draw_content(renderer);
    draw_close(renderer);

    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    SDL_RenderSetScale(renderer, old_scale_x, old_scale_y);
    SDL_RenderSetViewport(renderer, &old_viewport);
}
```

Implement the remaining methods with the concrete row data/positions:

```cpp
void CharacterOverlay::draw_background(SDL_Renderer *renderer) {
    if (gl::render::Texture *background = load_texture(kBackground); background != nullptr) {
        const SDL_FRect rect{0.0f, 0.0f, kCanvasWidth, kCanvasHeight};
        SDL_RenderCopyF(renderer, background->get(), nullptr, &rect);
        return;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    const SDL_FRect rect{0.0f, 0.0f, kCanvasWidth, kCanvasHeight};
    SDL_RenderFillRectF(renderer, &rect);
}

void CharacterOverlay::draw_tabs(SDL_Renderer *renderer) {
    const std::size_t active = static_cast<std::size_t>(tab_);
    SDL_SetRenderDrawColor(renderer, 216, 200, 154, 220);
    const SDL_FRect rect{kTabX[active], kTabY, kTabW, kTabH};
    SDL_RenderDrawRectF(renderer, &rect);
}

void CharacterOverlay::draw_close(SDL_Renderer *renderer) {
    const std::string_view path =
        hover_close_ ? "engineres/newinv/exitl.bmp" : "engineres/newinv/exit.bmp";
    if (gl::render::Texture *texture = load_texture(path); texture != nullptr) {
        const SDL_FRect rect{920.0f, 694.0f, 75.0f, 68.0f};
        SDL_RenderCopyF(renderer, texture->get(), nullptr, &rect);
    }
}

void CharacterOverlay::draw_rows(SDL_Renderer *renderer,
                                 const std::vector<gl::game::CharacterRow> &rows,
                                 const int *y_positions, int count, float x, float w,
                                 float height, bool right_align) {
    if (font_ == nullptr) return;
    for (int index = 0; index < count && index < static_cast<int>(rows.size()); ++index) {
        const auto &row = rows[static_cast<std::size_t>(index)];
        const float y = static_cast<float>(y_positions[index]);
        const SDL_Rect clip{static_cast<int>(x), static_cast<int>(y),
                            static_cast<int>(w), static_cast<int>(height)};
        SDL_RenderSetClipRect(renderer, &clip);
        font_->draw_text(renderer, row.label, x + 4.0f, y + (height - font_->line_height()) * 0.5f,
                         kTextColor);
        if (right_align) {
            const auto measured = font_->measure(row.value);
            font_->draw_text(renderer, row.value, x + w - 4.0f - measured.x,
                             y + (height - font_->line_height()) * 0.5f, kTextColor);
        } else {
            font_->draw_text(renderer, row.value, x + 4.0f,
                             y + (height - font_->line_height()) * 0.5f, kTextColor);
        }
        SDL_RenderSetClipRect(renderer, nullptr);
    }
}

void CharacterOverlay::draw_content(SDL_Renderer *renderer) {
    if (font_ == nullptr) return;
    static constexpr std::array<int, 7> kPrimaryRowsY{60, 88, 117, 144, 172, 199, 226};
    draw_rows(renderer, sheet_.primary, kPrimaryRowsY.data(), 7, 20.0f, 130.0f, 23.0f,
              true);
    if (sheet_.primary.size() == 8) {
        const auto &reputation = sheet_.primary[7];
        font_->draw_text(renderer, reputation.label, 24.0f, 264.0f, kTextColor);
        const auto measured = font_->measure(reputation.value);
        font_->draw_text(renderer, reputation.value, 181.0f - 4.0f - measured.x + 20.0f,
                         264.0f, kTextColor);
    }
    font_->draw_text(renderer, std::to_string(sheet_.skill_points), 526.0f, 634.0f,
                     kTextColor);

    switch (tab_) {
    case gl::game::CharacterTab::personality: {
        static constexpr std::array<int, 4> kY{102, 134, 163, 214};
        draw_rows(renderer, sheet_.personality, kY.data(), 4, 300.0f, 164.0f, 20.0f,
                  true);
        break;
    }
    case gl::game::CharacterTab::characteristics: {
        std::array<int, 15> y{};
        for (std::size_t index = 0; index < y.size(); ++index)
            y[index] = static_cast<int>(kCharacteristicsY[index]);
        draw_rows(renderer, sheet_.characteristics, y.data(), 15, 296.0f, 155.0f, 20.0f,
                  true);
        break;
    }
    case gl::game::CharacterTab::skills: {
        for (std::size_t index = 0; index < sheet_.skills.size() && index < kSkillX.size();
             ++index) {
            const float x = kSkillX[index];
            const float y = kSkillY[index];
            const auto &row = sheet_.skills[index];
            font_->draw_text(renderer, row.label, x + 4.0f, y + 2.0f, kTextColor);
            const auto measured = font_->measure(row.value);
            font_->draw_text(renderer, row.value, x + 140.0f - 4.0f - measured.x, y + 2.0f,
                             kTextColor);
        }
        break;
    }
    case gl::game::CharacterTab::state:
    case gl::game::CharacterTab::resistance:
        font_->draw_text(renderer, "Нет данных", 470.0f, 300.0f, kTextColor);
        break;
    }
}

bool CharacterOverlay::handle_mouse(int x, int y, bool click) {
    if (!visible_) return false;
    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;
    const bool on_close =
        contains(920.0f, 694.0f, 75.0f, 68.0f, logical_x, logical_y);
    hover_close_ = on_close;
    if (!click) return on_close;
    for (std::size_t index = 0; index < kTabX.size(); ++index) {
        if (contains(kTabX[index], kTabY, kTabW, kTabH, logical_x, logical_y)) {
            tab_ = static_cast<gl::game::CharacterTab>(index);
            return true;
        }
    }
    if (on_close) {
        close();
        return true;
    }
    return false;
}

bool CharacterOverlay::handle_key(SDL_Keycode key) {
    if (!visible_) return false;
    if (key == SDLK_ESCAPE || key == SDLK_AC_BACK || key == SDLK_c) {
        close();
        return true;
    }
    if (key >= SDLK_1 && key <= SDLK_5) {
        tab_ = static_cast<gl::game::CharacterTab>(key - SDLK_1);
        return true;
    }
    return false;
}

CharacterRegion CharacterOverlay::hit_test(int x, int y) const {
    if (!visible_ || scale_ <= 0.0f) return CharacterRegion::none;
    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;
    for (const float tab_x : kTabX)
        if (contains(tab_x, kTabY, kTabW, kTabH, logical_x, logical_y))
            return CharacterRegion::tab;
    if (contains(920.0f, 694.0f, 75.0f, 68.0f, logical_x, logical_y))
        return CharacterRegion::close;
    return CharacterRegion::none;
}
} // namespace gl::app
```

Note: `CharacterOverlay::render` must compile with `GL_GAME_DIR`; the test skips only the real-asset cases. If `font_` is null, rows are skipped; the tabs/background still render.

- [ ] **Step 5: Build and run**

```powershell
cmake --build build
build\bin\character_overlay_tests.exe
```
Expected: 2 cases pass (skip without `GL_GAME_DIR`).

---

### Task 3: Dynamic equipped hero doll

**Files:**
- Create: `src/app/include/gl/app/hero_portrait.hpp`
- Create: `src/app/src/hero_portrait.cpp`
- Modify: `src/app/include/gl/app/character_overlay.hpp`
- Modify: `src/app/src/character_overlay.cpp`
- Modify: `tests/app/test_character_overlay.cpp`

**Interfaces:**
- Consumes: `gl::game::load_hero_visual`, `gl::game::HeroOutfit`, `gl::assets::pad_person_row`, `gl::assets::PadAnimationType::rt_stay`, `gl::render::Texture::from_image`.
- Produces: `class HeroPortrait { bool load(const GameVfs&, const HeroOutfit&); bool ready() const; void render(SDL_Renderer*, float x, float y, float w, float h) const; };`

- [ ] **Step 1: Add the failing doll test**

Append to `tests/app/test_character_overlay.cpp`:

```cpp
TEST_CASE("hero portrait composes the equipped idle frame" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    gl::game::HeroOutfit outfit;
    outfit.wear_set = "noweapon_thrw";
    gl::app::HeroPortrait portrait;
    REQUIRE(portrait.load(vfs.value(), outfit));
    REQUIRE(portrait.ready());
    SoftwareRenderer software(300, 340);
    REQUIRE(software.renderer != nullptr);
    SDL_SetRenderDrawColor(software.renderer, 0, 0, 0, 255);
    SDL_RenderClear(software.renderer);
    portrait.render(software.renderer, 0.0f, 0.0f, 300.0f, 340.0f);
    const auto pixels = snapshot(software.renderer, software.surface);
    std::size_t non_black = 0;
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
        if (pixels[i] != 0 || pixels[i + 1] != 0 || pixels[i + 2] != 0) ++non_black;
    CHECK(non_black > 100);
}
```

Add includes:
```cpp
#include <gl/app/hero_portrait.hpp>
#include <gl/game/hero_wear.hpp>
```

- [ ] **Step 2: Build and confirm red**

```powershell
cmake --build build
build\bin\character_overlay_tests.exe
```
Expected: compile error for missing `hero_portrait.hpp`.

- [ ] **Step 3: Implement `HeroPortrait`**

`src/app/include/gl/app/hero_portrait.hpp`:

```cpp
#pragma once
#include <gl/assets/pad.hpp>
#include <gl/game/hero_wear.hpp>
#include <gl/render/texture.hpp>
#include <SDL.h>

namespace gl::assets { class GameVfs; }

namespace gl::app {
class HeroPortrait {
  public:
    bool load(SDL_Renderer *renderer, const gl::assets::GameVfs &vfs,
              const gl::game::HeroOutfit &outfit);
    bool ready() const { return ready_; }
    void render(SDL_Renderer *renderer, float x, float y, float w, float h) const;

  private:
    gl::render::Texture body_;
    gl::assets::WearHadAnimation header_;
    bool ready_ = false;
};
} // namespace gl::app
```

`src/app/src/hero_portrait.cpp`:

```cpp
#include "gl/app/hero_portrait.hpp"
#include "gl/assets/vfs.hpp"
#include <algorithm>

namespace gl::app {
bool HeroPortrait::load(SDL_Renderer *renderer, const gl::assets::GameVfs &vfs,
                        const gl::game::HeroOutfit &outfit) {
    ready_ = false;
    if (renderer == nullptr) return false;
    auto visual = gl::game::load_hero_visual(vfs, outfit);
    if (!visual) return false;
    const auto it = visual->animations().find(0x1u);
    if (it == visual->animations().end()) return false;
    auto texture = gl::render::Texture::from_image(renderer, it->second.body);
    if (!texture) return false;
    body_ = std::move(texture.value());
    header_ = it->second.header;
    ready_ = true;
    return true;
}

void HeroPortrait::render(SDL_Renderer *renderer, float x, float y, float w,
                          float h) const {
    if (!ready_ || renderer == nullptr) return;
    const int frame_w = header_.frame_width;
    const int frame_h = header_.frame_height;
    if (frame_w <= 0 || frame_h <= 0) return;
    const auto row = gl::assets::pad_person_row(gl::assets::PadAnimationType::rt_stay, 0);
    const SDL_Rect source{0, static_cast<int>(row.row) * frame_h, frame_w, frame_h};
    int scale = std::min({3, static_cast<int>(w) / frame_w,
                          static_cast<int>(h) / frame_h});
    scale = std::max(scale, 1);
    const float draw_w = static_cast<float>(frame_w * scale);
    const float draw_h = static_cast<float>(frame_h * scale);
    const SDL_FRect dest{x + (w - draw_w) * 0.5f, y + h - draw_h, draw_w, draw_h};
    SDL_RenderCopyF(renderer, body_.get(), &source, &dest);
}
} // namespace gl::app
```

Update the test call to `portrait.load(software.renderer, vfs.value(), outfit)`.

- [ ] **Step 4: Add the portrait to the overlay**

In `character_overlay.hpp`, include `gl/app/hero_portrait.hpp` and add:
```cpp
    bool open(SDL_Renderer *renderer, const gl::assets::GameVfs &vfs,
              gl::game::CharacterSheet sheet, const gl::game::HeroOutfit *outfit);
    ...
    HeroPortrait portrait_;
```
Update `character_overlay.cpp` `open`:
```cpp
bool CharacterOverlay::open(SDL_Renderer *renderer, const gl::assets::GameVfs &vfs,
                            gl::game::CharacterSheet sheet,
                            const gl::game::HeroOutfit *outfit) {
    vfs_ = &vfs;
    sheet_ = std::move(sheet);
    tab_ = gl::game::CharacterTab::skills;
    visible_ = true;
    scale_ = 1.0f;
    offset_x_ = 0.0f;
    offset_y_ = 0.0f;
    if (renderer != nullptr) {
        reset_texture_cache(renderer);
        if (outfit != nullptr) portrait_.load(renderer, vfs, *outfit);
    }
    return true;
}
```
In `render`, after `draw_content`:
```cpp
    portrait_.render(renderer, 8.0f, 296.0f, 270.0f, 320.0f);
```
Update the tests' `open` calls to pass `software.renderer`/`nullptr` and the outfit (null in the static tests).

- [ ] **Step 5: Build and run**

```powershell
cmake --build build
build\bin\character_overlay_tests.exe
```
Expected: 3 cases pass.

---

### Task 4: `glgame` integration

**Files:**
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp`
- Modify: `src/app/include/gl/app/input_gate.hpp`
- Modify: `tests/app/test_level_click.cpp` (input-gate assertion)
- Modify: `tests/game/test_character_sheet.cpp` (real slot0 case)

**Interfaces:**
- Consumes: Task 1-3 `CharacterSheet`, `CharacterOverlay`, `HeroPortrait`.
- Produces: `--character`, `C`, inventory skills-tab entry, input gating, screenshot coverage.

- [ ] **Step 1: Add the input gate field and test**

In `input_gate.hpp`:
```cpp
inline bool level_input_enabled(bool dialog_active, bool save_slots_active,
                                bool console_active, bool inventory_active = false,
                                bool trade_active = false, bool diary_active = false,
                                bool character_active = false) {
    return !dialog_active && !save_slots_active && !console_active && !inventory_active &&
           !trade_active && !diary_active && !character_active;
}
```
In `tests/app/test_level_click.cpp`, add:
```cpp
    CHECK_FALSE(gl::app::level_input_enabled(false, false, false, false, false, false, true));
```

- [ ] **Step 2: Add the `--character` option and state**

In `tools/glgame/main.cpp`:
- include `gl/app/character_overlay.hpp`, `gl/assets/sdb.hpp`, `gl/game/character_sheet.hpp`.
- add `bool character = false;` to the options struct near `inventory`.
- parse `arg == "--character"` → `options.character = true;`.
- help text: `"  --character          Open the character sheet for the loaded save\n"`.
- members:
```cpp
    std::optional<gl::assets::Sdb> user_interface_sdb_;
    gl::app::CharacterOverlay character_overlay_;
    std::optional<gl::game::HeroOutfit> character_outfit_;
```
- after VFS mount (near item catalog load), load:
```cpp
    if (auto data = vfs_.read("sdb/user_interface.sdb"); data.has_value()) {
        if (auto parsed = gl::assets::Sdb::parse(data.value()); parsed.has_value())
            user_interface_sdb_ = std::move(parsed.value());
        else
            gl::log(gl::LogLevel::warning, "user interface sdb: " + parsed.error().message);
    }
```
- set `character_outfit_` wherever the level path resolves the hero outfit: after `resolve_hero_outfit` succeeds (`character_outfit_ = outfit;`) and in the `make_naked_outfit()` fallback (`character_outfit_ = make_naked_outfit();`).
- `open_character()`:
```cpp
    bool open_character() {
        if (!user_interface_sdb_ || !person_runtime_.has_person("Hero")) return false;
        auto sheet = gl::game::build_character_sheet(
            person_runtime_, *user_interface_sdb_, "Hero");
        character_overlay_.set_font(inventory_font());
        character_overlay_.open(window_.renderer(), vfs_, std::move(sheet),
                                character_outfit_ ? &*character_outfit_ : nullptr);
        return true;
    }
```
- after `--inventory` handling (line ~822): `if (options_.character) open_character();`.
- key handling in the key event block:
```cpp
        if (character_overlay_.visible()) {
            if (key.repeat == 0) character_overlay_.handle_key(symbol);
            return;
        }
        if (symbol == SDLK_c) {
            if (!character_overlay_.visible()) {
                if (inventory_overlay_.visible()) inventory_overlay_.close();
                open_character();
            } else {
                character_overlay_.close();
            }
            return;
        }
```
Place this before the inventory key handling.
- mouse handling: before inventory handling:
```cpp
        if (character_overlay_.visible()) {
            character_overlay_.handle_mouse(x, y, true);
            return;
        }
        if (inventory_overlay_.visible()) {
            const auto region = inventory_overlay_.hit_test(x, y);
            inventory_overlay_.handle_mouse(x, y, true);
            if (region == gl::app::InventoryRegion::skills_tab) {
                inventory_overlay_.close();
                open_character();
            }
            return;
        }
```
(match the actual mouse event block; keep the existing drag/wheel paths).
- render after inventory/diary:
```cpp
        if (character_overlay_.visible()) {
            character_overlay_.render(window_.renderer(), viewport_width_, viewport_height_,
                                      inventory_font());
        }
```
- pass `character_overlay_.visible()` to both `level_input_enabled` calls (line ~905 and ~2926).

- [ ] **Step 3: Add the real slot0 model test**

Append to `tests/game/test_character_sheet.cpp`:
```cpp
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

TEST_CASE("character sheet reads real slot0" * doctest::skip(std::getenv("GL_GAME_DIR") == nullptr)) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    const std::filesystem::path path =
        std::filesystem::path(dir) / "Saves" / "slot0.sav";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    auto body = gl::assets::decode_save_body(bytes);
    REQUIRE(body.has_value());
    auto stats = gl::assets::parse_hero_stats(body.value());
    REQUIRE(stats.has_value());
    auto sdb_data = vfs->read("sdb/user_interface.sdb");
    REQUIRE(sdb_data.has_value());
    auto sdb = gl::assets::Sdb::parse(sdb_data.value());
    REQUIRE(sdb.has_value());
    auto runtime = gl::game::PersonRuntime::create({}, stats.value());
    const auto sheet = gl::game::build_character_sheet(runtime, sdb.value());
    CHECK(sheet.primary[0].value == "30");
    CHECK(sheet.primary[1].value == "25");
    CHECK(sheet.primary[2].value == "18");
    CHECK(sheet.primary[3].value == "12");
    CHECK(sheet.primary[4].value == "7");
    CHECK(sheet.primary[5].value == "5");
    CHECK(sheet.primary[6].value == "9");
    CHECK(sheet.primary[7].value == "-40");
    CHECK(sheet.characteristics[1].value == "81099");
    CHECK(sheet.skills[0].value == "0");   // SWORD
    CHECK(sheet.skills[1].value == "15");  // AXE
    CHECK(sheet.skills[6].value == "0");   // THROW
    CHECK(sheet.skills[21].value == "0");  // TALKING
}
```

- [ ] **Step 4: Add the screenshot case**

In `tests/app/test_glgame_screenshot.cpp`, add a case using the existing `screenshot_arguments`/`TempDir` helpers:
```cpp
TEST_CASE("glgame --character --load-save renders the sheet") {
    const char *game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path png = temp.path / "character.png";
    const int code = run_glgame(screenshot_arguments(
        game_dir, config, png,
        {"--load-save", "0", "--character", "--width", "1024", "--height", "768",
         "--seed", "1"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(fs::file_size(png) > 0);
}
```

- [ ] **Step 5: Build and run**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*character sheet*"
build\bin\character_overlay_tests.exe
build\bin\glgame_screenshot_tests.exe --test-case="*character*"
```
Expected: all pass with `GL_GAME_DIR`.

---

### Task 5: Documentation and full verification

**Files:**
- Modify: `docs/research/p7b-character-sheet.md` (new)
- Modify: `docs/DECISIONS.md` (D-112)
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`

- [ ] **Step 1: Write research**

Create `docs/research/p7b-character-sheet.md` with: `Client.dll` SHA/base; `user_interface.sdb` id map (0–4 tabs, 5 name, 6–12 attributes, 13–17 characteristics, 18–28 combat/life, 43–48 immunities, 49–75 skills, 76 points); `inv_gui.scr` rects; skill permutation `0x1212e0a0`; screen arrays `0x120f8520`/`0x120f85a0`/`0x120f8620`/`0x120f8738`/`0x120f8774`; real slot0 values; residual.

Also update `docs/specs/2026-11-23-p7b-character-sheet-design.md` where it says a missing person yields `name Hero`: align it with the implementation (name = the requested person string, `Hero` in normal use; test uses `missing`).

- [ ] **Step 2: Add the decision**

Append to `docs/DECISIONS.md`:
```markdown
| D-112 | 2026-11-23 | P7b read-only character sheet - `CharacterOverlay` renders `inventory_skills.bmp` with five sub-tabs (personality/characteristics/skills/state/resistance), 8 left attributes, 27 skills in the original `0x1212e0a0` order, 15 characteristics rows (level/EXP/reputation/life/energy modeled; combat/armor/accuracy/crit `-`), skill points 0, and the equipped hero idle frame via `HeroPortrait` (P4d/P5a). Opened by the inventory skills tab and `C`; `--character` for screenshots; `level_input_enabled` gains `character_active`. Labels from `sdb/user_interface.sdb`. Skill-point spending, state/resistance data, remaining combat columns, tooltips, animation and touch are out of scope. | spec/plan 2026-11-23 p7b; research p7b-character-sheet.md; D-045, D-110. |
```

- [ ] **Step 3: Update STATUS/PROJECT**

Add a P7b section to `docs/STATUS.md` with task, spec/plan, result, verification, residual, status. Extend the P7 row in `docs/PROJECT.md` with the character sheet.

- [ ] **Step 4: Run focused and full verification**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*character sheet*"
build\bin\character_overlay_tests.exe
build\bin\glgame_screenshot_tests.exe --test-case="*character*"
ctest --test-dir build --output-on-failure
```
Record the actual cases/assertions and full `ctest` N/N + seconds in `STATUS.md`.

- [ ] **Step 5: Verify docs**

Check UTF-8/no BOM/NUL and `git diff --check` for all touched docs. Report output; do not commit without explicit user permission.
