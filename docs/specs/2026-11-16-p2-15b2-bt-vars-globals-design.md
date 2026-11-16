# P2-15b2 — Restore script globals from `BT_VARS` Design

Status: approved (2026-11-16)
Phase: P2-15b2 (P2-15b read domains; after b1b1–b1b3)
Layers: `assets` (SaveVar accessor) + `script` (VM overlay) + `tools/glgame`

## 1. Goal

Применять script globals из `BT_VARS` (type/name/value) к CS-VM при
load-save: saved значения перекрывают `zlato_vars.scr` defaults, приводясь
к объявленному типу глобала; неизвестные имена добавляются; overlay
применяется после каждого `set_globals` при открытии диалога. New Game не
затронут.

## 2. Evidence

`docs/research/p2-15a-save-payload.md` §`BT_VARS `:

- Body: `u32 count`, per var `u32 type` (`0` int, `1` DWORD, `2` float,
  `3` string), `u32 name_len + cp1251 name` (no NUL), type `<3` — **8 raw
  bytes f64 LE**, type `3` — `u32 value_len + cp1251 bytes`. Reader
  `FUN_140023f0` RVA `0x23f0` (1424-1456), writer `FUN_14002324` RVA
  `0x2324` (1383-1416), global object `DAT_1407f9a0`.
- `0`/`1` are indistinguishable in the VM; saved numeric encoding is f64
  regardless of declared type (`10.0`, `1.0`, `0.5`). OQ-5 (f64 → declared
  type conversion inside the original VM) is unresolved.
- Corpus: 28 saves total types `{0:23498, 1:2632, 2:61}`; type 3 absent.
- Port: `SaveVar{type,name,value[8],text_value}` already parsed and
  validated (`save_payload.cpp:394-413`); `ScriptGlobal{type,name,Value}`
  and `parse_globals` (`globals.cpp:100`) read text `.scr` declarations;
  `ScriptVm::set_globals` fills `globals_` and `rebuild_variables`
  (`script_vm.cpp:214-218,756`); `Value = variant<int32_t,uint32_t,double,
  std::string>`; assignment coercion uses `to_int32`/`to_uint32`/`to_double`
  (`script_vm.cpp:26-74,679`).
- glgame creates the VM per dialog from
  `scripts/dialogs_special/zlato_vars.scr` (`main.cpp:77,1261-1296`);
  saved vars are currently ignored.
- Independent scan (2026-11-16): slot0 1002 vars `{0:905,1:94,2:3}`,
  slot2 1004 `{0:907,1:94,2:3}`, slot21 879 `{0:783,1:94,2:2}`; corpus
  total 26,191. Among 26,191 saved names, 4,239 are not declared in
  `zlato_vars.scr` (mostly `L*_state`); for declared names there are **0**
  type mismatches (int↔0, DWORD↔1, float↔2). Examples: `voevoda_povtor`
  `10.0` (slot0), `ShurbaHal_guardian_povtor` `3.0`, `L1_plotnik_son_life`
  `0.0`.

## 3. Scope

Входит:

- `assets::save_var_number(const SaveVar&)` -> `std::optional<double>`
  (`type < 3`, f64 LE); `nullopt` для string.
- `script::GlobalApplyCounts {matched, added, kept}`;
  `ScriptVm::apply_saved_globals(std::span<const ScriptGlobal>)`;
  `ScriptVm::globals()` const accessor;
  `script::save_globals_from_vars(std::span<const SaveVar>)` adapter.
- `glgame`: `save_globals_` из `payload.vars` при успешном load-save,
  rollback-снимок при ошибке, очистка на New Game, применение в
  `open_dialog` после `set_globals`, лог `save globals: saved=<n>`.
- Tests: script unit (conversion/unknown/keep/duplicates/rebuild), real
  28-save 0/N application, E2E log.
- Docs: D-075, STATUS/PROJECT/parity/research (OQ-5 chosen policy).

Не входит:

- `BT_TRIBE`/`BT_DIARY`/`BT_HSTRY`, save write (`P2-15c`), live-oracle.
- Семантика OQ-5 оригинала (выбранная политика документируется, не
  заявляется как parity).
- Другие VM (в production есть только dialog-VM); world-script VM появится
  позже и должен использовать тот же overlay.

## 4. API

`src/assets/include/gl/assets/save_payload.hpp`:

```cpp
std::optional<double> save_var_number(const SaveVar &var);
```

`src/script/include/gl/script/globals.hpp`:

```cpp
std::vector<ScriptGlobal> save_globals_from_vars(
    std::span<const gl::assets::SaveVar> vars);
```

`src/script/include/gl/script/script_vm.hpp`:

```cpp
struct GlobalApplyCounts {
    std::size_t matched = 0;
    std::size_t added = 0;
    std::size_t kept = 0;
};
GlobalApplyCounts apply_saved_globals(std::span<const ScriptGlobal> saved);
const std::map<std::string, Value> &globals() const;
```

## 5. Conversion rules

Для каждого saved-глобала в порядке файла:

