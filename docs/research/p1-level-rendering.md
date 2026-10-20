# Исследование: рендер уровня (P1)

Дата: 2026-10-20. Источники: реальные файлы `I:\Games\Златогорье-2` (VFS `levels.pak`),
`levels/lvl/*.lvl`, `levels/pack/l1_1/*`, реализация `gl::game::load_level` и
интеграционные тесты `tests/assets/test_level_loader.cpp`; сверка с
`DarkContact/GoldenLandEditor` (`LevelViewer::drawAnimations`, `Animation::update`).

## Выбор уровня и pack

- `levels/single/<name>/<name>.sef` содержит `pack: "<pack>"`; имя pack приводится
  к нижнему регистру. Для `l1_1` pack — `l1_1`.
- Все пути ресурсов pack строятся от `levels/pack/<pack>/`.

## Фон

- `levels/pack/<pack>/bitmaps/layer.jpg` — пререндеренный JPEG, пиксели мира.
- `l1_1`: 2760×2016 px (совпадает с `BLK_MPSZ`).
- В `glview` фон — одна `SDL_Texture`; кадр рисуется одним `SDL_RenderCopyF` из
  видимой области (source rect), без копирования текстуры.

## Статика

- `levels/pack/<pack>/bitmaps/static/static_<number>.csx`; `<number>` — из `BLK_SDSC`.
- `l1_1`: 2 статики в позициях (905, 368) и (2163, 61) px.
- Позиция `(x, y)` — пиксели мира; спрайт рисуется от **левого верхнего угла**.
- Порядок отрисовки — по возрастанию `number`.

## Анимации

- `levels/pack/<pack>/data/animated/<pack>.lao` — пары `(u32 height, u32 delay)`.
- Кадры: `levels/pack/<pack>/bitmaps/animated/anim_<i>.csx`, нарезаются по вертикали
  на кадры высотой `height` из записи `i` LAO; последний кадр может быть короче.
- `delay` — миллисекунды на кадр; кадры зациклены:
  `(elapsed_ms / delay) % frame_count`. При `delay == 0` всегда рисуется кадр 0.
- `l1_1`: 2 анимации; `anim_0` — height 93, delay 100 мс; `anim_1` — height 257,
  delay 100 мс.
- Позиция — также левый верхний угол в пикселях мира; порядок — по `number`.

## Тайлы и чанки (LVL)

- `BLK_MPSZ` — размер мира в пикселях (`u32` × 2).
- `BLK_MHDR` — `chunkWidth × chunkHeight` чанков; один чанк = 4 тайла
  (2×2), тайл 12×9 px, чанк 24×18 px.
- В P1 фон уже пререндерен, тайлы/маски/глубина не рисуются; данные разбираются.

## Порядок кадра и камера (P1)

- Порядок: фон → статика (по `number`) → анимации (по `number`).
- `Camera { x, y, zoom }`, `(x, y)` — левый верхний угол видимой области в мире.
- Экран: `(world - camera.xy) * zoom`; зум `0.25…4.0`, шаг колеса `1.1` к курсору.
- Камера ограничивается границами мира: `[0, world - view]`; если карта меньше
  экрана — центрируется. Диагональное движение нормализуется.
- Панорама: WASD/стрелки `600 / zoom` px/с; drag ЛКМ — на delta мыши / zoom.
