# Phase 1b2b2f — Per-person pre-loot RNG Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Потреблять `weapon` + `spawn init` RNG-draw в SEF-порядке после loot каждого персонажа, чтобы live NPC-oracle `l1_1` дал `mismatched_items == 0 && mismatched_mods == 0`.

**Architecture:** `assets` парсит `weapon`-блок в файловом порядке; новый `game::consume_person_init_rng` делает `coin(0.5)`-цикл и `range(3000,10000)`; `generate_npc_inventories` и live-gate вызывают его после loot для каждого не-Hero персонажа. Оракулы пересчитываются независимым Python-референсом, валидированным на live capture.

**Tech Stack:** C++20, CMake/Ninja, doctest, llvm-mingw i686 (только для уже готового live-tool), Python 3 (reference, не коммитится).

## Global Constraints

- Коммиты — только после явного подтверждения пользователя; live-артефакты и Python-reference не коммитить.
- Слои только вниз: `assets` не зависит от `game`; SDL не трогать.
- Порядок на персонажа строго: loot draws → weapon draws → один spawn draw.
- Weapon eligibility: `min_level + weapon_level_offset <= person.level <= max_level + weapon_level_offset`, файловый порядок.
- `coin(0.5)`-цикл с wrap; ровно один `range(3000,10000)`; никаких фиксированных offset.
- Тесты не ослаблять. Deterministic-оракулы обновлять только из независимого Python-reference, валидированного на live capture; не из C++-порта.
- Документы UTF-8 без BOM, `nul=0`, без литеральных `\n`; не писать markdown через `Set-Content`/here-string.
- Тулчейн:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
```

## Scope

Только RNG-выравнивание (spec `docs/specs/2026-11-11-p3b2b2f-person-rng-design.md`).
Материализация оружия NPC, хранение `spawn_roll`, Hero spawn и пост-спавн RNG — вне scope.

---

### Task 0: Зафиксировать задачу в STATUS

**Files:**
- Modify: `docs/STATUS.md` (новый раздел сверху, после заголовка `# STATUS`/`Обновлено`)

**Interfaces:**
- Consumes: ничего.
- Produces: запись Phase 1b2b2f с критериями и командами для последующих задач.

- [ ] **Step 1: Добавить раздел**

Вставить сразу после строки `Обновлено: 2026-11-11`:

```markdown
## Phase 1b2b2f — Per-person pre-loot RNG (in progress)

- **Задача:** потреблять weapon (`FUN_1401d99c`) + spawn
  (`FUN_14021bc8`) RNG-draw в SEF-порядке после loot каждого персонажа.
- **Spec:** `docs/specs/2026-11-11-p3b2b2f-person-rng-design.md`.
- **Plan:** `docs/plans/2026-11-11-p3b2b2f-person-rng.md`.
- **Критерии приёмки:** live l1_1 oracle
  `mismatched_items==0 && mismatched_mods==0`; deterministic seed-1 оракулы
  пересчитаны независимым reference; полный `ctest` с `GL_GAME_DIR`.
- **Команды проверки:** `cmake --build build`;
  `build\bin\gl_game_tests.exe --test-case="*person_rng*,*npc_loot*"`;
  `GL_SPECIALS_LIVE=<capture> build\bin\gl_game_tests.exe
  --test-case="*live NPC-mod oracle*" --success`;
  `ctest --test-dir build --output-on-failure`.
- **Статус:** `in progress`.
```

- [ ] **Step 2: Проверить кодировку**

```powershell
$p="docs\STATUS.md"; $b=[System.IO.File]::ReadAllBytes($p)
"nul=$((@($b | Where-Object { $_ -eq 0 })).Count) bom=$([BitConverter]::ToString($b[0..2]))"
```

Expected: `nul=0`, BOM-байты не `EF-BB-BF`.

- [ ] **Step 3: Commit (после approve)**

```powershell
git add docs/STATUS.md
git commit -m "docs(status): start per-person rng task"
```

---

### Task 1: `PersonDef` weapon block (assets)

**Files:**
- Modify: `src/assets/include/gl/assets/person_script.hpp`
- Modify: `src/assets/src/person_script.cpp:98-143` (`parse_line`), `:192-207` (inline brace header)
- Test: `tests/assets/test_person_script.cpp`

**Interfaces:**
- Consumes: `parse_person_script(std::span<const std::uint8_t>)`.
- Produces:
  `struct PersonWeapon { std::string name; std::int32_t min_level = 0; std::int32_t max_level = 0; };`
  `PersonDef { bool has_weapon; std::int32_t weapon_level_offset; std::vector<PersonWeapon> weapons; }`.

- [ ] **Step 1: Написать падающие тесты**

Добавить в конец `tests/assets/test_person_script.cpp`:

