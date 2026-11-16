# P2-15b4b — BT_DIARY / BT_HSTRY Framing Parser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Parse `BT_DIARY` (entries of 0x58 blocks + sparse) and `BT_HSTRY` (head/40 slots/tail) into `SavePayload`, preserving raw bytes and exact consume.

**Architecture:** `assets` gains the model structs and two block parsers; `parse_save_payload` wires blocks 4/5; `glgame --dump-save` reports counts; unit/real/E2E tests pin the framing. No field/ring interpretation.

**Tech Stack:** C++20, CMake + Ninja, doctest, `assets` + `tools/glgame`.

**Spec:** `docs/specs/2026-11-16-p2-15b4b-framing-parser-design.md`.

## Global Constraints

- C++20; doctest; no new dependency; `assets` has no SDL/platform dependency.
- No comments unless parity-critical.
- Raw 0x58 blocks and HSTRY text are preserved; fields/ring are not interpreted.
- Real tests skip without `GL_GAME_DIR`.
- Toolchain:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

---

### Task 1: Model and parsers

**Files:**
- Modify: `src/assets/include/gl/assets/save_payload.hpp`
- Modify: `src/assets/src/save_payload.cpp`
- Test: `tests/assets/test_save_payload.cpp`

- [ ] **Step 1: Extend the unit fixtures and add tests**

In `tests/assets/test_save_payload.cpp`, add helpers after `make_tribe_body`:

```cpp
std::vector<std::uint8_t> make_empty_diary() {
    std::vector<std::uint8_t> body;
    put_u32(body, 0);
    put_u32(body, 0);
    return body;
}

std::vector<std::uint8_t> make_empty_history() {
    std::vector<std::uint8_t> body;
    put_u32(body, 0);
    for (int slot = 0; slot < 40; ++slot) {
        put_u32(body, 0);
        put_u32(body, 0);
    }
    put_u32(body, 0);
    return body;
}
```

Change `make_body` to accept optional diary/history and use the new defaults:

```cpp
std::vector<std::uint8_t> make_body(std::string_view level,
                                    const std::vector<std::uint8_t> &vars,
                                    const std::vector<std::uint8_t> &tribe = make_tribe_body(),
                                    const std::vector<std::uint8_t> &diary = make_empty_diary(),
                                    const std::vector<std::uint8_t> &history = make_empty_history()) {
    std::vector<std::uint8_t> d;
    put_block(d, "BT_SCRN ", {});
    put_block(d, "BT_INFO ", make_info(level));
    put_block(d, "BT_VARS ", vars);
    put_block(d, "BT_TRIBE", tribe);
    put_block(d, "BT_DIARY", diary);
    put_block(d, "BT_HSTRY", history);
    put_block(d, "BT_HERO ", {});
    return d;
}
```

Add tests:

