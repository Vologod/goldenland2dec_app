# P6a8 Dialog/Injury Natives Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `RS_GetDialogEnabled` (`0x700000E`) and `RS_SetInjured` (`0x7000013`) with the `Server.dll` semantics recorded in `docs/specs/2026-11-23-p6a8-dialog-injury-runtime-design.md`.

**Architecture:** `PersonRuntime` gains the injury record machine and the dialog-percent formula; `PersonAccess`/`RandomAccess` expose them to the script layer; `native_registry.cpp` registers both natives; `glgame`/`glscript` wire person and coin services. Tests cover unit, native dispatch, synthetic VM E2E and real `.cs` data.

**Tech Stack:** C++20, CMake + Ninja, doctest, SDL2 build (no SDL needed for these tests), existing `gl_assets`/`gl_script`/`gl_game` libraries.

## Global Constraints

- C++20; no comments in code unless strictly necessary; types `PascalCase`, functions/variables `snake_case`, files `snake_case`.
- Layers only downward: `platform -> app -> game/render/ui/audio -> script/core -> assets`.
- Test framework is doctest; build with CMake + Ninja from `build/`.
- The user explicitly authorized one commit per task (2026-11-23). Commit each task's code/tests/docs with a concise message; never push. Stop and ask before any push or history rewrite.
- Docs are UTF-8 without BOM; after edits verify `bom=False nul=False` and no literal `\n` artifacts.
- Toolchain/PATH for this session:
  ```powershell
  $tools="I:\goldenland2dec\.local\toolchains"
  $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
  $env:GL_GAME_DIR="I:\Games\Златогорье-2"
  ```
- Full acceptance runs `ctest --test-dir build --output-on-failure` with `GL_GAME_DIR` set.

---

### Task 1: Game runtime — injury records and dialog percent

**Files:**
- Modify: `src/game/include/gl/game/person_runtime.hpp`
- Modify: `src/game/src/person_runtime.cpp`
- Test: `tests/game/test_person_runtime.cpp`

**Interfaces:**
- Consumes: existing `PersonState`, `PersonRuntime::create`, `intelligence_value`, `skill_value`, `clamp_value`, `round_half_away_from_zero`, `kMaxSkill`.
- Produces:
  - `struct gl::game::PersonEffect { std::array<std::int32_t, 5> words{}; };`
  - `void gl::game::set_injured_records(std::vector<PersonEffect> &records, bool injured);`
  - `void PersonRuntime::set_injured(std::string_view person, bool injured);`
  - `bool PersonRuntime::injured(std::string_view person) const;`
  - `std::int32_t PersonRuntime::dialog_percent(std::string_view person) const;`

- [ ] **Step 1: Write the failing unit tests**

Append to `tests/game/test_person_runtime.cpp`:

```cpp
TEST_CASE("injury records use the fixed SetInjured key") {
    const std::array<std::int32_t, 5> key{22, -49, 129, 0, -1};
    std::vector<gl::game::PersonEffect> records{{1, 2, 3, 4, 5}};

    gl::game::set_injured_records(records, true);
    REQUIRE(records.size() == 2);
    CHECK(records.back().words == key);

    gl::game::set_injured_records(records, true);
    REQUIRE(records.size() == 2);
    CHECK(records.back().words == key);

    gl::game::set_injured_records(records, false);
    REQUIRE(records.size() == 1);
    CHECK(records.front().words == std::array<std::int32_t, 5>{1, 2, 3, 4, 5});
}

TEST_CASE("person runtime set-injured toggles known persons only") {
    const std::vector<gl::game::PersonInstance> persons{make_person("L1.P1", "L1.P1")};
    auto runtime = gl::game::PersonRuntime::create(persons, std::nullopt);

    CHECK_FALSE(runtime.injured("L1.P1"));
    runtime.set_injured("L1.P1", true);
    CHECK(runtime.injured("L1.P1"));
    runtime.set_injured("missing", true);
    CHECK_FALSE(runtime.injured("missing"));
    runtime.set_injured("L1.P1", false);
    CHECK_FALSE(runtime.injured("L1.P1"));
}

TEST_CASE("dialog percent reproduces the talking formula") {
    const auto hero = [](std::int32_t intelligence, std::int32_t talking,
                         std::int32_t trade) {
        gl::assets::HeroStats stats;
        stats.intelligence = intelligence;
        stats.skills[21] = talking;
        stats.skills[16] = trade;
        return stats;
    };

    auto silent = gl::game::PersonRuntime::create({}, hero(8, 0, 10));
    CHECK(silent.dialog_percent("Hero") == 0);

    auto maxed = gl::game::PersonRuntime::create({}, hero(0, 15, 0));
    CHECK(maxed.dialog_percent("Hero") == 100);

    auto base = gl::game::PersonRuntime::create({}, hero(7, 5, 0));
    CHECK(base.dialog_percent("Hero") == 37);

    auto trader = gl::game::PersonRuntime::create({}, hero(7, 5, 10));
    CHECK(trader.dialog_percent("Hero") == 57);

    auto talking_bonus = gl::game::PersonRuntime::create({}, hero(5, 10, 9));
    CHECK(talking_bonus.dialog_percent("Hero") == 57);

    CHECK(base.dialog_percent("missing") == 0);
}
```

