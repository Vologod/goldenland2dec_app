# P5f Weapon/Shield Sets A/B — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Runtime A/B weapon-set state with Z and inactive-slot-click toggles, applied to combat stats, hero outfit, character sheet and inventory.

**Architecture:** SDL-free `weapon_set` masks; `StatInputs.use_b_set`; resolver parameter; overlays expose both slot areas + toggle requests; glgame owns the active set and re-applies outfit/refresh overlays.

**Tech Stack:** C++20, CMake+Ninja, doctest, SDL2 app layer.

## Global Constraints

- C++20; `gl/game` SDL-free; SDL only in app/render/tools; PascalCase types, snake_case functions; no comments.
- Masks: weapon A `0x001000`, B `0x002000`; shield A `0x400000`, B `0x800000`; helmet/armor/elbarms common.
- Active set default A, **not saved**; reset to A on save load and New Game.
- Combat stats and outfit use only the active set (**no fallback**). Slot0 A `armor=15` must stay.
- Triggers: `Z` (`SDLK_z`/'Z') and click on the **inactive** weapon/shield slot in inventory/character overlays.
- Inactive pair icon is not drawn; both slot areas remain hit-testable. Active pair may get a frame (port UX, D-117).
- Slot0 B oracle: `armor=35`, `primary[3]=13`, `damage[0]=0`, `damage[1]>0`, `damage[2]>0`, weight `1320/2046`.
- Toolchain:
  `$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"; $env:GL_GAME_DIR="I:\Games\Златогорье-2"`
- Close `glgame.exe` if the linker reports a locked file.

---

### Task 1: Weapon-set masks + combat stats

**Files:**
- Create: `src/game/include/gl/game/weapon_set.hpp`
- Modify: `src/game/include/gl/game/combat_stats.hpp`
- Modify: `src/game/src/combat_stats.cpp`
- Test: `tests/game/test_weapon_set.cpp` (new)
- Modify: `tests/game/test_combat_stats.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `enum class gl::game::WeaponSet : std::uint8_t { a = 0, b = 1 };`
  - `gl::game::WeaponSetMasks gl::game::weapon_set_masks(WeaponSet)`
  - `StatInputs::use_b_set` (`bool`, default false).

- [ ] **Step 1: Write the failing tests**

Create `tests/game/test_weapon_set.cpp`:

```cpp
#include <doctest/doctest.h>

#include "gl/game/combat_stats.hpp"
#include "gl/game/weapon_set.hpp"

namespace {
gl::game::EquippedStatItem weapon(std::uint32_t mask, std::array<std::uint32_t, 6> damage) {
    gl::game::EquippedStatItem item;
    item.type = 0;
    item.flags = mask;
    item.durability = 100;
    item.damage = damage;
    item.unknown_b = {0, 0, 0, 0, 10};
    return item;
}

gl::game::EquippedStatItem shield(std::uint32_t mask, std::uint32_t armor) {
    gl::game::EquippedStatItem item;
    item.type = 13;
    item.flags = mask;
    item.durability = 100;
    item.unknown_b = {0, 0, 0, armor, 0};
    return item;
}
} // namespace

TEST_CASE("weapon_set_masks selects A and B pairs") {
    const auto a = gl::game::weapon_set_masks(gl::game::WeaponSet::a);
    CHECK(a.weapon == 0x001000u);
    CHECK(a.shield == 0x400000u);
    const auto b = gl::game::weapon_set_masks(gl::game::WeaponSet::b);
    CHECK(b.weapon == 0x002000u);
    CHECK(b.shield == 0x800000u);
}

