# Research: P3c dialogs, SDB phrases and TTF text

Дата: 2026-10-21. Источник данных: `I:\Games\Златогорье-2` (VFS учитывает
`*.update.3.pak`). Код: `gl_render::Font`, `gl_script::DialogSession`, `glgame`,
`glscript`.

## SDB и связка с `.cs`

- `sdb/dialogs/dialogsphrases.sdb` — 12 040 записей; `Sdb::parse` читает подпись `SDB `
  и без подписи (XOR `0xAA`), строки cp1251 → UTF-8.
- `scripts/dialogs/demon.d1.age.cs`: первый останов `wait_user` даёт
  `dialogs_data() = [2684, 2686, 2696]`, `answers_count() == 2`.
  `DialogSession` берёт первый id как фразу NPC, остальные — как ответы.
- Фактические тексты (проверено `glscript --sdb`):
  - `2684` — `Чего уставился?`
  - `2686` — `Я не думал, что демоны разумны...`
  - `2696` — `Ничего. Я пошел...`
- После `user_input(0)` второй останов:
  `dialogs_data() = [2719, 1529, 2721]`, `answers_count() == 2`.
  - `2719` — длинная реплика демона о плате Короля Гуннара.
  - `1529` — `Понятно. Ну, пока.`
  - `2721` — `Где ты научился так разговаривать?`
- Пример из spec §3 (`2684` = «Кто ты, старик?») не совпал с целевой установкой;
  фактическая строка — `Чего уставился?`. Это данные игры, парсер ничего не меняет.
- Отсутствующий id не фатален: `DialogSession` подставляет `<phrase N>`.

## TTF-шрифт

- `third_party/stb/stb_truetype.h` (stb commit `013ac3beddff3dbffafd5177e7972067cd2b5083`).
- `gl::render::Font::load_from_file` собирает атлас RGBA: ASCII `U+0020..U+007E`,
  кириллица `U+0400..U+04FF`, пунктуация `U+2000..U+206F`; `pixel_height` ограничен
  `[8, 256]`.
- `draw_text`/`measure`/`wrap` используют один и тот же UTF-8 декодер и нормализацию
  control-символов (`\t` → 4 пробела, `\r`/C0/C1 игнорируются, `\n` сохраняется).
- `find_system_font()` проверяет `C:\Windows\Fonts\segoeui.ttf`, `arial.ttf`,
  `consola.ttf` (и `%WINDIR%\Fonts`). В `glgame` приоритет: `--font <ttf>` →
  системный шрифт → ASCII `stb_easy_font` (не-ASCII → `?`). Если `--font` задан, но
  не загрузился, `glgame` пробует системный шрифт, а затем ASCII; невалидный `--font`
  не фатален (warning в лог). `--no-font` — dev/debug-флаг: принудительно использует
  ASCII-текст и перекрывает `--font`/системный шрифт.
- Шрифт не коммитится. Android/P9 должен приложить свободный TTF и передать `--font`.
- В `glgame` шрифт загружается с высотой 20 px; им рисуются диалог, заголовки
  save-слотов и строки слотов, поэтому кириллические title больше не превращаются в `?`.

## Экран диалога (`glgame`)

- `--dialog <virtual/path.cs>` читает globals (`scripts/dialogs_special/zlato_vars.scr`),
  SDB (`sdb/dialogs/dialogsphrases.sdb`) и `.cs`, создаёт `ScriptVm` + `DialogSession`
  и открывает экран сразу, минуя меню. В этом режиме `d_console` из INI игнорируется:
  консоль принудительно выключается на старте (интерактивный `~` по-прежнему работает).
- Меню: пункт `Test Dialog` появляется только если VFS находит
  `scripts/dialogs/demon.d1.age.cs`; он открывает тот же экран.
- Панель — нижние 55% экрана (не меньше 220 px), тёмный фон, золотая граница сверху.
  Фраза NPC переносится по ширине панели; ответы нумеруются с 1, выбранный подсвечен.
- Управление: `Up`/`Down` и мышь — выбор, `Enter`/клик — `choose`, `Esc` — меню
  (если диалог открыт из меню) или выход (если запущен через `--dialog`).
- Если `waiting()` ложно или ответов нет, показывается сообщение и подсказка `Esc`;
  при отсутствии ответов подсказка сокращается до `Esc to close`. Сообщение об ошибке
  `choose()` (`dialog_message_`) рисуется и при `waiting()`.
- Текст и ответы ограничены по вертикали (`dialog_content_bottom()`): строки, не
  помещающиеся в панель, не рисуются; прямоугольник подсветки выбранного ответа тоже
  обрезается по `content_bottom`. При видимом `dialog_message_` (ошибка `choose()`)
  нижняя граница контента поднимается, а сообщение рисуется в отдельной полосе ниже
  ответов, не перепечатывая последний ответ. Полноценный скролл не реализован —
  вынесен в backlog.
