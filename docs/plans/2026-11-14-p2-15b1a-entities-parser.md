# P2-15b1a — Save entities parser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Разобрать `BT_ENTS`/`BT_PNAME`/`BT_RGINF` всех tail-секций в `SavePayload` с точным потреблением и тестами.

**Architecture:** `assets` расширяет `SaveTailEntry`/`SavePayload`; парсер tail-секции читает RGINF, PNAME, ENT список по формулам research; `glgame --dump-save` печатает counts.

**Tech Stack:** C++20, CMake/Ninja, doctest.

## Global Constraints

- Только `assets` + `--dump-save`/E2E; restore persons не входит.
- Body formulas строго из `docs/research/p2-15b1-save-entities.md`; никаких догадок.
- Unknown type/truncation/missing terminator → `ErrorCode::format`.
- PNAME==type2 — test invariant, не parser failure.
- Docs UTF-8 без BOM/NUL; код без лишних комментариев.
- Коммиты после approve; full ctest с `GL_GAME_DIR` в конце.

Toolchain:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

---

### Task 0: STATUS entry

**Files:** `docs/STATUS.md`

- [x] **Step 1:** Insert after `Обновлено:`:

```markdown
## P2-15b1a — Save entities parser (in progress 2026-11-14)

- **Задача:** parse BT_ENTS (types/bodies), BT_PNAME, BT_RGINF во всех
  tail-секциях; `--dump-save` counts; без restore.
- **Spec:** `docs/specs/2026-11-14-p2-15b1a-entities-parser-design.md`.
- **Plan:** `docs/plans/2026-11-14-p2-15b1a-entities-parser.md`.
- **Критерии:** 2599/2599 exact; histogram; PNAME==type2; malformed errors.
- **Статус:** `in progress`.
```

- [x] **Step 2:** encoding check; commit `docs(status): start save entities parser`.

---

### Task 1: Structs + tail parser + unit tests

**Files:**
- Modify: `src/assets/include/gl/assets/save_payload.hpp`
- Modify: `src/assets/src/save_payload.cpp`
- Test: `tests/assets/test_save_payload.cpp`

**Interfaces:**
- `SaveEntity { u32 type; vector<u8> body; }`
- `SaveRginf { u32 latch; array<u32,8> datetime; }`
- `SaveTailEntry { name; raw_len; section; rginf; person_names; entities; }`

- [x] **Step 1: Tests first**

Extend the tail builder:
- valid tail section = RGINF(36) + PNAME(two names + zero terminator) +
  ENT (type 8 body 12 bytes) + type 1 + padding per section rules.
- assert `rginf.latch`, datetime; names; entities `{type=8, body.size()=12}`.
- add minimal type 0x20 (28 B), 0x10 (32 B + zero M=4), type 2 (552 B with
  three zero M flags) fixtures.
- malformed: unknown type 9, truncated body, no terminator, RGINF 35,
  PNAME unterminated → `!has_value()` + format.

- [x] **Step 2: RED**.

- [x] **Step 3: Implement**

Parser helpers:
```cpp
void parse_rginf(std::span<const u8> body, SaveRginf &out);
void parse_pnames(std::span<const u8> body, std::vector<std::string> &out);
void parse_entities(std::span<const u8> body, std::vector<SaveEntity> &out);
```
- RGINF: require 36; latch + 8×u32.
- PNAME: loop `u32 len`; len==0 → end; read len bytes; cp1251→UTF-8.
- ENT: loop `u32 type`; type==1 → end; size by formula:
  - type 8: `8 + M`
  - type 0x10: `32 + n1+n2+n3+n4 + M`
  - type 0x20: `28 + n1+n2+n3`
  - type 2: `540 + n1..n4 + M1..M3 + 0x14*c`
  where `M` = 4 if flag==0 else `8 + sum(0xc0+4+0x14*m_i)`;
  strings `u32 n + min(n,0x104)`.
  Read body bytes; require exact consumption after type 1; else format.
- In tail decompression step, parse selected/each entry into `rginf`,
  `person_names`, `entities`; keep raw `section`.
- Update existing synthetic tail fixture to valid bodies.

- [x] **Step 4: GREEN** (`*save payload*` + full `gl_assets_tests`); commit
  `feat(assets): parse save entities`.

---

### Task 2: Real 2599-section tests

**Files:**
- Modify: `tests/assets/test_save_payload_real.cpp` (or new file)
- Modify: `tests/assets/CMakeLists.txt` if new

- [x] **Step 1:** iterate all 28 saves and every `tail` entry; parse already
  done by decoder. Assert:
  - `entities` non-empty; last type 1 not stored; exact consumption is
    guaranteed by parser (no raw length needed here).
  - histogram `{2:14507, 8:354, 16:12090, 32:2591}` across all sections.
  - PNAME count == type-2 count per section: 2599/2599.
  - RGINF datetime sanity (month 1..12, day 1..31 etc.) where latch/fields
    expected; slot0/slot2 selected counts pinned from independent scan.
- [x] **Step 2:** GREEN with `GL_GAME_DIR`; skip without; commit
  `test(assets): cover save entities corpus`.

---

### Task 3: `--dump-save` counts + E2E

**Files:**
- Modify: `tools/glgame/main.cpp`
- Test: `tests/app/test_glgame_screenshot.cpp`

- [x] **Step 1:** extend dump line for selected tail:
  `... entities=<n> pnames=<n> rginf_latch=<u32>`
  (slot0 values from reference scan).
- [x] **Step 2:** update existing `--dump-save` E2E expected line/values;
  ensure parse failures still exit 2.
- [x] **Step 3:** GREEN; commit `feat(tools): dump save entity counts`.

---

### Task 4: Full ctest + docs

**Files:** `docs/DECISIONS.md`, `docs/STATUS.md`, `docs/PROJECT.md`,
`docs/research/parity-matrix.md`, spec/plan.

- [x] **Step 1:** full `ctest` with `GL_GAME_DIR`; record N/time.
- [x] **Step 2:** D-070 (parser behavior, formulas, PNAME invariant,
  type-4 reject); STATUS done; PROJECT P2; parity P2-15/OQ-12 note;
  research implementation result; spec status.
- [x] **Step 3:** encoding; commit docs incl. spec/plan
  `docs: record save entities parser`.

---

## Self-review notes

- Spec §3/§4 → Tasks 1–3; acceptance → Task 4.
- Formulas copied from research, not guessed; type-4 rejected by map.
- No restore/b1b in this plan.
