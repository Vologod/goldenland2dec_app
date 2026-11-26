# P5 Effect Runtime Core — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Модель активных эффектов персоны (`{id,value,flags,cond,extra}`): старение по игровым минутам, применение к статам листа, восстановление/запись в сейве (BT_HERO и type-2 tail).

**Architecture:** Новый SDL-free модуль `gl/game/effect`; `PersonRuntime` хранит эффекты и per-person часы; `combat_stats` подмешивает активные эффекты как псевдо-предмет; save restore/write синхронизирует `extra`/`tail80`; `glgame` тикает старение и грузит hero-эффекты.

**Tech Stack:** C++20, CMake+Ninja, doctest, существующие `gl_assets`/`gl_game`.

## Global Constraints

- C++20; имена типов `PascalCase`, функций/переменных `snake_case`; без комментариев без необходимости.
- SDL только в `platform/app/render/ui/tools`; `gl/game` остаётся SDL-free.
- Кодировки docs — UTF-8 без BOM; не писать docs через here-string/`Set-Content`.
- Запущенный `glgame.exe` блокирует линковку — закрыть перед build.
- Коммит — только по явной просьбе пользователя.
- Тулчейн:
  `$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"; $env:GL_GAME_DIR="I:\Games\Златогорье-2"`
- Эффекты: `0x10` длительность; `0x20` задержка (младшее слово) + активная длительность (старшее); `0x200` reject; `0x8` не реализуется; клиент применяет, если не `((flags&0x20)&&(cond&0xffff))`.
- `cond`: младшее слово b0=часы,b1=минуты; старшее b2=часы,b3=минуты.
- Реальный оракул: только `slot16` hero `extra` = `{9,-3,0x60,0x23000000,-1}`; raw DEX=10, стат DEX=7; через 35 игровых минут удаляется.

---

### Task 1: Ядро эффектов (aging, фильтр, часы)

**Files:**
- Create: `src/game/include/gl/game/effect.hpp`
- Create: `src/game/src/effect.cpp`
- Modify: `src/game/include/gl/game/person_runtime.hpp` (перенести `PersonEffect`, включить `effect.hpp`)
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_effect.cpp` (новый)
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `struct gl::game::PersonEffect { std::array<std::int32_t,5> words{}; };`
  - `bool gl::game::effect_delay_pending(const PersonEffect&)`
  - `void gl::game::age_effects(std::vector<PersonEffect>&, std::uint32_t elapsed_minutes)`
  - `std::int64_t gl::game::total_minutes(const GameDatetime&)`
  - `std::uint32_t gl::game::minutes_between(const GameDatetime& previous, const GameDatetime& now)`
  - `std::vector<gl::assets::SpecialMod> gl::game::active_effect_mods(const std::vector<PersonEffect>&)`

- [ ] **Step 1: Write the failing tests**

Create `tests/game/test_effect.cpp`:

```cpp
#include <doctest/doctest.h>

#include "gl/game/effect.hpp"

#include <cstdint>
#include <vector>

namespace {
gl::game::PersonEffect make_effect(std::int32_t id, std::int32_t value, std::uint32_t flags,
                                   std::uint32_t cond, std::int32_t extra = -1) {
    gl::game::PersonEffect effect;
    effect.words = {id, value, static_cast<std::int32_t>(flags), static_cast<std::int32_t>(cond),
                    extra};
    return effect;
}
} // namespace

TEST_CASE("effect_delay_pending follows the client skip rule") {
    CHECK(gl::game::effect_delay_pending(make_effect(9, -3, 0x20, 0x00000A00)));
    CHECK_FALSE(gl::game::effect_delay_pending(make_effect(9, -3, 0x20, 0x23000000)));
    CHECK_FALSE(gl::game::effect_delay_pending(make_effect(9, -3, 0x60, 0x23000000)));
    CHECK_FALSE(gl::game::effect_delay_pending(make_effect(9, -3, 0x40, 0)));
}

TEST_CASE("age_effects decrements 0x10 duration and removes on expiry") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x10, 0x00002801)};
    gl::game::age_effects(effects, 40);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x00000001);
    gl::game::age_effects(effects, 60);
    CHECK(effects.empty());
}

TEST_CASE("age_effects handles 0x20 delay then active duration") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x20, 0x23000A00)};
    gl::game::age_effects(effects, 5);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x23000500);
    gl::game::age_effects(effects, 5);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x23000000);
    gl::game::age_effects(effects, 35);
    CHECK(effects.empty());
}

TEST_CASE("age_effects overwrites the whole cond in the 0x10 branch") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x10, 0xAB002801)};
    gl::game::age_effects(effects, 40);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x00000001);
}

TEST_CASE("age_effects is a no-op for zero elapsed time") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x60, 0x23000000)};
    gl::game::age_effects(effects, 0);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x23000000);
}

TEST_CASE("total_minutes and minutes_between use whole minutes") {
    gl::game::GameDatetime a;
    a.year = 1124;
    a.month = 11;
    a.day = 27;
    a.hour = 4;
    a.minute = 0;
    gl::game::GameDatetime b = a;
    b.minute = 35;
    b.second = 59;
    CHECK(gl::game::minutes_between(a, b) == 35);
    CHECK(gl::game::minutes_between(b, a) == 0);
    CHECK(gl::game::minutes_between(gl::game::GameDatetime{}, b) == 0);
}

TEST_CASE("active_effect_mods drops pending delay records") {
    const std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x60, 0x23000000),
        make_effect(7, 2, 0x20, 0x00000500),
    };
    const auto mods = gl::game::active_effect_mods(effects);
    REQUIRE(mods.size() == 1);
    CHECK(mods[0].id == 9);
    CHECK(mods[0].value == -3);
    CHECK(mods[0].flags == 0x60);
    CHECK(mods[0].word3 == 0x23000000);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```powershell
$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"; $env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*effect*"
```
Expected: FAIL — `gl/game/effect.hpp` not found / link error.

