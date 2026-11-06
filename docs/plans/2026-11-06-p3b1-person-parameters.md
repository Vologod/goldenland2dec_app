# Phase 1b1 — Person parameter natives (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Реализовать `RS_GetPersonParameterI`, `RS_SetPersonParameterI` и `RS_GetPersonSkillI` на состоянии порта с семантикой из `Server.dll`, включая Hero-статы из save.

**Architecture:** `gl_script` получает словарь параметров и `PersonAccess` callbacks; `gl_game::PersonRuntime` хранит статы NPC/Hero и реализует callbacks; `gl_assets::parse_hero_stats` читает stat-блок `BT_HERO`; `glgame` связывает runtime с `GameScriptHost`.

**Tech Stack:** C++20, CMake + Ninja, doctest, Ghidra 12.1.4 headless / llvm-objdump (research), PowerShell 5.1.

## Global Constraints

- `Server.dll` SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Поведение — только из `Server.dll` (D-029); «правдоподобные» реализации запрещены.
- Слои только вниз: `game → script → assets`; `gl_script` не зависит от `gl_game`/SDL.
- Документы UTF-8 без BOM, `nul=0`.
- Комментарии в коде не добавлять без необходимости.
- Коммиты по задачам разрешены: пользователь явно разрешил их 2026-11-06
  (ответ «main, с коммитами»).
- Перед каждым шагом сверять `Get-Date`; в окне 21:00–05:00 соблюдать checkpoint.

---

### Task 1: Research person parameter semantics

**Files:**
- Create: `docs/research/p3b1-person-parameters-server.md`
- Read: `.local/decomp/server/decompiled.c`, `.local/decomp/server/natives/natives.c`

**Interfaces:**
- Consumes: `Server.dll` v1.3.
- Produces: таблицы, которые используют Tasks 2–4:
  - `§params`: 12 имён → getter RVA → setter RVA → поле/формула;
  - `§defaults`: значения `DAT_140913d8`, `DAT_140913e0`, `DAT_140913f0`, `DAT_14091400`;
  - `§hero_block`: offset каждого поля в `BT_HERO` stat-блоке;
  - `§skill_ids`: имя навыка → id (или verdict `unverified`).

- [ ] **Step 1: Record existing evidence**

Copy the native table rows for IDs `0x4000000`, `0x4000001`, `0x400000C`
from `docs/research/p3-vm-natives-server.md` §5 into the new document with
the real usage counts from the spec §2.

- [ ] **Step 2: Resolve default constants**

Read 8 bytes at RVAs `0x913d8`, `0x913e0`, `0x913f0`, `0x91400` from
`Server.dll` (PE section mapping). Record hex bytes and decoded double.

```powershell
& "C:\Users\volog\AppData\Local\Programs\Python\Python39\python.exe" `
  "C:\Users\volog\AppData\Local\Temp\opencode\dump_strings.py"