- [ ] **Step 2: Build and confirm the red state**

Run:
```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
cmake --build build
```
Expected: compile errors for unknown `gl::game::PersonEffect`, `set_injured_records`, `injured`, `dialog_percent`.

- [ ] **Step 3: Add the header declarations**

In `src/game/include/gl/game/person_runtime.hpp`, after `int hero_level_from_experience(double experience);`:

```cpp
struct PersonEffect {
    std::array<std::int32_t, 5> words{};
};

void set_injured_records(std::vector<PersonEffect> &records, bool injured);
```

Add to `PersonState` after `std::int32_t party_role = 0;`:

```cpp
    std::vector<PersonEffect> effects;
```

Add to `PersonRuntime` after `std::int32_t party_role(std::string_view person) const;`:

```cpp
    void set_injured(std::string_view person, bool injured);
    bool injured(std::string_view person) const;
    std::int32_t dialog_percent(std::string_view person) const;
```

- [ ] **Step 4: Implement the runtime**

In `src/game/src/person_runtime.cpp`, add `#include <algorithm>` next to the other includes.

In the anonymous namespace (before the closing `} // namespace` at line 210), after `constexpr double kMaxExperience = 500000.0;`:

```cpp
constexpr std::array<std::int32_t, 5> kInjuryRecord{22, -49, 129, 0, -1};
```

Immediately after the anonymous namespace closes and before `int hero_level_from_experience(...)`, add:

```cpp
void set_injured_records(std::vector<PersonEffect> &records, bool injured) {
    records.erase(std::remove_if(records.begin(), records.end(),
                                 [](const PersonEffect &record) {
                                     return record.words == kInjuryRecord;
                                 }),
                  records.end());
    if (injured) records.push_back(PersonEffect{kInjuryRecord});
}
```

After `PersonRuntime::party_role`, add:

```cpp
void PersonRuntime::set_injured(std::string_view person, bool injured) {
    PersonState *state = find(person);
    if (state != nullptr) set_injured_records(state->effects, injured);
}

bool PersonRuntime::injured(std::string_view person) const {
    const PersonState *state = find(person);
    if (state == nullptr) return false;
    return std::any_of(state->effects.begin(), state->effects.end(),
                       [](const PersonEffect &record) {
                           return record.words == kInjuryRecord;
                       });
}

std::int32_t PersonRuntime::dialog_percent(std::string_view person) const {
    const PersonState *state = find(person);
    if (state == nullptr) return 0;
    const auto clamped_skill = [state](std::string_view name) {
        return static_cast<std::int32_t>(clamp_value(skill_value(*state, name), 0.0, kMaxSkill));
    };
    const std::int32_t talking = clamped_skill("SKILL_TALKING");
    if (talking == 0) return 0;
    if (talking > 14) return 100;
    const std::int32_t intelligence =
        static_cast<std::int32_t>(intelligence_value(*state));
    float value = static_cast<float>(intelligence + 10 + talking * 4);
    if (clamped_skill("SKILL_TRADE") > 9) value += 20.0f;
    return static_cast<std::int32_t>(
        round_half_away_from_zero(static_cast<double>(value)));
}
```

