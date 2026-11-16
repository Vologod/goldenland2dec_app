# P2-15b2 — Restore Script Globals from `BT_VARS` Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Apply saved `BT_VARS` script globals to the CS VM on save load by overlaying the declared `zlato_vars.scr` values, with declared-type coercion and unknown-name insertion.

**Architecture:** `assets::save_var_number` exposes the saved f64; `script::save_globals_from_vars` converts `SaveVar` into `ScriptGlobal`; `ScriptVm::apply_saved_globals` overlays them onto `globals_` using the existing VM coercion helpers and rebuilds node variables; `glgame` keeps the overlay across dialogs, applies it after every `set_globals`, rolls it back on failed loads, and clears it for New Game.

**Tech Stack:** C++20, CMake + Ninja, doctest, SDL2 project layers (`assets` → `script`/`game` → `tools/glgame`).

**Spec:** `docs/specs/2026-11-16-p2-15b2-bt-vars-globals-design.md`.

## Global Constraints

- C++20; CMake ≥ 3.20 + Ninja; doctest; no new dependency.
- Layers downward; `script` may depend on `assets`; `assets` must not depend on `script`.
- No comments in code unless they carry non-obvious parity information.
- Docs UTF-8 no BOM; no NUL/literal `\n` artifacts.
- Real tests skip when `GL_GAME_DIR` is unset.
- OQ-5 conversion policy is documented as a chosen policy, not claimed original parity.
- Toolchain for every build/test command:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

---

### Task 1: `assets::save_var_number`

**Files:**
- Modify: `src/assets/include/gl/assets/save_payload.hpp`
- Modify: `src/assets/src/save_payload.cpp`
- Test: `tests/assets/test_save_payload.cpp`

**Interfaces:**
- Produces: `std::optional<double> gl::assets::save_var_number(const SaveVar &var)`
  (`type < 3`, f64 little-endian; `nullopt` for type 3).

- [ ] **Step 1: Add the failing test**

Append to `tests/assets/test_save_payload.cpp`:

```cpp
TEST_CASE("save_var_number decodes the little-endian f64 value") {
    gl::assets::SaveVar var;
    const double value = 10.5;
    std::memcpy(var.value.data(), &value, sizeof(value));
    var.type = 0;
    const auto number = gl::assets::save_var_number(var);
    REQUIRE(number.has_value());
    CHECK(*number == value);

    var.type = 3;
    CHECK_FALSE(gl::assets::save_var_number(var).has_value());
}
```

Add `#include <cstring>` if missing.

- [ ] **Step 2: Run to verify failure**

```powershell
cmake --build build --target gl_assets_tests
```

Expected: compile failure (`save_var_number` missing).

- [ ] **Step 3: Implement**

In `save_payload.hpp` after `SaveVar`:

```cpp
std::optional<double> save_var_number(const SaveVar &var);
```

In `save_payload.cpp` (add `<bit>` include):

```cpp
std::optional<double> save_var_number(const SaveVar &var) {
    if (var.type >= 3)
        return std::nullopt;
    std::uint64_t bits = 0;
    std::memcpy(&bits, var.value.data(), sizeof(bits));
    return std::bit_cast<double>(bits);
}
```

- [ ] **Step 4: Run**

```powershell
cmake --build build --target gl_assets_tests
build\bin\gl_assets_tests.exe --test-case="*save_var_number*"
```

Expected: pass.

- [ ] **Step 5: Commit**

```powershell
git add src/assets/include/gl/assets/save_payload.hpp src/assets/src/save_payload.cpp tests/assets/test_save_payload.cpp
git commit -m "feat(assets): expose saved global numbers"
```

---

### Task 2: Script overlay API and adapter

**Files:**
- Modify: `src/script/include/gl/script/globals.hpp`
- Modify: `src/script/src/globals.cpp`
- Modify: `src/script/include/gl/script/script_vm.hpp`
- Modify: `src/script/src/script_vm.cpp`
- Create: `tests/script/test_saved_globals.cpp`
- Modify: `tests/script/CMakeLists.txt`

