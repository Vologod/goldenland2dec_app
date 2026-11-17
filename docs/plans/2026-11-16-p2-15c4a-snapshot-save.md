# P2-15c4a — Snapshot Save Write Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement task-by-task.

**Goal:** Write the loaded save payload to an explicit path: patched header date/title, encoded container, atomic temp-file write, and a glgame CLI hook.

**Architecture:** `assets` parses the SYSTEMTIME weekday and patches `header_raw`; `core` writes bytes via tmp+rename; `glgame` keeps the loaded payload, patches the header from the local clock/title, encodes, and writes only the explicit `--write-save` path. Snapshot only — no state serialization yet.

**Tech Stack:** C++20, CMake + Ninja, doctest, `assets`/`core`/`tools/glgame`.

**Spec:** `docs/specs/2026-11-16-p2-15c4a-snapshot-save-design.md`.

## Global Constraints

- C++20; no new dependency; layers downward.
- No comments unless parity-critical.
- Only explicit `--write-save` paths; never implicit `Saves/` writes.
- Snapshot only (HERO/tail/ENTS raw); decode-equality with source block bodies.
- Real tests skip without `GL_GAME_DIR`.
- Toolchain:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

---

### Task 1: Weekday + header patch

**Files:**
- Modify: `src/assets/include/gl/assets/save.hpp`
- Modify: `src/assets/src/save.cpp`
- Modify: `src/assets/include/gl/assets/save_payload.hpp`
- Modify: `src/assets/src/save_payload.cpp`
- Test: `tests/assets/test_save_payload.cpp`

- [ ] **Step 1: Failing tests**

Append to `tests/assets/test_save_payload.cpp`:

```cpp
TEST_CASE("patch_save_header writes version, date and title") {
    auto payload = gl::assets::parse_save_payload(
        make_save(make_body("L93_4", make_vars()), make_default_tail("L93_4")));
    REQUIRE(payload.has_value());

    gl::assets::SaveHeader header;
    header.version = 17;
    header.year = 2026;
    header.month = 11;
    header.weekday = 2;
    header.day = 16;
    header.time_a = 12;
    header.time_b = 34;
    header.time_c = 56;
    header.title = "TestSave";
    const auto patched = gl::assets::patch_save_header(payload.value(), header);
    REQUIRE(patched.has_value());

    CHECK(payload->header_raw[0x14] == 17);
    const auto u16 = [&payload](std::size_t offset) {
        return static_cast<std::uint16_t>(payload->header_raw[offset]) |
               (static_cast<std::uint16_t>(payload->header_raw[offset + 1]) << 8);
    };
    CHECK(u16(0x18) == 2026);
    CHECK(u16(0x1a) == 11);
    CHECK(u16(0x1c) == 2);
    CHECK(u16(0x1e) == 16);
    CHECK(u16(0x20) == 12);
    CHECK(u16(0x22) == 34);
    CHECK(u16(0x24) == 56);
    CHECK(payload->header_raw[0x28] == 'T');
    CHECK(payload->header_raw[0x28 + header.title.size()] == 0);

    header.title = std::string(65, 'x');
    const auto too_long = gl::assets::patch_save_header(payload.value(), header);
    REQUIRE_FALSE(too_long.has_value());
    CHECK(too_long.error().code == gl::ErrorCode::format);
}

TEST_CASE("save header parses the weekday word") {
    auto file = make_save(make_body("L93_4", make_vars()), make_default_tail("L93_4"));
    file[0x1c] = 3;
    file[0x1d] = 0;
    const auto header = gl::assets::parse_save_header(file);
    REQUIRE(header.has_value());
    CHECK(header->weekday == 3);
}
```

- [ ] **Step 2: Run red**

```powershell
cmake --build build --target gl_assets_tests
```

- [ ] **Step 3: Implement**

`save.hpp`: add `std::uint16_t weekday = 0;` after `month`.
`save.cpp`: replace `reader.skip(2)` with `reader.read_u16le(header.weekday)`.

`save_payload.hpp` after `encode_tribe_body`:

```cpp
VoidResult patch_save_header(SavePayload &payload, const SaveHeader &header);
```

`save_payload.cpp` (after `encode_tribe_body`):

