# P6a12 — Location access native implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `RS_SetLocationAccess` (0x7000008) through a
`ScriptVm` global bridge.

**Architecture:** `GlobalAccess` in `NativeServices` writes a double to
the active `ScriptVm`; the native builds `name_state`, converts the
argument as uint32 and returns `0.0`.

**Tech Stack:** C++20, CMake/Ninja, doctest; no SDL in script/game.

## Global Constraints

- Layers: `app/tools → game/script/assets`; no SDL in script/game.
- Names/types as in `AGENTS.md`; no unnecessary comments.
- Docs UTF-8 without BOM via `write`/`edit`.
- Full check with `GL_GAME_DIR`.

---

### Task 1: VM global bridge

**Files:**
- Modify: `src/script/include/gl/script/script_vm.hpp`
- Modify: `src/script/src/script_vm.cpp`
- Test: `tests/script/test_vm.cpp`

**Interfaces:**
- Produces: `bool ScriptVm::set_global(std::string_view, Value)` (update-only).

- [x] **Step 1: Write the failing test**

Add to `tests/script/test_vm.cpp`:

```cpp
namespace {
struct NullHost final : gl::script::ScriptHost {
    gl::script::Value call(std::uint32_t, std::span<const gl::script::Value>) override {
        return std::int32_t{0};
    }
};
} // namespace

TEST_CASE("set_global updates declared globals only") {
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    gl::script::ScriptGlobal declared;
    declared.name = "L96_state";
    declared.value = std::int32_t{0};
    vm.set_globals(std::span<const gl::script::ScriptGlobal>(&declared, 1));

    CHECK(vm.set_global("L96_state", gl::script::Value{double{2.0}}));
    CHECK(std::get<double>(vm.globals().at("L96_state")) == doctest::Approx(2.0));
    CHECK_FALSE(vm.set_global("unknown_state", gl::script::Value{double{3.0}}));
    CHECK(vm.globals().count("unknown_state") == 0);
}
```

- [x] **Step 2: Implement**

`script_vm.hpp` public:

```cpp
bool set_global(std::string_view name, Value value);
```

`script_vm.cpp`:

```cpp
bool ScriptVm::set_global(std::string_view name, Value value) {
    const std::string key(name);
    auto global = globals_.find(key);
    if (global == globals_.end())
        return false;
    global->second = value;
    auto variable = variables_.find(key);
    if (variable != variables_.end())
        variable->second = value;
    return true;
}
```

- [x] **Step 3: Run tests**

`cmake --build build; build\bin\gl_script_tests.exe --test-case="*set_global*"`

### Task 2: Native and access

**Files:**
- Create: `src/script/include/gl/script/global_access.hpp`
- Modify: `src/script/include/gl/script/native_registry.hpp`
- Modify: `src/script/src/native_registry.cpp`
- Test: `tests/game/test_location_access.cpp` (new)
- Modify: `tests/game/CMakeLists.txt`
- Modify: count tests (`test_inventory_mutations.cpp`,
  `test_trade_native.cpp`, `test_transfer_natives.cpp`)

**Interfaces:**
- Produces: `GlobalAccess::set_double` and `NativeServices::global`.

- [x] **Step 1: Write the failing test**

`test_location_access.cpp`:

```cpp
#include "gl/script/native_registry.hpp"
#include <doctest/doctest.h>
#include <string>
#include <vector>

TEST_CASE("set-location-access native writes the state global") {
    gl::script::NativeServices services;
    std::string key;
    double value = 0.0;
    services.global.set_double = [&](std::string_view name, double v) {
        key = std::string(name);
        value = v;
    };
    const std::vector<gl::script::Value> args{std::string{"L96"}, std::int32_t{2}};
    const auto *info = gl::script::NativeRegistry::find(0x7000008);
    REQUIRE(info != nullptr);
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 2);
    CHECK(gl::script::NativeRegistry::call(*info, args, services) ==
          gl::script::Value{double{0.0}});
    CHECK(key == "L96_state");
    CHECK(value == doctest::Approx(2.0));

    const std::vector<gl::script::Value> negative{std::string{"L96"}, std::int32_t{-1}};
    gl::script::NativeRegistry::call(*info, negative, services);
    CHECK(value == doctest::Approx(4294967295.0));

    key.clear();
    const std::vector<gl::script::Value> bad{std::string{"L96"}, std::string{"2"}};
    gl::script::NativeRegistry::call(*info, bad, services);
    CHECK(key.empty());
}
```

- [x] **Step 2: Implement**

`global_access.hpp`:

```cpp
#pragma once
#include <functional>
#include <string_view>
namespace gl::script {
struct GlobalAccess {
    std::function<void(std::string_view, double)> set_double;
};
}
```

Add `GlobalAccess global;` to `NativeServices`.

Handler:

```cpp
Value native_set_location_access(std::span<const Value> args, NativeServices &services) {
    if (!services.global.set_double || !is_string(args[0]) || !is_int(args[1]))
        return double{0.0};
    std::string key(as_string(args[0]));
    key += "_state";
    const auto raw = static_cast<std::uint32_t>(as_int(args[1]));
    services.global.set_double(key, static_cast<double>(raw));
    return double{0.0};
}
```

Registry: `add(0x7000008, NativeSupport::supported, {},
&native_set_location_access, 2);`

Update the three supported-count tests to 36/65.

- [x] **Step 3: Run tests**

`cmake --build build; build\bin\gl_game_tests.exe --test-case="*location*"; build\bin\gl_script_tests.exe`

### Task 3: Wire glgame and glscript

**Files:**
- Modify: `tools/glgame/main.cpp`
- Modify: `tools/glscript/main.cpp`

- [x] **Step 1: Implement**

glgame, after `context->vm` creation:

```cpp
auto *active_vm = context->vm.get();
context->host.services().global.set_double =
    [this, active_vm](std::string_view name, double value) {
        const gl::script::Value written{double{value}};
        if (active_vm->set_global(name, written))
            runtime_globals_[std::string(name)] = written;
    };
for (const auto &[name, value] : runtime_globals_)
    context->vm->set_global(name, value);
```

glscript, after its `ScriptVm vm(...)`:

```cpp
host.services().global.set_double = [&vm](std::string_view name, double value) {
    vm.set_global(name, value);
};
```

- [x] **Step 2: Build**

`cmake --build build`

### Task 4: Docs and full verification

**Files:**
- Modify: `docs/DECISIONS.md` (D-108)
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`
- Modify: `docs/research/p6a4-location-access.md` (unblock note)
- Modify: `docs/research/p3-vm-natives-server.md` row `0x7000008`

- [x] **Step 1: Docs**
  - D-108: native contract, VM bridge, 68 real calls, residuals.
  - STATUS: P6a12 done with commands/output.
  - PROJECT: P6a 36/65 and location access done.
  - `p3-vm-natives-server.md` row `0x7000008`: supported D-108.
  - `p6a4-location-access.md`: unblock note pointing to p6a12.
- [x] **Step 2: Full check**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: registry 36/65, location tests pass, full `ctest` green.

## Self-review

- Spec coverage: bridge, native, wiring, tests, docs.
- No placeholders.
- Types consistent (`set_double`, `set_global`, `GlobalAccess`).
