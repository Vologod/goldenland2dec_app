# P5e Potions/Food Use — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Right-click on a potion/food in the inventory applies the original `Server.dll FUN_14057cf0` rule: direct permanent stats, timed effects into the P5 channel, HP/EP healing, consume one; changes survive save/load.

**Architecture:** SDL-free `gl/game/item_use` core; `PersonRuntime` gains raw setter + effect append; `InventoryRuntime` gains exact-slot consumption; `HeroSaveBody` stat block is written from `HeroStats`; `InventoryOverlay` exposes a right-click use request; glgame wires the request and syncs pending state.

**Tech Stack:** C++20, CMake+Ninja, doctest, existing `gl_assets`/`gl_game`/`gl_app`.

## Global Constraints

- C++20; `gl/game` SDL-free; SDL only in `app/render/ui/tools`; PascalCase types, snake_case functions; no comments.
- Effect `cond` packing (P5): low word b0=hours,b1=minutes; high word b2=hours,b3=minutes.
- Potion/food rule: `type ∈ {19 potion, 20 food}`; direct stat iff `(flags & 0x41) == 0x41` and id ∈ `{6,7,8,9,10,11,12,28}`; otherwise append `PersonEffect{type,value,flags,word3,-1}` with `word3 = dur_hi | dur_lo<<8 | delay_hi<<16 | delay_lo<<24`.
- Direct stats: `6 str, 7 con, 8 per, 9 dex, 10 int, 11 wis, 12 luck` clamp `1..30`; `28 reputation` clamp `-50..50`.
- Healing: `HP=min(max_life,cur+healing_health)`, `EP=min(max_energy,cur+healing_mana)`; consume exactly one from the chosen slot, erase at 0.
- Trigger: right-click on bottom/hot inventory slot (original code `0x4b`); left-click target-select, books/scrolls/recipes, NPC use, `scripts/useitem` are out of scope.
- No client 2-second `gv_item_using_timeout` queue: apply immediately (D-116).
- Toolchain:
  `$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"; $env:GL_GAME_DIR="I:\Games\Златогорье-2"`
- Close `glgame.exe` if the linker reports a locked file.
- Commit only the files named in each task; docs are committed in Task 8.

---

### Task 1: PersonRuntime raw setter and effect append

**Files:**
- Modify: `src/game/include/gl/game/person_runtime.hpp`
- Modify: `src/game/src/person_runtime.cpp`
- Test: `tests/game/test_item_use.cpp` (new)
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `void PersonRuntime::add_effect(std::string_view person, const PersonEffect &effect);`
  - `void PersonRuntime::set_raw_parameter(std::string_view person, gl::script::PersonParameter parameter, double value);`

- [ ] **Step 1: Write the failing test**

Create `tests/game/test_item_use.cpp`:

```cpp
#include <doctest/doctest.h>

#include "gl/game/person_runtime.hpp"

TEST_CASE("PersonRuntime appends effects and writes raw parameters") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);

    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x40, 0, -1};
    runtime.add_effect("Hero", effect);
    REQUIRE(runtime.effects("Hero").size() == 1);
    CHECK(runtime.effects("Hero")[0].words[0] == 9);
    CHECK(runtime.effects("Hero")[0].words[1] == -3);

    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::reputation, 40.0);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::reputation) ==
          doctest::Approx(40.0));

    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::strength, 12.0);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::strength) ==
          doctest::Approx(12.0));

    runtime.add_effect("missing", effect);
    runtime.set_raw_parameter("missing", gl::script::PersonParameter::strength, 5.0);
}
```

Modify `tests/game/CMakeLists.txt`: add `test_item_use.cpp` after `test_inventory_view.cpp`.

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*raw parameters*"
```
Expected: FAIL — `add_effect`/`set_raw_parameter` not declared.

- [ ] **Step 3: Implement**

In `src/game/include/gl/game/person_runtime.hpp`, add public methods:

```cpp
    void add_effect(std::string_view person, const PersonEffect &effect);
    void set_raw_parameter(std::string_view person, gl::script::PersonParameter parameter,
                           double value);
```

In `src/game/src/person_runtime.cpp`, add:

```cpp
void PersonRuntime::add_effect(std::string_view person, const PersonEffect &effect) {
    PersonState *state = find(person);
    if (state != nullptr)
        state->effects.push_back(effect);
}

void PersonRuntime::set_raw_parameter(std::string_view person, PersonParameter parameter,
                                      double value) {
    PersonState *state = find(person);
    if (state == nullptr)
        return;
    if (parameter == PersonParameter::unknown || parameter == PersonParameter::maximum_life)
        return;
    const std::size_t index = value_index(parameter);
    if (index >= state->values.size())
        return;
    state->present[index] = true;
    state->values[index] = value;
}
```

- [ ] **Step 4: Run test to verify it passes**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*raw parameters*"
```
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/game/include/gl/game/person_runtime.hpp src/game/src/person_runtime.cpp tests/game/test_item_use.cpp tests/game/CMakeLists.txt
git commit -m "feat: add raw person parameter setter and effect append"
```

---

### Task 2: InventoryRuntime exact-slot consumption

**Files:**
- Modify: `src/game/include/gl/game/inventory_runtime.hpp`
- Modify: `src/game/src/inventory_runtime.cpp`
- Test: `tests/game/test_item_use.cpp`

**Interfaces:**
- Produces:
  - `enum class gl::game::HeroItemContainer { main, hot };`
  - `bool InventoryRuntime::has_hero_item(HeroItemContainer container, std::size_t index) const;`
  - `bool InventoryRuntime::consume_hero_item(HeroItemContainer container, std::size_t index);`

- [ ] **Step 1: Write the failing test**

Append to `tests/game/test_item_use.cpp`:

```cpp
#include "gl/game/inventory_runtime.hpp"