1. Если имя уже есть в `globals_` (объявлено `.scr`):
   - destination `int32_t` → `Value{to_int32(saved)}`;
   - destination `uint32_t` → `Value{to_uint32(saved)}`;
   - destination `double` → `Value{to_double(saved)}`;
   - destination `std::string`: saved только string → сохранить; numeric
     → `kept++` + warning, значение не меняется;
   - numeric destination + saved string → `kept++` + warning, не меняется;
   - иначе `matched++`.
2. Имени нет → добавить: numeric saved (`type<3`) как `double`, string
   (`type==3`) как `std::string`; `added++`.
3. Дубликат имени в saved-списке: последнее значение побеждает + warning.
4. После overlay — `rebuild_variables()`; `to_int32`/`to_uint32`
   семантика (clamp/trunc) переиспользуется как есть.

`save_globals_from_vars`: numeric → `ScriptGlobal{"", name, *save_var_number}`;
type 3 → `ScriptGlobal{"", name, text_value}`; `type` поле не используется
overlay-логикой.

## 6. glgame lifecycle

- Member `std::vector<gl::script::ScriptGlobal> save_globals_;`.
- `load_save`: snapshot `save_globals_` до загрузки; после успешного
  `parse_save_payload` и до `load_level` заполнить из `payload->vars`;
  при ошибке загрузки — восстановить snapshot (вместе с остальным
  rollback); при успехе — лог `save globals: saved=<n>`.
- `load_level` (callback без `pending_restore_`, т.е. New Game) очищает
  `save_globals_`.
- `open_dialog`: после `context->vm->set_globals(globals.value())`
  вызвать `context->vm->apply_saved_globals(save_globals_)`.
- Открытый диалог при загрузке не перезапускается; overlay действует на
  следующие `open_dialog`.

## 7. Tests

Script unit (`tests/script/test_saved_globals.cpp`):

- int32: `7.9 → 7`, clamp `> int32 max`, `NaN → 0`;
- DWORD: `-1.0 → to_uint32(-1)` (clamp/wrap как существующий `to_uint32`);
- float: `2.25` остаётся `double`;
- string↔int keep + warning; unknown numeric → added double; unknown
  string (type 3) → added string;
- duplicate saved name last-wins;
- `rebuild_variables` подхватывает overlay для node-переменной (создать VM
  с одним node opcode 23 на имя глобала и проверить `variable(name)`).

Real (`tests/script/test_real_save_globals.cpp`, skip без `GL_GAME_DIR`):

- для каждого из 28 saves: `parse_save_payload`, `parse_globals` из
  `scripts/dialogs_special/zlato_vars.scr` (через VFS), `set_globals`,
  `save_globals_from_vars` + `apply_saved_globals`;
- 0/N: каждый numeric global (int32→`static_cast<double>`, uint32→,
  double→) равен независимо декодированному f64 из `SaveVar`; каждый
  type-3 string равен `text_value`; для unknown имён значение равно
  decoded double;
- counts: `matched + added == vars.size()`, `kept == 0` на корпусе;
- pinned: slot0 1002 `{905,94,3}`, slot2 1004 `{907,94,3}`, slot21 879
  `{783,94,2}`; corpus aggregate `{0:23498,1:2632,2:61}`; первые имена и
  значения (`voevoda_povtor` slot0 `10.0`, slot21 `0.0`;
  `ShurbaHal_guardian_povtor` slot0 `3.0`).

App (`tests/app/test_glgame_screenshot.cpp`):

- `--load-save 0` добавляет лог `save globals: saved=1002`; существующие
  restoring/logs и E2E без изменений.

## 8. Acceptance

1. 28-save 0/N application of every saved numeric/string value to the VM.
2. Conversion unit rules (trunc/clamp/float/string/unknown/keep/duplicates)
   and `rebuild_variables` integration.
3. glgame applies overlay on every dialog, rolls back on failed load,
   clears on New Game; E2E log `saved=1002` for slot0.
4. Полный `ctest` с `GL_GAME_DIR=I:\Games\Златогорье-2` зелёный.
5. Docs: D-075, STATUS/PROJECT/parity/research; OQ-5 policy documented.

## 9. Risks

- OQ-5 (f64 → declared type conversion in the original VM) not proven; the
  chosen policy keeps `.scr` declared types authoritative and uses the VM's
  existing coercion helpers. Real corpus has 0 declared-type mismatches.
- 4,239 saved names are not declared in `zlato_vars.scr`; they are added
  as `double`/`string`. Original global manager behavior for unknown names
  is not separately traced.
- Type 3 (string) is absent from the 28-save corpus; encoding/application
  is covered only synthetically.
- Overlay is applied only to the dialog VM; any future world-script VM must
  apply the same overlay.
- Duplicate saved names are last-wins with a warning; corpus duplicate
  count is not pinned here.
- P2-15c write must encode overlay values back as f64/string; not in scope.

## 10. References

- `docs/research/p2-15a-save-payload.md` §`BT_VARS `, OQ-5/OQ-6.
- `docs/research/p3-vm-natives-server.md`, `src/script/src/script_vm.cpp`,
  `src/script/src/globals.cpp`.
- Specs/plans `2026-11-14-p2-15b1b1`, `2026-11-15-p2-15b1b2`,
  `2026-11-15-p2-15b1b3`; D-071/D-072/D-073/D-074.
