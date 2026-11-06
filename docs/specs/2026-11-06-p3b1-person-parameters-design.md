# Phase 1b1 — Person parameter natives (design)

Дата: 2026-11-06. Статус: approved (пользователь, 2026-11-06).

## 1. Цель

Снять часть `P3-9`: реализовать нативы параметров/навыков персоны на
состоянии порта, с точной семантикой из `Server.dll`:

- `RS_GetPersonParameterI` (`0x4000000`);
- `RS_SetPersonParameterI` (`0x4000001`);
- `RS_GetPersonSkillI` (`0x400000C`).

Задача вводит runtime-состояние персон (`PersonRuntime`) и read-only чтение
stat-блока героя из save. Она не реализует остальные подсистемы persons
(lifecycle, партия, инвентарь, квесты, время, мир).

## 2. Контекст

- Источник истины — `Server.dll` v1.3, SHA-256
  `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Native-таблица Phase 1a: `docs/research/p3-vm-natives-server.md` §5.
- Реальные данные (`scripts.pak`, 556 `.cs`): только три указанных ID
  относятся к параметрам/навыкам персоны.
  - `RS_GetPersonParameterI`: 470 вызовов, все — `"Hero"`; параметры
    `REPUTATION` 444, `INTELLIGENCE` 10, `LIFE` 4, `MAXIMUM LIFE` 4,
    `EXPERIENCE` 4, `REPUTATIUON` 2 (опечатка), `WISDOM` 1,
    `CONSTITUTION` 1.
  - `RS_SetPersonParameterI`: 236 вызовов; `"Hero"/"REPUTATION"` 225,
    `"Hero"/"LIFE"` 1, `"L2_2.P71_Ivlis"/"LIFE"` 5,
    `"L0.P738_artist3"/"LIFE"` 1.
  - `RS_GetPersonSkillI`: в поставляемых `.cs` не вызывается.
- Строки параметров в `Server.dll` (RVA строк): `EXPERIENCE` `0x91840`,
  `REPUTATION` `0x91834`, `INTELLIGENCE` `0x91824`, `WISDOM` `0x9181c`,
  `STRENGTH` `0x91810`, `CONSTITUTION` `0x91800`, `DEXTERITY` `0x917f4`,
  `LUCK` `0x917ec`, `PERCEPTION` `0x917e0`, `MAXIMUM LIFE` `0x917d0`,
  `LIFE` `0x917c8`, `ENERGY` `0x917c0`.
- NPC-статы уже доступны в `PersonInstance::def.values`
  (`strength`, `constitution`, `dexterity`, `perception`, `intelligence`,
  `wisdom`, `luck`, `reputation`, `experience_value`, `skills.<SKILL>`).
- Hero stat-блок находится в `BT_HERO` save по offset `0x00C`, размер
  `0x11C` (копируется в `hero+0xFF0`); writer `0x1401e690`, reader
  `0x3210c` (см. `docs/research/p5-save-equipment.md` §4).
- В `glgame` уже есть чтение тела save (`decode_save_body`) для
  экипировки/инвентаря (`apply_equip_save`).

## 3. Scope

### 3.1 Поддерживаемые нативы

| ID | Имя | Поведение порта |
|---|---|---|
| `0x4000000` | `RS_GetPersonParameterI` | вернуть double параметра персоны |
| `0x4000001` | `RS_SetPersonParameterI` | записать параметр, вернуть `0.0` |
| `0x400000C` | `RS_GetPersonSkillI` | вернуть double навыка персоны |

Остальные ID остаются `unsupported` с прежними reason.

### 3.2 Параметры

Getter (`RS_GetPersonParameterI`) поддерживает ровно эти имена (сравнение
побайтовое, case-sensitive):

| Имя | Getter `Server.dll` |
|---|---|
| `EXPERIENCE` | `FUN_1400cc84` |
| `REPUTATION` | `FUN_1400cc0c` |
| `INTELLIGENCE` | `FUN_1400d4c0` |
| `WISDOM` | `FUN_1400d3d8` |
| `STRENGTH` | `FUN_1400d678` |
| `CONSTITUTION` | `FUN_1400d610` |
| `DEXTERITY` | `FUN_1400d540` |
| `LUCK` | `FUN_1400d458` |
| `PERCEPTION` | `FUN_1400d5a8` |
| `MAXIMUM LIFE` | `FUN_1400c280` (второй out) |
| `LIFE` | `FUN_1400c280` (первый out) |
| `ENERGY` | `FUN_1400c1fc` (первый out) |

Setter (`RS_SetPersonParameterI`) поддерживает:

| Имя | Setter `Server.dll` |
|---|---|
| `ENERGY` | `person+0x1500 = value` |
| `LIFE` | `FUN_14019674(person, value)` |
| `REPUTATION` | `FUN_1400aa18(db, id, value)` |
| `STRENGTH` | `FUN_1400b824(db, id, value, 1)` |
| `CONSTITUTION` | `FUN_1400b5e8(db, id, value, 1)` |
| `DEXTERITY` | `FUN_1400b3ac(db, id, value, 1)` |
| `LUCK` | `FUN_1400aabc(db, id, value, 1)` |
| `PERCEPTION` | `FUN_1400b170(db, id, value, 1)` |
| `INTELLIGENCE` | `FUN_1400acf8(db, id, value, 1)` |
| `WISDOM` | `FUN_1400af34(db, id, value, 1)` |

`EXPERIENCE` и `MAXIMUM LIFE` у setter'а не имеют ветки: вызов ничего не
меняет и возвращает `0.0`. Это поведение сохраняется.

Неизвестное имя параметра (включая `REPUTATIUON`): get → значение по
умолчанию `DAT_140913e0`, set → no-op. Значения `DAT_140913d8`/`DAT_140913e0`
и точные преобразования аргумента фиксируются research (Task 1) и
используются в тестах.

### 3.3 Навыки

`RS_GetPersonSkillI(person, skill)`:

1. Найти персону по имени; если не найдена — default `DAT_140913f0`.
2. Разрешить имя навыка в skill id тем же способом, что оригинал
   (`DAT_140891bc+0x78`, vtable `+0x2c`); если не разрешено — default.
3. Вернуть значение навыка из person-базы.

В порте источник — `PersonDef.values["skills.<name>"]`. Точный порядок
skill id и его совпадение с `.scr`-ключами фиксируются research. Если
соответствие не доказано, `RS_GetPersonSkillI` остаётся `unsupported` с
reason, а не реализуется догадкой.

### 3.4 Персоны и Hero

- `PersonRuntime` строится из `Level::persons`; каждая персона получает
  state из своего `PersonDef.values`.
- `"Hero"` — специальная запись, доступная всегда. Если в `glgame`
  выбран save, начальные значения берутся из `parse_hero_stats(body)`;
  без save — нули и один info-лог при создании runtime.
- Поиск имени — точное побайтовое совпадение, без case-fold. NPC-имена и
  `"Hero"` не пересекаются.
- Setter меняет только runtime-значение порта; .scr/save не пишутся.

## 4. Non-goals

- `RS_IsPersonExistsI`: межлокационная проверка `(location, person)`.
- `RS_AddExp`: распределение опыта по партии.
- `RS_AddPerson_1/2`, `RS_DelPerson`: lifecycle/спавн/удаление.
- `RS_SetUndeadState`, `RS_SetInjured`: флаги/состояния персоны.
- Inventory (`0x5000000`–`0x5000008`), party, quests/events, time,
  world/triggers, magic/effects.
- UI-отображение статов, запись save, изменение `.scr`.
- Пересчёт производных величин (max life от constitution и т.п.) сверх
  поведения конкретных setter-функций.

## 5. Архитектура

### 5.1 `gl::script`

- `enum class PersonParameter { experience, reputation, intelligence,
  wisdom, strength, constitution, dexterity, luck, perception, maximum_life,
  life, energy, unknown };`
- `PersonParameter person_parameter_from_name(std::string_view)` —
  единственная таблица соответствия строк `Server.dll` → enum; покрыта
  тестом на все 12 имён, регистр и опечатку.
- `struct PersonAccess { std::function<double(std::string_view person,
  PersonParameter)> get_parameter; std::function<void(std::string_view
  person, PersonParameter, double)> set_parameter;
  std::function<double(std::string_view person, std::string_view skill)>
  get_skill; };`
- `NativeServices` получает поле `PersonAccess person;`.
- `native_registry.cpp`: три handler'а; arity/типы по §5 research
  (`0`/`1` = int/DWORD, `2` = float, `3` = string); отсутствие callback'а
  трактуется как «персона не найдена»: get → default, set → no-op.

`gl_script` не зависит от `gl_game`; маппинг имён параметров живёт здесь,
потому что это семантика `Server.dll`.

### 5.2 `gl::game`

- `struct PersonState` — типизированные значения 12 параметров и
  `std::map<std::string, std::int32_t> skills`.
- `class PersonRuntime`:
  - `static PersonRuntime from_level(const Level&, std::optional<const
    HeroStats*> hero_stats);`
  - `double get_parameter(std::string_view person, gl::script::PersonParameter) const;`
  - `void set_parameter(std::string_view person,
    gl::script::PersonParameter, double value);`
  - `double get_skill(std::string_view person, std::string_view skill) const;`
  - инъекция callbacks: `gl::script::PersonAccess access();`
- Значения NPC берутся из `PersonDef.values` с теми же именами ключей,
  что в `.scr`; извлечение чисел — `from_chars`, отсутствующее значение —
  `0`.
- `PersonRuntime` не зависит от SDL и `LevelScreen`; только `gl_game`,
  `gl_script`, `gl_assets`.

### 5.3 `gl::assets`

- `struct HeroStats` и `Result<HeroStats> parse_hero_stats(std::span<const
  std::uint8_t> body);` — read-only разбор stat-блока `BT_HERO`
  (`0x00C`, `0x11C`), без изменения существующих `parse_hero_equipment`/
  `parse_hero_inventory`.

### 5.4 Проводка

- `glgame`: `PersonRuntime person_runtime_` — член `Game`, строится в
  `load_level` и при применении save (`apply_equip_save`), переживает
  диалоги. В `open_dialog` каждый `DialogContext::host.services().person`
  получает callbacks на `person_runtime_`.
- `glscript`: `PersonAccess` не задан; get → default, set → no-op.
- `GameScriptHost`/`NativeRegistry` структурно не меняются.

## 6. Research deliverables (Task 1)

Создать `docs/research/p3b1-person-parameters-server.md`:

1. Таблица `параметр → getter RVA → setter RVA → поле/преобразование`,
   включая точные значения `DAT_140913d8`, `DAT_140913e0`,
   `DAT_140913f0`, `DAT_14091400`.
2. Семантика `FUN_1400c1fc`/`FUN_1400c280` (порядок out-параметров
   `LIFE`/`MAXIMUM LIFE`/`ENERGY`).
3. Семантика setter-функций: truncation/unsigned, значение флага `1`,
   пересчёт производных (если есть) — что именно меняется.
4. Skill id: как `DAT_140891bc+0x78` vtable `+0x2c` преобразует имя в id;
   соответствие id и ключей `skills.<...>` в `.scr`.
5. Stat-блок `BT_HERO 0x00C..0x128`: offset каждого из 12 параметров,
   подтверждённый writer `0x1401e690` и/или reader `0x3210c`; значения
   real slot0/slot2 для проверки.
6. Любое расхождение с §3 фиксируется здесь; если поле не доказуемо,
   параметр остаётся `unsupported` с reason.

## 7. Обработка ошибок

- Персона не найдена: get → default (по research), set → no-op,
  skill → default.
- Неизвестный параметр/навык: get → default; set → no-op.
- Недостаточно аргументов или неверный тип: warning + `0` (текущая
  конвенция `NativeRegistry`; `FUN_14038c6c` проверяет только первые
  `arity` аргументов).
- Некорректный save/stat-блок: `parse_hero_stats` возвращает `Error`;
  `glgame` логирует warning и создаёт Hero с нулями; диалоги не
  блокируются.
- Дубликаты имён персон: первая запись в порядке `Level::persons`
  побеждает; warning один раз.

## 8. Тестирование

Unit (`tests/script`, `tests/game`, `tests/assets`):

- `person_parameter_from_name`: 12 имён, регистр, `REPUTATIUON`, пустая
  строка.
- handler'ы: get/set/skill вызывают callbacks с правильными аргументами;
  отсутствие callback'а; неверная арность/тип.
- `PersonRuntime`: NPC из синтетического `PersonDef`, точный поиск,
  get/set roundtrip, skill, отсутствующая персона, Hero без save.
- `parse_hero_stats`: synthetic (все поля), real slot0/slot2; значения
  совпадают с research; ошибка на коротком/битом теле.

Real-data (`tests/*_real`):

- `PersonRuntime` из реального `l1_1`/`l*` NPC: значения совпадают с
  `.scr` для всех 10 базовых параметров и навыков.
- Hero из real slot0/slot2: значения совпадают с research.
- `inter_kid3.age.cs` (`"Hero"/"REPUTATION"`) и
  `l12_2.p234_nature_wizard.d234.age.cs` (get/set): один и тот же
  `DialogSession` через `GameScriptHost` с разными значениями runtime
  выбирает ожидаемые ветки; baseline — VM с fake host.

E2E:

- `glgame --level l1_1 --click-npc 13` открывает диалог как раньше;
  `answers`/`steps` не регрессируют.
- Полный `ctest` с `GL_GAME_DIR`.

## 9. Критерии приёмки

1. Research содержит getter/setter RVA для всех 12 параметров, значения
   default-констант, семантику преобразований и offset stat-блока.
2. `NativeRegistry` помечает `0x4000000`/`0x4000001`/`0x400000C` как
   `supported`; остальные не меняются.
3. `PersonRuntime` и `parse_hero_stats` покрыты unit + real тестами.
4. Реальные диалоги, использующие `"Hero"/"REPUTATION"`, исполняются с
   реальными значениями; ветки совпадают с research.
5. Targeted и полный `ctest` с `GL_GAME_DIR` проходят; числа
   зафиксированы в `STATUS.md`.
6. `STATUS`/`PROJECT`/`parity-matrix` обновлены: `P3-9` supported `5 → 8`
   (если `GetSkill` подтверждён research; иначе `5 → 7` и reason).

## 10. Риски

- Stat-блок save может не содержать всех 12 параметров; поддержанными
  считаются только доказанные, остальные — `unsupported`.
- Setter-функции могут пересчитывать производные (max life/energy) и
  влиять на другие поля; порт меняет только доказанные поля.
- Общие шаблоны NPC: если оригинал разделяет person-базу между
  инстансами, per-instance runtime может расходиться; research должен
  зафиксировать модель, решение — в `DECISIONS.md`.
- Skill id mapping может быть недоказуем; тогда `GetSkill` остаётся
  `unsupported`.