```cpp
TEST_CASE("person script: weapon block keeps file order and level offset") {
    const std::string text = "res_name: \"L0.P6_Test\"\n"
                             "level: 7\n"
                             "weapon:\n"
                             "{\n"
                             "  level_offset -3\n"
                             "  NPC_HANDS_A 10 20\n"
                             "  NPC_HANDS_B -100 10\n"
                             "  NPC_HANDS_C 1 3\n"
                             "}\n";
    auto result = parse_person_script(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->has_weapon);
    CHECK(result->weapon_level_offset == -3);
    REQUIRE(result->weapons.size() == 3);
    CHECK(result->weapons[0].name == "NPC_HANDS_A");
    CHECK(result->weapons[0].min_level == 10);
    CHECK(result->weapons[0].max_level == 20);
    CHECK(result->weapons[1].name == "NPC_HANDS_B");
    CHECK(result->weapons[2].name == "NPC_HANDS_C");
    CHECK(result->values.at("weapon.NPC_HANDS_A") == "10 20");
    CHECK(result->values.at("weapon.level_offset") == "-3");
}

TEST_CASE("person script: absent weapon block leaves weapon fields clear") {
    auto result = parse_person_script(
        as_bytes("res_name: \"L0.P7_Test\"\nlevel: 3\nstrength: 4\n"));
    REQUIRE(result.has_value());
    CHECK_FALSE(result->has_weapon);
    CHECK(result->weapons.empty());
    CHECK(result->weapon_level_offset == 0);
}

TEST_CASE("person script: inline weapon block skips non-numeric rows") {
    const std::string text = "res_name: \"L0.P8_Test\"\n"
                             "level: 5\n"
                             "weapon: {\n"
                             "  res_name: \"ignored\"\n"
                             "  level: 99\n"
                             "  NPC_OK 1 10 // comment\n"
                             "}\n";
    auto result = parse_person_script(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->has_weapon);
    REQUIRE(result->weapons.size() == 1);
    CHECK(result->weapons[0].name == "NPC_OK");
    CHECK(result->weapons[0].min_level == 1);
    CHECK(result->weapons[0].max_level == 10);
}
```

- [ ] **Step 2: RED**

```powershell
cmake --build build
build\bin\gl_assets_tests.exe --test-case="*person script*"
```

Expected: FAIL — `PersonDef` не имеет `weapons`/`has_weapon`.

- [ ] **Step 3: Расширить заголовок**

В `src/assets/include/gl/assets/person_script.hpp` заменить `struct PersonDef` на:

```cpp
struct PersonWeapon {
    std::string name;
    std::int32_t min_level = 0;
    std::int32_t max_level = 0;
};

struct PersonDef {
    std::string res_name;
    std::int32_t level = 0;
    bool has_weapon = false;
    std::int32_t weapon_level_offset = 0;
    std::vector<PersonWeapon> weapons;
    std::map<std::string, std::string> values;
};
```

Добавить `#include <vector>`.

- [ ] **Step 4: Реализовать парсер**

В `src/assets/src/person_script.cpp`:

4a. Перед `parse_line` добавить helper:

```cpp
bool parse_two_ints(std::string_view value, std::int32_t &first, std::int32_t &second) {
    auto read_int = [&](std::int32_t &out) {
        value = trim(value);
        if (value.empty())
            return false;
        std::int32_t parsed = 0;
        const auto [end, error] =
            std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{} || end == value.data())
            return false;
        out = parsed;
        value.remove_prefix(static_cast<std::size_t>(end - value.data()));
        return true;
    };
    return read_int(first) && read_int(second) && trim(value).empty();
}
```

4b. В `parse_line` в ветке пустого значения:

```cpp
    std::string_view value = trim(line.substr(position));
    if (value.empty()) {
        if (top_level && key == "weapon")
            person.has_weapon = true;
        pending_block = std::string(key);
        return VoidResult{std::monostate{}};
    }
```

4c. После снятия кавычек и `pending_block.clear();` добавить:

```cpp
    const bool in_weapon_block = blocks.size() == 1 && blocks[0] == "weapon";
    if (in_weapon_block) {
        if (key == "level_offset") {
            std::int32_t offset = 0;
            if (parse_level(value, offset))
                person.weapon_level_offset = offset;
        } else {
            PersonWeapon weapon;
            if (parse_two_ints(value, weapon.min_level, weapon.max_level)) {
                weapon.name = std::string(key);
                person.weapons.push_back(std::move(weapon));
            }
        }
    }
```

4d. В `parse_person_script`, в ветке `if (opens > 0)`:

```cpp
            if (opens > 0) {
                const std::string_view key = header_key(line);
                if (depth == 0 && key == "weapon")
                    person.has_weapon = true;
                if (!key.empty())
                    blocks.emplace_back(key);
                else
                    blocks.emplace_back();
```

- [ ] **Step 5: GREEN**

```powershell
cmake --build build
build\bin\gl_assets_tests.exe --test-case="*person script*"
```

Expected: PASS, включая старые person-script тесты.

- [ ] **Step 6: Commit (после approve)**

```powershell
git add src/assets/include/gl/assets/person_script.hpp src/assets/src/person_script.cpp tests/assets/test_person_script.cpp
git commit -m "feat(assets): parse person weapon block"
```

---

### Task 2: `consume_person_init_rng` (game)

**Files:**
- Create: `src/game/include/gl/game/person_rng.hpp`
- Create: `src/game/src/person_rng.cpp`
- Modify: `src/game/CMakeLists.txt` (список `gl_game`)
- Create: `tests/game/test_person_rng.cpp`
- Modify: `tests/game/CMakeLists.txt` (список `gl_game_tests`)

