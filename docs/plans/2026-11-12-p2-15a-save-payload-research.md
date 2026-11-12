# P2-15a — Save payload research Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Разобрать payload `.sav` (7 блоков), reader/writer `Server.dll`, state domains и выдать декомпозицию `P2-15b`/`P2-15c`.

**Architecture:** Research-only: Python-скан 28 save + Ghidra/decompiled.c mapping writer `FUN_14031b64` и reader `FUN_1403210c`; один документ `docs/research/p2-15a-save-payload.md`; production-код не меняется.

**Tech Stack:** Python 3 (`zlib`, `struct`), Ghidra headless (уже разобранный `Server.dll`), `.local/decomp/server/decompiled.c`.

**Status (2026-11-12):** research complete (verdict `implementable`); the step checkboxes below are historical and left unchecked. Task 6 concluded that a live diff was not needed; no step requiring a live run was executed.

## Global Constraints

- Research-only: никаких изменений `src/`, `tests/`, `tools/` и production-поведения.
- Скрипты и live-артефакты — только `%TEMP%`, не коммитить.
- Факты — с файлом, SHA-256, RVA + decompiled.c line, инструментом и наблюдением.
- Сетки/таблицы — полное совпадение на 28/28 save, не «на глаз».
- Не писать «похоже/вероятно» без `unverified` и плана проверки.
- Документы UTF-8 без BOM, NUL=0, без литеральных `\n`; markdown — только через edit/write.
- Коммиты — после approve; docs-only.
- Reference SHA: `Server.dll` `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`; `decompiled.c` `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA`; `slot0.sav` `8AC545FE9715EF11C68B1CCA8617BEB03003ECA360F07F71C26E3FC5E5C73442`; 28-save manifest `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B`.

---

### Task 0: STATUS entry

**Files:**
- Modify: `docs/STATUS.md`

**Interfaces:**
- Consumes: nothing.
- Produces: Phase P2-15a entry (`in progress`) with criteria/commands.

- [ ] **Step 1: Add section** after `Обновлено: 2026-11-12`, before the Stage 4 section:

```markdown
## P2-15a — Save payload research (in progress 2026-11-12)

- **Задача:** research-only разбор `.sav` payload после 112 байт: zlib,
  каталог блоков, reader/writer `Server.dll`, state domains, декомпозиция
  P2-15b/P2-15c.
- **Spec:** `docs/specs/2026-11-12-p2-15a-save-payload-research-design.md`.
- **Plan:** `docs/plans/2026-11-12-p2-15a-save-payload-research.md`.
- **Критерии:** таблица 7 блоков 28/28; reader/writer RVA+lines для каждого;
  `BT_SCRN` layout минимум; декомпозиция с acceptance; docs-only.
- **Команды:** Python scan 28 save; Ghidra/decompiled.c refs; encoding check.
- **Статус:** `in progress`.
```

- [ ] **Step 2: Encoding check**

```powershell
$b=[System.IO.File]::ReadAllBytes("docs\STATUS.md"); "nul=$((@($b|Where-Object {$_ -eq 0})).Count) bom=$([BitConverter]::ToString($b[0..2]))"
```

Expected: `nul=0`, no `EF-BB-BF`.

- [ ] **Step 3: Commit (after approve)** — `docs(status): start save payload research`.

---

### Task 1: Container scan (28/28)

**Files:**
- Create (not committed): `%TEMP%\p2_15a_scan.py`
- Create: `docs/research/p2-15a-save-payload.md` (container section)

**Interfaces:**
- Consumes: `Saves/slot*.sav`.
- Produces: table `tag/offset/size/end/order` for all 28; contiguity check.

- [ ] **Step 1: Write scan script**