**Interfaces:**
- Produces: `std::vector<ScriptGlobal> save_globals_from_vars(std::span<const gl::assets::SaveVar>)`;
  `ScriptVm::GlobalApplyCounts {matched, added, kept}`;
  `GlobalApplyCounts ScriptVm::apply_saved_globals(std::span<const ScriptGlobal>)`;
  `const std::map<std::string, Value>& ScriptVm::globals() const`.

- [ ] **Step 1: Write the failing unit tests**

Create `tests/script/test_saved_globals.cpp`:

```cpp
#include "gl/assets/cs.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
struct NullHost : gl::script::ScriptHost {
    gl::script::Value call(std::uint32_t, std::span<const gl::script::Value>) override {
        return std::int32_t{0};
    }
};

gl::script::ScriptGlobal global(std::string name, gl::script::Value value) {
    gl::script::ScriptGlobal out;
    out.name = std::move(name);
    out.value = std::move(value);
    return out;
}

gl::assets::CsNode variable_node(std::string name) {
    gl::assets::CsNode node;
    node.opcode = 23;
    node.text = std::move(name);
    return node;
}
} // namespace

TEST_CASE("saved globals coerce to the declared variant type") {
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    const std::vector<gl::script::ScriptGlobal> initial{
        global("i", std::int32_t{1}),
        global("d", std::uint32_t{2}),
        global("f", 0.5),
        global("s", std::string{"a"}),
    };
    vm.set_globals(initial);

    const std::vector<gl::script::ScriptGlobal> overlay{
        global("i", 7.9),
        global("d", -1.0),
        global("f", 2.25),
        global("s", std::string{"b"}),
    };
    const auto counts = vm.apply_saved_globals(overlay);
    CHECK(counts.matched == 4);
    CHECK(counts.added == 0);
    CHECK(counts.kept == 0);
    CHECK(std::get<std::int32_t>(vm.globals().at("i")) == 7);
    CHECK(std::get<std::uint32_t>(vm.globals().at("d")) == 0u);
    CHECK(std::get<double>(vm.globals().at("f")) == 2.25);
    CHECK(std::get<std::string>(vm.globals().at("s")) == "b");
}

TEST_CASE("saved globals add unknown names and keep type mismatches") {
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    const std::vector<gl::script::ScriptGlobal> initial{
        global("i", std::int32_t{1}),
        global("s", std::string{"a"}),
    };
    vm.set_globals(initial);

    const std::vector<gl::script::ScriptGlobal> overlay{
        global("new_number", 3.5),
        global("new_text", std::string{"x"}),
        global("i", std::string{"bad"}),
        global("s", 4.0),
    };
    const auto counts = vm.apply_saved_globals(overlay);
    CHECK(counts.matched == 0);
    CHECK(counts.added == 2);
    CHECK(counts.kept == 2);
    CHECK(std::get<double>(vm.globals().at("new_number")) == 3.5);
    CHECK(std::get<std::string>(vm.globals().at("new_text")) == "x");
    CHECK(std::get<std::int32_t>(vm.globals().at("i")) == 1);
    CHECK(std::get<std::string>(vm.globals().at("s")) == "a");
}

TEST_CASE("saved globals use the last duplicate value") {
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    const std::vector<gl::script::ScriptGlobal> initial{global("i", std::int32_t{1})};
    vm.set_globals(initial);
    const std::vector<gl::script::ScriptGlobal> overlay{
        global("i", 2.0),
        global("i", 3.0),
    };
    vm.apply_saved_globals(overlay);
    CHECK(std::get<std::int32_t>(vm.globals().at("i")) == 3);
}

TEST_CASE("saved globals rebuild node variables") {
    NullHost host;
    std::vector<gl::assets::CsNode> nodes{variable_node("g")};
    gl::script::ScriptVm vm(std::move(nodes), host);
    const std::vector<gl::script::ScriptGlobal> initial{global("g", std::int32_t{1})};
    vm.set_globals(initial);
    const std::vector<gl::script::ScriptGlobal> overlay{global("g", 9.0)};
    vm.apply_saved_globals(overlay);
    REQUIRE(vm.variable("g") != nullptr);
    CHECK(std::get<std::int32_t>(*vm.variable("g")) == 9);
}
```

