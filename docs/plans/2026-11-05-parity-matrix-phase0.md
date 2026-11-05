# Parity Matrix & Decomp Reference — Phase 0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Зафиксировать текущий паритет в `docs/research/parity-matrix.md` и подготовить локальный декомпил-корпус `Server.dll`, чтобы дальнейшая реконструкция шла по матрице, а не по памяти чата.

**Architecture:** Документы + скрипт Ghidra. Код порта не меняется. Корпус — локальный (`/.local/`), в git только индекс и скрипт.

**Tech Stack:** Markdown (UTF-8 без BOM), Ghidra 12.1.4 headless, JDK 21, PowerShell 5.1.

## Global Constraints

- Результат проекта — современный C++20/SDL2; matching/VC6 не используется.
- `Server.dll` SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Матрица — единственный источник истины о покрытии; строка без evidence не считается.
- Корпус `.local/decomp/` не коммитится; `docs/research/decomp-index.md` — коммитится.
- Документы писать через `write`/`edit`, не через PowerShell here-string.
- После правок проверить: BOM отсутствует, `nul=0`, литеральных `\n` нет.
- Коммит — только по явной команде пользователя.

---

### Task 1: Seed parity matrix

**Files:**
- Create `docs/research/parity-matrix.md`

**Interfaces:**
- Canonical columns: `ID | Подсистема | Пункт | Реализация | Evidence | Статус | Пробел/задача | Источник`.
- Status vocabulary: `verified | partial | missing | contradicted | superseded`.

- [x] **Step 1:** Перенести строки P0/P1 из `parity-audit-p0-p1.md`, P2/P3 из
  `parity-audit-p2-p3.md`, A/B/C из `parity-audit-p4.md`, сохранив evidence и
  формулировки пробелов.
- [x] **Step 2:** Добавить блоки, появившиеся после аудита: P4d, A8 parity,
  P5a, P5b1 — по `STATUS.md`, specs и research; статус — по фактическим
  доказательствам (golden-хеши, hook-трассы, manual gates, real-save counts).
- [x] **Step 3:** Пометить отменённые строки (`B15`, `B2/B3` частично,
  `C16/C17` и т.п.) как `superseded` со ссылкой на D-030/D-031/D-032/D-035.
- [x] **Step 4:** Добавить сводку counts по статусам и топ приоритетных
  пробелов (B1/B2/B3/B4, C1, PAD crops/anchors, 6-осевой CanInteract,
  331 runtime-клетка, бой/магия/мир).

### Task 2: Ghidra export script

**Files:**
- Create `tools/ghidra/ExportDecomp.java`

**Interfaces:**
- Args: `<output_dir>`.
- Produces `functions.csv` (`rva,size,name,thunk,callers`) and
  `decompiled.c` (header + C per function).

- [x] **Step 1:** Написать скрипт на Ghidra API: обойти
  `currentProgram.getFunctionManager().getFunctions(true)`, для каждой функции
  вызвать `DecompInterface.decompileFunction`, писать в оба файла.
- [x] **Step 2:** Не падать на thunk/неразобранных функциях: пропускать с
  комментарием в `decompiled.c`, сохраняя строку в `functions.csv`.

### Task 3: Export Server.dll corpus

**Files:**
- Generated: `.local/decomp/server/{functions.csv,decompiled.c,meta.txt}`
- Create `docs/research/decomp-index.md`

- [x] **Step 1:** `analyzeHeadless` импорт `Server.dll` в
  `%TEMP%\ghidra_proj` с `-overwrite`, затем повторный запуск `-process`
  с `-postScript ExportDecomp.java`.
- [x] **Step 2:** Проверить: `functions.csv` непуст, `decompiled.c` содержит
  тела функций (не только заголовки), `meta.txt` содержит SHA-256.
- [x] **Step 3:** Записать в `decomp-index.md` число функций, дату, SHA-256,
  путь и точную команду воспроизведения.

### Task 4: Docs and status

**Files:**
- Modify `docs/STATUS.md`
- Modify `docs/DECISIONS.md`

- [x] **Step 1:** Добавить D-046: результат — современный C++20; matching/VC6
  не используется; паритет — поведенческие оракулы + матрица; корпус локальный.
- [x] **Step 2:** Добавить в `STATUS.md` секцию Phase 0: задача, критерии,
  команды проверки, результат.
- [x] **Step 3:** Обновить дату `STATUS.md` на 2026-11-05.

### Task 5: Verification

- [x] **Step 1:** Проверить кодировку всех изменённых документов:
  нет BOM, `nul=0`, нет литеральных `\n`.
- [x] **Step 2:** Проверить, что `git status` содержит только документы и
  скрипт; `.local/decomp/` не попал в индекс.
- [x] **Step 3:** Зафиксировать в `STATUS.md` фактический вывод проверок.
