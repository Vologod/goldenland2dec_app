# STATUS

Обновлено: 2026-10-21

## P3c final fix wave (2026-10-21)

- Font atlas now covers U+00A0-U+00FF plus U+2116/U+2122; NBSP normalizes to a space;
  `Font::has_glyph()` exposes atlas coverage; NaN/Inf pixel heights are rejected.
- Tests: cp1251 punctuation/symbol coverage and measurement, dialog-before-renderer
  lifetime, strict UTF-8 Cyrillic checks, line-start phrase IDs, no `infinity` acceptance.
- `DialogSession::choose()` now fails when `ScriptVm::user_input()` rejects the answer;
  `DialogSession::failed()` added. `open_dialog` logs VFS/parse errors.
- Verified: `cmake --build build`; `ctest --test-dir build --output-on-failure` with
  `GL_GAME_DIR` - 13/13 passed, 655.80 s; targeted font/glgame/cs-tools/real-script tests pass.

## Текущая фаза

P0 завершена и принята. P1 — Level viewer: реализация и автоматизированная приёмка
завершены (2026-10-20); открыт финальный manual desktop pass (см. Open Questions).
P2a — оболочка игры и консоль: реализация, автоматизированная приёмка и manual desktop
gate завершены (2026-10-20); P2a принята. P2b — детерминированный тик: реализация,
автоматизированная приёмка и manual interactive gate завершены (2026-10-20); P2b принята.
P2c — save/load каркас: реализация и автоматизированная приёмка завершены (2026-10-20);
открыт manual UI gate (Continue → слоты, Esc). P3a — CS VM: Tasks 1–5 завершены
2026-10-21, автоматизированная приёмка пройдена, P3a принята (manual UI gate для P3a
нет). P3c — dialogs + SDB + TTF: Tasks 1–4 завершены 2026-10-21; автоматизированная
приёмка пройдена (13/13 ctest, `glscript --sdb`, dialog PNG), **manual dialog UI gate
OPEN**. Следующий — P4 (NPC) или P1b (Android/touch); P2 backfill отложен.

## Текущая задача

**P3c — Task 4 (automated acceptance + docs) завершён 2026-10-21; P3c automated
acceptance passed.** `cmake --build build` — `ninja: no work to do`, exit 0. Полный
`ctest --test-dir build --output-on-failure` с `GL_GAME_DIR` — **13/13 passed**,
652.23 s, exit 0 (real-data 562.27 s, glview 32.26 s, glgame 42.46 s); суммарно
318 test cases / 2649 assertions, 0 failed. `glscript --game-dir
"I:\Games\Златогорье-2" --file scripts/dialogs/demon.d1.age.cs --run --sdb
sdb/dialogs/dialogsphrases.sdb` — exit 0, `status: wait_user`, `steps: 43`,
`dialogs: 2684 2686 2696`, `answers: 2`, затем `2684<TAB>Чего уставился?`,
`2686<TAB>Я не думал, что демоны разумны...`, `2696<TAB>Ничего. Я пошел...`;
два ожидаемых warning `globals: line 918/1262: missing value`.
`glgame --game-dir ... --dialog scripts/dialogs/demon.d1.age.cs --screenshot
%TEMP%\p3c_dialog.png` — exit 0; PNG 1280×720, 54 332 байта, 552 цвета (выборка
каждые 2 px; 771 уникальный цвет), SHA-256
`2544FFD6F2A4B5729EF43413C26D20862B669E62F6C8CC71875EB45E65F2BFE1`; два прогона
побайтово совпали, загружен `C:\Windows\Fonts\segoeui.ttf`, console=0. Дефектов
автоматизированная приёмка не выявила. Ограничение P2c с `?` в заголовках save-слотов
закрыто TTF (ASCII-fallback остаётся только без шрифта). **Manual dialog UI gate OPEN:**
человек должен запустить `glgame --dialog ...` и проверить читаемость кириллицы, выбор
ответа (Up/Down, Enter/мышь) и `Esc`; агент это не проверял. Следующая задача —
P4 (NPC) или P1b (Android/touch).

**P3a — финальная волна правок (whole-branch review) завершена 2026-10-21.**
Исправлены: бюджет вычисления выражения (`kEvalBudget = 100'000` вызовов `eval` на
`step()`) против циклических/разделяемых графов; assign приводит RHS к текущему типу
приёмника (`int32`/`uint32`/`double`/`string`), включая float `0.5` и DWORD
`0xFFFFFFFF`; реализованы `1 ^^`, `3..5`, `12/13`, `19/20` (строки → `failed()`);
ошибки исполнения стали «липкими» и останавливают `step()` без лавины логов, а
`user_input` больше не сбрасывает `failed()`; `glscript`-тест VM-ошибки теперь
падает именно в VM (валидный `jmp` за границу), а не в `parse_cs`; `parse_globals`
игнорирует UTF-8 BOM и предупреждает о незакрытой кавычке. Несколько `D_Say` на
странице — по-прежнему только первый (как в `CsExecutor`, задокументировано).
Проверка: `cmake --build build` — успех; `gl_script_tests` — 49 cases / 374 assertions;
`gl_script_real_tests` — 1/20; `gl_cs_tools_tests` — 8/56; полный `ctest` с
`GL_GAME_DIR` — **11/11 passed**, 560.62 s, exit 0.

**P3a — Task 5 (acceptance + docs) завершён 2026-10-21. P3a принята.**
`cmake --build build` — `ninja: no work to do`, exit 0. Полный
`ctest --test-dir build --output-on-failure` с `GL_GAME_DIR` — **11/11 passed**,
573.09 s, exit 0 (real-data 518.81 s, `gl_cs_tools` 8.30 s, glview 26.26 s,
glgame 17.39 s); в логе real-data: `parsed=555 failures=0 nodes=338977`.
`glpak disasm-cs ... --file scripts/dialogs/demon.d1.age.cs --out %TEMP%\demon_disasm.txt`
— exit 0, **5353 байта / 240 строк**, дамп начинается с `0: jmp | 1 1`,
`1: == | 2 3 193 4`, `2: str_var | LastPhrase`, `3: num | 0`.
`glscript ... --file scripts/dialogs/demon.d1.age.cs --run` — exit 0,
`status: wait_user`, `steps: 43`, `dialogs: 2684 2686 2696`, `answers: 2`
(два ожидаемых warning `globals: line 918/1262: missing value`).
Тот же запуск с `--answer 0` — exit 0, второй останов:
`status: wait_user`, `steps: 84`, `dialogs: 2719 1529 2721`, `answers: 2`.
Дефектов приёмка не выявила. P3a automated acceptance passed; следующий подпроект —
P3b (AgeScript) / P3c (dialogs + SDB), затем P1b.