```cpp
VoidResult patch_save_header(SavePayload &payload, const SaveHeader &header) {
    const std::string title = utf8_to_cp1251(header.title);
    if (title.size() > 64)
        return Error{ErrorCode::format, "save title is longer than 64 bytes"};
    const auto put_u16 = [&payload](std::size_t offset, std::uint16_t value) {
        payload.header_raw[offset] = static_cast<std::uint8_t>(value & 0xFF);
        payload.header_raw[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    };
    payload.header_raw[0x14] = static_cast<std::uint8_t>(header.version & 0xFF);
    payload.header_raw[0x15] = static_cast<std::uint8_t>((header.version >> 8) & 0xFF);
    payload.header_raw[0x16] = static_cast<std::uint8_t>((header.version >> 16) & 0xFF);
    payload.header_raw[0x17] = static_cast<std::uint8_t>((header.version >> 24) & 0xFF);
    put_u16(0x18, header.year);
    put_u16(0x1a, header.month);
    put_u16(0x1c, header.weekday);
    put_u16(0x1e, header.day);
    put_u16(0x20, header.time_a);
    put_u16(0x22, header.time_b);
    put_u16(0x24, header.time_c);
    for (std::size_t index = 0; index < 64; ++index)
        payload.header_raw[0x28 + index] = 0;
    for (std::size_t index = 0; index < title.size(); ++index)
        payload.header_raw[0x28 + index] = static_cast<std::uint8_t>(title[index]);
    return VoidResult{};
}
```

- [ ] **Step 4: Run**

```powershell
cmake --build build --target gl_assets_tests
build\bin\gl_assets_tests.exe --test-case="*patch_save_header*,*weekday*"
```

- [ ] **Step 5: Commit**

```powershell
git add src/assets/include/gl/assets/save.hpp src/assets/src/save.cpp src/assets/include/gl/assets/save_payload.hpp src/assets/src/save_payload.cpp tests/assets/test_save_payload.cpp
git commit -m "feat(assets): patch save headers"
```

---

### Task 2: Core file write + glgame hook + E2E

**Files:**
- Modify: `src/core/include/gl/core/save_manager.hpp`
- Modify: `src/core/src/save_manager.cpp`
- Test: `tests/core/test_save_manager.cpp`
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp`

- [ ] **Step 1: Core test**

Append to `tests/core/test_save_manager.cpp`:

```cpp
TEST_CASE("write_save_file writes, overwrites and leaves no temp file") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "gl_save_write_test.sav";
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path.string() + ".tmp", ec);

    const std::vector<std::uint8_t> first{1, 2, 3};
    REQUIRE(gl::core::write_save_file(path, first).has_value());
    {
        std::ifstream in(path, std::ios::binary);
        const std::vector<std::uint8_t> read((std::istreambuf_iterator<char>(in)),
                                             std::istreambuf_iterator<char>());
        CHECK(read == first);
    }
    const std::vector<std::uint8_t> second{9, 8};
    REQUIRE(gl::core::write_save_file(path, second).has_value());
    {
        std::ifstream in(path, std::ios::binary);
        const std::vector<std::uint8_t> read((std::istreambuf_iterator<char>(in)),
                                             std::istreambuf_iterator<char>());
        CHECK(read == second);
    }
    CHECK_FALSE(std::filesystem::exists(path.string() + ".tmp"));
    std::filesystem::remove(path, ec);
}
```

Add `<fstream>`, `<iterator>`, `<vector>`, `<cstdint>` if missing.

- [ ] **Step 2: Implement core write**

`save_manager.hpp`:

```cpp
gl::VoidResult write_save_file(const std::filesystem::path &path,
                               std::span<const std::uint8_t> bytes);
```

Add `#include <span>`.

`save_manager.cpp`:

```cpp
gl::VoidResult write_save_file(const std::filesystem::path &path,
                               std::span<const std::uint8_t> bytes) {
    std::error_code ec;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
        return Error{ErrorCode::io, "cannot create save directory"};
    const std::string temp = path.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
            return Error{ErrorCode::io, "cannot open temporary save file"};
        out.write(reinterpret_cast<const char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!out)
            return Error{ErrorCode::io, "cannot write temporary save file"};
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        ec.clear();
        std::filesystem::rename(temp, path, ec);
    }
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return Error{ErrorCode::io, "cannot replace save file"};
    }
    return VoidResult{};
}
```

