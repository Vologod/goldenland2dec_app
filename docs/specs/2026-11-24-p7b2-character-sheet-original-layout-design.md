# P7b2 — Character sheet original layout (design)

Status: approved (2026-11-24, manual-gate follow-up to P7b)
Scope: replace the five invented sub-tabs with the original two-tab
character sheet, correct page content, name box, bottom inventory and
doll. Data parity (effective stats, skill-point pools, exact original
rows) stays residual.

## Evidence

- Manual gate screenshots (2026-11-24) of the original:
  - `ХАРАКТЕРИСТИКИ` active: left `Вертас` + 7 attributes + `Очки опыта`
    33, doll/inventory; center `СОСТОЯНИЕ` (Уровень/Опыт/Слава,
    Жизнь/Энергия/Очки действия), `БОЕВЫЕ ХАРАКТЕРИСТИКИ`; right
    `СОПРОТИВЛЕНИЕ`, `ИММУНИТЕТ К МАГИИ`.
  - `НАВЫКИ` active: `Навыки Воина` / `Навыки Мага` / `Навыки
    Следопыта` (9 each, `-/+` buttons), bottom `Очки распределения`.
- Backgrounds:
  - `engineres/newinv/inventory_secondary.bmp` = ХАРАКТЕРИСТИКИ page
    (state/combat/resistance/immunity strips; left panel, doll, bottom
    inventory).
  - `engineres/newinv/inventory_skills.bmp` = НАВЫКИ page (three skill
    groups; same left panel, doll, bottom inventory).
- Both backgrounds already contain the top name box `(0,0,265x35)` and
  two tab boxes `(265,0,350x35)` / `(615,0,409x35)`.
- Localized labels: `sdb/user_interface.sdb` ids 1 `ХАРАКТЕРИСТИКИ`,
  2 `НАВЫКИ`, 3 `СОСТОЯНИЕ`, 4 `СОПРОТИВЛЕНИЕ`, 86 `БОЕВЫЕ
  ХАРАКТЕРИСТИКИ`, 42 `ИММУНИТЕТ К МАГИИ`, 83/84/85 warrior/mage/
  ranger group headers, 76 `Очки распределения`; attributes 6–12,
  experience points 13, state 14/15/16, life/energy/AP 27/28/18,
  combat 19–26/17/29, resistances 30–41, immunities 43–48, skills
  49–75.
- Hero display name in the original is `Вертас`; scripts still address
  the person as `"Hero"`.

## Tabs and top strip

- Two tabs only: `CharacterTab::characteristics` (default) and
  `CharacterTab::skills`.
- Top boxes: name `(0,0,265x35)` (`Вертас`), characteristics
  `(265,0,350x35)`, skills `(615,0,409x35)`.
- Active tab = bright text (e.g. `{235,230,215}`), inactive = dark
  `{70,65,55}`. Labels centered in the boxes.
- Click a tab; keys `1`/`2`; `C`/Esc close. No five I–V sub-tabs.
- Background switches: characteristics -> `inventory_secondary.bmp`,
  skills -> `inventory_skills.bmp`.

## Always-visible left/bottom

Measured on `inventory_secondary.bmp` (light box rectangles; the same
layout on `inventory_skills.bmp`):

- 7 attributes: label box `x=22..151`, value box `x=196..225`
  (right-aligned), baked `-`/`+` at `172..189`/`235..255`; rows
  `y = 62/89/117/144/172/199/227`, heights
  `21/21/20/21/20/21/20`.
- `Очки опыта` (id 13): label box `x=22..202`, value box `x=214..255`,
  `y=263`, height 21, value 0 for now.
- Doll: baked into the background BMP inside `(8,296,270x320)`; both
  pages already contain the figure, no separate sprite or animation.
  Equipment icons (`55x55`) are drawn from `HeroInventory` at the
  original mask positions; `puppet_name` wear overlays on the figure
  remain residual (P5b2).
