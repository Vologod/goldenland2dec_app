# P2-15c3a — Save write-back research (design)

Status: approved (2026-11-17)
Task: `P2-15c3a` (research-only; follow-up to P2-15c4b, D-082)
Scope: `docs/research/`, `docs/specs/`, `docs/plans/`, `docs/STATUS.md`,
`docs/PROJECT.md`, `docs/DECISIONS.md`; no `src/`/`tests/`/`tools/` changes.

## 1. Context

`P2-15c1`…`P2-15c4b` implemented a save container writer, typed
`BT_VARS`/`BT_TRIBE` encoders, header patching, explicit-path write and
slot writes. The written save is a **snapshot** of the payload that was
loaded: `BT_HERO`, every tail section and the container bodies are copied
from the parsed payload, `VARS`/`TRIBE` are re-encoded from the loaded
values, and only the header date/weekday/title are new (D-079…D-082).

A save produced after live gameplay must reflect the live simulation:
hero/party state, person position/HP/EP/containers/loot, quests/history,
visited-level tails. The next decision is per save domain:

- **patch-in-raw** — keep the loaded raw bytes and patch known fixed
  offsets from live state;
- **full-inverse** — rebuild the body from live state using the original
  writer contract.

No implementation may start before this split is evidence-backed.

## 2. Goal and research questions

**Goal:** for every save domain decide `raw`, `patch-in-raw`,
`full-inverse` or `blocked`, with evidence from `Server.dll` (writer/reader
contract) and the 28-save corpus, and turn the remaining work into
implementable follow-up tasks under the full live-campaign fidelity
target.

Research questions:

1. Which save domains can change during live play, and which writer
   functions/fields carry those changes?
2. For each mutable domain: are the writer offsets fixed and independently
   patchable, or does the writer require a full variable-length inverse?
3. Which domains are blocked by unresolved semantics (DIARY `0x58`,
   HSTRY ring, type-2 opaque fields, HERO `0x11c`/`0x600`), and what is the
   exact next check for each?
4. What does the current snapshot writer already preserve semantically,
   and where does it diverge from the original layout?
5. What is the minimal ordered decomposition (tasks, dependencies,
   acceptance oracles) to reach live-state save write?

## 3. Scope

In scope:

- header and `BT_INFO`;
- `BT_VARS`, `BT_TRIBE`;
- `BT_DIARY`, `BT_HSTRY`;
- `BT_HERO`: `0x000` header, `0x11c` state block, `0x128` header, `0x600`
  buffer, three containers, post-container `2×u32`, `0x80` tail
  (`4×0x20`), `extra` records, attached list;
- tail dictionary and every per-level section: `BT_RGINF`, `BT_PNAME`,
  `BT_ENTS` types 2/8/16/32, ordering and snapshot filters;
- live mutation scenarios (movement/death/spawn, loot/regeneration,
  item add/remove/transfer/trade, VARS, diary/history, level change).

Out of scope: implementation of live write; DIARY `0x58` field semantics
(`P2-15b4c`); HSTRY ring ordering (`P2-15b4d`); `BT_SCRN `; save UI.

## 4. Inputs

