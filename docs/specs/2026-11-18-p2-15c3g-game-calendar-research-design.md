# P2-15c3g — Game calendar research (design)

Status: implemented (2026-11-19)
Task: `P2-15c3g` (research-only; follow-up to P2-15c3f, D-087)
Scope: `docs/research/`; no `src/` changes.

## 1. Context

`BT_INFO` carries a 0x20-byte world time blob; `BT_RGINF` carries the same
8×u32 layout as the last item-regeneration timestamp. The port keeps both raw
because it has no game clock. OQ-3 (time blob layout) is still open.

## 2. Questions

1. What is the exact field layout and calendar of `BT_INFO` time and
   `BT_RGINF` datetime (year/month/day/weekday/hour/min/sec/ms, units, epoch)?
2. Where is the clock implemented (`Server.dll` vs `GoldenLand.exe`), and what
   are the add/compare/get/set/advance contracts?
3. How does game time advance (real-time factor, update cadence, speed)?
4. How does `gv_items_regenerate_interval` relate to `+0x26c0`/`+0x26a0`?
5. What is implementable in the port now vs blocked on live evidence?

## 3. Method

- Static: `Server.dll` calendar object `DAT_14088e14`, vtable `0x14092b98`
  (add `0x5b38c`, compare `0x5b490`, get `0x5bb84`, set `0x5bbc4`, advance
  `0x5bc84`), helpers `FUN_1405b140`/`FUN_1405ad58`/`FUN_1405b504`,
  constants at `0x92b20..0x92b88`; writer/reader at 35389-35390/35633-35634;
  RGINF reader/writer and due-check at 35812-35846.
- Corpus: 28 `BT_INFO` blobs and all 2,599 `BT_RGINF` datetimes; range and
  monotonic checks; compare with the header SYSTEMTIME (wall clock).
- No live capture required for the layout; rate/cadence marked inferred.

## 4. Deliverable

`docs/research/p2-15c3g-game-calendar.md` with:

- layout table (offsets, 1-based vs internal, calendar constants);
- clock contract/vtable and advance algorithm (rate, gate, speed);
- interval/due-check semantics and port mapping;
- verdict `implementable (scoped)` or `blocked` for unresolved items;
- decomposition: `GameCalendar` model (layout + advance + interval) in port,
  `BT_INFO`/`RGINF` live time wiring (c3f follow-up), live parity check.

## 5. Acceptance

- Every field has an offset/unit/evidence source; every claim is either
  proven (static/corpus) or labelled inferred/blocked with a next check.
- 28 BT_INFO + 2,599 RGINF corpus scan results recorded.
- No production code or test changes; docs UTF-8 no BOM/NUL.
