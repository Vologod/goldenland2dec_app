# P7a — Quest journal overlay (design)

Status: approved (2026-11-22)
Scope: read-only quest journal from the loaded save DIARY; original
diary frame; app overlay + glgame CLI/key; tests. One subproject.

## Data

- Source: `loaded_payload_` from the current save, decoded with
  `decode_diary_entries`.
- Text: `sdb/diary/single_quest_lit.sdb` by `DiaryEntry.text_id`; a
  missing/empty string becomes `#<id>`.
- New SDL-free model in `gl/game/diary_view.hpp`:

```cpp
struct DiaryRow {
    std::uint32_t key = 0;
    std::uint32_t text_id = 0;
    std::uint32_t status = 0;
    GameDatetime start;
    std::optional<GameDatetime> end;
    std::string title; // SDB[key]
    std::string text;  // SDB[text_id]
};
std::vector<DiaryRow> build_diary_rows(
    std::span<const DiaryEntry> entries, const gl::assets::Sdb &texts);
std::string format_diary_date(const GameDatetime &value);
std::string format_diary_day(const GameDatetime &value);
```

- `single_quest_lit.sdb` supplies the title by `key`; the description is
  all stage paragraphs from record-block word `+0x04` (blocks 1..n-1)
  with completion from word `+0x08`, skipping `0`/`-1` and missing ids.
  Title/description fall back to `#id`; the header `text_id` is not a
  display text id.
- Only `status == 1` entries are the "Сюжетные" storyline list; side
  quests are out of scope for this slice.
- Order: active (`end` absent) first, completed last; newest-first by
  `start` inside each group.
- Date format: list `dd.mm.yyyy`; completed rows/paragraphs are struck
  through.

## UI

`gl::app::DiaryOverlay` in `src/app`, SDL only there:

- 1024x768 canvas letterboxed like the inventory/trade overlays.
- `engineres/diary/main.bmp` (open book), `page1.bmp` strip at the top,
  close button at `(909,687,71,65)` using `close_l.bmp`/`close_p.bmp`
  on hover.
- Palatino font `engineres/fonts/ttf/pala.ttf` at 18 px.
- Five original tab labels are drawn (`Сюжетные`, `Несюжетные`,
  `Биография`, `Создания`, `История`); only the quest tab is functional.
- Left page `(70,110,340,540)`: scrollable one-line rows = `dd.mm.yyyy`
  + title; completed rows (`end` present) are struck through; selected
  row uses darker text and a thin underline.
- Right page `(455,110,490,540)`: selected row's full multi-stage
  description, clipped and vertically scrolled (no date header, matching
  the original).
- Empty state: `Нет записей` centered on the left page.
- Keys: `J`/Esc/Alt+F4 close; Up/Down select; PageUp/PageDown scroll
  detail; Home/End first/last.
- Wheel scrolls the detail by 3 lines (natural direction); click selects
  a row; click close closes; mouse motion updates hover only.
- Original bottom arrows from `diary.scr` are clickable: left pair pages
  the quest list, right pair pages the description (PageUp/PageDown
  step); their `IMAGE_PRESSED` (`but/1..4`) is drawn while the mouse is
  held. Close uses `close_l`/`close_p` on hover/press.

## Integration

- `--diary` after `--load-save` opens the journal for screenshots.
- `J` toggles the journal in the level state.
- `level_input_enabled` gains `diary_active` (default false) so the
  level does not pan while the journal is open.
- `DiaryOverlay` gets the same font as inventory (`inventory_font()`).
- Missing save/DIARY or SDB logs a warning and does not open.

## Non-goals

- Live journal entries during the session, DIARY write.
- Statistics/monsters/countries/perks tabs and their SDB data.
- Exact original row sort, tab labels and page-turn animation.
- Record payload interpretation.

## Tests

- `tests/game/test_diary_view.cpp`: title/description mapping/fallback,
  newest-first order, dates, empty entries.
- `tests/app/test_diary_overlay.cpp`: open/close, selection, wheel
  scroll, key handling, render changes pixels in list/detail/close
  regions using a headless SDL renderer and the real diary assets/font.
- `tests/app/test_level_click.cpp`: `level_input_enabled` diary gate.
- `glgame_screenshot_tests`: `--diary --load-save` produces a non-empty
  journal frame.
- Full `ctest` with `GL_GAME_DIR`.

## Acceptance

- Journal opens from a real save, shows localized quest text and dates.
- Overlay controls and gating tested.
- Full `ctest` green; STATUS/PROJECT/DECISIONS updated.

## Manual-gate follow-up (2026-11-22, implemented)

1. List order: active quests (`end` absent) first, completed (`end`
   present) last; newest-first inside each group.
2. Description stages: stage paragraphs use `record+0x04` as the text id
   and `record+0x08` as the completed flag; completed paragraphs are
   struck through; all paragraphs are reachable from the first to the
   last line through the detail scroll (natural wheel direction, clamped
   max). The selected row has no underline.
3. Tab labels are vertically centered in the `page1.bmp` tab strip.

## Residual

Live journal, other tabs, exact original order/labels/animations, and
DIARY write remain out of scope.
