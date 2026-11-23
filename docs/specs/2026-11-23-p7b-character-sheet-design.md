# P7b — Character sheet (design)

Status: approved (2026-11-23)
Scope: read-only character sheet opened from the inventory "skills"
tab and by `C`; original `inventory_skills.bmp` frame, five sub-tabs,
left attribute panel, 27 skills, dynamic equipped hero doll, tests.
One subproject.

## Source of truth

- `Client.dll` v1.3 SHA-256
  `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`,
  image base `0x12000000` (RVA = VA - 0x12000000).
- `sdb/user_interface.sdb` (`glpak decode-sdb`): 310 localized UI
  strings. IDs used here: 0–4 tabs, 5 name, 6–12 attributes, 13–17
  characteristics, 18–26 combat stats, 27–28 life/energy, 29 weight,
  43–48 magic immunities, 49–75 skills, 76 skill points.
- `scripts/ui/inv_gui.scr`: object rects for the left panel
  (`90:97`), 27 skill rows (`100:126`), secondary rows (`160:170`),
  inventory tabs (`5:6`), close (`7`).
- Client arrays read from `Client.dll`:
  - skill row x `0x120f8520`, y `0x120f85a0`, UI text ids
    `0x120f8620`;
  - characteristics row y `0x120f8738`, UI ids `0x120f8774`;
  - skill row -> skill id permutation `0x1212e0a0`:
    `12,13,7,8,9,10,11,14,24,4,5,0,1,2,3,17,25,23,26,15,20,21,16,18,19,22,6`.
- Background `engineres/newinv/inventory_skills.bmp` (1024x768, 8bpp).
- Real save `Saves/slot0.sav` (`BT_HERO`): STR 30, CON 25, DEX 18,
  PER 12, INT 5, WIS 7, LUK 9, REP -40, EXP 81099; 27 skills
  (ids 0..26, §7.2 of `p3b1-person-parameters-server.md`).

## Data model (`gl::game`, SDL-free)

```cpp
enum class CharacterTab { personality, characteristics, skills, state, resistance };

struct CharacterRow {
    std::string label;   // UTF-8 from user_interface.sdb
    std::string value;   // decimal / signed / "-"
    bool modeled = true; // false -> "-"
};

struct CharacterSheet {
    std::string name;                                  // id 5 + Hero
    std::vector<CharacterRow> primary;                 // ids 6..12, 16
    std::vector<CharacterRow> personality;             // id 5, 14, 15, 16
    std::vector<CharacterRow> characteristics;         // ids 14,15,16,27,28,18..26,17
    std::vector<CharacterRow> skills;                  // ids 49..75
    std::int32_t skill_points = 0;
};

CharacterSheet build_character_sheet(const PersonRuntime &persons,
                                     const gl::assets::Sdb &ui_texts,
                                     std::string_view person = "Hero");
```

- `primary` order: id 6 Сила (strength), 7 Телосложение
  (constitution), 8 Ловкость (dexterity), 9 Внимание (perception),
  10 Мудрость (wisdom), 11 Интеллект (intelligence), 12 Удача (luck),
  16 Слава (reputation). Values are the effective `PersonRuntime`
  parameters (already clamped by the runtime).
- `personality`: name (id 5, `Hero`), level (id 14,
  `hero_level_from_experience(EXPERIENCE)`), experience (id 15),
  reputation (id 16).
- `characteristics` order and ids: 14 Уровень, 15 Опыт, 16 Слава,
  27 Жизнь, 28 Энергия, 18 Очки действия, 19 Дробящее, 20 Колющее,
  21 Рубящее, 22 Огонь, 23 Холод, 24 Яд, 25 Базовая точность,
  26 Шанс крит. удара, 17 Класс брони.
  Modeled: 14 (computed level), 15 (EXP), 16 (reputation), 27
  (`LIFE`), 28 (`ENERGY`). The rest are `modeled = false` -> "-".
- `skills`: 27 rows in UI order; label = user_interface.sdb ids 49..75,
  value = `PersonRuntime::get_skill` for the mapped skill id
  (`0x1212e0a0` permutation):
  `WPN_SWORD, WPN_AXE, WPN_CRUSH, WPN_STAFF, WPN_DIST, WPN_SPEAR,
  WPN_THROW, WPN_HAND, CRITICAL_HIT, SHADMAG, NATRMAG, GODSMAG,
  ELEMMAG, LGHTMAG, DARKMAG, MAGICUSE, ALCHEMY, IDENTIFY, TACTIC,
  SCOUT, HEALING, TALKING, TRADE, STEAL, HACK, SMITH, ATHLETIC`.
- `skill_points` = 0 (read-only; allocation out of scope).
- Missing UI string -> label `#<id>`; missing person -> all values 0/`-`
  and name = the requested person string (`Hero` in normal use; the test
  passes `missing`).

## UI (`gl::app::CharacterOverlay`, SDL only here)

