# Phase 1b2b2e — Specials live parity (part 1) Implementation Plan

> **Executed 2026-11-11** (part 1: Stage 0/1/2a/3). Results, residuals and
> command output are recorded in `docs/STATUS.md` (1b2b2e); Stage 2b/4 remain
> a follow-up.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Сделать live-захват NPC-лута доказуемым, смоделировать inherited ITM effects и привести specials/`TradePrice` к доказанной x87 single-precision модели, не ослабляя гейты.

**Architecture:** Три независимых слоя: (1) read-only 32-bit tool пишет provenance захвата (порядок вызовов, LCG double-draw, `loot_level` из памяти); (2) порт копирует ITM `effects` в mod-массив при создании предмета; (3) specials и цена считаются в binary32 (x87 PC=24) вместо double. Live NPC oracle сравнивает raw ITM flags и включает inherited rows.

**Tech Stack:** C++20, CMake/Ninja, doctest, SDL2 (только для production wiring), i686 llvm-mingw для live tool.

## Global Constraints

- Коммиты — только после явного подтверждения пользователя (AGENTS.md). Шаги «Commit» выполнять после approve.
- Live-артефакты (`.local/live/*`, `tools/live/gl_live_capture.exe`) не коммитить.
- Тесты не ослаблять: mismatch → статус `partial`, факты в research.
- Item flags в oracle — raw ITM flags, без маски.
- Документы — UTF-8 без BOM; после правок проверить `nul=0`.
- Слои: `platform → app → game/render/ui/audio → script/core → assets`; tool не входит в 64-bit CMake.
- Тулчейн и окружение:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

## Scope of this plan

Покрывает Stage 0, Stage 1, Stage 2a (inherited effects), Stage 3 (precision).
**Вне этого плана:** Stage 2b (порядок/scope/merge по live-evidence) и Stage 4
(FPU-гейт на измеренном price CW) — отдельный follow-up plan после live-сессии.
Полный 0/N заявляется только после обоих планов.

---

### Task 0: Tool correctness (map-резолв + scan-only)

**Files:**
- Modify: `tools/live/README.md`
- Verify (уже изменён в рабочем дереве): `tools/live/gl_live_capture.cpp`

**Interfaces:**
- Consumes: `FUN_1404bf8c` → `FUN_14004f14(manager+0xc)` → `FUN_14004a20`
  (count `+0`, entries `+4`, stride 8; key `+0`, value `+4`).
- Produces: `--scan-only` CLI flag; корректный `resolve_container`.

- [ ] **Step 1: Обновить README: резолв контейнера**

В `tools/live/README.md` заменить в разделе «What it captures» абзац про
`DAT_14088df8` item manager на:

```markdown
- `DAT_14088df8` item manager. Containers are resolved through the manager's
  key→value map at `+0x0c` (count at `+0`, entry array at `+4`, stride 8:
  key at `+0`, container pointer at `+4`), matching `FUN_1404bf8c` →
  `FUN_14004f14(manager+0xc)` → `FUN_14004a20`. The old array-index lookup was
  wrong. A container record has `+0x0c` record-id array, `+0x10` slot count,
  `+0x14` used bitmap.
```

- [ ] **Step 2: Обновить README: `--scan-only`**

В раздел «Run» после описания `--timeout`/`--server` добавить:

```markdown
`--scan-only` attaches without installing breakpoints, scans the already loaded
level once, prints up to 20 `person/item/count/mods` rows, and exits 0 when the
scan is complete with at least one person, otherwise 4. It is a diagnostic mode;
it never writes an oracle capture.
```

- [ ] **Step 3: Собрать tool**

Run:

```powershell
I:\goldenland2dec\.local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin\i686-w64-mingw32-g++.exe `
    -std=c++20 -O2 -Wall -Wextra -static tools/live/gl_live_capture.cpp `
    -o tools/live/gl_live_capture.exe -lpsapi -lshell32
```

Expected: exit 0, пустой вывод.

- [ ] **Step 4: Проверить `--help` и scan-only ошибку**

Run:

```powershell
.\tools\live\gl_live_capture.exe --help
.\tools\live\gl_live_capture.exe --pid 999999 --scan-only
```

Expected: первая команда печатает usage с `--scan-only`, exit 0; вторая даёт
`module`/process error и exit 2 (никакого crash). `--pid 0` — usage error
(exit 1) из-за существующего guard'а; это не цель проверки.

- [ ] **Step 5: Commit (после approve)**

```powershell
git add tools/live/gl_live_capture.cpp tools/live/README.md docs/DECISIONS.md docs/STATUS.md docs/research/p3b2b2c-npc-loot-specials.md
git commit -m "fix(tools): resolve item containers by map"
```

---

### Task 1: Live tool provenance (call order, LCG double-draw, loot level)

**Files:**
- Modify: `tools/live/gl_live_capture.cpp`
- Modify: `tools/live/README.md`

**Interfaces:**
- Consumes: `FUN_1404b1c0` is `__thiscall`; at DR0 entry `[esp+4]=hot id`,
  `[esp+8]=main id`, `[esp+0xc]=inv name (cp1251 char*)`.