**Interfaces:**
- Consumes: `gl::assets::PersonDef`, `gl::game::MsvcRandom`.
- Produces:
  `struct PersonInitRolls { std::optional<std::size_t> weapon_index; std::int32_t spawn_roll; };`
  `PersonInitRolls consume_person_init_rng(const gl::assets::PersonDef &, MsvcRandom &);`
  `weapon_index` — индекс в `PersonDef.weapons`.

- [ ] **Step 1: Написать падающие тесты**

Создать `tests/game/test_person_rng.cpp`:

```cpp
#include "gl/game/person_rng.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>

using gl::assets::PersonDef;
using gl::assets::PersonWeapon;
using gl::game::consume_person_init_rng;
using gl::game::MsvcRandom;

namespace {
PersonWeapon make_weapon(std::string name, std::int32_t min_level, std::int32_t max_level) {
    return PersonWeapon{std::move(name), min_level, max_level};
}
} // namespace

TEST_CASE("person rng: no weapon block consumes only the spawn draw") {
    PersonDef person;
    person.level = 1;
    MsvcRandom rng(1);
    const auto rolls = consume_person_init_rng(person, rng);
    CHECK_FALSE(rolls.weapon_index.has_value());
    CHECK(rolls.spawn_roll == 3008);
    CHECK(rng.next() == 18467u);
}

TEST_CASE("person rng: first coin success selects the first eligible weapon") {
    PersonDef person;
    person.level = 1;
    person.has_weapon = true;
    person.weapons = {make_weapon("A", 1, 10), make_weapon("B", -100, 10)};

    MsvcRandom rng(1);
    const auto rolls = consume_person_init_rng(person, rng);
    REQUIRE(rolls.weapon_index.has_value());
    CHECK(*rolls.weapon_index == 0u);
    CHECK(rolls.spawn_roll >= 3000);
    CHECK(rolls.spawn_roll <= 10000);
    CHECK(rng.next() == 6334u);
}

TEST_CASE("person rng: failed coin advances to the next eligible weapon") {
    PersonDef person;
    person.level = 20;
    person.weapon_level_offset = -7;
    person.has_weapon = true;
    person.weapons = {make_weapon("HIGH", 18, 200), make_weapon("MID", 12, 22),
                      make_weapon("LOW", -100, 10)};

    MsvcRandom rng(1);
    CHECK(rng.next() == 41u);
    const auto rolls = consume_person_init_rng(person, rng);
    REQUIRE(rolls.weapon_index.has_value());
    CHECK(*rolls.weapon_index == 0u);
    CHECK(rng.next() == 19169u);
}

TEST_CASE("person rng: wrap returns to index zero with one eligible weapon") {
    PersonDef person;
    person.level = 1;
    person.has_weapon = true;
    person.weapons = {make_weapon("ONLY", 1, 10)};

    MsvcRandom rng(1);
    CHECK(rng.next() == 41u);
    const auto rolls = consume_person_init_rng(person, rng);
    REQUIRE(rolls.weapon_index.has_value());
    CHECK(*rolls.weapon_index == 0u);
    CHECK(rng.next() == 19169u);
}

TEST_CASE("person rng: two eligible weapons can select the second index") {
    PersonDef person;
    person.level = 1;
    person.has_weapon = true;
    person.weapons = {make_weapon("A", 1, 10), make_weapon("B", 1, 10)};

    MsvcRandom rng(1);
    CHECK(rng.next() == 41u);
    const auto rolls = consume_person_init_rng(person, rng);
    REQUIRE(rolls.weapon_index.has_value());
    CHECK(*rolls.weapon_index == 1u);
    CHECK(rng.next() == 19169u);
}

TEST_CASE("person rng: weapon block without eligible rows consumes no weapon draw") {
    PersonDef person;
    person.level = 500;
    person.has_weapon = true;
    person.weapons = {make_weapon("LOW", 1, 10)};

    MsvcRandom rng(1);
    const auto rolls = consume_person_init_rng(person, rng);
    CHECK_FALSE(rolls.weapon_index.has_value());
    CHECK(rolls.spawn_roll == 3008);
    CHECK(rng.next() == 18467u);
}
```

- [ ] **Step 2: RED**

```powershell
cmake --build build
```

Expected: FAIL — нет `gl/game/person_rng.hpp`.

- [ ] **Step 3: Создать заголовок**

`src/game/include/gl/game/person_rng.hpp`:

```cpp
#pragma once
#include "gl/assets/person_script.hpp"
#include "gl/game/msvc_random.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace gl::game {
struct PersonInitRolls {
    std::optional<std::size_t> weapon_index;
    std::int32_t spawn_roll = 0;
};

PersonInitRolls consume_person_init_rng(const gl::assets::PersonDef &person, MsvcRandom &rng);
} // namespace gl::game
```

- [ ] **Step 4: Создать реализацию**

`src/game/src/person_rng.cpp`:

```cpp
#include "gl/game/person_rng.hpp"

#include "gl/assets/log.hpp"

#include <string>
#include <vector>

namespace gl::game {
namespace {
constexpr std::int32_t kSpawnDelayLow = 3000;
constexpr std::int32_t kSpawnDelayHigh = 10000;

std::vector<std::size_t> eligible_weapons(const gl::assets::PersonDef &person) {
    std::vector<std::size_t> eligible;
    for (std::size_t index = 0; index < person.weapons.size(); ++index) {
        const gl::assets::PersonWeapon &weapon = person.weapons[index];
        if (weapon.min_level + person.weapon_level_offset <= person.level &&
            person.level <= weapon.max_level + person.weapon_level_offset)
            eligible.push_back(index);
    }
    return eligible;
}
} // namespace

PersonInitRolls consume_person_init_rng(const gl::assets::PersonDef &person, MsvcRandom &rng) {
    PersonInitRolls rolls;
    if (person.has_weapon) {
        const std::vector<std::size_t> eligible = eligible_weapons(person);
        if (eligible.empty()) {
            gl::log(gl::LogLevel::warning,
                    "person rng: '" + person.res_name +
                        "' has a weapon block without eligible entries; no weapon draw");
        } else {
            std::size_t index = 0;
            while (!rng.coin(0.5)) {
                index = (index + 1) % eligible.size();
            }
            rolls.weapon_index = eligible[index];
        }
    }
    rolls.spawn_roll = rng.range(kSpawnDelayLow, kSpawnDelayHigh);
    return rolls;
}
} // namespace gl::game
```

- [ ] **Step 5: Подключить к сборке**

В `src/game/CMakeLists.txt` добавить строку `src/person_rng.cpp` после `src/person_runtime.cpp`.
В `tests/game/CMakeLists.txt` добавить `test_person_rng.cpp` после `test_person_runtime_real.cpp`.

- [ ] **Step 6: GREEN**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*person rng*"
```

Expected: PASS (6 cases).

- [ ] **Step 7: Commit (после approve)**

```powershell
git add src/game/include/gl/game/person_rng.hpp src/game/src/person_rng.cpp src/game/CMakeLists.txt tests/game/test_person_rng.cpp tests/game/CMakeLists.txt
git commit -m "feat(game): consume person weapon and spawn rng"
```

---

### Task 3: Интеграция в `generate_npc_inventories`

**Files:**
- Modify: `src/game/src/npc_loot.cpp:142-196`
- Test: `tests/game/test_inventory_runtime.cpp` (рядом с тестом `generate_npc_inventories stores pass-1 hot...`)

**Interfaces:**
- Consumes: `consume_person_init_rng`, `PersonInstance.def`.
- Produces: `generate_npc_inventories` потребляет draw для каждого не-Hero `level.persons` в SEF-порядке.

- [ ] **Step 1: Написать падающие тесты**

Добавить в `tests/game/test_inventory_runtime.cpp` после существующего теста `generate_npc_inventories stores pass-1 hot as npc trade items`:

```cpp
TEST_CASE("generate_npc_inventories consumes weapon and spawn draws without inventory") {
    struct ScopedDir {
        std::filesystem::path path;
        ~ScopedDir() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    };
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    ScopedDir dir{std::filesystem::temp_directory_path() /
                  ("gl_person_rng_inv_" + std::to_string(stamp))};
    std::filesystem::create_directories(dir.path);

    auto vfs = gl::assets::GameVfs::mount(dir.path);
    REQUIRE(vfs.has_value());
    auto runtime = InventoryRuntime::create({}, make_names(), template_for);

    PersonInstance person;
    person.name = "L0.NoInv";
    person.def.level = 1;
    person.def.has_weapon = true;
    person.def.weapons = {{"A", 1, 10}, {"B", 1, 10}};

    MsvcRandom rng(1);
    generate_npc_inventories(runtime, std::vector<PersonInstance>{person}, vfs.value(),
                             make_names(), template_for, 1, rng);

    CHECK(rng.next() == 6334u);
}

TEST_CASE("generate_npc_inventories consumes duplicate person rng twice") {
    struct ScopedDir {
        std::filesystem::path path;
        ~ScopedDir() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    };
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    ScopedDir dir{std::filesystem::temp_directory_path() /
                  ("gl_person_rng_dup_" + std::to_string(stamp))};
    std::filesystem::create_directories(dir.path);

    auto vfs = gl::assets::GameVfs::mount(dir.path);
    REQUIRE(vfs.has_value());
    auto runtime = InventoryRuntime::create({}, make_names(), template_for);

    PersonInstance person;
    person.name = "L0.Dup";
    person.def.level = 1;
    person.def.has_weapon = true;
    person.def.weapons = {{"A", 1, 10}};

    MsvcRandom rng(1);
    generate_npc_inventories(runtime, std::vector<PersonInstance>{person, person}, vfs.value(),
                             make_names(), template_for, 1, rng);

    CHECK(rng.next() == 19169u);
    CHECK(runtime.person_items("L0.Dup").empty());
}

