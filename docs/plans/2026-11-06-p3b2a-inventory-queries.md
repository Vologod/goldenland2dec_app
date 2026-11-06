# Phase 1b2a — Hero inventory query natives (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Реализовать `RS_TestPersonHasItem`, `RS_GetItemCountI`, `RS_GetMoney` на `HeroInventory` из save с TECH-именами из `tech_names.sdb`.

**Architecture:** `gl_assets::ItemNameIndex` даёт `tech name → item id`; `gl_script::InventoryAccess` — callbacks; `gl_game::InventoryRuntime` хранит counts/money Hero; `glgame` связывает runtime с `GameScriptHost`.

**Tech Stack:** C++20, CMake + Ninja, doctest, Ghidra/llvm-objdump (research), PowerShell 5.1.

## Global Constraints

- `Server.dll` SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Поведение — только из `Server.dll` (D-029); без догадок.
- Слои только вниз: `game → script → assets`; `gl_script` без `gl_game`/SDL.
- Документы UTF-8 без BOM, `nul=0`.
- Коммиты по задачам разрешены пользователем 2026-11-06.
- Перед каждым шагом сверять `Get-Date`; в окне 21:00–05:00 — checkpoint.

---

### Task 1: Research inventory query semantics

**Files:**
- Create: `docs/research/p3b2a-inventory-queries-server.md`

- [ ] **Step 1: Confirm natives**

Record from `.local/decomp/server/natives/natives.c` for `0x5000000`,
`0x5000002`, `0x5000008`: arity, arg types, return, default constants
(`0x91398=1.0`, `0x913a0=0.0`, `0x91408=0.0`, `0x91478=0.0`), and side
effects.

- [ ] **Step 2: Confirm container/item resolution**

Record `FUN_1404c574` (`0x4c574`) TECH→id + sum count, `FUN_1404c494`
(`0x4c494`) type `0x12` sum, and `entity+0x10` container mapping
(`FUN_1404bf8c`, P5b1 layout). Cite `decompiled.c` lines.

- [ ] **Step 3: Validate tech names**

Parse `sdb/items/tech_names.sdb` (use `glpak decode-sdb` or `Sdb`); record
1848 entries, uniqueness, exact case; mapping for `MON_1_0_1`,
`QST_1_0_159`, `ARM_1_08_2`.

- [ ] **Step 4: Compute real oracle values**

Decode `Saves/slot0.sav`/`slot2.sav`, `parse_hero_inventory`, and record
`items` counts for `MON_1_0_1`, `QST_1_0_159`, `ARM_1_08_2` and money
(sum type 18). Put the exact numbers in the doc.

- [ ] **Step 5: Choose a real dialog**

Scan real `.cs` for `RS_GetItemCountI` (83886082) or
`RS_TestPersonHasItem` (83886080) whose branch is observable via a script
variable/answer; record the script path, nodes, expected values for
slot0/slot2, and why it is deterministic.

- [ ] **Step 6: Record limitations**

Document `FUN_14029270` (EF_INVCONTAINER boxes) and NPC `.inv` loot as
out of scope; NPC/box queries return 0 in the port.

- [ ] **Step 7: Verify and commit**

UTF-8 no BOM/nul=0. Commit `docs(research): record inventory query natives`.

---

### Task 2: `gl::assets::ItemNameIndex`

**Files:**
- Create: `src/assets/include/gl/assets/item_name_index.hpp`
- Create: `src/assets/src/item_name_index.cpp`
- Modify: root `CMakeLists.txt` (`gl_assets` sources)
- Create: `tests/assets/test_item_name_index.cpp`
- Modify: `tests/assets/CMakeLists.txt`

**Interfaces:**
```cpp
namespace gl::assets {
class ItemNameIndex {
  public:
    static Result<ItemNameIndex> load(const GameVfs& vfs);
    std::optional<std::uint32_t> find(std::string_view tech_name) const;
  private:
    std::map<std::string, std::uint32_t, std::less<>> by_name_;
};
}
```

- [ ] **Step 1: Failing test**

```cpp
TEST_CASE("item name index resolves tech names exactly") {
    auto index = gl::assets::ItemNameIndex::load(vfs);
    REQUIRE(index);
    CHECK(index->find("MON_1_0_1") == 812);
    CHECK(index->find("ARM_1_08_2") == 307);
    CHECK(index->find("QST_1_0_159") == 1101);
    CHECK_FALSE(index->find("mon_1_0_1"));
    CHECK_FALSE(index->find("NOPE"));
}
```

Use `GL_GAME_DIR` + `GameVfs::mount` and `doctest::skip` in the real test;
add a synthetic test by exposing a test constructor or a `from_entries`
helper if needed.

- [ ] **Step 2: Implement**

Parse `sdb/items/tech_names.sdb` with `Sdb::parse`; insert `name → id`;
on duplicate keep first and log a warning (real data has none).

- [ ] **Step 3: Build/run**

`cmake --build build; build\bin\gl_assets_tests.exe --test-case=*item_name*`
(and the real binary if the test is real-data).

- [ ] **Step 4: Commit** `feat(assets): add item tech-name index`.

---

### Task 3: `gl_script` inventory access and handlers

**Files:**
- Modify: `src/script/include/gl/script/native_registry.hpp`
- Modify: `src/script/src/native_registry.cpp`
- Create: `tests/script/test_inventory_natives.cpp`
- Modify: `tests/script/CMakeLists.txt`
- Modify: `tests/script/test_native_registry.cpp`