TEST_CASE("compute_armor_weight uses the active weapon and shield set") {
    gl::game::StatInputs inputs;
    inputs.strength = 10;
    inputs.constitution = 10;
    inputs.dexterity = 10;
    inputs.perception = 10;
    inputs.luck = 10;
    inputs.wisdom = 10;
    inputs.intelligence = 10;
    inputs.experience = 1000;
    inputs.skills.fill(0);
    inputs.equipped = {
        weapon(0x001000u, {10, 20, 0, 0, 0, 0}),
        weapon(0x002000u, {0, 0, 30, 40, 0, 0}),
        shield(0x400000u, 10),
        shield(0x800000u, 20),
    };

    const auto a = gl::game::compute_armor_weight(inputs);
    inputs.use_b_set = true;
    const auto b = gl::game::compute_armor_weight(inputs);

    CHECK(b.armor_class == a.armor_class + 10);
    CHECK(a.damage[0].minimum > 0);
    CHECK(a.damage[1].minimum == 0);
    CHECK(b.damage[0].minimum == 0);
    CHECK(b.damage[2].minimum > 0);
}
```

Modify `tests/game/CMakeLists.txt`: add `test_weapon_set.cpp` after `test_combat_stats.cpp`.

Append to `tests/game/test_combat_stats.cpp`:

```cpp
TEST_CASE("combat stats real: slot0 set B armor, perception and weapon" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    const std::filesystem::path path = std::filesystem::path(dir) / "Saves" / "slot0.sav";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    auto body = gl::assets::decode_save_body(bytes);
    REQUIRE(body.has_value());
    auto stats = gl::assets::parse_hero_stats(body.value());
    REQUIRE(stats.has_value());
    auto inventory = gl::assets::parse_hero_inventory(body.value());
    REQUIRE(inventory.has_value());

    StatInputs inputs;
    inputs.strength = stats->strength;
    inputs.constitution = stats->constitution;
    inputs.dexterity = stats->dexterity;
    inputs.perception = stats->perception;
    inputs.luck = stats->luck;
    inputs.wisdom = stats->wisdom;
    inputs.intelligence = stats->intelligence;
    inputs.experience = stats->experience;
    inputs.person_flags = stats->flags;
    for (std::size_t id = 0; id < inputs.skills.size(); ++id)
        inputs.skills[id] = stats->skills[id];
    for (const auto &item : inventory->equipped) {
        EquippedStatItem equipped;
        equipped.type = item.type;
        equipped.flags = item.flags;
        equipped.durability = item.durability;
        equipped.mods = item.mods;
        if (const auto *entry = catalog->find(item.item_id)) {
            equipped.unknown_b = entry->item.unknown_b;
            equipped.damage = entry->item.damage;
        }
        inputs.equipped.push_back(equipped);
    }
    for (const auto &list : {inventory->hot, inventory->items, inventory->equipped})
        for (const auto &item : list)
            inputs.carried.push_back(
                CarriedItem{static_cast<std::int32_t>(item.count), item.weight_x10});

    const auto a = gl::game::compute_armor_weight(inputs);
    CHECK(a.armor_class == 15);
    inputs.use_b_set = true;
    const auto b = gl::game::compute_armor_weight(inputs);
    CHECK(b.armor_class == 35);
    CHECK(b.primary[3] == 13);
    CHECK(b.damage[0].minimum == 0);
    CHECK(b.damage[1].minimum > 0);
    CHECK(b.damage[2].minimum > 0);
    CHECK(b.weight_cur_x10 == 1320);
    CHECK(b.weight_max_x10 == 2046);
}
```

- [ ] **Step 2: Run tests to verify they fail**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*weapon_set_masks*,*active weapon and shield*"
```
Expected: FAIL — `weapon_set.hpp` missing / no `use_b_set`.

- [ ] **Step 3: Implement**

Create `src/game/include/gl/game/weapon_set.hpp`:

```cpp
#pragma once
#include <cstdint>

namespace gl::game {
enum class WeaponSet : std::uint8_t { a = 0, b = 1 };

inline constexpr std::uint32_t kWeaponMaskA = 0x001000u;
inline constexpr std::uint32_t kWeaponMaskB = 0x002000u;
inline constexpr std::uint32_t kShieldMaskA = 0x400000u;
inline constexpr std::uint32_t kShieldMaskB = 0x800000u;

struct WeaponSetMasks {
    std::uint32_t weapon = kWeaponMaskA;
    std::uint32_t shield = kShieldMaskA;
};

inline constexpr WeaponSetMasks weapon_set_masks(WeaponSet set) {
    return set == WeaponSet::b ? WeaponSetMasks{kWeaponMaskB, kShieldMaskB}
                               : WeaponSetMasks{kWeaponMaskA, kShieldMaskA};
}
} // namespace gl::game
```

In `src/game/include/gl/game/combat_stats.hpp`, inside `StatInputs` add:

```cpp
    bool use_b_set = false;
```