- Fonts (`scripts/fonts.scr`, `Client.dll` `FUN_1202ffd0`):
  rows = `main_interface` (`pala.ttf`, 12 pt, weight 500),
  headers/tabs/name = `heads_interface` (`pala.ttf`, 14 pt,
  weight 600); size converted via `MulDiv(size, LOGPIXELSY, 72)`.
  Port pixel heights: rows 16, headers 17 (must fit the 245 px header
  strips; 19 px overflows).
- Vertical text placement = GDI cell centering:
  `y = box_top + (box_h - text_height) / 2`, where `text_height`
  (`Font::text_height()`) is ascent+descent at the pixel height;
  not `line_height` (that includes the 5 px Palatino line gap and
  clips glyphs).
- Equipment icons around the doll and the hotbar/bottom grid come from
  the loaded `HeroInventory` + `ItemCatalog`: bottom strip 12 slots at
  `x = 140,201,261,321,382,443,504,565,627,688,749,811`, `y=703`,
  `55x55`; hotbar slots `1..9` at `x=14,43,72,101,130,159,188,217,246`,
  `y=624`, `27x27`; puppet icons at the existing `PuppetSpec` rects.
  Read-only: no drag/select.
- Filters/scroll/close are baked in the backgrounds; close remains
  `(920,694,75x68)` with `exit`/`exitl` hover.

## ХАРАКТЕРИСТИКИ page

- Left: attributes + `Очки опыта` as above.
- Center:
  - header `СОСТОЯНИЕ` (id 3) text centered at `x=466` in box
    `x=344..588, y=55..78`;
  - rows `Уровень` (14), `Опыт` (15, value `cur/max`), `Слава` (16):
    label box `x=299..567`, value box `x=585..631` (right `x=626`),
    y `103/134/163`, heights `23/22/22`;
  - `Жизнь` (27, `cur/max`), `Энергия` (28, `cur/max`), `Очки
    действия` (18) at y `214/240/267`, heights `19/19/19`, same boxes;
  - header `БОЕВЫЕ ХАРАКТЕРИСТИКИ` (86) centered at `x=466` in box
    `y=305..328`;
  - 10 rows at the same label/value boxes, y
    `349,376,403,431,458,485,512,539,566,598`, heights
    `19×9,21`: `Дробящее` (19),
    `Колющее` (20), `Рубящее` (21), `Огонь` (22), `Холод` (23),
    `Яд` (24), `Базовая точность` (25), `Шанс критического удара`
    (26), `Класс брони` (17), `Вес` (29).
- Right:
  - header `СОПРОТИВЛЕНИЕ` (4) centered at `x=822` in box
    `x=700..944, y=55..79`;
  - rows label box `x=665..919`, value box `x=948..999` (right `x=995`),
    y `105,137,169,222,253,283,314,344,375,431,457,484`, heights
    `20×4,19×8`: `Дробящее` (30),
    `Колющее` (31), `Рубящее` (32), six magic schools (33–38),
    `Огонь` (39), `Холод` (40), `Яд` (41);
  - header `ИММУНИТЕТ К МАГИИ` (42) centered at `x=828` in box
    `x=706..950, y=525..549`;
  - 2 columns at y `571,597,624` (heights `19/19/18`):
    col1 label `x=668..759`, value `x=773..817`; col2 label
    `x=844..936`, value `x=949..994`: `Теней` (43), `Природы` (44),
    `Богов` (45) | `Стихий` (46), `Света` (47), `Тьмы` (48).

## НАВЫКИ page

- Group header plaques `x=529..775` (text centered `x=652`), y
  `74..102` / `260..287` / `445..472`: ids 83/84/85.
- Rows 3 columns, label boxes `x=291..429 / 537..675 / 783..921`;
  baked `-` at `436..456 / 682..702 / 928..948`, value boxes
  `457..492 / 703..737 / 950..982` (right `488/733/978`), baked `+` at
  `493..513 / 738..758 / 984..1004`; y rows
  `126/159/191`, `312/345/377`, `497/529/561` (heights `19/18/19`):
  ids 49–57, 58–66, 67–75. Baked `-/+` stay disabled (read-only).