- [ ] **Step 3: Implement the core**

Create `src/game/include/gl/game/effect.hpp`:

```cpp
#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/game/rginf.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace gl::game {
struct PersonEffect {
    std::array<std::int32_t, 5> words{};
};

constexpr std::uint32_t kEffectDuration = 0x10;
constexpr std::uint32_t kEffectDelayed = 0x20;
constexpr std::uint32_t kEffectDelayMask = 0xffff;

bool effect_delay_pending(const PersonEffect &effect);
void age_effects(std::vector<PersonEffect> &effects, std::uint32_t elapsed_minutes);
std::int64_t total_minutes(const GameDatetime &value);
std::uint32_t minutes_between(const GameDatetime &previous, const GameDatetime &now);
std::vector<gl::assets::SpecialMod> active_effect_mods(const std::vector<PersonEffect> &effects);
} // namespace gl::game
```

Create `src/game/src/effect.cpp`:

```cpp
#include "gl/game/effect.hpp"

#include "gl/game/calendar.hpp"

#include <cstddef>
#include <limits>

namespace gl::game {
namespace {
std::uint32_t packed_minutes(std::uint32_t cond, unsigned first_byte) {
    return ((cond >> (8 * first_byte)) & 0xffu) * 60u +
           ((cond >> (8 * (first_byte + 1))) & 0xffu);
}

std::uint32_t pack_low(std::uint32_t minutes) {
    return (minutes / 60u) | ((minutes % 60u) << 8);
}

std::uint32_t pack_high(std::uint32_t minutes) {
    return ((minutes / 60u) << 16) | ((minutes % 60u) << 24);
}
} // namespace

bool effect_delay_pending(const PersonEffect &effect) {
    const auto flags = static_cast<std::uint32_t>(effect.words[2]);
    const auto cond = static_cast<std::uint32_t>(effect.words[3]);
    return (flags & kEffectDelayed) != 0 && (cond & kEffectDelayMask) != 0;
}

void age_effects(std::vector<PersonEffect> &effects, std::uint32_t elapsed_minutes) {
    if (elapsed_minutes == 0)
        return;
    std::size_t index = 0;
    while (index < effects.size()) {
        PersonEffect &effect = effects[index];
        const auto flags = static_cast<std::uint32_t>(effect.words[2]);
        std::uint32_t cond = static_cast<std::uint32_t>(effect.words[3]);
        bool remove = false;
        if ((flags & kEffectDuration) != 0) {
            const std::uint32_t duration = packed_minutes(cond, 0);
            if (elapsed_minutes >= duration)
                remove = true;
            else
                cond = pack_low(duration - elapsed_minutes);
        }
        if (!remove && (flags & kEffectDelayed) != 0) {
            if ((cond & kEffectDelayMask) != 0) {
                const std::uint32_t delay = packed_minutes(cond, 0);
                if (elapsed_minutes >= delay)
                    cond &= 0xffff0000u;
                else
                    cond = (cond & 0xffff0000u) | pack_low(delay - elapsed_minutes);
            } else {
                const std::uint32_t duration = packed_minutes(cond, 2);
                if (elapsed_minutes >= duration)
                    remove = true;
                else
                    cond = pack_high(duration - elapsed_minutes);
            }
        }
        if (remove) {
            effects.erase(effects.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        effect.words[3] = static_cast<std::int32_t>(cond);
        ++index;
    }
}

std::int64_t total_minutes(const GameDatetime &value) {
    if (value.year == 0)
        return 0;
    return days_from_civil(static_cast<std::int32_t>(value.year), value.month, value.day) * 1440 +
           static_cast<std::int64_t>(value.hour) * 60 + static_cast<std::int64_t>(value.minute);
}

std::uint32_t minutes_between(const GameDatetime &previous, const GameDatetime &now) {
    const std::int64_t before = total_minutes(previous);
    const std::int64_t after = total_minutes(now);
    if (before == 0 || after <= before)
        return 0;
    const std::uint64_t delta = static_cast<std::uint64_t>(after - before);
    return delta > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(delta);
}

std::vector<gl::assets::SpecialMod> active_effect_mods(const std::vector<PersonEffect> &effects) {
    std::vector<gl::assets::SpecialMod> mods;
    mods.reserve(effects.size());
    for (const PersonEffect &effect : effects) {
        if (effect_delay_pending(effect))
            continue;
        gl::assets::SpecialMod mod;
        mod.id = static_cast<std::uint32_t>(effect.words[0]);
        mod.value = effect.words[1];
        mod.flags = static_cast<std::uint32_t>(effect.words[2]);
        mod.word3 = static_cast<std::uint32_t>(effect.words[3]);
        mod.word4 = static_cast<std::uint32_t>(effect.words[4]);
        mods.push_back(mod);
    }
    return mods;
}
} // namespace gl::game
```

Modify `src/game/include/gl/game/person_runtime.hpp`: add `#include "gl/game/effect.hpp"`, delete the local `struct PersonEffect` definition, keep `void set_injured_records(std::vector<PersonEffect>&, bool);`.

Modify `src/game/CMakeLists.txt`: add `src/effect.cpp` after `src/door_runtime.cpp`.

Modify `tests/game/CMakeLists.txt`: add `test_effect.cpp` to `gl_game_tests` sources.

