# Phase 1b2b2e — Live parity part 2 (merge + RNG phase + price CW) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Устранить остаточный `l1_1` live-mismatch: реализовать merge/stacking по `FUN_1404e3a4`, измерить per-call RNG phase и захватить live price CW.

**Architecture:** (1) `NpcLootGenerator` получает per-container merge с overwrite-count семантикой; (2) live tool считает `_rand` pre-draw hits между `FUN_1404b1c0` и пишет return-address каждого draw; (3) отдельный `--price-cw` режим снимает CW у `Client.dll!FUN_1206E8D0`; (4) по измеренной phase пишется Part 3 plan для источника лишних draw.

**Tech Stack:** C++20, CMake/Ninja, doctest, i686 llvm-mingw для tool.

## Global Constraints

- Коммиты — после явного подтверждения; live-артефакты не коммитить.
- Item flags — raw ITM flags; inherited effects не пересматривать.
- Merge семантика — только по `FUN_1404e3a4` + caller asm (overwrite, не sum).
- Не подгонять phase фиксированным offset; только измеренные draw counts.
- Тесты не ослаблять; при изменении deterministic oracle — обосновать merge-правилом.
- Документы UTF-8 без BOM, `nul=0`.
- Тулчейн:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

## Scope

Part 2: merge/stacking, per-call draw measurement, price CW. Источник переменной
RNG phase — research/Part 3 после live trace. Full 0/N заявляется только после
Part 3.

---

### Task 1: Merge/stacking по `FUN_1404e3a4`

**Files:**
- Modify: `src/game/src/npc_loot.cpp`
- Modify: `src/game/include/gl/game/npc_loot.hpp` (если нужен helper)
- Test: `tests/game/test_npc_loot.cpp`
- Test/regression: `tests/game/test_npc_loot_specials.cpp`,
  `tests/game/test_inventory_runtime_real.cpp` (если oracle меняется)

**Interfaces:**
- Consumes: `LootItem {item_id,count,type,flags,style,mod_count,mods}`,
  `ItemTemplate {max_stack, effects,...}`.
- Produces: `append_chunks` добавляет чанк в контейнер по правилу:
  merge только если same item id, `max_stack >= 2`, `record.count < max_stack`;
  при merge `record.count = chunk` (overwrite), флаги/mods остаются от первой
  записи; иначе новая запись `count=chunk`.

- [ ] **Step 1: Написать падающий unit-тест merge**

В `tests/game/test_npc_loot.cpp` добавить:

```cpp
TEST_CASE("npc_loot: same-prototype chunks merge with overwrite count") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 0.0;
    table.chance_decrease = 1.0;
    table.quantity_decrease = 1.0;
    table.items = {make_item("A", 0, 20, 100, 2, 2),
                   make_item("A", 0, 20, 100, 3, 3)};

    MsvcRandom rng(1);
    const NpcLoot loot = NpcLootGenerator::generate(table, 11, names, lookup, rng);
    REQUIRE(loot.main.size() == 1);
    CHECK(loot.main[0].item_id == 100);
    CHECK(loot.main[0].count == 3); // overwrite, not 2+3
}
```

- [ ] **Step 2: RED**

