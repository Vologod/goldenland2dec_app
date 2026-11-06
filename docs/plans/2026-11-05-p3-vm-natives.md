# Phase 1a — Server.dll VM parity & native dispatch (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Привести CS-VM порта к семантике `Server.dll` и ввести `NativeRegistry` + `GameScriptHost` для всех 65 зарегистрированных нативов, с явной классификацией поддержки.

**Architecture:** `gl_script` получает реестр нативов и сервисы-колбэки; `gl_game` — `GameScriptHost`, который диспетчеризует через реестр и пишет один info-лог на неподдерживаемый ID. `glgame`/`glscript` подключают host; `gl_assets::cs_function_name` остаётся единственным источником имён.

**Tech Stack:** C++20, CMake + Ninja, doctest, Ghidra 12.1.4 headless (только research), PowerShell 5.1.

## Global Constraints

- `Server.dll` SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`; корпус `.local/decomp/` не коммитится.
- Поведение — только из `Server.dll`/`Client.dll`/`GoldenLand.exe`; «правдоподобные» реализации запрещены (D-029).
- Слои только вниз: `game → script → assets`; `script` не зависит от `game`/SDL.
- Комментарии в коде не добавлять без необходимости; имена — `snake_case`/`PascalCase`.
- Документы — UTF-8 без BOM; после правок `nul=0`.
- Коммит — только по явной команде пользователя.
- Часовой checkpoint: перед каждым шагом сверять `Get-Date`; на границе ровного часа остановиться и спросить пользователя.

---

### Task 1: Export native handlers from Server.dll

**Files:**
- Create: `tools/ghidra/ExportNativeHandlers.java`
- Generated (gitignored): `.local/decomp/server/natives/{natives.csv,natives.c}`

**Interfaces:**
- Consumes: registration block `FUN_14042464` (RVA `0x42464`) in `Server.dll`.
- Produces: `natives.csv` (`id,rva,name,created`) and `natives.c` (decompiled handler per ID) for 65 IDs.

- [x] **Step 1: Write the Ghidra export script**

`tools/ghidra/ExportNativeHandlers.java` reads `id,rva,name` lines, calls
`disassemble`/`createFunction` for every RVA, then decompiles each function
and writes `natives.csv` + `natives.c`.

- [x] **Step 2: Build the handler list from the registration block**

```powershell
$lines = Get-Content .local\decomp\server\decompiled.c
# pair FUN_14005e78(...s_NAME...,0xID) with FUN_14004d00(...,0xID,handler)
# output: id,0xRVA,name  ->  $env:TEMP\native_handlers.csv
```

- [x] **Step 3: Run headless export**

```powershell
$env:JAVA_HOME="$PWD\.local\toolchains\jdk-21.0.12.1+1"
$proj="$env:TEMP\ghidra_proj"
New-Item -ItemType Directory -Force -Path $proj | Out-Null
& .local\toolchains\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat `
  $proj glserver -import "I:\Games\Златогорье-2\Server.dll" -overwrite
& .local\toolchains\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat `
  $proj glserver -process Server.dll -noanalysis `
  -scriptPath "$PWD\tools\ghidra" `
  -postScript ExportNativeHandlers.java "$env:TEMP\native_handlers.csv" `
  "$PWD\.local\decomp\server\natives"
```

- [x] **Step 4: Verify the export**

Expected: `done natives=65 created=<n> ok=65 failed=0`; `natives.csv` has
66 lines (header + 65); every ID from the registration block is present.

Evidence 2026-11-05: `done natives=65 created=33 ok=65 failed=0`.

- [x] **Step 5: Commit**

```bash
git add tools/ghidra/ExportNativeHandlers.java
git commit -m "feat(ghidra): export Server.dll native handlers"
```

---

### Task 2: Research VM and native semantics

**Files:**
- Create: `docs/research/p3-vm-natives-server.md`
- Modify: `docs/research/decomp-index.md` (native handler export command and counts)

**Interfaces:**
- Consumes: `.local/decomp/server/decompiled.c`, `.local/decomp/server/natives/natives.c`, `docs/research/p3a-cs-vm.md`.
- Produces: exact facts consumed by Tasks 3–5:
  - §3 node layout and opcode table;
  - §4 VM delta list vs `gl_script` (each row: opcode, Server behavior, port behavior, verdict);
  - §5 native table: `ID | name | RVA | arity | arg types | return | side effects | support`.

