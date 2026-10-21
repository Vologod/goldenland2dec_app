# P3c — Dialogs + SDB + Cyrillic Font Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development or executing-plans. Steps: `- [ ]`.

**Goal:** Show real dialogs with Cyrillic text: TTF font, SDB phrase lookup, `DialogSession`, and a `glgame` dialog screen with answer selection.

**Architecture:** `gl_render` gains a `Font` (stb_truetype, UTF-8, atlas, wrap) with ASCII fallback. `gl_script` gains `DialogSession` (VM + SDB). `glgame` gains a dialog screen and `--dialog`.

**Tech Stack:** C++20, SDL2, stb_truetype (pinned), existing `gl_assets`/`gl_script`.

## Global Constraints

- C++20; `-Wall -Wextra -Werror`; `gl_assets`/`gl_script` SDL-free.
- Text API is UTF-8; SDB already returns UTF-8.
- Missing SDB id → `<phrase N>`, never a crash.
- No game assets committed; system font paths are runtime-only.
- Every task ends with a commit.

## File Structure

```
third_party/stb/stb_truetype.h
src/render/include/gl/render/font.hpp
src/render/src/font.cpp
src/render/include/gl/render/utf8.hpp
src/render/src/utf8.cpp
src/script/include/gl/script/dialog_session.hpp
src/script/src/dialog_session.cpp
tests/render/{test_utf8.cpp,test_font.cpp}
tests/script/{test_dialog_session.cpp,test_real_dialog_session.cpp}
tools/glgame/main.cpp
tools/glscript/main.cpp
tests/tools/test_cs_tools.cpp (glscript --sdb)
tests/app/test_glgame_screenshot.cpp (dialog screenshot)
```

---

### Task 1: UTF-8 + TTF font (`gl_render`)

- Add `stb_truetype.h` from stb commit `013ac3beddff3dbffafd5177e7972067cd2b5083`.
- `utf8.hpp`: `std::vector<char32_t> utf8_decode(std::string_view)` (invalid → U+FFFD).
- `Font`:
```cpp
static Result<Font> load_from_file(SDL_Renderer*, const std::filesystem::path&, float pixel_height);
void draw_text(SDL_Renderer*, std::string_view utf8, float x, float y, Color);
Vec2 measure(std::string_view utf8) const;
float line_height() const;
std::vector<std::string> wrap(std::string_view utf8, float max_width) const;
```
Glyph atlas: ASCII + U+0400–U+04FF + U+2000–U+206F; RGBA texture; `stbtt_GetCodepointBitmap`.
- Fallback: if font load fails, use the existing ASCII `draw_text`.
- Font discovery helper: `find_system_font()` checks `segoeui.ttf`/`arial.ttf`/`consola.ttf` in
  `C:\Windows\Fonts`; `--font <path>` is owned by `glgame` (Task 3).
- `pixel_height` is limited to `[8, 256]`; out-of-range values return `ErrorCode::argument`
  (prevents atlas overflow). Atlas rows widen to the widest glyph.
- Control characters: `\r` and other C0/C1 controls are ignored, `\t` expands to four spaces;
  `measure`, `wrap` and `draw_text` share the same normalization.
- Tests: UTF-8 decode always runs (`utf8_tests`); font-dependent cases are `font_tests`
  (skip 77 if no system font) and cover measure/wrap, control characters, render pixels,
  the missing-font error, and rejected pixel heights.

- [x] TDD, implement, commit.

---

### Task 2: `DialogSession` (`gl_script`)

```cpp
struct DialogLine { std::uint32_t phrase_id; std::string text; };
class DialogSession {
    DialogSession(ScriptVm&, const gl::assets::Sdb&);
    gl::Result<void> start();
    bool waiting() const;
    std::string_view npc_text() const;
    std::span<const DialogLine> answers() const;
    gl::Result<void> choose(std::size_t index);
    VmStatus status() const;
};
```
- `start`/`choose` run VM to wait/end, map ids via `Sdb::find`, fallback `<phrase N>`.
- Tests: synthetic VM dialog; choose/end; out-of-range; missing SDB id.
- Real: `demon.d1.age.cs` + globals + `sdb/dialogs/dialogsphrases.sdb` → non-empty phrase,
  ≥1 answer; `choose(0)` → next non-empty/end. Gated by `GL_GAME_DIR`, skip 77.

- [x] TDD, implement, commit.

---

### Task 3: `glgame` dialog screen + tools

- `--dialog <virtual/path.cs>`: load globals/SDB/CS, open the dialog screen directly.
- `--font <path>`: explicit TTF override; otherwise `find_system_font()`, then ASCII fallback
  if no font loads. No font file is bundled or committed. `glscript` stays SDL-free and only
  prints UTF-8 text, so it does not take `--font`.
- Android/P9 must bundle a free TTF and pass it via `--font` (no `C:\Windows\Fonts`).
- Menu: `Test Dialog` (if `scripts/dialogs/demon.d1.age.cs` exists).
- Screen: dark panel, NPC text (TTF/wrap), numbered answers, Up/Down/mouse, Enter/click,
  `Esc`; `waiting()` false → message + Esc.
- `--screenshot --dialog ...` renders the dialog frame.
- `glscript --sdb <virtual/path>` prints dialog phrase texts (id + text) for the run.
- Tests: `glscript --sdb` on `demon...` contains a Russian phrase; glgame dialog screenshot
  1280×720 (skip if no font); existing tests pass.

- [x] Implement, tests, commit.

---

### Task 4: Acceptance + docs

- Full `ctest` with `GL_GAME_DIR`; dialog screenshot; glscript `--sdb` evidence.
- Manual: run `glgame --dialog ...`, read Cyrillic, choose an answer, Esc.
- Docs: STATUS/PROJECT/plan/research; P3c accepted; close the save-title `?` limitation;
  next P4 (NPC) or P1b.
- Commit.

## Self-Review

- Covers font, SDB, session, UI, tools, tests, docs.
- Deferred: NPC wiring, portraits/voice, AgeScript compiler, P7 UI.
