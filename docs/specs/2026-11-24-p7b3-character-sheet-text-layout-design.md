# P7b3 — Character sheet text layout parity (design)

Status: implemented (automated; 2026-11-24), manual gate open. Follow-up to P7b2 `cb6bee2`; the P7b2
manual gate reported "надписи поехали" (text positioning/appearance of
the sheet rows). Scope: reproduce the **exact** text rectangles and
centering of the original character-sheet renderer. Data parity
(effective stats, real point pools) stays residual.

## Evidence

- `Client.dll` SHA-256
  `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`,
  ImageBase `0x12000000`.
- Renderer: `FUN_120661cc` (RVA `0x661cc`, size 8668) — both pages;
  `*(this+0x1d0)`: `0` = ХАРАКТЕРИСТИКИ, `1` = НАВЫКИ.
- Text primitives (disassembly-confirmed, `llvm-objdump`):
  - `FUN_12030980` (RVA `0x30980`) — GDI `TextOutA(hdc, x, y, text)`;
    `y` is the **top of the character cell**.
  - `FUN_12030a88(this, hdc, x, y, textId, widthBox, heightBox)` (RVA
    `0x30a88`, `ret 0x18`). Draws localized text id. If `widthBox != -1`
    and `textWidth < widthBox`, `x += (widthBox - textWidth) >> 1`
    (horizontal center). If `heightBox != -1` and `cellHeight <
    heightBox`, `y += (heightBox - cellHeight) >> 1` (vertical center).
    `-1` = no centering (left/top). `cellHeight = abs(*(*font+8))`.
  - `FUN_12030b4c(this, hdc, x, y, str, widthBox, heightBox)` (RVA
    `0x30b4c`) — same centering for a raw string.
  - `FUN_12030a04(this, hdc, x, y, number, widthBox, heightBox)` (RVA
    `0x30a04`) — same centering for a numeric value.
  - Therefore **every value is horizontally centered in its box**, not
    right-aligned; the port's current right-alignment is wrong.
- Left panel, tabs, headers and row **labels** were already moved to the
  original coordinates in the in-progress tree; this spec keeps them and
  adds the **value** rects (the remaining deviation).
- Row/label arrays read from `.data` (dwords):
  - `DAT_120f8520` skills.x (28): `295×3,544×3,790×3, 295×3,544×3,790×3,
    295×3,544×3,790×3, 526`.
  - `DAT_120f85a0` skills.y (28): `130,162,194, 130,162,194, 130,162,194,
    315,348,381, 315,348,381, 315,348,381, 500,532,564, 500,532,564,
    500,532,564, 632`.
  - `DAT_120f8620` skills.id (28): `49..76`.
  - `DAT_120f8690` immunity.x (6): `673×3, 850×3`.
  - `DAT_120f86a8` immunity.y (6): `576,602,628, 576,602,628`.
  - `DAT_120f86c0` immunity.id (6): `43,44,45, 46,47,48`.
  - `DAT_120f86d8` resistance.y (12): `110,142,174,227,257,288,318,349,
    379,436,461,488`.
  - `DAT_120f8708` resistance.id (12): `30..41`.
  - `DAT_120f8738` state+combat.y (15): `109,140,168,218,244,271,354,381,
    408,435,462,489,516,543,570`.
  - `DAT_120f8774` state+combat.id (15): `14,15,16,27,28,18,19,20,21,22,
    23,24,25,26,17` (+ weight `29` drawn separately).
- Label rows pass `widthBox=-1, heightBox=-1` and `y = arrayY - 6`
  (left/top aligned). Skills label 28 (`id 76`, «Очки распределения») is
  drawn by the same loop at `x=526, y=632-6=626`.

## Primitive to add

One helper mirrors the original:

```cpp
// w/h < 0 => no centering on that axis (left/top at x/y); else center.
void draw_text_rect(SDL_Renderer*, const Font*, std::string_view,
                    float x, float y, float w, float h, Color);
```

- `textW = font->measure(text).x`; if `w >= 0 && textW < w`:
  `x += (w - textW) / 2`.
- `cellH = font->text_height()`; if `h >= 0 && cellH < h`:
  `y += (h - cellH) / 2`.