```

Expected: at least one of the constants is `0.0`; record all four verbatim.

- [ ] **Step 3: Record getter formulas**

For each getter in the spec §3.2, read the function body in
`.local/decomp/server/decompiled.c` and record:

- base field offset inside the copied `0x11C` record (`accessor+0x50 + k`);
- calls to `FUN_14007fb0` / `FUN_1400619c` with their constant indexes;
- equipment/skill bonuses (`accessor+0x16c..0x184`);
- clamps/min/max.

Known starting points:

| Param | Function | Base field |
|---|---|---|
| `STRENGTH` | `FUN_1400d678` | `+0x50` |
| `CONSTITUTION` | `FUN_1400d610` | `+0x54` |
| `DEXTERITY` | `FUN_1400d540` | `+0x58` |
| `PERCEPTION` | `FUN_1400d5a8` | `+0x5c` |
| `INTELLIGENCE` | `FUN_1400d4c0` | `+0x60` |
| `WISDOM` | `FUN_1400d3d8` | `+0x64` |
| `LUCK` | `FUN_1400d458` | `+0x68` |
| `REPUTATION` | `FUN_1400cc0c` | `+0x6c` |
| `EXPERIENCE` | `FUN_1400cc84` | `+0x70` |
| `LIFE` | `FUN_1400c280` out1 → vtable `+0x34` | entity `+0x14FC` |
| `MAXIMUM LIFE` | `FUN_1400c280` out2 → `FUN_1400c304` | computed |
| `ENERGY` | `FUN_1400c1fc` out1 → vtable `+0x38` | entity `+0x1500` |

Vtable: `PTR_FUN_140928c0` (RVA `0x928c0`), entries `+0x34 = 0x511d0`,
`+0x38 = 0x51294`, `+0x40 = 0x51cfc`, `+0x44 = 0x51dc0`.

- [ ] **Step 4: Record setter semantics**

For each setter in spec §3.2, read the callee and record:

- truncation/unsigned conversion of the double argument;
- what the final `1` flag means;
- which fields/derived values are recomputed;
- whether the setter writes base or effective state.

- [ ] **Step 5: Map the `BT_HERO` stat block**

Read `FUN_1401e424` (reader), `FUN_1401e690` (writer), `FUN_1401d8bc`
and record the layout:

| block offset | field |
|---|---|
| `0x000` | strength |
| `0x004` | constitution |
| `0x008` | dexterity |
| `0x00C` | perception |
| `0x010` | intelligence |
| `0x014` | wisdom |
| `0x018` | luck |
| `0x01C` | reputation |
| `0x020` | experience |

Then record the three `u32` after the block: body `0x128` = entity `+0x14FC`
(LIFE), body `0x12C` = entity `+0x1500` (ENERGY), body `0x130` =
entity `+0x1504` (purpose).

Verify on real `Saves/slot0.sav` and `slot2.sav`: decode body, print the
first 9 int32 and the three u32.

- [ ] **Step 6: Resolve skill id mapping**

Inspect `DAT_140891bc+0x78` vtable `+0x2c` (skill-name → id) and
`FUN_1400cb64` (read skill by id). Record the id order and compare with
`PersonDef.values` keys `skills.<NAME>` from a real `.scr`.
Verdict: `verified` (mapping proven) or `unverified` (then `GetSkill`
stays `unsupported`).

- [ ] **Step 7: Verify document**

Checks:

- 12 parameter rows with getter and setter RVAs;
- all four default constants with exact double values;
- `BT_HERO` offsets confirmed by both reader and writer;
- real slot0/slot2 values recorded;
- skill verdict recorded;
- UTF-8 without BOM, `nul=0`.

- [ ] **Step 8: Commit**

Only on explicit user command.

---

### Task 2: `gl_script` parameter vocabulary and handlers

**Files:**
- Create: `src/script/include/gl/script/person_access.hpp`
- Modify: `src/script/include/gl/script/native_registry.hpp`
- Modify: `src/script/src/native_registry.cpp`
- Create: `tests/script/test_person_parameters.cpp`
- Modify: `tests/script/CMakeLists.txt`
- Modify: `tests/script/test_native_registry.cpp`

**Interfaces:**
- Consumes: `gl::script::Value`.
- Produces:
  - `enum class gl::script::PersonParameter`;
  - `gl::script::person_parameter_from_name(std::string_view)`;
  - `struct gl::script::PersonAccess { get_parameter; set_parameter; get_skill; };`
  - `NativeServices::person`;
  - handlers for `0x4000000`, `0x4000001`, `0x400000C`.

- [ ] **Step 1: Write failing tests**

`tests/script/test_person_parameters.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/script/native_registry.hpp"
#include "gl/script/person_access.hpp"
#include <string>
#include <vector>

using gl::script::PersonParameter;

TEST_CASE("person parameter names match Server.dll") {
    CHECK(gl::script::person_parameter_from_name("EXPERIENCE") == PersonParameter::experience);
    CHECK(gl::script::person_parameter_from_name("REPUTATION") == PersonParameter::reputation);
    CHECK(gl::script::person_parameter_from_name("INTELLIGENCE") == PersonParameter::intelligence);
    CHECK(gl::script::person_parameter_from_name("WISDOM") == PersonParameter::wisdom);
    CHECK(gl::script::person_parameter_from_name("STRENGTH") == PersonParameter::strength);
    CHECK(gl::script::person_parameter_from_name("CONSTITUTION") == PersonParameter::constitution);
    CHECK(gl::script::person_parameter_from_name("DEXTERITY") == PersonParameter::dexterity);
    CHECK(gl::script::person_parameter_from_name("LUCK") == PersonParameter::luck);
    CHECK(gl::script::person_parameter_from_name("PERCEPTION") == PersonParameter::perception);
    CHECK(gl::script::person_parameter_from_name("MAXIMUM LIFE") == PersonParameter::maximum_life);
    CHECK(gl::script::person_parameter_from_name("LIFE") == PersonParameter::life);
    CHECK(gl::script::person_parameter_from_name("ENERGY") == PersonParameter::energy);
    CHECK(gl::script::person_parameter_from_name("REPUTATIUON") == PersonParameter::unknown);
    CHECK(gl::script::person_parameter_from_name("reputation") == PersonParameter::unknown);
    CHECK(gl::script::person_parameter_from_name("") == PersonParameter::unknown);
}