- [ ] **Step 5: Build and run the focused tests**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*injury*,*dialog percent*"
```
Expected: all selected cases pass, including the three added cases.

---

### Task 2: Script access, native handlers, registry

**Files:**
- Modify: `src/script/include/gl/script/person_access.hpp`
- Modify: `src/script/include/gl/script/random_access.hpp`
- Modify: `src/script/src/native_registry.cpp`
- Test: `tests/game/test_person_runtime.cpp`
- Test: `tests/game/test_random_native.cpp`
- Test: `tests/script/test_native_registry.cpp`

**Interfaces:**
- Consumes: Task 1 `PersonRuntime::{set_injured,dialog_percent}`; existing `is_string`, `is_numeric`, `as_int`, `as_string`, `warn_native`.
- Produces:
  - `PersonAccess::set_injured` = `std::function<void(std::string_view, bool)>`
  - `PersonAccess::dialog_percent` = `std::function<std::int32_t()>`
  - `RandomAccess::coin` = `std::function<bool(double)>`
  - registry entries `0x700000E` (arity 1) and `0x7000013` (arity 2), both `supported`.

- [ ] **Step 1: Write the failing native tests**

Append to `tests/game/test_person_runtime.cpp`:

```cpp
TEST_CASE("dialog-enabled native gates the percent with the shared coin") {
    gl::assets::HeroStats stats;
    stats.intelligence = 7;
    stats.skills[21] = 5;
    auto runtime = gl::game::PersonRuntime::create({}, stats);

    gl::script::NativeServices services;
    services.person = runtime.access();
    bool gate = true;
    double probability = -1.0;
    services.random.coin = [&](double value) {
        probability = value;
        return gate;
    };

    const auto *info = gl::script::NativeRegistry::find(0x700000e);
    REQUIRE(info != nullptr);
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 1);

    const std::vector<gl::script::Value> args{double{2.0}};
    CHECK(gl::script::NativeRegistry::call(*info, args, services) ==
          gl::script::Value{double{1.0}});
    CHECK(probability == doctest::Approx(0.37));

    gate = false;
    CHECK(gl::script::NativeRegistry::call(*info, args, services) ==
          gl::script::Value{double{0.0}});
}

TEST_CASE("dialog-enabled native rejects invalid arguments and missing services") {
    const auto *info = gl::script::NativeRegistry::find(0x700000e);
    REQUIRE(info != nullptr);
    gl::script::NativeServices services;

    const std::vector<gl::script::Value> string_arg{std::string{"Hero"}};
    CHECK(gl::script::NativeRegistry::call(*info, string_arg, services) ==
          gl::script::Value{double{0.0}});

    const std::vector<gl::script::Value> empty;
    CHECK(gl::script::NativeRegistry::call(*info, empty, services) ==
          gl::script::Value{double{0.0}});

    const std::vector<gl::script::Value> args{double{2.0}};
    CHECK(gl::script::NativeRegistry::call(*info, args, services) ==
          gl::script::Value{double{0.0}});
}

TEST_CASE("set-injured native forwards the person and flag") {
    const std::vector<gl::game::PersonInstance> persons{make_person("L1.P1", "L1.P1")};
    auto runtime = gl::game::PersonRuntime::create(persons, std::nullopt);
    gl::script::NativeServices services;
    services.person = runtime.access();

    const auto *info = gl::script::NativeRegistry::find(0x7000013);
    REQUIRE(info != nullptr);
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 2);

    const std::vector<gl::script::Value> on{std::string{"L1.P1"}, double{1.0}};
    const std::vector<gl::script::Value> off{std::string{"L1.P1"}, double{0.0}};
    CHECK(gl::script::NativeRegistry::call(*info, on, services) ==
          gl::script::Value{double{0.0}});
    CHECK(runtime.injured("L1.P1"));
    CHECK(gl::script::NativeRegistry::call(*info, off, services) ==
          gl::script::Value{double{0.0}});
    CHECK_FALSE(runtime.injured("L1.P1"));
}

TEST_CASE("person access exposes dialog percent and set-injured") {
    gl::assets::HeroStats stats;
    stats.intelligence = 7;
    stats.skills[21] = 5;
    auto runtime = gl::game::PersonRuntime::create({}, stats);
    auto access = runtime.access();
    REQUIRE(access.dialog_percent);
    CHECK(access.dialog_percent() == 37);
    REQUIRE(access.set_injured);
    access.set_injured("Hero", true);
    CHECK(runtime.injured("Hero"));
}
```

Append to `tests/game/test_random_native.cpp`:

```cpp
TEST_CASE("dialog-enabled coin matches the shared MsvcRandom stream") {
    MsvcRandom expected(7);
    MsvcRandom actual(7);
    NativeServices services;
    services.person.dialog_percent = [] { return std::int32_t{50}; };
    services.random.coin = [&actual](double probability) {
        return actual.coin(probability);
    };

    for (int index = 0; index < 20; ++index) {
        const bool expected_result = expected.coin(0.5);
        const Value result = call(0x700000e, {double{2.0}}, services);
        REQUIRE(std::holds_alternative<double>(result));
        CHECK(std::get<double>(result) == (expected_result ? 1.0 : 0.0));
    }
}
```

In `tests/script/test_native_registry.cpp`, replace:
```cpp
    CHECK(NativeRegistry::find(0x700000e)->support == NativeSupport::unsupported);