```cpp
TEST_CASE("save payload: parses diary entries and sparse records") {
    std::vector<std::uint8_t> diary;
    put_u32(diary, 2);
    put_u32(diary, 1);
    diary.insert(diary.end(), 0x58, 0x11);
    put_u32(diary, 3);
    for (int block = 0; block < 3; ++block)
        for (int byte = 0; byte < 0x58; ++byte)
            diary.push_back(static_cast<std::uint8_t>(block * 0x10 + (byte & 0x0F)));
    put_u32(diary, 1);
    diary.insert(diary.end(), 0x58, 0x22);

    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), make_tribe_body(), diary,
                            make_empty_history()),
                  make_default_tail("L93_4")));
    REQUIRE(payload.has_value());
    REQUIRE(payload->diary.size() == 2u);
    CHECK(payload->diary[0].blocks.size() == 1u);
    CHECK(payload->diary[0].blocks[0][0] == 0x11);
    CHECK(payload->diary[1].blocks.size() == 3u);
    CHECK(payload->diary[1].blocks[2][0x10] == 0x20);
    REQUIRE(payload->diary_sparse.size() == 1u);
    CHECK(payload->diary_sparse[0][0] == 0x22);
}

TEST_CASE("save payload: parses history slots") {
    std::vector<std::uint8_t> history;
    put_u32(history, 7);
    for (int slot = 0; slot < 40; ++slot) {
        if (slot == 0)
            put_string(history, "first");
        else if (slot == 1)
            put_string(history, "");
        else
            put_u32(history, 0);
        put_u32(history, static_cast<std::uint32_t>(slot));
    }
    put_u32(history, 2);

    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), make_tribe_body(),
                            make_empty_diary(), history),
                  make_default_tail("L93_4")));
    REQUIRE(payload.has_value());
    CHECK(payload->history.head == 7u);
    CHECK(payload->history.tail == 2u);
    CHECK(payload->history.slots[0].text == "first");
    CHECK(payload->history.slots[0].id == 0u);
    CHECK(payload->history.slots[1].text.empty());
    CHECK(payload->history.slots[39].id == 39u);
}

TEST_CASE("save payload: rejects malformed diary and history framing") {
    std::vector<std::uint8_t> zero_blocks;
    put_u32(zero_blocks, 1);
    put_u32(zero_blocks, 0);
    auto diary_error = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), make_tribe_body(), zero_blocks,
                            make_empty_history()),
                  make_default_tail("L93_4")));
    REQUIRE_FALSE(diary_error.has_value());
    CHECK(diary_error.error().code == gl::ErrorCode::format);

    std::vector<std::uint8_t> short_history;
    put_u32(short_history, 1);
    for (int slot = 0; slot < 39; ++slot) {
        put_u32(short_history, 0);
        put_u32(short_history, 0);
    }
    put_u32(short_history, 1);
    auto history_error = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars(), make_tribe_body(),
                            make_empty_diary(), short_history),
                  make_default_tail("L93_4")));
    REQUIRE_FALSE(history_error.has_value());
    CHECK(history_error.error().code == gl::ErrorCode::format);
}
```

- [ ] **Step 2: Run to verify failure**

```powershell
cmake --build build --target gl_assets_tests
```

Expected: compile failure (types missing).

- [ ] **Step 3: Add the model**

In `save_payload.hpp`, before `SavePayload`:

```cpp
using SaveBlock58 = std::array<std::uint8_t, 0x58>;

struct SaveDiaryEntry {
    std::uint32_t key = 0;
    std::uint32_t declared_count = 0;
    std::vector<SaveBlock58> blocks;
};

struct SaveHistorySlot {
    std::string text;
    std::uint32_t id = 0;
};

struct SaveHistory {
    std::uint32_t head = 0;
    std::array<SaveHistorySlot, 40> slots;
    std::uint32_t tail = 0;
};
```

Add to `SavePayload`:

```cpp
    std::vector<SaveDiaryEntry> diary;
    std::vector<SaveBlock58> diary_sparse;
    SaveHistory history;
```

- [ ] **Step 4: Implement the parsers**

In `save_payload.cpp` (after `parse_tribe`; both are in the anonymous
namespace):