```python
import glob, os, struct, zlib
root = r"I:\Games\Златогорье-2\Saves"
files = sorted(glob.glob(os.path.join(root, "slot*.sav")),
               key=lambda p: int(os.path.basename(p)[4:-4]))
print("files", len(files))
for path in files:
    raw = open(path, "rb").read()
    body = zlib.decompress(raw[112:])
    blocks = []
    i = 0
    while i + 12 <= len(body):
        if body[i:i+3] == b"BT_":
            tag = body[i:i+8].decode("ascii", "replace")
            size = struct.unpack_from("<I", body, i + 8)[0]
            blocks.append((tag, i, size))
            i += 12 + size
        else:
            print("UNALIGNED", os.path.basename(path), hex(i)); break
    ok = i == len(body)
    print(os.path.basename(path), "raw", len(raw), "dec", len(body),
          "blocks", len(blocks), "end", ok)
    for tag, off, size in blocks:
        print("   ", tag, hex(off), size, "end", hex(off + 12 + size))
```

- [ ] **Step 2: Run and record**

```powershell
py -3 "$env:TEMP\p2_15a_scan.py"
```

Expected (verified 2026-11-12): 28 files; each 7 blocks in order
`BT_SCRN | BT_INFO | BT_VARS | BT_TRIBE | BT_DIARY | BT_HSTRY | BT_HERO`;
`BT_SCRN` offset `0x0`, size `750008`; blocks contiguous; final
`BT_HERO` end == decompressed length; `UNALIGNED` absent.

- [ ] **Step 3: Record in research doc** §Container with the real 28/28
  numbers, zlib input range (`raw[112:]`), tag format (8 bytes, no NUL),
  u32 LE size, block body, order. Add slot0 table:
  `BT_SCRN 0x0 750008; BT_INFO 0xB71C4 100; BT_VARS 0xB7234 31035;
  BT_TRIBE 0xBEB7B 58884; BT_DIARY 0xCD18B 32960; BT_HSTRY 0xD5257 1592;
  BT_HERO 0xD589B 30388`.

- [ ] **Step 4: Verify no code changes**

```powershell
git status --short
```

Expected: only `docs/STATUS.md` modified; no `src/tests/tools`.

---

### Task 2: Writer mapping (`FUN_14031b64`)

**Files:**
- Modify: `docs/research/p2-15a-save-payload.md`
- Read: `.local/decomp/server/decompiled.c:35329-35522`
- Read: `.local/decomp/server/functions.csv` (RVAs)

**Interfaces:**
- Consumes: Task 1 block order.
- Produces: per-block writer function/RVA/call, header write sequence.

- [ ] **Step 1: Extract writer call list**

From decompiled.c:35384-35422 record:

| block | writer call | decompiled lines |
|---|---|---|
| `BT_SCRN ` | vtable `iVar3+0x50` with `(500,0x177)` | 35384-35385 |
| `BT_INFO ` | `FUN_1400116c`/`FUN_1400126c`/`FUN_1400127c`, `DAT_1407fcc8+4`, `DAT_14088e14+0x18`, two strings via `DAT_14088e30+8/+0x54`, `this+0x26c8` 8 bytes | 35386-35396 |
| `BT_VARS ` | `FUN_14002324(DAT_1407f9a0, ...)` | 35397-35398 |
| `BT_TRIBE` | `FUN_14016828(DAT_14089fc0, ...)` | 35399-35400 |
| `BT_DIARY` | `FUN_14058768(DAT_14088e04, ...)` | 35401-35402 |
| `BT_HSTRY` | vtable `iVar3+0x28` +4 | 35403-35404 |
| `BT_HERO ` | `FUN_1401e690` + `FUN_1401e340` | 35405, 35421-35422 |

- [ ] **Step 2: Resolve RVAs**

```powershell
Select-String -Path .local\decomp\server\functions.csv -Pattern "FUN_14002324|FUN_14016828|FUN_14058768|FUN_1401e690|FUN_1401e340|FUN_14031b64" |
    ForEach-Object { $_.Line }
```

Expected: each function with `rva,size,callers`; record RVA in the table.

- [ ] **Step 3: Record header sequence**

From decompiled.c:35423-35522: magic `Goldenland II save `, version
`local_188 = 0x11`, header copy `0x68`, slot path `Saves\slot%d.sav`,
directory creation. Write exact observed order.

---

### Task 3: Reader mapping (`FUN_1403210c`)