Add `test_saved_globals.cpp` to `tests/script/CMakeLists.txt` (both test
executable lists are separate; add it to `gl_script_tests` only).

- [ ] **Step 2: Run to verify failure**

```powershell
cmake --build build --target gl_script_tests
```

Expected: compile failure (`apply_saved_globals`/`globals()` missing).

- [ ] **Step 3: Implement the adapter**

In `globals.hpp` add `#include "gl/assets/save_payload.hpp"` and:

```cpp
std::vector<ScriptGlobal> save_globals_from_vars(
    std::span<const gl::assets::SaveVar> vars);
```

In `globals.cpp` (end of file, outside the anonymous namespace):

```cpp
std::vector<ScriptGlobal> save_globals_from_vars(
    std::span<const gl::assets::SaveVar> vars) {
    std::vector<ScriptGlobal> out;
    out.reserve(vars.size());
    for (const gl::assets::SaveVar &var : vars) {
        ScriptGlobal global;
        global.name = var.name;
        if (var.type == 3) {
            global.value = var.text_value;
        } else {
            const auto number = gl::assets::save_var_number(var);
            if (!number)
                continue;
            global.value = *number;
        }
        out.push_back(std::move(global));
    }
    return out;
}
```

- [ ] **Step 4: Implement the VM overlay**

In `script_vm.hpp` add `#include <cstddef>` if missing and, in
`ScriptVm`'s public section:

```cpp
struct GlobalApplyCounts {
    std::size_t matched = 0;
    std::size_t added = 0;
    std::size_t kept = 0;
};
GlobalApplyCounts apply_saved_globals(std::span<const ScriptGlobal> saved);
const std::map<std::string, Value> &globals() const { return globals_; }
```

In `script_vm.cpp`, after `set_globals`:

```cpp
ScriptVm::GlobalApplyCounts ScriptVm::apply_saved_globals(
    std::span<const ScriptGlobal> saved) {
    GlobalApplyCounts counts;
    std::set<std::string> seen;
    for (const ScriptGlobal &global : saved) {
        if (!seen.insert(global.name).second) {
            gl::log(gl::LogLevel::warning,
                    "script vm: duplicate saved global '" + global.name +
                        "'; last value wins");
        }
        auto found = globals_.find(global.name);
        if (found == globals_.end()) {
            globals_.emplace(global.name, global.value);
            ++counts.added;
            continue;
        }
        const Value &destination = found->second;
        if (std::holds_alternative<std::string>(destination)) {
            if (!std::holds_alternative<std::string>(global.value)) {
                ++counts.kept;
                gl::log(gl::LogLevel::warning,
                        "script vm: saved numeric global '" + global.name +
                            "' does not match a string declaration; keeping script value");
                continue;
            }
            found->second = global.value;
        } else if (std::holds_alternative<std::string>(global.value)) {
            ++counts.kept;
            gl::log(gl::LogLevel::warning,
                    "script vm: saved string global '" + global.name +
                        "' does not match a numeric declaration; keeping script value");
            continue;
        } else if (std::holds_alternative<std::int32_t>(destination)) {
            found->second = std::int32_t{to_int32(global.value)};
        } else if (std::holds_alternative<std::uint32_t>(destination)) {
            found->second = std::uint32_t{to_uint32(global.value)};
        } else {
            found->second = to_double(global.value);
        }
        ++counts.matched;
    }
    rebuild_variables();
    return counts;
}
```