- Produces: `# call=<n> tid=<tid> hot=<id> main=<id> inv=<name>` comments;
  `# rng_second=0x........ check=ok|fail|unavailable`; `loot_level` from
  `DAT_14088dc4+0x26c4 + 1`; `# loot_level_source=memory|cli`.

- [ ] **Step 1: Добавить константы и структуры**

В `tools/live/gl_live_capture.cpp` после `kRandBeforeDrawRva`:

```cpp
constexpr uint32_t kLootLevelOffset = 0x26c4;
constexpr uint32_t kNpcSpawnHotStackOffset = 4;
constexpr uint32_t kNpcSpawnMainStackOffset = 8;
constexpr uint32_t kNpcSpawnInventoryStackOffset = 0xc;
constexpr std::size_t kMaxCallTrace = 512;
```

В `struct Options` добавить:

```cpp
    int trace_calls_ms = 0;
```

В `struct ScanResult` добавить:

```cpp
    std::optional<uint32_t> loot_level;
```

Перед `struct CaptureState` добавить:

```cpp
struct CallTrace {
    uint32_t index = 0;
    DWORD tid = 0;
    uint32_t hot = 0;
    uint32_t main = 0;
    std::string inventory;
};
```

В `struct CaptureState` добавить, а неиспользуемое после Step 5 поле
`bool draw_attempted` удалить:

```cpp
    std::vector<CallTrace> calls;
    uint32_t second_rng_state = 0;
    bool second_draw_seen = false;
    bool second_draw_valid = false;
```

- [ ] **Step 2: Разобрать `--trace-calls`**

В `print_usage()` добавить строку в Options:

```text
  --trace-calls <ms>   keep logging NPC spawn calls for <ms> after attach
```

В `parse_args` перед `--scan-only` добавить (используя существующий `take_value`):

```cpp
        } else if (key == L"--trace-calls") {
            if (!take_value())
                return false;
            int32_t milliseconds = 0;
            if (!parse_i32(value, milliseconds) || milliseconds < 0 ||
                milliseconds > 600000) {
                error = "invalid --trace-calls";
                return false;
            }
            options.trace_calls_ms = milliseconds;
```

**Expected:** `--help` печатает новый флаг.

- [ ] **Step 3: Писать live `loot_level` из памяти**

В `scan_once` после проверки `level_ptr` добавить:

```cpp
    uint32_t raw_loot_level = 0;
    if (mem.u32(level_ptr + kLootLevelOffset, raw_loot_level) &&
        raw_loot_level < 1000000u)
        result.loot_level = raw_loot_level + 1;
```

В `write_capture` заменить строку `file << "loot_level=" << options.loot_level << "\n";` на:

```cpp
    const std::optional<uint32_t> loot_level =
        scan.loot_level.has_value() ? scan.loot_level
                                    : std::optional<uint32_t>(
                                          static_cast<uint32_t>(options.loot_level));
    file << "loot_level=" << *loot_level << "\n";
    file << "# loot_level_source=" << (scan.loot_level.has_value() ? "memory" : "cli") << "\n";
```

- [ ] **Step 4: Логировать каждый DR0-вызов**

В обработчике `address == state.bp0` заменить блок логирования/очистки на:

```cpp
                    if (state.calls.size() < kMaxCallTrace) {
                        CallTrace call;
                        call.index = static_cast<uint32_t>(state.calls.size() + 1);
                        call.tid = event.dwThreadId;
                        CONTEXT context{};
                        if (debugger.get_context(event.dwThreadId, context)) {
                            std::uint32_t name_ptr = 0;
                            mem.u32(context.Esp + kNpcSpawnHotStackOffset, call.hot);
                            mem.u32(context.Esp + kNpcSpawnMainStackOffset, call.main);
                            mem.u32(context.Esp + kNpcSpawnInventoryStackOffset, name_ptr);
                            std::string raw;
                            if (name_ptr != 0 && mem.cstring(name_ptr, kMaxNameBytes, raw))
                                call.inventory = sanitize_name(cp1251_to_utf8(raw));
                        }
                        log_info("npc call " + std::to_string(call.index) + " tid=" +
                                 std::to_string(call.tid) + " hot=" + address_string(call.hot) +
                                 " main=" + address_string(call.main) + " inv=" +
                                 call.inventory);
                        state.calls.push_back(std::move(call));
                    }
                    if (!state.second_draw_seen)
                        debugger.arm_dr1(event.dwThreadId);
                    if (options.trace_calls_ms == 0)
                        debugger.clear_dr0(event.dwThreadId);
```

**Note:** при `--trace-calls` DR0 остаётся взведённым, чтобы поймать все вызовы;
без флага — прежнее поведение. DR1 повторно не взводится после второго draw.

- [ ] **Step 5: Двойной draw + LCG self-check**

В обработчике `address == state.bp1` заменить ветку `else if (!state.draw_attempted)`
на две стадии:

```cpp
                        } else if (!state.state_captured) {
                            CONTEXT context{};
                            if (debugger.get_context(event.dwThreadId, context)) {
                                const uint32_t crt = context.Eax;
                                uint32_t rng_state = 0;
                                if (crt != 0 && mem.u32(crt + 0x14, rng_state)) {
                                    state.rng_state = rng_state;
                                    state.draw_cw = static_cast<uint16_t>(
                                        context.FloatSave.ControlWord & 0xffffu);
                                    state.state_captured = true;
                                    log_info("captured CRT _rand state " +
                                             address_string(rng_state) +
                                             " before the first NPC draw");
                                } else {
                                    state.capture_failed = true;
                                    log_error("cannot read CRT state from EAX=" +
                                              address_string(crt) + " at the first NPC draw");
                                }
                            } else {
                                state.capture_failed = true;
                                log_error("cannot read the generation thread context at the "
                                          "first NPC draw");
                            }
                        } else if (!state.second_draw_seen) {
                            CONTEXT context{};
                            state.second_draw_seen = true;
                            if (debugger.get_context(event.dwThreadId, context)) {
                                const uint32_t crt = context.Eax;
                                uint32_t rng_state = 0;
                                if (crt != 0 && mem.u32(crt + 0x14, rng_state)) {
                                    state.second_rng_state = rng_state;
                                    state.second_draw_valid =
                                        rng_state == state.rng_state * 0x343FDu + 0x269EC3u;
                                    log_info("second draw state " + address_string(rng_state) +
                                             (state.second_draw_valid ? " LCG check ok"
                                                                      : " LCG CHECK FAILED"));
                                }
                            }
                            debugger.clear_dr1(event.dwThreadId);
                            if (options.trace_calls_ms == 0)
                                stop = true;
                        } else {
                            debugger.clear_dr1(event.dwThreadId);
                        }
```

**Note:** DR1 остаётся взведённым до второго draw (нужен для LCG-проверки) и
снимается во второй стадии. При обычном режиме после второго draw — `stop`;
при `--trace-calls` цикл продолжается до конца trace-окна. Третья+ ветка
снимает DR1, если он всё же сработал.

- [ ] **Step 6: Ограничить trace окном**

В `run()` после `state.bp1 = ...` добавить:

```cpp
    std::optional<std::chrono::steady_clock::time_point> trace_deadline;
    if (options.trace_calls_ms != 0)
        trace_deadline = std::chrono::steady_clock::now() +
                         std::chrono::milliseconds(options.trace_calls_ms);
```

В цикле после проверки `deadline` добавить:

```cpp
        if (trace_deadline.has_value() && now >= *trace_deadline) {
            log_info("trace window elapsed; detaching");
            stop = true;
            break;
        }
```

- [ ] **Step 7: Записывать provenance в capture**

В `write_capture` после строки `# server_base=...` добавить:

```cpp
    file << "# rng_second=0x" << std::hex << state.second_rng_state << std::dec
         << " check=" << (state.second_draw_seen
                              ? (state.second_draw_valid ? "ok" : "fail")
                              : "unavailable")
         << "\n";
    file << "# calls=" << state.calls.size() << "\n";
    for (const CallTrace &call : state.calls) {
        file << "# call=" << call.index << " tid=" << call.tid
             << " hot=" << call.hot << " main=" << call.main
             << " inv=" << call.inventory << "\n";
    }
```

- [ ] **Step 8: Обновить README и собрать**

Добавить в `tools/live/README.md` в «What it captures» и «Capture format»:
`--trace-calls <ms>` (логирует `# call=<n> tid= main= hot= inv=`), `# calls=`,
`# rng_second=0x........ check=ok|fail`, `loot_level_source=memory|cli`.

Run те же команды, что в Task 0 Step 3–4, затем:

```powershell
.\tools\live\gl_live_capture.exe --help
```

Expected: usage содержит `--trace-calls`; сборка exit 0 без warning.

- [ ] **Step 9: Commit (после approve)**

```powershell
git add tools/live/gl_live_capture.cpp tools/live/README.md
git commit -m "feat(tools): record live capture provenance"
```

---

### Task 2: Live evidence run (user-assisted)

**Files:**
- Create (agent, not committed): `.local/live/special-l1_1-trace.txt`
- Modify: `docs/research/p3b2b2c-npc-loot-specials.md` (§11)
- Modify: `tests/game/test_special_gates.cpp` (port order diagnostic)

**Interfaces:**
- Consumes: Task 1 build; running 32-bit `GoldenLand.exe`.
- Produces: evidence report §11: call order, LCG check, live `loot_level`,
  fresh mismatch table.

- [ ] **Step 1: Добавить диагностику порядка порта**

В `test_special_gates.cpp` в `struct PortRun` добавить:

```cpp
    std::vector<std::string> order;
```

В `build_port_records` в цикле по `level->persons` после `++port.generated_persons;`:

```cpp
        port.order.push_back(person.name + "(" + person.inventory + ")");
```

В тесте `special gates: live NPC-mod oracle` после `build_port_records` добавить:

```cpp
    std::ostringstream order;
    for (const std::string &name : port.order)
        order << name << " ";
    MESSAGE("port order: " << order.str());
```

- [ ] **Step 2: Собрать и запустить симуляцию порядка**