In `src/game/src/combat_stats.cpp`, add `#include "gl/game/weapon_set.hpp"`, remove
`kShieldMaskA`/`kWeaponMaskA`, and in `compute_armor_weight` after `ArmorWeight out;` add:

```cpp
    const WeaponSetMasks masks =
        weapon_set_masks(inputs.use_b_set ? WeaponSet::b : WeaponSet::a);
```

Replace shield/weapon mask tests with `masks.shield` / `masks.weapon`:

```cpp
        if ((item.flags & masks.shield) != 0) {
...
        if ((item.flags & masks.weapon) != 0) {
```

- [ ] **Step 4: Run tests to verify they pass**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*weapon_set_masks*,*active weapon and shield*,*slot0 set B*"
```
Expected: PASS (A stays 15, B 35/11/damage structure).

- [ ] **Step 5: Commit**

```powershell
git add src/game/include/gl/game/weapon_set.hpp src/game/include/gl/game/combat_stats.hpp src/game/src/combat_stats.cpp tests/game/test_weapon_set.cpp tests/game/test_combat_stats.cpp tests/game/CMakeLists.txt
git commit -m "feat: select combat weapon/shield by active set"
```

---

### Task 2: Outfit resolver A/B

**Files:**
- Modify: `src/game/include/gl/game/hero_outfit_resolver.hpp`
- Modify: `src/game/src/hero_outfit_resolver.cpp`
- Test: `tests/game/test_hero_outfit_resolver.cpp`

**Interfaces:**
- Produces: `resolve_hero_outfit(equipment, tech, wear, bool use_b_set = false)`.
- Consumes: Task 1 masks.

- [ ] **Step 1: Write the failing test**

Append to `tests/game/test_hero_outfit_resolver.cpp`:

```cpp
TEST_CASE("resolve_hero_outfit selects set B weapon and shield") {
    auto sdb = assets::Sdb::parse(make_sdb({{1, "AXE_1_13_2"},
                                             {2, "SWR_1_08_1"},
                                             {3, "SHL_1_10_1"}}));
    REQUIRE(sdb.has_value());
    const auto catalog = slot_catalog();
    const auto gear = equipment({
        {1, 1, 0x001000u, 0},
        {2, 0, 0x002000u, 0},
        {3, 13, 0x800000u, 0},
    });

    const auto a = resolve_hero_outfit(gear, sdb.value(), catalog, false);
    REQUIRE(a.has_value());
    CHECK(a->weapon == "axe_goluboy2");
    CHECK(a->shield.empty());

    const auto b = resolve_hero_outfit(gear, sdb.value(), catalog, true);
    REQUIRE(b.has_value());
    CHECK(b->weapon == "sword_platina");
    CHECK(b->shield == "shit_bio");
}
```

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*set B weapon and shield*"
```
Expected: FAIL — resolver takes 3 args / B not selected.

- [ ] **Step 3: Implement**

In `src/game/include/gl/game/hero_outfit_resolver.hpp`:

```cpp
Result<HeroOutfit> resolve_hero_outfit(const gl::assets::HeroEquipment& equipment,
                                       const gl::assets::Sdb& tech_names,
                                       const gl::assets::HeroWearCatalog& wear,
                                       bool use_b_set = false);
```

In `src/game/src/hero_outfit_resolver.cpp`, include `gl/game/weapon_set.hpp`; replace
`kWeaponA`/`kShieldA` constants and selection:

```cpp
    const WeaponSetMasks masks =
        weapon_set_masks(use_b_set ? WeaponSet::b : WeaponSet::a);
...
        if (mask == masks.weapon) {
...
        } else if (mask == masks.shield && item.type == 13) {
```

Keep helmet/armor/elbarms logic unchanged.

- [ ] **Step 4: Run tests to verify they pass**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*resolve_hero_outfit*"
```
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/game/include/gl/game/hero_outfit_resolver.hpp src/game/src/hero_outfit_resolver.cpp tests/game/test_hero_outfit_resolver.cpp
git commit -m "feat: resolve hero outfit for the active weapon set"
```

---

### Task 3: CharacterOverlay A/B slots and toggle request

**Files:**
- Modify: `src/app/include/gl/app/character_overlay.hpp`
- Modify: `src/app/src/character_overlay.cpp`
- Test: `tests/app/test_character_overlay.cpp`