```
with:
```cpp
    CHECK(NativeRegistry::find(0x700000e)->support == NativeSupport::supported);
    CHECK(NativeRegistry::find(0x7000013)->support == NativeSupport::supported);
```

- [ ] **Step 2: Build and confirm the red state**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*dialog-enabled*,*set-injured*"
build\bin\gl_script_tests.exe --test-case="*native registry classifies*"
```
Expected: compile errors for missing `PersonAccess`/`RandomAccess` members and failing registry checks.

- [ ] **Step 3: Extend the access structs**

In `src/script/include/gl/script/person_access.hpp`, add to `PersonAccess` after `set_undead`:

```cpp
    std::function<void(std::string_view person, bool)> set_injured;
    std::function<std::int32_t()> dialog_percent;
```

In `src/script/include/gl/script/random_access.hpp`, add to `RandomAccess` after `range`:

```cpp
    std::function<bool(double)> coin;
```

- [ ] **Step 4: Add native handlers**

In `src/script/src/native_registry.cpp`, after `native_set_undead`, add:

```cpp
Value native_get_dialog_enabled(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 1 || !is_numeric(args[0])) {
        warn_native("script native RS_GetDialogEnabled: invalid arguments", services);
        return double{0.0};
    }
    if (!services.person.dialog_percent || !services.random.coin) return double{0.0};
    const float probability =
        static_cast<float>(services.person.dialog_percent()) * 0.01f;
    return services.random.coin(static_cast<double>(probability)) ? double{1.0}
                                                                 : double{0.0};
}

Value native_set_injured(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 2 || !is_string(args[0]) || !is_numeric(args[1])) {
        warn_native("script native RS_SetInjured: invalid arguments", services);
        return double{0.0};
    }
    if (services.person.set_injured)
        services.person.set_injured(as_string(args[0]), as_int(args[1]) != 0);
    return double{0.0};
}
```

In the registry list, replace:
```cpp
    add(0x700000e, NativeSupport::unsupported, "requires dialog/person runtime");
```
with:
```cpp
    add(0x700000e, NativeSupport::supported, {}, &native_get_dialog_enabled, 1);
```
and replace:
```cpp
    add(0x7000013, NativeSupport::unsupported, "requires person runtime state");
```
with:
```cpp
    add(0x7000013, NativeSupport::supported, {}, &native_set_injured, 2);
```

- [ ] **Step 5: Build and run the focused tests**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*dialog-enabled*,*set-injured*,*dialog percent*,*injury*"
build\bin\gl_script_tests.exe --test-case="*native registry*"
```
Expected: pass, including the coin-stream parity case.

---

### Task 3: App wiring (`glgame`/`glscript`)

**Files:**
- Modify: `src/game/src/person_runtime.cpp` (`PersonRuntime::access`)
- Modify: `tools/glgame/main.cpp` (near `random.range` at ~1473)
- Modify: `tools/glscript/main.cpp` (near `random.range` at ~239)

**Interfaces:**
- Consumes: Task 1/2 `dialog_percent`, `set_injured`, `RandomAccess::coin`.
- Produces: `glgame`/`glscript` services that satisfy both natives.

> Ordering note (2026-11-23): the Task 2 implementer added the `PersonAccess` bindings and the `person access exposes ...` test early, because that test belongs to Task 2's brief. If they are already present and passing, skip Step 2 and the test creation here; only Steps 3-4 (the `coin` wiring) remain.

- [ ] **Step 1: Write the failing access test**

The test from Task 1 (`person access exposes dialog percent and set-injured`) already covers bindings; run it before the implementation:
```powershell
build\bin\gl_game_tests.exe --test-case="*person access exposes*"
```
Expected: FAIL (missing bindings).

- [ ] **Step 2: Bind `PersonAccess`**

In `src/game/src/person_runtime.cpp`, inside `PersonRuntime::access()` after `access.set_undead = ...`:

```cpp
    access.set_injured = [this](std::string_view person, bool injured) {
        set_injured(person, injured);
    };
    access.dialog_percent = [this] { return dialog_percent("Hero"); };