TEST_CASE("InventoryRuntime consumes one item from the selected hero slot") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back(gl::assets::InventoryItem(953, 3, 0, 19, 0));
    inventory.hot.push_back(gl::assets::InventoryItem(948, 1, 0, 19, 0));
    auto runtime = gl::game::InventoryRuntime::create(inventory, {}, {});

    CHECK(runtime.has_hero_item(gl::game::HeroItemContainer::main, 0));
    CHECK(runtime.has_hero_item(gl::game::HeroItemContainer::hot, 0));
    CHECK_FALSE(runtime.has_hero_item(gl::game::HeroItemContainer::main, 5));

    CHECK(runtime.consume_hero_item(gl::game::HeroItemContainer::main, 0));
    REQUIRE(runtime.hero_inventory().items.size() == 1);
    CHECK(runtime.hero_inventory().items[0].count == 2);

    CHECK(runtime.consume_hero_item(gl::game::HeroItemContainer::hot, 0));
    CHECK(runtime.hero_inventory().hot.empty());
    CHECK_FALSE(runtime.consume_hero_item(gl::game::HeroItemContainer::hot, 0));
}
```

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*selected hero slot*"
```
Expected: FAIL — no `HeroItemContainer`/methods.

- [ ] **Step 3: Implement**

In `src/game/include/gl/game/inventory_runtime.hpp`, add near the top of `namespace gl::game`:

```cpp
enum class HeroItemContainer { main, hot };
```

and inside `class InventoryRuntime` public section:

```cpp
    bool has_hero_item(HeroItemContainer container, std::size_t index) const;
    bool consume_hero_item(HeroItemContainer container, std::size_t index);
```

In `src/game/src/inventory_runtime.cpp`, add:

```cpp
bool InventoryRuntime::has_hero_item(HeroItemContainer container, std::size_t index) const {
    const auto &items =
        container == HeroItemContainer::main ? hero_inventory_.items : hero_inventory_.hot;
    return index < items.size();
}

bool InventoryRuntime::consume_hero_item(HeroItemContainer container, std::size_t index) {
    auto &items = container == HeroItemContainer::main ? hero_inventory_.items : hero_inventory_.hot;
    if (index >= items.size())
        return false;
    if (items[index].count > 1) {
        --items[index].count;
        return true;
    }
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}
```

- [ ] **Step 4: Run test to verify it passes**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*selected hero slot*"
```
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/game/include/gl/game/inventory_runtime.hpp src/game/src/inventory_runtime.cpp tests/game/test_item_use.cpp
git commit -m "feat: add hero item consumption to inventory runtime"
```

---

### Task 3: item_use core (rule §3)

**Files:**
- Create: `src/game/include/gl/game/item_use.hpp`
- Create: `src/game/src/item_use.cpp`
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_item_use.cpp`

**Interfaces:**
- Consumes: Task 1 `add_effect`/`set_raw_parameter`; Task 2 `HeroItemContainer`/`has_hero_item`/`consume_hero_item`.
- Produces:
  - `bool gl::game::is_consumable(gl::assets::ItemType type);`
  - `gl::game::HeroUseResult gl::game::use_hero_item(const gl::assets::Item &item, HeroItemContainer container, std::size_t index, gl::assets::HeroStats &stats, PersonRuntime &runtime, InventoryRuntime &inventory);`

- [ ] **Step 1: Write the failing tests**

Append to `tests/game/test_item_use.cpp`:

```cpp
#include "gl/game/item_use.hpp"

namespace {
gl::assets::ItemEffect make_effect(std::uint32_t type, std::int32_t value, std::uint32_t flags,
                                   std::uint8_t dur_hi = 0, std::uint8_t dur_lo = 0,
                                   std::uint8_t delay_hi = 0, std::uint8_t delay_lo = 0) {
    gl::assets::ItemEffect effect;
    effect.type = type;
    effect.amount = static_cast<std::uint32_t>(value);
    effect.flags = flags;
    effect.duration.hi = dur_hi;
    effect.duration.lo = dur_lo;
    effect.delay.hi = delay_hi;
    effect.delay.lo = delay_lo;
    return effect;
}

gl::assets::Item make_item(gl::assets::ItemType type, std::uint32_t id,
                           std::vector<gl::assets::ItemEffect> effects = {},
                           std::uint32_t heal_hp = 0, std::uint32_t heal_ep = 0) {
    gl::assets::Item item;
    item.type = type;
    item.description_id = id;
    item.effects = std::move(effects);
    item.healing_health = heal_hp;
    item.healing_mana = heal_ep;
    return item;
}

gl::assets::InventoryItem inventory_item(std::uint32_t id, std::uint32_t type,
                                         std::uint32_t count) {
    return gl::assets::InventoryItem(id, count, 0, type, 0);
}

gl::game::HeroUseResult use_single(const gl::assets::Item &item, gl::assets::HeroStats &stats,
                                   gl::game::PersonRuntime &runtime,
                                   gl::game::InventoryRuntime &inventory) {
    return gl::game::use_hero_item(item, gl::game::HeroItemContainer::main, 0, stats, runtime,
                                   inventory);
}
} // namespace