**Interfaces:**
- Produces:
  - `void CharacterOverlay::set_use_b_set(bool use_b);`
  - `bool CharacterOverlay::take_weapon_set_toggle();`
- Behavior: both weapon/shield slots hit-testable; inactive pair icon not drawn; click inactive → request.

- [ ] **Step 1: Write the failing test**

Append to `tests/app/test_character_overlay.cpp`:

```cpp
TEST_CASE("character overlay toggles weapon set from the inactive slot" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    gl::assets::HeroInventory inventory;
    inventory.equipped.push_back({1, 1, 0x001000u, 0, 0});
    inventory.equipped.push_back({2, 1, 0x002000u, 0, 0});
    inventory.equipped.push_back({3, 1, 0x400000u, 13, 0});
    inventory.equipped.push_back({4, 1, 0x800000u, 13, 0});

    gl::app::CharacterOverlay overlay;
    REQUIRE(overlay.open(nullptr, vfs.value(), make_sheet(), &inventory, &catalog.value()));

    CHECK(overlay.handle_mouse(18, 468, true));
    CHECK(overlay.take_weapon_set_toggle());
    CHECK_FALSE(overlay.take_weapon_set_toggle());

    CHECK(overlay.handle_mouse(75, 525, true));
    CHECK_FALSE(overlay.take_weapon_set_toggle());

    overlay.set_use_b_set(true);
    CHECK(overlay.handle_mouse(75, 525, true));
    CHECK(overlay.take_weapon_set_toggle());
}
```

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build
build\bin\character_overlay_tests.exe --test-case="*inactive slot*"
```
Expected: FAIL — no `take_weapon_set_toggle`/`set_use_b_set`.

- [ ] **Step 3: Implement**

In `src/app/include/gl/app/character_overlay.hpp` public section:

```cpp
    void set_use_b_set(bool use_b_set) { use_b_set_ = use_b_set; }
    bool take_weapon_set_toggle();
```

and private member:

```cpp
    bool weapon_set_toggle_requested_ = false;
```

In `src/app/src/character_overlay.cpp`, include `gl/game/weapon_set.hpp`; add helpers in the anonymous namespace:

```cpp
bool is_weapon_set_slot(std::uint32_t mask) {
    return mask == gl::game::kWeaponMaskA || mask == gl::game::kWeaponMaskB ||
           mask == gl::game::kShieldMaskA || mask == gl::game::kShieldMaskB;
}

bool mask_is_active(std::uint32_t mask, bool use_b_set) {
    if (mask == gl::game::kWeaponMaskA || mask == gl::game::kShieldMaskA)
        return !use_b_set;
    if (mask == gl::game::kWeaponMaskB || mask == gl::game::kShieldMaskB)
        return use_b_set;
    return true;
}

const PuppetSpec *hit_puppet(const float x, const float y) {
    for (const PuppetSpec &slot : kPuppetSlots) {
        if (contains(static_cast<float>(slot.x), static_cast<float>(slot.y), 55.0f, 55.0f, x, y))
            return &slot;
    }
    return nullptr;
}
```

Replace `puppet_visible` body with `return true;` (both slot areas exist). In the render slot loop, after the visible check add:

```cpp
        if (!mask_is_active(slot.mask, use_b_set_)) continue;
```

In `handle_mouse`, after computing `logical_x/logical_y` add:

```cpp
    if (const PuppetSpec *puppet = hit_puppet(logical_x, logical_y);
        puppet != nullptr && is_weapon_set_slot(puppet->mask) &&
        !mask_is_active(puppet->mask, use_b_set_)) {
        if (click) weapon_set_toggle_requested_ = true;
        return true;
    }
```

Add:

```cpp
bool CharacterOverlay::take_weapon_set_toggle() {
    const bool value = weapon_set_toggle_requested_;
    weapon_set_toggle_requested_ = false;
    return value;
}
```

- [ ] **Step 4: Run tests to verify they pass**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\character_overlay_tests.exe --test-case="*inactive slot*,*puppet*"
```
Expected: PASS (existing puppet tests still pass; inactive icons not drawn).

- [ ] **Step 5: Commit**

```powershell
git add src/app/include/gl/app/character_overlay.hpp src/app/src/character_overlay.cpp tests/app/test_character_overlay.cpp
git commit -m "feat: add weapon-set selection to the character overlay"
```

---

### Task 4: InventoryOverlay A/B slots and toggle request