- [x] **Step 1: Record node layout from the parser**

Read `FUN_1403e020` (RVA `0x3e020`) and write the parsed record layout and
sizes for opcodes `0..0x32`; state whether it matches
`gl::assets::CsNode` in `src/assets/include/gl/assets/cs.hpp`.
Cite RVA, record size, and the `case` blocks used as evidence.

- [x] **Step 2: Extract VM opcode semantics**

Read `FUN_1403a010` (evaluator, RVA `0x3a010`) and `FUN_1403a914`
(executor, RVA `0x3a914`). For each opcode `0..0x32` write: operands,
result type, rounding/overflow behavior, control-flow effect. For ambiguous
Ghidra output, disassemble the specific block (Ghidra listing or `objdump`)
and record the instruction sequence instead of guessing.

- [x] **Step 3: Produce the port delta list**

Compare §2 findings with `src/script/src/script_vm.cpp`. Write one row per
difference:

`opcode | Server.dll behavior (RVA) | port behavior (file:line) | verdict (match / port-fix / unverified)`.

Only rows with `port-fix` become Task 5 tests. Rows that remain ambiguous
are `unverified` and must not change code.

- [x] **Step 4: Fill the 65-row native table**

For every ID in `.local/decomp/server/natives/natives.csv`:
- copy name and handler RVA;
- determine arity from the `FUN_14038c6c(...)` call in `natives.c`
  (the global argument block is `DAT_14089744 + i*8` type,
  `DAT_14089740 + i*8` value; types `0/1` int, `2` float, `3` string, `-1`
  absent);
- record return value and side effects (globals/objects written);
- assign support: `vm_internal`, `supported`, `unsupported` + subsystem
  required.

- [x] **Step 5: Verify the document**

Checks:
- 65 rows in §5, IDs unique and equal to `natives.csv`;
- every `supported` row has a decompiled handler body in `natives.c`;
- every `port-fix` row in §4 has both RVAs and port line numbers;
- UTF-8 without BOM, `nul=0`.

- [x] **Step 6: Commit**

```bash
git add docs/research/p3-vm-natives-server.md docs/research/decomp-index.md
git commit -m "docs(research): record Server.dll VM and native semantics"
```

---

### Task 3: Native registry in gl_script

**Files:**
- Create: `src/script/include/gl/script/native_registry.hpp`
- Create: `src/script/src/native_registry.cpp`
- Modify: `src/script/CMakeLists.txt`
- Create: `tests/script/test_native_registry.cpp`
- Modify: `tests/script/CMakeLists.txt`

**Interfaces:**
- Consumes: `gl::script::Value`; `gl::assets::cs_function_name(uint32_t)`.
- Produces:
  - `enum class NativeSupport { vm_internal, supported, unsupported };`
  - `struct NativeServices` with `console`, `command`, `signal`, `exit`,
    `play_sound`, `log` callbacks;
  - `using NativeHandler = Value (*)(std::span<const Value>, NativeServices&);`
  - `struct NativeInfo { std::uint32_t id; NativeSupport support;
    std::string_view reason; NativeHandler handler; const char* name() const; };`
  - `NativeRegistry::entries()`, `find(id)`, `call(info, args, services)`.

- [x] **Step 1: Write the failing registry tests**