- [ ] **Step 4: Run tests to verify they pass**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*effect*"
```
Expected: PASS (all cases above).

---

### Task 2: PersonRuntime — хранение эффектов, часы, активные моды

**Files:**
- Modify: `src/game/include/gl/game/person.hpp`
- Modify: `src/game/include/gl/game/person_runtime.hpp`
- Modify: `src/game/src/person_runtime.cpp`
- Test: `tests/game/test_effect.cpp`

**Interfaces:**
- Consumes: Task 1 (`PersonEffect`, `age_effects`, `minutes_between`, `active_effect_mods`).
- Produces:
  - `PersonInstance::saved_effects` (`std::vector<PersonEffect>`), `PersonInstance::effect_clock` (`std::optional<GameDatetime>`).
  - `PersonState::effect_clock`.
  - `std::vector<PersonEffect> PersonRuntime::effects(std::string_view) const`
  - `void PersonRuntime::set_effects(std::string_view, std::vector<PersonEffect>)`
  - `std::optional<GameDatetime> PersonRuntime::effect_clock(std::string_view) const`
  - `void PersonRuntime::set_effect_clock(std::string_view, std::optional<GameDatetime>)`
  - `void PersonRuntime::age_effects(const GameDatetime&)`
  - `std::vector<gl::assets::SpecialMod> PersonRuntime::active_effect_mods(std::string_view) const`

- [ ] **Step 1: Write the failing tests**

Append to `tests/game/test_effect.cpp`:

```cpp
#include "gl/game/person.hpp"
#include "gl/game/person_runtime.hpp"

TEST_CASE("PersonRuntime restores saved effects and exposes active mods") {
    gl::game::PersonInstance person;
    person.name = "A";
    person.route_type = "STAY";
    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x60, 0x23000000, -1};
    person.saved_effects.push_back(effect);
    auto runtime = gl::game::PersonRuntime::create({person}, std::nullopt);

    REQUIRE(runtime.effects("A").size() == 1);
    const auto mods = runtime.active_effect_mods("A");
    REQUIRE(mods.size() == 1);
    CHECK(mods[0].id == 9);
    CHECK(mods[0].value == -3);
    CHECK(runtime.effects("missing").empty());
    CHECK(runtime.active_effect_mods("missing").empty());
}

TEST_CASE("PersonRuntime ages each person from its own clock") {
    gl::game::PersonInstance person;
    person.name = "A";
    person.route_type = "STAY";
    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x20, 0x00000A00, -1};
    person.saved_effects.push_back(effect);
    gl::game::GameDatetime start;
    start.year = 1124;
    start.month = 11;
    start.day = 27;
    start.hour = 4;
    start.minute = 0;
    person.effect_clock = start;
    auto runtime = gl::game::PersonRuntime::create({person}, std::nullopt);

    gl::game::GameDatetime now = start;
    now.minute = 5;
    runtime.age_effects(now);
    REQUIRE(runtime.effects("A").size() == 1);
    CHECK(static_cast<std::uint32_t>(runtime.effects("A")[0].words[3]) == 0x00000500);

    now.minute = 10;
    runtime.age_effects(now);
    now.minute = 20;
    runtime.age_effects(now);
    CHECK(runtime.active_effect_mods("A").empty());
}

TEST_CASE("PersonRuntime set_effects and set_effect_clock replace state") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);
    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x40, 0, -1};
    runtime.set_effects("Hero", {effect});
    REQUIRE(runtime.effects("Hero").size() == 1);
    gl::game::GameDatetime clock;
    clock.year = 1124;
    runtime.set_effect_clock("Hero", clock);
    REQUIRE(runtime.effect_clock("Hero").has_value());
    CHECK(runtime.effect_clock("Hero")->year == 1124);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*effect*,PersonRuntime*"
```
Expected: FAIL — `PersonRuntime::effects` / `saved_effects` not declared.

- [ ] **Step 3: Implement**

In `src/game/include/gl/game/person.hpp` add includes and fields:

```cpp
#include "gl/game/effect.hpp"
#include <optional>
```
Inside `struct PersonInstance`:
```cpp
    std::vector<PersonEffect> saved_effects;
    std::optional<GameDatetime> effect_clock;
```
(`person.hpp` already includes `<optional>` and `<vector>`.)

In `src/game/include/gl/game/person_runtime.hpp`, inside `struct PersonState` add:
```cpp
    std::optional<GameDatetime> effect_clock;
```
and public methods:
```cpp
    std::vector<PersonEffect> effects(std::string_view person) const;
    void set_effects(std::string_view person, std::vector<PersonEffect> effects);
    std::optional<GameDatetime> effect_clock(std::string_view person) const;
    void set_effect_clock(std::string_view person, std::optional<GameDatetime> clock);
    void age_effects(const GameDatetime &now);
    std::vector<gl::assets::SpecialMod> active_effect_mods(std::string_view person) const;
```

In `src/game/src/person_runtime.cpp` in the NPC loop of `create`, immediately before `add(std::move(state));` add:
```cpp
        state.effects = person.saved_effects;
        state.effect_clock = person.effect_clock;
```
Place right before `add(std::move(state));`.

Add implementations:
```cpp
std::vector<PersonEffect> PersonRuntime::effects(std::string_view person) const {
    const PersonState *state = find(person);
    return state == nullptr ? std::vector<PersonEffect>{} : state->effects;
}

void PersonRuntime::set_effects(std::string_view person, std::vector<PersonEffect> effects) {
    PersonState *state = find(person);
    if (state != nullptr)
        state->effects = std::move(effects);
}

std::optional<GameDatetime> PersonRuntime::effect_clock(std::string_view person) const {
    const PersonState *state = find(person);
    return state == nullptr ? std::nullopt : state->effect_clock;
}

void PersonRuntime::set_effect_clock(std::string_view person,
                                     std::optional<GameDatetime> clock) {
    PersonState *state = find(person);
    if (state != nullptr)
        state->effect_clock = clock;
}

void PersonRuntime::age_effects(const GameDatetime &now) {
    for (PersonState &state : persons_) {
        const GameDatetime previous = state.effect_clock.value_or(now);
        gl::game::age_effects(state.effects, minutes_between(previous, now));
        state.effect_clock = now;
    }
}