Run:

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="special gates: live NPC-mod oracle" --success
```

Expected: тест либо skip (`GL_SPECIALS_LIVE` не задан), либо печатает
`port order: ...`. Строка нужна для сверки с live `# call=`.

- [ ] **Step 3: Пользователь запускает оригинал и tool**

Команды для пользователя (не агент):

```powershell
$pg = (Get-Process GoldenLand | Select-Object -First 1).Id
.\tools\live\gl_live_capture.exe --pid $pg --level l1_1 --loot-level 2 `
    --trace-calls 5000 --out .local\live\special-l1_1-trace.txt
```

Условие запуска: tool запускается в главном меню, и `l1_1` загружается
**первым** уровнем после attach (новая игра или первый вход в локацию, не
повторный re-enter и не restock).

Ожидание: tool пишет `wrote ...`, exit 0; файл содержит
`format=gl-specials-live-2`, `rng_state=...`, `loot_level=...`, `# calls=N`,
`# call=...`, `# rng_second=... check=ok`.

Если exit 3/4 — агенту передать `.partial` и лог; сессия повторяется.

- [ ] **Step 4: Сверка порядка и provenance**

Агент сравнивает `# call=` последовательность с `port order`:
- совпали ли имена и порядок;
- какие persons вызываются live, но отсутствуют/лишние в порте;
- `# rng_second ... check=ok` (иначе захват невалиден);
- `loot_level_source=memory` и значение.

Записать в `docs/research/p3b2b2c-npc-loot-specials.md` новый `## 11. Live
provenance (2026-11-11)` со следующими фактическими данными из capture-файла
и лога (значения копировать из вывода, без пересказа):

1. Путь артефакта (`.local/live/special-l1_1-trace.txt`, not committed).
2. `rng_state` и `loot_level` + `loot_level_source`.
3. `# rng_second` и `check=ok|fail`.
4. Полная live-последовательность `# call=` в виде `index:inv`.
5. `port order` из gate-сообщения.
6. Первый несовпавший вызов/person (номер и имя).
7. Gate-строку `oracle compare: items ...; mods ...` целиком.

- [ ] **Step 5: Commit (после approve)**

```powershell
git add tests/game/test_special_gates.cpp docs/research/p3b2b2c-npc-loot-specials.md
git commit -m "test(game): print port loot order and record live provenance"
```

---

### Task 3: Inherited ITM effects → mods

**Files:**
- Modify: `src/game/include/gl/game/inventory_runtime.hpp`
- Modify: `src/game/src/inventory_runtime.cpp`
- Modify: `src/game/src/npc_loot.cpp`
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/game/test_inventory_runtime.cpp`
- Modify: `tests/game/test_npc_loot.cpp`
- Modify: `tests/game/test_special_gates.cpp`

**Interfaces:**
- Produces: `struct ItemTemplateEffect { uint32_t type; int32_t value; uint32_t flags; };`
  `ItemTemplate` получает поле `std::vector<ItemTemplateEffect> effects;`
  (6-е поле, после `mod_count`; существующие 4/5-элементные brace-init
  остаются валидными).
- Consumes: `gl::assets::Item::effects` (`type`, `amount`, `flags`).

- [ ] **Step 1: Написать падающий unit-тест копирования effects**

В `tests/game/test_inventory_runtime.cpp` заменить тест
`inventory_runtime add initializes inherited mod count from the template`
(строка 404) на:

```cpp
TEST_CASE("inventory_runtime add copies inherited ITM effects into mods") {
    auto lookup = [](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        if (item_id == 1101)
            return ItemTemplate{26, 0x04u, 0, 1, 2,
                                {ItemTemplateEffect{7, 1, 0x50},
                                 ItemTemplateEffect{8, -1, 0x50}}};
        return std::nullopt;
    };
    auto runtime = InventoryRuntime::create({}, make_names(), lookup);

    REQUIRE(runtime.add_item("Hero", "QST_1_0_159", 1));
    const auto &items = runtime.hero_inventory().items;
    REQUIRE(items.size() == 1);
    CHECK(items[0].mod_count == 2);
    REQUIRE(items[0].mods.size() == 2);
    CHECK(items[0].mods[0].id == 7);
    CHECK(items[0].mods[0].value == 1);
    CHECK(items[0].mods[0].flags == 0x50u);
    CHECK(items[0].mods[1].id == 8);
    CHECK(items[0].mods[1].value == -1);
    CHECK(items[0].mods[1].flags == 0x50u);

    TradePriceInput input{};
    input.item_id = 1101;
    input.item_price = 50;
    input.durability = 100;
    input.flags = items[0].flags;
    input.mod_count = items[0].mod_count;
    input.skill_trade = 1;
    input.count = 1;
    input.assume_pc53 = true;
    CHECK_FALSE(compute_trade_price(input).has_value());
    input.mod_count = 0;
    CHECK(compute_trade_price(input).has_value());
}
```

`assume_pc53` будет переименован в `assume_x87_single` в Task 5; этот тест
попадает в общий список замен.

- [ ] **Step 2: Запустить тест — должен упасть**

Run:

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*inherited ITM effects*"
```