**Files:**
- Modify: `src/app/include/gl/app/inventory_overlay.hpp`
- Modify: `src/app/src/inventory_overlay.cpp`
- Test: `tests/app/test_inventory_overlay.cpp`

**Interfaces:**
- Produces:
  - `open(..., bool use_b_set = false)`, `set_use_b_set(bool)`,
    `bool take_weapon_set_toggle()`.
- Both weapon/shield slot areas hit-testable; inactive icon hidden; click inactive → request.

- [ ] **Step 1: Update/write failing tests**

Replace `TEST_CASE("inventory overlay draws only the active puppet weapon and shield set")`
with:

```cpp
TEST_CASE("inventory overlay exposes both set slots and toggles from the inactive one") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;

    gl::assets::HeroInventory active;
    active.equipped.push_back(make_item(1, 0, 1, 0x001000));
    active.equipped.push_back(make_item(2, 0, 1, 0x002000));
    active.equipped.push_back(make_item(3, 13, 1, 0x400000));
    active.equipped.push_back(make_item(4, 13, 1, 0x800000));

    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, active));
    CHECK(overlay.hit_test(30, 480) == gl::app::InventoryRegion::puppet_slot);
    CHECK(overlay.hit_test(18, 468) == gl::app::InventoryRegion::puppet_slot);
    CHECK(overlay.handle_mouse(30, 480, true));
    CHECK(overlay.selection() == gl::app::InventorySelection::equipped);
    CHECK_FALSE(overlay.take_weapon_set_toggle());

    CHECK(overlay.handle_mouse(18, 468, true));
    CHECK(overlay.take_weapon_set_toggle());
    CHECK_FALSE(overlay.take_weapon_set_toggle());

    gl::app::InventoryOverlay b_overlay;
    REQUIRE(b_overlay.open(vfs, catalog, active, true));
    CHECK(b_overlay.handle_mouse(18, 468, true));
    CHECK(b_overlay.selection() == gl::app::InventorySelection::equipped);
    CHECK_FALSE(b_overlay.take_weapon_set_toggle());
    CHECK(b_overlay.handle_mouse(30, 480, true));
    CHECK_FALSE(b_overlay.take_weapon_set_toggle());
    CHECK(b_overlay.handle_mouse(75, 525, true));
    CHECK(b_overlay.take_weapon_set_toggle());
}
```

Keep a pixel test with the same fixture: default A must not draw the B icon, and
`open(..., true)` must not draw the A icon. Use the existing `SoftwareRenderer` and
`region_difference` helpers:

```cpp
TEST_CASE("inventory overlay hides the inactive set icon") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::assets::HeroInventory only_a;
    only_a.equipped.push_back(make_item(1, 0, 1, 0x001000));
    gl::assets::HeroInventory both;
    both.equipped.push_back(make_item(1, 0, 1, 0x001000));
    both.equipped.push_back(make_item(2, 0, 1, 0x002000));

    SoftwareRenderer canvas(1024, 768);
    REQUIRE(canvas.renderer != nullptr);
    gl::app::InventoryOverlay a_overlay;
    gl::app::InventoryOverlay both_overlay;
    REQUIRE(a_overlay.open(vfs, catalog, only_a));
    REQUIRE(both_overlay.open(vfs, catalog, both));
    SDL_SetRenderDrawColor(canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(canvas.renderer);
    a_overlay.render(canvas.renderer, 1024, 768);
    std::vector<std::uint8_t> a_pixels(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 a_pixels.data(), 1024 * 4) == 0);
    SDL_SetRenderDrawColor(canvas.renderer, 0, 0, 0, 255);
    SDL_RenderClear(canvas.renderer);
    both_overlay.render(canvas.renderer, 1024, 768);
    std::vector<std::uint8_t> both_pixels(1024 * 768 * 4);
    REQUIRE(SDL_RenderReadPixels(canvas.renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 both_pixels.data(), 1024 * 4) == 0);
    CHECK(region_difference(a_pixels, both_pixels, 16, 466, 61, 62) <= 4);
}
```

(Remove the old real "active weapon A" assertions that conflict with both-slot behavior;
the real slot0 test should now assert A icon region differs from B-active render.)

Also update the two existing real slot0 tests:

- `real slot0 puppet overlap selects the active weapon A`: default A, `(30,480)` still
  selects the axe (`selected_name() == "Топор Сумеречного Палача"`); `(18,468)` (weapon
  B) now returns `puppet_slot` and a click sets the toggle request instead of selecting.
- `real slot0 shield B is selectable when shield A is absent`: open with
  `use_b_set=true` to select shield 353 at `(238,493)`; with the default A open, the
  same click must set `take_weapon_set_toggle()` and not change the selection.

- [ ] **Step 2: Run tests to verify they fail**

```powershell
cmake --build build
build\bin\inventory_overlay_tests.exe --test-case="*both set slots*,*inactive set icon*"
```
Expected: FAIL — no `take_weapon_set_toggle` / `open` arity.

- [ ] **Step 3: Implement**

In `src/app/include/gl/app/inventory_overlay.hpp`:

```cpp
    bool open(const gl::assets::GameVfs &vfs, const gl::assets::ItemCatalog &catalog,
              const gl::assets::HeroInventory &inventory, bool use_b_set = false);
    void set_use_b_set(bool use_b_set) { use_b_set_ = use_b_set; }
    bool take_weapon_set_toggle();
```

private members:

```cpp
    bool use_b_set_ = false;
    bool weapon_set_toggle_requested_ = false;
```

In `src/app/src/inventory_overlay.cpp`, include `gl/game/weapon_set.hpp`; add helpers:

```cpp
bool is_weapon_set_slot(std::uint32_t mask) {
    return mask == gl::game::kWeaponMaskA || mask == gl::game::kWeaponMaskB ||
           mask == gl::game::kShieldMaskA || mask == gl::game::kShieldMaskB;
}

bool mask_is_active(std::uint32_t mask, bool use_b_set) {
    if (mask == gl::game::kWeaponMaskA || mask == gl::game::kShieldMaskA)
        return !use_b_set;
    if (mask == gl::game::kWeaponMaskB || mask == gl::game::kShieldMaskB)
        return use_b_set;
    return true;
}
```

- `open` gains `bool use_b_set` and stores `use_b_set_ = use_b_set;` and
  `weapon_set_toggle_requested_ = false;`.
- `puppet_slot_visible` returns `true` always.
- In the puppet render loop, after `if (!puppet_slot_visible(...)) continue;` add
  `if (!mask_is_active(slot.mask, use_b_set_)) continue;`.
- Add an active-first lookup (A/B rects overlap, so the active set wins):

```cpp
const PuppetSpec *hit_weapon_set_slot(const gl::assets::HeroInventory &inventory,
                                      float x, float y, bool use_b_set) {
    for (const PuppetSpec &slot : kPuppetSlots) {
        if (!is_weapon_set_slot(slot.mask) || !mask_is_active(slot.mask, use_b_set))
            continue;
        if (contains(static_cast<float>(slot.x), static_cast<float>(slot.y), 55.0f, 55.0f, x, y))
            return &slot;
    }
    for (const PuppetSpec &slot : kPuppetSlots) {
        if (!is_weapon_set_slot(slot.mask) || mask_is_active(slot.mask, use_b_set))
            continue;
        if (contains(static_cast<float>(slot.x), static_cast<float>(slot.y), 55.0f, 55.0f, x, y))
            return &slot;
    }
    return nullptr;
}
```

- In `handle_mouse` `case InventoryRegion::puppet_slot`, first use the active-first slot:

```cpp
        if (const PuppetSpec *set_slot =
                hit_weapon_set_slot(inventory_, logical_x, logical_y, use_b_set_);
            set_slot != nullptr && !mask_is_active(set_slot->mask, use_b_set_)) {
            weapon_set_toggle_requested_ = true;
            break;
        }
```

Then use `hit_weapon_set_slot(...)` (if non-null) as `puppet`, otherwise the existing
`hit_puppet_slot(..., true)`/`(..., false)` fallback, before the equipped-selection loop.

Add:

```cpp
bool InventoryOverlay::take_weapon_set_toggle() {
    const bool value = weapon_set_toggle_requested_;
    weapon_set_toggle_requested_ = false;
    return value;
}
```

- [ ] **Step 4: Run tests to verify they pass**

