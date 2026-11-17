# P2-15c3a — Save write-back research (plan)

Spec: `docs/specs/2026-11-17-p2-15c3a-write-back-research-design.md`
Status: approved (2026-11-17)
Type: research-only (docs); `src/`/`tests/`/`tools/` unchanged.

## Preconditions

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
Test-Path "$env:GL_GAME_DIR\Saves"        # 28 slot*.sav
Test-Path ".local\decomp\server\decompiled.c"
Get-FileHash -Algorithm SHA256 ".local\decomp\server\decompiled.c"
```

Scripts live in `.local/research/c3a/` (not committed); their SHA-256 go
into the research doc. Read-only over the game directory.

## Task 1 — Static writer/reader contract

1. Verify the block order and anchors from §5.1 of the spec against
   `decompiled.c` (writer 35384–35522, reader 35604–35686).
2. For each domain record: writer function, line range, memory source
   object/offset, variable-length parts, reader counterpart.
3. Extract, from decompiled code, the HERO body writer sequence:
   `0x000`, `0x11c`, `0x128`, `0x600`, containers `hero+0x0C/+0x10/+0x109C`,
   post-container `2×u32`, `0x80` block (`FUN_1401d84c`), `extra_count`
   records (`0x14*count`), `list_count` + item records (`FUN_1401e340`).
   Map each to a live port source if one exists; otherwise mark missing.
4. Extract the tail snapshot generator `FUN_14032c2c`: `BT_PNAME` filter,
   `BT_RGINF`, `BT_ENTS` filter, ordering, per-level dictionary scope.
5. For type-2 opaque fields `+0x40/+0x44/+0x654/+0x658[8]/+0xff0[0x11c]/
   +0x6cc` and HERO `0x600`/`0x11c`: record writer/reader sites and
   consumers; if static evidence is insufficient, mark `blocked` with the
   exact next check. Use Ghidra only for sites not already in
   `decompiled.c`, with toolchain from `AGENTS.md`.
6. Evidence format: table rows with RVA/line, function, observation.

Acceptance: every block has writer + reader anchor and a recorded memory
source; blocked fields have a named next check.

## Task 2 — Baseline re-encode experiment (28 saves)

1. Write `.local/research/c3a/reencode.py`:
   - parse header (112 B), `raw_len` @ `0x68`, `comp_len` @ `0x6c`, zlib
     @ `0x70`;
   - parse the 7 expected blocks (`BT_SCRN `, `BT_INFO `, `BT_VARS `,
     `BT_TRIBE`, `BT_DIARY`, `BT_HSTRY`, `BT_HERO `);
   - parse the tail dictionary (`u32 count`, name, `u32 len`, section
     `u32 raw_len + u32 zlib_len + zlib`);
   - rebuild with the current policy: header bytes unchanged, main stream
     `tag(8)+u32 size+body`, `zlib.compress` (default level), tail
     dictionary re-compressed from raw sections;
   - compare: decompressed main-stream byte equality, tail section raw
     equality, tail name/order equality, header diff count, compressed
     lengths, first byte-diff offset.
2. Run on all 28 slots; write `out/baseline.tsv` and a summary.
3. Repeat with `BT_VARS`/`BT_TRIBE` bodies replaced by the c2 encoders
   (equality with the parsed body must hold 28/28).
4. Record the script SHA-256 and command in the research doc.

Acceptance: table 28/28 with the columns from the spec; no writes outside
`.local/research/c3a/out/`.

## Task 3 — Write-back matrix

1. Build one row per domain (header, `BT_INFO `, `BT_VARS`, `BT_TRIBE`,
   `BT_DIARY`, `BT_HSTRY`, HERO header/`0x11c`/`0x600`/3 containers/
   post-container/`0x80`/extra/attached, tail dictionary, `BT_RGINF`,
   `BT_PNAME`, `BT_ENTS` 2/8/16/32, `BT_SCRN `).
2. Apply the verdict rules from the spec §5.3. Each row needs evidence:
   writer site, port state status, corpus count, and an acceptance oracle
   for the follow-up.
3. Cross-check each verdict against the baseline experiment (Task 2).

Acceptance: every row has verdict + evidence or `blocked` + next check.

## Task 4 — Scenario cross-check

1. Walk the port runtime and list every state mutation that can reach a
   save domain: `LevelSimulation` (position/direction/route/HP/EP),
   `InventoryRuntime` (add/remove/transfer/trade, loot), `ScriptVm`
   (globals), quest/history placeholders, level transitions.
2. Map each scenario to matrix rows; mark uncovered scenarios.
3. For each uncovered scenario produce a decomposition item with scope,
   dependencies, acceptance oracle, non-claims.

Acceptance: scenario table has no unmapped live mutation.

## Task 5 — Verdict, decomposition, docs

1. Write `docs/research/p2-15c3a-write-back.md`:
   provenance, method, static contract, baseline table, matrix, scenario
   table, verdict, decomposition, unknowns/non-claims, commands.
2. Decomposition tasks (final names from evidence): HERO write-back,
   selected-tail rebuild, DIARY/HSTRY, multi-level tails, live→save
   integration oracle. Each: scope, deps, oracle.
3. Update `docs/STATUS.md` (task record, next task), `docs/PROJECT.md`
   (P2 row), `docs/DECISIONS.md` (one decision for the split).
4. Docs UTF-8 no BOM/NUL; `git diff --check`; commit docs (after user
   approval where required by AGENTS).

Acceptance: research doc complete, no TBD/TODO; docs updated.

## Task 6 — Review and verification

1. Independent reviewer: spec conformance + quality (evidence, verdict
   rules, decomposition).
2. Fix Critical/Important, re-review.
3. Final checks:
   ```powershell
   git status --short          # only docs/.local ignored
   git diff --check
   # docs encoding spot check: no BOM/NUL
   ```
4. Record the reviewer outcome in STATUS; Minors to ledger.

## Verification commands

```powershell
python .local\research\c3a\reencode.py --saves "I:\Games\Златогорье-2\Saves" --out .local\research\c3a\out
Get-FileHash -Algorithm SHA256 .local\research\c3a\reencode.py
git -C I:\goldenland2dec diff --check
```