Expected: FAIL — `ItemTemplateEffect`/`effects` ещё не существуют (compile
error) или `mods.size() == 0`.

- [ ] **Step 3: Добавить тип и поле**

В `src/game/include/gl/game/inventory_runtime.hpp` перед `struct ItemTemplate`:

```cpp
struct ItemTemplateEffect {
    std::uint32_t type = 0;
    std::int32_t value = 0;
    std::uint32_t flags = 0;
};
```

В `struct ItemTemplate` после `mod_count`:

```cpp
    std::vector<ItemTemplateEffect> effects;
```

- [ ] **Step 4: Копировать effects при add**

В `src/game/src/inventory_runtime.cpp` в `append_items` заменить создание
записи:

```cpp
        gl::assets::InventoryItem record{item_id, added, item_template.flags,
                                         item_template.type, item_template.style, 100,
                                         item_template.mod_count};
        record.mods.reserve(item_template.effects.size());
        for (const ItemTemplateEffect &effect : item_template.effects)
            record.mods.push_back({effect.type, effect.value, effect.flags});
        items.push_back(std::move(record));
```

- [ ] **Step 5: Запустить тест — должен пройти**

Run: та же команда, что Step 2.
Expected: PASS.

- [ ] **Step 6: Написать падающий тест NPC-лута**

В `tests/game/test_npc_loot.cpp` добавить `using gl::game::ItemTemplateEffect;`
рядом с существующими `using`, затем тест:

```cpp
TEST_CASE("npc_loot: inherited ITM effects are copied without consuming RNG") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 0.75;
    table.chance_decrease = 1.0;
    table.quantity_decrease = 1.0;
    table.items = {make_item("A", 0, 20, 100, 1, 1)};

    auto plain_lookup = [](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        if (item_id == 100)
            return ItemTemplate{1, 0x4u, 7, 4};
        return std::nullopt;
    };
    auto effects_lookup = [](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        if (item_id == 100)
            return ItemTemplate{1, 0x4u, 7, 4, 2,
                                {ItemTemplateEffect{7, 1, 0x50},
                                 ItemTemplateEffect{8, -1, 0x50}}};
        return std::nullopt;
    };

    MsvcRandom plain_rng(1);
    const NpcLoot plain =
        NpcLootGenerator::generate(table, 11, names, plain_lookup, plain_rng);
    MsvcRandom effects_rng(1);
    const NpcLoot effects =
        NpcLootGenerator::generate(table, 11, names, effects_lookup, effects_rng);

    CHECK(effects_rng.state() == plain_rng.state());
    REQUIRE(effects.main.size() == 1);
    CHECK(effects.main[0].mod_count == 2);
    REQUIRE(effects.main[0].mods.size() == 2);
    CHECK(effects.main[0].mods[0].id == 7);
    CHECK(effects.main[0].mods[0].value == 1);
    CHECK(effects.main[0].mods[0].flags == 0x50u);
    CHECK(effects.main[0].mods[1].id == 8);
    CHECK(effects.main[0].mods[1].value == -1);
    CHECK(effects.main[0].mods[1].flags == 0x50u);
    CHECK(effects.main[0].flags == plain.main[0].flags);
}
```

Run: `build\bin\gl_game_tests.exe --test-case="*inherited ITM effects*"` →
Expected: FAIL (`mods` пуст, `mod_count` 0).

- [ ] **Step 7: Копировать effects в `append_chunks`**

В `src/game/src/npc_loot.cpp` после создания `LootItem loot{...}` и
`loot.mod_count = item_template.mod_count;` добавить:

```cpp
        loot.mods.reserve(item_template.effects.size());
        for (const ItemTemplateEffect &effect : item_template.effects)
            loot.mods.push_back({effect.type, effect.value, effect.flags});
```

- [ ] **Step 8: Запустить оба теста**

Run:

```powershell
build\bin\gl_game_tests.exe --test-case="*inherited ITM effects*"
build\bin\gl_game_tests.exe --test-case="*npc_loot*"
```

Expected: PASS; RNG state совпадает, specials не потребляют лишних draw.

- [ ] **Step 9: Включить effects в production lookup**