- [ ] **Step 3: glgame options and payload storage**

- `Options`: `std::filesystem::path write_save; bool has_write_save = false; std::string save_title = "Quick save";`
- Parse `--write-save <path>` and `--save-title <text>`; add help lines.
- Member `std::optional<gl::assets::SavePayload> loaded_payload_;`.
- In `load_save`, after the success log (`save: loaded ...`) store
  `loaded_payload_ = payload.value();`.
- In `load_level`, when `pending_restore_ == nullptr`, also
  `loaded_payload_.reset();`.

Add a method:

```cpp
    bool write_loaded_save() {
        if (!loaded_payload_) {
            gl::log(gl::LogLevel::error, "save write: no loaded save payload");
            return false;
        }
        gl::assets::SavePayload payload = *loaded_payload_;
        gl::assets::SaveHeader header = payload.header;
        header.title = options_.save_title;
        const std::time_t now = std::time(nullptr);
        const std::tm *local = std::localtime(&now);
        if (local != nullptr) {
            header.year = static_cast<std::uint16_t>(local->tm_year + 1900);
            header.month = static_cast<std::uint16_t>(local->tm_mon + 1);
            header.weekday = static_cast<std::uint16_t>(local->tm_wday);
            header.day = static_cast<std::uint16_t>(local->tm_mday);
            header.time_a = static_cast<std::uint16_t>(local->tm_hour);
            header.time_b = static_cast<std::uint16_t>(local->tm_min);
            header.time_c = static_cast<std::uint16_t>(local->tm_sec);
        }
        const auto patched = gl::assets::patch_save_header(payload, header);
        if (!patched) {
            gl::log(gl::LogLevel::error, "save write: " + patched.error().message);
            return false;
        }
        const auto encoded = gl::assets::encode_save_payload(payload);
        if (!encoded) {
            gl::log(gl::LogLevel::error, "save write: " + encoded.error().message);
            return false;
        }
        const auto written =
            gl::core::write_save_file(options_.write_save, encoded.value());
        if (!written) {
            gl::log(gl::LogLevel::error, "save write: " + written.error().message);
            return false;
        }
        gl::log(gl::LogLevel::info,
                "save write: path=" + options_.write_save.string() +
                    " bytes=" + std::to_string(encoded->size()));
        return true;
    }
```

In the CLI flow after a successful `--load-save` block:

```cpp
            if (options_.has_write_save && !write_loaded_save()) return 4;
```

- [ ] **Step 4: E2E**

Append a case to `tests/app/test_glgame_screenshot.cpp`:

```cpp
TEST_CASE("glgame --write-save writes a parseable snapshot") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path png = temp.path / "write_save.png";
    const fs::path out = temp.path / "out.sav";
    std::string output;
    const int code = run_glgame_capture(
        screenshot_arguments(game_dir, config, png,
                             {"--load-save", "0", "--write-save", out.string(),
                              "--save-title", "TestSave", "--seed", "1"}),
        output);
    CAPTURE(code);
    CAPTURE(output);
    CHECK(code == 0);
    CHECK(output.find("save write: path=") != std::string::npos);
    REQUIRE(fs::exists(out));

    const std::string source = read_file(fs::path(game_dir) / "Saves" / "slot0.sav");
    const auto source_payload = gl::assets::parse_save_payload(
        std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t *>(source.data()), source.size()));
    REQUIRE(source_payload.has_value());
    const std::string written = read_file(out);
    const auto written_payload = gl::assets::parse_save_payload(
        std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t *>(written.data()), written.size()));
    REQUIRE(written_payload.has_value());
    CHECK(written_payload->header.title == "TestSave");
    CHECK(written_payload->header.version == source_payload->header.version);
    CHECK(written_payload->info.level_id == source_payload->info.level_id);
    CHECK(written_payload->header.weekday <= 6);
    CHECK(written_payload->header.month >= 1);
    CHECK(written_payload->header.month <= 12);
    CHECK(written_payload->decompressed == source_payload->decompressed);
}
```

