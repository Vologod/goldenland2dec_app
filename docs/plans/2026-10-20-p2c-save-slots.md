# P2c — Save Slot Framework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development or executing-plans. Steps use `- [ ]`.

**Goal:** Read the original `.sav` header, list save slots with metadata, show them in `glgame` (`Continue` → slot list), and expose `saves`/`--list-saves`. Read-only; no world deserialization or writing.

**Architecture:** `gl_assets` gains a `SaveHeader` parser; `gl_core` gains `SaveManager` (directory scan, metadata); `glgame` gains a slot-list state and CLI/console output.

**Tech Stack:** C++20, existing `gl_assets`/`gl_core`/`glgame`, doctest.

## Global Constraints

- C++20; `-Wall -Wextra -Werror`.
- `gl_assets`/`gl_core` SDL-free; `glgame` handles UI.
- Header format per spec §3: magic 20 B, version u32, year/month/day, times, title[64] cp1251 at offset 40; payload after 112 is opaque.
- Do not write `.sav`; `game_save` stays a stub.
- No game assets committed. Every task ends with a commit.

## File Structure

```
src/assets/include/gl/assets/save.hpp
src/assets/src/save.cpp
src/core/include/gl/core/save_manager.hpp
src/core/src/save_manager.cpp
tools/glgame/main.cpp
tests/assets/test_save.cpp
tests/core/test_save_manager.cpp
tests/assets/test_real_saves.cpp (or extend test_real_levels)
tests/app/test_glgame_screenshot.cpp (list-saves)
```

---

### Task 1: SaveHeader parser (`gl_assets`)

**Interface:**
```cpp
namespace gl::assets {
struct SaveHeader {
    std::uint32_t version = 0;
    std::uint16_t year = 0, month = 0, day = 0;
    std::uint16_t time_a = 0, time_b = 0, time_c = 0;
    std::string title;
};
Result<SaveHeader> parse_save_header(std::span<const std::uint8_t> data);
}
```
Parse: require `data.size() >= 112`; magic `"Goldenland II save \0"` (20 bytes); `version` at 20;
`year` at 24, `month` at 26, `day` at 30, `time_a/b/c` at 32/34/36; `title` = bytes 40..103 up to
the first NUL, cp1251→UTF-8, trim trailing spaces/NULs. Bad magic/short → `format`.

Tests: synthetic header (ASCII title `Quick save`; cp1251 title; empty title; no NUL within 64;
short; bad magic).

- [ ] TDD, implement, commit.

---

### Task 2: SaveManager (`gl_core`)

**Interface:**
```cpp
struct SaveSlotInfo {
    int slot = -1;
    std::filesystem::path path;
    std::uint64_t file_size = 0;
    std::filesystem::file_time_type modified;
    bool valid = false;
    gl::assets::SaveHeader header;
};
class SaveManager {
public:
    static gl::Result<SaveManager> scan(const std::filesystem::path& save_dir);
    const std::vector<SaveSlotInfo>& slots() const;
    const SaveSlotInfo* find(int slot) const;
    bool has_valid_slots() const;
};
```
Scan `slot<N>.sav` (case-insensitive), parse index, read the first 128 bytes, set `valid=false`
on parse failure; sort by slot; missing directory → empty list (not an error). Add a
`slot_index_from_filename` helper (tested).

Tests (synthetic): temp dir with `slot0.sav`, `slot3.sav`, `broken.sav`, `other.txt`;
indexes/sort/valid/find; missing dir; has_valid_slots.
Integration (`GL_GAME_DIR`): 28 slots, all valid, version 17, slot0 title `Quick save`,
slot16 contains `Семейство`, slot25 contains `Прохождение`; year/month match file mtimes.

- [ ] TDD, implement, commit.

---

### Task 3: `glgame` slot UI + CLI/console

- Load `SaveManager` at boot (`<game-dir>/Saves`).
- Menu: `Continue` enabled iff `has_valid_slots()`; activating opens `Save slots` state.
- `Save slots` screen: title + rows `Slot N  YYYY-MM-DD  title  (NNN KB)`; Up/Down/mouse;
  Enter → message `world deserialization not implemented (P2c)`; Esc → menu.
- Console command `saves` prints the same list (empty → `no saves`).
- CLI `--list-saves`: scan and print `slot<N>\tYYYY-MM-DD\t<title>\t<size>` lines, exit 0,
  no window/SDL needed (handle before window creation).
- Add an App state enum value for the save list (in `glgame` only; `gl_core` AppState stays
  boot/menu/level/exit — use a local sub-state or extend the enum with `save_slots` and
  document).
- Screenshot: `glgame --menu --screenshot` with Continue active; optionally add
  `--save-slots-screenshot` if a direct state flag is cheap.

Tests (`tests/app`): `glgame --game-dir ... --list-saves` → exit 0, contains `slot0` and 28
lines; menu screenshot still 1280×720.

- [ ] Implement, tests, commit.

---

### Task 4: Acceptance + docs

- Full `ctest` with `GL_GAME_DIR`.
- `glgame --game-dir ... --list-saves` output evidence.
- Manual: menu `Continue` → slot list, Esc back.
- Docs: STATUS/PROJECT (P2c accepted; next P3 or P1b), research note (save header facts),
  plan checkboxes.
- Commit.

## Self-Review

- Covers header parsing, slot scan, UI/CLI, tests, docs.
- Deferred: writing saves, world deserialization, previews.
