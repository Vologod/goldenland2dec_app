# P2-15b3 — `BT_TRIBE` Relation Matrix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Store the `BT_TRIBE` 234×234 relation matrix and expose id-based relation/row/name lookup with the original default `2`.

**Architecture:** `SaveTribeTable` gains the matrix bytes and lookup methods; `parse_tribe` reads the matrix exactly instead of skipping it; `glgame --dump-save` reports the matrix size; a real 28-save oracle checks storage, entry-order indexing, default `2`, and the corpus histograms. No runtime faction/AI behavior is added.

**Tech Stack:** C++20, CMake + Ninja, doctest, project layers (`assets` → `tools/glgame`).

**Spec:** `docs/specs/2026-11-16-p2-15b3-tribe-matrix-design.md`.

## Global Constraints

- C++20; CMake ≥ 3.20 + Ninja; doctest; no new dependency.
- Layers downward; no SDL/platform dependency in `assets`.
- No comments in code unless they carry non-obvious parity information.
- Docs UTF-8 no BOM; no NUL/literal `\n` artifacts.
- Real tests skip when `GL_GAME_DIR` is unset.
- OQ-7 value labels remain unverified and are not claimed; only storage/lookup parity.
- Toolchain:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

---

### Task 1: Matrix storage and lookup API

**Files:**
- Modify: `src/assets/include/gl/assets/save_payload.hpp`
- Modify: `src/assets/src/save_payload.cpp`
- Test: `tests/assets/test_save_payload.cpp`

**Interfaces:**
- Produces: `SaveTribeTable.matrix`, `count()`, `index_of`, `find(id)`,
  `find(name)`, `relation(id_a,id_b)`, `row(index)`.

- [ ] **Step 1: Extend the unit tests**

In `tests/assets/test_save_payload.cpp`, replace the body of
`TEST_CASE("save payload: parses BT_TRIBE names")` checks with the
existing entry checks **plus**:

```cpp
    REQUIRE(payload->tribes.matrix.size() == 4u);
    CHECK(payload->tribes.matrix[0] == 0x30);
    CHECK(payload->tribes.matrix[1] == 0x31);
    CHECK(payload->tribes.matrix[2] == 0x32);
    CHECK(payload->tribes.matrix[3] == 0x33);
    CHECK(payload->tribes.count() == 2u);
    CHECK(payload->tribes.index_of(5) == 0u);
    CHECK(payload->tribes.index_of(9) == 1u);
    CHECK_FALSE(payload->tribes.index_of(7).has_value());
    REQUIRE(payload->tribes.find(9) != nullptr);
    CHECK(payload->tribes.find(9)->name == "hills");
    REQUIRE(payload->tribes.find("hills") != nullptr);
    CHECK(payload->tribes.find("hills")->id == 9u);
    CHECK(payload->tribes.relation(5, 5) == 0x30);
    CHECK(payload->tribes.relation(5, 9) == 0x31);
    CHECK(payload->tribes.relation(9, 5) == 0x32);
    CHECK(payload->tribes.relation(9, 9) == 0x33);
    CHECK(payload->tribes.relation(5, 7) == 2);
    CHECK(payload->tribes.row(1).size() == 2u);
    CHECK(payload->tribes.row(1)[0] == 0x32);
    CHECK(payload->tribes.row(2).empty());
```

Add new tests:

```cpp
TEST_CASE("save payload: tribe relation uses entry order and defaults to two") {
    std::vector<std::uint8_t> tribe;
    put_u32(tribe, 3);
    put_u32(tribe, 7);
    put_string(tribe, "a");
    put_u32(tribe, 1);
    put_string(tribe, "b");
    put_u32(tribe, 4);
    put_string(tribe, "c");
    for (std::uint8_t value : std::array<std::uint8_t, 9>{3, 5, 6, 7, 3, 8, 9, 10, 3})
        tribe.push_back(value);

    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), tribe), make_default_tail("L93_4")));
    REQUIRE(payload.has_value());
    const auto &table = payload->tribes;
    CHECK(table.count() == 3u);
    CHECK(table.index_of(7) == 0u);
    CHECK(table.index_of(1) == 1u);
    CHECK(table.index_of(4) == 2u);
    CHECK(table.relation(7, 1) == 5);
    CHECK(table.relation(1, 7) == 7);
    CHECK(table.relation(4, 4) == 3);
    CHECK(table.relation(1, 1) == 3);
    CHECK(table.relation(9, 1) == 2);
}

TEST_CASE("save payload: duplicate tribe names resolve to the first entry") {
    std::vector<std::uint8_t> tribe;
    put_u32(tribe, 2);
    put_u32(tribe, 1);
    put_string(tribe, "dup");
    put_u32(tribe, 2);
    put_string(tribe, "dup");
    for (std::uint8_t value : std::array<std::uint8_t, 4>{3, 0, 1, 3})
        tribe.push_back(value);

    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), tribe), make_default_tail("L93_4")));
    REQUIRE(payload.has_value());
    REQUIRE(payload->tribes.find("dup") != nullptr);
    CHECK(payload->tribes.find("dup")->id == 1u);
    CHECK(payload->tribes.find(2)->name == "dup");
}

TEST_CASE("save payload: rejects a truncated or oversized tribe matrix") {
    std::vector<std::uint8_t> truncated;
    put_u32(truncated, 2);
    put_u32(truncated, 5);
    put_string(truncated, "forest");
    put_u32(truncated, 9);
    put_string(truncated, "hills");
    truncated.push_back(0x30);

    auto short_payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), truncated), make_default_tail("L93_4")));
    REQUIRE_FALSE(short_payload.has_value());
    CHECK(short_payload.error().code == gl::ErrorCode::format);

    auto long_tribe = make_tribe_body();
    long_tribe.push_back(0x7F);
    auto long_payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), long_tribe), make_default_tail("L93_4")));
    REQUIRE_FALSE(long_payload.has_value());
    CHECK(long_payload.error().code == gl::ErrorCode::format);
}
```

`make_body`/`make_tribe_body` already exist in that file.

- [ ] **Step 2: Run to verify failure**

```powershell
cmake --build build --target gl_assets_tests
```

Expected: compile failure (matrix/methods missing).

- [ ] **Step 3: Extend the header**

In `save_payload.hpp` add `#include <string_view>` and replace
`SaveTribeTable` with:

```cpp
struct SaveTribeTable {
    std::vector<SaveTribeEntry> entries;
    std::vector<std::uint8_t> matrix;

    std::size_t count() const { return entries.size(); }
    std::optional<std::size_t> index_of(std::uint32_t id) const;
    const SaveTribeEntry *find(std::uint32_t id) const;
    const SaveTribeEntry *find(std::string_view name) const;
    std::int32_t relation(std::uint32_t id_a, std::uint32_t id_b) const;
    std::span<const std::uint8_t> row(std::size_t index) const;
};
```

- [ ] **Step 4: Implement the methods and matrix read**

In `save_payload.cpp` replace the matrix skip block in `parse_tribe`
(lines ~156-164) with:

```cpp
    const std::uint64_t matrix = static_cast<std::uint64_t>(count) * count;
    if (matrix != reader.remaining().size()) {
        detail = matrix > reader.remaining().size()
                     ? "relation matrix is truncated (" + std::to_string(matrix) +
                           " bytes expected)"
                     : "relation matrix has trailing bytes";
        return false;
    }
    std::span<const std::uint8_t> bytes;
    if (!reader.read_bytes(static_cast<std::size_t>(matrix), bytes)) {
        detail = "relation matrix is truncated";
        return false;
    }
    out.matrix.assign(bytes.begin(), bytes.end());
```

Add the method definitions after `parse_tribe`:

```cpp
std::optional<std::size_t> SaveTribeTable::index_of(std::uint32_t id) const {
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (entries[index].id == id)
            return index;
    }
    return std::nullopt;
}

const SaveTribeEntry *SaveTribeTable::find(std::uint32_t id) const {
    const auto index = index_of(id);
    return index ? &entries[*index] : nullptr;
}

const SaveTribeEntry *SaveTribeTable::find(std::string_view name) const {
    for (const SaveTribeEntry &entry : entries) {
        if (entry.name == name)
            return &entry;
    }
    return nullptr;
}

std::int32_t SaveTribeTable::relation(std::uint32_t id_a, std::uint32_t id_b) const {
    const auto a = index_of(id_a);
    const auto b = index_of(id_b);
    if (!a || !b || matrix.size() != entries.size() * entries.size())
        return 2;
    return matrix[*a * entries.size() + *b];
}

std::span<const std::uint8_t> SaveTribeTable::row(std::size_t index) const {
    if (index >= entries.size())
        return {};
    return std::span<const std::uint8_t>(matrix).subspan(index * entries.size(),
                                                         entries.size());
}
```

- [ ] **Step 5: Run**

```powershell
cmake --build build --target gl_assets_tests
build\bin\gl_assets_tests.exe --test-case="*tribe*"
build\bin\gl_assets_tests.exe --test-case="*save payload*"
```