TEST_CASE("get person parameter dispatches to services") {
    gl::script::NativeServices services;
    std::string seen_person;
    PersonParameter seen_param = PersonParameter::unknown;
    services.person.get_parameter = [&](std::string_view person, PersonParameter param) {
        seen_person = std::string(person);
        seen_param = param;
        return 42.0;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value param = std::string("REPUTATION");
    const gl::script::Value args[] = {person, param};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000000), args, services);
    CHECK(seen_person == "Hero");
    CHECK(seen_param == PersonParameter::reputation);
    CHECK(std::get<double>(result) == doctest::Approx(42.0));
}

TEST_CASE("set person parameter dispatches to services") {
    gl::script::NativeServices services;
    std::string seen_person;
    PersonParameter seen_param = PersonParameter::unknown;
    double seen_value = 0.0;
    services.person.set_parameter = [&](std::string_view person, PersonParameter param, double value) {
        seen_person = std::string(person);
        seen_param = param;
        seen_value = value;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value param = std::string("LIFE");
    const gl::script::Value value = std::int32_t{7};
    const gl::script::Value args[] = {person, param, value};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000001), args, services);
    CHECK(seen_person == "Hero");
    CHECK(seen_param == PersonParameter::life);
    CHECK(seen_value == 7.0);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("set person parameter truncates fractional values to int") {
    gl::script::NativeServices services;
    double seen_value = 0.0;
    services.person.set_parameter = [&](std::string_view, PersonParameter, double value) {
        seen_value = value;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value param = std::string("LIFE");
    const gl::script::Value value = 7.9;
    const gl::script::Value args[] = {person, param, value};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000001), args, services);
    CHECK(seen_value == 7.0);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("set person parameter rejects a non-numeric value") {
    gl::script::NativeServices services;
    bool called = false;
    std::vector<std::string> messages;
    services.person.set_parameter = [&](std::string_view, PersonParameter, double) {
        called = true;
    };
    services.log = [&](std::string_view text) { messages.emplace_back(text); };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value param = std::string("LIFE");
    const gl::script::Value value = std::string("seven");
    const gl::script::Value args[] = {person, param, value};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000001), args, services);
    CHECK_FALSE(called);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("invalid") != std::string::npos);
}

TEST_CASE("person native registry arities match Server.dll") {
    CHECK(gl::script::NativeRegistry::find(0x4000000)->arity == 2);
    CHECK(gl::script::NativeRegistry::find(0x4000001)->arity == 3);
    CHECK(gl::script::NativeRegistry::find(0x400000C)->arity == 2);
}

TEST_CASE("get person skill dispatches to services") {
    gl::script::NativeServices services;
    std::string seen_skill;
    services.person.get_skill = [&](std::string_view, std::string_view skill) {
        seen_skill = std::string(skill);
        return 3.0;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value skill = std::string("SKILL_WPN_AXE");
    const gl::script::Value args[] = {person, skill};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x400000C), args, services);
    CHECK(seen_skill == "SKILL_WPN_AXE");
    CHECK(std::get<double>(result) == doctest::Approx(3.0));
}

TEST_CASE("person native without services returns zero") {
    gl::script::NativeServices services;
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value param = std::string("REPUTATION");
    const gl::script::Value args[] = {person, param};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000000), args, services);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("person native rejects wrong arguments") {
    gl::script::NativeServices services;
    const gl::script::Value only = std::string("Hero");
    const gl::script::Value args[] = {only};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000000), args, services);
    CHECK(std::get<std::int32_t>(result) == 0);
}
```

- [ ] **Step 2: Verify tests fail**

```powershell
cmake --build build
build\bin\gl_script_tests.exe --test-case=*person*
```

Expected: build failure — `person_access.hpp` missing.

- [ ] **Step 3: Write `person_access.hpp`**

```cpp
#pragma once
#include <cstdint>
#include <functional>
#include <string_view>