std::vector<gl::assets::SpecialMod>
PersonRuntime::active_effect_mods(std::string_view person) const {
    const PersonState *state = find(person);
    return state == nullptr ? std::vector<gl::assets::SpecialMod>{}
                            : gl::game::active_effect_mods(state->effects);
}
```
Add `#include <utility>` if `std::move` is not already included (file already uses move).

- [ ] **Step 4: Run tests to verify they pass**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*effect*,PersonRuntime*"
```
Expected: PASS.

---

### Task 3: combat_stats — модификаторы эффектов

**Files:**
- Modify: `src/game/include/gl/game/combat_stats.hpp`
- Modify: `src/game/src/combat_stats.cpp`
- Test: `tests/game/test_effect.cpp`

**Interfaces:**
- Consumes: `gl::assets::SpecialMod`.
- Produces: `StatInputs::effect_mods` (`std::vector<gl::assets::SpecialMod>`).

- [ ] **Step 1: Write the failing test**

Append to `tests/game/test_effect.cpp`:

```cpp
#include "gl/game/combat_stats.hpp"

TEST_CASE("compute_armor_weight applies effect mods") {
    gl::game::StatInputs inputs;
    inputs.strength = 10;
    inputs.constitution = 15;
    inputs.dexterity = 10;
    inputs.perception = 21;
    inputs.luck = 10;
    inputs.wisdom = 5;
    inputs.intelligence = 11;
    inputs.experience = 14155;
    inputs.skills.fill(0);

    const auto without = gl::game::compute_armor_weight(inputs);
    gl::assets::SpecialMod effect;
    effect.id = 9;
    effect.value = -3;
    effect.flags = 0x60;
    effect.word3 = 0x23000000;
    inputs.effect_mods.push_back(effect);
    const auto with = gl::game::compute_armor_weight(inputs);

    CHECK(without.primary[2] == 10);
    CHECK(with.primary[2] == 7);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*effect*"
```
Expected: FAIL — no member `effect_mods`.

- [ ] **Step 3: Implement**

In `src/game/include/gl/game/combat_stats.hpp`, inside `struct StatInputs` add:
```cpp
    std::vector<gl::assets::SpecialMod> effect_mods;
```

In `src/game/src/combat_stats.cpp`, at the start of `compute_armor_weight` add:
```cpp
    std::vector<EquippedStatItem> equipped = inputs.equipped;
    if (!inputs.effect_mods.empty()) {
        EquippedStatItem effects_item;
        effects_item.mods = inputs.effect_mods;
        equipped.push_back(effects_item);
    }
```
Then replace every `inputs.equipped` inside `compute_armor_weight` with `equipped` (shield loop, weapon loop, all `apply_modifiers`/`attribute_value` calls). Do not touch `StatInputs::equipped` itself.

- [ ] **Step 4: Run test to verify it passes**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*effect*,*combat stats*,*character sheet*"
```
Expected: PASS.

---

### Task 4: NPC save restore/write — эффекты и часы

**Files:**
- Modify: `src/game/src/save_restore.cpp`
- Modify: `src/game/include/gl/game/tail_write.hpp`
- Modify: `src/game/src/tail_write.cpp`
- Modify: `src/game/src/live_save.cpp`
- Test: `tests/game/test_tail_write.cpp`

**Interfaces:**
- Consumes: `PersonEffect`, `GameDatetime`, `PersonRuntime::effects/effect_clock`.
- Produces:
  - `PersonInstance::saved_effects`/`effect_clock` заполняются в `restore_save_persons`.
  - `TailPersonLive::effects` (`std::optional<std::vector<PersonEffect>>`),
    `TailPersonLive::effect_clock` (`std::optional<GameDatetime>`).

- [ ] **Step 1: Write the failing test**

Append to `tests/game/test_tail_write.cpp`:

```cpp
TEST_CASE("build_selected_tail writes live effects and clock") {
    const auto record = make_person("A", 10, 10, 0);
    gl::assets::SaveTailEntry loaded;
    loaded.name = "test";
    loaded.rginf.latch = 1;
    loaded.entities.push_back(person_entity(record));
    const auto level = make_level({make_instance("A", 10, 10, 0)});
    const auto simulation = gl::game::LevelSimulation::create(level, 1);
    const auto inventory = make_inventory(record);
    std::array<gl::game::TailPersonLive, 1> live{};
    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x60, 0x23000000, -1};
    live[0].effects = std::vector<gl::game::PersonEffect>{effect};
    gl::game::GameDatetime clock;
    clock.year = 1124;
    clock.month = 11;
    clock.day = 27;
    clock.hour = 4;
    clock.minute = 31;
    live[0].effect_clock = clock;

    const auto rebuilt = gl::game::build_selected_tail(
        loaded, level, simulation, inventory, make_restore(0, 0), live, {}, loaded.rginf,
        nullptr);
    REQUIRE(rebuilt.has_value());
    REQUIRE(rebuilt->entities.size() == 1);
    const auto decoded = gl::game::decode_save_person(rebuilt->entities[0]);
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->extra.size() == 1);
    CHECK(decoded->tail_count == 1);
    std::int32_t words[5];
    for (std::size_t i = 0; i < 5; ++i) {
        words[i] = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(decoded->extra[0][4 * i]) |
            (static_cast<std::uint32_t>(decoded->extra[0][4 * i + 1]) << 8) |
            (static_cast<std::uint32_t>(decoded->extra[0][4 * i + 2]) << 16) |
            (static_cast<std::uint32_t>(decoded->extra[0][4 * i + 3]) << 24));
    }
    CHECK(words[0] == 9);
    CHECK(words[1] == -3);
    CHECK(static_cast<std::uint32_t>(words[3]) == 0x23000000);
    const auto clock_back = gl::game::GameDatetime::from_time_bytes(
        std::array<std::uint8_t, 0x20>{decoded->tail80[0x40], decoded->tail80[0x41],
                                        decoded->tail80[0x42], decoded->tail80[0x43],
                                        decoded->tail80[0x44], decoded->tail80[0x45],
                                        decoded->tail80[0x46], decoded->tail80[0x47],
                                        decoded->tail80[0x48], decoded->tail80[0x49],
                                        decoded->tail80[0x4A], decoded->tail80[0x4B],
                                        decoded->tail80[0x4C], decoded->tail80[0x4D],
                                        decoded->tail80[0x4E], decoded->tail80[0x4F],
                                        decoded->tail80[0x50], decoded->tail80[0x51],
                                        decoded->tail80[0x52], decoded->tail80[0x53],
                                        decoded->tail80[0x54], decoded->tail80[0x55],
                                        decoded->tail80[0x56], decoded->tail80[0x57],
                                        decoded->tail80[0x58], decoded->tail80[0x59],
                                        decoded->tail80[0x5A], decoded->tail80[0x5B],
                                        decoded->tail80[0x5C], decoded->tail80[0x5D],
                                        decoded->tail80[0x5E], decoded->tail80[0x5F]});
    CHECK(clock_back.year == 1124);
    CHECK(clock_back.minute == 31);
}
```

Add includes to `test_tail_write.cpp`:
```cpp
#include "gl/game/effect.hpp"
#include "gl/game/rginf.hpp"
```

- [ ] **Step 2: Run test to verify it fails**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*build_selected_tail writes live effects*"
```
Expected: FAIL — `TailPersonLive` has no `effects`.

- [ ] **Step 3: Implement**

In `src/game/src/save_restore.cpp`, add includes:
```cpp
#include "gl/game/effect.hpp"
#include "gl/game/rginf.hpp"

#include <algorithm>
#include <optional>
```
Add helper in the anonymous namespace:
```cpp
std::int32_t read_i32_le(const std::uint8_t *bytes) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(bytes[0]) |
                                     (static_cast<std::uint32_t>(bytes[1]) << 8) |
                                     (static_cast<std::uint32_t>(bytes[2]) << 16) |
                                     (static_cast<std::uint32_t>(bytes[3]) << 24));
}
```
In `apply_record`, at the end add:
```cpp
    person.saved_effects.clear();
    person.saved_effects.reserve(record.extra.size());
    for (const auto &raw : record.extra) {
        PersonEffect effect;
        for (std::size_t i = 0; i < effect.words.size(); ++i)
            effect.words[i] = read_i32_le(raw.data() + 4 * i);
        person.saved_effects.push_back(effect);
    }
    std::array<std::uint8_t, 0x20> clock_bytes{};
    std::copy_n(record.tail80.begin() + 0x40, 0x20, clock_bytes.begin());
    const GameDatetime clock = GameDatetime::from_time_bytes(clock_bytes);
    person.effect_clock = clock.year == 0 ? std::nullopt : std::optional<GameDatetime>(clock);