**Files:**
- Modify: `docs/research/p2-15a-save-payload.md`
- Read: `.local/decomp/server/decompiled.c:35523-35696`

**Interfaces:**
- Consumes: Task 1 block order; Task 2 writer table.
- Produces: reader order/asymmetry, field offsets, per-block reader calls.

- [ ] **Step 1: Extract reader call list**

From decompiled.c:35628-35686 record:

| read order | block | reader call | lines |
|---|---|---|---|
| 1 | `BT_INFO ` | `FUN_140037d0(DAT_1407fcc8)`, `+0x218`, `FUN_1400129c`, `+0x26c4`, `+0x26c8` 8 bytes, `FUN_140325fc` | 35628-35640 |
| 2 | `BT_VARS ` | `FUN_140023f0(DAT_1407f9a0)` | 35664-35665 |
| 3 | `BT_TRIBE` | `FUN_140168e4(DAT_14089fc0)` | 35666-35667 |
| 4 | `BT_DIARY` | `FUN_140588c8(DAT_14088e04)` | 35668-35669 |
| 5 | `BT_HERO ` | `FUN_1401e424` + extra persons loop `FUN_1405c83c`/`FUN_140368d4` | 35670-35684 |
| 6 | `BT_HSTRY` | vtable `iVar3+0x28` | 35685-35686 |

- [ ] **Step 2: Document asymmetry**

Writer order is `... BT_HSTRY, BT_HERO`; reader order is `... BT_HERO,
BT_HSTRY`. Tags+size make order independent; record as observed, no
inference beyond that.

- [ ] **Step 3: Record BT_SCRN reader**

Identify how `BT_SCRN ` is consumed in this reader (the 750008-byte block;
look at calls before `BT_INFO` and vtable `iVar3+0x18/+0x28`, plus xrefs
to the tag). If not visible in `FUN_1403210c`, record `unverified` and
the exact Ghidra follow-up (function/RVA) rather than guessing.

---

### Task 4: `BT_SCRN ` = preview; locate world dynamic state

**Files:**
- Modify: `docs/research/p2-15a-save-payload.md`
- Read: `.local/decomp/server/decompiled.c` writer/reader call paths

**Interfaces:**
- Consumes: Task 2/3 tables (Task 3 evidence: `BT_SCRN` = 8 + 500×375×4 preview, client consumer only).
- Produces: confirmed preview classification + what `P2-15b` must read for a level.

- [ ] **Step 1: Confirm preview, fix container hypothesis**

Cross-check `750008 = 8 + 500*375*4`; cite client consumer
`FUN_12088444`→`FUN_12022b98` (decompiled client) and the server writer
vtable (`iVar3+0x50`, args `500/0x177`) / no server reader. Update the
§Container domain column: `BT_SCRN` is a save preview/thumbnail, not world
data. Mark gameplay-scene use `unverified` only if evidence is missing.

- [ ] **Step 2: Locate dynamic world state**

Given static level data is reloaded from SEF, determine which blocks carry
dynamic world/person state: inspect `BT_TRIBE` reader/writer
(`FUN_140168e4`/`FUN_14016828`), `BT_VARS`, `BT_DIARY`, `BT_HSTRY`,
`BT_INFO`. Record evidence (RVA/lines) for persons/entities/triggers vs
static terrain; mark unresolved items `unverified` with exact next check.

- [ ] **Step 3: Define minimum `P2-15b` read set**

List ordered blocks/fields needed to restore a level from a save (e.g.
`BT_INFO` level id/time, `BT_TRIBE` persons, `BT_VARS` globals, `BT_HERO`
hero), and what can be deferred.

---

### Task 5: Remaining block domains + port coverage

**Files:**
- Modify: `docs/research/p2-15a-save-payload.md`
- Read: `docs/research/p3b2b2-npc-loot-server.md` §15, `docs/research/p5-save-equipment.md`, `src/script` globals

**Interfaces:**
- Consumes: Task 1–4.
- Produces: domain table with reader/writer RVA, fields, existing port coverage.