namespace gl::script {
enum class PersonParameter : std::uint8_t {
    experience,
    reputation,
    intelligence,
    wisdom,
    strength,
    constitution,
    dexterity,
    luck,
    perception,
    maximum_life,
    life,
    energy,
    unknown,
};

struct PersonAccess {
    std::function<double(std::string_view person, PersonParameter parameter)> get_parameter;
    std::function<void(std::string_view person, PersonParameter parameter, double value)>
        set_parameter;
    std::function<double(std::string_view person, std::string_view skill)> get_skill;
};

PersonParameter person_parameter_from_name(std::string_view name);
} // namespace gl::script
```

- [ ] **Step 4: Wire `NativeServices` and handlers**

Add `#include "gl/script/person_access.hpp"` and `PersonAccess person;` to
`NativeServices` in `native_registry.hpp`.

In `native_registry.cpp`:

```cpp
namespace {
std::int32_t as_int(const Value& value) {
    if (const auto* item = std::get_if<std::int32_t>(&value)) return *item;
    if (const auto* item = std::get_if<std::uint32_t>(&value))
        return static_cast<std::int32_t>(*item);
    if (const auto* item = std::get_if<double>(&value)) {
        if (!std::isfinite(*item)) return 0;
        if (*item >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
            return std::numeric_limits<std::int32_t>::max();
        if (*item <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))
            return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(*item);
    }
    return 0;
}

std::string_view as_string(const Value& value) {
    if (const auto* item = std::get_if<std::string>(&value)) return *item;
    return {};
}

bool is_string(const Value& value) {
    return std::holds_alternative<std::string>(value);
}

bool is_numeric(const Value& value) {
    return std::holds_alternative<std::int32_t>(value) ||
           std::holds_alternative<std::uint32_t>(value) ||
           std::holds_alternative<double>(value);
}

void warn_native(std::string_view text, NativeServices& services) {
    if (services.log) services.log(text);
}

Value native_get_person_parameter(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 2 || !is_string(args[0]) || !is_string(args[1])) {
        warn_native("script native RS_GetPersonParameterI: invalid arguments", services);
        return double{0.0};
    }
    const auto parameter = person_parameter_from_name(as_string(args[1]));
    if (!services.person.get_parameter) return double{0.0};
    return services.person.get_parameter(as_string(args[0]), parameter);
}

Value native_set_person_parameter(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 3 || !is_string(args[0]) || !is_string(args[1]) ||
        !is_numeric(args[2])) {
        warn_native("script native RS_SetPersonParameterI: invalid arguments", services);
        return double{0.0};
    }
    const auto parameter = person_parameter_from_name(as_string(args[1]));
    if (services.person.set_parameter)
        services.person.set_parameter(as_string(args[0]), parameter,
                                      static_cast<double>(as_int(args[2])));
    return double{0.0};
}

Value native_get_person_skill(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 2 || !is_string(args[0]) || !is_string(args[1])) {
        warn_native("script native RS_GetPersonSkillI: invalid arguments", services);
        return double{0.0};
    }
    if (!services.person.get_skill) return double{0.0};
    return services.person.get_skill(as_string(args[0]), as_string(args[1]));
}
} // namespace
```

Change the registry entries for `0x4000000`, `0x4000001`, `0x400000C` to
`NativeSupport::supported` with the corresponding handlers and arity
`2`, `3`, `2` (research §2). The setter rejects a non-numeric `args[2]`
with a warning and returns `double{0.0}`; numeric values are truncated to
`int32` before dispatch (VM behavior, research §2).

- [ ] **Step 5: Implement `person_parameter_from_name`**

```cpp
PersonParameter person_parameter_from_name(std::string_view name) {
    if (name == "EXPERIENCE") return PersonParameter::experience;
    if (name == "REPUTATION") return PersonParameter::reputation;
    if (name == "INTELLIGENCE") return PersonParameter::intelligence;
    if (name == "WISDOM") return PersonParameter::wisdom;
    if (name == "STRENGTH") return PersonParameter::strength;
    if (name == "CONSTITUTION") return PersonParameter::constitution;
    if (name == "DEXTERITY") return PersonParameter::dexterity;
    if (name == "LUCK") return PersonParameter::luck;
    if (name == "PERCEPTION") return PersonParameter::perception;
    if (name == "MAXIMUM LIFE") return PersonParameter::maximum_life;
    if (name == "LIFE") return PersonParameter::life;
    if (name == "ENERGY") return PersonParameter::energy;
    return PersonParameter::unknown;
}
```

- [ ] **Step 6: Update the registry test**

