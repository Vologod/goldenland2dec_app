# STATUS

Обновлено: 2026-10-19

## Текущая фаза

P0 завершена и принята. Следующая фаза — P1: рендер изометрии и уровень.

## Текущая задача

**P0 — Ассеты и форматы.** Реализация и приёмка завершены (Tasks 0–10).

Проверено на `I:\Games\Златогорье-2`:
- `cmake --build build` — успех, без предупреждений.
- `ctest --test-dir build --output-on-failure` (`GL_GAME_DIR` задан): unit 63/63,
  real-data 1/1 (9 assertions), 0 skipped.
- `glpak extract --archive`: scripts 4485/9 324 686, sdb 45/1 411 188,
  items 3786/28 847 595, engineres 1041/62 293 207.
- `glpak decode-csx --file engineres/globalmap/hero.csx` → `%TEMP%\hero.png`
  (27×27, 253 байта; исходный `hero.csx` — 390 байт), PNG проверен через System.Drawing.
  `engineres/editor/person.csx` → 32×81 PNG.

Следующая задача: P1 — рендер изометрии и уровень.

## Критерии приёмки P0 (кратко)

- `gl_assets` собирается на Windows x64 (CMake + Ninja, C++20), не зависит от SDL/ОС.
- `glpak` открывает все `.pak` в `I:\Games\Златогорье-2\Data`, включая `*.update.*.pak`.
- Полная распаковка совпадает по количеству файлов и суммарным размерам с эталоном.
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

## Следующие задачи

1. P1: рендер изометрии и загрузка уровня.

## Открытые вопросы

- Строгий запрет хвостовых байт в ITM: пересмотреть, если реальные файлы окажутся с padding.