```cpp
bool parse_diary(std::span<const std::uint8_t> body,
                 std::vector<SaveDiaryEntry> &entries,
                 std::vector<SaveBlock58> &sparse, std::string &detail) {
    ByteReader reader(body);
    std::uint32_t count = 0;
    if (!reader.read_u32le(count)) {
        detail = "entry count is truncated";
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t blocks = 0;
        if (!reader.read_u32le(blocks)) {
            detail = "entry " + std::to_string(index) + " block count is truncated";
            return false;
        }
        if (blocks == 0) {
            detail = "entry " + std::to_string(index) + " has no header";
            return false;
        }
        std::span<const std::uint8_t> bytes;
        const std::uint64_t size = static_cast<std::uint64_t>(blocks) * 0x58;
        if (!reader.read_bytes(static_cast<std::size_t>(size), bytes)) {
            detail = "entry " + std::to_string(index) + " is truncated";
            return false;
        }
        SaveDiaryEntry entry;
        entry.key = u32_at(bytes, 0);
        entry.declared_count = u32_at(bytes, 4);
        entry.blocks.resize(blocks);
        for (std::uint32_t block = 0; block < blocks; ++block)
            std::memcpy(entry.blocks[block].data(),
                        bytes.data() + static_cast<std::size_t>(block) * 0x58, 0x58);
        entries.push_back(std::move(entry));
    }
    std::uint32_t sparse_count = 0;
    if (!reader.read_u32le(sparse_count)) {
        detail = "sparse count is truncated";
        return false;
    }
    std::span<const std::uint8_t> bytes;
    const std::uint64_t size = static_cast<std::uint64_t>(sparse_count) * 0x58;
    if (!reader.read_bytes(static_cast<std::size_t>(size), bytes)) {
        detail = "sparse records are truncated";
        return false;
    }
    sparse.resize(sparse_count);
    for (std::uint32_t block = 0; block < sparse_count; ++block)
        std::memcpy(sparse[block].data(),
                    bytes.data() + static_cast<std::size_t>(block) * 0x58, 0x58);
    if (!reader.at_end()) {
        detail = "has trailing bytes";
        return false;
    }
    return true;
}

bool parse_history(std::span<const std::uint8_t> body, SaveHistory &history,
                   std::string &detail) {
    ByteReader reader(body);
    if (!reader.read_u32le(history.head)) {
        detail = "head is truncated";
        return false;
    }
    for (auto &slot : history.slots) {
        if (!read_string(reader, slot.text)) {
            detail = "slot text is truncated";
            return false;
        }
        if (!reader.read_u32le(slot.id)) {
            detail = "slot id is truncated";
            return false;
        }
    }
    if (!reader.read_u32le(history.tail)) {
        detail = "tail is truncated";
        return false;
    }
    if (!reader.at_end()) {
        detail = "has trailing bytes";
        return false;
    }
    return true;
}
```

In `parse_save_payload` after the `BT_TRIBE` parse block:

```cpp
        const SaveBlockView &diary_view = payload.blocks[4];
        std::string diary_detail;
        if (!parse_diary(std::span<const std::uint8_t>(payload.decompressed)
                             .subspan(diary_view.offset, diary_view.size),
                         payload.diary, payload.diary_sparse, diary_detail))
            return format_error("BT_DIARY " + diary_detail);

        const SaveBlockView &history_view = payload.blocks[5];
        std::string history_detail;
        if (!parse_history(std::span<const std::uint8_t>(payload.decompressed)
                               .subspan(history_view.offset, history_view.size),
                           payload.history, history_detail))
            return format_error("BT_HSTRY " + history_detail);
```

- [ ] **Step 5: Run**

```powershell
cmake --build build --target gl_assets_tests
build\bin\gl_assets_tests.exe --test-case="*diary*"
build\bin\gl_assets_tests.exe --test-case="*history*"
build\bin\gl_assets_tests.exe --test-case="*save payload*"
```

Expected: all pass.

- [ ] **Step 6: Commit**

```powershell
git add src/assets/include/gl/assets/save_payload.hpp src/assets/src/save_payload.cpp tests/assets/test_save_payload.cpp
git commit -m "feat(assets): parse diary and history blocks"
```

---

### Task 2: Dump, E2E, real oracle

**Files:**
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp`
- Modify: `tests/assets/test_save_payload_real.cpp`

- [ ] **Step 1: Dump fields**

In `tools/glgame/main.cpp`, before the `const std::string line = "save: ..."`
dump-string construction, compute:

```cpp
    std::size_t diary_blocks = 0;
    for (const auto& entry : payload->diary)
        diary_blocks += entry.blocks.size();
```

Append to the `--dump-save` line after `tribe_matrix=`:

```cpp
        " diary=" + std::to_string(payload->diary.size()) +
        " diary_blocks=" + std::to_string(diary_blocks) +
        " sparse=" + std::to_string(payload->diary_sparse.size()) +
        " hstry_slots=40"
```

Keep all prior fields/order.

- [ ] **Step 2: E2E**

In the `--dump-save 0` case of `tests/app/test_glgame_screenshot.cpp`
add:

```cpp
    CHECK(output.find("diary=76 diary_blocks=371 sparse=0 hstry_slots=40") !=
          std::string::npos);