**P3a — Task 4 (tools `glpak disasm-cs`, `glscript`) завершён 2026-10-21.**
`glpak disasm-cs <game_dir> --file <path> [--out <txt>]` читает `.cs` через `GameVfs`,
парсит `parse_cs` и печатает по строке на узел (`index: opcode | поля | value/text |
funcName(args)`); кавычки и control-символы в `text` экранируются (`\"`, `\\n`, `\\xHH`),
поэтому дамп остаётся по одной строке на узел. Без `--out` — в stdout, с `--out` —
UTF-8 текст. Команда добавлена в `--help`, коды выхода 0/1/2/3 как у остальных команд.
Новый SDL-free `tools/glscript` (`--game-dir <dir> [--globals <path>] --file <path>
[--run] [--answer N]`) читает CS и globals через VFS (по умолчанию
`scripts/dialogs_special/zlato_vars.scr`), исполняет до `wait_user`/`end`/`infinity`,
печатает `status`, `steps`, id из `dialogs_data()` и `answers_count()`. Повторяемый
`--answer N` подаётся в `user_input`, после чего исполнение продолжается до следующего
останова с печатью нового состояния. `--answer` без `--run` и ответ вне диапазона —
exit 1; ошибка исполнения VM (`ScriptVm::failed()`, включая ошибки типов/вычислений
и `infinity`) — exit 3. `ScriptVm::failed()` теперь отделяет такие ошибки от
нормального `end`/`wait_user`; `user_input()` возвращает `bool`.
Тесты: `gl_cs_tools_tests` (8 cases / 56 assertions, skip 77 без `GL_GAME_DIR` или при
пустом значении) — `disasm-cs` в `--out` и stdout (непустой дамп с `func`/`jmp`),
экранирование кавычек/переводов строк, `glscript --run` (`wait_user`, answers > 0),
`--run --answer 0` (exit 0, `steps` растут), reject `--answer` без `--run`,
out-of-range answer → exit 1, missing script → exit 2, synthetic VM failure → exit 3.
`gl_script_tests` — 33 cases / 247 assertions; полный `ctest` с `GL_GAME_DIR` — **11/11
passed**, 571.23 s. Task 5 повторно прогнал полный набор: 11/11, 573.09 s, exit 0.

**P3a — Task 3 (dialog flow + native registry) завершён 2026-10-21.**
`ScriptVm` получил `dialogs_data()` (id `D_Say` + id всех `D_Answer`, действительно только
при `wait_user`), `answers_count()` и `user_input(answer)` с нумерацией ответов с нуля:
при успехе выставляет `LastPhrase`/`LastAnswer` (`uint32`), очищает записанные вызовы,
переходит к узлу 0 со статусом `restart`; выход за границы — warning без изменений.
Добавлен SDL-free `DefaultScriptHost`: `RS_GetPersonParameterI` → 0,
`Exit`/`Signal`/`Console`/`Cmd` → 0 + info, прочие известные функции → 0 + debug,
неизвестные → 0 + warning; тесты могут наследовать хост. Task 2 minors: правый операнд
сравнений/логики не вычисляется после ошибки левого; top-level `func` больше не
использует `d` как переход (реальные данные имеют `c = d = -1`); формулировки
unknown-opcode (`end` + error log) выровнены в spec/plan. Fix round 1: `Exit`/`Signal`/
`Console`/`Cmd` больше не перехватываются VM, а идут через `ScriptHost` (их обрабатывает
`DefaultScriptHost`); добавлен `gl::assets::cs_function_is_known` и общий
`gl/script/function_ids.hpp`; тесты на `user_input(1)` и делегирование stub-функций.
Тесты: `gl_script_tests` — 33 cases / 233 assertions; `gl_script_real_tests` (skip 77) —
`demon.d1.age.cs` + `zlato_vars.scr` → `wait_user`, `answers_count() == 2`,
`user_input(0)` продолжает без падения (end/wait), ids > 0. Полный `ctest` с
`GL_GAME_DIR` — **10/10 passed**, 565.78 s (real-data 516.06 s, gl_script_real 1.30 s).
Следующая задача — P3a Task 4 (tools).

**P3a — Task 2 (VM core + globals) завершён 2026-10-21.**
Добавлена SDL-free библиотека `gl_script`: `Value`, `ScriptGlobal`/`parse_globals`
(`int`/`DWORD`/`float`/`string`, `//`-комментарии, cp1251→UTF-8, строки без значения/битые
строки пропускаются с warning), `ScriptHost`, `ScriptVm` (opcodes 0/2, 6..11, 14..18,
21..24, 48..50; логика/сравнения/арифметика/assign/jmp; `D_Say`/`D_Answer` записываются,
`D_CloseDialog` → `end`, `Exit`/`Signal`/`Console`/`Cmd` — заглушки, остальные — host),
`restart(true/false)`, счётчик 10 000 000 шагов → `infinity`, безопасная обработка битых
индексов/опкодов. Семантика сверена с `GoldenLandEditor::CsExecutor` и реальными `.cs`
(`docs/research/p3a-cs-vm.md`, D-023). Тесты: `gl_script_tests` — 22 cases / 139
assertions; полный `ctest` с `GL_GAME_DIR` — **9/9 passed**, 594.42 s. Smoke-прогон
`demon.d1.age.cs` + `zlato_vars.scr` → `wait_user` (43 шага). Следующая задача — P3a
Task 3 (dialogs + native registry).

**P3a — Task 1 (`.cs` parser + name tables) завершён 2026-10-21.**
`gl_assets` получил `CsNode`, `parse_cs`, `cs_opcode_name`, `cs_function_name`,
`cs_opcode_valid`; строки cp1251→UTF-8; строгая проверка размера/опкодов/усечения;
func-аргументы до 9 с обязательным `-1`. Обнаружены 6 легаси-скриптов в `scripts.pak`
с XOR-обфускацией тела (`plain[i] = data[i] ^ ((i + 0x55) & 0xFF)`, `i >= 4`);
`parse_cs` распознаёт их fallback-разбором (D-022, spec §3; подтверждено копиями из
`scripts.update.3.pak`, см. `docs/research/p3a-cs-format.md`). Fix round 1: XOR-разбор
выполняется только при невалидном первом опкоде, ошибка объединяет обе попытки;
real-тест читает `scripts.pak` в память один раз. Тесты:
`gl_assets_tests` — 120 cases / 748 assertions; real-data — **555/555** `.cs`,
0 ошибок, 338 977 узлов (real `.cs` test 1.26 s). `cmake --build build` — успех;
полный `ctest` с `GL_GAME_DIR` — **8/8 passed**, 453.16 s. Следующая задача —
P3a Task 2 (VM core + globals).