TEST_CASE("is_consumable accepts potions and food only") {
    CHECK(gl::game::is_consumable(gl::assets::ItemType::potion));
    CHECK(gl::game::is_consumable(gl::assets::ItemType::food));
    CHECK_FALSE(gl::game::is_consumable(gl::assets::ItemType::sword));
    CHECK_FALSE(gl::game::is_consumable(gl::assets::ItemType::scroll));
}

TEST_CASE("use_hero_item appends timed effects with packed cond") {
    gl::assets::Item item = make_item(gl::assets::ItemType::food, 408,
                                      {make_effect(30, 100, 0x50, 3, 0)});
    gl::assets::HeroStats stats;
    gl::assets::HeroInventory hero;
    hero.items.push_back(inventory_item(408, 20, 1));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);

    const auto result = use_single(item, stats, runtime, inventory);
    CHECK(result.used);
    CHECK(result.consumed);
    CHECK(result.effects_added == 1);
    REQUIRE(runtime.effects("Hero").size() == 1);
    CHECK(runtime.effects("Hero")[0].words[0] == 30);
    CHECK(runtime.effects("Hero")[0].words[1] == 100);
    CHECK(static_cast<std::uint32_t>(runtime.effects("Hero")[0].words[2]) == 0x50);
    CHECK(static_cast<std::uint32_t>(runtime.effects("Hero")[0].words[3]) == 0x00000003);
    CHECK(runtime.effects("Hero")[0].words[4] == -1);
    CHECK(inventory.hero_inventory().items.empty());
}

TEST_CASE("use_hero_item applies direct stats with clamps") {
    gl::assets::Item item = make_item(gl::assets::ItemType::potion, 948,
                                      {make_effect(6, 1, 0x41), make_effect(10, -1, 0x41)});
    gl::assets::HeroStats stats;
    stats.strength = 10;
    stats.intelligence = 10;
    gl::assets::HeroInventory hero;
    hero.items.push_back(inventory_item(948, 19, 1));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);

    const auto result = use_single(item, stats, runtime, inventory);
    CHECK(result.used);
    CHECK(result.effects_added == 0);
    CHECK(stats.strength == 11);
    CHECK(stats.intelligence == 9);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::strength) ==
          doctest::Approx(11.0));
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::intelligence) ==
          doctest::Approx(9.0));

    stats.strength = 30;
    gl::assets::HeroInventory hero2;
    hero2.items.push_back(inventory_item(948, 19, 1));
    auto inventory2 = gl::game::InventoryRuntime::create(hero2, {}, {});
    auto runtime2 = gl::game::PersonRuntime::create({}, stats);
    use_single(item, stats, runtime2, inventory2);
    CHECK(stats.strength == 30);

    stats.strength = 1;
    gl::assets::HeroInventory hero3;
    hero3.items.push_back(inventory_item(948, 19, 1));
    auto inventory3 = gl::game::InventoryRuntime::create(hero3, {}, {});
    auto runtime3 = gl::game::PersonRuntime::create({}, stats);
    use_single(make_item(gl::assets::ItemType::potion, 948, {make_effect(6, -1, 0x41)}), stats,
               runtime3, inventory3);
    CHECK(stats.strength == 1);
}

TEST_CASE("use_hero_item clamps reputation") {
    gl::assets::Item item = make_item(gl::assets::ItemType::food, 410,
                                      {make_effect(28, -1, 0x41)});
    gl::assets::HeroStats stats;
    stats.reputation = -50;
    gl::assets::HeroInventory hero;
    hero.items.push_back(inventory_item(410, 20, 1));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);
    use_single(item, stats, runtime, inventory);
    CHECK(stats.reputation == -50);
}

TEST_CASE("use_hero_item heals HP and EP with clamp") {
    gl::assets::Item item = make_item(gl::assets::ItemType::potion, 957, {}, 20, 20);
    gl::assets::HeroStats stats;
    stats.life = 10;
    stats.energy = 10;
    stats.experience = 1000;
    gl::assets::HeroInventory hero;
    hero.items.push_back(inventory_item(957, 19, 1));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);
    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::life, 10.0);
    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::energy, 10.0);
    const double max_life =
        runtime.get_parameter("Hero", gl::script::PersonParameter::maximum_life);
    const double max_energy = runtime.maximum_energy("Hero");

    const auto result = use_single(item, stats, runtime, inventory);
    CHECK(result.used);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::life) ==
          doctest::Approx(std::min(max_life, 30.0)));
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::energy) ==
          doctest::Approx(std::min(max_energy, 30.0)));
    CHECK(stats.life == static_cast<std::int32_t>(std::min(max_life, 30.0)));
    CHECK(stats.energy == static_cast<std::int32_t>(std::min(max_energy, 30.0)));
}

TEST_CASE("use_hero_item rejects non-consumables and bad slots") {
    gl::assets::HeroStats stats;
    auto runtime = gl::game::PersonRuntime::create({}, stats);
    gl::assets::HeroInventory hero;
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});

    const auto sword = use_single(make_item(gl::assets::ItemType::sword, 1), stats, runtime,
                                  inventory);
    CHECK_FALSE(sword.used);
    CHECK_FALSE(sword.consumed);

    const auto potion = use_single(make_item(gl::assets::ItemType::potion, 953), stats, runtime,
                                   inventory);
    CHECK_FALSE(potion.used);
}
```

Add `#include "gl/assets/hero_stats.hpp"`, `<algorithm>`, `<cstdint>`, `<utility>`, `<vector>` to `tests/game/test_item_use.cpp` (at the top).

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*use_hero_item*"
```
Expected: FAIL — `gl/game/item_use.hpp` not found.

- [ ] **Step 3: Implement**

Create `src/game/include/gl/game/item_use.hpp`:

```cpp
#pragma once
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/itm.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/person_runtime.hpp"