В `tools/glgame/main.cpp` добавить helper в анонимном namespace (рядом с
другими helper'ами):

```cpp
std::optional<gl::game::ItemTemplate>
make_item_template(const gl::assets::ItemCatalogEntry *entry) {
    if (entry == nullptr)
        return std::nullopt;
    std::vector<gl::game::ItemTemplateEffect> effects;
    effects.reserve(entry->item.effects.size());
    for (const gl::assets::ItemEffect &effect : entry->item.effects)
        effects.push_back({effect.type, static_cast<std::int32_t>(effect.amount),
                           effect.flags});
    return gl::game::ItemTemplate{
        static_cast<std::uint32_t>(entry->item.type), entry->item.flags,
        entry->item.unknown_b[1], entry->item.max_stack,
        static_cast<std::uint32_t>(effects.size()), std::move(effects)};
}
```

Заменить оба lambda-возврата `ItemTemplate{...}` (около строк 908 и 938) на
`make_item_template(entry)`.

- [ ] **Step 10: Включить effects в oracle lookup и убрать исключение inherited**

В `tests/game/test_special_gates.cpp`:

1. `make_lookup` заменить на:

```cpp
gl::game::InventoryRuntime::ItemTemplateLookup make_lookup(const gl::assets::ItemCatalog &catalog) {
    return [&catalog](std::uint32_t item_id) -> std::optional<gl::game::ItemTemplate> {
        const gl::assets::ItemCatalogEntry *entry = catalog.find(item_id);
        if (entry == nullptr)
            return std::nullopt;
        std::vector<gl::game::ItemTemplateEffect> effects;
        effects.reserve(entry->item.effects.size());
        for (const gl::assets::ItemEffect &effect : entry->item.effects)
            effects.push_back({effect.type, static_cast<std::int32_t>(effect.amount),
                               effect.flags});
        return gl::game::ItemTemplate{static_cast<std::uint32_t>(entry->item.type),
                                      entry->item.flags, entry->item.unknown_b[1],
                                      entry->item.max_stack,
                                      static_cast<std::uint32_t>(effects.size()),
                                      std::move(effects)};
    };
}
```

2. Удалить `item_has_inherited_mods`, `filter_inherited_capture`, поля
   `inherited_capture_items/…/inherited_port_items`, вызов
   `filter_inherited_capture` и блок skip по `item_template->mod_count > 0`
   в `build_port_records`.
3. Удалить тест `special gates: inherited ITM-effect rows are excluded from comparison`.

- [ ] **Step 11: Прогнать gate-тесты и real specials**

Run:

```powershell
cmake --build build
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\gl_game_tests.exe --test-case="*special*"
```

Expected: все тесты PASS; oracle self-check не падает; при заданном
`GL_SPECIALS_LIVE` mismatched уменьшается и в отчёте больше нет
`skipped inherited` строки.

- [ ] **Step 12: Commit (после approve)**

```powershell
git add src/game/include/gl/game/inventory_runtime.hpp src/game/src/inventory_runtime.cpp src/game/src/npc_loot.cpp tools/glgame/main.cpp tests/game/test_inventory_runtime.cpp tests/game/test_npc_loot.cpp tests/game/test_special_gates.cpp
git commit -m "feat(game): copy inherited ITM effects into item mods"
```

---

### Task 4: Specials x87 single-precision model

**Files:**
- Modify: `src/game/src/special_engine.cpp`
- Modify: `tests/game/test_special_engine.cpp`

**Interfaces:**
- Consumes: `SpecialSlot::value` (float), `loot_level` (int).
- Produces: `SpecialMod.value` совпадает с x87 PC=24 (binary32 после каждой
  операции) и trunc-`ROUND`.

- [ ] **Step 1: Написать падающий boundary-тест**

В `tests/game/test_special_engine.cpp` добавить:

```cpp
TEST_CASE("special engine uses x87 single-precision product") {
    SpecialDatabase db;
    SpecialDef def;
    def.id = 0;
    def.value_slots[0] = value_slot(0x40, 1, 0x1.4d29c2p+0f);
    def.time_slots[0] = value_slot(0, 1, 1.0f);
    db.defs = {def};
    db.item_classes[ItemType::sword] = {0};

    MsvcRandom rng(9969);
    const std::vector<SpecialMod> mods =
        SpecialEngine::apply(db, ItemType::sword, 1, 282, rng);
    REQUIRE(mods.size() == 1);
    CHECK(mods[0].id == 0);
    CHECK(mods[0].value == 367);
}
```

**Note:** double-модель даёт `int(1.301418423652649 * 282) = 366`; x87
PC=24 даёт `float(...) = 367`. Seed `9969` (`next()==32593`) нужен, чтобы
`range(half, value)` вернул верхнюю границу: при seed 1 оба варианта дают
`half=183`, и тест не различает модели.

- [ ] **Step 2: Запустить — должен упасть**

Run:

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*x87 single-precision product*"
```

Expected: FAIL `367 == 366`.

- [ ] **Step 3: Перевести `SpecialEngine::apply` на float**

В `src/game/src/special_engine.cpp` заменить блок value (от `double fval`
до `int half = value >> 1;`) на:

```cpp
        float fval = 0.0f;
        gl::assets::SpecialFlags tag1 = 0;
        gl::assets::SpecialFlags tag2 = 0;

        int present = 0;
        for (const gl::assets::SpecialSlot &slot : def->value_slots) {
            if (slot.access != 0)
                ++present;
        }
        int pick = present > 1 ? rng.range(0, present - 1) : 0;
        for (const gl::assets::SpecialSlot &slot : def->value_slots) {
            if (slot.access == 0)
                continue;
            if (pick == 0) {
                tag1 = slot.tag;
                fval = slot.value;
                break;
            }
            --pick;
        }

        present = 0;
        for (std::size_t index = 0; index < kTimeSlotsRead; ++index) {
            if (def->time_slots[index].access != 0)
                ++present;
        }
        pick = present > 1 ? rng.range(0, present - 1) : 0;
        for (std::size_t index = 0; index < kTimeSlotsRead; ++index) {
            const gl::assets::SpecialSlot &slot = def->time_slots[index];
            if (slot.access == 0)
                continue;
            if (pick == 0) {
                tag2 = slot.tag;
                fval *= slot.value;
                break;
            }
            --pick;
        }

        const gl::assets::SpecialFlags flags = tag1 | tag2;
        int value = static_cast<int>(fval * static_cast<float>(loot_level));
        int half = value >> 1;
        if (fval < 0.0f) {
```

- [ ] **Step 4: Запустить boundary + deterministic specials**

Run:

```powershell
build\bin\gl_game_tests.exe --test-case="*x87 single-precision product*"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\gl_game_tests.exe --test-case="*special*"
build\bin\gl_game_tests.exe --test-case="*npc_loot*"
```

Expected: PASS. Если deterministic `l1_1` oracle изменился — **остановиться**,
записать diff в research, не обновлять ожидания вслепую.

- [ ] **Step 5: Commit (после approve)**

```powershell
git add src/game/src/special_engine.cpp tests/game/test_special_engine.cpp
git commit -m "fix(game): use x87 single precision for specials"
```

---

### Task 5: TradePrice x87 single-precision model

**Files:**
- Modify: `src/game/include/gl/game/trade_price.hpp`
- Modify: `src/game/src/trade_price.cpp`
- Modify: `tests/game/test_trade_price.cpp`
- Modify: `tests/app/test_trade_overlay.cpp`
- Modify: `tests/game/test_inventory_runtime.cpp`
- Modify: `tests/game/test_npc_loot_specials.cpp`
- Modify: `tools/glgame/main.cpp`

**Interfaces:**
- Produces: `TradePriceInput::assume_x87_single` (переименование
  `assume_pc53`); binary32-арифметика в `compute_trade_price`.
- Consumes: live CW evidence (Stage 4 подтвердит price CW).

- [ ] **Step 1: Написать падающий boundary-тест**

В `tests/game/test_trade_price.cpp` добавить:

```cpp
TEST_CASE("trade_price uses x87 single precision for the skill factor") {
    TradePriceInput input{};
    input.item_id = 1;
    input.item_price = 5;
    input.durability = 100;
    input.flags = 0;
    input.mod_count = 0;
    input.skill_trade = 9;
    input.count = 1;
    input.assume_pc53 = true;

    const auto price = compute_trade_price(input);
    REQUIRE(price.has_value());
    CHECK(price->buy == 8);
    CHECK(price->sell == 3);
}
```

Expected: FAIL `7 == 8` (текущая float-literal-модель; правильная модель —
double-константы с binary32-результатом).

- [ ] **Step 2: Переименовать флаг**

Заменить `assume_pc53` на `assume_x87_single` в каждом сайте (использовать
`edit` по одному вхождению, не `Set-Content`):

| Файл | Вхождения (номера — из текущего дерева, сверять `grep`) |
|---|---|
| `src/game/include/gl/game/trade_price.hpp` | поле + комментарий |
| `src/game/src/trade_price.cpp` | guard |
| `tests/app/test_trade_overlay.cpp` | 1 |
| `tests/game/test_inventory_runtime.cpp` | 3 |
| `tests/game/test_npc_loot_specials.cpp` | 2 |
| `tests/game/test_trade_price.cpp` | 2 |
| `tools/glgame/main.cpp` | 1 |

В `trade_price.hpp` обновить комментарий: `assume_x87_single` — x87
PC=24/RC=nearest.

Проверить, что не осталось старого имени:

```powershell
Select-String -Path src\**\*.cpp,src\**\*.hpp,tests\**\*.cpp,tools\**\*.cpp -Pattern "assume_pc53"
```

Expected: пусто.

- [ ] **Step 3: Перевести цену на binary32-результаты**

`FUN_1206E8D0` грузит константы как m64 doubles (`0.0025`, `0.75`, `-0.1`,
`2.5`), а x87 CW=0x007F округляет **каждый результат** до binary32.
В `src/game/src/trade_price.cpp` заменить тело после guard'ов на:

```cpp
    const float durability_scaled = static_cast<float>(
        static_cast<double>(input.durability) * 0.0025);
    const float durability_factor = static_cast<float>(
        static_cast<double>(durability_scaled) + 0.75);
    const float base_scaled = static_cast<float>(
        static_cast<double>(durability_factor) * static_cast<double>(input.item_price));
    const std::int64_t base0 = static_cast<std::int64_t>(base_scaled);

    std::int64_t base = base0;
    if (base < base0 / 3)
        base = base0 / 3;

    const float skill_scaled = static_cast<float>(
        static_cast<double>(input.skill_trade) * -0.1);
    float factor = static_cast<float>(static_cast<double>(skill_scaled) + 2.5);
    if (factor < 1.0f)
        factor = 1.0f;

    const float buy_factor = factor;
    const float sell_factor = static_cast<float>(1.0 / static_cast<double>(factor));
    std::int64_t buy_unit = static_cast<std::int64_t>(
        static_cast<float>(static_cast<double>(buy_factor) * static_cast<double>(base)));
    std::int64_t sell_unit = static_cast<std::int64_t>(
        static_cast<float>(static_cast<double>(sell_factor) * static_cast<double>(base)));
    if (buy_unit < 1)
        buy_unit = 1;
    if (sell_unit < 1)
        sell_unit = 1;
```

Остальные проверки/`TradePrice` не меняются.

- [ ] **Step 4: Запустить trade-тесты**

Run:

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*trade_price*"
build\bin\gl_game_tests.exe --test-case="*inventory_runtime*"
build\bin\trade_overlay_tests.exe
```

Expected: PASS; прежние boat-trader оракулы (16800/2912, 17000/4250 и т.д.)
не изменились, новый boundary-тест даёт 8/3.

- [ ] **Step 5: Commit (после approve)**

```powershell
git add src/game/include/gl/game/trade_price.hpp src/game/src/trade_price.cpp tests/game/test_trade_price.cpp tests/app/test_trade_overlay.cpp tests/game/test_inventory_runtime.cpp tests/game/test_npc_loot_specials.cpp tools/glgame/main.cpp
git commit -m "fix(game): use x87 single precision for trade price"
```

---

### Task 6: Regression, docs, follow-up gate

**Files:**
- Modify: `docs/DECISIONS.md` (D-062)
- Modify: `docs/STATUS.md` (1b2b2e progress)
- Modify: `docs/PROJECT.md` (P5/parity summary)
- Modify: `docs/research/parity-matrix.md` (specials/price rows)
- Modify: `docs/research/p3b2b2c-npc-loot-specials.md` (§11 ссылка на D-062)

**Interfaces:**
- Consumes: Tasks 0–5, Task 2 evidence.
- Produces: documented precision model; список residual mismatches для
  follow-up plan (Stage 2b/4).

- [ ] **Step 1: Полный `ctest`**

Run:

```powershell
cmake --build build
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
ctest --test-dir build --output-on-failure
```

Expected: все тесты passed, exit 0. Зафиксировать `Total Test time` в STATUS.

- [ ] **Step 2: Re-run live gate (если capture есть)**

Run:

```powershell
$env:GL_SPECIALS_LIVE="I:\goldenland2dec\.local\live\special-l1_1-trace.txt"
build\bin\gl_game_tests.exe --test-case="special gates: live NPC-mod oracle" --success
```

Expected: печатает новый mismatch-отчёт. Если 0/N — зафиксировать; если остались
mismatch — перенести точный diff в STATUS Open Questions и research §11.

- [ ] **Step 3: Записать D-062**

Добавить строку в `docs/DECISIONS.md`:

```markdown
| D-062 | 2026-11-11 | Phase 1b2b2e — live precision/inherited model. Live x87 CW `0x007F` (PC=24, RC=nearest) означает binary32-округление после каждой x87-операции; specials (`FUN_14047284`) и `TradePrice` (`FUN_1206E8D0`) считаются в float-модели, без опоры на host FPU. Inherited ITM `effects` копируются в runtime mod-массив как `id=type`, `value=int32(amount)`, `flags` (без RNG); `mod_count = effects.size() + generated`. Item flags в oracle — raw ITM flags. Live price CW остаётся OPEN до Stage 4. | research `p3b2b2c-npc-loot-specials.md` §11; spec 1b2b2e; STATUS 1b2b2e. |
```

- [ ] **Step 4: Обновить STATUS/PROJECT/parity**

- STATUS 1b2b2e: что сделано по Stage 0/1/2a/3, что осталось (Stage 2b/4),
  точные residual mismatch, команды и результаты.
- PROJECT P5: строка про inherited effects + single precision, статус
  `partial` до Stage 4.
- parity-matrix: обновить specials/price строки (`partial`/`verified`) в
  соответствии с фактическими гейтами.

- [ ] **Step 5: Проверить документы**

Run:

```powershell
$files = "docs/DECISIONS.md","docs/STATUS.md","docs/PROJECT.md","docs/research/p3b2b2c-npc-loot-specials.md","docs/research/parity-matrix.md"
foreach ($f in $files) { $b=[System.IO.File]::ReadAllBytes((Resolve-Path $f)); "$f nul=$((@($b | Where-Object {$_ -eq 0})).Count)" }
```

Expected: `nul=0` у всех.

- [ ] **Step 6: Commit (после approve)**

```powershell
git add docs/DECISIONS.md docs/STATUS.md docs/PROJECT.md docs/research/p3b2b2c-npc-loot-specials.md docs/research/parity-matrix.md
git commit -m "docs: record live parity phase 1"
```

---

## Follow-up plan (не входит в этот план)

После Task 2 evidence:

1. **Stage 2b** — порядок/scope генерации persons и merge/stacking по live
   `# call=` последовательности; при необходимости — новый research по
   spawn-условиям SEF.
2. **Stage 4** — price CW capture у `FUN_1206E8D0`, обновление FPU-гейта
   на измеренные значения (PC=24/RC=nearest), финальный 0/N.

Follow-up plan создаётся после получения evidence; до этого full parity
не заявляется.