**P2c — Task 4 (automated acceptance + docs) завершён 2026-10-20.**
`cmake --build build` — `ninja: no work to do` (exit 0); полный `ctest --test-dir build
--output-on-failure` с `GL_GAME_DIR` — **8/8 passed**, 395.96 s (real-data 366.67 s,
glview 17.44 s, glgame 11.55 s). `glgame --game-dir "I:\Games\Златогорье-2" --list-saves`
— exit 0, **28 строк**, slot0 `2022-11-06  Quick save  642409`; реальные заголовки
slot16 `Убмйство кукуя`, slot25 `Продаем артифакты` выводятся без изменений. P2c
automated acceptance passed. **Manual UI gate OPEN:** запустить
`glgame --game-dir "I:\Games\Златогорье-2"`, меню → `Continue` (активен при наличии
валидных слотов) → экран `Save slots`; проверить прокрутку списка при маленьком
разрешении, `Enter` → сообщение `world deserialization not implemented (P2c)`,
`Esc` → возврат в меню. Кириллица в заголовках отображается как `?` (ограничение
`stb_easy_font`, вынесено в P7). Следующая задача — P3 (скрипт-машина) или P1b.

**P2b — Task 4 (automated acceptance + docs) завершён 2026-10-20.**
`cmake --build build` — `ninja: no work to do`; полный `ctest --test-dir build
--output-on-failure` с `GL_GAME_DIR` — **8/8 passed**, 541.46 s (real-data 510.18 s,
glview 18.80 s, glgame 12.14 s). Все три screenshot-прогона `glgame` — exit 0,
PNG 1280×720 и не одноцветные: menu 38 273 B / 15 цветов, level 2 715 273 B /
115 244 цвета, paused 2 714 099 B / 115 177 цветов (SHA-256 в task-4-report).
P2b automated acceptance passed; manual interactive gate пройден пользователем
2026-10-20 (анимации `l1_1`, `gv_pause 1/0`). P1 manual desktop pass остаётся открытым.

**P2a — Task 7 (automated acceptance + docs) завершён 2026-10-20.**
`cmake --build build` — `ninja: no work to do`; полный `ctest --test-dir build
--output-on-failure` с `GL_GAME_DIR` — **8/8 passed**, 550.76 s (real-data 512.93 s,
glview 25.30 s, glgame 12.16 s). `glgame --menu --screenshot` и `--level l1_1
--screenshot` — exit 0, оба PNG 1280×720 (menu.png 38 273 B, 15 цветов;
level.png 2 715 273 B, 115 244 цвета; SHA-256 в task-7-report). Manual desktop gate
**пройден 2026-10-20** (подтверждено пользователем): New Game → `l1_1`, `~`, `help`,
`gv_pause`, `exit` на обычном рабочем столе с реальным вводом. P2a принята.

**P2a — Task 6 (`glgame` shell) завершён 2026-10-20.** Добавлен `tools/glgame`
(меню, уровень через `LevelScreen`, консоль `~`, команды, конфиг/`cl_username`
round-trip); `gl_core` отклоняет дубли имён команд. `--screenshot` меню/уровня,
`--exec`, коды выхода 0/1/2/3. Тесты `glgame_screenshot_tests` (skip 77 без
`GL_GAME_DIR`, управляемый temp INI): CLI `--width/--height` приоритетнее
`r_resolution`, меню и уровень 1280×720, `help`+unknown не фатальны, `cl_username`
сохраняется с `bind`/комментариями, `gv_pause`/`r_windowed` персистятся.
`d_console` применяется из конфига при старте, server-события проходят через
`ClientCore`. Полный `ctest` с `GL_GAME_DIR`: 8/8 passed (real-data 503.11 s,
glgame 11.22 s).

**P2a — Task 5 (state machine + ServerCore/ClientCore skeleton) завершён 2026-10-20.**
В SDL-free `gl_core` добавлены `StateMachine` (`boot→menu→level→menu`, `level` reload,
`exit` final) и `ServerCore`/`ClientCore`: `submit(Command)` + парсящий overload,
`tick`, `paused()`/`free_camera()`, `drain_events()`. Контракт `Event`:
`paused_changed` — `flag` = новое состояние; `config_changed` — `key`/`value`/`text`
(`flag` = bool для `r_windowed`); `log` — `text` = сообщение. `ServerCore` не пишет файлы;
`Config` применяет оболочка (Task 6). `level→level` — reload только на уровне state machine,
`ServerCore` не сбрасывается, reload мира принадлежит оболочке. Лишние аргументы
`gv_pause`/`r_windowed`/`cl_username` дают usage-log и ничего не меняют. Тесты
`test_state.cpp`/`test_server.cpp` в `gl_core_tests` зелёные.

P2a — Task 2 (реестр команд и консоль) завершён 2026-10-20: `CommandRegistry`
(регистрация, execute, `help` с сортировкой, автодополнение по префиксу),
`split_command_line` (пробелы, кавычки, экранирование `\"`/`\\`) и `ConsoleBuffer`
(вывод до 256 строк, история до 64 с навигацией и сбросом).

P2a — Task 1 (`gl_core` + Config) завершён 2026-10-20: `gl::assets::utf8_to_cp1251` и
SDL-free `gl::core::Config` (cp1251 INI, сохранение неизвестных строк/комментариев/`bind`,
порядка и смешанных CRLF/LF; `get`/`set` строк/int/float/bool).

P1 — Level viewer: автоматизированная приёмка завершена 2026-10-20; финальный manual
desktop pass (WASD/стрелки, `Home`, визуальный `F1`) остаётся открытым (Open Questions).

### История задач P1

Task 1 завершён: SDL2 (`release-2.32.10`, FetchContent),
`gl_platform` (`SdlContext`, `Window`), placeholder-таргеты `gl_render`/`gl_game`,
`glview` с `--help` и окном 1280×720 (закрытие по Esc/quit).
Проверено: сборка без предупреждений в first-party коде; `ctest` без `GL_GAME_DIR` —
2 passed + 1 skipped; `gl_assets_real_tests` с `GL_GAME_DIR` — 2/2 (32 assertions);
окно открыто и закрыто с exit 0. Коды выхода `glview`: `0` успех, `1` аргументы,
`2` I/O, `3` формат/загрузка.