#include <cstddef>

namespace gl::game {
bool is_consumable(gl::assets::ItemType type);

struct HeroUseResult {
    bool used = false;
    bool consumed = false;
    int effects_added = 0;
};

HeroUseResult use_hero_item(const gl::assets::Item &item, HeroItemContainer container,
                            std::size_t index, gl::assets::HeroStats &stats,
                            PersonRuntime &runtime, InventoryRuntime &inventory);
} // namespace gl::game
```

Create `src/game/src/item_use.cpp`:

```cpp
#include "gl/game/item_use.hpp"

#include <algorithm>
#include <optional>

namespace gl::game {
namespace {
constexpr std::uint32_t kDirectFlag = 0x41;
constexpr std::int32_t kPrimaryMin = 1;
constexpr std::int32_t kPrimaryMax = 30;
constexpr std::int32_t kReputationMin = -50;
constexpr std::int32_t kReputationMax = 50;

std::optional<gl::script::PersonParameter> direct_parameter(std::uint32_t id) {
    using P = gl::script::PersonParameter;
    switch (id) {
    case 6: return P::strength;
    case 7: return P::constitution;
    case 8: return P::perception;
    case 9: return P::dexterity;
    case 10: return P::intelligence;
    case 11: return P::wisdom;
    case 12: return P::luck;
    case 28: return P::reputation;
    default: return std::nullopt;
    }
}

std::int32_t &primary_field(gl::assets::HeroStats &stats, gl::script::PersonParameter parameter) {
    switch (parameter) {
    case gl::script::PersonParameter::strength: return stats.strength;
    case gl::script::PersonParameter::constitution: return stats.constitution;
    case gl::script::PersonParameter::perception: return stats.perception;
    case gl::script::PersonParameter::dexterity: return stats.dexterity;
    case gl::script::PersonParameter::intelligence: return stats.intelligence;
    case gl::script::PersonParameter::wisdom: return stats.wisdom;
    case gl::script::PersonParameter::luck: return stats.luck;
    default: return stats.reputation;
    }
}

void apply_direct(gl::assets::HeroStats &stats, PersonRuntime &runtime,
                  gl::script::PersonParameter parameter, std::int32_t value) {
    std::int32_t &field = primary_field(stats, parameter);
    std::int32_t next = field + value;
    if (parameter == gl::script::PersonParameter::reputation)
        next = std::clamp(next, kReputationMin, kReputationMax);
    else
        next = std::clamp(next, kPrimaryMin, kPrimaryMax);
    field = next;
    runtime.set_raw_parameter("Hero", parameter, static_cast<double>(next));
}

void apply_heal(PersonRuntime &runtime, gl::assets::HeroStats &stats,
                gl::script::PersonParameter parameter, std::uint32_t amount, double maximum) {
    double next = runtime.get_parameter("Hero", parameter) + static_cast<double>(amount);
    if (next > maximum)
        next = maximum;
    if (next < 0.0)
        next = 0.0;
    runtime.set_raw_parameter("Hero", parameter, next);
    if (parameter == gl::script::PersonParameter::life)
        stats.life = static_cast<std::int32_t>(next);
    else
        stats.energy = static_cast<std::int32_t>(next);
}

std::uint32_t pack_word3(const gl::assets::ItemEffect &effect) {
    return static_cast<std::uint32_t>(effect.duration.hi) |
           (static_cast<std::uint32_t>(effect.duration.lo) << 8) |
           (static_cast<std::uint32_t>(effect.delay.hi) << 16) |
           (static_cast<std::uint32_t>(effect.delay.lo) << 24);
}
} // namespace

bool is_consumable(gl::assets::ItemType type) {
    return type == gl::assets::ItemType::potion || type == gl::assets::ItemType::food;
}

HeroUseResult use_hero_item(const gl::assets::Item &item, HeroItemContainer container,
                            std::size_t index, gl::assets::HeroStats &stats,
                            PersonRuntime &runtime, InventoryRuntime &inventory) {
    HeroUseResult result;
    if (!is_consumable(item.type) || !inventory.has_hero_item(container, index))
        return result;
    for (const gl::assets::ItemEffect &effect : item.effects) {
        const auto value = static_cast<std::int32_t>(effect.amount);
        if ((effect.flags & kDirectFlag) == kDirectFlag) {
            if (const auto parameter = direct_parameter(effect.type)) {
                apply_direct(stats, runtime, *parameter, value);
                continue;
            }
        }
        PersonEffect record;
        record.words[0] = static_cast<std::int32_t>(effect.type);
        record.words[1] = value;
        record.words[2] = static_cast<std::int32_t>(effect.flags);
        record.words[3] = static_cast<std::int32_t>(pack_word3(effect));
        record.words[4] = -1;
        runtime.add_effect("Hero", record);
        ++result.effects_added;
    }
    const double max_life =
        runtime.get_parameter("Hero", gl::script::PersonParameter::maximum_life);
    apply_heal(runtime, stats, gl::script::PersonParameter::life, item.healing_health, max_life);
    apply_heal(runtime, stats, gl::script::PersonParameter::energy, item.healing_mana,
               runtime.maximum_energy("Hero"));
    result.consumed = inventory.consume_hero_item(container, index);
    result.used = result.consumed;
    return result;
}
} // namespace gl::game
```

Modify `src/game/CMakeLists.txt`: add `src/item_use.cpp` after `src/inventory_view.cpp`.

- [ ] **Step 4: Run tests to verify they pass**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*use_hero_item*,*consumable*"
```
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/game/include/gl/game/item_use.hpp src/game/src/item_use.cpp src/game/CMakeLists.txt tests/game/test_item_use.cpp
git commit -m "feat: apply potions and food to the hero"
```

---

### Task 4: Persist hero stat block in saves

**Files:**
- Modify: `src/game/include/gl/game/hero_save_write.hpp`
- Modify: `src/game/src/hero_save_write.cpp`
- Test: `tests/game/test_item_use.cpp`

**Interfaces:**
- Produces: `HeroLiveState::stats` (`std::optional<gl::assets::HeroStats>`); `make_hero_save_body` writes 9 stats at `stat_block+0x00..0x20` and 27 skills at `+0x2C+4*id`.

- [ ] **Step 1: Write the failing test**

Append to `tests/game/test_item_use.cpp`:

```cpp
#include "gl/game/hero_save_write.hpp"