`tests/script/test_native_registry.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/cs.hpp"
#include "gl/script/native_registry.hpp"
#include <set>
#include <string>
#include <string_view>

TEST_CASE("native registry covers every Server.dll id") {
    const auto entries = gl::script::NativeRegistry::entries();
    REQUIRE(entries.size() == 65);
    std::set<std::uint32_t> ids;
    for (const auto& entry : entries) {
        CHECK(ids.insert(entry.id).second);
        CHECK(std::string_view(gl::assets::cs_function_name(entry.id)) != "unknown");
        if (entry.support == gl::script::NativeSupport::supported)
            CHECK(entry.handler != nullptr);
        if (entry.support == gl::script::NativeSupport::unsupported)
            CHECK(!entry.reason.empty());
    }
}

TEST_CASE("native registry classifies dialog and host natives") {
    using gl::script::NativeRegistry;
    using gl::script::NativeSupport;
    for (std::uint32_t id : {0x1000004u, 0x1000005u, 0x1000006u})
        CHECK(NativeRegistry::find(id)->support == NativeSupport::vm_internal);
    for (std::uint32_t id : {0x1000000u, 0x1000001u, 0x1000002u, 0x1000003u, 0x1000007u})
        CHECK(NativeRegistry::find(id)->support == NativeSupport::supported);
    CHECK(NativeRegistry::find(0x4000001)->support == NativeSupport::unsupported);
    CHECK(NativeRegistry::find(0xDEADBEEF) == nullptr);
}

TEST_CASE("native registry dispatches supported services") {
    gl::script::NativeServices services;
    std::string console;
    std::string command;
    std::string sound;
    std::int32_t exit_code = -1;
    services.console = [&](std::string_view s) { console = std::string(s); };
    services.command = [&](std::string_view s) { command = std::string(s); };
    services.play_sound = [&](std::string_view s) { sound = std::string(s); };
    services.exit = [&](std::int32_t code) { exit_code = code; };

    const std::string text = "hello";
    const gl::script::Value arg = text;
    gl::script::Value result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x1000002), {&arg, 1}, services);
    CHECK(console == "hello");
    CHECK(std::get<std::int32_t>(result) == 0);

    gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x1000003), {&arg, 1}, services);
    CHECK(command == "hello");

    gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x1000007), {&arg, 1}, services);
    CHECK(sound == "hello");

    const gl::script::Value code = std::int32_t{7};
    gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x1000000), {&code, 1}, services);
    CHECK(exit_code == 7);
}

TEST_CASE("unsupported native returns zero") {
    gl::script::NativeServices services;
    gl::script::Value result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000000), {}, services);
    CHECK(std::get<std::int32_t>(result) == 0);
}
```

- [x] **Step 2: Run the tests to verify they fail**

Run:
```powershell
cmake --build build
build\bin\gl_script_tests.exe
```
Expected: build failure — `native_registry.hpp` does not exist.

- [x] **Step 3: Write the registry header**

`src/script/include/gl/script/native_registry.hpp`:

```cpp
#pragma once
#include "gl/script/value.hpp"
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

namespace gl::script {
enum class NativeSupport { vm_internal, supported, unsupported };

struct NativeServices {
    std::function<void(std::string_view)> console;
    std::function<void(std::string_view)> command;
    std::function<void(std::string_view)> signal;
    std::function<void(std::int32_t)> exit;
    std::function<void(std::string_view)> play_sound;
    std::function<void(std::string_view)> log;
};

using NativeHandler = Value (*)(std::span<const Value>, NativeServices&);

struct NativeInfo {
    std::uint32_t id = 0;
    NativeSupport support = NativeSupport::unsupported;
    std::string_view reason;
    NativeHandler handler = nullptr;
    const char* name() const;
};

class NativeRegistry {
  public:
    static std::span<const NativeInfo> entries();
    static const NativeInfo* find(std::uint32_t id);
    static Value call(const NativeInfo& info, std::span<const Value> args,
                      NativeServices& services);
};
} // namespace gl::script
```

- [x] **Step 4: Write the registry implementation**

`src/script/src/native_registry.cpp`:

```cpp
#include "gl/script/native_registry.hpp"
#include "gl/assets/cs.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace gl::script {
namespace {
std::int32_t as_int(const Value& value) {
    if (const auto* item = std::get_if<std::int32_t>(&value)) return *item;
    if (const auto* item = std::get_if<std::uint32_t>(&value))
        return static_cast<std::int32_t>(*item);
    if (const auto* item = std::get_if<double>(&value))
        return static_cast<std::int32_t>(*item);
    return 0;
}

std::string_view as_string(const Value& value) {
    if (const auto* item = std::get_if<std::string>(&value)) return *item;
    return {};
}

std::string_view first_string(std::span<const Value> args) {
    return args.empty() ? std::string_view{} : as_string(args.front());
}

Value native_exit(std::span<const Value> args, NativeServices& services) {
    const std::int32_t code = args.empty() ? 0 : as_int(args.front());
    if (services.exit) services.exit(code);
    return std::int32_t{0};
}

Value native_signal(std::span<const Value> args, NativeServices& services) {
    if (services.signal) services.signal(first_string(args));
    return std::int32_t{0};
}

Value native_console(std::span<const Value> args, NativeServices& services) {
    if (services.console) services.console(first_string(args));
    return std::int32_t{0};
}

Value native_cmd(std::span<const Value> args, NativeServices& services) {
    if (services.command) services.command(first_string(args));
    return std::int32_t{0};
}

Value native_play_sound(std::span<const Value> args, NativeServices& services) {
    if (services.play_sound) services.play_sound(first_string(args));
    return std::int32_t{0};
}

std::vector<NativeInfo> make_entries() {
    std::vector<NativeInfo> entries;
    entries.reserve(65);
    const auto add = [&entries](std::uint32_t id, NativeSupport support,
                                std::string_view reason = {},
                                NativeHandler handler = nullptr) {
        entries.push_back(NativeInfo{id, support, reason, handler});
    };

    add(0x1000000, NativeSupport::supported, {}, &native_exit);
    add(0x1000001, NativeSupport::supported, {}, &native_signal);
    add(0x1000002, NativeSupport::supported, {}, &native_console);
    add(0x1000003, NativeSupport::supported, {}, &native_cmd);
    add(0x1000004, NativeSupport::vm_internal, "handled by ScriptVm");
    add(0x1000005, NativeSupport::vm_internal, "handled by ScriptVm");
    add(0x1000006, NativeSupport::vm_internal, "handled by ScriptVm");
    add(0x1000007, NativeSupport::supported, {}, &native_play_sound);

    for (std::uint32_t id = 0x2000000; id <= 0x2000002; ++id)
        add(id, NativeSupport::unsupported, "requires world/effect runtime");
    for (std::uint32_t id = 0x3000000; id <= 0x3000008; ++id)
        add(id, NativeSupport::unsupported, "requires world/level runtime");
    for (std::uint32_t id = 0x4000000; id <= 0x400000c; ++id)
        add(id, NativeSupport::unsupported, "requires person runtime state");
    for (std::uint32_t id = 0x5000000; id <= 0x5000008; ++id)
        add(id, NativeSupport::unsupported, "requires item runtime state");
    for (std::uint32_t id = 0x6000000; id <= 0x6000003; ++id)
        add(id, NativeSupport::unsupported, "requires world clock");
    for (std::uint32_t id = 0x7000000; id <= 0x7000010; ++id)
        add(id, NativeSupport::unsupported, "requires quest/event runtime");
    add(0x7000013, NativeSupport::unsupported, "requires person runtime state");
    add(0x7000014, NativeSupport::unsupported, "requires world/level runtime");
    return entries;
}
} // namespace

const char* NativeInfo::name() const { return gl::assets::cs_function_name(id); }

std::span<const NativeInfo> NativeRegistry::entries() {
    static const std::vector<NativeInfo> list = make_entries();
    return list;
}

const NativeInfo* NativeRegistry::find(std::uint32_t id) {
    const auto all = entries();
    const auto it = std::find_if(all.begin(), all.end(),
                                 [id](const NativeInfo& entry) { return entry.id == id; });
    return it == all.end() ? nullptr : &*it;
}

Value NativeRegistry::call(const NativeInfo& info, std::span<const Value> args,
                           NativeServices& services) {
    if (info.support != NativeSupport::supported || info.handler == nullptr)
        return std::int32_t{0};
    return info.handler(args, services);
}
} // namespace gl::script
```

- [x] **Step 5: Register the sources and test**

`src/script/CMakeLists.txt`: add `src/native_registry.cpp` to `gl_script`.
`tests/script/CMakeLists.txt`: add `test_native_registry.cpp` to
`gl_script_tests`.

- [x] **Step 6: Run the tests to verify they pass**

Run:
```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
build\bin\gl_script_tests.exe
```
Expected: all test cases pass; registry size 65.

- [x] **Step 7: Commit**