```

In `src/game/include/gl/game/tail_write.hpp`, add `#include "gl/game/effect.hpp"` and `<vector>`, then inside `TailPersonLive`:
```cpp
    std::optional<std::vector<PersonEffect>> effects;
    std::optional<GameDatetime> effect_clock;
```

In `src/game/src/tail_write.cpp`, add includes:
```cpp
#include "gl/game/effect.hpp"

#include <algorithm>
#include <array>
```
Add helper in the anonymous namespace:
```cpp
std::array<std::uint8_t, 0x14> pack_effect(const PersonEffect &effect) {
    std::array<std::uint8_t, 0x14> out{};
    for (std::size_t i = 0; i < effect.words.size(); ++i) {
        const auto value = static_cast<std::uint32_t>(effect.words[i]);
        out[4 * i] = static_cast<std::uint8_t>(value & 0xFF);
        out[4 * i + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        out[4 * i + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
        out[4 * i + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    }
    return out;
}
```
In `build_selected_tail`, after the existing HP/EP/AP patch block (before `if (name_counts[...] == 1) apply_live_containers(...)`) add:
```cpp
        if (live[restore_index].effects) {
            record.extra.clear();
            record.extra.reserve(live[restore_index].effects->size());
            for (const PersonEffect &effect : *live[restore_index].effects)
                record.extra.push_back(pack_effect(effect));
            record.tail_count = static_cast<std::uint32_t>(record.extra.size());
        }
        if (live[restore_index].effect_clock) {
            const auto bytes = live[restore_index].effect_clock->to_time_bytes();
            std::copy(bytes.begin(), bytes.end(), record.tail80.begin() + 0x40);
        }
```

In `src/game/src/live_save.cpp`, inside the loop building `TailPersonLive`, after the energy block add:
```cpp
                if (input.persons != nullptr && name_counts[name] == 1) {
                    person.effects = input.persons->effects(name);
                    person.effect_clock = input.persons->effect_clock(name);
                }
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*build_selected_tail*"
```
Expected: PASS.

---

### Task 5: Hero save write — эффекты и часы

**Files:**
- Modify: `src/game/include/gl/game/hero_save_write.hpp`
- Modify: `src/game/src/hero_save_write.cpp`
- Test: `tests/game/test_hero_save_write.cpp`

**Interfaces:**
- Produces: `HeroLiveState::effects` (`std::vector<PersonEffect>`), `HeroLiveState::has_effects` (`bool`), `HeroLiveState::effect_clock` (`std::optional<GameDatetime>`).

- [ ] **Step 1: Write the failing test**

Append to `tests/game/test_hero_save_write.cpp`:

```cpp
TEST_CASE("make_hero_save_body writes live effects and clock") {
    auto body = gl::assets::HeroSaveBody{};
    auto live = gl::game::HeroLiveState{};
    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x60, 0x23000000, -1};
    live.effects.push_back(effect);
    live.has_effects = true;
    gl::game::GameDatetime clock;
    clock.year = 1124;
    clock.month = 11;
    clock.day = 27;
    clock.hour = 4;
    clock.minute = 31;
    live.effect_clock = clock;

    const auto result = gl::game::make_hero_save_body(body, live);
    REQUIRE(result.extra.size() == 1);
    const auto decoded = gl::assets::parse_hero_save_body([&] {
        const auto encoded = gl::assets::encode_hero_save_body(result);
        REQUIRE(encoded.has_value());
        return encoded.value();
    }());
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->extra.size() == 1);
    std::int32_t words[5];
    for (std::size_t i = 0; i < 5; ++i) {
        words[i] = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(decoded->extra[0][4 * i]) |
            (static_cast<std::uint32_t>(decoded->extra[0][4 * i + 1]) << 8) |
            (static_cast<std::uint32_t>(decoded->extra[0][4 * i + 2]) << 16) |
            (static_cast<std::uint32_t>(decoded->extra[0][4 * i + 3]) << 24));
    }
    CHECK(words[0] == 9);
    CHECK(words[1] == -3);
    const auto clock_back = gl::game::GameDatetime::from_time_bytes(
        std::array<std::uint8_t, 0x20>{decoded->tail80[0x40], decoded->tail80[0x41],
                                        decoded->tail80[0x42], decoded->tail80[0x43],
                                        decoded->tail80[0x44], decoded->tail80[0x45],
                                        decoded->tail80[0x46], decoded->tail80[0x47],
                                        decoded->tail80[0x48], decoded->tail80[0x49],
                                        decoded->tail80[0x4A], decoded->tail80[0x4B],
                                        decoded->tail80[0x4C], decoded->tail80[0x4D],
                                        decoded->tail80[0x4E], decoded->tail80[0x4F],
                                        decoded->tail80[0x50], decoded->tail80[0x51],
                                        decoded->tail80[0x52], decoded->tail80[0x53],
                                        decoded->tail80[0x54], decoded->tail80[0x55],
                                        decoded->tail80[0x56], decoded->tail80[0x57],
                                        decoded->tail80[0x58], decoded->tail80[0x59],
                                        decoded->tail80[0x5A], decoded->tail80[0x5B],
                                        decoded->tail80[0x5C], decoded->tail80[0x5D],
                                        decoded->tail80[0x5E], decoded->tail80[0x5F]});
    CHECK(clock_back.year == 1124);
    CHECK(clock_back.minute == 31);
}
```

Add includes to `test_hero_save_write.cpp`:
```cpp
#include "gl/game/effect.hpp"
#include "gl/game/rginf.hpp"
```

- [ ] **Step 2: Run test to verify it fails**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*make_hero_save_body writes live effects*"
```
Expected: FAIL — `HeroLiveState` has no `effects`.

- [ ] **Step 3: Implement**

In `src/game/include/gl/game/hero_save_write.hpp`, add `#include "gl/game/effect.hpp"` and `<optional>`, then inside `struct HeroLiveState`:
```cpp
    std::vector<PersonEffect> effects;
    bool has_effects = false;
    std::optional<GameDatetime> effect_clock;
```

In `src/game/src/hero_save_write.cpp`, add `#include <algorithm>` and `#include <array>` and a local helper:
```cpp
namespace {
std::array<std::uint8_t, 0x14> pack_effect(const PersonEffect &effect) {
    std::array<std::uint8_t, 0x14> out{};
    for (std::size_t i = 0; i < effect.words.size(); ++i) {
        const auto value = static_cast<std::uint32_t>(effect.words[i]);
        out[4 * i] = static_cast<std::uint8_t>(value & 0xFF);
        out[4 * i + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        out[4 * i + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
        out[4 * i + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    }
    return out;
}
} // namespace
```
At the end of `make_hero_save_body`, before `return body;`:
```cpp
    if (live.has_effects) {
        body.extra.clear();
        body.extra.reserve(live.effects.size());
        for (const PersonEffect &effect : live.effects)
            body.extra.push_back(pack_effect(effect));
    }
    if (live.effect_clock) {
        const auto bytes = live.effect_clock->to_time_bytes();
        std::copy(bytes.begin(), bytes.end(), body.tail80.begin() + 0x40);
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*make_hero_save_body*,*hero*"
```
Expected: PASS.

---

### Task 6: Real-оракул slot16 (aging + DEX)

**Files:**
- Create: `tests/game/test_effect_real.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Consumes: `gl::assets::decode_save_body`, `parse_hero_stats`, `parse_hero_save_body`, `gl::game::age_effects`, `compute_armor_weight`.

- [ ] **Step 1: Write the failing tests**

Create `tests/game/test_effect_real.cpp`:

```cpp
#include <doctest/doctest.h>

#include "gl/assets/hero_save_body.hpp"
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/combat_stats.hpp"
#include "gl/game/effect.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::vector<std::uint8_t> read_save(const std::string &name) {
    const std::filesystem::path path =
        std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" / name;
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

std::vector<std::uint8_t> hero_body(int slot) {
    auto body = gl::assets::decode_save_body(read_save("slot" + std::to_string(slot) + ".sav"));
    REQUIRE(body.has_value());
    return body.value();
}

gl::game::PersonEffect effect_at(const std::vector<std::uint8_t> &raw) {
    gl::game::PersonEffect effect;
    for (std::size_t i = 0; i < effect.words.size(); ++i) {
        effect.words[i] = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(raw[4 * i]) |
            (static_cast<std::uint32_t>(raw[4 * i + 1]) << 8) |
            (static_cast<std::uint32_t>(raw[4 * i + 2]) << 16) |
            (static_cast<std::uint32_t>(raw[4 * i + 3]) << 24));
    }
    return effect;
}
} // namespace

