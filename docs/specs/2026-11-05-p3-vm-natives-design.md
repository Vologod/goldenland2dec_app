# Phase 1a — Server.dll VM parity & native dispatch (design)

Дата: 2026-11-05. Статус: approved (пользователь, 2026-11-05).

## 1. Цель

Снять крупнейший разрыв P3 (`P3-4`–`P3-9`): привести исполнение CS-скриптов
в `gl_script` к семантике `Server.dll` и ввести явный реестр нативов, в
котором каждый из 65 зарегистрированных в `Server.dll` ID имеет имя,
обработчик, арность и статус поддержки.

Phase 1a создаёт инфраструктуру и доказательную базу. Она не реализует
поведение игровых подсистем, которых в порте ещё нет (квесты, торговля,
партия, магия, время мира, глобальная карта).

## 2. Контекст

- `Server.dll` v1.3, SHA-256
  `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`.
- Локальный корпус: `.local/decomp/server/` (1108 функций; в git не
  коммитится). Индекс — `docs/research/decomp-index.md`.
- Текущая VM порта реализована по `GoldenLandEditor::CsExecutor`; evidence
  из `Server.dll` нет. `DefaultScriptHost::call` возвращает `0` для всех
  известных ID (`P3-9`).
- В корпусе локализованы:
  - `FUN_1403e020` (RVA `0x3e020`) — разбор CS-узлов; подтверждает layout
    узла и набор опкодов `0..0x32`;
  - `FUN_1403a010` (RVA `0x3a010`) — вычисление выражений (опкоды
    `0..0x32`);
  - `FUN_1403a914` (RVA `0x3a914`) — исполнение/переходы;
  - `FUN_14042464` (RVA `0x42464`) — регистрация нативов через
    `FUN_14004d00(registry, id, handler)`;
  - `FUN_14039888` (RVA `0x39888`) — вызов натива по ID через
    `FUN_14004f14(registry, id, out)`;
  - 65 регистраций с обработчиками в диапазоне `0x3fb8c`–`0x42464`.
- `cs_function_name` в `gl_assets` уже содержит editor-derived таблицу
  ID→имя; в `Server.dll` регистрируются те же 65 ID.

## 3. Scope

### 3.1 Research

Создать `docs/research/p3-vm-natives-server.md` со следующими фактами:

1. Точный layout CS-узла и размеры записей по `FUN_1403e020`; сверка с
   текущим `CsNode`.
2. Семантика опкодов по `FUN_1403a010`/`FUN_1403a914`: логические,
   сравнения, арифметика (включая знак/деление на ноль/округление),
   битовые, unary, литералы, `func`, `jmp`, `assign`; лимиты и обработка
   ошибок, если они есть в оригинале.
3. Таблица 65/65: `ID | имя | RVA handler | аргументы | возврат |
   побочные эффекты | статус в порте`.
4. Для каждого обработчика — какие поля игрового состояния он читает и
   меняет (с offset/RVA), чтобы будущие подпроекты не переисследовали.
5. Явная фиксация мест, где декомпил неоднозначен, с указанием, что
   остаётся `unsupported` без угадывания.

### 3.2 VM parity

- Изменять `gl_script` VM только там, где `Server.dll` противоречит
  текущей реализации; каждое изменение — с ссылкой на RVA и тестом.
- Если оригинал и редактор расходятся, приоритет у `Server.dll`; решение
  фиксируется в `docs/DECISIONS.md`.
- Сохранить существующие публичные контракты `ScriptVm`/`DialogSession`
  без необходимости.

### 3.3 Native registry

Новый модуль `gl::script`:

- `NativeInfo { std::uint32_t id; NativeSupport support;
  std::string_view reason; NativeHandler handler; }`; имя берётся из
  `gl::assets::cs_function_name(id)`, чтобы не дублировать таблицу.
- `NativeSupport { vm_internal, supported, unsupported }`:
  `vm_internal` — `D_Say`/`D_Answer`/`D_CloseDialog`, перехватываются VM до
  host; `supported` — реализуются host'ом; `unsupported` — нужна
  подсистема.
- `NativeRegistry`:
  - статический список 65 записей;
  - `find(id)` → `const NativeInfo*`;
  - `call(info, args, NativeServices&)` → `Value`;
  - `std::size_t size()`, итерация для тестов/логов.
- ABI порта: аргументы передаются как `std::span<const Value>`
  (типы int32/uint32/double/string), а не через глобальные
  `DAT_14089740/44` оригинала; `NativeServices` заменяет прямые
  зависимости от клиента/консоли.