```powershell
cmake --build build
build\bin\inventory_overlay_tests.exe --test-case="*both set slots*,*inactive set icon*,*real slot0*"
```
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/app/include/gl/app/inventory_overlay.hpp src/app/src/inventory_overlay.cpp tests/app/test_inventory_overlay.cpp
git commit -m "feat: add weapon-set selection to the inventory overlay"
```

---

### Task 5: glgame active set, Z, clicks, CLI

**Files:**
- Modify: `tools/glgame/main.cpp`

**Interfaces:**
- Consumes: Tasks 1–4.
- Produces: `active_weapon_set_`, `toggle_weapon_set()`, `apply_active_weapon_set(WeaponSet)`, CLI `--weapon-set <a|b>`.

- [ ] **Step 1: State, toggle and reset**

Add includes `gl/game/weapon_set.hpp`. Add member:

```cpp
    gl::game::WeaponSet active_weapon_set_ = gl::game::WeaponSet::a;
```

Add `bool use_b_set() const { return active_weapon_set_ == gl::game::WeaponSet::b; }` and:

```cpp
    void apply_active_weapon_set(gl::game::WeaponSet set, bool refresh_overlays) {
        active_weapon_set_ = set;
        if (level_ && pending_equipment_) {
            std::string error;
            if (auto outfit = resolve_outfit_from_equipment(*pending_equipment_, use_b_set(), error)) {
                level_->apply_hero_outfit(*outfit);
                character_outfit_ = *outfit;
            } else {
                gl::log(gl::LogLevel::warning, "outfit: " + error);
            }
        }
        if (refresh_overlays) {
            inventory_overlay_.set_use_b_set(use_b_set());
            character_overlay_.set_use_b_set(use_b_set());
        }
        gl::log(gl::LogLevel::info,
                "weapon set: " + std::string(use_b_set() ? "B" : "A"));
    }

    void toggle_weapon_set() {
        apply_active_weapon_set(use_b_set() ? gl::game::WeaponSet::a
                                            : gl::game::WeaponSet::b,
                                true);
    }
```

- `resolve_outfit_from_equipment` gains `bool use_b_set` (after equipment, before `std::string& error`)
  and passes it to `resolve_hero_outfit(...)`.
- Update all three call sites to pass `use_b_set()`.
- Reset `active_weapon_set_ = gl::game::WeaponSet::a;` at the start of
  `apply_hero_from_body` and in `menu_activate(0)` (New Game).

- [ ] **Step 2: Z key**

In `handle_keydown`, after the console check and before overlay key blocks add:

```cpp
        if (state_.state() == gl::core::AppState::level && level_ && !dialog_ &&
            !save_slots_ && !console_visible_ &&
            (symbol == SDLK_z || symbol == static_cast<SDL_Keycode>('Z'))) {
            toggle_weapon_set();
            return;
        }
```

- [ ] **Step 3: Clicks**

In `SDL_MOUSEBUTTONDOWN`:
- character block: after `character_overlay_.handle_mouse(...)` (both left/right? only left
  currently) add: `if (character_overlay_.take_weapon_set_toggle()) toggle_weapon_set();`
- inventory block: after the existing `handle_mouse` (left) and also in the right-click
  branch before returning, check `take_weapon_set_toggle()` and call `toggle_weapon_set()`.

- [ ] **Step 4: Pass active set / CLI**

- `open_inventory`: `inventory_overlay_.open(vfs_, item_catalog_, inventory, use_b_set());`
- `open_character`: `inputs.use_b_set = use_b_set();` and pass `use_b_set()` as the last
  argument to `character_overlay_.open(...)`.
- Options: add `std::optional<int> weapon_set;`; parse `--weapon-set <a|b>` (set 0/1,
  invalid → error). In `run()` after load/equip and before `--inventory`/`--character`
  (around line 847) add:
```cpp
        if (options_.weapon_set)
            apply_active_weapon_set(*options_.weapon_set != 0 ? gl::game::WeaponSet::b
                                                              : gl::game::WeaponSet::a,
                                    true);
```
- Add usage line `"  --weapon-set <a|b>   Select the active weapon/shield set\n"`.

- [ ] **Step 5: Build and verify**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"; $env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*weapon_set*,*slot0 set B*,*resolve_hero_outfit*"
build\bin\inventory_overlay_tests.exe --test-case="*both set slots*,*inactive set icon*"
build\bin\character_overlay_tests.exe --test-case="*inactive slot*"
```
Expected: PASS.