Run:

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*same-prototype chunks merge*"
```

Expected: FAIL — две записи (2 и 3).

- [ ] **Step 3: Реализовать merge в `append_chunks`**

В `src/game/src/npc_loot.cpp` заменить создание/добавление `LootItem` на:

```cpp
auto add_chunk = [&](std::int32_t chunk) -> LootItem & {
    if (item_template.max_stack >= 2) {
        for (LootItem &existing : out) {
            if (existing.item_id == item_id &&
                existing.count < item_template.max_stack) {
                existing.count = chunk;
                return existing;
            }
        }
    }
    LootItem loot{item_id, chunk, item_template.type, flags, item_template.style};
    loot.mod_count = item_template.mod_count;
    loot.mods.reserve(item_template.effects.size());
    for (const ItemTemplateEffect &effect : item_template.effects)
        loot.mods.push_back({effect.type, effect.value, effect.flags});
    out.push_back(std::move(loot));
    return out.back();
};
```

Затем в цикле чанков: `LootItem &loot = add_chunk(chunk);`, и спец-ролл
применять к `loot` (условие `chunk==1 && (flags&4) && item_template.mod_count==0`),
`loot.flags |= 0x200`, `loot.mod_count += loot.mods.size()`.
НЕ суммировать count и НЕ добавлять новую запись при merge.

- [ ] **Step 4: GREEN + edge-тесты**

Добавить тесты:
- full stack: `max_stack=2`, qty=5 → три записи `2,2,1` (не merge в полный стек);
- `max_stack<2` → каждый чанк новая запись;
- hot/main не смешиваются (таблица, дающая оба прохода; hot и main отдельно);
- merge не потребляет RNG (сравнить `rng.state()` с ручным ожиданием draw'ов).

Run:

```powershell
build\bin\gl_game_tests.exe --test-case="*npc_loot*"
```

Expected: PASS.

- [ ] **Step 5: Обновить deterministic `l1_1` oracle**

Run:

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\gl_game_tests.exe --test-case="*npc_loot*"
build\bin\gl_game_tests.exe --test-case="*special*"
```

Если ожидания изменились: для каждой изменённой money/stack записи проверить,
что новое значение = последний chunk по merge-правилу (не «подогнать»).
Записать до/после в отчёт. Live-подтверждение merge: P692 `89 → 67` (research §1.2).
Обновить `tests/game/test_inventory_runtime_real.cpp`/`test_npc_loot_specials.cpp`,
только если их реальные ожидания затронуты merge.

- [ ] **Step 6: Commit (после approve)**

```powershell
git add src/game/src/npc_loot.cpp tests/game/test_npc_loot.cpp tests/game/test_npc_loot_specials.cpp tests/game/test_inventory_runtime_real.cpp
git commit -m "feat(game): merge same-prototype npc loot stacks"
```

---

### Task 2: Live tool per-call `_rand` draw trace

**Files:**
- Modify: `tools/live/gl_live_capture.cpp`
- Modify: `tools/live/README.md`

**Interfaces:**
- `CallTrace` получает `std::vector<std::uint32_t> draw_returns;` и
  `std::uint32_t draws = 0;`.
- Capture comments: `# call=<n> ... draws=<M>`; `# draw=<call> <k> ret=0x........ state=0x........`.
- Parser по-прежнему игнорирует `#`.

- [ ] **Step 1: Считать каждый DR1 hit в trace-режиме**

В DR1 handler (при `options.trace_calls_ms != 0`):
- для каждого hit читать `context.Esp` → `ret = *(u32*)context.Esp` (return address
  в вызывающую `_rand` функцию) и `state = *(u32*)(EAX+0x14)`;
- если `!state.calls.empty()`: `++state.calls.back().draws;`
  `state.calls.back().draw_returns.push_back(ret);` (cap 512);
- первый draw — capture state, второй — LCG validate (как сейчас), последующие —
  только считать;
- **не снимать DR1** в trace-режиме; ставить RF после каждого hit (как для DR0);
- в non-trace режиме поведение прежнее (RF после первого, clear после второго).

- [ ] **Step 2: Писать draw provenance**

В `write_capture` в `# call=` строку добавить ` draws=<M>`, и после каждой
call-строки — `# draw=<call> <k> ret=0x........ state=0x........` для
`draw_returns` (если есть). Cap на call/draw.

- [ ] **Step 3: README**

Документировать `--trace-calls` draw-count/return-address provenance и то, что
`# draw=` — диагностика, не oracle.

- [ ] **Step 4: Build + help**

```powershell
I:\goldenland2dec\.local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin\i686-w64-mingw32-g++.exe `
    -std=c++20 -O2 -Wall -Wextra -static tools/live/gl_live_capture.cpp `
    -o tools/live/gl_live_capture.exe -lpsapi -lshell32
.\tools\live\gl_live_capture.exe --help
```

Expected: exit 0, без warnings.

- [ ] **Step 5: Commit (после approve)**

```powershell
git add tools/live/gl_live_capture.cpp tools/live/README.md
git commit -m "feat(tools): trace per-call rand draws"
```

---

### Task 3: Price CW capture mode

**Files:**
- Modify: `tools/live/gl_live_capture.cpp`
- Modify: `tools/live/README.md`

