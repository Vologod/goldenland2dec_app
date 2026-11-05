# P5b1 Inventory UI — Research Gate Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Извлечь из оригинала точную геометрию inventory UI, семантику фильтров и item→name/description/icon, чтобы P5b1 реализовать без подгонки.

**Architecture:** Ghidra (Client.dll UI code) + `scripts/ui/inv_gui.scr` + `engineres/newinv` + ITM/SDB/items.pak; результат — `docs/research/p5b1-inventory-ui.md`. Implementation plan — после 0/N проверки на slot0/slot2.

**Tech Stack:** Ghidra headless, существующие `glpak`, Python/PIL для BMP-проверки; без изменений игрового кода.

## Global Constraints

- Источник истины: `Client.dll` SHA-256 `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`.
- Оригинальные ресурсы не коммитить; в research — только offsets/rects/имена.
- Docs UTF-8 no BOM; без game code в этом этапе.
- Acceptance: точные rects сетки/панелей, mapping фильтров 1..7, item name/description/icon на slot0+slot2; unresolved → BLOCKED.

---

### Task 1: Inventory UI layout, filters, item presentation

**Files:**
- Create: `docs/research/p5b1-inventory-ui.md`
- Read: `scripts/ui/inv_gui.scr`, `engineres/newinv/*`,
  `docs/research/p5-save-equipment.md`

**Interfaces:**
- Produces: таблицы `container → UI panel`, grid cell rects/page count,
  filter button → item type/mask, item id → name/description/icon path
  (slot0/slot2), fallback rules.

- [ ] **Step 1: Extract UI script and objects**

Extract `scripts/ui/inv_gui.scr` and `engineres/newinv/*` via `glpak`.
Parse `OBJECT_START` blocks into a table: id/type/left/top/width/height/
images/comments. Identify container 0/1/2 panels, grid, filters, scroll,
skills/inventory tabs.

Expected: complete object table; grid/page rects or explicit code-drawn
objects.

- [ ] **Step 2: Resolve code-drawn grid/page/filter logic**

In Ghidra Client.dll, find references to `inv_gui.scr` / UI object ids and
decompile the inventory panel handler(s). Record exact grid cell size,
columns/rows, page stride, filter masks/type groups, and container mapping.

Expected: formula/constants + RVAs; if geometry is image-only, derive exact
cell rects from `inventory_secondary.bmp` and record pixel coordinates.

- [ ] **Step 3: Verify layout against original resource**

Overlay table rects on `inventory_secondary.bmp`; verify tabs/filters/
scroll/exit/puppet/hotbar coordinates are inside their visual frames.
Check `inventory_skills.bmp` tab variant.

Expected: 0 mismatched/overlapping anchors; screenshot/PNG evidence in
research notes.

- [ ] **Step 4: Resolve item name/description/icon**

For slot0/slot2 inventory items: `item_id` → `items/data/<id>.itm`
(`description_id`, `icon_name`, type/price/weight) → `lit_names.sdb` /
`descriptions.sdb`; verify strings and icon BMP path/format. Define fallback
for missing ITM/SDB/icon.

Expected: 0/N on all slot0/slot2 items; missing assets listed.

- [ ] **Step 5: Write research doc + commit**

Write `docs/research/p5b1-inventory-ui.md` with object table, grid/filter
formulas/RVAs, item→text/icon table, fallbacks, and 0/N checks.

Commit: `docs(p5b1): record inventory UI layout and item catalog`.

Expected: implementation plan can use concrete rects/masks without guesses.

---

## Next phase

After Task 1 passes, create
`docs/plans/2026-11-04-p5b1-inventory-ui-implementation.md` with parser,
InventoryModel/View, ItemCatalog, overlay, input, and tests.