```

- [ ] **Step 3: Wire `glgame` coin**

In `tools/glgame/main.cpp`, after the `random.range` assignment:

```cpp
        context->host.services().random.coin = [this](double probability) {
            return level_ ? level_->simulation().rng().coin(probability)
                          : fallback_rng_.coin(probability);
        };
```

- [ ] **Step 4: Wire `glscript` coin**

In `tools/glscript/main.cpp`, after the `random.range` assignment:

```cpp
    services.random.coin = [&rng](double probability) {
        return rng.coin(probability);
    };
```

- [ ] **Step 5: Build and run**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*person access exposes*,*dialog-enabled*,*set-injured*"
build\bin\gl_script_tests.exe --test-case="*native registry*"
```
Expected: pass.

---

### Task 4: VM end-to-end and real `.cs` coverage

**Files:**
- Modify: `tests/game/test_game_script_host.cpp` (synthetic VM E2E)
- Modify: `tests/script/test_real_dialogs.cpp` (real data structural + behavioral)

**Interfaces:**
- Consumes: `gl::game::GameScriptHost`, `gl::script::ScriptVm`, `gl::assets::parse_cs`, `gl::game::PersonRuntime`.
- Produces: proof that both natives run through the VM and that shipped `.cs` call sites use the expected arguments.

- [ ] **Step 1: Add the synthetic VM tests**

Append to `tests/game/test_game_script_host.cpp`; add includes at the top:
```cpp
#include "gl/assets/cs.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <variant>
```

```cpp
namespace {
gl::assets::CsNode dialog_node(std::int32_t opcode) {
    gl::assets::CsNode node;
    node.opcode = opcode;
    return node;
}
gl::assets::CsNode dialog_num(double value) {
    gl::assets::CsNode node = dialog_node(24);
    node.value = value;
    return node;
}
gl::assets::CsNode dialog_str(std::string text) {
    gl::assets::CsNode node = dialog_node(22);
    node.text = std::move(text);
    return node;
}
gl::assets::CsNode dialog_str_var(std::string text) {
    gl::assets::CsNode node = dialog_node(23);
    node.text = std::move(text);
    return node;
}
gl::assets::CsNode dialog_func(std::uint32_t id,
                               std::initializer_list<std::int32_t> args) {
    gl::assets::CsNode node = dialog_node(48);
    node.value = static_cast<double>(id);
    std::size_t index = 0;
    for (std::int32_t arg : args) node.args[index++] = arg;
    return node;
}
gl::assets::CsNode dialog_assign(std::int32_t dest, std::int32_t source,
                                 std::int32_t next) {
    gl::assets::CsNode node = dialog_node(50);
    node.a = dest;
    node.b = source;
    node.d = next;
    return node;
}
gl::assets::CsNode dialog_jmp(std::int32_t target, std::int32_t next) {
    gl::assets::CsNode node = dialog_node(49);
    node.c = target;
    node.d = next;
    return node;
}
} // namespace

TEST_CASE("game script host runs GetDialogEnabled through the VM") {
    std::vector<gl::assets::CsNode> nodes(5);
    nodes[0] = dialog_assign(1, 2, 3);
    nodes[1] = dialog_str_var("result");
    nodes[2] = dialog_func(0x700000e, {4});
    nodes[3] = dialog_jmp(-1, -1);
    nodes[4] = dialog_num(2.0);

    gl::script::NativeServices services;
    bool gate = true;
    double probability = -1.0;
    services.person.dialog_percent = [] { return std::int32_t{37}; };
    services.random.coin = [&](double value) {
        probability = value;
        return gate;
    };

    gl::game::GameScriptHost host(services);
    gl::script::ScriptVm vm(std::move(nodes), host);
    vm.set_globals({gl::script::ScriptGlobal{"float", "result", double{0.0}}});
    vm.restart(true);

    CHECK(vm.step());
    const gl::script::Value *result = vm.variable("result");
    REQUIRE(result != nullptr);
    REQUIRE(std::holds_alternative<double>(*result));
    CHECK(std::get<double>(*result) == doctest::Approx(1.0));
    CHECK(probability == doctest::Approx(0.37));

    gate = false;
    vm.restart(true);
    CHECK(vm.step());
    const gl::script::Value *again = vm.variable("result");
    REQUIRE(again != nullptr);
    REQUIRE(std::holds_alternative<double>(*again));
    CHECK(std::get<double>(*again) == doctest::Approx(0.0));
}

TEST_CASE("game script host runs SetInjured through the VM") {
    std::vector<gl::assets::CsNode> nodes(4);
    nodes[0] = dialog_func(0x7000013, {1, 2});
    nodes[1] = dialog_str("Hero");
    nodes[2] = dialog_num(1.0);
    nodes[3] = dialog_jmp(-1, -1);

    gl::script::NativeServices services;
    std::string name;
    bool flag = false;
    services.person.set_injured = [&](std::string_view person, bool injured) {
        name = std::string(person);
        flag = injured;
    };

    gl::game::GameScriptHost host(services);
    gl::script::ScriptVm vm(std::move(nodes), host);
    vm.restart(true);
    while (vm.step()) {
    }
    CHECK(name == "Hero");
    CHECK(flag);
}
```