```

- [ ] **Step 3: Real oracle**

Append to `tests/assets/test_save_payload_real.cpp`:

```cpp
TEST_CASE("real data: diary framing" * doctest::skip(!game_dir_available())) {
    std::size_t entries = 0;
    std::size_t blocks = 0;
    std::size_t records = 0;
    std::size_t sparse = 0;
    std::size_t count_mismatches = 0;
    std::array<std::size_t, 28> per_slot_blocks{};

    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        entries += payload->diary.size();
        sparse += payload->diary_sparse.size();
        for (const auto &entry : payload->diary) {
            blocks += entry.blocks.size();
            records += entry.blocks.size() - 1;
            per_slot_blocks[slot] += entry.blocks.size();
            if (entry.declared_count != entry.blocks.size() - 1)
                ++count_mismatches;
        }
    }
    CHECK(entries == 1013u);
    CHECK(blocks == 4855u);
    CHECK(records == 3842u);
    CHECK(sparse == 0u);
    CHECK(count_mismatches == 0u);
    CHECK(per_slot_blocks[0] == 371u);
    CHECK(per_slot_blocks[2] == 377u);
    CHECK(per_slot_blocks[21] == 2u);

    auto slot0 = gl::assets::parse_save_payload(read_save(0));
    REQUIRE(slot0.has_value());
    REQUIRE(slot0->diary.size() == 76u);
    CHECK(slot0->diary[0].key == 0u);
    CHECK(slot0->diary[0].declared_count == 4u);
    CHECK(slot0->diary[0].blocks.size() == 5u);

    auto slot21 = gl::assets::parse_save_payload(read_save(21));
    REQUIRE(slot21.has_value());
    REQUIRE(slot21->diary.size() == 1u);
    CHECK(slot21->diary[0].key == 482u);
    CHECK(slot21->diary[0].declared_count == 1u);
    CHECK(slot21->diary[0].blocks.size() == 2u);
}

