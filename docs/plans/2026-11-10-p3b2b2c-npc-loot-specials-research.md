# Phase 1b2b2c — NPC loot specials research (plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Доказать/опровергнуть воспроизводимость specials NPC-лута; production-код не меняется.

**Architecture:** Research-only: item manager `DAT_14088df4`, `FUN_14047284`, R4–R10, DB source, oracle, verdict.

**Tech Stack:** Ghidra/objdump, decompiled corpus, Python 3.9.

## Global Constraints

- Server.dll SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Поведение — только из бинарников (D-029); без догадок.
- Research-only; docs UTF-8 без BOM/NUL; коммиты разрешены.
- Checkpoint 21:00–05:00.

---

### Task 1: Modifier DB and algorithm

**Files:**
- Create: `docs/research/p3b2b2c-npc-loot-specials.md`

- [ ] **Step 1: DB source**

Trace `DAT_14088df4` init (`63169`), vtable `+4/+8/+0x14/+0x1c`,
item modifier data files (`items.pak`, `sdb/items/itemdescparts.sdb`,
`magic.pak`); record layout/fields and RVA/file:line.

- [ ] **Step 2: `FUN_14047284`**

Record pseudocode: pools/groups (`+0x68/+0x78/+0x88`,
`+0xc/+0x1c/+0x2c`), class/type multipliers, R6–R10 calls/args/order,
ROUND/trunc, applied record fields/flags.

- [ ] **Step 3: `roll_special_count`**

Record R4/R5 conditions/values (`flags&4`, `chunk==1`, `item+0x78==0`).

- [ ] **Step 4: Real data**

Count modifier-bearing items/fields in `.itm`/real data; effect on
price/trade; link to D-058.

- [ ] **Step 5: Verdict/oracle**

State `implementable` or `blocked`; if blocked, exact missing evidence.
If implementable, define oracle (live/differential) and acceptance.

- [ ] **Step 6: Verify/commit**

UTF-8 no BOM/nul=0; commit `docs(research): record NPC loot specials`.

---

### Task 2: Status/handoff

**Files:**
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md` (verdict)

- [ ] **Step 1: Docs**

Record verdict, proven/unproven, next action (implementation spec or
remain blocked). Add Open Questions if blocked.

- [ ] **Step 2: Encoding/commit**

`bom=False nul=False`; commit `docs: record NPC loot specials verdict`.