- [ ] **Step 2: Build and run the synthetic VM tests**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*through the VM*"
```
Expected: pass.

- [ ] **Step 3: Add real-data tests**

In `tests/script/test_real_dialogs.cpp`, add includes:
```cpp
#include "gl/game/game_script_host.hpp"
#include "gl/game/person_runtime.hpp"
#include <cstddef>
#include <utility>
```

Append:

```cpp
TEST_CASE("real data: SetInjured call sites carry the Hero flag" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto script_data =
        vfs->read("scripts/dialogs/l1_2.p397_healer_experimentator.d397.age.cs");
    REQUIRE(script_data.has_value());
    auto nodes = parse_cs(script_data.value());
    REQUIRE(nodes.has_value());

    const auto &list = nodes.value();
    std::size_t calls = 0;
    for (const auto &node : list) {
        if (node.opcode != 48 || node.value != 0x7000013.0) continue;
        ++calls;
        REQUIRE(node.args[0] >= 0);
        REQUIRE(node.args[1] >= 0);
        const std::int32_t name_index = node.args[0];
        const std::int32_t flag_index = node.args[1];
        REQUIRE(static_cast<std::size_t>(name_index) < list.size());
        REQUIRE(static_cast<std::size_t>(flag_index) < list.size());
        CHECK(list[static_cast<std::size_t>(name_index)].text == "Hero");
        const double flag = list[static_cast<std::size_t>(flag_index)].value;
        CHECK((flag == 0.0 || flag == 1.0));
    }
    CHECK(calls == 4);
}

TEST_CASE("real data: SetInjured toggles the Hero effect list" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto script_data =
        vfs->read("scripts/dialogs/l1_2.p397_healer_experimentator.d397.age.cs");
    REQUIRE(script_data.has_value());
    auto parsed = parse_cs(script_data.value());
    REQUIRE(parsed.has_value());
    const auto &list = parsed.value();

    const auto run_call = [&list](double flag) {
        const auto found = [&]() -> std::size_t {
            for (std::size_t index = 0; index < list.size(); ++index) {
                const auto &node = list[index];
                if (node.opcode != 48 || node.value != 0x7000013.0) continue;
                const std::int32_t flag_index = node.args[1];
                if (flag_index < 0 ||
                    static_cast<std::size_t>(flag_index) >= list.size())
                    continue;
                if (list[static_cast<std::size_t>(flag_index)].value == flag)
                    return index;
            }
            return list.size();
        }();
        REQUIRE(found < list.size());

        std::vector<gl::assets::CsNode> nodes = list;
        nodes[0] = gl::assets::CsNode{};
        nodes[0].opcode = 49;
        nodes[0].c = static_cast<std::int32_t>(found);
        nodes[0].d = static_cast<std::int32_t>(found);

        auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);
        gl::script::NativeServices services;
        services.person = runtime.access();
        gl::game::GameScriptHost host(services);
        gl::script::ScriptVm vm(std::move(nodes), host);
        vm.restart(true);
        CHECK(vm.step());
        CHECK(vm.step());
        CHECK_FALSE(vm.failed());
        return runtime.injured("Hero");
    };

    CHECK(run_call(1.0));
    CHECK_FALSE(run_call(0.0));
}

