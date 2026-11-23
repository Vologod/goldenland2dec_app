# P7b — Character sheet research

Source of truth: `Client.dll` v1.3, SHA-256
`FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`,
image base `0x12000000` (RVA = VA - `0x12000000`). UI strings come from
`sdb/user_interface.sdb` (310 localized strings, decoded with
`glpak decode-sdb`). Object rects come from `scripts/ui/inv_gui.scr`.

## `user_interface.sdb` id map

| ids | meaning |
| --- | --- |
| 0–4 | sub-tab labels (personality / characteristics / skills / state / resistance) |
| 5 | hero/person name |
| 6–12 | primary attributes (strength, constitution, dexterity, perception, wisdom, intelligence, luck) |
| 13–17 | characteristics (level, experience, reputation, armor class) |
| 18–28 | combat / life / energy (action points, damage types, accuracy, crit, life, energy) |
| 43–48 | magic immunities |
| 49–75 | skills (27 rows, UI order) |
| 76 | skill points |

Missing id -> label `#<id>` (port fallback).

## Skill order and screen arrays

- Skill-row -> skill-id permutation at `0x1212e0a0`:
  `12,13,7,8,9,10,11,14,24,4,5,0,1,2,3,17,25,23,26,15,20,21,16,18,19,22,6`.
  In `SKILL_*` terms: `WPN_SWORD, WPN_AXE, WPN_CRUSH, WPN_STAFF, WPN_DIST,
  WPN_SPEAR, WPN_THROW, WPN_HAND, CRITICAL_HIT, SHADMAG, NATRMAG, GODSMAG,
  ELEMMAG, LGHTMAG, DARKMAG, MAGICUSE, ALCHEMY, IDENTIFY, TACTIC, SCOUT,
  HEALING, TALKING, TRADE, STEAL, HACK, SMITH, ATHLETIC`.
- Skill row x: `0x120f8520`; skill row y: `0x120f85a0`; skill UI text
  ids: `0x120f8620`.
- Characteristics row y: `0x120f8738`; characteristics UI ids:
  `0x120f8774`.

## `inv_gui.scr` object rects

- Left attribute panel: objects `90:97` (7 attribute rows + reputation).
- 27 skill rows: objects `100:126`.
- Secondary rows (state/resistance): objects `160:170`.
- Inventory tabs: objects `5:6`; close: object `7`.
- Overlay rects (1024x768 canvas, background
  `engineres/newinv/inventory_skills.bmp`):
  - tabs `(3,4)`, `(62,4)`, `(121,4)`, `(179,4)`, `(239,4)`, each `56x25`;
  - 7 attribute rows `(20,60/88/117/144/172/199/226, 130x23)`;
  - reputation row `(20,262,181x23)`; doll area `(8,296,270x320)`;
  - skill points row `(515,624,210x27)`, text `(526,632)`;
  - personality rows `(296,102,272x24)`, `(296,134,155x23)`,
    `(296,163,272x24)`, `(296,214,164x20)`;
  - characteristics rows `x=296`, `155x20`, y
    `109,140,168,218,244,271,354,381,408,435,462,489,516,543,570`;
  - skills `x=295/544/790`, `y=130/162/194`, `315/348/381`,
    `500/532/564`, `140x20`;
  - close `(920,694,75x68)`, art `engineres/newinv/exit.bmp` /
    `exitl.bmp` on hover.

## Model mapping (implemented)

- `primary`: ids `6,7,8,9,10,11,12,16` in UI order (strength,
  constitution, dexterity, perception, wisdom, intelligence, luck,
  reputation).
- `characteristics`: ids `14,15,16,27,28,18,19,20,21,22,23,24,25,26,17`
  (level, EXP, reputation, life, energy modeled; action points, damage,
  magic, accuracy, crit, armor `-`).
- `skills`: ids `49..75` paired with the `0x1212e0a0` permutation above;
  value from `PersonRuntime::get_skill`.
- `personality`: name (id 5, requested person string, `Hero` in normal
  use), level (id 14, `hero_level_from_experience(EXP)`), experience
  (id 15), reputation (id 16).
- `skill_points = 0` (read-only).

## Real slot0 (`Saves/slot0.sav`, `BT_HERO`)

- STR 30, CON 25, DEX 18, PER 12, INT 5, WIS 7, LUK 9, REP -40,
  EXP 81099.
- Level = `hero_level_from_experience(81099)`.
- Skills by mapped id: `SWORD` 0, `AXE` 15, `THROW` 0, `TALKING` 0
  (asserted in the real test).

## Residual

- Skill-point spending / allocation, save write.
- State/resistance data (tabs show `Нет данных`).
- Remaining combat columns (action points, damage, magic, accuracy,
  crit, armor), weight and magic immunities.
- Tooltips/help (`question.bmp`), idle animation, drag-and-drop, exact
  original personality detail rows.
- Touch mapping (P9) and crafting (`scripts/ui/skills_gui.scr`).