- 1024x768 canvas, letterboxed like inventory/trade/diary.
- Background `engineres/newinv/inventory_skills.bmp`.
- Font: same as inventory (`inventory_font()`), text color from the
  original (dark on parchment).
- Five sub-tabs I..V at `(3,4)`, `(62,4)`, `(121,4)`, `(179,4)`,
  `(239,4)`, each `56x25`; the baked Roman numerals stay visible.
  Active tab is highlighted by the original tab art (no new art).
- Left panel:
  - 7 attribute rows `(20,60/88/117/144/172/199/226, 130x23)`;
  - reputation row `(20,262,181x23)`;
  - doll area `(8,296,270x320)` (dynamic hero);
  - skill points row background `(515,624,210x27)`, text at
    `(526,632)`, value `0`.
- Content panel by tab:
  - personality: four rows at `(296,102,272x24)`,
    `(296,134,155x23)`, `(296,163,272x24)`, `(296,214,164x20)`
    (name, level, experience, reputation), plus the doll;
  - characteristics: 15 rows at x `296`, y `109,140,168,218,244,271,
    354,381,408,435,462,489,516,543,570`, `155x20`;
  - skills: 27 rows at x `295/544/790` and y
    `130/162/194,315/348/381,500/532/564`, `140x20`;
  - state/resistance: centered `Нет данных` on the content area.
- Text: near-black `gl::render::Color{20,18,16,255}` on the paper
  strips; primary/personality 18 px, characteristics/skills 16 px;
  labels are clipped to the row width.
- Read-only: no `-/+`, no drag, no allocation, no item interaction.
- Close button `(920,694,75x68)` with `engineres/newinv/exit`/
  `exitl` on hover; `Esc`/`Alt+F4` close; `C` toggles.
- Mouse: click a sub-tab switches; click close closes; motion updates
  hover only.
- Keys: `1..5` switch tabs; `C`/Esc close.

## Dynamic hero doll

`gl::app::HeroPortrait` (new helper, SDL in `app`):

- Loads `HeroVisual` via `gl::game::load_hero_visual(vfs, outfit)`.
- Composes one idle frame with `compose_wear_body` /
  `compose_wear_shadow` for the `tb_stay` animation, uploads it as a
  texture.
- Draw at the doll area anchored bottom-center (`position - anchor`
  semantics) at the largest integer scale `1..3` that fits inside
  `(8,296,270x320)` (nearest-neighbour, no fractional scaling).
- If the visual/outfit is unavailable, draw nothing (the baked
  silhouette remains visible).

## Integration

- `InventoryOverlay` hit region `skills_tab` `(280,4,368x40)` opens
  `CharacterOverlay` instead of being ignored.
- `glgame`: `C` toggles the character sheet; while open,
  `level_input_enabled` gains `character_active` (camera/person input
  off). `--character` opens it after `--load-save` for screenshots.
- `CharacterOverlay` gets `GameVfs`, `Sdb`, `PersonRuntime`,
  `HeroOutfit`/`HeroVisual` from `glgame`; the inventory overlay keeps
  its current behavior when the character sheet is closed.
- Switching from the skills tab back to the inventory tab (6,
  `(654,4,360x40)`) closes the character sheet.

## Tests

- `tests/game/test_character_sheet.cpp`: 8 primary rows order/values;
  27 skill rows order/values from the `0x1212e0a0` mapping; level from
  EXP; `-` for unmodeled characteristics; missing UI text fallback;
  empty runtime.
- `tests/app/test_character_overlay.cpp`: open/close; five tabs;
  click tabs; close hit; key `C`/`1..5`/Esc; render changes pixels in
  the left panel, skill rows, characteristics rows and doll region
  with headless SDL and real assets/font.
- Real slot0: primary values `30/25/18/12/7/5/9/-40`, EXP `81099`,
  level from `hero_level_from_experience`, 27 skills match the save.
- `glgame_screenshot_tests`: `--character --load-save` produces a
  non-empty frame.
- Full `ctest` with `GL_GAME_DIR`.

## Non-goals / residual

- Skill-point spending, `-/+`, allocation, save write.
- State/resistance/combat statistics data (unmodeled; tabs show
  `Нет данных`), remaining characteristics columns 2–3
  (damage/resistances/magic immunities) and weight/armor/accuracy/crit
  values.
- Crafting (`scripts/ui/skills_gui.scr`), exact original personality
  detail rows, tooltips/help (`question.bmp`), idle animation,
  drag-and-drop, touch layout (P9).
- Exact original per-tab content grouping is verified against
  `Client.dll` during implementation; deviations are recorded in
  `DECISIONS.md`.

## Acceptance

- Character sheet opens from a real save via the inventory skills tab
  and `C`, shows localized labels and the real hero values/doll.
- Five tabs switch; three functional, two `Нет данных`; close/keys
  work; level input is gated.
- Unit/overlay/real tests pass; full `ctest` green with
  `GL_GAME_DIR`; `STATUS`/`PROJECT`/`DECISIONS`/research updated.