Task 2 завершён: `gl::assets::parse_lvl` (`lvl.hpp`/`lvl.cpp`) разбирает 12 блоков LVL
в строгом порядке, строки cp1251→UTF-8, проверяет имена/размеры/counts/границы;
SENV/WTHR/DOOR валидируются без моделирования. После fix round: overflow-safe проверка
`BLK_MHDR`, структурная валидация SENV (path + 48-byte запись), D-017.
Тесты: `test_lvl.cpp` (unit 79 cases, 475 assertions) и `test_real_levels.cpp` —
175/175 реальных `levels/lvl/*.lvl` из `levels.pak`, 0 ошибок.
Полный `ctest` с `GL_GAME_DIR`: 3/3 passed.

Task 3 завершён: `gl::assets::parse_lao` (`lao.hpp`/`lao.cpp`) и `parse_sef`
(`sef.hpp`/`sef.cpp`). LAO: 33/33 `data/animated/*.lao` из `levels.pak` (33 pack-каталога),
0 ошибок; пустой файл допустим. SEF: `levels/single/l1_1/l1_1.sef` → pack `l1_1`;
реальный ключ `exit_to_globalmap` принимается как алиас `exit_to_global_map`, pack
нормализуется в ASCII-lowercase (D-018). Unit: 86 cases / 506 assertions.
Полный `ctest` с `GL_GAME_DIR`: 3/3 passed.

Task 4 завершён: `gl::assets::slice_csx_frames` (`csx_frames.hpp`/`csx_frames.cpp`) режет
декодированный CSX на вертикальные кадры фиксированной высоты; последний кадр может быть
короче. Отклоняются нулевые размеры, `frame_height == 0`, `frame_height > image.height`
и несоответствие размера RGBA-буфера. Unit: 90 cases / 547 assertions (+4 cases / +41).
Полный `ctest` с `GL_GAME_DIR`: 3/3 passed.

Task 5 завершён: `gl::game::load_level` (`level.hpp`/`level_loader.cpp`) загружает
SEF→pack, LVL, фон, статику и анимации из `GameVfs`; статика сортируется по `number`
(порядок отрисовки), отсутствующие необязательные файлы — warning и пропуск, фатальны только
SEF/LVL/фон. `gl_game` — статическая библиотека без SDL, зависит только от `gl_assets`.
Интеграционные тесты: `l1_1` — 2760×2016, 2 статики, 2 анимации, позиции, кадры и ceil-размер
соответствуют CSX/LAO; `l6_2_2` — статика в порядке `number`; отсутствующий уровень → io/format.
Полный `ctest` с `GL_GAME_DIR`: 3/3 passed (real-data 389.13 s).

Task 6 завершён: `gl_render` (`Texture`, `Camera`, `to_screen`, `draw_background`) и `glview`
с `--game-dir/--level/--width/--height/--screenshot/--frames`. Фон рисуется одним
`SDL_RenderCopyF` из видимой области текстуры, скриншот — `SDL_RenderReadPixels` RGBA →
`stbi_write_png`. Пути из `argv` конвертируются из UTF-8 (SDL2main на Windows). Render-тест
`glview_screenshot_tests`: `l1_1`, 1280×720, ≥2 цветов; без `GL_GAME_DIR` — skipped (77).
Полный `ctest` с `GL_GAME_DIR`: 4/4 passed (real-data 389.59 s); без `GL_GAME_DIR`: 2 passed,
2 skipped. Реальное окно открыто (процесс жив через 3 с), PNG с реальным драйвером — 1280×720.
Fix round 1: `gl_render` зависит от `gl_platform` + `gl_assets` (не от `gl_game`);
отсутствие обязательных SEF/LVL/фона возвращает `ErrorCode::format` (exit 3), добавлен
CLI-тест на пустой каталог; `parse_args` обёрнут в try/catch (exit 1). Полный `ctest`
после правок: 4/4 passed (real-data 392.88 s).

Task 7 завершён: `gl_render::draw_texture` (`SDL_RenderCopyF` в `to_screen`, пропуск
невидимых спрайтов) и отрисовка статики/анимаций в `glview`. Текстуры статики и кадров
анимаций создаются при загрузке (SDL-зависимость осталась только в app/render);
порядок — фон → статика (по `number`) → анимации. Кадр анимации:
`(elapsed_ms / delay_ms) % frames.size()`, при `delay_ms == 0` — первый кадр.
Интерактивный режим использует `SDL_GetTicks64`, скриншот — детерминированные
`elapsed_ms = N * 16`. Добавлен флаг `--background-only` (описан в `--help`).
Render-тест: normal `--frames 5` и `--background-only` — оба 1280×720, различие
> 1000 пикселей. Полный `ctest` с `GL_GAME_DIR`: 4/4 passed (real-data 392.15 s,
render 8.27 s).

Fix round 1: все текстуры создаются с `SDL_BLENDMODE_BLEND` (проверяется render-тестом),
`delay_ms == 0` всегда даёт кадр 0, viewport запрашивается один раз на кадр и передаётся
в `draw_texture`, предупреждения о пропущенных текстурах содержат уровень/номер/позицию,
тестовые PNG и временные каталоги освобождаются RAII. Полный `ctest` с `GL_GAME_DIR`:
4/4 passed (real-data 421.25 s, render 8.73 s).

Task 8 завершён (приёмка P1): в `glview` добавлены камера (WASD/стрелки `600/zoom`,
drag ЛКМ, wheel-зум к курсору `0.25…4.0` шагом `1.1`, `Home`, `Esc`), clamp к границам
карты (карта меньше окна центрируется), `F1` — рамки статики/анимаций, info-лог
`level '<name>' (pack '<pack>'): WxH px, N statics, M animations`.
Логика clamp/панорамы/зума вынесена в `gl_render` (`clamp_camera`, `pan_camera`,
`zoom_camera_at`), unit `camera_tests` — 6 cases / 31 assertion. Скриншот `l1_1`
1280×720: exit 0, PNG 3 122 294 байта, лог `2760x2016 px, 2 statics, 2 animations`.
Добавлен `--debug-overlay` для screenshot-режима (по умолчанию выключен, на `--screenshot`
без флага не влияет); render-тест снимает одинаковые кадры с флагом и без и проверяет
различие (рамки статики/анимаций нарисованы). Интерактивное окно открывается и
закрывается; drag-панорама и wheel-зум проверены на реальном уровне инъекцией мыши
(software renderer): отличие кадров 2932/14442 и 5383/14442 при фоновом различии
анимации 207/14442. WASD/стрелки, `Home` и визуальный `F1` toggle end-to-end не
прогонялись (в headless-сессии недоступен клавиатурный фокус): их код проверен ревью,
математика панорамы/зума — unit-тестами.
Полный `ctest` с `GL_GAME_DIR`: 5/5 passed (real-data 377.04 s, render 12.77 s).

