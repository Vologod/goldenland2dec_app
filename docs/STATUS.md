# STATUS

Обновлено: 2026-10-21

## Текущая фаза

P0 завершена и принята. P1 — Level viewer: реализация и автоматизированная приёмка
завершены (2026-10-20); открыт финальный manual desktop pass (см. Open Questions).
P2a — оболочка игры и консоль: реализация, автоматизированная приёмка и manual desktop
gate завершены (2026-10-20); P2a принята. P2b — детерминированный тик: реализация,
автоматизированная приёмка и manual interactive gate завершены (2026-10-20); P2b принята.
P2c — save/load каркас: реализация и автоматизированная приёмка завершены (2026-10-20);
открыт manual UI gate (Continue → слоты, Esc). P3a — CS VM: Task 1 (парсер `.cs`)
завершён 2026-10-21. Следующие подпроекты — продолжение P3a или P1b (Android/touch);
P2 backfill отложен.

## Текущая задача

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

## Следующие задачи

1. P3a: CS VM core — Task 1 (парсер `.cs`) завершён; следующая — Task 2 (VM + globals)
   (`docs/specs/2026-10-20-p3a-cs-vm-design.md`, `docs/plans/2026-10-20-p3a-cs-vm.md`).
2. P3b/P3c: AgeScript-компилятор, диалоги/SDB, полный набор native-функций.
3. P1b: Android/touch — отложенный follow-up P1.
4. P2 backfill: интерполяция/сеть/игровые системы — отложены до появления сущностей.

## Открытые вопросы

- P1: финальный manual desktop pass — WASD/стрелки, `Home` и визуальный `F1` toggle
  на обычном рабочем столе. В текущей headless-сессии клавиатурный фокус недоступен;
  drag-панорама и wheel-зум проверены инъекцией мыши, overlay — screenshot-тестом.
- P2c: Cyrillic/cp1251 save-slot titles render as `?` in `glgame` because `gl_render` uses
  `stb_easy_font` (ASCII-only). Accepted P2c limitation; a proper font with cp1251 glyphs is
  tracked as P7 backlog.
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