- `NativeServices` — инъекция побочных эффектов без зависимости от
  `game`/`core`: логирование, `console` (строка), `command` (строка),
  `play_sound` (id/имя), `signal`.
- Реестр не зависит от SDL, `game` и `core`; только `gl_assets` + std.

### 3.4 GameScriptHost

Новый `gl::game::GameScriptHost : gl::script::ScriptHost`:

- диспетчеризует через `NativeRegistry`;
- реально исполняет только нативы, для которых в порте уже есть
  состояние/сервис: `Exit`, `Signal`, `Console`, `Cmd`, `D_PlaySound`;
  их точная семантика — по обработчикам `Server.dll`;
- для `unsupported` возвращает `int32 0` и пишет **один** info-лог на ID
  за жизнь host'а (`"script native <name> (<hex>): unimplemented — <reason>"`);
- `vm_internal` host не получает; если всё же получил — warning и `0`;
- неизвестный ID (не из 65) — warning, `0` (как сейчас).

`D_Say`/`D_Answer`/`D_CloseDialog` остаются перехваченными VM, как в
текущем контракте; host их не получает.

### 3.5 Проводка

- `tools/glgame`: использует `GameScriptHost` с реальными колбэками
  `Console`/`Cmd`/`D_PlaySound`.
- `tools/glscript`: использует `GameScriptHost`; поведение CLI не
  меняется.
- `DefaultScriptHost` сохраняется для тестов и обратной совместимости.

## 4. Non-goals

- Поведение `RS_*`/`LE_*`/`WD_*` нативов, требующих новых подсистем.
- Новые state-хранилища (квесты, события, торговля, время, партия,
  магия, глобальная карта).
- Save payload (`P2-15`), частота 35 Гц (`P2-9`/`P4b-16`), UI.
- Изменение UI/сценария диалогов.

## 5. Обработка ошибок

- Дубликаты ID в реестре — ошибка инициализации в тестах; в release —
  error-лог и первая запись побеждает.
- Неизвестный ID — warning + `0`.
- Некорректный вызов supported-натива (например, недостаточно
  аргументов) — warning + `0`, без падения.
- VM-ошибки сохраняют текущую «липкую» семантику порта до подтверждения
  обратного из `Server.dll`; расхождения фиксируются в research.

## 6. Тестирование

Unit (`tests/script`):

- реестр: ровно 65 записей, уникальные ID, `name` совпадает с
  `gl::assets::cs_function_name`, все `supported` имеют handler;
- `supported` нативы вызываются и возвращают документированный тип;
- `unsupported` возвращает `0` и логирует один раз на ID;
- неизвестный ID — warning + `0`;
- VM parity-кейсы по каждому подтверждённому расхождению из research.

Real-data:

- `glscript` на 555 `.cs` — без новых ошибок разбора/исполнения;
- `demon.d1.age.cs` — тот же `wait_user`, шаги и `dialogs_data`;
- `glgame --click-npc` — диалог открывается и выбирается как раньше.

Полный прогон:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

с `GL_GAME_DIR=I:\Games\Златогорье-2`.

## 7. Критерии приёмки

1. `p3-vm-natives-server.md` содержит 65/65 строк `ID → handler` и
   семантику VM с RVA.
2. `NativeRegistry` содержит 65/65 ID; тест сверки имён с
   `cs_function_name` проходит.
3. Каждое изменение VM-семантики имеет evidence из `Server.dll` и тест.
4. `GameScriptHost` подключён в `glgame` и `glscript`; unsupported-нативы
   не спамят лог.
5. Targeted `ctest` по script/game/tools/app — pass; полный `ctest` с
   `GL_GAME_DIR` — pass (числа зафиксированы в `STATUS.md`).
6. `STATUS.md`, `PROJECT.md`, `parity-matrix.md` обновлены: `P3-4`–
   `P3-8` получают evidence, `P3-9` → `partial` с числом supported.

## 8. Риски

- Ghidra-декомпил обработчиков может быть неоднозначным; такие нативы
  остаются `unsupported` с записанной причиной, без «правдоподобной»
  реализации (D-029).
- VM-семантика опирается на FPU-поведение MSVC; при расхождении
  фиксируется поведение оригинала, а не предполагаемое.
- Добавление `gl_script`/`gl_core` в зависимости `gl_game` может выявить
  скрытые циклы; зависимости добавляются только вниз по слоям.