Финальная волна правок P1 (2026-10-20): тайминг анимации вынесен в
`gl_render::animation_frame_index` с unit-тестами; screenshot-тест `--frames 1` vs `--frames 8`
подтверждает смену кадра; alpha-blend тест перенесён в негейтед `camera_tests`
(`SDL_VIDEODRIVER=dummy`). Полный `ctest` с `GL_GAME_DIR`: 5/5 passed (real-data 401.94 s,
render 18.73 s).

**P0 — Ассеты и форматы.** Реализация и приёмка завершены (Tasks 0–10).

Перед запуском `glpak` и тестов LLVM-MinGW `bin` должен быть в `PATH`
(иначе `0xC0000135` — не найдены runtime DLL).

Проверено на `I:\Games\Златогорье-2`:
- `cmake --build build` — успех, без предупреждений.
- `ctest --test-dir build --output-on-failure` (`GL_GAME_DIR` задан): unit 68/68
  (390 assertions), real-data 2/2 (32 assertions), glpak CLI 1/1 (12 assertions),
  0 skipped. Без `GL_GAME_DIR` real-data тест помечается skipped (exit 77).
- Real-data декодирование: `.csx` 19 704, `.sdb` 315, `.itm` 1848; 1 пустой `.csx`
  (`levels/pack/tl41/bitmaps/minimap.csx`) пропущен как отсутствующий ресурс.
- `glpak info` открывает все 16 архивов без ошибок; количества не изменились
  (scripts 4485, sdb 45, items 3786, engineres 1041).
- Полностью распакованы 4 обязательных архива, количества и размеры совпали:
  scripts 4485/9 324 686, sdb 45/1 411 188, items 3786/28 847 595,
  engineres 1041/62 293 207. Остальные базовые архивы распакованы не были: их суммы
  измерены по полю `size` FAT и сверены с фактической распаковкой этих четырёх.
- `glpak decode-csx --file engineres/globalmap/hero.csx` → `%TEMP%\hero.png`
  (27×27, 253 байта; исходный `hero.csx` — 390 байт). PNG проверен программно
  (загрузка через System.Drawing, размеры совпали); визуальная проверка не выполнялась.
  `engineres/editor/person.csx` → 32×81 PNG.

P0 закрыт 2026-10-19; далее выполнялись задачи P1 (см. выше).

## Критерии приёмки P0 (кратко)

- `gl_assets` собирается на Windows x64 (CMake + Ninja, C++20), не зависит от SDL/ОС.
- `glpak` открывает все `.pak` в `I:\Games\Златогорье-2\Data`, включая `*.update.*.pak`.
- Распаковка обязательных архивов совпадает по количеству файлов и суммарным размерам
  с эталоном; остальные архивы сверены по суммарному `size` из FAT.
- Декодеры CSX/SDB/ITM/BMP покрыты unit-тестами и не падают на всех файлах игры.
- `ctest` зелёный; результаты команд зафиксированы в этом файле.
- Обновлены `PROJECT.md`, `DECISIONS.md`, `docs/research/`.

## Сделано

- 2026-10-19: обследованы `I:\Games\Златогорье-2` и `I:\Games\Златогорье2 ХН`.
- 2026-10-19: выбран путь «чистая реконструкция C++20 + SDL2».
- 2026-10-19: декомпозиция P0–P10, протокол сохранения контекста, design-spec.
- 2026-10-19: создан git-репозиторий, коммит `cd8009e` опубликован в
  `https://github.com/Vologod/goldenland2dec_app` (ветка `main`).
  SSH: ключ `~/.ssh/id_ed25519_gl2`, repo-local `core.sshCommand`.
- 2026-10-19: написан план реализации P0 (`docs/plans/2026-10-19-p0-assets-formats.md`).
- 2026-10-19: Task 9 — CLI `glpak` (`info/list/extract/decode-*`), сборка LLVM-MinGW,
  smoke-тест на `I:\Games\Златогорье-2`, `ctest` зелёный.
- 2026-10-19: исправлен формат payload `.pak` (D-013) и правило `damage` в ITM (D-014);
  факты зафиксированы в `docs/research/formats.md`.
- 2026-10-19: Task 10 — `gl_assets_real_tests` (`GL_GAME_DIR`), приёмка распаковки
  и CSX-декодирования; P0 закрыт.
- 2026-10-19: P1 Task 1 — `cmake/SDL2.cmake` (FetchContent, SDL2 `release-2.32.10`),
  `gl_platform` (`SdlContext`/`Window`), `gl_render`/`gl_game` placeholders, `glview`.
- 2026-10-19: финальная волна правок P0 — продолжение batch-extract после битых
  записей (exit 3), CLI-тест, полное real-data декодирование `.csx/.sdb/.itm`,
  проверка размеров изображений до аллокации, валидация дубликатов/перекрытий PAK,
  контекст ошибок архив/файл/offset.
- 2026-10-19: P1 Task 2 — парсер LVL (`gl_assets::parse_lvl`), synthetic + real-data
  тесты (175/175 уровней), `ctest` 3/3.
- 2026-10-20: P1 Task 3 — парсеры LAO (`parse_lao`) и минимального SEF (`parse_sef`);
  33/33 LAO и `l1_1.sef` на реальных данных, `ctest` 3/3.
- 2026-10-20: P1 Task 4 — `slice_csx_frames`: нарезка CSX на кадры, unit 90/547, `ctest` 3/3.
- 2026-10-20: P1 Task 6 — `gl_render` (Texture/Camera/background) и `glview`
  (load/background/screenshot); render-тест на реальном `l1_1`, `ctest` 4/4.
- 2026-10-20: P1 Task 7 — `draw_texture`, текстуры статики/анимаций, детерминированный
  `--frames`, `--background-only`; render-тест на различие с фоном, `ctest` 4/4.