TEST_CASE("make_hero_save_body writes stats and preserves the rest of the block") {
    gl::assets::HeroSaveBody body;
    body.stat_block.fill(0xAA);
    gl::game::HeroLiveState live;
    gl::assets::HeroStats stats;
    stats.strength = 11;
    stats.constitution = 12;
    stats.dexterity = 13;
    stats.perception = 14;
    stats.intelligence = 15;
    stats.wisdom = 16;
    stats.luck = 17;
    stats.reputation = 18;
    stats.experience = 12345;
    stats.skills[0] = 5;
    stats.skills[26] = 9;
    live.stats = stats;

    const auto result = gl::game::make_hero_save_body(body, live);
    const auto read = [&](std::size_t offset) {
        return static_cast<std::uint32_t>(result.stat_block[offset]) |
               (static_cast<std::uint32_t>(result.stat_block[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(result.stat_block[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(result.stat_block[offset + 3]) << 24);
    };
    CHECK(read(0) == 11);
    CHECK(read(4) == 12);
    CHECK(read(8) == 13);
    CHECK(read(0x0C) == 14);
    CHECK(read(0x10) == 15);
    CHECK(read(0x14) == 16);
    CHECK(read(0x18) == 17);
    CHECK(read(0x1C) == 18);
    CHECK(read(0x20) == 12345);
    CHECK(read(0x2C) == 5);
    CHECK(read(0x2C + 4 * 26) == 9);
    CHECK(result.stat_block[0x100] == 0xAA);
}
```

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*writes stats*"
```
Expected: FAIL — no `HeroLiveState::stats`.

- [ ] **Step 3: Implement**

In `src/game/include/gl/game/hero_save_write.hpp`, add `#include "gl/assets/hero_stats.hpp"` and `<optional>`, then inside `struct HeroLiveState`:

```cpp
    std::optional<gl::assets::HeroStats> stats;
```

In `src/game/src/hero_save_write.cpp`, add at the top of the anonymous namespace (or file namespace):

```cpp
namespace {
void write_u32(std::array<std::uint8_t, 0x11C> &block, std::size_t offset,
               std::uint32_t value) {
    block[offset] = static_cast<std::uint8_t>(value & 0xFF);
    block[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    block[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    block[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}
} // namespace
```

At the end of `make_hero_save_body`, before `return body;`:

```cpp
    if (live.stats) {
        const gl::assets::HeroStats &stats = *live.stats;
        const std::array<std::int32_t, 9> primary{
            stats.strength,     stats.constitution, stats.dexterity, stats.perception,
            stats.intelligence, stats.wisdom,       stats.luck,      stats.reputation,
            stats.experience};
        for (std::size_t index = 0; index < primary.size(); ++index)
            write_u32(body.stat_block, 4 * index,
                      static_cast<std::uint32_t>(primary[index]));
        for (std::size_t id = 0; id < stats.skills.size(); ++id)
            write_u32(body.stat_block, 0x2C + 4 * id,
                      static_cast<std::uint32_t>(stats.skills[id]));
    }
```

Add `#include <array>` if missing.

- [ ] **Step 4: Run test to verify it passes**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*writes stats*"
```
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/game/include/gl/game/hero_save_write.hpp src/game/src/hero_save_write.cpp tests/game/test_item_use.cpp
git commit -m "feat: persist hero stat block changes in saves"
```

---

### Task 5: Real potion/food oracle

**Files:**
- Create: `tests/game/test_item_use_real.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 3 `use_hero_item`; real `items/data/{953,948,408}.itm`; real slot0 inventory.

- [ ] **Step 1: Write the failing tests**

Create `tests/game/test_item_use_real.cpp`:

```cpp
#include <doctest/doctest.h>

#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/itm.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/item_use.hpp"
#include "gl/game/person_runtime.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::vector<std::uint8_t> read_save(int slot) {
    const std::filesystem::path path =
        std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
        ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

gl::assets::Item read_item(gl::assets::GameVfs &vfs, const std::string &path) {
    auto bytes = vfs.read(path);
    REQUIRE(bytes.has_value());
    auto item = gl::assets::parse_item(std::span<const std::uint8_t>(bytes->data(), bytes->size()));
    REQUIRE(item.has_value());
    return item.value();
}
} // namespace

TEST_CASE("real 953 potion heals HP and consumes one" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const auto item = read_item(vfs.value(), "items/data/953.itm");
    CHECK(item.type == gl::assets::ItemType::potion);
    CHECK(item.healing_health == 100);

    auto body = gl::assets::decode_save_body(read_save(0));
    REQUIRE(body.has_value());
    auto stats = gl::assets::parse_hero_stats(body.value());
    REQUIRE(stats.has_value());
    auto inventory_data = gl::assets::parse_hero_inventory(body.value());
    REQUIRE(inventory_data.has_value());
    std::size_t index = inventory_data->items.size();
    for (std::size_t i = 0; i < inventory_data->items.size(); ++i) {
        if (inventory_data->items[i].item_id == 953) {
            index = i;
            break;
        }
    }
    REQUIRE(index < inventory_data->items.size());
    const std::uint32_t before = inventory_data->items[index].count;

    auto inventory = gl::game::InventoryRuntime::create(inventory_data.value(), {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats.value());
    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::life, 10.0);
    stats->life = 10;

    const auto result = gl::game::use_hero_item(item, gl::game::HeroItemContainer::main, index,
                                                stats.value(), runtime, inventory);
    CHECK(result.used);
    CHECK(result.consumed);
    const double max_life =
        runtime.get_parameter("Hero", gl::script::PersonParameter::maximum_life);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::life) ==
          doctest::Approx(std::min(max_life, 110.0)));
    REQUIRE(inventory.hero_inventory().items.size() == inventory_data->items.size());
    CHECK(inventory.hero_inventory().items[index].item_id == 953);
    CHECK(inventory.hero_inventory().items[index].count == before - 1);
}

TEST_CASE("real 948 potion applies direct strength and intelligence" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const auto item = read_item(vfs.value(), "items/data/948.itm");
    REQUIRE(item.type == gl::assets::ItemType::potion);

    gl::assets::HeroStats stats;
    stats.strength = 10;
    stats.intelligence = 10;
    gl::assets::HeroInventory hero;
    hero.items.push_back(gl::assets::InventoryItem(948, 1, 0, 19, 0));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);

    const auto result = gl::game::use_hero_item(item, gl::game::HeroItemContainer::main, 0, stats,
                                                runtime, inventory);
    CHECK(result.used);
    CHECK(result.effects_added == 0);
    CHECK(stats.strength == 11);
    CHECK(stats.intelligence == 9);
}

