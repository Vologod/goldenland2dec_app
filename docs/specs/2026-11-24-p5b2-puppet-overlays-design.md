# P5b2-1 — Paper-doll wear overlays (design)

Status: approved (2026-11-24, inline in `CharacterOverlay`).
Scope: character sheet doll only.

## Evidence

`docs/research/p5b2-puppet-overlays.md`:
- ITM `puppet_name` → `items/res/<name>` (BMP, magenta key), e.g.
  armor 318 `puppet\Bronya\Almaz\00.bmp` (190x230).
- Non-accessory layers: `x=5`, `y = 768 - tex_h - 146` (left 5, bottom
  622); full sprite.
- Accessories (`mask & 0xF0000`): half-canvas 135 px:
  - `mask & 0x50000` (ring A, bracer A): src `(0,0,min(w,135),h)`, `x=5`;
  - otherwise (amulet, ring B, bracer B): `x0 = w<135 ? w-30 : 135`,
    src `(x0,0,w,h)`, `x = x0+5`.
- Layer order (`FUN_120683a8`): `0x200000` (elbarms) → `0x100000`
  (armor) → `0xF8000` (amulet/rings/bracers) → `0x4000` (helmet) →
  `0xC00000` (weapon/shield) → `0x3000`.
- Active weapon/shield set: `this+0x1d4` (0=A, 1=B); the other pair is
  not drawn.
- Verified by composition at `(5, 622)` on `inventory_secondary.bmp`.

## Data

`ItemCatalogEntry::item.puppet_name` (already parsed from ITM). Path =
`items/res/` + `normalize_virtual_path(puppet_name)`, `.bmp` appended
when the name has no extension.

## Rendering

`CharacterOverlay::draw_doll_overlays`, called after the background and
before slot icons:

- for each group mask in original order, for each equipped item whose
  `flags & 0x00FFF000` intersects the group: draw the puppet texture
  (magenta key) with the anchor/crop above;
- skip weapon A (`0x1000`/`0x400000`) when `use_b_set_`, skip weapon B
  (`0x2000`/`0x800000`) otherwise;
- empty `puppet_name`, missing file or decode failure → skip silently
  (texture failure cache).

## Tests

- Synthetic equipped armor 318 + helmet 280 + elbarms 397: doll region
  differs from the bare background (> 1000 px) and from an empty
  inventory.
- Accessory ring A 233 stays inside the left half (`x <= 141`).
- Unknown item id / missing asset: no overlay, no crash, doll region
  equals the bare background (±8 px for the font-atlas seam).
- Full `ctest` with `GL_GAME_DIR`; manual visual gate.

## Non-goals

- `InventoryOverlay` (P5b1) — follow-up, same mechanics.
- Wear-set A/B alternative frames, attack/idle overlay animations,
  selection highlight of the active hand item.
- Active weapon/shield set from the save (`local_player+0x6E4`): the
  port defaults to set A (same as the slot icons).