- 2026-10-20: P1 Task 8 — камера/ввод, `F1` overlay, `clamp_camera`/`pan_camera`/
  `zoom_camera_at` в `gl_render`, `camera_tests`; `--debug-overlay` и render-тест
  оверлея; автоматизированная приёмка P1 (`ctest` 5/5); открыт manual desktop pass.
- 2026-10-20: P2a Task 1 — `gl_core` (`Config`: cp1251 INI, комментарии/`bind`/порядок,
  byte-identical round-trip, `get`/`set`/`set_int`), `utf8_to_cp1251` в `gl_assets`,
  `gl_core_tests`; сборка без предупреждений, `ctest` без `GL_GAME_DIR`: 4 passed,
  2 skipped.
- 2026-10-20: P2a Task 1 fix round 1 — пустой `Config` и загрузка отсутствующего INI как
  defaults (с warning), `save` создаёт файл; real-data round-trip `GoldenLand.ini`
  (`gl_core_real_tests`, skip 77); `get_float` через `from_chars`; append использует
  доминирующий EOL; D-019 (`Result<void>`); план Task 1 отмечен `[x]`.
- 2026-10-20: P2a Task 2 — `CommandRegistry`/`split_command_line`/`ConsoleBuffer` в
  `gl_core`; тесты `gl_core_tests` 25 cases / 227 assertions; полный `ctest` без
  `GL_GAME_DIR`: 4 passed, 3 skipped.
- 2026-10-20: P2a Task 5 — `StateMachine` и `ServerCore`/`ClientCore` skeleton;
  `Command`/`Event` (`key`/`value`) контракт зафиксирован для Task 6; fix round 1
  добавил `submit(Command)`, usage-ошибки на лишние аргументы и тесты payload.
- 2026-10-20: P2a Task 6 — `glgame` (меню, `LevelScreen`, консоль с историей и
  автодополнением, команды `help`/`exit`/`d_console`/`gv_pause`/`gv_free_camera`/
  `r_resolution`/`r_windowed`/`cl_username`/`game_save`, сохранение Config);
  `glgame_screenshot_tests`, отказ от дублей команд в `gl_core`; `ctest` с
  `GL_GAME_DIR` 8/8.
- 2026-10-20: P2a Task 6 fix round 1 — CLI `--width/--height` приоритетнее
  `r_resolution`, `gv_pause` персистится, `d_console` применяется при старте,
  server-события идут через `ClientCore`, пустые строки не попадают в консоль/историю,
  результаты команд логируются; тесты на persistence и пустую историю; `ctest` 8/8.
- 2026-10-20: P2a Task 7 — automated acceptance: `ctest` 8/8 (550.76 s), menu/level
  screenshots 1280×720 (exit 0), research note `docs/research/p2a-game-shell.md`.
- 2026-10-20: P2a manual desktop gate пройден пользователем (меню, New Game → `l1_1`,
  `~`, `help`, `gv_pause`, `exit`); P2a принята.
- 2026-10-20: P2a final fix round — `r_resolution` идёт через `ServerCore`
  (`config_changed`), атомарная запись Config, `//` внутри значения не комментарий,
  отказ от `\r`/`\n` в `set`, New Game всегда `l1_1`, D-020 (отсутствующий INI =
  defaults + warning); `gv_free_camera`/`gv_skip_logo` помечены как P2a-заглушки.
- 2026-10-20: P2b Task 1 — фиксированный 60 Гц `ServerCore` (`tick_index`,
  `simulation_time_ms`, `snapshot`), лимиты `kMaxFrameDt=0.25`/`kMaxStepsPerTick=15`,
  защита от отрицательного и нефинитного `dt`; D-021, unit-тесты детерминизма/паузы.
- 2026-10-20: P2b Task 2 — `ClientCore::on_snapshot`/`snapshot`; snapshot не зависит
  от последующих тиков сервера.
- 2026-10-20: P2b Task 3 — `glgame` тикает сервер от frame delta (кап 250 мс), передаёт
  snapshot клиенту, рисует анимации по `simulation_time_ms`, `PAUSED` — из snapshot;
  ручной `animation_ms_` удалён, screenshot-режим остаётся детерминированным.
- 2026-10-20: P2b Task 4 — automated acceptance: `ctest` 8/8 (541.46 s), menu/level/paused
  screenshots exit 0, 1280×720, не одноцветные; research note
  `docs/research/p2b-deterministic-tick.md`.
- 2026-10-20: P2b manual interactive gate пройден пользователем (анимации `l1_1`,
  `gv_pause 1/0` замирание/возобновление); P2b принят.
- 2026-10-20: P2c Task 1 — `gl::assets::parse_save_header` (magic 20 B, version/date/times,
  title[64] cp1251→UTF-8, offset 40; короткий файл/битый magic → `format`), unit-тесты.
- 2026-10-20: P2c Task 2 — `gl::core::SaveManager` (скан `Saves`, `slot<N>.sav`
  регистронезависимо, сортировка, `valid=false` для битых/не-slot файлов, отсутствующий
  каталог → пустой список), unit и real-data тесты (28 слотов, version 17).
- 2026-10-20: P2c Task 3 — `glgame`: пункт меню `Continue`, экран `Save slots`
  (Up/Down/мышь, прокрутка, `Enter` → сообщение, `Esc` → меню), команда `saves`,
  CLI `--list-saves` (28 строк, exit 0), screenshot-тест меню.
- 2026-10-20: P2c Task 4 — automated acceptance: `ctest` 8/8 (395.96 s),
  `--list-saves` 28 строк / exit 0; research note `docs/research/p2c-save-slots.md`.
- 2026-10-20: P2c manual UI gate пройден пользователем (`Continue` → `Save slots`,
  прокрутка, `Enter`/`Esc`); P2c принят. P1 manual desktop pass остаётся открытым.
- 2026-10-21: P3a Task 1 — `gl_assets`: `parse_cs`/`CsNode`/таблицы имён опкодов и
  функций, cp1251→UTF-8, строгая валидация, fallback для 6 XOR-обфусцированных
  легаси-скриптов; `test_cs.cpp` и `test_real_cs.cpp` (555/555, 0 ошибок,
  338 977 узлов); research note `docs/research/p3a-cs-format.md`.
- 2026-10-21: P3a Task 1 fix round 1 — D-022 + spec §3 (два кодирования), XOR-fallback
  только при невалидном первом опкоде с объединённой ошибкой, real-тест читает
  `scripts.pak` через `PakArchive::open_from_memory` (1.26 s); тесты усечений
  опкода/func и битого legacy; `ctest` 8/8 (453.16 s).
