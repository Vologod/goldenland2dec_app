# P5a Save → Outfit — Research Gate Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Извлечь точный layout equipped-записей в `BT_HERO` и item→tech/group, чтобы P5a можно было реализовать без догадок.

**Architecture:** Ghidra (GoldenLand.exe writer/reader) + разбор реальных `.sav`; результат — `docs/research/p5-save-equipment.md` с offset/RVA/таблицами. Implementation plan создаётся после 0/N проверки на двух слотах.

**Tech Stack:** Ghidra headless, Python zlib/struct, существующие `glpak`/`SaveManager`; без изменений игрового кода.

## Global Constraints

- Оригинал — источник истины: GoldenLand.exe SHA-256
  `7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF`.
- Ассеты игры не коммитить; в research — только offsets/ID/наблюдения.
- Документы UTF-8 без BOM; правки через `edit`/`write`.
- Коммит только research-документа и тестовых фикстур без game data.
- Acceptance: slot0 IDs `318/397/280` и второй слот; 0/N расхождений
  по equipped-записям, иначе — BLOCKED.

---

### Task 1: Locate BT_HERO equipped records in GoldenLand.exe

**Files:**
- Create: `docs/research/p5-save-equipment.md`
- Read: `docs/research/p4b2-render-order.md:440-500`,
  `docs/research/p4d-hero-wear.md:254-305`

**Interfaces:**
- Produces: таблица `BT_HERO` layout: absolute file/zlib offset, record
  size, field offsets для `item_id/type/subcount`, начало/конец
  equipped-списка; список RVA writer/reader.

- [ ] **Step 1: Re-import GoldenLand.exe in Ghidra with real base**

Run:

```powershell
$env:JAVA_HOME="$PWD\.local\toolchains\jdk-21.0.12.1+1"
$proj="$env:TEMP\ghidra_glexe"
& .local\toolchains\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat `
  $proj glexe -import "I:\Games\Златогорье-2\GoldenLand.exe" -overwrite
```

Expected: `.text 00401000..00440fff`, image base `00400000`.

- [ ] **Step 2: Find save writer by the save magic**

Run a Ghidra script that searches references to the string
`"Goldenland II save"` at file offset `0x50308` and prints containing
functions. Record function VAs and caller chain in the research doc.

Expected: writer/caller addresses, one of which writes `BT_HERO`.

- [ ] **Step 3: Decompile the hero save writer**

Decompile the function(s) that write `BT_HERO` using `DecompFuncs.java`.
Record:

- order of fields: `8+4+0x11c+12+0x600+12+8`, 4×32 bytes, `count×20`;
- item record layout: `0xC0` raw item + `u32 subcount` + `12*subcount`;
- where equipped list starts and how slots are mapped.

Expected: exact byte offsets and counts for slot0.

- [ ] **Step 4: Verify against slot0 decompressed body**

Run:

```powershell
py -3 -c "import zlib,pathlib; b=pathlib.Path(r'I:\Games\Златогорье-2\Saves\slot0.sav').read_bytes(); d=zlib.decompress(b[112:]); print(len(d)); print(d.find(b'BT_HERO'))"
```

Expected: `BT_HERO` block found (slot0 currently at decompressed offset
`874651`; size `30388`); IDs `318/397/280` at block-relative offsets
around `0x6d14/0x6e00/0x6ed8`.

- [ ] **Step 5: Verify a second slot**

Repeat Step 4 for one other valid slot (title not `Quick save`).
Record its equipped IDs and confirm the same record layout. If layout
differs — record BLOCKED and stop.

- [ ] **Step 6: Write research doc**

Create `docs/research/p5-save-equipment.md` with:

- SHA-256, Ghidra function VAs, base;
- zlib offset of `BT_HERO`, block size;
- record layout table (offset/size/type/meaning);
- slot0 and second-slot equipped IDs;
- 0/N verification table;
- open item→tech mapping (ITM `unknown_b` + `herowear.scr`) and exact
  fields needed.

- [ ] **Step 7: Commit**

```powershell
git add docs/research/p5-save-equipment.md
git commit -m "docs(p5): record BT_HERO equipped layout"
```

Expected: commit created; implementation plan is written only after
0/N verification on both slots.

---

## Next phase

After Task 1 passes, create `docs/plans/2026-11-04-p5a-save-outfit-implementation.md`
with concrete parser/menu/render tasks from the verified layout and the
approved spec (`docs/specs/2026-11-04-p5-save-outfit-design.md`).