**Interfaces:**
```cpp
struct InventoryAccess {
    std::function<std::int32_t(std::string_view person, std::string_view tech_name)> item_count;
    std::function<std::int32_t()> money;
};
```
`NativeServices::inventory`; handlers for `0x5000000`, `0x5000002`,
`0x5000008`; arity 2/2/0.

- [ ] **Step 1: Failing tests**

Cover: Test returns `1.0` when count>0 else `0.0`; Count returns count;
Money returns money; empty callbacks → `0.0`; wrong args → warning + `0.0`;
registry arity 2/2/0 and supported classification; the other inventory
IDs stay unsupported.

- [ ] **Step 2: Implement handlers**

Use the same argument-validation style as the person handlers:
`is_string`, `as_int`, `warn_native`; Test compares count > 0.

- [ ] **Step 3: Run `gl_script_tests`**; report counts.

- [ ] **Step 4: Commit** `feat(script): implement inventory query natives`.

---

### Task 4: `gl::game::InventoryRuntime`

**Files:**
- Create: `src/game/include/gl/game/inventory_runtime.hpp`
- Create: `src/game/src/inventory_runtime.cpp`
- Modify: root `CMakeLists.txt` (`gl_game` sources)
- Create: `tests/game/test_inventory_runtime.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Interfaces:**
```cpp
class InventoryRuntime {
  public:
    static InventoryRuntime create(const gl::assets::HeroInventory& hero,
                                   const gl::assets::ItemNameIndex& names);
    std::int32_t count(std::string_view person, std::string_view tech_name) const;
    std::int32_t money() const;
    gl::script::InventoryAccess access();
};
```

- [ ] **Step 1: Failing tests**

Synthetic `HeroInventory` with items `{812, 500, type 18}`, `{1101, 2,
type 0}`, `{307, 1, type 0}`:
- `count("Hero","MON_1_0_1") == 500`;
- `count("Hero","QST_1_0_159") == 2`;
- `count("Hero","ARM_1_08_2") == 1`;
- `count("Hero","NOPE") == 0`;
- `count("Other","MON_1_0_1") == 0`;
- `money() == 500`;
- empty inventory → counts/money 0;
- `access()` callbacks work.

- [ ] **Step 2: Implement**

Precompute `std::map<std::uint32_t, std::int32_t>` counts and `money_`
(sum type 18). `count` requires `person == "Hero"`; resolve tech via
`ItemNameIndex`, lookup id, return 0 if absent.

- [ ] **Step 3: Run `gl_game_tests --test-case=*inventory_runtime*`**.

- [ ] **Step 4: Commit** `feat(game): add hero inventory runtime`.

---

### Task 5: Wire into `glgame` + real test

**Files:**
- Modify: `tools/glgame/main.cpp`
- Create: `tests/script/test_real_inventory_natives.cpp`
- Modify: `tests/script/CMakeLists.txt` (link `gl_game`, add test)

- [ ] **Step 1: Load ItemNameIndex and build runtime**

In `Game::boot` load `ItemNameIndex` (log warning on failure). Add
`gl::game::InventoryRuntime inventory_runtime_;`. Rebuild it after
`apply_equip_save` parses `HeroInventory`; on invalid/missing save build
from an empty inventory. Wire
`context->host.services().inventory = inventory_runtime_.access();` in
`open_dialog`; also rebuild for the menu Test Dialog path.

- [ ] **Step 2: Real-script test**

Use the dialog chosen in Task 1: load via `GameVfs`, `parse_cs`,
`parse_globals`, build `InventoryRuntime` from slot0/slot2
`parse_hero_inventory`, run through `GameScriptHost` + `ScriptVm`, and
assert the branch/global matches the research oracle. Use
`doctest::skip(!game_dir_available())`.

- [ ] **Step 3: Real counts test**

Add a real test comparing `InventoryRuntime` counts/money for slot0/slot2
with the research numbers, plus `ItemNameIndex` lookups.

- [ ] **Step 4: CLI smoke**

`glscript demon.d1.age.cs --run` unchanged; `glgame --level l1_1
--click-npc 13 --screenshot` exit 0.

- [ ] **Step 5: Commit** `feat(tools): wire inventory queries into dialogs`.

---

### Task 6: Regression, docs, status

**Files:**
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`, `docs/DECISIONS.md`.

- [ ] **Step 1: Targeted tests**

`ctest -R "gl_script_tests|gl_script_real_tests|gl_game_tests|gl_assets_tests|gl_assets_real_tests|glgame_screenshot_tests"`.

- [ ] **Step 2: Full ctest** with `GL_GAME_DIR`; record counts/times/exit.

- [ ] **Step 3: Docs**

- `P3-9`: supported `8 → 11`; list the three IDs; note NPC/box limitation.
- `STATUS`: completed 1b2a section with exact outputs and next step.
- `PROJECT`: P3/P5 row update.
- `DECISIONS D-051`: Hero-only inventory source, TECH-name identity,
  money = type 18 sum, NPC/box queries 0 until 1b2b/c.

- [ ] **Step 4: Encoding check** (`bom=False nul=False`).

- [ ] **Step 5: Commit** `docs: record Phase 1b2a inventory queries`.