- [ ] **Step 1: `BT_VARS`** — reader `FUN_140023f0` / writer `FUN_14002324`
  (`DAT_1407f9a0`); determine name/value/type layout and relation to
  `gl::script` globals. Record RVA/decompiled lines.
- [ ] **Step 2: `BT_TRIBE`** — `FUN_140168e4`/`FUN_14016828` (`DAT_14089fc0`);
  record main fields and whether persons reference tribe ids.
- [ ] **Step 3: `BT_DIARY`** — `FUN_140588c8`/`FUN_14058768` (`DAT_14088e04`);
  quest/journal structure.
- [ ] **Step 4: `BT_HSTRY`** — vtable `iVar3+0x28`; record semantics.
- [ ] **Step 5: `BT_INFO`/`BT_HERO` coverage** — cite existing:
  `BT_INFO+0x26c4` (1b2b2b), `BT_HERO` containers/equipment (P5a), VM globals.

---

### Task 6: Live diff (only if needed) + Open Questions

**Files:**
- Modify: `docs/research/p2-15a-save-payload.md`

**Interfaces:**
- Consumes: previous tasks.
- Produces: resolved unknowns or explicit `unverified` + plan.

- [ ] **Step 1: Decide necessity**

If `BT_SCRN`/`BT_VARS` layout is ambiguous from static evidence, plan a
read-only live diff: load slot0 in the original, read the corresponding
memory region, compare to the decoded block. User-assisted; no writes.
If not needed or unavailable, skip to Step 3.

- [ ] **Step 2: Live diff procedure (only if triggered)**

Record: save slot, level, process/module, module base, memory region,
length, comparison result (0/N differences), tool (`gl_live_capture` style
or external read-only). Do not use WriteProcessMemory.

- [ ] **Step 3: Open Questions**

List every unresolved item with: question, what was checked, RVA/offset,
next exact check. No TBD.

---

### Task 7: Research doc finalize + STATUS/parity

**Files:**
- Modify: `docs/research/p2-15a-save-payload.md`
- Modify: `docs/STATUS.md`
- Modify: `docs/research/parity-matrix.md` (P2-15 row)

**Interfaces:**
- Consumes: Task 1–6.
- Produces: verdict + decomposition `P2-15b`/`P2-15c`.

- [ ] **Step 1: Verdict/decomposition section**

State one of: `implementable`/`scoped`/`blocked`, with:
- `P2-15b` read-only load: minimal blocks/fields, acceptance (loaded level
  + persons/quests/globals match save 0/N; deterministic),
- `P2-15c` write/`game_save`: block order, writer reuse, round-trip
  acceptance (write→read→compare),
- risks/unknowns and what blocks each.

- [ ] **Step 2: STATUS update**

Mark P2-15a done; record what was proven (28/28 blocks, reader/writer,
domains), residual unknowns, and next subproject recommendation.

- [ ] **Step 3: Parity matrix**

Update P2-15 row: from `unverified/absent` to research verdict; keep
write/load `absent` until P2-15b/c; add references.

- [ ] **Step 4: Encoding check**

```powershell
foreach($p in @("docs\research\p2-15a-save-payload.md","docs\STATUS.md","docs\research\parity-matrix.md")){
  $b=[System.IO.File]::ReadAllBytes($p); "$p nul=$((@($b|Where-Object {$_ -eq 0})).Count) bom=$([BitConverter]::ToString($b[0..2]))"
}
```

Expected: `nul=0`, no BOM.

- [ ] **Step 5: Commit (after approve)** — `docs: research save payload format`.

---

## Self-review notes

- Spec §2–§5 покрыты Task 1–5; §6 acceptance — Task 1/7; §8 risks — Task 6/7.
- Все наблюдения привязаны к decompiled.c line/RVA или Python-скану; unknowns
  явно помечаются.
- Production-код не трогается ни в одной задаче; коммиты docs-only.
- `BT_SCRN` reader может быть вне `FUN_1403210c` — Task 3 Step 3 требует
  зафиксировать `unverified` + точный Ghidra follow-up, а не догадку.
