# STATUS

Обновлено: 2026-10-20

## Текущая фаза

P0 завершена и принята. P1 — Level viewer: реализация и автоматизированная приёмка
завершены (2026-10-20); открыт финальный manual desktop pass (см. Open Questions).
Текущая фаза — **P2a: оболочка игры и консоль** (spec и план готовы).

## Текущая задача

**P2a — Task 1 (`gl_core` + Config) завершён 2026-10-20.** Добавлены
`gl::assets::utf8_to_cp1251` (обратная таблица, `?` для непредставимых code points) и
SDL-free `gl_core` с `gl::core::Config`: чтение/запись cp1251 INI с сохранением
неизвестных строк, комментариев, `bind`, порядка и смешанных CRLF/LF; `get`/`set`
строк/int/float/bool, inline-комментарии. Тесты `gl_core_tests` (Config) и обратное
преобразование cp1251 в `gl_assets_tests` зелёные. Следующее — P2a Tasks 2+.

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

## Следующие задачи

1. P2a: оболочка игры и консоль — `docs/specs/2026-10-20-p2a-game-shell-design.md`,
   `docs/plans/2026-10-20-p2a-game-shell.md` (Task 1 завершён; дальше Task 2).
2. P2b: ServerCore/ClientCore + детерминированный тик (после P2a).
3. P1b: Android/touch — отложенный follow-up P1.

## Открытые вопросы

- P1: финальный manual desktop pass — WASD/стрелки, `Home` и визуальный `F1` toggle
  на обычном рабочем столе. В текущей headless-сессии клавиатурный фокус недоступен;
  drag-панорама и wheel-зум проверены инъекцией мыши, overlay — screenshot-тестом.
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
