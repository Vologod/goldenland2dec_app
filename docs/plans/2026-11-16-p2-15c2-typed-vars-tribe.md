# P2-15c2 — Typed `BT_VARS` / `BT_TRIBE` Encoders Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement task-by-task.

**Goal:** Encode `BT_VARS`/`BT_TRIBE` bodies from typed models and prove byte-identical 28-save round-trip through the VM overlay.

**Architecture:** `assets` gains `encode_vars_body`/`encode_tribe_body`; `script` gains `update_vars_from_globals` (VM `Value` → `SaveVar` f64/string, preserving saved order/type). The container writer (c1) will call these later; no file I/O here.

**Tech Stack:** C++20, CMake + Ninja, doctest, `assets`/`script`.

**Spec:** `docs/specs/2026-11-16-p2-15c2-typed-vars-tribe-design.md`.

## Global Constraints

- C++20; no new dependency; layers downward.
- No comments unless parity-critical.
- Saved order/type/names are authoritative; new VM globals are not appended.
- byte-equality with the original body is the oracle (no reinterpretation).
- Real tests skip without `GL_GAME_DIR`.
- Toolchain:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

---

### Task 1: Assets body encoders + unit tests

**Files:**
- Modify: `src/assets/include/gl/assets/save_payload.hpp`
- Modify: `src/assets/src/save_payload.cpp`
- Test: `tests/assets/test_save_payload.cpp`

- [ ] **Step 1: Failing tests**

Append to `tests/assets/test_save_payload.cpp`:

```cpp
TEST_CASE("encode_vars_body writes typed variables") {
    std::vector<gl::assets::SaveVar> vars;
    gl::assets::SaveVar integer;
    integer.type = 0;
    integer.name = "i";
    const double ten = 10.0;
    std::memcpy(integer.value.data(), &ten, 8);
    vars.push_back(integer);

    gl::assets::SaveVar word;
    word.type = 1;
    word.name = "d";
    std::memcpy(word.value.data(), &ten, 8);
    vars.push_back(word);

    gl::assets::SaveVar text;
    text.type = 3;
    text.name = "s";
    text.text_value = "x";
    vars.push_back(text);

    const auto body = gl::assets::encode_vars_body(vars);
    REQUIRE(body.has_value());

    std::vector<std::uint8_t> expected;
    put_u32(expected, 3);
    put_u32(expected, 0);
    put_string(expected, "i");
    expected.insert(expected.end(), integer.value.begin(), integer.value.end());
    put_u32(expected, 1);
    put_string(expected, "d");
    expected.insert(expected.end(), word.value.begin(), word.value.end());
    put_u32(expected, 3);
    put_string(expected, "s");
    put_string(expected, "x");
    CHECK(body.value() == expected);

    CHECK(gl::assets::encode_vars_body(std::span<const gl::assets::SaveVar>()).value() ==
          std::vector<std::uint8_t>{0, 0, 0, 0});

    auto bad = vars;
    bad[0].type = 4;
    const auto error = gl::assets::encode_vars_body(bad);
    REQUIRE_FALSE(error.has_value());
    CHECK(error.error().code == gl::ErrorCode::format);
}

TEST_CASE("encode_tribe_body writes entries and the matrix") {
    gl::assets::SaveTribeTable table;
    table.entries.push_back(gl::assets::SaveTribeEntry{7, "a"});
    table.entries.push_back(gl::assets::SaveTribeEntry{1, "b"});
    table.entries.push_back(gl::assets::SaveTribeEntry{4, "c"});
    table.matrix = {3, 5, 6, 7, 3, 8, 9, 10, 3};

    std::vector<std::uint8_t> expected;
    put_u32(expected, 3);
    put_u32(expected, 7);
    put_string(expected, "a");
    put_u32(expected, 1);
    put_string(expected, "b");
    put_u32(expected, 4);
    put_string(expected, "c");
    expected.insert(expected.end(), table.matrix.begin(), table.matrix.end());

    const auto body = gl::assets::encode_tribe_body(table);
    REQUIRE(body.has_value());
    CHECK(body.value() == expected);

    CHECK(gl::assets::encode_tribe_body({}).value() ==
          std::vector<std::uint8_t>{0, 0, 0, 0});

    table.matrix.pop_back();
    const auto error = gl::assets::encode_tribe_body(table);
    REQUIRE_FALSE(error.has_value());
    CHECK(error.error().code == gl::ErrorCode::format);
}
```

- [ ] **Step 2: Run red**

```powershell
cmake --build build --target gl_assets_tests
```

