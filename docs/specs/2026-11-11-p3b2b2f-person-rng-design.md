# Phase 1b2b2f — Per-person pre-loot RNG (weapon + spawn init) Design

Status: implemented (2026-11-11)
Phase: 1b2b2f (follow-up to 1b2b2e Part 2)
Layer: `assets` (person script) + `game` (RNG consumer) + `game` (NPC loot wiring)

## 1. Goal

Выровнять общий CRT `_rand` поток порта с оригиналом на участке
инициализации SEF-персонажей, чтобы NPC-loot oracle `l1_1` достиг
`mismatched_items == 0 && mismatched_mods == 0` (live gate).

Оригинал между loot-вызовами `FUN_1404b1c0` потребляет переменное число
draw на каждого персонажа: выбор начального оружия
(`FUN_1401d99c`, `coin(0.5)` до успеха) и spawn init
(`FUN_14021bc8`, `range(+0x5e4,+0x5e8)`). Порт эти системы не моделирует,
поэтому loot-поток сдвинут на 2–8 draw на персонажа (D-063, research §8).

## 2. Evidence / provenance

| Artifact | Value |
|---|---|
| `Server.dll` | SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | SHA-256 `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| live draw trace `.local/live/special-start-draws5.txt` | SHA-256 `A9358B902CB41FB35B82BBF5B308CD7A358AB5D6F955BCDEE2C3EEA6C251393B` |
| live l1_1 capture `.local/live/special-l1_1-trace.txt` | 16 calls, `rng_state=0xad5bd7cd`, `loot_level=2` (pre-fix tool artifact `rng_second check=fail`, known) |

Facts extracted from `Server.dll` (Ghidra decompile):

- `FUN_1401d99c` RVA `0x1d99c` (2466 B, callers 3), decompiled.c:21303+.
  Разбирает `scripts/persons/<name>.scr`; `weapon`-блок: 21397–21424,
  выбор — 21434–21448. Draw site caller RVA `0x1dd4e`
  (`FUN_14006288` coin).
- `FUN_140061e4` (int `range`) RVA `0x61e4`: `hi<=lo` → 0 draw; иначе
  один `_rand` (decompiled.c:4347–4358).
- `FUN_14006288` (coin) RVA `0x6288`: `p<=0`/`p>1` → 0 draw; иначе один
  `_rand` (decompiled.c:4385–4400).
- `FUN_14021bc8` RVA `0x21bc8`: spawn init; states 0/1 вызывают
  `FUN_140061e4(this+0x5e4, this+0x5e8)` (decompiled.c:24769, 24783);
  поля инициализируются `3000`/`10000` (decompiled.c:24183, 24185).
  Draw site caller RVA `0x221c0`.
- Порядок на SEF-персонажа (decompiled.c:22440–22491): обработка ключей
  блока (`scr_inv` → `FUN_1404b1c0`, 22451–22455) → `FUN_1401d99c`
  (weapon, 22484) → spawn (`FUN_14021658`/`FUN_140222d0`, 22488–22491).
  То есть per person: **loot → weapon → spawn**.
- Live trace подтверждает порядок: call 1..8 содержат loot-draw, затем
  в конце call — `0x1dd4e`×N (weapon) и ровно один `0x221c0` (spawn);
  tail extras отсутствуют между loot-вызовами как отдельные системы.
- Weapon eligibility: `min_level + level_offset <= person.level <=
  max_level + level_offset` (decompiled.c:21411–21414), строки блока в
  файловом порядке, до 32 записей.
- Retail corpus: 782 `scripts/persons/*.scr`, у всех есть `weapon`-блок
  и ≥1 eligible строка для своего `level`/`level_offset`.
- `l1_1` live phase gaps (research §5) = weapon draws (geometric, p=0.5)
  + 1 spawn draw: `2,3,4,2,2,3,7,2,4,3,?,4,2,3,3` для 15 из 16 persons
  (дубликат `P744` не искался).

## 3. Non-goals

- Материализация выбранного оружия в экипировке/инвентаре NPC.
- Хранение/использование `spawn_roll` в состоянии персонажа.
- Hero spawn и пост-спавн AI/movement RNG (вне loot-окна).
- Паритет персонажей, которых текущий загрузчик пропустил из-за
  отсутствующего PAD/спрайта/скрипта (warning + документированное
  ограничение; официальные данные `l1_1` загружают все 16).
- Изменение window/quantity/merge/specials/precision моделей.

## 4. Architecture and components

### 4.1 `assets` — person script weapon block

`src/assets/include/gl/assets/person_script.hpp`:

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

`parse_person_script` сохраняет строки `weapon`-блока в файловом порядке;
`level_offset` — отдельное поле (не строка в `weapons`). Существующий
`values`-map не меняется, поэтому прежние потребители/тесты не ломаются.
Строки вида `NAME min max` вне `weapon`-блока игнорировать как раньше.

### 4.2 `game` — consumer RNG

`src/game/include/gl/game/person_rng.hpp`:

```cpp
struct PersonInitRolls {
    std::optional<std::size_t> weapon_index;
    std::int32_t spawn_roll = 0;
};

PersonInitRolls consume_person_init_rng(const gl::assets::PersonDef &person,
                                        MsvcRandom &rng);
```

`src/game/src/person_rng.cpp` semantics:

1. `eligible` — индексы `weapons`, где
   `min_level + weapon_level_offset <= person.level <=
   max_level + weapon_level_offset`, в файловом порядке.
2. Если `has_weapon && !eligible.empty()`: повторять `rng.coin(0.5)`,
   начиная с индекса 0; при неудаче индекс=`(index+1) % eligible.size()`;
   при успехе вернуть этот `weapon_index` (индекс в `PersonDef.weapons`).
   Число draw = номер первого успеха (geometric, p=0.5); wrap совпадает
   с decompiled-циклом 21434–21448.
3. Всегда `spawn_roll = rng.range(3000, 10000)` — ровно один draw
   (`FUN_14021bc8` states 0/1, bounds 24183/24185).
4. Guard: `has_weapon && eligible.empty()` → warning, 0 weapon draw,
   `weapon_index = nullopt`. В retail-корпусе недостижимо (782/782
   имеют eligible); guard документируется как port-side защита, не
   паритет.

### 4.3 Интеграция `generate_npc_inventories`

`src/game/src/npc_loot.cpp`: цикл по `persons` (уже `level.persons` в
SEF-порядке) меняется так:

- Hero (если встретится) пропускается полностью;
- если `inventory` не пуст — загрузить/закешировать `InvTable` и
  сгенерировать loot (как сейчас, оба прохода + merge + specials);
- **после** loot для любого не-Hero персонажа вызвать
  `consume_person_init_rng(person.def, rng)`;
- персонажи без инвентаря тоже потребляют weapon+spawn draw;
- дубликаты имени (`L0.P744_oficer1`) потребляют draw дважды; запись в
  `InventoryRuntime` остаётся first-wins (существующее поведение).

Порядок per person: `loot draws → weapon draws → spawn draw`.

## 5. Data flow (l1_1, 16 SEF persons)

```text
rng_state (captured / seed)
  for each SEF person i:
    if scr_inv: NpcLootGenerator::generate(...)   // R1 + pass1 + pass2 + specials + merge
    consume_person_init_rng(person.def, rng)      // coin(0.5)... + range(3000,10000)
  LevelSimulation::create(level, rng)             // post-generation, вне loot-окна
```

Ожидаемые draw-гэпы между сохраняемыми main-контейнерами совпадают с
live trace. Никаких фиксированных offset/подгонок не вводится.

## 6. Testing

Unit (`tests/game/test_person_rng.cpp` + assets test):

- parser: weapon-строки в файловом порядке, `level_offset`, `has_weapon`,
  отсутствие блока, невалидные строки, trailing `//`-комментарии;
- eligibility: offset/level включают/исключают строки (P678 level 1 → 2,
  P7 level 20 offset -7 → 1);
- consumer: для известного seed точный `weapon_index`, число draw и
  `spawn_roll`; `coin` fail→success, wrap при n=1; пустой eligible guard;
- порядок в `generate_npc_inventories`: person без инвентаря всё равно
  двигает RNG; duplicate name двигает RNG дважды, запись first-wins.

Real/deterministic:

- Независимый Python reference (MSVC LCG, loot-модель + weapon/spawn)
  прогоняется на живом capture (`rng_state=0xad5bd7cd`, l1_1, level 2) и
  обязан дать live 22 items / 14 mods 0/N. Только после этого
  пересчитываются seed-1 `174/218/323`-производные оракулы
  (`tests/game/test_inventory_runtime_real.cpp`, `test_npc_loot_specials.cpp`,
  trade app-level или другие затронутые) — значения из reference, не из
  C++-порта.
- Live gate (`GL_SPECIALS_LIVE=...`) на `l1_1`:
  `mismatched_items == 0 && mismatched_mods == 0`.
  Gate mirror-ит first-wins duplicate-семантику capture-tool и
  `InventoryRuntime`: каждый SEF-персонаж потребляет RNG, но записи
  вставляет только первый экземпляр имени (P744-дубликат).
  Для приёмки использовать re-capture с исправленным tool; при
  отсутствии — существующий capture с удалённой pre-fix строкой
  `# rng_second=... check=fail` (известный tool-артефакт, не ослабление
  проверки; research §0/§7). Это решение фиксируется в отчёте.

Acceptance commands:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_game_tests.exe --test-case="*person_rng*,*npc_loot*,*special*"
ctest --test-dir build --output-on-failure
```

## 7. Acceptance criteria

1. `PersonDef` отдаёт weapon-строки в файловом порядке и `level_offset`;
   unit-тесты зелёные.
2. `consume_person_init_rng` воспроизводит geometric `coin(0.5)` +
   один `range(3000,10000)`; guard для пустого eligible залогирован.
3. `generate_npc_inventories` потребляет draw для всех `level.persons` в
   SEF-порядке, включая персонажей без инвентаря и дубликаты.
4. Live l1_1 oracle: `mismatched_items == 0 && mismatched_mods == 0`
   (минимум P678 `812:139 + 420:1(6 mods)`, P692 `812:67`,
   P693 `938:1(2 mods)`).
5. Полный `ctest` с `GL_GAME_DIR` — 100% passed; docs (`DECISIONS`,
   `STATUS`, `PROJECT`, research/parity) обновлены.

## 8. Risks / limitations

- `level.persons` может не содержать персонажа, пропущенного загрузчиком
  (PAD/спрайт/скрипт). Официальные `l1_1` данные содержат все 16;
  расхождение логируется и документируется, не маскируется.
- Weapon draw count не зависит от `eligible.size()` при `size>=1` (каждый
  `coin` независим); размер влияет только на выбранный индекс. Поэтому
  ошибка в eligibility не сдвинет поток, но исказит будущую
  материализацию оружия — parser всё равно покрывается тестами.
- `level_offset` в decompile читается как накопительное значение;
  для retail-скриптов он один на блок (`-7`/`0`). Множественные
  `level_offset` строки обрабатываются как последнее значение до
  weapon-строки (соответствует потоку 21397–21424); отдельного теста на
  несколько offset-строк нет — зафиксировать в Open Questions, если
  встретится в данных.
- Post-generation RNG (movement/AI/generic loot) остаётся вне модели
  loot-окна; он не влияет на NPC-main oracle, но повлияет на будущие
  системы, читающие RNG после загрузки.

## 9. References

- `docs/research/p3b2b2e-live-parity-stage2b.md` §1, §5, §8 (D-063).
- `docs/research/p3b2b2c-npc-loot-specials.md` §11 (D-062).
- `docs/STATUS.md` Phase 1b2b2e; `docs/DECISIONS.md` D-062/D-063.
- `Server.dll`/`decompiled.c` offsets из §2.