TEST_CASE("generate_npc_inventories runs loot draws before person draws") {
    struct ScopedDir {
        std::filesystem::path path;
        ~ScopedDir() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    };
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    ScopedDir dir{std::filesystem::temp_directory_path() /
                  ("gl_person_rng_order_" + std::to_string(stamp))};
    const std::filesystem::path inv_path = dir.path / "scripts" / "inventory" / "empty_test.inv";
    std::filesystem::create_directories(inv_path.parent_path());
    {
        std::ofstream out(inv_path, std::ios::binary);
        REQUIRE(out.good());
        out << "regenerate_chance 75\n"
               "chance_decrease 1\n"
               "quantity_decrease 1\n";
    }

    auto vfs = gl::assets::GameVfs::mount(dir.path);
    REQUIRE(vfs.has_value());
    auto runtime = InventoryRuntime::create({}, make_names(), template_for);

    PersonInstance person;
    person.name = "L0.Order";
    person.inventory = "empty_test";
    person.def.level = 1;
    person.def.has_weapon = true;
    person.def.weapons = {{"A", 1, 10}, {"B", 1, 10}};

    MsvcRandom rng(1);
    generate_npc_inventories(runtime, std::vector<PersonInstance>{person}, vfs.value(),
                             make_names(), template_for, 1, rng);

    CHECK(rng.next() == 19169u);
}
```

- [ ] **Step 2: RED**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*generate_npc_inventories consumes*"
```

Expected: FAIL — draw counts не совпадают (person draws отсутствуют).

- [ ] **Step 3: Переписать цикл**

В `src/game/src/npc_loot.cpp` добавить `#include "gl/game/person_rng.hpp"`.
Заменить тело цикла в `generate_npc_inventories` на:

```cpp
    for (const PersonInstance &person : persons) {
        if (person.name == "Hero")
            continue;

        if (!person.inventory.empty()) {
            auto cached = cache.find(person.inventory);
            if (cached == cache.end()) {
                auto table = gl::assets::load_inv(vfs, person.inventory);
                if (!table) {
                    gl::log(gl::LogLevel::warning,
                            "npc inventory: cannot load '" + person.inventory +
                                "': " + table.error().message);
                    cached = cache.emplace(person.inventory, std::nullopt).first;
                } else {
                    cached = cache.emplace(person.inventory, std::move(table.value())).first;
                }
            }
            if (cached->second) {
                const NpcLoot loot = NpcLootGenerator::generate(*cached->second, loot_level, names,
                                                                item_template, rng, specials);
                std::vector<gl::assets::InventoryItem> hot;
                hot.reserve(loot.hot.size());
                for (const LootItem &item : loot.hot) {
                    gl::assets::InventoryItem converted{item.item_id,
                                                        static_cast<std::uint32_t>(item.count),
                                                        item.flags,
                                                        item.type,
                                                        item.style,
                                                        100,
                                                        item.mod_count};
                    converted.mods = item.mods;
                    hot.push_back(std::move(converted));
                }
                std::vector<gl::assets::InventoryItem> main;
                main.reserve(loot.main.size());
                for (const LootItem &item : loot.main) {
                    gl::assets::InventoryItem converted{item.item_id,
                                                        static_cast<std::uint32_t>(item.count),
                                                        item.flags,
                                                        item.type,
                                                        item.style,
                                                        100,
                                                        item.mod_count};
                    converted.mods = item.mods;
                    main.push_back(std::move(converted));
                }
                runtime.set_person_trade_items(person.name, std::move(hot));
                runtime.set_person_items(person.name, std::move(main));
            }
        }

        consume_person_init_rng(person.def, rng);
    }
```

- [ ] **Step 4: GREEN**

```powershell
cmake --build build
build\bin\gl_game_tests.exe --test-case="*generate_npc_inventories*"
```

Expected: PASS, включая прежний trade-hot тест.

- [ ] **Step 5: Commit (после approve)**

```powershell
git add src/game/src/npc_loot.cpp tests/game/test_inventory_runtime.cpp
git commit -m "feat(game): consume person rng after npc loot"
```

---

### Task 4: Live gate wiring + live 0/N

**Files:**
- Modify: `tests/game/test_special_gates.cpp:553-626` (`build_port_records`)
- Verify only (not committed): `.local/live/` capture copy под `%TEMP%`

**Interfaces:**
- Consumes: `consume_person_init_rng`.
- Produces: live oracle проверяет полный per-person RNG-порядок.

- [ ] **Step 1: Обновить `build_port_records`**

Добавить `#include "gl/game/person_rng.hpp"`.
Заменить цикл на:

```cpp
    for (const gl::game::PersonInstance &person : level->persons) {
        if (person.name == "Hero")
            continue;
        if (!person.inventory.empty()) {
            ++port.generated_persons;
            port.order.push_back(person.name + "(" + person.inventory + ")");
            auto cached = cache.find(person.inventory);
            if (cached == cache.end()) {
                auto table = gl::assets::load_inv(vfs.value(), person.inventory);
                if (!table)
                    cached = cache.emplace(person.inventory, std::nullopt).first;
                else
                    cached = cache.emplace(person.inventory, std::move(table.value())).first;
            }
            if (cached->second) {
                const gl::game::NpcLoot loot = gl::game::NpcLootGenerator::generate(
                    *cached->second, capture.loot_level, names.value(), lookup, rng,
                    &specials.value());
                if (!filter_to_captured || captured_persons.contains(person.name)) {
                    for (const gl::game::LootItem &item : loot.main) {
                        port.items.insert(CapturedItem{person.name, item.item_id, item.count,
                                                       item.flags, item.mod_count});
                        for (const gl::assets::SpecialMod &mod : item.mods) {
                            port.mods.insert(CapturedMod{
                                person.name, item.item_id, item.count, item.flags,
                                static_cast<std::int32_t>(mod.id), mod.value, mod.flags});
                        }
                    }
                }
            }
        }
        gl::game::consume_person_init_rng(person.def, rng);
    }
```