- `--screenshot --dialog ...` рендерит детерминированный кадр 1280×720. Проверка
  Task 3 на целевом конфиге (`--config <tmp.ini>`, `d_console 0`, `--width 1280
  --height 720`): PNG 49 484 байта, 511 цветов (выборка каждые 2 px), SHA-256
  `0CA504587996AE0A7345C0754C940DDB6730AC732FD562CD5C56622A8A82A56C`.
- В приёмочном прогоне Task 4 без `--config` используется реальный `GoldenLand.ini`;
  кадр отличается от чистого конфига (54 332 байта, 552 цвета при выборке каждые 2 px,
  771 уникальный цвет, SHA-256
  `2544FFD6F2A4B5729EF43413C26D20862B669E62F6C8CC71875EB45E65F2BFE1`), но два
  последовательных прогона побайтово совпали. Это различие конфигурации, не
  недетерминированность рендера.

## `glscript --sdb`

- `--sdb <virtual/path.sdb>` читает SDB через VFS и после каждого состояния
  (`--run` и каждый `--answer`) печатает по строке на id из `dialogs_data()`:
  `<id>\t<text>`; отсутствующий id — `<phrase N>`.
- Пример: `glscript ... --file scripts/dialogs/demon.d1.age.cs --run --sdb
  sdb/dialogs/dialogsphrases.sdb` → `status: wait_user`, `steps: 43`,
  `dialogs: 2684 2686 2696`, `answers: 2`, затем три строки `id<TAB>текст`.
- Коды выхода не изменились: 0 успех, 1 аргументы, 2 I/O, 3 формат/VM.

## Automated acceptance (Task 4, 2026-10-21)

- `cmake --build build` — `ninja: no work to do`, exit 0.
- `ctest --test-dir build --output-on-failure` с `GL_GAME_DIR=I:\Games\Златогорье-2`
  — **13/13 passed**, 652.23 s, exit 0; суммарно 318 test cases / 2649 assertions,
  0 failed. `gl_assets_real_tests` — 562.27 s, `glgame_screenshot_tests` — 42.46 s.
- `glscript --game-dir "I:\Games\Златогорье-2" --file
  scripts/dialogs/demon.d1.age.cs --run --sdb sdb/dialogs/dialogsphrases.sdb` — exit 0:
  `status: wait_user`, `steps: 43`, `dialogs: 2684 2686 2696`, `answers: 2`, затем
  `2684<TAB>Чего уставился?`, `2686<TAB>Я не думал, что демоны разумны...`,
  `2696<TAB>Ничего. Я пошел...`; два ожидаемых warning
  `globals: line 918/1262: missing value`.
- `glgame --game-dir "I:\Games\Златогорье-2" --dialog
  scripts/dialogs/demon.d1.age.cs --screenshot %TEMP%\p3c_dialog.png` — exit 0,
  шрифт `C:\Windows\Fonts\segoeui.ttf`, console=0; PNG 1280×720, 54 332 байта,
  552 цвета (выборка каждые 2 px), SHA-256 `2544FFD6F2A4B5729EF43413C26D20862B669E62F6C8CC71875EB45E65F2BFE1`.
- **Manual dialog UI gate OPEN:** читаемость кириллицы, выбор ответа (Up/Down,
  Enter/мышь) и `Esc` в реальном окне должен проверить человек; automated acceptance
  этого не заменяет.

## Тесты

- `gl_cs_tools_tests`: `--sdb` на `demon.d1.age.cs` — exit 0, в выводе есть id `2684`
  и не-ASCII текст (не `<phrase 2684>`).
- `glgame_screenshot_tests`: `--dialog ... --screenshot` — 1280×720, ≥2 цветов,
  кадр отличается от меню, нижние 40% содержат ≥10% не-фоновых пикселей и ≥8 цветов;
  конфиг с `d_console 1` даёт такой же кадр, как `d_console 0` (консоль выключена).
- Цепочка шрифтов закреплена тестами: `--font` с *другим* системным шрифтом
  (`consola.ttf`, если системный — `segoeui.ttf`) даёт кадр, отличающийся от
  дефолтного; `--no-font` даёт ASCII-кадр, отличающийся от TTF; невалидный `--font`
  даёт побайтово тот же кадр, что и дефолтный системный шрифт.
- `font_available()` в тесте проверяет реальную загрузку `Font::load_from_file`, а не
  только наличие файла, и кэшируется; alternate-шрифт тоже кэшируется. Без системного
  шрифта бинарник возвращает 77 после прогона (как `tests/render/test_font.cpp`); без
  `GL_GAME_DIR` — 77 сразу.
- `gl_assets`/`gl_script` остались без SDL.
