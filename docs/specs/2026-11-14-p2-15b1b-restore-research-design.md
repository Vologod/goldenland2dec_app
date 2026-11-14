# P2-15b1b — Restore persons from save entities (research design)

Status: research complete (2026-11-14); verdict `scoped`
(decomposition `P2-15b1b1`/`b1b2`/`b1b3`)
Phase: P2-15b1b research (docs-only; precedes restore implementation)
Layers: research only

## 1. Goal

Определить, как восстановить persons/entities из `BT_ENTS` в состояние порта:
какие поля type-2 body куда мапятся, как читать `0xC0` item-записи и
контейнеры, в каком порядке применять save поверх SEF/NPC-лута, и как
проверять 0/N. Production-код не меняется.

## 2. Evidence

- `docs/research/p2-15b1-save-entities.md`: types `{2,8,16,32}`, type-2 body
  `540 + strings + 3 containers + tail`; `BT_PNAME` == type-2 order
  2599/2599; restore flow `FUN_1405c83c` → reader → `FUN_14003718`
  (`+0x2564`); field offsets table (name/+0x40/+0x44/coords/+0x5f8…).
- `docs/research/p5-save-equipment.md` §4/§5: `BT_HERO` item record
  `0xC0 + u32 subcount + 0x14*subcount`; container layout; but entity
  containers (`+0xc/+0x10/+0x109c`) may share it (unverified).
- `p2-15a`/`p2-15b`: tail selected by level id; SEF supplies static persons;
  port currently regenerates NPC loot at level load (`generate_npc_inventories`).
- Research gaps: most type-2 field semantics, `0xC0` entity item offsets,
  container manager walk (`DAT_14088df8+0xc`), restore/override order.

## 3. Research questions

1. **Type-2 field map.** Для каждого поля таблицы (name, +0x40/+0x44,
   coords, +0x5f8/+0x5fc, +0x654/+0x658, +0x151c/+0x1520/+0x1524,
   +0x2668/+0x266c, +0x150c, +0x6cc, +0xff0, +0x14fc/+0x1500/+0x1504,
   +0x1630): semantics, unit (tile/pixel/ms/id), mapping на
   `PersonInstance`/`PersonRuntime`/`LevelSimulation`. Minimum: position +
   direction + tribe/params.
2. **`0xC0` item record.** Точные offsets id/count/flags/type/style/
   durability/mods в entity-контейнерах; совпадают ли с `BT_HERO` record
   (`p5-save-equipment` §5); sub-array `0x14*m` поля.
3. **Container manager.** Как читать `entity+0xc/+0x10/+0x109c` снаружи
   (`DAT_14088df8+0xc`, `FUN_14004f14`/`FUN_14004a20`) для oracle; entry
   stride/id/count.
4. **Restore ordering.** SEF persons → save entities (match by name/order)
   → NPC loot: что заменяется, что merge, что игнорируется; фильтр
   `+0x150 == 0`; attached list; duplicate names.
5. **Port mapping.** `PersonInstance`, `PersonRuntime`, `InventoryRuntime`,
   `LevelSimulation`: какие поля можно применить сейчас, какие требуют
   дополнительного reverse; как отключить/перезаписать NPC loot при load.
6. **Oracle.** Как проверять: raw body equality, name/type sequence,
   позиция/направление, inventory counts/ids; slot0/slot2/l1_1.
7. **Verdict/decomposition.** Что входит в b1b implementation; что
   остаётся b1c/out-of-scope.

## 4. Methods

- Ghidra/decompiled readers/consumers; cross-ref p5 item layout.
- Python-скан type-2 bodies 28 save: field ranges/correlations
  (coords vs SEF positions, direction, names) — `%TEMP%`.
- Read-only live diff только при неоднозначности маппинга; user-assisted.
- No production changes.

## 5. Deliverable

`docs/research/p2-15b1b-restore.md`:

1. Type-2 field map table: save offset/kind → semantics → port field →
   status (verified/inferred/unverified).
2. Item record map + container manager walk.
3. Restore algorithm (pseudocode): SEF → match entities → apply state →
   inventory override/loot suppression.
4. Port mapping/gap table.
5. Oracle plan (0/N) and implementation decomposition/acceptance.
6. Open Questions (no TBD).

## 6. Acceptance

1. Type-2 fields have evidence (RVA/line/consumer) or unverified+next; at
   least position/direction/tribe/params decided.
2. `0xC0` item record offsets proven for entity containers or explicitly
   blocked with exact next.
3. Container manager walk documented for oracle.
4. Restore order/override behavior defined and testable.
5. Verdict/decomposition with b1b acceptance.
6. Docs-only; UTF-8 no BOM/NUL.

## 7. Out of scope

- Implementation restore (separate spec/plan).
- Save write/P2-15c, diary/hstry/vars/tribe semantics beyond person fields.
- Animation/UI.

## 8. Risks

- `0xC0` entity records may not match `BT_HERO`; then inventory restore
  blocks and b1b reduces to position/params.
- Type-2 body may contain pointers/ids tied to runtime instances; restore
  may need re-resolution.
- Restore ordering can desync RNG/NPC loot; must be defined before code.

## 9. References

- `docs/research/p2-15b1-save-entities.md` §Body layout/§Restore/§Port.
- `docs/research/p5-save-equipment.md` §4–§5.
- `docs/research/p2-15a-save-payload.md` tail/reader.
- `docs/specs/2026-11-12-p2-15b-save-load-design.md`.