`<set>` is already included in `script_vm.cpp`; keep the existing
`to_int32`/`to_uint32`/`to_double` helpers unchanged.

- [ ] **Step 5: Run the script tests**

```powershell
cmake --build build --target gl_script_tests
build\bin\gl_script_tests.exe --test-case="*saved globals*"
build\bin\gl_script_tests.exe --test-case="*globals*"
```

Expected: all pass.

- [ ] **Step 6: Commit**

```powershell
git add src/script/include/gl/script/globals.hpp src/script/src/globals.cpp src/script/include/gl/script/script_vm.hpp src/script/src/script_vm.cpp tests/script/test_saved_globals.cpp tests/script/CMakeLists.txt
git commit -m "feat(script): overlay saved globals"
```

---

### Task 3: glgame lifecycle and E2E log

**Files:**
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp`

**Interfaces:**
- Consumes: `save_globals_from_vars`, `apply_saved_globals`.
- Produces: `save_globals_` member, log `save globals: saved=<n>`.

- [ ] **Step 1: Add the overlay member**

Near `restore_result_` (`main.cpp:3013` area):

```cpp
    std::vector<gl::script::ScriptGlobal> save_globals_;
```

- [ ] **Step 2: Clear on New Game**

At the start of `load_level` (`main.cpp:2085` area, before
`restore_result_ = {};`):

```cpp
        if (pending_restore_ == nullptr)
            save_globals_.clear();
```

- [ ] **Step 3: Build/rollback the overlay in `load_save`**

Next to the existing rollback snapshot (`main.cpp:2234-2239`) add:

```cpp
        auto save_globals_before = save_globals_;
```

After `apply_hero_from_body(...)` and before `load_level(...)` add:

```cpp
        save_globals_ = gl::script::save_globals_from_vars(payload->vars);
```

In the `if (!level_loaded)` rollback block (`main.cpp:2253-2268`) add:

```cpp
            save_globals_ = std::move(save_globals_before);
```

After the successful `save: loaded slot=...` log (`main.cpp:2273-2277`)
add:

```cpp
        gl::log(gl::LogLevel::info,
                "save globals: saved=" + std::to_string(save_globals_.size()));
```

- [ ] **Step 4: Apply after `set_globals` in `open_dialog`**

At `main.cpp:1296` after `context->vm->set_globals(globals.value());` add:

```cpp
        context->vm->apply_saved_globals(save_globals_);
```

- [ ] **Step 5: E2E**

In `tests/app/test_glgame_screenshot.cpp`, in the `--load-save 0` case add:

```cpp
    CHECK(output.find("save globals: saved=1002") != std::string::npos);
```

- [ ] **Step 6: Run**

```powershell
cmake --build build --target glgame_screenshot_tests
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\glgame_screenshot_tests.exe --test-case="*load-save*"
```

Expected: pass; the new log appears and existing pins are unchanged.

- [ ] **Step 7: Commit**

```powershell
git add tools/glgame/main.cpp tests/app/test_glgame_screenshot.cpp
git commit -m "feat(tools): apply saved globals to dialogs"
```

---

### Task 4: Real 28-save oracle, docs, full verification

**Files:**
- Create: `tests/script/test_real_save_globals.cpp`
- Modify: `tests/script/CMakeLists.txt`
- Modify: `docs/DECISIONS.md`, `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`, `docs/research/p2-15a-save-payload.md`

- [ ] **Step 1: Create the real oracle**

Create `tests/script/test_real_save_globals.cpp`:

```cpp
#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

struct NullHost : gl::script::ScriptHost {
    gl::script::Value call(std::uint32_t, std::span<const gl::script::Value>) override {
        return std::int32_t{0};
    }
};

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) /
                                       "Saves" / ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

std::map<std::string, gl::script::Value>
declared_globals(gl::assets::GameVfs &vfs) {
    auto data = vfs.read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(data.has_value());
    auto parsed = gl::script::parse_globals(data.value());
    REQUIRE(parsed.has_value());
    std::map<std::string, gl::script::Value> out;
    for (const gl::script::ScriptGlobal &global : parsed.value())
        out[global.name] = global.value;
    return out;
}
} // namespace