- 2026-10-21: P3a Task 2 — `gl_script` (`parse_globals`, `ScriptHost`, `ScriptVm`):
  логика/сравнения/арифметика/assign/jmp/func, `D_Say`/`D_Answer`/`D_CloseDialog`,
  restart-семантика, лимит 10M шагов; D-023; research note `docs/research/p3a-cs-vm.md`;
  `gl_script_tests` 22 cases / 139 assertions; полный `ctest` 9/9 (594.42 s, по
  фактическому отчёту Task 2; ранее в STATUS было ошибочно указано 478.88 s).
- 2026-10-21: P3a Task 3 — `ScriptVm::dialogs_data()`/`answers_count()`/`user_input()`
  (ответы с нуля, `LastPhrase`/`LastAnswer`, `restart` на узле 0, warning при выходе за
  границы), `DefaultScriptHost` (0 + info/debug/warning по категориям), тесты синтетических
  диалогов и real `demon.d1.age.cs` + `zlato_vars.scr` (`gl_script_real_tests`, skip 77);
  Task 2 minors (правый операнд после ошибки левого, top-level `func` без `d`-перехода,
  unknown-opcode = `end` в spec/plan); D-024; `gl_script_tests` 31 cases / 197 assertions,
  `ctest` 10/10 (561.69 s).
- 2026-10-21: P3a Task 4 — `glpak disasm-cs` (stdout/`--out`, one line per node) и
  `glscript` (`--run`, repeatable zero-based `--answer`, status/steps/dialogs/answers,
  exit 0/1/2/3); `gl_cs_tools_tests` 8 cases / 56 assertions; `ctest` 11/11 (571.23 s).
- 2026-10-21: P3a Task 5 — automated acceptance + docs: build exit 0; `ctest` **11/11**
  (573.09 s, exit 0); real-data `parsed=555 failures=0 nodes=338977`; disasm
  `demon.d1.age.cs` 5353 B / 240 lines; `glscript --run` → `wait_user`, 43 steps,
  dialogs `2684 2686 2696`, 2 answers; `--answer 0` → second `wait_user`, 84 steps,
  dialogs `2719 1529 2721`, 2 answers. P3a принята.
- 2026-10-21: P3a final fix wave — eval budget, assign type coercion, opcodes
  `1/3..5/12/13/19/20`, sticky error semantics (stop + no log flood), VM-failure CLI
  test, globals BOM/unterminated-quote warning, multiple `D_Say` documented;
  `gl_script_tests` 49/374, `gl_script_real_tests` 1/20, `gl_cs_tools_tests` 8/56,
  полный `ctest` 11/11 (560.62 s).
- 2026-10-21: P3c Task 2 — `gl::script::DialogSession` (`dialog_session.hpp/.cpp`):
  `start`/`choose` гонят VM до `wait_user`/`end`/`infinity`, id→текст через `Sdb::find`,
  fallback `<phrase N>`, `waiting`/`npc_text`/`answers`; `choose` возвращает `argument`
  за границей; synthetic 5 cases и real `demon.d1.age.cs` + `dialogsphrases.sdb`
  (`gl_script_real_tests`, skip 77); `gl_script_tests` 54/411; full `ctest` 13/13 (606.15 s).
- 2026-10-21: P3c Task 2 fix round 1 — `vm.failed()` после `start`/`choose` → `ErrorCode::format`
  (`status()`/`failed()` остаются доступны); тесты end-path и падающей VM; real-тест проверяет
  `!vm.failed()` и не-ASCII текст; fallback `<phrase N>` покрыт для ответов; контракт
  `start()` (`restart(true)`) задокументирован в заголовке и Open Questions;
  `gl_script_tests` 58/443, `gl_script_real_tests` 2/44, full `ctest` 13/13 (536.01 s).
- 2026-10-21: P3c Task 3 — `glgame`: `--dialog`/`--font`, меню `Test Dialog`, экран
  диалога (панель, TTF-перенос, нумерованные ответы, Up/Down/мышь, Enter/клик, Esc),
  TTF для save-строк; `glscript --sdb` (`id<TAB>text`); research note
  `docs/research/p3c-dialogs-sdb-font.md`.
- 2026-10-21: P3c Task 3 fix round 1 — прямой `--dialog` выключает консоль независимо от
  `d_console`; шрифты `--font` → системный → ASCII; видимое сообщение `choose()` при
  `waiting`; подсказка `Esc to close` без ответов; вертикальный clamp длинного текста
  (скролл — backlog); тесты `--font`/fallback/отличие от меню/нижняя панель/`d_console`/
  реальная загрузка шрифта, 77 без шрифта; `gl_cs_tools_tests` 9/9 (67 assertions),
  `glgame_screenshot_tests` 11/11 (182), полный `ctest` 13/13 (629.23 s).
- 2026-10-21: P3c Task 3 fix round 2 — `--no-font` (ASCII dev/debug); цепочка шрифтов
  закреплена тестами A/B/C (другой системный TTF, ASCII, невалидный `--font` = дефолт);
  подсветка обрезается по `content_bottom`; `dialog_message_` в отдельной полосе ниже
  ответов; проба шрифта кэширована; `glgame_screenshot_tests` 13/13 (236), полный
  `ctest` 13/13 (607.05 s).
- 2026-10-21: P3c Task 4 — automated acceptance + docs: build exit 0; полный `ctest`
  13/13 (652.23 s), 318 test cases / 2649 assertions; `glscript --sdb` — `2684<TAB>Чего
  уставился?` и др.; dialog PNG 1280×720, 552 sampled / 771 unique colours, SHA-256
  `2544FFD6...`; P3c automated acceptance passed.
  Ограничение P2c с `?` в заголовках save-слотов закрыто TTF.
- 2026-10-21: P3c manual dialog UI gate пройден пользователем (кириллица, ответы,
  Up/Down/Enter/мышь, Esc); **P3c принят**.

## Следующие задачи

1. P4: персонажи/NPC (`.prs/.inf`), привязка диалогов к NPC — следующий подпроект.
2. P1b: Android/touch — отложенный follow-up P1.
3. P7-follow-up: свободный TTF для Android (P9), прокрутка длинных диалогов, layout-константы.

## Открытые вопросы