- Bottom `Очки распределения` (76): label box `x=517..711` (text
  centered `614`), value box `x=739..783` (right `779`),
  `y=628..649`, value 0.

## Data model

```cpp
enum class CharacterTab { characteristics, skills };

struct CharacterRow { std::string label; std::string value; bool modeled = true; };
struct CharacterGroup { std::string title; std::vector<CharacterRow> rows; };

struct CharacterSheet {
    std::string name;                    // "Вертас"
    std::vector<CharacterRow> primary;    // 7 attributes
    CharacterRow experience_points;       // id 13, 0
    std::vector<CharacterRow> state;      // 6 rows
    std::vector<CharacterRow> combat;     // 10 rows
    std::vector<CharacterRow> resistance; // 12 rows
    std::vector<CharacterRow> immunity;   // 6 rows
    std::vector<CharacterGroup> skill_groups; // 3 groups
    std::int32_t skill_points = 0;
    std::uint32_t experience_max = 0;
};

CharacterSheet build_character_sheet(
    const PersonRuntime &persons,
    const std::function<std::string(std::uint32_t)> &ui_text,
    std::string_view display_name,
    std::string_view person = "Hero");
CharacterSheet build_character_sheet(
    const PersonRuntime &persons, const gl::assets::Sdb &ui_texts,
    std::string_view display_name, std::string_view person = "Hero");
```

- Modeled: level (`hero_level_from_experience`), experience `cur/max`
  (max from the level formula), fame (reputation), life/energy
  `cur/max` from the runtime; skills from the `0x1212e0a0` order.
- Unmodeled (`modeled=false`, value `—`): experience points, action
  points, all combat rows, all resistances/immunities, skill points.
- `name` = `display_name` (`Вертас`), falling back to `person`.
- `experience_max`: next-level threshold for the current level (the
  same formula as `level_from_experience`).

## UI data

- `CharacterOverlay::open(SDL_Renderer*, const GameVfs&, CharacterSheet,
  const HeroOutfit*, const HeroInventory*, const ItemCatalog*)`.
- Bottom/hotbar/puppet item icons are drawn read-only from the
  inventory/catalog; missing icons use the inventory fallback.
- No item selection/drag; click only tabs/close.

## Tests

- Model: two tabs; primary/experience points; state/combat/resistance/
  immunity rows/order/ids; three skill groups 9/9/9 with the
  permutation; modeled vs `—`; level/EXP max; `Вертас` name; missing UI
  text.
- Overlay: background switches per tab; name/tab labels; active tab
  click/keys; section headers/rows; skill group headers; bottom item
  icons drawn from a synthetic inventory; the doll region equals the
  background BMP pixels (no world-hero sprite); row text is centered in
  the measured boxes and values right-align to the measured value-box
  edges (attributes, state, combat, resistance, immunity, skills);
  close hover.
- Real slot0: attribute/skill values; EXP `81099/82000`; life/energy
  from the save; name `Вертас`.
- Screenshot: `--character --load-save 0` characteristics and
  `--character --load-save 0 --character-tab skills` (or `2` key in a
  test) differ; non-empty 1024x768.
- Full `ctest` with `GL_GAME_DIR`.

## Non-goals / residual

- Effective stats with equipment/special bonuses (original shows CON 30
  / DEX 20 / PER 13 / WIS 10 for slot0 vs raw 25/18/12/7).
- Real `Очки опыта`/`Очки распределения` pools; hero-name source
  (`Вертас` is a port display constant until `.hero`/save mapping).
- Combat/resistance/immunity/AP/weight values, exact original row
  strips/positions beyond the measured layout, baked `-/+` interaction,
  full inventory page/filter state, item selection.
- Touch layout (P9).
- `puppet_name` wear overlays on the sheet doll (P5b2-1): the baked
  figure stays unclothed by equipment while the slot icons are shown.