TEST_CASE("real data: saved globals apply to the dialog vm" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const auto declared = declared_globals(vfs.value());

    std::size_t total_vars = 0;
    std::size_t type0 = 0, type1 = 0, type2 = 0, type3 = 0;
    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        total_vars += payload->vars.size();
        for (const auto &var : payload->vars) {
            if (var.type == 0) ++type0;
            if (var.type == 1) ++type1;
            if (var.type == 2) ++type2;
            if (var.type == 3) ++type3;
        }

        NullHost host;
        gl::script::ScriptVm vm({}, host);
        const auto saved = gl::script::save_globals_from_vars(payload->vars);
        const auto counts = vm.apply_saved_globals(saved);
        CHECK(counts.matched + counts.added == payload->vars.size());
        CHECK(counts.kept == 0);

        for (const auto &var : payload->vars) {
            const auto found = vm.globals().find(var.name);
            REQUIRE(found != vm.globals().end());
            if (var.type == 3) {
                CHECK(std::get<std::string>(found->second) == var.text_value);
                continue;
            }
            const auto number = gl::assets::save_var_number(var);
            REQUIRE(number.has_value());
            const auto declaration = declared.find(var.name);
            if (declaration == declared.end()) {
                CHECK(std::get<double>(found->second) == *number);
            } else if (std::holds_alternative<std::int32_t>(declaration->second)) {
                CHECK(std::get<std::int32_t>(found->second) ==
                      static_cast<std::int32_t>(*number));
            } else if (std::holds_alternative<std::uint32_t>(declaration->second)) {
                CHECK(std::get<std::uint32_t>(found->second) ==
                      static_cast<std::uint32_t>(*number));
            } else {
                CHECK(std::get<double>(found->second) == *number);
            }
        }
    }
    CHECK(total_vars == 26191);
    CHECK(type0 == 23498);
    CHECK(type1 == 2632);
    CHECK(type2 == 61);
    CHECK(type3 == 0);
}