TEST_CASE("real data: GetDialogEnabled call sites use the ignored int argument" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto script_data =
        vfs->read("scripts/dialogs/l12_1/l32.p214_feather_girl.d214.age.cs");
    REQUIRE(script_data.has_value());
    auto nodes = parse_cs(script_data.value());
    REQUIRE(nodes.has_value());

    const auto &list = nodes.value();
    std::size_t calls = 0;
    for (const auto &node : list) {
        if (node.opcode != 48 || node.value != 0x700000e.0) continue;
        ++calls;
        REQUIRE(node.args[0] >= 0);
        const std::size_t arg_index = static_cast<std::size_t>(node.args[0]);
        REQUIRE(arg_index < list.size());
        CHECK(list[arg_index].value == doctest::Approx(2.0));
    }
    CHECK(calls >= 1);
}
```

Note: the real files are present in both `scripts.pak` and `scripts.update.3.pak` with the same counts (l1_2: 4 SetInjured calls, l32.p214: 13 GetDialogEnabled calls); the VFS override result has the same shapes.

- [ ] **Step 4: Build and run the real tests**

Run:
```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_script_real_tests.exe --test-case="*SetInjured*,*GetDialogEnabled*"
```
Expected: pass (or skip with exit 77 when `GL_GAME_DIR` is absent).

---

### Task 5: Documentation, registry evidence, full verification

**Files:**
- Modify: `docs/research/p6a8-person-dialog-injury.md`
- Modify: `docs/DECISIONS.md`
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`
- Verify: build, focused tests, full `ctest`.

**Interfaces:**
- Consumes: all prior tasks.
- Produces: updated facts/decisions and recorded verification output.

- [ ] **Step 1: Update research**

Append to `docs/research/p6a8-person-dialog-injury.md`:

```markdown
## Resolution (2026-11-23)

- `RS_GetDialogEnabled` handler `0x410b4`: type gate `FUN_14038c6c(0, -1)`
  (one numeric argument, ignored), `FUN_14027f7c` returns the first valid
  controlled-party id (`world+0x1cf4/+0x1cf8/+0x1cfc`; the same list is
  averaged by `FUN_140254bc`), `FUN_14009f08` uses
  `SKILL_TALKING` (`0x15`) and effective INT, `+20` for `SKILL_TRADE`
  (`0x10`) > 9, then `FUN_14006288((float)percent * 0.01f)`.
- Constants: `DAT_1408d7a0` 0.5f, `DAT_1408d7a4` -0.5f,
  `DAT_1408d7a8` 0.0f, `DAT_1408d7c4` 1/32767f (`0x38000100`),
  `DAT_1408d7c8` 1.0f, `DAT_1408d7cc` 0.0f, `DAT_1409146c` 0.01f,
  `DAT_14091468` 1.0f, `DAT_14091460/70` 0.0f.
- `RS_SetInjured` handler `0x41c34`: type gate `(3, 0, -1)`,
  `FUN_1402914c`, `FUN_14018848`. The fixed key `DAT_140790e8` is
  `16 00 00 00 cf ff ff ff 81 00 00 00 00 00 00 00 ff ff ff ff`
  (dwords `{22, -49, 129, 0, -1}`); the function removes all matching
  records and appends the key when the argument is non-zero.
- Usage re-scanned with `glpak extract` over `scripts.pak` +
  `scripts.update.3.pak`: `RS_GetDialogEnabled` 100 calls / 34 files
  (99 plain + 1 legacy-XOR `l2.p588_kor_aferist.d588`),
  `RS_SetInjured` 8 calls / 2 files; args `"Hero"` + 0/1; the
  `RS_GetDialogEnabled` int argument is 2 in sampled sites.
- Port decision: `FUN_14007fb0` identity, `manager+0xC` gate 0 and
  `gv_addon` 0 follow D-050; hero is the single-player controlled
  party member. Consumers, save interpretation and other effect
  producers remain out of scope.
```

- [ ] **Step 2: Add the decision**

Append to `docs/DECISIONS.md` after D-110:

```markdown
| D-111 | 2026-11-23 | P6a8 dialog/injury natives - `RS_GetDialogEnabled` (0x700000E) is supported (38/65), arity 1, ignores its numeric argument and evaluates the hero (`FUN_14027f7c` first controlled-party member; NPCs have no `SKILL_TALKING`). Percent = `round(INT_eff + 10 + SKILL_TALKING*4 + (SKILL_TRADE>9 ? 20 : 0))`, with `SKILL_TALKING` 0 -> 0 and 15 -> 100, `INT_eff` from `get_parameter(INTELLIGENCE)`; the result is gated by `coin(float(percent)*0.01f)` on the shared MSVC stream and returns 1.0/0.0. `RS_SetInjured` (0x7000013) is supported (38/65), arity 2, and maintains the ordered 0x14-byte person effect list: remove every record equal to `{22,-49,129,0,-1}`, then append it when the flag is non-zero; unknown persons are a port no-op (original AV). `PersonAccess` exposes `dialog_percent`/`set_injured`, `RandomAccess` exposes `coin`; `glgame` uses the level RNG (fallback with `--seed`), `glscript` seed 1. `FUN_14007fb0` is identity, `manager+0xC` and `gv_addon` are 0 per D-050; effect consumers, save interpretation, companions/multiplayer and list capacity are out of scope. | spec/plan 2026-11-23 p6a8; research p6a8-person-dialog-injury.md; D-050, D-103. |
```

- [ ] **Step 3: Update STATUS/PROJECT**

In `docs/STATUS.md`, add a new section at the top after the intro:

```markdown
## P6a8 — Dialog/injury natives (done 2026-11-23)

- **Задача:** `RS_GetDialogEnabled` (0x700000E) и `RS_SetInjured`
  (0x7000013): runtime, нативы, wiring, real/synthetic тесты.
- **Spec/plan:** `docs/specs/2026-11-23-p6a8-dialog-injury-runtime-design.md`,
  `docs/plans/2026-11-23-p6a8-dialog-injury-runtime.md`; **D-111**;
  research `docs/research/p6a8-person-dialog-injury.md`.
- **Результат:** `PersonEffect`/`set_injured_records` (ключ
  `{22,-49,129,0,-1}`); `PersonRuntime::{set_injured,injured,dialog_percent}`
  (формула `INT_eff+10+TALKING*4`, `TRADE>9 ? +20`);
  `PersonAccess::{dialog_percent,set_injured}`, `RandomAccess::coin`;
  registry 38/65; glgame/glscript wiring; synthetic VM E2E и real `.cs`
  call-site/toggle проверки.
- **Проверка:** вписать сюда фактический вывод Step 4/5: число focused
  cases/assertions и полный `ctest` (N/N passed, seconds).
- **Residual:** `FUN_14007fb0` при непустой таблице модификаторов,
  `manager+0xC`/`gv_addon`, потребители эффектов, save-интерпретация,
  компаньоны, capacity `+0x14f8`.
- **Статус:** done.
```

In `docs/PROJECT.md` P6 row, extend the existing P6 summary with:
`P6a8 dialog/injury natives done 2026-11-23 (D-111: GetDialogEnabled/SetInjured supported, 38/65).`

- [ ] **Step 4: Build and run focused verification**

Run:
```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*injury*,*dialog percent*,*dialog-enabled*,*set-injured*,*person access exposes*"
build\bin\gl_game_tests.exe --test-case="*through the VM*"
build\bin\gl_script_tests.exe --test-case="*native registry*"
build\bin\gl_script_real_tests.exe --test-case="*SetInjured*,*GetDialogEnabled*"
```
Expected: all selected cases pass (real tests skip only without `GL_GAME_DIR`).

- [ ] **Step 5: Run the full suite**

Run:
```powershell
ctest --test-dir build --output-on-failure
```
Expected: all tests pass; record the failing/passing count and elapsed time in `STATUS.md`.

- [ ] **Step 6: Verify docs and report**

Run:
```powershell
$paths = @("docs\specs\2026-11-23-p6a8-dialog-injury-runtime-design.md",
           "docs\plans\2026-11-23-p6a8-dialog-injury-runtime.md",
           "docs\research\p6a8-person-dialog-injury.md",
           "docs\STATUS.md","docs\PROJECT.md","docs\DECISIONS.md")
foreach ($p in $paths) {
  $b=[IO.File]::ReadAllBytes($p)
  $bom=($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF)
  $nul=($b -contains 0)
  $lit=([regex]::Matches([IO.File]::ReadAllText($p),'\\n')).Count
  "$p bom=$bom nul=$nul literal_n=$lit"
}
git diff --check
```
Expected: all `bom=False nul=False literal_n=0`; `git diff --check` clean.

Report the focused and full test output to the user. Do not commit.