In `tests/script/test_native_registry.cpp`, change the expectation for
`0x4000001` from `unsupported` to `supported`, and add
`0x4000000`/`0x400000C` to the supported set.

- [ ] **Step 7: Register the new test**

Add `test_person_parameters.cpp` to `tests/script/CMakeLists.txt`.

- [ ] **Step 8: Run the tests**

```powershell
cmake --build build
build\bin\gl_script_tests.exe --test-case=*person*
build\bin\gl_script_tests.exe
```

Expected: all person tests pass; registry still has 65 entries.

- [ ] **Step 9: Commit**

Only on explicit user command.

---

### Task 3: `gl_assets::parse_hero_stats`

**Files:**
- Create: `src/assets/include/gl/assets/hero_stats.hpp`
- Create: `src/assets/src/hero_stats.cpp`
- Modify: `src/assets/CMakeLists.txt`
- Create: `tests/assets/test_hero_stats.cpp`
- Modify: `tests/assets/CMakeLists.txt`
- Modify: `tests/assets/test_hero_equipment_real.cpp` (add real stat assertions)

**Interfaces:**
- Consumes: decompressed `BT_HERO` body from `decode_save_body`.
- Produces:

```cpp
namespace gl::assets {
struct HeroStats {
    std::int32_t strength = 0;
    std::int32_t constitution = 0;
    std::int32_t dexterity = 0;
    std::int32_t perception = 0;
    std::int32_t intelligence = 0;
    std::int32_t wisdom = 0;
    std::int32_t luck = 0;
    std::int32_t reputation = 0;
    std::int32_t experience = 0;
    std::int32_t life = 0;
    std::int32_t energy = 0;
    std::int32_t extra = 0;
};
Result<HeroStats> parse_hero_stats(std::span<const std::uint8_t> body);
} // namespace gl::assets
```

- [ ] **Step 1: Write the failing synthetic test**

`tests/assets/test_hero_stats.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/hero_stats.hpp"
#include <array>
#include <cstdint>

namespace {
void put_i32(std::vector<std::uint8_t>& body, std::size_t offset, std::int32_t value) {
    for (int i = 0; i < 4; ++i)
        body[offset + i] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(value) >> (i * 8)) & 0xFF);
}
} // namespace

TEST_CASE("hero stats use the BT_HERO stat block") {
    std::vector<std::uint8_t> body(0x134, 0);
    put_i32(body, 0x00C + 0x00, 3);
    put_i32(body, 0x00C + 0x04, 8);
    put_i32(body, 0x00C + 0x08, 7);
    put_i32(body, 0x00C + 0x0C, 15);
    put_i32(body, 0x00C + 0x10, 13);
    put_i32(body, 0x00C + 0x14, 8);
    put_i32(body, 0x00C + 0x18, 7);
    put_i32(body, 0x00C + 0x1C, 11);
    put_i32(body, 0x00C + 0x20, 1234);
    put_i32(body, 0x128, 263);
    put_i32(body, 0x12C, 5);
    put_i32(body, 0x130, 99);

    const auto stats = gl::assets::parse_hero_stats(body);
    REQUIRE(stats);
    CHECK(stats->strength == 3);
    CHECK(stats->constitution == 8);
    CHECK(stats->dexterity == 7);
    CHECK(stats->perception == 15);
    CHECK(stats->intelligence == 13);
    CHECK(stats->wisdom == 8);
    CHECK(stats->luck == 7);
    CHECK(stats->reputation == 11);
    CHECK(stats->experience == 1234);
    CHECK(stats->life == 263);
    CHECK(stats->energy == 5);
    CHECK(stats->extra == 99);
}

TEST_CASE("hero stats reject a short body") {
    const std::array<std::uint8_t, 16> body{};
    CHECK_FALSE(gl::assets::parse_hero_stats(body));
}
```

- [ ] **Step 2: Verify it fails**

`cmake --build build` — expected: `hero_stats.hpp` missing.

- [ ] **Step 3: Implement**

`hero_stats.cpp`:

```cpp
#include "gl/assets/hero_stats.hpp"
#include <string>

namespace gl::assets {
namespace {
std::int32_t read_i32(std::span<const std::uint8_t> data, std::size_t offset) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(data[offset + i]) << (i * 8);
    return static_cast<std::int32_t>(value);
}
} // namespace

Result<HeroStats> parse_hero_stats(std::span<const std::uint8_t> body) {
    if (body.size() < 0x134)
        return Error{ErrorCode::format, "hero stat block is too short"};
    HeroStats stats;
    stats.strength = read_i32(body, 0x00C + 0x00);
    stats.constitution = read_i32(body, 0x00C + 0x04);
    stats.dexterity = read_i32(body, 0x00C + 0x08);
    stats.perception = read_i32(body, 0x00C + 0x0C);
    stats.intelligence = read_i32(body, 0x00C + 0x10);
    stats.wisdom = read_i32(body, 0x00C + 0x14);
    stats.luck = read_i32(body, 0x00C + 0x18);
    stats.reputation = read_i32(body, 0x00C + 0x1C);
    stats.experience = read_i32(body, 0x00C + 0x20);
    stats.life = read_i32(body, 0x128);
    stats.energy = read_i32(body, 0x12C);
    stats.extra = read_i32(body, 0x130);
    return stats;
}
} // namespace gl::assets
```

- [ ] **Step 4: Add a real-save assertion**

In `tests/assets/test_hero_equipment_real.cpp` add a test that decodes
`Saves/slot0.sav`, calls `parse_hero_stats`, and compares each field with
the values recorded in research `§hero_block` real-save table.

- [ ] **Step 5: Run the tests**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_assets_tests.exe --test-case=*hero_stats*
build\bin\gl_assets_real_tests.exe --test-case=*hero_stats*
```

Expected: pass on synthetic and real data.

- [ ] **Step 6: Commit**

Only on explicit user command.

---

### Task 4: `gl_game::PersonRuntime`

**Files:**
- Create: `src/game/include/gl/game/person_runtime.hpp`
- Create: `src/game/src/person_runtime.cpp`
- Modify: `src/game/CMakeLists.txt`
- Create: `tests/game/test_person_runtime.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Consumes: `gl::game::PersonInstance`, `gl::assets::HeroStats`,
  `gl::script::PersonAccess`, formulas from research `§params`.
- Produces:

```cpp
namespace gl::game {
class PersonRuntime {
  public:
    static PersonRuntime create(const std::vector<PersonInstance>& persons,
                                const std::optional<gl::assets::HeroStats>& hero);
    double get_parameter(std::string_view person,
                         gl::script::PersonParameter parameter) const;
    void set_parameter(std::string_view person,
                       gl::script::PersonParameter parameter, double value);
    double get_skill(std::string_view person, std::string_view skill) const;
    gl::script::PersonAccess access();
};
} // namespace gl::game
```

- [ ] **Step 1: Write failing tests**

`tests/game/test_person_runtime.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/game/person_runtime.hpp"
#include "gl/game/person.hpp"
#include <string>

namespace {
gl::game::PersonInstance make_person(std::string name, std::string res) {
    gl::game::PersonInstance person;
    person.name = std::move(name);
    person.res_name = res;
    person.def.res_name = res;
    person.def.values["strength"] = "10";
    person.def.values["constitution"] = "8";
    person.def.values["dexterity"] = "7";
    person.def.values["perception"] = "6";
    person.def.values["intelligence"] = "5";
    person.def.values["wisdom"] = "4";
    person.def.values["luck"] = "3";
    person.def.values["reputation"] = "2";
    person.def.values["experience_value"] = "0";
    person.def.values["skills.SKILL_WPN_AXE"] = "9";
    return person;
}
} // namespace

TEST_CASE("person runtime finds NPCs by exact name") {
    const std::vector<gl::game::PersonInstance> persons{make_person("L1.P1", "L1.P1")};
    auto runtime = gl::game::PersonRuntime::create(persons, std::nullopt);
    CHECK(runtime.get_parameter("L1.P1", gl::script::PersonParameter::strength) != 0.0);
    CHECK(runtime.get_parameter("l1.p1", gl::script::PersonParameter::strength) == 0.0);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::strength) == 0.0);
}

TEST_CASE("person runtime keeps the hero entry without save") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::reputation) == 0.0);
}

TEST_CASE("person runtime applies save hero stats") {
    gl::assets::HeroStats hero;
    hero.reputation = 11;
    hero.experience = 1234;
    hero.life = 263;
    auto runtime = gl::game::PersonRuntime::create({}, hero);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::reputation) != 0.0);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::experience) == doctest::Approx(1234.0));
}

TEST_CASE("person runtime set/get roundtrip") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    runtime.set_parameter("L1.P1", gl::script::PersonParameter::life, 7.0);
    CHECK(runtime.get_parameter("L1.P1", gl::script::PersonParameter::life) == doctest::Approx(7.0));
}

TEST_CASE("person runtime reads skills") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    CHECK(runtime.get_skill("L1.P1", "SKILL_WPN_AXE") == doctest::Approx(9.0));
    CHECK(runtime.get_skill("L1.P1", "SKILL_UNKNOWN") == doctest::Approx(0.0));
}

TEST_CASE("person runtime exposes access callbacks") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    auto access = runtime.access();
    REQUIRE(access.get_parameter);
    CHECK(access.get_parameter("L1.P1", gl::script::PersonParameter::strength) > 0.0);
}
```