TEST_CASE("real 408 food appends a timed effect" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const auto item = read_item(vfs.value(), "items/data/408.itm");
    REQUIRE(item.type == gl::assets::ItemType::food);

    gl::assets::HeroStats stats;
    gl::assets::HeroInventory hero;
    hero.items.push_back(gl::assets::InventoryItem(408, 1, 0, 20, 0));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);

    const auto result = gl::game::use_hero_item(item, gl::game::HeroItemContainer::main, 0, stats,
                                                runtime, inventory);
    CHECK(result.used);
    CHECK(result.effects_added == 1);
    REQUIRE(runtime.effects("Hero").size() == 1);
    CHECK(runtime.effects("Hero")[0].words[0] == 30);
    CHECK(runtime.effects("Hero")[0].words[1] == 100);
    CHECK(static_cast<std::uint32_t>(runtime.effects("Hero")[0].words[3]) == 0x00000003);
}
```

Add `<algorithm>` for `std::min`.

Modify `tests/game/CMakeLists.txt`: add `test_item_use_real.cpp` after `test_item_use.cpp`.

- [ ] **Step 2: Run tests to verify they fail**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*real 953*,*real 948*,*real 408*"
```
Expected: FAIL until Task 3 is present; with Task 3 they must pass.

- [ ] **Step 3: Run tests to verify they pass**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*real 953*,*real 948*,*real 408*"
```
Expected: PASS (3 cases).

- [ ] **Step 4: Commit**

```powershell
git add tests/game/test_item_use_real.cpp tests/game/CMakeLists.txt
git commit -m "test: add real potion/food use oracle"
```

---

### Task 6: Inventory overlay right-click use request

**Files:**
- Modify: `src/app/include/gl/app/inventory_overlay.hpp`
- Modify: `src/app/src/inventory_overlay.cpp`
- Test: `tests/app/test_inventory_overlay.cpp`

**Interfaces:**
- Produces:
  - `struct gl::app::InventoryUseRequest { InventorySelection source; std::size_t index; };`
  - `std::optional<InventoryUseRequest> InventoryOverlay::use_request_at(int x, int y) const;`
  Returns `item`/`hot` + record index for bottom/hot slots; `nullopt` otherwise.

- [ ] **Step 1: Write the failing test**

Append to `tests/app/test_inventory_overlay.cpp`:

```cpp
TEST_CASE("inventory overlay exposes a right-click use request for bottom slots") {
    gl::assets::GameVfs vfs;
    gl::assets::ItemCatalog catalog;
    gl::app::InventoryOverlay overlay;
    REQUIRE(overlay.open(vfs, catalog, make_inventory()));

    auto request = overlay.use_request_at(167, 730);
    REQUIRE(request.has_value());
    CHECK(request->source == gl::app::InventorySelection::item);
    CHECK(request->index == 0);
    CHECK_FALSE(overlay.use_request_at(10, 10).has_value());
}
```

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build
build\bin\inventory_overlay_tests.exe --test-case="*right-click use request*"
```
Expected: FAIL — `use_request_at` not declared.