```bash
git add src/script/include/gl/script/native_registry.hpp src/script/src/native_registry.cpp \
  src/script/CMakeLists.txt tests/script/test_native_registry.cpp tests/script/CMakeLists.txt
git commit -m "feat(script): add Server.dll native registry"
```

---

### Task 4: GameScriptHost in gl_game

**Files:**
- Create: `src/game/include/gl/game/game_script_host.hpp`
- Create: `src/game/src/game_script_host.cpp`
- Modify: `src/game/CMakeLists.txt`
- Create: `tests/game/test_game_script_host.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
- Consumes: `gl::script::ScriptHost`, `NativeRegistry`, `NativeServices`.
- Produces: `gl::game::GameScriptHost(gl::script::NativeServices)` with
  `Value call(std::uint32_t, std::span<const Value>) override` and
  `gl::script::NativeServices& services()`.

- [x] **Step 1: Write the failing host tests**

`tests/game/test_game_script_host.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/game/game_script_host.hpp"
#include <string>
#include <string_view>
#include <vector>

TEST_CASE("game script host logs an unsupported native once") {
    std::vector<std::string> messages;
    gl::script::NativeServices services;
    services.log = [&](std::string_view text) { messages.emplace_back(text); };
    gl::game::GameScriptHost host(services);
    const auto args = std::span<const gl::script::Value>{};
    CHECK(std::get<std::int32_t>(host.call(0x4000000, args)) == 0);
    CHECK(std::get<std::int32_t>(host.call(0x4000000, args)) == 0);
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("RS_GetPersonParameterI") != std::string::npos);
    CHECK(messages.front().find("unimplemented") != std::string::npos);
}

TEST_CASE("game script host dispatches supported natives") {
    std::string console;
    gl::script::NativeServices services;
    services.console = [&](std::string_view text) { console = std::string(text); };
    gl::game::GameScriptHost host(services);
    const gl::script::Value argument = std::string("hello");
    host.call(0x1000002, {&argument, 1});
    CHECK(console == "hello");
}

TEST_CASE("game script host ignores unknown ids") {
    gl::game::GameScriptHost host;
    const auto args = std::span<const gl::script::Value>{};
    CHECK(std::get<std::int32_t>(host.call(0xDEADBEEF, args)) == 0);
}
```

- [x] **Step 2: Run the tests to verify they fail**

Run: `cmake --build build` — expected error: `game_script_host.hpp` missing.

- [x] **Step 3: Write the header**

`src/game/include/gl/game/game_script_host.hpp`:

```cpp
#pragma once
#include "gl/script/host.hpp"
#include "gl/script/native_registry.hpp"
#include <cstdint>
#include <set>
#include <span>

namespace gl::game {
class GameScriptHost : public gl::script::ScriptHost {
  public:
    explicit GameScriptHost(gl::script::NativeServices services = {});
    gl::script::Value call(std::uint32_t id,
                           std::span<const gl::script::Value> args) override;
    gl::script::NativeServices& services() { return services_; }

  private:
    gl::script::NativeServices services_;
    std::set<std::uint32_t> logged_;
};
} // namespace gl::game
```

- [x] **Step 4: Write the implementation**

`src/game/src/game_script_host.cpp`:

```cpp
#include "gl/game/game_script_host.hpp"
#include "gl/assets/cs.hpp"
#include "gl/assets/log.hpp"
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace gl::game {
GameScriptHost::GameScriptHost(gl::script::NativeServices services)
    : services_(std::move(services)) {}

gl::script::Value GameScriptHost::call(std::uint32_t id,
                                       std::span<const gl::script::Value> args) {
    const auto* info = gl::script::NativeRegistry::find(id);
    if (info == nullptr) {
        gl::log(gl::LogLevel::warning,
                "script host: unknown function id " + std::to_string(id));
        return std::int32_t{0};
    }

    if (info->support == gl::script::NativeSupport::vm_internal) {
        gl::log(gl::LogLevel::warning,
                std::string("script host: vm-internal native reached host: ") +
                    info->name());
        return std::int32_t{0};
    }

    if (info->support == gl::script::NativeSupport::unsupported) {
        if (logged_.insert(id).second) {
            char hex[16];
            std::snprintf(hex, sizeof(hex), "0x%X", id);
            const std::string message =
                std::string("script native ") + info->name() + " (" + hex +
                "): unimplemented — " + std::string(info->reason);
            if (services_.log)
                services_.log(message);
            else
                gl::log(gl::LogLevel::info, message);
        }
        return std::int32_t{0};
    }

    return gl::script::NativeRegistry::call(*info, args, services_);
}
} // namespace gl::game
```

- [x] **Step 5: Link gl_script into gl_game**

`src/game/CMakeLists.txt`:
- add `src/game_script_host.cpp` to `gl_game`;
- change dependencies to `target_link_libraries(gl_game PUBLIC gl_assets gl_script)`.

`tests/game/CMakeLists.txt`: add `test_game_script_host.cpp`.

- [x] **Step 6: Run the tests to verify they pass**

Run:
```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case=*script_host*
```
Expected: 3 test cases pass.

- [x] **Step 7: Commit**

```bash
git add src/game/include/gl/game/game_script_host.hpp src/game/src/game_script_host.cpp \
  src/game/CMakeLists.txt tests/game/test_game_script_host.cpp tests/game/CMakeLists.txt