TEST_CASE("real data: history framing" * doctest::skip(!game_dir_available())) {
    std::size_t ids_zero = 0;
    std::size_t ids_total = 0;
    std::size_t tail_one = 0;
    std::size_t tail_two = 0;
    std::uint32_t head_min = 1000;
    std::uint32_t head_max = 0;

    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        CHECK(payload->history.slots.size() == 40u);
        head_min = std::min(head_min, payload->history.head);
        head_max = std::max(head_max, payload->history.head);
        if (payload->history.tail == 1)
            ++tail_one;
        if (payload->history.tail == 2)
            ++tail_two;
        for (const auto &slot_value : payload->history.slots) {
            ++ids_total;
            if (slot_value.id == 0)
                ++ids_zero;
        }
    }
    CHECK(ids_total == 1120u);
    CHECK(ids_zero == 1120u);
    CHECK(tail_one == 26u);
    CHECK(tail_two == 2u);
    CHECK(head_min == 1u);
    CHECK(head_max == 37u);

    auto slot0 = gl::assets::parse_save_payload(read_save(0));
    REQUIRE(slot0.has_value());
    CHECK(slot0->history.head == 5u);
    CHECK(slot0->history.tail == 1u);
    std::size_t non_empty = 0;
    for (const auto &slot_value : slot0->history.slots)
        if (!slot_value.text.empty())
            ++non_empty;
    CHECK(non_empty == 40u);
}
```

Add `<algorithm>` if missing.

- [ ] **Step 4: Run**

```powershell
cmake --build build --target gl_assets_real_tests glgame_screenshot_tests
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\gl_assets_real_tests.exe --test-case="*diary*"
build\bin\gl_assets_real_tests.exe --test-case="*history*"
build\bin\glgame_screenshot_tests.exe --test-case="*dump-save*"
```

Expected: pass.

- [ ] **Step 5: Commit**

```powershell
git add tools/glgame/main.cpp tests/app/test_glgame_screenshot.cpp tests/assets/test_save_payload_real.cpp
git commit -m "test(assets): cover diary and history framing"
```

---

### Task 3: Docs and full verification

**Files:**
- Modify: `docs/DECISIONS.md`, `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`, `docs/research/p2-15b4-diary-hstry.md`

- [ ] **Step 1: D-078**

```markdown
| D-078 | 2026-11-16 | P2-15b4b `BT_DIARY`/`BT_HSTRY` framing parser. `SavePayload.diary` stores entries (`key`, `declared_count`, raw `blocks` including the header plus record 0x58 blocks), `diary_sparse` stores raw sparse 0x58 records, and `history` stores `head`, 40 slots (`text` cp1251→UTF-8, `id`), `tail`. `parse_diary` reads `u32 entry_count`, per-entry `u32 block_count` (must be ≥1) and `block_count*0x58`, then `u32 sparse_count + sparse_count*0x58`; `parse_history` reads `head`, exactly 40 slots, `tail`; both require exact body consume and reject truncation/trailing with `format`. `declared_count` is stored but not validated against `blocks.size()-1` (the original copies without checking; the corpus invariant is oracle-only). Real 28 saves: 1,013 diary entries / 4,855 blocks / 3,842 records / sparse 0, header-count mismatches 0; slot0 76/371, slot2 75/377, slot21 1/2; slot0 first entry `key=0 declared=4 blocks=5`, slot21 `key=482 declared=1 blocks=2`. HSTRY: 40 slots 28/28, head 1..37, tail 1×26/2×2, ids 0 (1,120/1,120), slot0 head 5/tail 1/40 non-empty texts. `--dump-save` reports `diary=76 diary_blocks=371 sparse=0 hstry_slots=40`. 0x58 field labels, sparse semantics and HSTRY ring ordering remain `unverified` (D-077: 0x591b8/0x59f6c and `GoldenLand.exe` host methods); no journal UI/runtime application; save write (`P2-15c`) must emit the blocks/slots verbatim. | spec/plan 2026-11-16 p2-15b4b; research `p2-15b4-diary-hstry.md`; D-077. |
```

- [ ] **Step 2: STATUS/PROJECT/parity/research**

`STATUS.md`: replace the b4b design section with done (what, real
focused/full outputs, residuals: 0x58/ring semantics unverified, no UI,
raw-only, save write; `Статус: done`).

`PROJECT.md` P2 row: append `P2-15b4b DIARY/HSTRY framing done (2026-11-16, D-078: 1,013 entries/4,855 blocks/sparse 0, HSTRY 40 slots, raw preserved)`.

`parity-matrix.md` P2-15 row: replace the b4a research wording with
`BT_DIARY/BT_HSTRY framing done (D-078): 1,013 entries/4,855 blocks/sparse 0; HSTRY 40 slots; field/ring semantics unverified`;
append `; D-078` to the evidence cell.

`research/p2-15b4-diary-hstry.md`: append an implementation result
paragraph (framing stored, exact consume 28/28, raw preserved).

- [ ] **Step 3: Encoding check**

```powershell
$docs = @("docs/DECISIONS.md","docs/STATUS.md","docs/PROJECT.md","docs/research/parity-matrix.md","docs/research/p2-15b4-diary-hstry.md")
foreach ($doc in $docs) {
  $b=[IO.File]::ReadAllBytes((Resolve-Path $doc))
  "nul=$((($b | Where-Object {$_ -eq 0}).Count)) $doc"
}
```

- [ ] **Step 4: Full ctest**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```

Timeout ≥ 2400000 ms; record real count/time/exit in STATUS.

- [ ] **Step 5: Commit**

```powershell
git add docs/DECISIONS.md docs/STATUS.md docs/PROJECT.md docs/research/parity-matrix.md docs/research/p2-15b4-diary-hstry.md
git commit -m "docs: record diary and history framing"
```

---

## Self-Review

- Spec coverage: model/parsers (Task 1), dump/E2E/real (Task 2), docs/full
  verification (Task 3).
- No placeholders: code, fixtures and pins are concrete.
- Type consistency: `SaveBlock58`, `SaveDiaryEntry`, `SaveHistory`,
  `diary_sparse` used consistently.
- Non-claims preserved: 0x58 fields, sparse semantics and HSTRY ring are
  stored raw only; no runtime/UI.