- [ ] **Step 2: Собрать**

```powershell
cmake --build build
```

Expected: exit 0.

**Decision 2026-11-11 (user-approved):** committed capture теряет duplicate
person names (`tools/live/gl_live_capture.cpp:926-930`, first-wins), а
`InventoryRuntime` тоже first-wins по имени. Поэтому фактический
`build_port_records` ведёт `std::set<std::string> inserted_persons`: каждый
SEF-персонаж по-прежнему генерирует loot и потребляет
`consume_person_init_rng`, но items/mods в oracle вставляет только первый
экземпляр имени; `generated_persons` считает уникальные inventory-имена.
Это соответствует `tests/game/test_special_gates.cpp` после `e785091` и
даёт `items 22/22, mods 14/14, 0/0`.

- [ ] **Step 3: Подготовить capture без pre-fix артефакта**

```powershell
$src=".local\live\special-l1_1-trace.txt"
$tmp="$env:TEMP\gl_special_l1_1_fixed.txt"
Get-Content $src | Where-Object { $_ -notmatch '^# rng_second=' } |
    Set-Content -LiteralPath $tmp -Encoding ascii
Select-String -Path $tmp -Pattern "rng_second|rng_state|loot_level" |
    ForEach-Object { $_.Line }
```

Expected: строки `rng_second` нет; `rng_state=0xad5bd7cd`, `loot_level=2` есть.
Это удаление известного tool-артефакта (research §0/§7), не ослабление проверки.

- [ ] **Step 4: Прогнать live oracle**

```powershell
$env:GL_SPECIALS_LIVE="$env:TEMP\gl_special_l1_1_fixed.txt"
build\bin\gl_game_tests.exe --test-case="*live NPC-mod oracle*" --success
```

Expected: PASS; в MESSAGE
`items captured=22 port=22 matched=22 mismatched=0; mods captured=14 port=14 matched=14 mismatched=0`,
включая P678 `812:139 + 420:1`, P692 `812:67`, P693 `938:1`.
Если mismatches не 0 — STOP, записать фактические mismatch-строки в STATUS как BLOCKED; не подгонять.

- [ ] **Step 5: Полный scoped-прогон game-тестов**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\gl_game_tests.exe --test-case="*special gates*,*person rng*,*npc_loot*"
```

Expected: PASS (или только ожидаемые deterministic-oracle failures из Task 5).

- [ ] **Step 6: Commit (после approve)**

```powershell
git add tests/game/test_special_gates.cpp
git commit -m "test(game): consume person rng in live oracle"
```

---

### Task 5: Пересчитать deterministic-оракулы независимым reference

**Files:**
- Modify: `tests/game/test_npc_loot_specials.cpp`
- Modify: `tests/game/test_inventory_runtime_real.cpp`
- Modify (только если упадут): `tests/script/test_real_transfer_natives.cpp`,
  `tests/app/test_glgame_screenshot.cpp`, `tests/app/test_trade_overlay.cpp`
- Reference (not committed): `%TEMP%\gl_person_rng_ref.py`

**Interfaces:**
- Consumes: live-валидированный reference из Task 4.
- Produces: seed-1 deterministic-ожидания, объяснённые reference-output.

- [ ] **Step 1: Проверить наличие reference-базы**

```powershell
Test-Path "$env:TEMP\gl_stage2b\sim.py"
Test-Path "$env:TEMP\gl_stage2b\data.json"
```

Expected: `True`/`True`. Если нет — STOP/BLOCKED: не выводить оракул из C++-порта.

- [ ] **Step 2: Создать расширенный reference**

Скопировать `sim.py` в `%TEMP%\gl_person_rng_ref.py` и применить правки:

```python
# 1) после импортов: создание записи в оригинале не потребляет RNG
CREATION_DRAW = 0
# 2) переключатель specials для no-specials эталона
USE_SPECIALS = True
```

Заменить оба условия `if chunk==1 and (flags&4) and (len(it['effects'])==0):`
на `if USE_SPECIALS and chunk==1 and (flags&4) and (len(it['effects'])==0):`.

Добавить парсер/потребитель:

```python
PERSONS_DIR = os.environ.get('GL_PERSON_SCRIPTS', os.path.join(TMP, 'persons'))

def parse_person_weapons(name):
    path = os.path.join(PERSONS_DIR, name.lower() + '.scr')
    text = open(path, 'rb').read().decode('cp1251', 'replace')
    level = 0
    has_weapon = False
    offset = 0
    weapons = []
    in_weapon = False
    for raw in text.splitlines():
        line = raw.split('//', 1)[0].strip()
        if not line:
            continue
        if not in_weapon:
            m = re.match(r'level\s*:\s*(-?\d+)', line)
            if m and level == 0:
                level = int(m.group(1))
            if re.match(r'weapon\s*:', line):
                has_weapon = True
                if '{' in line:
                    in_weapon = True
                continue
            if line == '{' and has_weapon and not weapons:
                in_weapon = True
            continue
        if line.startswith('}'):
            in_weapon = False
            continue
        m = re.match(r'level_offset\s+(-?\d+)', line)
        if m:
            offset = int(m.group(1))
            continue
        m = re.match(r'(\S+)\s+(-?\d+)\s+(-?\d+)$', line)
        if m:
            weapons.append((m.group(1), int(m.group(2)), int(m.group(3))))
    return level, has_weapon, offset, weapons