Add `#include "gl/assets/save_payload.hpp"` if missing; `read_file` already
returns `std::string`.

- [ ] **Step 5: Run**

```powershell
cmake --build build --target gl_core_tests glgame_screenshot_tests
build\bin\gl_core_tests.exe --test-case="*write_save_file*"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\glgame_screenshot_tests.exe --test-case="*write-save*"
```

- [ ] **Step 6: Commit**

```powershell
git add src/core/include/gl/core/save_manager.hpp src/core/src/save_manager.cpp tests/core/test_save_manager.cpp tools/glgame/main.cpp tests/app/test_glgame_screenshot.cpp
git commit -m "feat(tools): write snapshot saves"
```

---

### Task 3: Docs and full verification

**Files:**
- Modify: `docs/DECISIONS.md`, `docs/STATUS.md`, `docs/PROJECT.md`,
  `docs/research/parity-matrix.md`, `docs/research/p2-15a-save-payload.md`

- [ ] **Step 1: D-081**

```markdown
| D-081 | 2026-11-16 | P2-15c4a snapshot save write. `SaveHeader.weekday` is parsed from the SYSTEMTIME weekday word; `patch_save_header` writes version at `+0x14`, year/month/weekday/day/hour/minute/second at `+0x18..+0x24` and a cp1251 NUL-padded 64-byte title at `+0x28`, rejecting titles >64 bytes with `format`. `core::write_save_file` creates parent directories, writes `<path>.tmp`, renames over the target (remove+retry fallback) and leaves no temp on failure. `glgame --load-save ... --write-save <explicit path> [--save-title <text>]` stores the parsed payload, patches date/weekday/title from the local clock, encodes with c1 (typed VARS/TRIBE from c2; HERO/tail/ENTS raw as loaded) and logs `save write: path=.. bytes=..`; no implicit `Saves/` writes, `--write-save` without a loaded payload exits 4. Accepted as a **snapshot**: current HERO/tail/world state is not serialized yet (c3), slot UI/`game_save` is c4b; the written file parses and its decompressed block bodies equal the source payload, title/date/weekday patched. | spec/plan 2026-11-16 p2-15c4a; research `p2-15a-save-payload.md` §Writer/§P2-15c; D-079/D-080. |
```

- [ ] **Step 2: STATUS/PROJECT/parity/research**

STATUS: add a done section (implementation, focused/full outputs, residuals
snapshot-only/c3, slot UI c4b, best-effort atomicity, SYSTEMTIME inference).

PROJECT P2 row: append `P2-15c4a snapshot save write done (2026-11-16, D-081: explicit-path write, header patch, E2E parse)`.

parity-matrix P2-15 row: append `snapshot save write done (D-081): explicit path, header patch, no Saves/ overwrite; HERO/tail raw`; add `; D-081`.

research p2-15a: append implementation result (write path, snapshot
limitation).

- [ ] **Step 3: Encoding + full ctest**

```powershell
$docs = @("docs/DECISIONS.md","docs/STATUS.md","docs/PROJECT.md","docs/research/parity-matrix.md","docs/research/p2-15a-save-payload.md")
foreach ($doc in $docs) { $b=[IO.File]::ReadAllBytes((Resolve-Path $doc)); "nul=$((($b | Where-Object {$_ -eq 0}).Count)) $doc" }
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```

Timeout ≥ 2400000 ms; record real count/time/exit in STATUS.

- [ ] **Step 4: Commit**

```powershell
git add docs/DECISIONS.md docs/STATUS.md docs/PROJECT.md docs/research/parity-matrix.md docs/research/p2-15a-save-payload.md
git commit -m "docs: record snapshot save write"
```

---

## Self-Review

- Spec coverage: header weekday/patch (Task 1), core write + glgame/E2E
  (Task 2), docs/full (Task 3).
- No placeholders.
- Type consistency: `SaveHeader.weekday`, `patch_save_header`,
  `write_save_file`, `--write-save`/`--save-title` match the spec.
- Non-claims: snapshot only, no `Saves/` overwrite, SYSTEMTIME mapping is
  inferred from the layout.
