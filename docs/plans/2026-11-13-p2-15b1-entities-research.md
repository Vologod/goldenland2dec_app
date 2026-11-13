# P2-15b1 — Save entities/persons restore research Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Разобрать `BT_ENTS`/`BT_RGINF`/`BT_PNAME`, восстановить class map и выдать scope restore persons.

**Architecture:** Research-only: Python-скан tail-секций 28 save + Ghidra/decompiled mapping фабрики `FUN_1405c83c`, vtable readers, `FUN_14032c2c`; документ `docs/research/p2-15b1-save-entities.md`.

**Tech Stack:** Python 3 (`zlib`/`struct`), Ghidra headless (`.local/decomp`), optional read-only live diff.

## Global Constraints

- Research-only: не менять `src/`, `tests/`, `tools/`.
- Скрипты/артефакты — `%TEMP%`, не коммитить.
- Факты: файл + SHA-256 + RVA/offset + decompiled line/tool.
- Не выдавать body size/layout за доказанный без reader/формулы; unknowns —
  `unverified` + exact next check.
- Docs UTF-8 без BOM/NUL; markdown через edit/write.
- Коммиты docs-only после approve.

---

### Task 0: STATUS entry

**Files:** `docs/STATUS.md`

- [x] **Step 1:** Insert after `Обновлено:`:

```markdown
## P2-15b1 — Save entities/persons research (in progress 2026-11-13)

- **Задача:** BT_ENTS class map/body layout, BT_RGINF/BT_PNAME, restore
  persons; docs-only.
- **Spec:** `docs/specs/2026-11-13-p2-15b1-entities-research-design.md`.
- **Plan:** `docs/plans/2026-11-13-p2-15b1-entities-research.md`.
- **Критерии:** type map 28/28; body size/layout per type; restore mapping;
  verdict/декомпозиция.
- **Статус:** `in progress`.
```

- [x] **Step 2:** encoding check; commit `docs(status): start save entities research`.

---

### Task 1: Tail inventory 28/28

**Files:**
- Create: `docs/research/p2-15b1-save-entities.md` (§Tail inventory)
- Create (not committed): `%TEMP%\p2_15b1_scan.py`

- [x] **Step 1: Scan script**

```python
import glob, os, struct, zlib
root = r"I:\Games\Златогорье-2\Saves"
for path in sorted(glob.glob(os.path.join(root, "slot*.sav")),
                   key=lambda p: int(os.path.basename(p)[4:-4])):
    raw = open(path, "rb").read()
    body = zlib.decompress(raw[112:])
    # parse 7 blocks, then tail at 112 + u32(raw,0x6c)
    # for each tail entry: name, section, decompress section
    # for the selected level (BT_INFO string3) and every section:
    # record section len, BT_PNAME presence/count, BT_RGINF len, BT_ENTS len
```

Implement full block/tail parsing from `docs/research/p2-15a-save-payload.md`
(§Minimum read set); print a table per save and per section.

- [x] **Step 2: Run** (`py -3`), record: 28/28 sections, tag presence,
  `BT_RGINF` length (36), `BT_ENTS` raw length, `BT_PNAME` entry count.
- [x] **Step 3: Write §Tail inventory** with the real table/histogram and
  exact command.

---

### Task 2: Entity factory/type map

**Files:**
- Modify: `docs/research/p2-15b1-save-entities.md` (§Type map)
- Read: `.local/decomp/server/decompiled.c` (`FUN_1405c83c`,
  `FUN_140368d4`, `FUN_1401e424`), `functions.csv`

- [x] **Step 1: Factory dispatch**

```powershell
Select-String -Path .local\decomp\server\decompiled.c -Pattern "FUN_1405c83c|FUN_140368d4" |
  ForEach-Object { "$($_.LineNumber): $($_.Line.Trim())" }
```

Read the factory switch: map each `type` case to a constructor RVA/class;
record the default/unknown behavior.

- [x] **Step 2: Readers**

For every constructor/class, find the vtable `+4` reader and `+0x0`
destructor/other slots; record RVA + decompiled lines. For person classes,
cross-reference `FUN_1401e424` (`BT_HERO` reader) and the save entity body.

- [x] **Step 3: Write §Type map** table: type | factory RVA | reader RVA |
  class/notes. Mark unresolved types.

---

### Task 3: Body size/layout + parse algorithm