TEST_CASE("real data: saved global pins" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());

    struct Pin {
        int slot;
        std::size_t count;
        const char *name;
        double value;
    };
    const std::vector<Pin> pins{
        {0, 1002, "voevoda_povtor", 10.0},
        {0, 1002, "ShurbaHal_guardian_povtor", 3.0},
        {2, 1004, "L2_male1_povtor", 1.0},
        {21, 879, "voevoda_povtor", 0.0},
    };
    for (const Pin &pin : pins) {
        auto payload = gl::assets::parse_save_payload(read_save(pin.slot));
        REQUIRE(payload.has_value());
        CAPTURE(pin.slot);
        CAPTURE(pin.name);
        CHECK(payload->vars.size() == pin.count);
        NullHost host;
        gl::script::ScriptVm vm({}, host);
        const auto saved = gl::script::save_globals_from_vars(payload->vars);
        vm.apply_saved_globals(saved);
        const auto found = vm.globals().find(pin.name);
        REQUIRE(found != vm.globals().end());
        CHECK(std::get<double>(found->second) == pin.value);
    }
}
```

Add the file to `gl_script_real_tests` in `tests/script/CMakeLists.txt`.

- [ ] **Step 2: Run**

```powershell
cmake --build build --target gl_script_real_tests
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\gl_script_real_tests.exe --test-case="*saved global*"
```

Expected: pass.

- [ ] **Step 3: Docs**

Append to `docs/DECISIONS.md`:

```markdown
| D-075 | 2026-11-16 | P2-15b2 saved script globals. `BT_VARS` values are applied to the dialog CS VM: numeric saves are 8-byte f64 (even for declared int/DWORD), type 3 is `u32 len + cp1251`. `assets::save_var_number` exposes the f64; `script::save_globals_from_vars` converts `SaveVar` to `ScriptGlobal`; `ScriptVm::apply_saved_globals` overlays `globals_` after `set_globals(zlato_vars.scr)` and rebuilds node variables. Existing declared globals keep the `.scr` declared variant: int32 via `to_int32` (clamp/trunc), DWORD via `to_uint32`, float via `to_double`, string only from saved strings; numeric↔string mismatches keep the script value and are counted as `kept`. Undeclared saved names are added as `double` (numeric) or `string` (type 3); duplicate saved names are last-wins with a warning. This is a chosen policy for OQ-5 (the original's f64→declared conversion is unproven), not an original-parity claim. `glgame` builds the overlay from `payload.vars` on successful load-save, rolls it back on failed level load, clears it on New Game, applies it in every `open_dialog` after `set_globals`, and logs `save globals: saved=<n>`. Real 28-save application is 0/N: 26,191 vars (types `{0:23498,1:2632,2:61}`, no type 3), 4,239 names undeclared in `zlato_vars.scr` (added), 0 declared-type mismatches; slot0 1002 `{905,94,3}`, slot2 1004 `{907,94,3}`, slot21 879 `{783,94,2}`. Out of scope: `BT_TRIBE` matrix, `BT_DIARY`/`BT_HSTRY`, save write (`P2-15c`), live oracle, other VM instances. | spec/plan 2026-11-16 p2-15b2; research `p2-15a-save-payload.md` §`BT_VARS ` application evidence; D-071…D-074. |
```

`STATUS.md`: replace the b1b2 design section with a done section
(what was implemented, exact focused/full outputs, residuals: OQ-5 chosen
policy, type 3 synthetic-only, undeclared names, save write; `Статус:
done`).

`PROJECT.md` P2 row: append `P2-15b2 BT_VARS globals done (2026-11-16, D-075: overlay over zlato_vars.scr, 26,191 vars 0/N, unknown names added)`.

`parity-matrix.md` P2-15 row: replace the `BT_VARS` gap wording (or add)
with `BT_VARS done (D-075): 26,191 values 0/N applied to dialog VM; OQ-5
policy documented.` and append `; D-075` to the evidence cell.

`research/p2-15a-save-payload.md`: append a short implementation result
section stating the chosen conversion policy and the real 0/N result.

- [ ] **Step 4: Verify docs encoding**

```powershell
$docs = @("docs/DECISIONS.md","docs/STATUS.md","docs/PROJECT.md","docs/research/parity-matrix.md","docs/research/p2-15a-save-payload.md")
foreach ($doc in $docs) {
  $b=[IO.File]::ReadAllBytes((Resolve-Path $doc))
  "nul=$((($b | Where-Object {$_ -eq 0}).Count)) $doc"
}
```

Expected: `nul=0`; changed lines contain no literal `\n`.

- [ ] **Step 5: Full verification**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```

Use a generous timeout (≥ 2400000 ms). Expected: all tests pass, exit 0;
record the real total/time in `STATUS.md`.

- [ ] **Step 6: Commit**

```powershell
git add tests/script/test_real_save_globals.cpp tests/script/CMakeLists.txt docs/DECISIONS.md docs/STATUS.md docs/PROJECT.md docs/research/parity-matrix.md docs/research/p2-15a-save-payload.md
git commit -m "docs: record saved globals restore"
```

---

## Self-Review

- Spec coverage: `save_var_number` (Task 1), adapter + VM overlay/accessor
  (Task 2), glgame lifecycle/E2E (Task 3), real 28-save oracle/docs/full
  verification (Task 4).
- No placeholders: code blocks contain the implementation; the real oracle
  pins counts/types/values.
- Type consistency: `GlobalApplyCounts`, `apply_saved_globals`,
  `save_globals_from_vars`, `globals()` names/signatures match across
  tasks and spec.
- Documented non-claim: OQ-5 policy is recorded as chosen, not proven
  parity.