def consume_person(rng, level, has_weapon, offset, weapons):
    eligible = [w for w in weapons if w[1] + offset <= level <= w[2] + offset]
    picked = None
    if has_weapon and eligible:
        i = 0
        while not rng.coin(0.5):
            i = (i + 1) % len(eligible)
        picked = eligible[i][0]
    roll = rng.range(3000, 10000)
    return picked, roll
```

В `generate(...)` перед циклом persons добавить:

```python
    PERSONS_META = [parse_person_weapons(pname) for pname, _ in PERSONS]
```

После `per_person.setdefault(pname, cont_main)` и
`LAST_CREATES[pi]=(pname,len(cont_hot),len(cont_main))` (основной путь,
`cd/qd != 0` в retail) добавить:

```python
        level_p, has_w, off_p, weapons_p = PERSONS_META[pi]
        consume_person(rng, level_p, has_w, off_p, weapons_p)
```

Ранний `continue` при `cd==0 or qd==0` в retail не встречается; если
захочется полноты — вынести store+consume в helper и вызвать перед ним.

- [ ] **Step 3: Извлечь person-скрипты для reference**

```powershell
$out="$env:TEMP\gl_stage2b\persons"
New-Item -ItemType Directory -Force -Path $out | Out-Null
foreach($f in @('l1_1.p387_poison_plotnik','l0.p678_citizen4','l0.p7_kolbasens_rodstvennik1',
  'l1_1.p389_crying_guardian','l1_2.p404_sister_of_mercy2','l0.p785_witch3',
  'l1_2.p403_sister_of_mercy1','l0.p693_citizen6','l0.p711_citizen8','l0.p692_citizen5',
  'l0.p744_oficer1','l0.p640_children2','l1.p621_svet_3circusworkerv','l0.p733_artist1',
  'l1.p620_svet_3circusworker2')){
  build\bin\glpak.exe extract "I:\Games\Златогорье-2" --file "scripts/persons/$f.scr" --out $out | Out-Null
}
$env:GL_PERSON_SCRIPTS="$out\scripts\persons"
(Get-ChildItem $env:GL_PERSON_SCRIPTS -Filter *.scr).Count
```

Expected: `15` (одна копия P744 — оба вызова используют одинаковый скрипт).

- [ ] **Step 4: Валидировать reference на live**

```powershell
python "$env:TEMP\gl_person_rng_ref.py" 0 2
```

Expected: для всех 16 persons `PORT == LIVE` по items (включая P678
`812:139 + 420:1`, P692 `812:67`, P693 `938:1`); `merge=True`.
Если нет — STOP/BLOCKED.

- [ ] **Step 5: Получить seed-1 level-1 эталон**

Заменить `if __name__=='__main__':` блок в reference на:

```python
if __name__ == '__main__':
    for use_specials in (True, False):
        USE_SPECIALS = use_specials
        for seed, loot_level in ((1, 1), (0xad5bd7cd, 2)):
            pp, rng = generate(seed, loot_level, 0, True)
            print('specials', use_specials, 'seed', seed, 'loot', loot_level,
                  'final_state', hex(rng.s))
            for pname, _ in PERSONS:
                rows = pp.get(pname, [])
                money = sum(r['count'] for r in rows if NAMES.get('MON_1_0_1') == r['id'])
                potion = sum(r['count'] for r in rows if NAMES.get('POT_1_00_27') == r['id'])
                print(pname, 'rows', [(r['id'], r['count'], r['flags'], len(r['mods']))
                                      for r in rows], 'money', money, 'potion', potion)
```

`USE_SPECIALS` присваивается на module level, поэтому виден внутри
`generate`. Ожидается:
`specials True seed 0xad5bd7cd loot 2` совпадает с live (Step 4);
`specials True/False seed 1 loot 1` дают эталонные `final_state`,
per-person rows/money/potion для обновления
`test_npc_loot_specials.cpp`/`test_inventory_runtime_real.cpp`.
Reference-output фиксируется в отчёте задачи; только эти значения
переносятся в тесты.

- [ ] **Step 6: Обновить `test_npc_loot_specials.cpp`**

Заменить изменившиеся ожидания на reference-значения:
`final_state`, `count_to_state`, `candidates`, `triggers`, `generated_mods`,
`events` (person/pass/tech/mods), `r5_draws`, `r6_draws`, `kolbasens_mon`.
Не менять структуру проверок и формулы (`candidates + r5_draws == ...`).

- [ ] **Step 7: Обновить `test_inventory_runtime_real.cpp`**

Заменить NPC-ожидания (`P7` money, `P389`, `P711`, `trade_money`,
`POT_1_00_27`, прочие затронутые) на reference-значения seed-1 level-1.
Hero-ожидания не менять.

- [ ] **Step 8: Прогнать real-тесты**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
build\bin\gl_game_tests.exe --test-case="*inventory_runtime*npc*,*npc_loot specials*,*transfer*"
```