**Interfaces:**
- Options: `--price-cw`, `--client <dll>` (default `Client.dll`), `--out <file>`.
- Output: `price=0x....` + `# source=client-price`; exit 0 on capture, 2 module,
  3 timeout.
- Breakpoint: `Client.dll base + 0x6e8d0` (или `+0x6e967`, первый x87 op).

- [ ] **Step 1: Добавить mode**

`--price-cw`:
- `find_module(pid, options.client_dll, client, error)`;
- если `--out` нет — error;
- attach; `Debugger(pid, client.base + 0x6e8d0, 0)`;
- на первом DR0 hit: `get_context`, прочитать
  `context.FloatSave.ControlWord`; записать файл:
  `price=0x%04x\n# source=client-price\n`; detach; exit 0;
- timeout → exit 3; не сканировать контейнеры.

- [ ] **Step 2: README**

Описать запуск (игра у торговца/инвентаря → tool → открыть trade/цену),
ожидаемое `price=0x007F` (PC=24/RC=nearest), и как объединить с
`generation=` в файл для `GL_SPECIALS_FPU_CW`.

- [ ] **Step 3: Build + help**

Как в Task 2 Step 4; `--help` содержит `--price-cw`.

- [ ] **Step 4: Commit (после approve)**

```powershell
git add tools/live/gl_live_capture.cpp tools/live/README.md
git commit -m "feat(tools): capture client price control word"
```

---

### Task 4: Live evidence (user-assisted) + phase analysis

**Files:**
- Create (not committed): `.local/live/special-l1_1-draws.txt`,
  `.local/live/price-cw.txt`
- Modify: `docs/research/p3b2b2e-live-parity-stage2b.md` (§12)

- [ ] **Step 1: Generation draw trace**

Пользователь: загрузить сохранение **до** первого входа в `l1_1`, дойти до
перехода, запустить:

```powershell
$pg = (Get-Process GoldenLand | Select-Object -First 1).Id
.\tools\live\gl_live_capture.exe --pid $pg --level l1_1 --loot-level 2 `
    --trace-calls 60000 --out .local\live\special-l1_1-draws.txt
```

Затем войти. Ожидание: valid capture, `# calls=16`, `# call=... draws=M`
и `# draw=... ret=0x...`.

- [ ] **Step 2: Price CW**

Пользователь: подойти к торговцу, запустить:

```powershell
.\tools\live\gl_live_capture.exe --pid $pg --price-cw --out .local\live\price-cw.txt
```

Открыть trade/цену. Ожидание: `price=0x....` (PC=24/RC=nearest).

- [ ] **Step 3: Анализ phase**

Агент:
- сравнить `draws` per call с port-моделью (R1+pass1+pass2+specials);
- для `ret`-адресов вычислить RVA (`ret - server.base`) и найти функции в
  `.local/decomp/server/functions.csv`/`decompiled.c`;
- определить потребителей лишних draw (per-call gaps 2–7);
- записать §12: per-call counts, ret-гистограмму, функции-кандидаты.
- Если источник не найден — BLOCKED, зафиксировать в STATUS.

- [ ] **Step 4: Commit docs (после approve)**

```powershell
git add docs/research/p3b2b2e-live-parity-stage2b.md
git commit -m "docs: record per-call rng draw trace"
```

---

### Task 5: Regression/docs

**Files:** `docs/DECISIONS.md`, `docs/STATUS.md`, `docs/PROJECT.md`,
`docs/research/parity-matrix.md`.

- [ ] **Step 1: Full ctest**

```powershell
cmake --build build
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
ctest --test-dir build --output-on-failure
```

- [ ] **Step 2: Docs**

D-063: merge semantics (overwrite, per-container, no RNG), phase measurement,
price CW status. STATUS/PROJECT/parity: Part 2 status, open Part 3.
- [ ] **Step 3: nul=0 check + commit (после approve).**

---

## Follow-up (Part 3, not in this plan)

После Task 4: реализовать найденных потребителей RNG phase; затем live gate
`mismatched_items==0 && mismatched_mods==0` на `l1_1`; обновить FPU price gate
на измеренный `price=`; docs. Full parity не заявляется до Part 3.
