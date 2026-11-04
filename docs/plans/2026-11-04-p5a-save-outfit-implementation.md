# P5a Save → Outfit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Выбор save-слота применяет экипировку из `BT_HERO` к hero (outfit), без загрузки мира.

**Architecture:** `gl_assets` декодирует save/`BT_HERO` и отдаёт `HeroEquipment`; `gl_game` сопоставляет item→TECH→wear group и строит `HeroOutfit`; `gl_app` перезагружает wear-текстуры без reload уровня; `glgame` хранит выбранный слот и применяет outfit.

**Tech Stack:** C++20, miniz, doctest, SDL2 (только app), существующие `SaveManager`, `GameVfs`, `load_hero_visual`.

## Global Constraints

- Источник истины: `Server.dll` SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`, `GoldenLand.exe` SHA-256 `7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF`.
- Layout: `BT_HERO ` + u32 size; body `+12`; fixed `0x734`; 3 контейнера; третий (`hero+0x109C`) — equipped. Record: `0xC0` raw + u32 subcount + `subcount*0x14`; поля `+0x08 type`, `+0x0C mask`, `+0x20 style`, `+0xA8 item_id`.
- item→TECH: reverse lookup `sdb/items/tech_names.sdb`; group/set — `scripts/herowear.scr`; активен set A.
- Без game assets/сейвов в git; docs UTF-8 no BOM; комментарии в коде не добавлять.
- Fallback: ошибка/неизвестный item/tech/asset → naked (`noweapon_thrw`, пустые слои) + warning.
- Каждый task заканчивается passing targeted tests + commit; полный `ctest` в Task 5.

---

### Task 1: `BT_HERO` equipped parser

**Files:**
- Create: `src/assets/include/gl/assets/hero_equipment.hpp`
- Create: `src/assets/src/hero_equipment.cpp`
- Modify: `src/assets/CMakeLists.txt`
- Test: `tests/assets/test_hero_equipment.cpp` (add to `gl_assets_tests`)
- Test: `tests/assets/test_hero_equipment_real.cpp` (add to `gl_assets_real_tests`)

**Interfaces:**
- Produces:
```cpp
struct EquippedItem {
    std::uint32_t item_id = 0;
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t style = 0;
};
struct HeroEquipment { std::vector<EquippedItem> items; };
Result<std::vector<std::uint8_t>> decode_save_body(std::span<const std::uint8_t> file);
Result<HeroEquipment> parse_hero_equipment(std::span<const std::uint8_t> body);
```

- [x] **Step 1: Write failing synthetic parser test**

```cpp
TEST_CASE("parse_hero_equipment reads third BT_HERO container") {
    std::vector<std::uint8_t> body(0x734, 0);
    auto put32 = [&](std::size_t off, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) body[off + i] = static_cast<std::uint8_t>(v >> (i * 8));
    };
    std::size_t pos = 0x734;
    put32(pos, 1); put32(pos + 4, 0); pos += 8;      // container 0
    put32(pos, 1); put32(pos + 4, 0); pos += 8;      // container 1
    put32(pos, 1); put32(pos + 4, 1); pos += 8;      // container 2: one equipped
    body.resize(pos + 0xC0 + 4);
    put32(pos + 0x08, 12);      // type armor
    put32(pos + 0x0C, 0x100000);// mask armor
    put32(pos + 0x20, 13);      // style
    put32(pos + 0xA8, 318);     // item id
    put32(pos + 0xC0, 0);       // subcount
    auto parsed = gl::assets::parse_hero_equipment(body);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->items.size() == 1);
    CHECK(parsed->items[0].item_id == 318);
    CHECK(parsed->items[0].type == 12);
    CHECK(parsed->items[0].flags == 0x100000);
}
```

- [x] **Step 2: Run test, expect fail**

Run: `cmake --build build --target gl_assets_tests && build\bin\gl_assets_tests.exe --test-case="*hero_equipment*"`
Expected: compile/`not found` failure.

- [x] **Step 3: Implement parser**

Implement in `hero_equipment.cpp` using `ByteReader`/span bounds checks:
find tag `BT_HERO `, read u32 size, body slice; skip `0x734`; read 3 containers
(`u32 id; if id==0xFFFFFFFF continue; u32 count;`); parse records
`0xC0 + 4 + subcount*0x14`; collect third container item_id/type/flags/style;
return `format` errors on truncation. `decode_save_body`: validate
`parse_save_header`, `mz_uncompress` from offset 112 into a growing 32 MiB buffer.

- [x] **Step 4: Add real test slot0/slot2**

```cpp
TEST_CASE("real save equipped layout slot0/slot2" * doctest::skip(!getenv("GL_GAME_DIR"))) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    for (auto [slot, expect] : {std::pair{0, std::vector<std::uint32_t>{486,318,280,397,554,353,72,80,233,245,141}},
                                std::pair{2, std::vector<std::uint32_t>{486,317,278,396,554,359,66,99,236,243,108}}}) {
        auto file = std::ifstream(std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" / ("slot" + std::to_string(slot) + ".sav"), std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
        auto body = gl::assets::decode_save_body(bytes);
        REQUIRE(body.has_value());
        auto eq = gl::assets::parse_hero_equipment(*body);
        REQUIRE(eq.has_value());
        std::vector<std::uint32_t> ids;
        for (auto &item : eq->items) ids.push_back(item.item_id);
        std::sort(ids.begin(), ids.end());
        auto sorted = expect; std::sort(sorted.begin(), sorted.end());
        CHECK(ids == sorted);
    }
}
```

- [x] **Step 5: Build and run**

Run: `cmake --build build --target gl_assets_tests gl_assets_real_tests && build\bin\gl_assets_tests.exe --test-case="*hero_equipment*" && build\bin\gl_assets_real_tests.exe --test-case="*equipped layout*"`
Expected: PASS; real test skipped only without `GL_GAME_DIR`.

- [x] **Step 6: Commit**

`git add src/assets/... tests/assets/... && git commit -m "feat(assets): parse BT_HERO equipped items"`

---

### Task 2: item → TECH → wear catalog

**Files:**
- Create: `src/assets/include/gl/assets/herowear.hpp`, `src/assets/src/herowear.cpp`
- Create: `src/game/include/gl/game/hero_outfit_resolver.hpp`, `src/game/src/hero_outfit_resolver.cpp`
- Modify: `src/assets/CMakeLists.txt`, `src/game/CMakeLists.txt`
- Test: `tests/assets/test_herowear.cpp`, `tests/game/test_hero_outfit_resolver.cpp`

**Interfaces:**
- Produces:
```cpp
struct HeroWearEntry { std::string tech, category, group, base_set; };
struct HeroWearCatalog { std::vector<HeroWearEntry> entries; };
Result<HeroWearCatalog> parse_herowear(std::span<const std::uint8_t> text);
Result<HeroOutfit> resolve_hero_outfit(const HeroEquipment&, const Sdb& tech_names,
                                       const HeroWearCatalog&);