Then produce A/B screenshots and verify they differ:

```powershell
$tmp="$env:TEMP\p5f"; New-Item -ItemType Directory -Force -Path $tmp | Out-Null
build\bin\glgame.exe --game-dir "I:\Games\Златогорье-2" --load-save 0 --inventory --screenshot "$tmp\inv_a.png"
build\bin\glgame.exe --game-dir "I:\Games\Златогорье-2" --load-save 0 --inventory --weapon-set b --screenshot "$tmp\inv_b.png"
build\bin\glgame.exe --game-dir "I:\Games\Златогорье-2" --load-save 0 --character --screenshot "$tmp\char_a.png"
build\bin\glgame.exe --game-dir "I:\Games\Златогорье-2" --load-save 0 --character --weapon-set b --screenshot "$tmp\char_b.png"
(Get-FileHash "$tmp\inv_a.png").Hash; (Get-FileHash "$tmp\inv_b.png").Hash
(Get-FileHash "$tmp\char_a.png").Hash; (Get-FileHash "$tmp\char_b.png").Hash
```
Expected: A/B hashes differ for both inventory and character.

- [ ] **Step 6: Commit**

```powershell
git add tools/glgame/main.cpp
git commit -m "feat: wire weapon-set toggling into glgame"
```

---

### Task 6: Docs and full ctest

**Files:**
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`
- Modify: `docs/DECISIONS.md`
- Already created: `docs/research/p5f-weapon-sets.md`,
  `docs/specs/2026-11-27-p5f-weapon-sets-design.md`,
  `docs/plans/2026-11-27-p5f-weapon-sets.md`

- [ ] **Step 1: Add D-117 after D-116**

```
| D-117 | 2026-11-27 | P5f weapon sets A/B: `active_weapon_set` (default A, not saved, reset on load/new game); Z (`vk_weapon_toggle`) and click on the inactive weapon/shield slot in inventory/character overlays; combat stats/outfit use only the active set (no fallback); both slot areas hit-testable, inactive icon hidden; port UX frames active pair. | Original: `local_player+0x6E4`, `FUN_120c7008`, `DIK_Z`; slot0 A armor=15, B armor=35. Saving the set is out of scope. |
```

- [ ] **Step 2: STATUS**

Add `## P5f — Weapon/shield sets A/B (done 2026-11-27)` with:
goal, spec/research/plan, D-117, oracles, verification commands/output,
status done after green full `ctest`, manual gate open (Z + click).

- [ ] **Step 3: PROJECT P5 row**

Append: `P5f weapon sets A/B done 2026-11-27 (D-117: Z/inactive-slot toggle, stats/outfit/sheet/inventory).`

- [ ] **Step 4: Full ctest**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"; $env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: 21/21 passed; use a long timeout (≥3,000,000 ms). Record time.

- [ ] **Step 5: Docs encoding**

```powershell
$files=@('docs/STATUS.md','docs/PROJECT.md','docs/DECISIONS.md','docs/research/p5f-weapon-sets.md','docs/specs/2026-11-27-p5f-weapon-sets-design.md','docs/plans/2026-11-27-p5f-weapon-sets.md'); foreach($f in $files){ $b=[IO.File]::ReadAllBytes((Join-Path 'I:\goldenland2dec' $f)); $nul=($b|Where-Object{$_ -eq 0}).Count; "$f nul=$nul" }
```
Expected `nul=0`.

- [ ] **Step 6: Commit docs**

```powershell
git add docs/STATUS.md docs/PROJECT.md docs/DECISIONS.md docs/research/p5f-weapon-sets.md docs/specs/2026-11-27-p5f-weapon-sets-design.md docs/plans/2026-11-27-p5f-weapon-sets.md
git commit -m "docs: p5f weapon sets handoff and full ctest"
```

---

## Self-Review Notes

- Spec §3 → Task 1/2; §4.1 → Task 1; §4.2 → Task 2; §4.3 → Task 3;
  §4.4 → Task 4; §4.5 → Task 5; §7 tests → Tasks 1–5; §8 → Task 6.
- `weapon_set_masks` defined once; overlays use the same mask constants.
- Slot0 A armor=15 preserved by no-fallback selection; B=35 pinned.
- Both overlay slot areas hit-testable, inactive icons hidden, toggle request API shared.