git commit -m "feat(game): dispatch script natives through GameScriptHost"
```

---

### Task 5: Apply VM parity deltas from research

**Files:**
- Modify: `src/script/src/script_vm.cpp`
- Modify: `tests/script/test_vm.cpp`
- Modify: `docs/DECISIONS.md` (if a change overrides a port decision)

**Interfaces:**
- Consumes: `docs/research/p3-vm-natives-server.md` §4 delta table.
- Produces: `script_vm.cpp` matching every `port-fix` row; one test per row.

- [x] **Step 1: List the port-fix rows**

Open `docs/research/p3-vm-natives-server.md` §4 and copy every row whose
verdict is `port-fix` into a checklist with: opcode, input values, Server
result, current port result.

- [x] **Step 2: Add one failing test per delta**

For each row append a test to `tests/script/test_vm.cpp` using the node
builder already used in that file. Example shape:

```cpp
TEST_CASE("vm opcode <N> matches Server.dll <RVA>") {
    VmHarness h;
    const std::int32_t left = h.num(6.0);
    const std::int32_t right = h.num(4.0);
    const std::int32_t expr = h.node(<N>, left, right);
    const std::int32_t stop = h.jmp(-1, -1);
    const std::int32_t assign = h.assign(0, expr, stop);
    h.run_to_end(assign);
    CHECK(h.variable_int("result") == <expected>);
}
```

- [x] **Step 3: Run the tests to verify they fail**

Run: `build\bin\gl_script_tests.exe --test-case=*Server.dll*`
Expected: each new test fails with the port value, not the Server value.

- [x] **Step 4: Fix script_vm.cpp**

Change only the branches named in the delta rows. Keep existing behavior for
all `match`/`unverified` rows. If a fix changes a documented decision
(D-023/D-024/D-025), append a new `DECISIONS.md` entry before coding.

- [x] **Step 5: Run the tests to verify they pass**

Run:
```powershell
cmake --build build
build\bin\gl_script_tests.exe
```
Expected: all VM tests pass, including the new Server.dll cases.

- [x] **Step 6: Commit**

```bash
git add src/script/src/script_vm.cpp tests/script/test_vm.cpp docs/DECISIONS.md
git commit -m "fix(script): match Server.dll VM semantics"
```

---

### Task 6: Wire GameScriptHost into tools

**Files:**
- Modify: `tools/glgame/main.cpp` (`DialogContext::host`, `GameScriptHost` construction)
- Modify: `tools/glscript/main.cpp` (host construction)

**Interfaces:**
- Consumes: `gl::game::GameScriptHost`, existing `CommandRegistry` in `glgame`.
- Produces: dialogs executed in `glgame` go through the registry; `glscript`
  keeps the same CLI output.

- [x] **Step 1: Switch glgame to GameScriptHost**

In `tools/glgame/main.cpp` replace
`gl::script::DefaultScriptHost host;` in `DialogContext` with
`gl::game::GameScriptHost host;`. The command registry member is `registry_`;
wire services right after `auto context = std::make_unique<DialogContext>();`
(before `ScriptVm` is constructed):

```cpp
context->host.services().console = [](std::string_view text) {
    gl::log(gl::LogLevel::info, "script console: " + std::string(text));
};
context->host.services().command = [this](std::string_view text) {
    const auto result = registry_.execute(text);
    if (!result) gl::log(gl::LogLevel::warning, "script cmd: " + result.error().message);
};
context->host.services().signal = [](std::string_view text) {
    gl::log(gl::LogLevel::info, "script signal: " + std::string(text));
};
context->host.services().play_sound = [](std::string_view name) {
    gl::log(gl::LogLevel::debug, "script sound: " + std::string(name));
};
context->host.services().exit = [](std::int32_t code) {
    gl::log(gl::LogLevel::info, "script exit: " + std::to_string(code));
};
```

Add `#include <gl/game/game_script_host.hpp>` and `#include <cstdint>` if
missing.