| Artifact | SHA-256 |
|---|---|
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| `Saves/slot0.sav` | `8AC545FE9715EF11C68B1CCA8617BEB03003ECA360F07F71C26E3FC5E5C73442` |
| 28-save manifest | `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |

Existing evidence: `p2-15a-save-payload.md`, `p2-15b1-save-entities.md`,
`p2-15b1b-restore.md`, `p2-15b4-diary-hstry.md`, `p5-save-equipment.md`,
`p3b2b2-npc-loot-server.md`; decisions D-069…D-082.

## 5. Method

### 5.1 Static writer/reader contract

Build the serialization order and, for each block, the exact memory
source the writer reads. Known anchors (re-verify in decompiled.c):

| block | writer | lines |
|---|---|---|
| header | `FUN_14031b64` | 35423–35522 |
| `BT_INFO ` | inline in `FUN_14031b64` | 35393–35394 |
| `BT_VARS ` | `FUN_14002324` | 1383–1416 |
| `BT_TRIBE` | `FUN_14016828` | 16955–16990 |
| `BT_DIARY` | `FUN_14058768` | 60171–60226 |
| `BT_HSTRY` | object `iVar3+0x28` vtable `+4` | 35404 |
| `BT_HERO ` | `FUN_1401e690` + `FUN_1401e340` | 21880–21899, 21743–21774 |
| item record | `FUN_1405c9d8 → FUN_1404c7a8 → FUN_1404f4bc` | 21892–21894 |
| HERO tail `0x80` | `FUN_1401d84c` / `FUN_1401d8bc` | 21824–21860 |
| tail dictionary | `FUN_14032c2c` | 35980–36105 |

Reader anchors for inverse symmetry: `FUN_1403210c` (RVA `0x3210c`),
`FUN_1401e424`, `FUN_1405c9d8`, tail reader 35604–35626.

Open fields to resolve or explicitly block: type-2 `+0x40`, `+0x44`,
`+0x654`, `+0x658[8]`, `+0xff0[0x11c]`, `+0x6cc`; HERO `0x000`,
`0x11c`, `0x128`, `0x600`, post-container `2×u32`, `0x80` tail, `extra`
records, attached list. Each gets: writer site, live source object, whether
the port models it, verdict.

### 5.2 Baseline re-encode experiment (28 saves)

Read-only Python script under `.local/research/c3a/` (not committed;
SHA-256 recorded in research):

1. parse each `slotN.sav` with the current payload framing;
2. rebuild the file with the current writer policy (header bytes kept,
   main stream `tag+u32+body`, zlib, tail dictionary from raw sections)
   without block replacements;
3. record: decompressed main-stream equality, per-tail-section raw
   equality, tail name/order equality, header byte diff count, zlib
   compressed-length deltas, first byte-diff offset;
4. repeat with `BT_VARS`/`BT_TRIBE` bodies replaced by the c2 encoders
   (must stay 28/28 byte-equal, D-080);
5. report the domains where current snapshot write loses live information
   (by construction: HERO/tail/DIARY/HSTRY bodies are never rebuilt).

The experiment must not write into the game directory; output goes to
`.local/research/c3a/out/`.

### 5.3 Write-back matrix

One row per domain, columns:

`domain | SavePayload field | live port state | writer contract
(RVA/lines) | corpus evidence | verdict | follow-up task | acceptance
oracle`.

Verdict rules:

- `raw` — live state cannot differ from the loaded raw bytes in the port
  scope, or the domain is read-only (`BT_SCRN `).
- `patch-in-raw` — fixed offsets, port holds the equivalent value, and all
  variable-length parts stay unchanged; requires an exact offset map and
  clamp/normalization rules.
- `full-inverse` — length/count/order can change; requires the complete
  writer contract and an encode↔decode 0/N corpus oracle.
- `blocked` — semantics or writer path unresolved statically; must name
  the exact next check (decompile target or live capture) and record a
  non-claim.

### 5.4 Scenario cross-check

Enumerate live mutations from the port and map each to the matrix:

| scenario | expected save change |
|---|---|
| hero moves / turns | HERO current tile/direction? (verify where stored) |
| hero stats/HP/EP change | HERO `0x11c` cells |
| hero item add/remove/equip | HERO containers 0/1/2 |
| person moves/dies/spawns | `BT_ENTS` type-2 body, `BT_PNAME` |
| NPC loot/restock/specials | type-2 c0/c1/c2, `BT_RGINF` |
| item transfer/trade | two type-2 container sets |
| script global changes | `BT_VARS` (done, c2) |
| quest/diary update | `BT_DIARY` (blocked b4c) |
| event log update | `BT_HSTRY` (blocked b4d) |
| level change | tail entry for the new level, `BT_INFO` |

Any scenario without a matrix verdict is a decomposition item.

## 6. Deliverables

- `docs/research/p2-15c3a-write-back.md` — verdict, matrix, baseline
  table, scenario table, decomposition, non-claims.
- This design and `docs/plans/2026-11-17-p2-15c3a-write-back-research.md`.
- Updated `docs/STATUS.md`, `docs/PROJECT.md`, `docs/DECISIONS.md`
  (one decision for the patch/inverse split).
- Proposed follow-up tasks with scope, dependencies and acceptance
  oracles (working names: `c3b` HERO, `c3c` selected tail, `c3d`
  DIARY/HSTRY, `c3e` multi-level tails, `c3f` live integration oracle).

## 7. Acceptance criteria

1. Every matrix row has a verdict and evidence (writer RVA/lines, offset
   or corpus count), or an explicit `blocked` with the exact next check.
2. Baseline experiment covers 28/28 saves and reports: semantic equality
   counts, header diff counts, compressed-length deltas, first-diff
   offsets.
3. Every live scenario from §5.4 is mapped to at least one verdict.
4. Decomposition tasks each have scope, dependencies, acceptance oracle,
   and non-claims; no TBD/TODO left in the research doc.
5. `src/`/`tests/`/`tools/` unchanged; `git diff --check` clean;
   docs UTF-8 without BOM/NUL; scripts and their SHA-256 recorded.
6. Independent reviewer (spec + quality) passes; Critical/Important fixed.

## 8. Risks and limitations

- Static evidence may not resolve runtime-variable fields (`+0xff0`,
  `0x658`, HERO `0x600`); they will be `blocked` with a live-capture next
  check, not guessed.
- zlib byte equality with the original writer is not required; only
  semantic (decode) equality and layout conformance are claims.
- RNG/restock parity is already a non-claim (D-064) and stays out of
  write-back claims.
- Some live state may have no writer field at all (e.g. port-only
  simulation timers); such state is listed as a non-claim, not patched
  into raw bytes.

## 9. Open questions (to close in the research)

- OQ-C3-1: where does the hero current tile/direction live in `BT_HERO`
  (`0x000` vs `0x11c` vs containers), and is it written from live state?
- OQ-C3-2: are type-2 `+0x40/+0x44/+0x654/+0x658/+0xff0/+0x6cc` stable
  per person after live play, i.e. can they be preserved raw while other
  fields change?
- OQ-C3-3: does `FUN_14032c2c` regenerate the tail dictionary for all
  visited levels or only the current one?
- OQ-C3-4: is the attached `BT_HERO` list ever non-empty outside slot16's
  `extra` record, and is it required for campaign saves?
- OQ-C3-5: does the writer serialize container presence flags (0/1) from
  live state or from a recomputed invariant?