- **GDI baseline correction:** `y += font->line_height() -
  font->text_height()` (the **full** internal leading, ~5.2 px at
  16 px). GDI `TextOutA` puts the baseline at `cell_top + tmAscent`,
  which is lower than stb_truetype's `hhea` ascent by the internal
  leading; without this the sheet text sits ~2.5–3.5 px high ("прижат к
  верху"). Measured against the true original capture this brings
  labels/values/headers/skills to ~±1 px. (Half-leading was tried first
  against a port render mistaken for the original — too small.)
- Draw at `(x, y)`, matching `TextOutA`.
- Replaces `draw_row`/`draw_rows`/`draw_centered`. Row **labels** are
  called with `w=-1,h=-1`; **values** with their measured box; tabs and
  section headers with the header `widthBox` and `h=-1`.

## Common left panel (both pages)

Labels (`FUN_12030a88`, x, w=-1, h=-1):
- 7 attributes + experience: `x=30`, `y = {67,93,120,148,175,203,230,
  267} - 6 = {61,87,114,142,169,197,224,261}`, ids `6..13`.

Values (`FUN_12030a04`, centered):
- 7 attributes: `x=197, w=30, h=18`, `y = {60,87,115,143,171,198,225}`.
- «Очки опыта»: `x=216, w=38, h=18`, `y=261`.

## ХАРАКТЕРИСТИКИ page

Tabs (always): `ХАРАКТЕРИСТИКИ` x=`292`, widthBox=`344` (center `464`),
y=`16`; `НАВЫКИ` x=`661`, widthBox=`344` (center `833`), y=`16`.
Name: `FUN_12030b4c` `(15,10,250x26)` -> center `140`.

Headers (`FUN_12030a88`, widthBox=`228`, h=-1):
- `СОСТОЯНИЕ` x=`352` (center `466`), y=`57`.
- `БОЕВЫЕ ХАРАКТЕРИСТИКИ` x=`352` (center `466`), y=`307`.
- `СОПРОТИВЛЕНИЕ` x=`709` (center `823`), y=`57`.
- `ИММУНИТЕТ К МАГИИ` x=`713` (center `827`), y=`527`.

State labels: `x=308`, `y = {109,140,168,218,244,271}-6 =
103,134,162,212,238,265`, ids `14,15,16,27,28,18`.
State values (centered):
- `Уровень` (14): `(588,103,41x18)` numeric.
- `Опыт` (15): `(471,133,159x18)` `%i/%i`.
- `Слава` (16): `(588,161,41x18)` numeric.
- `Жизнь` (27): `(480,212,150x16)` `%i/%i`.
- `Энергия` (28): `(480,238,150x16)` `%i/%i`.
- `Очки действия` (18): `(572,265,56x16)` `%i/%i`.

Combat labels: `x=308`, `y = {354,381,408,435,462,489,516,543,570}-6 =
348,375,402,429,456,483,510,537,564`, ids `19..26,17`; `Вес` (`29`)
`x=300`, `y=598`.
Combat values (centered):
- ids `19..26`: `x=582, w=46, h=15`, `y = {348,375,402,430,456,483,
  510,537}`.
- `Класс брони` (17): `(582,565,46x15)` numeric.
- `Вес` (29): `(497,597,130x18)` `%g/%g`.

Resistance labels: `x=673`, `y = {110,142,174,227,257,288,318,349,379,
436,461,488}-6 = 104,136,168,221,251,282,312,343,373,430,455,482`,
ids `30..41`.
Resistance values (centered): `x=950, w=47, h=17`, `y = {103,136,168,
220,251,282,312,343,373,430,456,482}`.

Immunity labels: column x `673`/`850`, `y = {576,602,628}-6 =
570,596,622`; ids `43..45` (col 1), `46..48` (col 2).
Immunity values (centered), `y = {570,596,622}`, `w=43, h=16`:
- col 1: `x=774`; col 2: `x=950`.

## НАВЫКИ page

Group headers (`FUN_12030a88`): `x=535`, widthBox=`230` (center `650`),
`y = {78,264,449}`, ids `83/84/85`.

Skill labels (28 rows from arrays): `x=skills.x[i]`, `y=skills.y[i]-6`,
id=`skills.id[i]`. Grid is column-major: per group, column 0 rows
`y=124/156/188`, column 1 `...`, column 2 `...`; group 2 rows
`309/342/375`; group 3 `494/526/558`; last row id 76 at `x=526, y=626`.

Skill values (`FUN_12030a04`, centered): `w=31, h=18`, `x =
460/706/952` per column, `y = 123/156/188` (group 1), `310/342/374`
(group 2), `494/527/559` (group 3). Value row `i` uses the same
column-major cell as label row `i`.

«Очки распределения» value (`FUN_12030a04`, centered):
`(739,623,45x23)`.

## Non-text parity notes

- The active/inactive tab colour observed on the manual screenshot
  (bright active / dark inactive) is kept from P7b2 (D-112); the
  decomp `0x14=0` vs `0x464646` is not treated as a black/white claim.
- Remove the port-only tab **underline** rect (the original only changes
  text colour).
- Row label clipping: **dropped**. The original `TextOutA` path does not
  clip, and the longest shipped labels fit their boxes with `pala.ttf`
  at 16 px (measured: `Шанс критического удара` ~191 px vs ~283 px gap;
  `Дробящее повреждение` ~180 px vs ~277 px; `Рукопашный бой` ~129 px
  vs ~162 px). No overflow guard is applied (matches the original).

## Tests

- Unit/screenshot with `GL_GAME_DIR`, `--character --load-save 0`:
  - each value glyph run's horizontal centre within +-2 px of the box
    centre (attributes, experience, state, combat, weight, resistance,
    immunity two columns, skills three columns, skill points);
  - each label's top within +-1 px of `arrayY-6`; value vertical centre
    within +-1 px of `y + (h - text_height)/2`;
  - tabs centred at 464/833, y=16; headers centred at 466/823/466/827;
    group headers centred at 650;
  - no port underline (tab strip pixels unchanged vs background except
    text);
  - name centred at x=140.
- Model/other `character_overlay_tests` cases stay green; `gl_game_tests
  --test-case="*character sheet*"` and `glgame_screenshot_tests
  --test-case="*character*"` stay green; full `ctest` with
  `GL_GAME_DIR`.

## Non-goals / residual

- Effective stats with equipment/special bonuses; real point pools.
- Exact Palatino weight 500/600 (stb_truetype no synthetic weight);
  `pala.ttf` vs system Palatino Linotype. Consequence:
  `БОЕВЫЕ ХАРАКТЕРИСТИКИ` at 17 px measures ~239 px > `widthBox` 228, so
  it is left-aligned at `x=352` (~+5 px vs centre 466) instead of
  centred — a font-width residual, not a coordinate error.
- Vertical anchor: resolved by the full-leding GDI correction above;
  remaining bias ~±1 px — within anti-aliasing.
- Font/size (separate task): `pala.ttf` at 17 px is ~6% wider than the
  original for rows/headers, while the original tabs/name are ~19%
  wider (heads_interface weight 600, not synthesised by stb_truetype).
  This is typeface/size, not coordinates.
- `puppet_name` wear overlays (P5b2-1); touch layout (P9).