```

- [x] **Step 1: Write failing catalog/resolver tests**

Use real `sdb/items/tech_names.sdb` and `scripts/herowear.scr`; assert
`tech_names.find(318)=="ARM_1_13_1"`, `parse_herowear(...).find("ARM_1_13_1").group=="Dospeh_almaz"`.
Resolver test: build `HeroEquipment` from slot0 IDs/masks; expect
`wear_set=="two_axes_maces"`, `armor=="dospeh_almaz"`, `elbarms=="ponogi_goluboy"`,
`helmet=="shlem_goluboy"`, `weapon=="axe_goluboy2"`.

- [x] **Step 2: Run, expect fail**

`cmake --build build --target gl_assets_tests gl_game_tests && build\bin\gl_game_tests.exe --test-case="*outfit*"`

- [x] **Step 3: Implement herowear parser + resolver**

Parse `herowear.scr` lines `category <c>`, `group "<Group>" ["<base>"]`,
`item "<TECH>"`; assign group id as file order, reuse duplicate names;
map TECH→entry. Resolver: choose set A by mask bits (`0x001000` weapon,
`0x400000` shield; shared armor/helmet/elbarms); item id→TECH via SDB reverse;
TECH→entry; set fields; base set is weapon's `base_set` else `noweapon_thrw`.
Missing item/tech/group → empty optional for category.

- [x] **Step 4: Real verify slot0/slot2**

Extend tests to check slot2: `wear_set=="two_axes_maces"`, armor
`dospeh_cherny`, elbarms `ponogi_goluboy`, helmet `shlem_goluboy`,
weapon `axe_goluboy2`; both sets' assets exist in `wear.pak` (18/18).

- [x] **Step 5: Run tests + commit**

`ctest --test-dir build -R "gl_assets_tests|gl_game_tests" --output-on-failure`
then `git commit -m "feat(game): resolve equipped items to hero outfit"`.

---

### Task 3: runtime outfit application

**Files:**
- Modify: `src/app/include/gl/app/level_screen.hpp`, `src/app/src/level_screen.cpp`
- Test: `tests/app/test_level_click.cpp` (existing gl_app test target)

**Interfaces:**
- Produces: `void LevelScreen::apply_hero_outfit(const HeroOutfit&);`
  and `const gl::assets::WearHadAnimation* LevelScreen::hero_header(std::uint32_t mask) const;`
- Consumes: `load_hero_visual`, existing `hero_textures_/hero_headers_`.

- [x] **Step 1: Write failing test**

In `tests/app/test_level_click.cpp`, load `l1_1` with outfit none, call
`apply_hero_outfit(slot0)`, and assert
`screen.hero_header(0x20)->frame_width == 75`; call naked outfit and assert
`screen.hero_header(0x1)->frame_width == 65`.

- [x] **Step 2: Implement**

`apply_hero_outfit`: call `load_hero_visual(vfs_, outfit)`; on error log warning
and retry with an explicitly cleared naked outfit (`noweapon_thrw`, empty layers;
`HeroOutfit{}` defaults are non-empty); clear/replace `hero_textures_`/
`hero_headers_`; no level reload. Store `hero_outfit_` for reapply after reload
if needed.

- [x] **Step 3: Run targeted + commit**

`cmake --build build --target person_render_tests && build\bin\person_render_tests.exe`
then `git commit -m "feat(app): reapply hero outfit without level reload"`.

---

### Task 4: save-slot outfit flow

**Files:**
- Modify: `tools/glgame/main.cpp` (save selection/storage, `load_level`)
- Test: `tests/app/test_glgame_screenshot.cpp` (headless `--equip-save` case)
- Modify: `tools/glgame/main.cpp` usage/help.

**Interfaces:**
- Consumes: `SaveManager`, `decode_save_body`, `parse_hero_equipment`,
  `resolve_hero_outfit`, `LevelScreen::apply_hero_outfit`.
- Produces: `Game::pending_equipment_` (`HeroEquipment`); save-slot selection
  stores all equipped records and resolves to `HeroOutfit` only on apply/load.

- [x] **Step 1: Add headless options `--equip-save <slot>` / `--equip-save-pending`**

Parse slot; on boot read `Saves/slot<N>.sav`, decode/parse all equipped
records into `pending_equipment_`. With `--equip-save-pending`, store before
the next `load_level`; otherwise resolve and apply to the open level. This is
the automated-test hook; menu uses the same helper.

- [x] **Step 2: Wire menu `save_activate`**

If `display_saves_` empty return; take selected slot; read/parse/store
`pending_equipment_`; resolve to `HeroOutfit` and apply to the open level or
mark for next `load_level`, message `outfit: slot N`; on error store empty
equipment (naked on next load), message warning, never block.

- [x] **Step 3: Apply in `load_level`**

Resolve `pending_equipment_` to `HeroOutfit` and pass to
`LevelScreen::load`; if empty/error use naked. Keep `pending_equipment_`
for subsequent level loads until another save slot is selected.

- [x] **Step 4: Screenshot test**

Run glgame with `--level l1_1 --equip-save 0/2 --screenshot ...`; assert
acceptance against slot0/slot2 outfit (hash or non-empty equipment textures).
Also run `--equip-save <slot> --equip-save-pending` and compare the result to
the direct slot image, and malformed/invalid slot → naked with exit 0.

- [x] **Step 5: Run + commit**

`ctest --test-dir build -R glgame_screenshot_tests --output-on-failure` then
`git commit -m "feat(glgame): apply outfit from selected save"`.

---

### Task 5: docs, full verification, gate

**Files:**
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`, `docs/DECISIONS.md` (if new
  decision), spec/plan status.

- [x] **Step 1: Update docs**

Record: parser/resolver APIs, slot0/slot2 verification, fallback naked,
known A/B risk, `shlem_cherny` not used. Update P5 status `[~]`/`[x]` as
appropriate.

- [x] **Step 2: Full ctest**

`$env:GL_GAME_DIR="I:\Games\Златогорье-2"; ctest --test-dir build --output-on-failure`
Expected: 17/17 (or current count) passed; record time.

- [ ] **Step 3: Manual gate**

User compares glgame with slot0 and slot2 against original. If visually
different, write findings to STATUS and do not claim P5a accepted.

- [x] **Step 4: Commit docs**

`git add docs && git commit -m "docs(p5): record save outfit slice"`

---

## Self-review notes

- Task 1 parser uses corrected `0x14` stride and third container from research.
- Task 2 uses SDB reverse lookup, not ITM type/style formula; active set A only.
- Task 3/4 keep SDL in app/tools; no assets dependency on SDL.
- No inventory UI/stats/full save load: matching spec non-goals.