Expected: PASS.

- [ ] **Step 9: Commit (после approve)**

```powershell
git add tests/game/test_npc_loot_specials.cpp tests/game/test_inventory_runtime_real.cpp
# добавить прочие поправленные оракулы, если они упали
git commit -m "test(game): refresh npc loot oracles for person rng"
```

---

### Task 6: Полный `ctest`, docs, parity

**Files:**
- Modify: `docs/DECISIONS.md` (D-064)
- Modify: `docs/STATUS.md` (Phase 1b2b2f: done/partial + проверка)
- Modify: `docs/PROJECT.md` (P4/P5 status blurb)
- Modify: `docs/research/p3b2b2e-live-parity-stage2b.md` (§9)
- Modify: `docs/specs/2026-11-11-p3b2b2f-person-rng-design.md` (Status)
- Modify: `docs/research/parity-matrix.md` (затронутые строки)

**Interfaces:**
- Consumes: результаты Task 1–5.
- Produces: документально закрытая задача.

- [ ] **Step 1: Полный build + ctest**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: `100% tests passed out of N`, exit 0. Записать реальное N и время.
Если упали screenshot/trade оракулы — обновить их из reference/посчитанного
состояния и повторить (не ослаблять).

- [ ] **Step 2: D-064**

Добавить в `docs/DECISIONS.md`:

```markdown
| D-064 | 2026-11-11 | Phase 1b2b2f - per-person pre-loot RNG. `FUN_1401d99c` selects the initial weapon by `coin(0.5)` over the person-script `weapon`-block entries eligible by `min+level_offset <= level <= max+level_offset` (file order, wrap on failure), then `FUN_14021bc8` consumes one `range(3000,10000)` spawn-init draw; order per SEF person is loot -> weapon -> spawn. Port: `PersonDef.weapons/weapon_level_offset/has_weapon`, `game::consume_person_init_rng`, `generate_npc_inventories` consumes for every non-Hero `level.persons` (empty inventory and duplicates included). Equipment materialization/`spawn_roll` storage/Hero spawn/post-generation RNG out of scope. Live l1_1 oracle `mismatched_items=0/mismatched_mods=0`; deterministic oracles re-derived by the independent Python reference validated on the live capture. | spec 1b2b2f; research stage2b §8; D-063. |
```

- [ ] **Step 3: Research §9**

Добавить в `docs/research/p3b2b2e-live-parity-stage2b.md` секцию:

```markdown
## 9. Per-person RNG model (2026-11-11)

Weapon (`FUN_1401d99c`, caller RVA `0x1dd4e`) + spawn init
(`FUN_14021bc8`, caller RVA `0x221c0`) are consumed after each person's
loot: `coin(0.5)` over the person-script `weapon` block (file order,
`min+offset <= level <= max+offset`, wrap) then one `range(3000,10000)`.
Retail corpus: 782/782 person scripts have a weapon block with >=1 eligible
row. Implemented in `game::consume_person_init_rng`; live l1_1 gate is
`items 22/22, mods 14/14` (0 mismatched).
```

- [ ] **Step 4: STATUS/PROJECT/spec/parity**

- STATUS: Phase 1b2b2f -> done, вписать фактический ctest-вывод, live-числа,
  residual limitations (materialization/spawn storage/missing-resource persons).
- PROJECT: P4/P5 blurb — RNG-поток NPC теперь включает weapon+spawn.
- Spec Status: `implemented`.
- parity-matrix: обновить строки RNG/NPC loot, где меняется статус; пересчитать summary.

- [ ] **Step 5: Проверить документы**

```powershell
foreach($p in @("docs\DECISIONS.md","docs\STATUS.md","docs\PROJECT.md",
  "docs\research\p3b2b2e-live-parity-stage2b.md","docs\specs\2026-11-11-p3b2b2f-person-rng-design.md",
  "docs\research\parity-matrix.md")){
  $b=[System.IO.File]::ReadAllBytes($p)
  "$p nul=$((@($b | Where-Object { $_ -eq 0 })).Count) bom=$([BitConverter]::ToString($b[0..2]))"
}
```

Expected: `nul=0`, без BOM.

- [ ] **Step 6: Commit (после approve)**

```powershell
git add docs/DECISIONS.md docs/STATUS.md docs/PROJECT.md docs/research/p3b2b2e-live-parity-stage2b.md docs/specs/2026-11-11-p3b2b2f-person-rng-design.md docs/research/parity-matrix.md
git commit -m "docs: record per-person rng parity"
```

---

## Self-review notes

- Spec §4.1/4.2/4.3 покрыты Task 1/2/3; live acceptance — Task 4;
  deterministic oracle provenance — Task 5; docs — Task 6.
- Типы: `PersonWeapon`, `PersonDef.weapons`, `PersonInitRolls.weapon_index`
  (индекс в `PersonDef.weapons`) согласованы между Task 1, 2, 3, 4.
- `gl_assets_real_tests` может потребовать обновления только если реальные
  данные меняются (person script parser); Task 5 Step 8/6 Step 1 ловят это.
- Вне scope: материализация оружия, `spawn_roll` в состоянии, Hero spawn,
  пост-спавн RNG, паритет пропущенных загрузчиком persons.