Expected: compile failure.

- [ ] **Step 3: Implement**

In `save_payload.hpp` after `encode_save_payload`:

```cpp
Result<std::vector<std::uint8_t>> encode_vars_body(std::span<const SaveVar> vars);
Result<std::vector<std::uint8_t>> encode_tribe_body(const SaveTribeTable &table);
```

In `save_payload.cpp` (add an anonymous-namespace helper near the top):

```cpp
namespace {
void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}
} // namespace
```

Then after `encode_save_payload`:

```cpp
Result<std::vector<std::uint8_t>> encode_vars_body(std::span<const SaveVar> vars) {
    try {
        if (vars.size() > std::numeric_limits<std::uint32_t>::max())
            return format_error("BT_VARS has too many entries");
        std::vector<std::uint8_t> out;
        append_u32(out, static_cast<std::uint32_t>(vars.size()));
        for (const SaveVar &var : vars) {
            if (var.type > 3)
                return format_error("BT_VARS type is unknown");
            append_u32(out, var.type);
            const std::string name = utf8_to_cp1251(var.name);
            append_u32(out, static_cast<std::uint32_t>(name.size()));
            out.insert(out.end(), name.begin(), name.end());
            if (var.type < 3) {
                out.insert(out.end(), var.value.begin(), var.value.end());
            } else {
                const std::string text = utf8_to_cp1251(var.text_value);
                append_u32(out, static_cast<std::uint32_t>(text.size()));
                out.insert(out.end(), text.begin(), text.end());
            }
        }
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("BT_VARS allocation failed");
    }
}

Result<std::vector<std::uint8_t>> encode_tribe_body(const SaveTribeTable &table) {
    try {
        if (table.entries.size() > std::numeric_limits<std::uint32_t>::max())
            return format_error("BT_TRIBE has too many entries");
        const std::uint64_t matrix_size =
            static_cast<std::uint64_t>(table.entries.size()) * table.entries.size();
        if (matrix_size != table.matrix.size())
            return format_error("BT_TRIBE matrix size mismatch");
        std::vector<std::uint8_t> out;
        append_u32(out, static_cast<std::uint32_t>(table.entries.size()));
        for (const SaveTribeEntry &entry : table.entries) {
            append_u32(out, entry.id);
            const std::string name = utf8_to_cp1251(entry.name);
            append_u32(out, static_cast<std::uint32_t>(name.size()));
            out.insert(out.end(), name.begin(), name.end());
        }
        out.insert(out.end(), table.matrix.begin(), table.matrix.end());
        return out;
    } catch (const std::bad_alloc &) {
        return format_error("BT_TRIBE allocation failed");
    }
}
```

- [ ] **Step 4: Run green**

```powershell
cmake --build build --target gl_assets_tests
build\bin\gl_assets_tests.exe --test-case="*encode_vars_body*,*encode_tribe_body*"
```

- [ ] **Step 5: Commit**

```powershell
git add src/assets/include/gl/assets/save_payload.hpp src/assets/src/save_payload.cpp tests/assets/test_save_payload.cpp
git commit -m "feat(assets): encode typed vars and tribe"
```

---

### Task 2: VM adapter and real `BT_VARS` equality

**Files:**
- Modify: `src/script/include/gl/script/globals.hpp`
- Modify: `src/script/src/globals.cpp`
- Modify: `tests/script/test_saved_globals.cpp`
- Create: `tests/script/test_real_save_vars.cpp`
- Modify: `tests/script/CMakeLists.txt`

- [ ] **Step 1: Failing adapter tests**

Append to `tests/script/test_saved_globals.cpp`:

```cpp
TEST_CASE("update_vars_from_globals writes VM values back into SaveVar") {
    gl::script::ScriptGlobal i{"int", "i", std::int32_t{7}};
    gl::script::ScriptGlobal d{"DWORD", "d", std::uint32_t{9}};
    gl::script::ScriptGlobal f{"float", "f", 2.5};
    gl::script::ScriptGlobal s{"string", "s", std::string{"x"}};
    const std::vector<gl::script::ScriptGlobal> initial{i, d, f, s};
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    vm.set_globals(initial);

    gl::assets::SaveVar vi;
    vi.type = 0;
    vi.name = "i";
    gl::assets::SaveVar vd = vi;
    vd.type = 1;
    vd.name = "d";
    gl::assets::SaveVar vf = vi;
    vf.type = 2;
    vf.name = "f";
    gl::assets::SaveVar vs;
    vs.type = 3;
    vs.name = "s";
    gl::assets::SaveVar missing;
    missing.type = 0;
    missing.name = "missing";
    const double keep = 1.5;
    std::memcpy(missing.value.data(), &keep, 8);

    const auto updated = gl::script::update_vars_from_globals(
        {vi, vd, vf, vs, missing}, vm);
    REQUIRE(updated.size() == 5);
    CHECK(gl::assets::save_var_number(updated[0]).has_value());
    CHECK(*gl::assets::save_var_number(updated[0]) == 7.0);
    CHECK(*gl::assets::save_var_number(updated[1]) == 9.0);
    CHECK(*gl::assets::save_var_number(updated[2]) == 2.5);
    CHECK(updated[3].text_value == "x");
    CHECK(*gl::assets::save_var_number(updated[4]) == 1.5);
    CHECK(updated[0].type == 0);
    CHECK(updated[1].name == "d");
}
```

