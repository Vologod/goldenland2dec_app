# P5b2-1 — Paper-doll wear overlays (plan)

Spec: `docs/specs/2026-11-24-p5b2-puppet-overlays-design.md`.

## Task 1 — Failing tests

- `character_overlay_tests`:
  - equipped armor/helmet/elbarms fake inventory → doll region differs
    from the bare background and from the empty inventory;
  - ring A (mask `0x10000`) overlay stays in the left half (`x<=141`);
  - unknown item id → no overlay, no crash.
- Run: expect red before implementation.

## Task 2 — Implementation

- `CharacterOverlay::draw_doll_overlays` + path/anchor/crop helpers;
  call after `draw_background`, before `draw_inventory_items`.
- `use_b_set_` gates the weapon/shield pair.

## Task 3 — Verification

- Focused: `character_overlay_tests`, `glgame_screenshot_tests *character*`.
- Screenshot `--character --load-save 0` (dressed doll) vs background BMP.
- Full `ctest` with `GL_GAME_DIR`.

## Task 4 — Docs and review

- `STATUS`/`PROJECT`: P5b2-1 status + residual (inventory layer,
  animations, active-hand highlight).
- Research: mark geometry implemented.
- Manual visual gate (user).