TEST_CASE("real saves: hero effects are 0 except slot16" *
          doctest::skip(!game_dir_available())) {
    for (int slot = 0; slot <= 27; ++slot) {
        const auto body = gl::assets::parse_hero_save_body(hero_body(slot));
        REQUIRE(body.has_value());
        if (slot == 16) {
            REQUIRE(body->extra.size() == 1);
            const auto effect = effect_at(body->extra[0]);
            CHECK(effect.words[0] == 9);
            CHECK(effect.words[1] == -3);
            CHECK(static_cast<std::uint32_t>(effect.words[2]) == 0x60);
            CHECK(static_cast<std::uint32_t>(effect.words[3]) == 0x23000000);
            CHECK(effect.words[4] == -1);
        } else {
            CHECK(body->extra.empty());
        }
    }
}

TEST_CASE("real slot16: effect ages out after 35 game minutes" *
          doctest::skip(!game_dir_available())) {
    const auto body = gl::assets::parse_hero_save_body(hero_body(16));
    REQUIRE(body.has_value());
    REQUIRE(body->extra.size() == 1);
    std::vector<gl::game::PersonEffect> effects{effect_at(body->extra[0])};
    gl::game::age_effects(effects, 10);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x19000000);
    gl::game::age_effects(effects, 25);
    CHECK(effects.empty());
}

TEST_CASE("real slot16: DEX effect reduces the sheet primary" *
          doctest::skip(!game_dir_available())) {
    const auto body = hero_body(16);
    auto stats = gl::assets::parse_hero_stats(body);
    REQUIRE(stats.has_value());
    auto effect_body = gl::assets::parse_hero_save_body(body);
    REQUIRE(effect_body.has_value());
    REQUIRE(effect_body->extra.size() == 1);

    gl::game::StatInputs inputs;
    inputs.strength = stats->strength;
    inputs.constitution = stats->constitution;
    inputs.dexterity = stats->dexterity;
    inputs.perception = stats->perception;
    inputs.luck = stats->luck;
    inputs.wisdom = stats->wisdom;
    inputs.intelligence = stats->intelligence;
    inputs.experience = stats->experience;
    inputs.skills.fill(0);
    for (std::size_t id = 0; id < inputs.skills.size(); ++id)
        inputs.skills[id] = stats->skills[id];
    CHECK(inputs.dexterity == 10);

    const auto without = gl::game::compute_armor_weight(inputs);
    CHECK(without.primary[2] == 10);

    const auto effect = effect_at(effect_body->extra[0]);
    const auto mods = gl::game::active_effect_mods({effect});
    REQUIRE(mods.size() == 1);
    inputs.effect_mods = mods;
    const auto with = gl::game::compute_armor_weight(inputs);
    CHECK(with.primary[2] == 7);
}
```

Modify `tests/game/CMakeLists.txt`: add `test_effect_real.cpp` right after `test_effect.cpp`.

Note: `decode_save_body` is declared in `gl/assets/vfs.hpp` in this codebase (used by `test_person_runtime_real.cpp`). If the compiler complains, include `gl/assets/hero_equipment.hpp` instead.

- [ ] **Step 2: Run tests to verify they fail**

Run:
```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*real*effect*,*real slot16*"
```
Expected: FAIL (test names not registered yet) — after build with CMake change, they run and should pass only after Tasks 1–3; if run before, failures pinpoint missing pieces.

- [ ] **Step 3: Run tests to verify they pass (after Tasks 1–3)**

Run:
```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*real saves: hero effects*,*real slot16*"
```
Expected: PASS (3 cases, DEX 10→7, 35 min expiry).

---

### Task 7: glgame wiring (load, tick, sheet, save)

**Files:**
- Modify: `tools/glgame/main.cpp`

**Interfaces:**
- Consumes: Tasks 2–5.

- [ ] **Step 1: Add pending hero effect state**

Near the existing `std::optional<gl::assets::HeroStats> pending_hero_stats_;` (around line 3736) add:
```cpp
    std::vector<gl::game::PersonEffect> pending_hero_effects_;
    std::optional<gl::game::GameDatetime> pending_hero_effect_clock_;
```

In `reset_pending_hero()` (around line 2344) add:
```cpp
        pending_hero_effects_.clear();
        pending_hero_effect_clock_.reset();
```

- [ ] **Step 2: Decode hero effects on load**

In `apply_hero_from_body` (after the `parse_hero_inventory` block, around line 2384) add:
```cpp
        if (auto save_body = gl::assets::parse_hero_save_body(body)) {
            pending_hero_effects_.clear();
            for (const auto &raw : save_body->extra) {
                gl::game::PersonEffect effect;
                for (std::size_t i = 0; i < effect.words.size(); ++i) {
                    effect.words[i] = static_cast<std::int32_t>(
                        static_cast<std::uint32_t>(raw[4 * i]) |
                        (static_cast<std::uint32_t>(raw[4 * i + 1]) << 8) |
                        (static_cast<std::uint32_t>(raw[4 * i + 2]) << 16) |
                        (static_cast<std::uint32_t>(raw[4 * i + 3]) << 24));
                }
                pending_hero_effects_.push_back(effect);
            }
            std::array<std::uint8_t, 0x20> clock_bytes{};
            std::copy_n(save_body->tail80.begin() + 0x40, 0x20, clock_bytes.begin());
            const gl::game::GameDatetime clock =
                gl::game::GameDatetime::from_time_bytes(clock_bytes);
            pending_hero_effect_clock_ =
                clock.year == 0 ? std::nullopt : std::optional<gl::game::GameDatetime>(clock);
        }
```
Add includes if missing: `#include "gl/assets/hero_save_body.hpp"`,
`#include "gl/game/effect.hpp"`, `#include "gl/game/rginf.hpp"`,
`#include <array>`, `#include <algorithm>` (main.cpp already includes
`gl/assets/hero_save_body.hpp` around line 17).

- [ ] **Step 3: Apply to runtime**