Expected: all pass.

- [ ] **Step 6: Commit**

```powershell
git add src/assets/include/gl/assets/save_payload.hpp src/assets/src/save_payload.cpp tests/assets/test_save_payload.cpp
git commit -m "feat(assets): store tribe relation matrix"
```

---

### Task 2: Dump string and real 28-save matrix oracle

**Files:**
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp`
- Modify: `tests/assets/test_save_payload_real.cpp`

- [ ] **Step 1: Add the dump field**

In `tools/glgame/main.cpp`, the `--dump-save` line currently ends with
`" rginf_latch=" + std::to_string(selected.rginf.latch)`. Append a new
field so the final line ends with:

```cpp
        " rginf_latch=" + std::to_string(selected.rginf.latch) +
        " tribe_matrix=" + std::to_string(payload->tribes.count())
```

Keep every existing field and their order unchanged.

- [ ] **Step 2: E2E assertion**

In `tests/app/test_glgame_screenshot.cpp`, in the `--dump-save 0` case add:

```cpp
    CHECK(output.find("tribe_matrix=234") != std::string::npos);
```

- [ ] **Step 3: Real oracle**

Append to `tests/assets/test_save_payload_real.cpp`:

```cpp
TEST_CASE("real data: tribe relation matrix" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());

    std::size_t total_values = 0;
    std::array<std::size_t, 5> histogram{};
    std::size_t relation_mismatches = 0;
    std::size_t duplicate_id_saves = 0;
    std::vector<int> histogram_slot0{103, 232, 54186, 234, 1};

    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        const auto &table = payload->tribes;
        CHECK(table.count() == 234u);
        REQUIRE(table.matrix.size() == 54756u);

        std::set<std::uint32_t> ids;
        for (const auto &entry : table.entries) {
            if (!ids.insert(entry.id).second)
                ++duplicate_id_saves;
        }
        for (std::size_t i = 0; i < table.count(); ++i) {
            CHECK(table.relation(table.entries[i].id, table.entries[i].id) == 3);
            for (std::size_t j = 0; j < table.count(); ++j) {
                if (table.relation(table.entries[i].id, table.entries[j].id) !=
                    table.matrix[i * table.count() + j])
                    ++relation_mismatches;
            }
        }
        CHECK(table.relation(0xFFFFFFFFu, table.entries[0].id) == 2);
        CHECK(table.relation(table.entries[0].id, 0xFFFFFFFFu) == 2);
        if (slot == 0) {
            for (std::size_t value = 0; value < 5; ++value) {
                std::size_t count = 0;
                for (std::uint8_t byte : table.matrix)
                    if (byte == value)
                        ++count;
                CHECK(count == static_cast<std::size_t>(histogram_slot0[value]));
            }
        }
        for (std::uint8_t byte : table.matrix) {
            if (byte < histogram.size())
                ++histogram[byte];
            ++total_values;
        }
        if (slot == 0) {
            const std::vector<std::pair<std::uint32_t, std::string>> first{
                {106, "L12_3_citizen"},
                {16, "snake"},
                {97, "L11_3_citizen"},
                {105, "L12_2_citizen"},
                {96, "L11_2_citizen"},
                {39, "citizen"},
            };
            REQUIRE(table.entries.size() >= first.size());
            for (std::size_t index = 0; index < first.size(); ++index) {
                CHECK(table.entries[index].id == first[index].first);
                CHECK(table.entries[index].name == first[index].second);
            }
            const std::array<std::uint8_t, 16> row{
                3, 2, 2, 2, 2, 2, 2, 0, 2, 0, 0, 2, 0, 0, 2, 0};
            REQUIRE(table.row(0).size() == 234u);
            for (std::size_t index = 0; index < row.size(); ++index)
                CHECK(table.row(0)[index] == row[index]);
        }
    }
    CHECK(duplicate_id_saves == 0);
    CHECK(relation_mismatches == 0);
    CHECK(total_values == 1533168u);
    CHECK(histogram[0] == 1257u);
    CHECK(histogram[1] == 6096u);
    CHECK(histogram[2] == 1519260u);
    CHECK(histogram[3] == 6552u);
    CHECK(histogram[4] == 3u);
}
```

Add `<set>` and `<utility>` includes if missing. `read_save` and
`game_dir_available` already exist in that file.

- [ ] **Step 4: Run**

```powershell
cmake --build build --target gl_assets_real_tests glgame_screenshot_tests
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\gl_assets_real_tests.exe --test-case="*tribe*"
build\bin\glgame_screenshot_tests.exe --test-case="*dump-save*"
```

Expected: pass.

- [ ] **Step 5: Commit**

```powershell
git add tools/glgame/main.cpp tests/app/test_glgame_screenshot.cpp tests/assets/test_save_payload_real.cpp
git commit -m "test(assets): cover tribe matrix"
```

---

### Task 3: Docs and full verification

**Files:**
- Modify: `docs/DECISIONS.md`, `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`, `docs/research/p2-15a-save-payload.md`

- [ ] **Step 1: Append D-076**

```markdown
| D-076 | 2026-11-16 | P2-15b3 `BT_TRIBE` relation matrix. `SaveTribeTable.matrix` stores the `count²` row-major bytes in entry-list order (not id order); `parse_tribe` now reads the matrix exactly instead of skipping it and rejects truncation/trailing bytes. `index_of(id)`/`find(id)` use sparse ids; `find(name)` is first-match because names duplicate in all 28 saves; `relation(id_a,id_b)` maps ids to entry indices and returns the byte, or the original `FUN_14016998` default `2` when an id is missing or the matrix size is inconsistent; `row(index)` exposes a row span. Real 28 saves: count 234, matrix 54,756 bytes, diagonal 3, 1,533,168 values total, histogram `{0:1257,1:6096,2:1519260,3:6552,4:3}`, ids unique 28/28, relation/matrix mismatches 0; slot0 first entries `(106,L12_3_citizen),(16,snake),(97,L11_3_citizen),(105,L12_2_citizen),(96,L11_2_citizen),(39,citizen)` and row0 `3,2,2,2,2,2,2,0,2,0,0,2,0,0,2,0`; slot0/slot2/slot21 histograms pinned; `--dump-save` reports `tribe_matrix=234`. OQ-7 labels 0–4 remain unverified; no runtime faction/AI application is claimed (no port consumer yet); `PersonInstance.tribe_id`, save write (`P2-15c`) and live oracle remain out of scope. | spec/plan 2026-11-16 p2-15b3; research `p2-15a-save-payload.md` §`BT_TRIBE` matrix evidence/OQ-7; D-071…D-075. |
```

- [ ] **Step 2: STATUS/PROJECT/parity/research**

`STATUS.md`: replace the b3 design section with a done section: what was
implemented, exact focused/full outputs, residuals (OQ-7 labels
unverified, no AI consumer, unique ids not enforced, save write), `Статус:
done`.

`PROJECT.md` P2 row: append `P2-15b3 BT_TRIBE matrix done (2026-11-16, D-076: 234×234 stored, relation API default 2, 28/28 0 mismatches)`.

`parity-matrix.md` P2-15 row: replace the `BT_TRIBE` gap wording with
`BT_TRIBE matrix done (D-076): 234×234 stored/lookup, 28/28 0 mismatches; OQ-7 labels unverified`;
append `; D-076` to the evidence cell.

`research/p2-15a-save-payload.md`: append a short implementation result:
matrix stored/exposed, relation uses entry order, default 2, real 0/N.

- [ ] **Step 3: Verify docs encoding**

```powershell
$docs = @("docs/DECISIONS.md","docs/STATUS.md","docs/PROJECT.md","docs/research/parity-matrix.md","docs/research/p2-15a-save-payload.md")
foreach ($doc in $docs) {
  $b=[IO.File]::ReadAllBytes((Resolve-Path $doc))
  "nul=$((($b | Where-Object {$_ -eq 0}).Count)) $doc"
}
```

Expected: `nul=0`; changed lines contain no literal `\n`.

- [ ] **Step 4: Full verification**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```

Use a generous timeout (≥ 2400000 ms). Expected: all tests pass, exit 0;
record the real total/time in `STATUS.md`.

- [ ] **Step 5: Commit**

```powershell
git add docs/DECISIONS.md docs/STATUS.md docs/PROJECT.md docs/research/parity-matrix.md docs/research/p2-15a-save-payload.md
git commit -m "docs: record tribe relation matrix"
```

---

## Self-Review

- Spec coverage: storage/API/parser (Task 1), dump/E2E/real oracle (Task 2),
  docs/full verification (Task 3).
- No placeholders: code and pins are concrete (entry order, default 2,
  histograms, row bytes).
- Type consistency: `matrix`, `count()`, `index_of`, `find`, `relation`,
  `row` are used with the same signatures across tasks and spec.
- Non-claim stated: OQ-7 labels remain unverified; no runtime application.