Add `#include "gl/assets/save_payload.hpp"` and `<cstring>` if missing.

- [ ] **Step 2: Run red**

```powershell
cmake --build build --target gl_script_tests
```

- [ ] **Step 3: Implement**

In `globals.hpp` add `class ScriptVm;` (forward declaration; no include,
because `script_vm.hpp` includes `globals.hpp`) and the declaration above.
In `globals.cpp` include `gl/script/script_vm.hpp` and `<bit>`, then add:

```cpp
std::vector<gl::assets::SaveVar> update_vars_from_globals(
    const std::vector<gl::assets::SaveVar> &saved, const ScriptVm &vm) {
    std::vector<gl::assets::SaveVar> out = saved;
    for (gl::assets::SaveVar &var : out) {
        const auto found = vm.globals().find(var.name);
        if (found == vm.globals().end())
            continue;
        const Value &value = found->second;
        if (var.type == 3) {
            if (std::holds_alternative<std::string>(value))
                var.text_value = std::get<std::string>(value);
            continue;
        }
        double number = 0.0;
        if (std::holds_alternative<std::int32_t>(value))
            number = static_cast<double>(std::get<std::int32_t>(value));
        else if (std::holds_alternative<std::uint32_t>(value))
            number = static_cast<double>(std::get<std::uint32_t>(value));
        else if (std::holds_alternative<double>(value))
            number = std::get<double>(value);
        else
            continue;
        const std::uint64_t bits = std::bit_cast<std::uint64_t>(number);
        for (std::size_t byte = 0; byte < var.value.size(); ++byte)
            var.value[byte] =
                static_cast<std::uint8_t>((bits >> (byte * 8)) & 0xFF);
    }
    return out;
}
```

- [ ] **Step 4: Unit green**

```powershell
cmake --build build --target gl_script_tests
build\bin\gl_script_tests.exe --test-case="*update_vars*"
```

- [ ] **Step 5: Real BT_VARS equality test**

Create `tests/script/test_real_save_vars.cpp`:

```cpp
#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
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

std::vector<std::uint8_t> block_body(const gl::assets::SavePayload &payload,
                                     std::size_t index) {
    const auto &block = payload.blocks[index];
    return std::vector<std::uint8_t>(
        payload.decompressed.begin() + static_cast<std::ptrdiff_t>(block.offset),
        payload.decompressed.begin() +
            static_cast<std::ptrdiff_t>(block.offset + block.size));
}
} // namespace

TEST_CASE("real data: BT_VARS re-encodes byte-identically" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto globals_data = vfs->read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(globals_data.has_value());
    auto parsed_globals = gl::script::parse_globals(globals_data.value());
    REQUIRE(parsed_globals.has_value());

    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);

        NullHost host;
        gl::script::ScriptVm vm({}, host);
        vm.set_globals(parsed_globals.value());
        vm.apply_saved_globals(gl::script::save_globals_from_vars(payload->vars));

        const auto updated =
            gl::script::update_vars_from_globals(payload->vars, vm);
        const auto body = gl::assets::encode_vars_body(updated);
        REQUIRE(body.has_value());
        CHECK(body.value() == block_body(payload.value(), 2));
    }
}
```

Add the file to `gl_script_real_tests` in `tests/script/CMakeLists.txt`.

- [ ] **Step 6: Run real**

```powershell
cmake --build build --target gl_script_real_tests
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\gl_script_real_tests.exe --test-case="*BT_VARS re-encodes*"
```

Expected: pass 28/28.

- [ ] **Step 7: Commit**