- [ ] **Step 2: Verify it fails**

`cmake --build build` — expected: `person_runtime.hpp` missing.

- [ ] **Step 3: Implement the state and lookup**

```cpp
struct PersonState {
    std::string name;
    std::array<double, 12> values{};
    std::map<std::string, double, std::less<>> skills;
};

class PersonRuntime {
  public:
    static PersonRuntime create(const std::vector<PersonInstance>& persons,
                                const std::optional<gl::assets::HeroStats>& hero);
    // ...
  private:
    std::vector<PersonState> persons_;
    std::map<std::string, std::size_t, std::less<>> index_;
};
```

- `create`: reserve `persons.size() + 1`; add Hero first with stats from
  `hero` (all zeros when absent); then each `PersonInstance` using
  `def.values`. Parse numbers with `std::from_chars`; missing → 0.
  `index_.emplace(name, size)` keeps the first duplicate (warning once).
- `get_parameter`: exact `index_.find`; not found or `unknown` → 0.
- `set_parameter`: exact `index_.find`; `unknown` or `maximum_life` →
  no-op (setter has no branch); otherwise store the value.
- `get_skill`: exact person lookup; `skills` map exact key; missing → 0.
- `access`: return lambdas capturing `this`.

- [ ] **Step 4: Apply Server.dll getter formulas**

Transcribe the formulas from research `§params` into private helpers
(`strength_value`, `reputation_value`, `maximum_life_value`, …). Each
helper takes the raw `PersonState` values and returns the effective double
exactly as the corresponding getter does, including clamps/min/max and
equipment/skill bonuses where the research says so. NPC raw values come
from `.scr`; Hero raw values come from `HeroStats`.

- [ ] **Step 5: Run the tests**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case=*person_runtime*
```

Expected: all cases pass.

- [ ] **Step 6: Real-data test**

Add `tests/game/test_person_runtime_real.cpp`:
- use `doctest::skip(!game_dir_available())` like
  `tests/game/test_real_person_loader.cpp` (the binary's `main` returns 77
  when `GL_GAME_DIR` is unset);
- load `l1_1` persons, pick one, compare `get_parameter` for all 9 base
  stats with direct `.scr` values transformed by the researched formula;
- load `slot0.sav`, call `parse_hero_stats`, build runtime, compare Hero
  values with research `§hero_block` expected values.

- [ ] **Step 7: Register tests**

Add both test files to `tests/game/CMakeLists.txt`.

- [ ] **Step 8: Commit**

Only on explicit user command.

---

### Task 5: Wire `PersonRuntime` into tools

**Files:**
- Modify: `tools/glgame/main.cpp`
- Modify: `tools/glscript/main.cpp` (only comments/no-op wiring if needed)
- Create: `tests/script/test_real_person_natives.cpp` (real dialog branch)
- Modify: `tests/script/CMakeLists.txt`

**Interfaces:**
- Consumes: `gl::game::PersonRuntime`, `gl::game::GameScriptHost`,
  `gl::assets::parse_hero_stats`.
- Produces: `glgame` dialogs see real person parameters; `glscript` keeps
  empty callbacks.

- [ ] **Step 1: Track hero stats in `Game`**

Add to `Game`:

```cpp
gl::game::PersonRuntime person_runtime_;
std::optional<gl::assets::HeroStats> pending_hero_stats_;
```

In `apply_equip_save`, after decoding `body`:

```cpp
auto hero_stats = gl::assets::parse_hero_stats(body);
if (hero_stats)
    pending_hero_stats_ = hero_stats.value();
else
    gl::log(gl::LogLevel::warning, "hero stats: " + hero_stats.error().message);
```

- [ ] **Step 2: Build the runtime when the level loads**

At the end of `load_level` (after `level_` is assigned):

```cpp
person_runtime_ = gl::game::PersonRuntime::create(level_->level().persons,
                                                  pending_hero_stats_);