- [ ] **Step 3: Implement**

In `src/app/include/gl/app/inventory_overlay.hpp`, add near `InventorySelection`:

```cpp
struct InventoryUseRequest {
    InventorySelection source = InventorySelection::none;
    std::size_t index = 0;
};
```

and in the public section:

```cpp
    std::optional<InventoryUseRequest> use_request_at(int x, int y) const;
```

Add `#include <optional>`.

In `src/app/src/inventory_overlay.cpp`, add:

```cpp
std::optional<InventoryUseRequest> InventoryOverlay::use_request_at(int x, int y) const {
    if (!visible_ || scale_ <= 0.0f)
        return std::nullopt;
    const InventoryRegion region = hit_test(x, y);
    const float logical_x = (static_cast<float>(x) - offset_x_) / scale_;
    const float logical_y = (static_cast<float>(y) - offset_y_) / scale_;
    if (region == InventoryRegion::bottom_slot) {
        for (std::size_t slot = 0; slot < kBottomX.size(); ++slot) {
            if (!contains(static_cast<float>(kBottomX[slot]), static_cast<float>(kBottomY), 55.0f,
                          55.0f, logical_x, logical_y))
                continue;
            const std::size_t visible = view_.page + slot;
            if (visible >= visible_indices_.size())
                return std::nullopt;
            return InventoryUseRequest{InventorySelection::item, visible_indices_[visible]};
        }
        return std::nullopt;
    }
    if (region == InventoryRegion::hot_slot) {
        for (std::size_t slot = 1; slot < kHotX.size(); ++slot) {
            if (!contains(static_cast<float>(kHotX[slot]), static_cast<float>(kHotY), 27.0f, 27.0f,
                          logical_x, logical_y))
                continue;
            for (std::size_t index = 0; index < inventory_.hot.size(); ++index) {
                if (((inventory_.hot[index].flags & 0x0000F000u) >> 12) == slot)
                    return InventoryUseRequest{InventorySelection::hot, index};
            }
            return std::nullopt;
        }
        return std::nullopt;
    }
    return std::nullopt;
}
```

- [ ] **Step 4: Run test to verify it passes**

```powershell
cmake --build build
build\bin\inventory_overlay_tests.exe --test-case="*right-click use request*"
```
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/app/include/gl/app/inventory_overlay.hpp src/app/src/inventory_overlay.cpp tests/app/test_inventory_overlay.cpp
git commit -m "feat: expose right-click use requests in the inventory overlay"
```

---

### Task 7: glgame wiring

**Files:**
- Modify: `tools/glgame/main.cpp`

**Interfaces:**
- Consumes: Tasks 3, 4, 6.

- [ ] **Step 1: Add the use helper**

In `class GlGameApp` (near `open_inventory`), add:

```cpp
    void use_inventory_item(const gl::app::InventoryUseRequest &request) {
        if (!pending_hero_stats_) return;
        const bool main = request.source == gl::app::InventorySelection::item;
        if (!main && request.source != gl::app::InventorySelection::hot) return;
        const auto &items = main ? inventory_runtime_.hero_inventory().items
                                 : inventory_runtime_.hero_inventory().hot;
        if (request.index >= items.size()) return;
        const auto *entry = item_catalog_.find(items[request.index].item_id);
        if (entry == nullptr) {
            gl::log(gl::LogLevel::warning, "item use: no catalog entry for item " +
                                               std::to_string(items[request.index].item_id));
            return;
        }
        const gl::game::HeroItemContainer container =
            main ? gl::game::HeroItemContainer::main : gl::game::HeroItemContainer::hot;
        const auto result = gl::game::use_hero_item(
            entry->item, container, request.index, *pending_hero_stats_, person_runtime_,
            inventory_runtime_);
        if (!result.used) return;
        pending_inventory_ = inventory_runtime_.hero_inventory();
        gl::log(gl::LogLevel::info,
                "item use: id=" + std::to_string(entry->item.description_id) +
                    " effects=" + std::to_string(result.effects_added) +
                    " count=" + std::to_string(main ? inventory_runtime_.hero_inventory().items.size()
                                                    : inventory_runtime_.hero_inventory().hot.size()));
        if (inventory_overlay_.visible()) {
            inventory_overlay_.close();
            open_inventory();
        }
    }
```

Add `#include "gl/game/item_use.hpp"` near the other game includes.

- [ ] **Step 2: Handle right-click**

In `case SDL_MOUSEBUTTONDOWN:` inside the `inventory_overlay_.visible()` block, replace the body with:

```cpp
                if (inventory_overlay_.visible()) {
                    if (event.button.which != SDL_TOUCH_MOUSEID) {
                        if (event.button.button == SDL_BUTTON_RIGHT) {
                            const auto request = inventory_overlay_.use_request_at(
                                event.button.x, event.button.y);
                            if (request) use_inventory_item(*request);
                            return;
                        }
                        const gl::app::InventoryRegion region =
                            inventory_overlay_.hit_test(event.button.x, event.button.y);
                        inventory_overlay_.handle_mouse(event.button.x, event.button.y,
                                                        event.button.button == SDL_BUTTON_LEFT);
                        if (region == gl::app::InventoryRegion::skills_tab &&
                            event.button.button == SDL_BUTTON_LEFT) {
                            open_character();
                        }
                    }
                    return;
                }
```