In `rebuild_person_runtime` after `person_runtime_ = gl::game::PersonRuntime::create(...)` add:
```cpp
        person_runtime_.set_effects("Hero", pending_hero_effects_);
        person_runtime_.set_effect_clock("Hero", pending_hero_effect_clock_);
```

- [ ] **Step 4: Age each frame**

In `run_interactive` after:
```cpp
            if (has_calendar_)
                calendar_.advance(now);
```
add:
```cpp
            if (has_calendar_)
                person_runtime_.age_effects(calendar_.now());
```

- [ ] **Step 5: Apply effects to the sheet**

In `open_character`, after the carried-items loop (before `armor_weight_values = ...`, around line 1332) add:
```cpp
            inputs.effect_mods = person_runtime_.active_effect_mods("Hero");
```

- [ ] **Step 6: Persist live hero effects**

In `write_loaded_save_to`, after `hero.containers[...]` assignments (around line 2767), add:
```cpp
                hero.has_effects = true;
                hero.effects = person_runtime_.effects("Hero");
                hero.effect_clock = person_runtime_.effect_clock("Hero");
```

- [ ] **Step 7: Build and run focused tests**

Run:
```powershell
$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"; $env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*effect*,*combat stats*,*character sheet*,*build_selected_tail*,*make_hero_save_body*"
build\bin\glgame_screenshot_tests.exe --test-case="*character*"
```
Expected: PASS; `character` screenshot case still passes (slot0 has no effects).

- [ ] **Step 8: Manual smoke (user gate)**

Run:
```powershell
build\bin\glgame.exe --load-save 16 --character
```
Expected: открывается лист; в логе `character: ...` значения с DEX−3 (armor/AP/acc ниже, чем без эффекта). Manual pass подтверждает пользователь.

---

### Task 8: Документы и полный прогон

**Files:**
- Modify: `docs/STATUS.md` (новая секция сверху)
- Modify: `docs/PROJECT.md` (P5/P7 статус)
- Modify: `docs/DECISIONS.md` (D-115)
- Already created: `docs/research/p5-effect-runtime.md`,
  `docs/specs/2026-11-26-p5-effect-runtime-core-design.md`

- [ ] **Step 1: Update `docs/DECISIONS.md`**

Add row after D-114:
```
| D-115 | 2026-11-26 | P5 effect runtime core: `PersonEffect` (5×int32), aging по `FUN_1405719c` (0x10/0x20, минуты), клиентский skip `0x20 && (cond&0xffff)`, эффекты → `combat_stats` как псевдо-предмет, save BT_HERO `extra`/`tail80` и type-2 tail. Продюсеры (зелья/магия) и `LE_CastMagic` — отдельные подпроекты. | Канал эффектов общий для экипировки/травм/магии; формулы листа уже принимают `FUN_1201b1c4`, но `PersonState.effects` не передавались. Реальный оракул: `slot16` hero `{9,-3,0x60,0x23000000}`. |
```

- [ ] **Step 2: Update `docs/STATUS.md`**

Add section at the top:
```
## P5 effect runtime core (in progress 2026-11-26)

- **Задача:** общий канал эффектов персоны: aging, клиентский skip
  задержки, применение к статам листа, restore/write в сейве.
- **Spec/research:** `docs/specs/2026-11-26-p5-effect-runtime-core-design.md`,
  `docs/research/p5-effect-runtime.md`; **D-115**.
- **Факты:** старение `FUN_1405719c` (RVA `0x5719c`, не `0x57954`);
  клиент `FUN_120b0690`; BT_HERO `FUN_1401d84c/0x1d8bc`; type-2 tail.
  `slot16` hero `{9,-3,0x60,0x23000000}`, raw DEX 10 → 7, expiry 35 мин.
- **Проверка (2026-11-26):** по факту выполнения записать сюда вывод
  `gl_game_tests --test-case="*effect*"` (число тестов/assertions),
  `glgame_screenshot_tests --test-case="*character*"` и полного
  `ctest` с `GL_GAME_DIR` (passed/total и время).
- **Статус:** done после зелёного полного `ctest`; иначе — open с причиной.
```
Впишите в секцию `STATUS` фактический вывод: число тестов/assertions
`gl_game_tests --test-case="*effect*"`, результат
`glgame_screenshot_tests --test-case="*character*"` и полного `ctest`.

- [ ] **Step 3: Update `docs/PROJECT.md`**

In the P5 row append: `P5 effect runtime core done 2026-11-26 (D-115: aging/stat/save канал; producers potions/magic pending).`

- [ ] **Step 4: Full ctest with real data**

Run:
```powershell
$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"; $env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: 21/21 passed (текущий набор), без регрессий. Record duration and result in `STATUS`.

- [ ] **Step 5: Verify docs encoding**

Run:
```powershell
$files=@('docs/STATUS.md','docs/PROJECT.md','docs/DECISIONS.md','docs/research/p5-effect-runtime.md','docs/specs/2026-11-26-p5-effect-runtime-core-design.md'); foreach($f in $files){ $b=[IO.File]::ReadAllBytes((Join-Path 'I:\goldenland2dec' $f)); $nul=($b|Where-Object{$_ -eq 0}).Count; "$f nul=$nul" }
```
Expected: `nul=0` for all.

---

## Self-Review Notes

- Spec §3 aging → Task 1; §4.1 → Task 1; §4.2/4.3 → Task 2; §4.4 → Task 3;
  §5 NPC → Task 4; §5 hero → Task 5; §7 real oracle → Task 6; §4.5 → Task 7;
  §7 full ctest/docs → Task 8.
- `PersonEffect` defined once in `effect.hpp`; every later task includes it and
  never redeclares.
- `effect_mods` name consistent between `StatInputs` and glgame.
- `tail80[0x40..0x60]` (3rd snapshot, `+0xf9c`) used consistently for the clock.