```powershell
git add src/script/include/gl/script/globals.hpp src/script/src/globals.cpp tests/script/test_saved_globals.cpp tests/script/test_real_save_vars.cpp tests/script/CMakeLists.txt
git commit -m "feat(script): write typed vars from globals"
```

---

### Task 3: Real `BT_TRIBE` equality, docs, full verification

**Files:**
- Modify: `tests/assets/test_save_payload_real.cpp`
- Modify: `docs/DECISIONS.md`, `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`, `docs/research/p2-15a-save-payload.md`

- [ ] **Step 1: Real BT_TRIBE test**

Append to `tests/assets/test_save_payload_real.cpp`:

```cpp
TEST_CASE("real data: BT_TRIBE re-encodes byte-identically" *
          doctest::skip(!game_dir_available())) {
    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        const auto body = gl::assets::encode_tribe_body(payload->tribes);
        REQUIRE(body.has_value());
        const auto &block = payload->blocks[3];
        const std::vector<std::uint8_t> expected(
            payload->decompressed.begin() + static_cast<std::ptrdiff_t>(block.offset),
            payload->decompressed.begin() +
                static_cast<std::ptrdiff_t>(block.offset + block.size));
        CHECK(body.value() == expected);
    }
}
```

- [ ] **Step 2: Run**

```powershell
cmake --build build --target gl_assets_real_tests
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\gl_assets_real_tests.exe --test-case="*BT_TRIBE re-encodes*"
```

- [ ] **Step 3: Docs**

D-080:

```markdown
| D-080 | 2026-11-16 | P2-15c2 typed `BT_VARS`/`BT_TRIBE` encoders. `encode_vars_body` writes `u32 count`, per-entry `u32 type` (0..3), `u32 name_len+cp1251`, 8 raw f64 bytes (types 0/1/2) or `u32 text_len+cp1251` (type 3); `encode_tribe_body` writes `u32 count`, `u32 id+string` entries and the `count²` matrix in entry order; invalid type/size → `format`. `script::update_vars_from_globals` copies the saved vector (order/type/name authoritative), replaces numeric values from VM `Value` with `static_cast<double>` LE f64, replaces type-3 text from string values, keeps raw on missing/type-mismatched names, and never appends/removes names. Real 28 saves: `update_vars_from_globals(parse(zlato_vars)+overlay)` → `encode_vars_body` equals the raw `BT_VARS` body byte-for-byte, and `encode_tribe_body(payload.tribes)` equals the raw `BT_TRIBE` body byte-for-byte (28/28). New/runtime-only globals, typed HERO/tail/ENTITIES, file write/`game_save` and header date/name remain P2-15c3/c4; type 3 is unit-only (absent from the corpus). | spec/plan 2026-11-16 p2-15c2; research `p2-15a-save-payload.md` §`BT_VARS `/§`BT_TRIBE`; D-075/D-076/D-079. |
```

STATUS: add done section with focused/full outputs and residuals
(new globals policy c4, type3 unit-only, HERO/tail raw c3, file write c4).

PROJECT P2 row: append `P2-15c2 typed VARS/TRIBE encoders done (2026-11-16, D-080: 28/28 body byte-equality)`.

parity-matrix P2-15 row: append `typed VARS/TRIBE encoders done (D-080): 28/28 body byte-equality`; add `; D-080` to evidence.

research p2-15a: append implementation result (adapter/encoders, byte-equality).

- [ ] **Step 4: Encoding + full ctest**

```powershell
$docs = @("docs/DECISIONS.md","docs/STATUS.md","docs/PROJECT.md","docs/research/parity-matrix.md","docs/research/p2-15a-save-payload.md")
foreach ($doc in $docs) { $b=[IO.File]::ReadAllBytes((Resolve-Path $doc)); "nul=$((($b | Where-Object {$_ -eq 0}).Count)) $doc" }
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```

Timeout ≥ 2400000 ms; record real count/time/exit.

- [ ] **Step 5: Commit**

```powershell
git add tests/assets/test_save_payload_real.cpp docs/DECISIONS.md docs/STATUS.md docs/PROJECT.md docs/research/parity-matrix.md docs/research/p2-15a-save-payload.md
git commit -m "docs: record typed vars and tribe encoders"
```

---

## Self-Review

- Spec coverage: encoders/unit (Task 1), adapter + real VARS (Task 2),
  real TRIBE/docs/full (Task 3).
- No placeholders.
- Type consistency: `encode_vars_body`, `encode_tribe_body`,
  `update_vars_from_globals`, `SaveVar` field use is consistent.
- Non-claims: no new globals, no file write, HERO/tail remain c3.
