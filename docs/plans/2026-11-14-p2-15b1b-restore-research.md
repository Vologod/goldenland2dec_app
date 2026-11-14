# P2-15b1b — Restore persons research Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Доказать маппинг type-2 полей, item/container layout и порядок restore persons; выдать implementation scope.

**Architecture:** Research-only: Ghidra/decompiled + Python-скан type-2 bodies 28 save; документ `docs/research/p2-15b1b-restore.md`.

**Tech Stack:** Ghidra headless (`.local/decomp`), Python 3 (`zlib`/`struct`), optional read-only live diff.

## Global Constraints

- Research-only: не менять `src/`, `tests/`, `tools/`.
- Скрипты/артефакты — `%TEMP%`, не коммитить.
- Факты: RVA/line/offset + corpus; unknowns `unverified` + exact next check.
- Docs UTF-8 без BOM/NUL; markdown через edit/write.
- Коммиты docs-only.

---

### Task 0: STATUS entry

**Files:** `docs/STATUS.md`

- [x] **Step 1:** Insert after `Обновлено:`:

```markdown
## P2-15b1b — Restore persons research (in progress 2026-11-14)

- **Задача:** type-2 field map, 0xC0 item/container layout, restore ordering,
  oracle; docs-only.
- **Spec:** `docs/specs/2026-11-14-p2-15b1b-restore-research-design.md`.
- **Plan:** `docs/plans/2026-11-14-p2-15b1b-restore-research.md`.
- **Критерии:** field map; item/container; restore order; verdict/декомпозиция.
- **Статус:** `in progress`.
```

- [x] **Step 2:** encoding check; commit `docs(status): start restore persons research`.

---

### Task 1: Type-2 field map

**Files:**
- Create: `docs/research/p2-15b1b-restore.md` (§Type-2 fields)
- Read: type-2 reader `FUN_1401e758` (21905-22096) and consumers of
  `+0x5f8/+0x5fc/+0x654/+0x151c/+0x2668/+0x150c/+0x6cc/+0x1630/+0x14fc`;
  `PersonInstance`/`PersonRuntime`/`LevelSimulation` headers.

- [x] **Step 1: Semantic map**

For each field in the b1a table: destination, kind, consumer(s) RVA/line,
unit, proposed port field. Mark verified/inferred/unverified.

- [x] **Step 2: Corpus correlation**

Python scan of type-2 bodies (28 save, 2599 sections): coordinate pair vs
SEF person positions (slot0/l1_1), direction-like values, name ↔ PNAME
order, tribe id ↔ BT_TRIBE names; record ranges/identity 0/N.

- [x] **Step 3: Write §Type-2 fields** table + corpus evidence; unknowns
  with exact next checks.

---

### Task 2: `0xC0` item record + container manager

**Files:**
- Modify: `docs/research/p2-15b1b-restore.md` (§Items, §Container walk)
- Read: `FUN_1405ca18`/`FUN_1404c890`, `FUN_1404c7a8`, `FUN_1404f4bc`,
  p5 item layout; `FUN_14004f14`/`FUN_14004a20` manager.

- [x] **Step 1: Item record**

Map `0xC0 + u32 m + 0x14*m` for entity containers: id/count/flags/type/
style/durability/mods offsets; compare with `BT_HERO` proven layout; mark
differences.

- [x] **Step 2: Container walk**

Document how to walk `entity+0xc/+0x10/+0x109c` from outside
(`DAT_14088df8+0xc`, count/entries/stride) for the oracle; note whether
call-free walk is possible.

- [x] **Step 3:** Write §Items/§Container walk with offsets/RVAs and
  unverified items.

---

### Task 3: Restore ordering + port mapping + oracle

**Files:** `docs/research/p2-15b1b-restore.md` (§Restore, §Port, §Oracle)

- [x] **Step 1: Restore algorithm** — SEF persons → save entities (match by
  name/order; duplicate policy) → apply state → inventory override/NPC loot
  suppression. Define what is replaced/merged/ignored; attached list; RNG
  implications.
- [x] **Step 2: Port mapping/gap table** — PersonInstance/PersonRuntime/
  InventoryRuntime/LevelSimulation; what can be restored now, what needs
  more reverse.
- [x] **Step 3: Oracle plan** — raw body/name/type, position/direction,
  inventory counts; slot0/slot2/l1_1; 0/N criteria.

---

### Task 4: Verdict/decomposition + docs/commit

**Files:** research doc, `docs/STATUS.md`, `docs/research/parity-matrix.md`,
`docs/DECISIONS.md` (if needed), spec/plan.

- [x] **Step 1: Verdict** — `implementable`/`scoped`/`blocked`; decompose:
  - `P2-15b1b1` restore position/direction/params;
  - `P2-15b1b2` inventory/containers (if item map proven);
  - `P2-15b1b3` NPC loot override/ordering (if needed).
  Acceptance per part.
- [x] **Step 2:** STATUS done; parity/OQ-12 honest; DECISIONS if a decision.
- [x] **Step 3:** encoding; commit docs incl. spec/plan
  `docs: research restore persons`.

---

## Self-review notes

- Spec §3.1–3.7 → Tasks 1–4; acceptance §6.
- No implementation; restore pseudocode only.
- Item/container blocked → b1b scope reduced explicitly, not guessed.