```

When no level is loaded (dialog-only mode), build from `{}` and
`pending_hero_stats_`.

- [ ] **Step 3: Wire callbacks in `open_dialog`**

After `context->host.services().exit = ...` add:

```cpp
context->host.services().person = person_runtime_.access();
```

`PersonRuntime::access()` lambdas capture `this`; ensure
`person_runtime_` outlives the dialog (it is a `Game` member).

- [ ] **Step 4: Write a real dialog branch test**

`tests/script/test_real_person_natives.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/cs.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/game_script_host.hpp"
#include "gl/game/person_runtime.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"
#include <cstdlib>
#include <string>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
} // namespace

TEST_CASE("real dialog reads Hero reputation" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto script = vfs->read("scripts/dialogs/inter_kid3.age.cs");
    auto globals_data = vfs->read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(script.has_value());
    REQUIRE(globals_data.has_value());
    auto nodes = gl::assets::parse_cs(script.value());
    auto globals = gl::script::parse_globals(globals_data.value());
    REQUIRE(nodes.has_value());
    REQUIRE(globals.has_value());

    gl::game::PersonRuntime runtime = gl::game::PersonRuntime::create({}, std::nullopt);
    gl::script::NativeServices services;
    services.person = runtime.access();
    gl::game::GameScriptHost host(services);

    runtime.set_parameter("Hero", gl::script::PersonParameter::reputation, 777.0);
    gl::script::ScriptVm vm(std::move(nodes.value()), host);
    vm.set_globals(globals.value());

    for (int i = 0; i < 2000 && vm.status() != gl::script::VmStatus::wait_user; ++i) {
        if (!vm.step()) break;
        const gl::script::Value *slava = vm.variable("slava");
        if (slava != nullptr) {
            if (const auto *number = std::get_if<std::int32_t>(slava);
                number != nullptr && *number == 777)
                break;
        }
    }
    const gl::script::Value *slava = vm.variable("slava");
    REQUIRE(slava != nullptr);
    CHECK(std::get<std::int32_t>(*slava) == 777);
}
```

`inter_kid3.age.cs` node 351 assigns `RS_GetPersonParameterI("Hero",
"REPUTATION")` to `slava`; the test proves the real script reaches the
same `PersonAccess` path as `glgame`.

- [ ] **Step 5: Run the CLI smoke tests**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\glscript.exe --game-dir "I:\Games\Златогорье-2" --file scripts/dialogs/demon.d1.age.cs --run
build\bin\glgame.exe --game-dir "I:\Games\Златогорье-2" --level l1_1 --click-npc 13 --screenshot "$env:TEMP\p3b1.png"
```

Expected: `glscript` output unchanged (`wait_user`, 43 steps, 3 dialogs);
`glgame` exits 0 and writes a screenshot.

- [ ] **Step 6: Commit**

Only on explicit user command.

---

### Task 6: Regression, docs, status

**Files:**
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`
- Modify: `docs/research/parity-matrix.md`

- [ ] **Step 1: Run targeted tests**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure -R "gl_script_tests|gl_script_real_tests|gl_game_tests|gl_assets_tests|gl_assets_real_tests|glgame_screenshot_tests"
```

Record exact counts/times.

- [ ] **Step 2: Run the full suite**

```powershell
ctest --test-dir build --output-on-failure
```

Record `N/N passed`, duration, exit code.

- [ ] **Step 3: Update `parity-matrix.md`**

- `P3-9`: supported natives `5 → 8` if skill mapping is `verified`, else
  `5 → 7`; list the three IDs and the remaining reason.
- Add a line for person parameter runtime/hero stats.

- [ ] **Step 4: Update `STATUS.md`**

Add a Phase 1b1 section: task, criteria, what was done, exact verification
output, next step (1b2 inventory or `P2-15`).

- [ ] **Step 5: Update `PROJECT.md`**

Update the P3 row: person parameters, Hero stat block, remaining P3-9
natives.

- [ ] **Step 6: Verify encoding**

```powershell
$files = 'docs\STATUS.md','docs\PROJECT.md','docs\research\parity-matrix.md','docs\research\p3b1-person-parameters-server.md'
foreach ($f in $files) {
  $b = [System.IO.File]::ReadAllBytes($f)
  "$f bom=$($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF) nul=$($b -contains 0)"
}
```

Expected: `bom=False nul=False`.

- [ ] **Step 7: Commit**

Only on explicit user command.