- P1: финальный manual desktop pass — WASD/стрелки, `Home` и визуальный `F1` toggle
  на обычном рабочем столе. В текущей headless-сессии клавиатурный фокус недоступен;
  drag-панорама и wheel-зум проверены инъекцией мыши, overlay — screenshot-тестом.
- P3c: `DialogSession::start()` intentionally calls `ScriptVm::restart(true)` — variables are
  rebuilt from globals and seeded runtime state is discarded. `start()`/`choose()` map
  `vm.failed()` to `ErrorCode::format`; `status()`/`failed()` remain available for diagnosis.
- Полная десериализация `.sav` и запись сохранений не реализованы (P2c — read-only каркас).
- SDL2 `release-2.32.10` выдаёт CMake deprecation warning из-за своего
  `cmake_minimum_required`; следить при обновлении CMake/SDL.
- `.clang-format` (LLVM base) не совпадает со стилем исходников P0; repo-wide
  форматирование не применялось.

## Backlog P1 (отложенные minor из ревью P0)

- PAK: `reserve(count)` без верхней границы; двойное копирование payload в памяти;
  `PakArchive::find` — O(n) с пересчётом ключа; `mz_uncompress` игнорирует хвост zlib;
  mount читает mode-заголовок каждого entry (I/O при монтировании).
- VFS: `.UPDATE.N.PAK` классифицируется как base (маркер регистрозависим); ошибки обхода
  каталогов проглатываются; дубликаты имён архивов резолвятся в первый; loose-файлы
  не попадают в `list()`/`extract --all`.
- SDB/ITM: копирование строк даже при наличии сигнатуры; `len==0`/дубликаты id не покрыты;
  строковые длины ограничены только размером файла (нет перевода `bad_alloc`).
- CSX/Image: `Result<CsxImage>` копирует пиксели; `bad_alloc` при построении `Error`
  вне guard; лишняя копия RGBA в CLI.
- CLI: недостижимый код выхода 4; `--prefix` не учитывает границу сегмента;
  тест не проверяет запись ошибки в stderr; ANSI-пути Windows.
- Сборка/тесты: `${CMAKE_SOURCE_DIR}` в tests (не embeddable); нет тестов `log`.

## Backlog P1b/P2 (после P1)

- Manual desktop pass: WASD/стрелки, Home, F1 (см. Open Questions).
- Screenshot/write_png: поддержка не-ASCII путей (wide/_wfopen или stbi_write_png_to_func + ofstream).
- SEF: учитывать brace depth/секции; отвергать `pack: ""`; интеграционный прогон всех `levels/single/*/*.sef`.
- LevelLoader: предупреждение при пустом LAO и непустых ADSC; отличать I/O от format для required-файлов; тесты missing optional.
- Память: освобождать CPU-копии Image после загрузки текстур (background/static/anim frames).
- Проверить соответствие `anim_<i>` ↔ `description.number` на уровне с несортированными/непоследовательными ADSC.
- Сборка без SDL2: опция `GL_BUILD_VIEWER` и условный FetchContent.
- Platform/render nits: SOFTWARE fallback без vsync; null-renderer ErrorCode; `<utility>` include; теоретический uint32 wrap в CSX-нарезке.
- CI: пометить real-data тест как slow / кэшировать фикстуры (~7 мин).

## Backlog P2a → P2b/P7

- P2b: детерминированный тик выполнен (D-021); отложены интерполяция движения и сеть.
- Вынести Game-логику из 	ools/glgame/main.cpp в тестируемый компонент (меню/консоль/состояния покрыты только subprocess-скриншотами и ручным gate).
- gv_free_camera: заглушка P2a — подключить к камере (отключение clamp/следования) или убрать.
- gv_skip_logo: no-op в P2a; удалить из активных ключей или реализовать при экране логотипа.
- Конфиг: валидировать ключи (не только значения) на CR/LF; атомарное сохранение уже есть.
- ServerCore _resolution: зафиксировать допустимые границы в контракте события (сейчас shell ограничивает, сервер — нет).
- Тесты: 	est_real_main возвращает 77 безусловно при отсутствии GL_GAME_DIR; text height scaling; non-ASCII UI — P7.
- Уточнить exit code в spec §7: отсутствующий обязательный аргумент CLI — 1, отсутствующий каталог — 2.

## Backlog P2c → P7

- Proper font / cp1251 glyphs: save-slot titles currently show `?` for Cyrillic bytes
  (`stb_easy_font` is ASCII-only); accepted P2c limitation, implement in P7.

## Backlog P2b → P2c

- Добавить determinism-тест «разные последовательности dt с одинаковой суммой → одинаковые tick/time».
- glgame: вынести sync-последовательность 	ick → on_snapshot → process events в helper.
- Screenshot-тесты проверяют размеры, не пиксели; золотые хэши — позже.
- Анимации стартуют от глобального simulation_time_ms; для one-shot анимаций уровня понадобится фаза/сброс.
- 	est_server: non-finite тест переупорядочить (Inf → NaN), чтобы он реально ловил регрессию.

## Backlog P3a → P3b/P4

- Bitwise operands:  xFFFFFFFF литерал через 	o_int32 теряет старший бит; уточнить семантику DWORD-литералов и >> (логический/арифметический).
- 	o_uint32(-1.0) → 0; при необходимости — модульный каст.
- Sticky-fatal: ошибки типов теперь останавливают VM; прогнать остальные реальные диалоги (не только demon.d1.age.cs), чтобы убедиться в отсутствии ложных остановок.
- coerce_assignment для необъявленной переменной создаёт int32 0; string→undeclared теперь фатален — подтвердить на реальных скриптах.
- AgeScript .scr компилятор/интерпретатор (P3b).
- Полный набор native-функций, диалоговый UI и SDB-фразы (P3c/P4).
- P3c: длинные диалоги/узкие окна — сейчас простой вертикальный clamp (лишние строки
  не рисуются); нужен scroll/пагинация ответов и NPC-текста.

## Backlog P3c → P4/P7/P9

- Длинные диалоги: прокрутка/пагинация текста и ответов; клавиатурный выбор только видимых ответов.
- Android: приложить свободный TTF (или путь --font); системные пути Windows — P9.
- Атлас: добавить U+20AC (€), U+0490/U+0491 (Ґ/ґ); учитывать лимит max texture size при больших pixel_height.
- SDL_RenderGeometry — поднять/проверить минимум SDL (2.0.18) и pragma stb_truetype для Clang/NDK.
- UI выбора диалога (список 555 .cs) и привязка к NPC — P4.
