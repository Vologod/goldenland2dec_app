# Phase 1b2b2a — NPC `.inv` loot generation research (plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Completion note (2026-11-07):** Tasks 1–2 + verdict выполнены
> (research + docs-only handoff); все шаги отмечены `[x]` как журнал
> исполнения. Итоговый вердикт и блокеры — в
> `docs/research/p3b2b2-npc-loot-server.md` §13.

**Goal:** Доказать или опровергнуть возможность точного воспроизведения NPC-лута из `.inv`; production-код не меняется.

**Architecture:** Research-only: разбор `FUN_14047784` (парсер), `FUN_140489ec` (генератор), RNG и точки вызова; итоговый verdict и handoff.

**Tech Stack:** Ghidra/llvm-objdump, decompiled corpus, Python 3.9 (temp), PowerShell.

## Global Constraints

- `Server.dll` SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Поведение — только из `Server.dll` (D-029); без догадок.
- Research-only: никаких изменений production-кода.
- Документы UTF-8 без BOM, `nul=0`.
- Коммиты разрешены (2026-11-06).
- Checkpoint в окне 21:00–05:00.

---

### Task 1: Recover `.inv` format and parser

**Files:**
- Create: `docs/research/p3b2b2-npc-loot-server.md`

- [x] **Step 1: Grammar**

Read `FUN_14047784` (`decompiled.c:48581`) and record the exact `.inv`
grammar, header fields/defaults, `item` line fields, `special`, comments,
and the in-memory layout (`+0x38/+0x40/+0x44/+0x48/+0x50/+0x54`).

- [x] **Step 2: Real files**

Read representative real `.inv` (`scripts/inventory/inv001_swordsman.inv`,
`scripts/inventory/boxes/*.inv`, `scripts/inventory/hero_items.inv`) and
check the grammar covers all observed lines.

- [x] **Step 3: Verify and commit**

UTF-8 no BOM/nul=0. Commit `docs(research): record .inv loot format`.

---

### Task 2: Recover generator, RNG and timing

**Files:**
- Modify: `docs/research/p3b2b2-npc-loot-server.md`

- [x] **Step 1: Generator**

Read `FUN_140489ec` (`decompiled.c:49416`) and write pseudocode: entry
selection, player level/`level_offset`, `regenerate_chance`,
`chance_decrease`, `quantity_decrease`, quantity roll, item creation,
stacking/insertion order, money.

- [x] **Step 2: RNG list**

List every RNG call in order (`FUN_14006288`, `FUN_140061e4`, `rand`),
with argument/parameter and whether it is conditional. Identify the RNG
source/seed and whether it is the shared MSVC stream.

- [x] **Step 3: Timing/callers**

Trace callers of `FUN_1404805c`/`FUN_140489ec`/`FUN_14047784` to determine
when loot is generated (level load, spawn, lazy), and what level/input is
used. Record RVA/file:line.

- [x] **Step 4: Container records**

Record insertion order, item record fields (type/flags/style/max_stack),
stacking, and money.

- [x] **Step 5: Verdict**

State `implementable` or `blocked`, with exact missing evidence. If
`implementable`, list inputs/state needed and a verification plan (live
oracle/differential). If `blocked`, list options.

- [x] **Step 6: Verify and commit**

Commit `docs(research): record NPC loot generation`.

---

### Task 3: Status/handoff

**Files:**
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md` (if verdict changes P3/P5 status)
- Modify: `docs/DECISIONS.md` (if a parity decision is needed)

- [x] **Step 1: Update docs**

Record the verdict, what is proven/unproven, and the next action
(implementation spec for 1b2b2b, live-oracle task, or keep NPC loot
unsupported). If blocked, add Open Questions with exact missing facts.

- [x] **Step 2: Encoding check**

`bom=False nul=False` for changed docs.

- [x] **Step 3: Commit** `docs: record 1b2b2a NPC loot research verdict`.
