# Design: P3a — CS bytecode parser, disassembler and VM core

Статус: **утверждён**.
Дата: 2026-10-20.
Базируется на: `docs/specs/2026-10-19-goldenland2-android-port-design.md`, `docs/research/` и
реверсе `GoldenLandEditor` (`CS_Parser`, `CsExecutor`, `CsFunctions`, `CsOpcodes`).

## 1. Цель

Научиться читать и исполнять оригинальный байткод `.cs`: парсер узлов, дизассемблер,
VM с переменными и базовыми операциями, глобальные переменные и диалоговый flow
(`D_Say`/`D_Answer`). Это фундамент P3 (скрипты/квесты/NPC).

P3a не включает AgeScript-компилятор `.scr` и игровые системы (P3b/P4+).

## 2. Границы

Входит:

- `gl_assets`: парсер `.cs` (`CS_Node`), имена опкодов и функций, безопасная валидация.
- `gl_script`: VM (переменные, арифметика/сравнения/логика/assign/jmp/func),
  глобальные переменные, реестр нативных функций, диалоговый flow.
- Инструменты: `glpak disasm-cs` и `glscript` (headless-запуск скрипта).
- Тесты: синтетические + все 555 `.cs` + реальный диалог.

Не входит:

- Компиляция/интерпретация `.scr` (AgeScript) — P3b.
- Полный набор нативных функций, привязанных к миру — P4–P6 (P3a даёт реестр и заглушки).
- UI диалогов, SDB-фразы, портреты — P3c/P7.
- Скрипты `.ssh/.prs/.inv/.inf` (это конфиги, не CS).

## 3. Формат `.cs` (подтверждён по `CS_Parser`)

```
u32 fileSize (== размер файла)
повторять до fileSize:
  i32 opcode
  payload:
    opcode 0..20 или 50 (assign):
       i32 a, i32 b, i32 c, i32 d
    opcode 21 (num_var) или 24 (num):
       f64 value
    opcode 22 (str) или 23 (str_var):
       C-string (NUL-terminated, cp1251)
    opcode 48 (func):
       i32 c, i32 d, f64 functionId,
       до 9 × i32 args (терминатор -1)
    opcode 49 (jmp):
       i32 c, i32 d
```

Имена опкодов: 0 `||`, 1 `^^`, 2 `&&`, 6 `!=`, 7 `==`, 8 `>=`, 9 `<=`, 10 `>`, 11 `<`,
14 `+`, 15 `-`, 16 `*`, 17 `/`, 18 `%`, 21 `num_var`, 22 `str`, 23 `str_var`, 24 `num`,
48 `func`, 49 `jmp`, 50 `assign` (полный список — `CsOpcodes.cpp`).
ID функций: `0x1000000` Exit, `0x1000004` D_Say, `0x1000005` D_CloseDialog,
`0x1000006` D_Answer, `0x4000000` RS_GetPersonParameterI и т.д. (`CsFunctions.cpp`).

## 4. VM

`gl_script::Value = std::variant<int32_t, uint32_t, double, std::string>`.
`ScriptVm`:

- `nodes` (`vector<CS_Node>`), `vars` (map name→Value), `host` (нативные функции).
- Статусы: `start, restart, continue, wait_user, end, infinity`.
- `step()`:
  - logical (0..2): a,b — узлы; c/d — куда перейти; рекурсивно вычисляет сравнения/логику/func.
  - comparison (6..11): a,b — значения; c/d — переход.
  - assign (50): a — переменная, b — источник (var/literal/func/арифметика), d — next.
  - jmp (49): c — target; `d == -1` → если в dialogFuncs есть `D_Say` → `wait_user`,
    иначе `end`.
  - func (48): вызвать host; `D_Say`/`D_Answer` складывать в dialogFuncs; вернуть значение.
  - арифметика (14..18) — только числа; строки запрещены.
- Счётчик шагов (10 млн) → `infinity`.
- `dialogs_data()` — `[sayValue, answerValue...]`; `answers_count()`.
- `user_input(answer)`: `LastPhrase = sayValue`, `LastAnswer = answerValue`, restart at 0.
- `D_CloseDialog` → `end` (по комментарию в `CsExecutor`).

Глобальные переменные: `.scr`-файл вида `int Name 0`, `DWORD Name 0`, `float Name 0.5`,
`string Name "text"`, комментарии `//`; строки без значения пропускаются с warning.

Нативные функции: `ScriptHost` интерфейс `Value call(uint32_t id, span<const Value>)`.
P3a-реализация: `D_Say`/`D_Answer` обрабатываются VM; `D_CloseDialog` → end;
`RS_GetPersonParameterI` → 0; `Exit`/`Signal`/`Console`/`Cmd` — заглушки (лог);
неизвестные → 0 + warning. Реестр расширяемый.

## 5. Инструменты

- `glpak disasm-cs <game_dir> --file <path.cs> [--out <txt>]` — человекочитаемый дамп
  узлов: индекс, opcode/имя, a/b/c/d, литералы, func id/args.
- `glscript --game-dir <dir> [--globals <path>] --file <path.cs> [--run] [--answer N]`:
  парсит, опционально исполняет до `wait_user`/`end`, печатает статус, `dialogs_data`,
  число ответов; `--answer N` подаёт ответ и продолжает.
  Коды выхода: 0 успех, 1 аргументы, 2 I/O, 3 формат/исполнение.

## 6. Тестирование

Unit (`gl_assets`):

- синтетические узлы всех типов (a/b/c/d, num/str, func с args, jmp);
- битый размер/opcode/усечённый payload → `format`;
- все 555 реальных `.cs` из `scripts.pak` парсятся, 0 ошибок.

Unit (`gl_script`):

- арифметика/сравнения/логика/assign/jmp на синтетическом байткоде;
- цикл и счётчик безопасности;
- globals: int/DWORD/float/string, комментарии, битые строки;
- диалог: `D_Say` + 2 `D_Answer` → `wait_user`, `answers_count == 2`;
  `user_input(0)` → `LastPhrase/LastAnswer`, продолжение; `D_CloseDialog` → end;
- реальный `demon.d1.age.cs`: до `wait_user`, ответы > 0, ответ 0 не падает.

Integration:

- `glpak disasm-cs` для `demon.d1.age.cs` → непустой дамп;
- `glscript --run` на нём → `wait_user`; с `--answer 0` → `end`/`wait_user` без падения.

## 7. Критерии готовности

1. 555/555 `.cs` парсятся.
2. `glpak disasm-cs` работает.
3. `glscript` исполняет реальный диалог до `wait_user` и обрабатывает ответ.
4. Unit/integration тесты зелёные; `gl_assets`/`gl_script` SDL-free.
5. Документы обновлены.

## 8. Риски

- Семантика отдельных опкодов (1 `^^`, 3..5, 12/13, 19/20) не проверена — P3a
  поддерживает основные (0/2, 6..11, 14..18, 21..24, 48..50); остальные логируются/ошибка.
- Функции, влияющие на мир, — заглушки; поведение квестов появится в P4+.
- Кодировка строк — cp1251; на API UTF-8.
- Защита от бесконечных скриптов — счётчик шагов.

## 9. Отложено

AgeScript-компилятор, полный набор native-функций, UI диалогов/SDB, квесты/NPC/бой.