**Files:**
- Modify: `docs/research/p2-15b1-save-entities.md` (§Body layout,
  §Parse algorithm)

- [x] **Step 1: Derive body size per type**

From each reader: fixed body size or self-delimiting count/string fields.
Record enough to advance the cursor: `size(type, body)` formula. If a type
cannot be sized, mark `unverified` + exact Ghidra target.

- [x] **Step 2: Parse 28/28**

Extend `p2_15b1_scan.py` with the derived sizes; parse every tail section's
`BT_ENTS` to the `type==1` terminator. Record:
- type histogram (counts per type),
- terminator present 28/28 (and all sections),
- consumed bytes == `BT_ENTS` body length (0 mismatches),
- any type not in the map.

- [x] **Step 3: Write §Body layout/§Parse algorithm** with sizes/formulas
  and the 28/28 results; unknowns explicit. If parsing cannot consume
  exactly, report BLOCKED with the first failing type/section.

---

### Task 4: `BT_RGINF`, `BT_PNAME`, restore flow, port mapping

**Files:**
- Modify: `docs/research/p2-15b1-save-entities.md` (§RGINF/§PNAME,
  §Restore, §Port mapping)
- Read: `FUN_14032c2c`, `FUN_140325fc`, lookup at 38563, `FUN_140368d4`,
  `src/game` (`person.hpp`, `person_runtime.hpp`, `inventory_runtime.hpp`,
  `level_simulation.hpp`)

- [x] **Step 1: `BT_RGINF`** — fields of `u32 + 0x20`; source at 36040,
  consumer(s) `+0x26c0`/`+0x26a0`; state whether needed for restore.
- [x] **Step 2: `BT_PNAME`** — format/order/count; relation to `BT_ENTS`
  (names only for person entities?); lookup 38563; required for restore.
- [x] **Step 3: Restore flow** — what the original does with `BT_ENTS`:
  create entity (`FUN_1405c83c`), read body, install (`FUN_140368d4`);
  which person fields come from body vs SEF vs inventory/loot. Order.
- [x] **Step 4: Port mapping** — table: save field → port struct/field →
  available/missing/unverified. Cover position/direction/route/params/
  inventory/health; `PersonInstance`/`PersonRuntime`/`InventoryRuntime`/
  `LevelSimulation`.
- [x] **Step 5: Oracle plan** — how to verify restore 0/N (raw body equality,
  named fields, inventory counts) on slot0/slot2 and l1_1.

Completion (2026-11-13): all five steps are in
`docs/research/p2-15b1-save-entities.md` §RGINF/§PNAME/§Restore flow/
§Port mapping. Inventory-count live oracle is explicitly deferred pending
the container-map and entity item-record reverse noted in §Oracle plan step 6.

---

### Task 5: Verdict/decomposition + docs/commit

**Files:**
- Modify: `docs/research/p2-15b1-save-entities.md`, `docs/STATUS.md`,
  `docs/research/parity-matrix.md`, `docs/DECISIONS.md` (if needed),
  spec/plan.

- [x] **Step 1: Verdict** — `implementable`/`scoped`/`blocked`; if scoped,
  split:
  - `P2-15b1a` entity parser + type map;
  - `P2-15b1b` person restore (mapping, SEF/loot ordering);
  - `P2-15b1c` `BT_RGINF`/`BT_PNAME` if needed.
  Acceptance per part.
- [x] **Step 2:** STATUS done with proven facts/unknowns/next; parity
  OQ-12/P2-15 row updated honestly (still `partial` until implementation).
- [x] **Step 3:** encoding checks; commit docs incl. spec/plan:
  `docs: research save entities`.

Completion (2026-11-13): verdict `scoped`; `P2-15b1a`/`P2-15b1b` acceptance in
`p2-15b1-save-entities.md` §Verdict / decomposition. `BT_RGINF`/`BT_PNAME` are
folded into `P2-15b1a` (no separate `P2-15b1c` needed). D-069 recorded; parity
P2-15 stays `partial`; OQ-12 marked resolved at research level.

---

## Self-review notes

- Spec §3.1–3.7 → Tasks 1–5; acceptance §6 → Tasks 1/3/4/5.
- Parse algorithm only accepted if it consumes `BT_ENTS` exactly 28/28 to
  terminator; otherwise BLOCKED with failing type.
- No production code; restore design only.