- [ ] **Step 3: Persist stats on save**

In `write_loaded_save_to`, inside the `if (body)` hero block after the containers/present loop, add:

```cpp
                if (pending_hero_stats_)
                    hero.stats = *pending_hero_stats_;
```

- [ ] **Step 4: Build and focused tests**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"; $env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*use_hero_item*,*writes stats*,*real 953*,*real 948*,*real 408*"
build\bin\inventory_overlay_tests.exe --test-case="*right-click use request*"
build\bin\glgame_screenshot_tests.exe --test-case="*inventory*"
```
Expected: PASS; inventory screenshot unchanged (no use side effect).

- [ ] **Step 5: Commit**

```powershell
git add tools/glgame/main.cpp
git commit -m "feat: wire potion/food use into glgame"
```

---

### Task 8: Docs and full ctest

**Files:**
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`
- Modify: `docs/DECISIONS.md`
- Already created: `docs/research/p5e-potions-food.md`,
  `docs/specs/2026-11-27-p5e-potions-food-design.md`,
  `docs/plans/2026-11-27-p5e-potions-food.md`

- [ ] **Step 1: Add D-116**

Add after D-115 in `docs/DECISIONS.md`:

```
| D-116 | 2026-11-27 | P5e potions/food use: `gl::game::use_hero_item` (type 19/20; direct stats for `0x41` ids 6..12/28 with clamps; other ITM effects -> P5 channel; HP/EP heal clamp; consume 1 from the exact slot); right-click on bottom/hot inventory slot (original code `0x4b`); BT_HERO stat block written from HeroStats. Books/scrolls/recipes, use-on-other, NPC use, `scripts/useitem`, and the client 2s queue are out of scope; the port applies immediately. |
```

- [ ] **Step 2: Update `docs/STATUS.md`**

Add at the top:

```
## P5e — Potions/food use (done 2026-11-27)

- **Задача:** right-click по зелью/еде применяет `FUN_14057cf0`: прямые
  статы, timed-эффекты в P5-канал, лечение HP/EP, расход 1 шт.; stat block
  сохраняется в BT_HERO.
- **Spec/research/plan:** `docs/specs/2026-11-27-p5e-potions-food-design.md`,
  `docs/research/p5e-potions-food.md`,
  `docs/plans/2026-11-27-p5e-potions-food.md`; **D-116**.
- **Оракулы:** `953.itm` HP+100 (slot0 51→50), `948.itm` `{6,+1},{10,-1}`,
  `408.itm` timed id30 3ч (`word3=0x00000003`).
- **Проверка (2026-11-27):** вписать фактический вывод focused
  (`gl_game_tests *use_hero_item*,*writes stats*,*real 953*,*real 948*,*real 408*`;
  `inventory_overlay_tests *right-click*`; `glgame_screenshot_tests *inventory*`)
  и полного `ctest` с `GL_GAME_DIR` (passed/total, время).
- **Статус:** done после зелёного полного `ctest`; иначе open с причиной.
```

- [ ] **Step 3: Update `docs/PROJECT.md`**

Append to the P5 row:
`P5e potions/food use done 2026-11-27 (D-116: right-click use, direct stats/timed effects/heal, BT_HERO stat block).`

- [ ] **Step 4: Full ctest with real data**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"; $env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: 21/21 passed, no regressions. Use a long bash timeout (≥3,000,000 ms).

- [ ] **Step 5: Verify docs encoding**

```powershell
$files=@('docs/STATUS.md','docs/PROJECT.md','docs/DECISIONS.md','docs/research/p5e-potions-food.md','docs/specs/2026-11-27-p5e-potions-food-design.md','docs/plans/2026-11-27-p5e-potions-food.md'); foreach($f in $files){ $b=[IO.File]::ReadAllBytes((Join-Path 'I:\goldenland2dec' $f)); $nul=($b|Where-Object{$_ -eq 0}).Count; "$f nul=$nul" }
```
Expected: `nul=0` for all.

- [ ] **Step 6: Commit docs**

```powershell
git add docs/STATUS.md docs/PROJECT.md docs/DECISIONS.md docs/research/p5e-potions-food.md docs/specs/2026-11-27-p5e-potions-food-design.md docs/plans/2026-11-27-p5e-potions-food.md
git commit -m "docs: p5e potions/food handoff and full ctest"
```

---

## Self-Review Notes

- Spec §3 → Task 3; §4.1 → Task 3; §4.2 → Task 1; §4.3 → Task 2;
  §4.4 → Task 6 + Task 7 Step 2; §4.5 → Task 4 + Task 7 Step 3;
  §7 unit → Tasks 1–4/6; §7 real → Task 5; §7 full ctest → Task 8.
- `HeroItemContainer` defined in `inventory_runtime.hpp`, included by
  `item_use.hpp` and used by `InventoryRuntime`, `InventoryUseRequest`
  maps app `InventorySelection` in glgame.
- `word3` for `408.itm` is `0x00000003` (dur_hi=3 hours), not `0x300`.
- Direct stat clamp 1..30 / reputation −50..50 matches runtime getters
  (`kMaxPrimary=30`, `kReputationMinimum/Maximum`).