- [x] **Step 2: Switch glscript to GameScriptHost**

In `tools/glscript/main.cpp` replace `gl::script::DefaultScriptHost host;`
with:

```cpp
gl::script::NativeServices services;
services.console = [](std::string_view text) {
    gl::log(gl::LogLevel::info, "script console: " + std::string(text));
};
gl::game::GameScriptHost host(services);
```

Add `gl_game` to the `glscript` target link libraries if it is not already
linked (the translation unit needs `game_script_host.cpp`).

- [x] **Step 3: Build and run the CLI smoke tests**

Run:
```powershell
cmake --build build
build\bin\glscript.exe --game-dir "I:\Games\Златогорье-2" --file scripts/dialogs/demon.d1.age.cs --run
```
Expected: `status waiting_user`/`wait_user`, `steps 43`,
`dialogs 2684 2686 2696`, `answers 2` (same as `p3a-cs-vm.md`).

- [x] **Step 4: Commit**

```bash
git add tools/glgame/main.cpp tools/glscript/main.cpp tools/glscript/CMakeLists.txt
git commit -m "feat(tools): run dialogs through GameScriptHost"
```

---

### Task 7: Regression, docs, status

**Files:**
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`
- Modify: `docs/research/parity-matrix.md`

- [x] **Step 1: Run the targeted test set**

```powershell
$env:PATH="I:\goldenland2dec\.local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin;I:\goldenland2dec\.local\toolchains\ninja;I:\goldenland2dec\.local\toolchains\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure -R "gl_script_tests|gl_script_real_tests|gl_game_tests|glgame_screenshot_tests|glscript_tests"
```

Expected: all selected tests pass; record exact counts/times.

- [x] **Step 2: Run the full test suite**

```powershell
ctest --test-dir build --output-on-failure
```

Expected: full pass; record `N/N passed`, duration, exit code.

- [x] **Step 3: Update the parity matrix**

In `docs/research/parity-matrix.md`:
- `P3-4`–`P3-8`: add the new RVA evidence and set `verified`/`partial`
  per Task 2 §4 verdicts;
- `P3-9`: set `partial`, evidence = registry + supported IDs (5/65), gap =
  remaining 57 natives require subsystems;
- add a line in the summary for the native dispatch infrastructure.

- [x] **Step 4: Update STATUS and PROJECT**

`docs/STATUS.md` Phase 1a section: what was done, exact test output, next
step (Phase 1b: person natives or `P2-15`).
`docs/PROJECT.md` P3 row: `NativeRegistry` + `GameScriptHost`, supported
5/65, remaining gaps.

- [x] **Step 5: Verify document encoding**

```powershell
$files = 'docs\STATUS.md','docs\PROJECT.md','docs\research\parity-matrix.md','docs\research\p3-vm-natives-server.md'
foreach ($f in $files) {
  $b = [System.IO.File]::ReadAllBytes($f)
  "$f bom=$($b.Length -ge 3 -and $b[0] -eq 0xEF) nul=$($b -contains 0)"
}
```

Expected: `bom=False nul=False`.

- [x] **Step 6: Commit**

```bash
git add docs/STATUS.md docs/PROJECT.md docs/research/parity-matrix.md
git commit -m "docs: record Phase 1a VM parity and native dispatch"
```
