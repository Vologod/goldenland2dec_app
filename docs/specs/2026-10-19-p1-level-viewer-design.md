# Design: P1 — Level viewer (Windows)

Статус: **утверждён**.
Дата: 2026-10-19.
Базируется на: `docs/specs/2026-10-19-goldenland2-android-port-design.md`, `docs/DECISIONS.md`.

## 1. Цель

Загрузить реальный уровень «Златогорья 2» из оригинальных `.pak` и отрисовать его в окне SDL2
на Windows: пререндеренный фон + статические спрайты + анимированные спрайты, с камерой
(панорама/зум) и анимацией по `.lao`.

P1 — это фундамент рендера и загрузчика уровней для будущей игры (P2+). Android и тач —
следующие шаги (P1b/P9), не входят в эту задачу.

## 2. Границы

Входит:

- Парсеры `LVL`, `LAO`, минимальный `SEF` (поле `pack`).
- Нарезка `CSX` на кадры по высоте (для анимаций).
- `LevelLoader`: `SEF` → `pack` → `LVL` + фон + статика + анимации.
- SDL2-слой (окно, рендерер, ввод, время) на Windows.
- Рендер: фон, статика, анимации; камера scroll+zoom; nearest-neighbor.
- CLI `glview` с `--game-dir`, `--level`, `--screenshot`, `--width`, `--height`.
- Unit/integration/render-тесты.

Не входит:

- Герой, движение, коллизии, pathfinding.
- Маски/глубина (композиция «объект над героем»), тайловый рендер, light/weather.
- Триггеры, двери, NPC, скрипты.
- Звук.
- Android/touch.
- Редактор/выбор уровня через UI.

## 3. Подтверждённая модель данных (реальные файлы)

Уровень выбирается по имени `l1_1`:

- `levels/single/<name>/<name>.sef` — текстовый; содержит `pack: "<pack>"`.
- `levels/lvl/<pack>.lvl` — 12 блоков (`BLK_LVER`, `BLK_MPSZ`, `BLK_MHDR`, `BLK_MDSC`,
  `BLK_SDSC`, `BLK_ADSC`, `BLK_TDSC`, `BLK_CGRP`, `BLK_SENV`, `BLK_WTHR`, `BLK_DOOR`,
  `BLK_LFLS`). Формат: 8-байтовое имя блока, `u32 size`, payload.
- `levels/pack/<pack>/bitmaps/layer.jpg` — пререндеренный фон (для `l1_1` 2760×2016).
- `bitmaps/static/static_<number>.csx` — статические спрайты (`BLK_SDSC`).
- `bitmaps/animated/anim_<i>.csx` + `data/animated/<pack>.lao` — анимации (`BLK_ADSC`);
  `.lao` — пары `(u32 height, u32 delay)` на каждую анимацию.
- `bitmaps/masks/mask_<N>.csx`, `bitmaps/triggers/trigger_<N>.csx` — маски и триггеры
  (в P1 не рисуются, но данные парсятся).

`LVL`:

- `BLK_MPSZ`: `u32 pixelWidth`, `u32 pixelHeight`.
- `BLK_MHDR`: `u32 chunkWidth`, `u32 chunkHeight`, затем `chunkWidth*chunkHeight` чанков
  по 4 тайла (`u16 relief`, `u16 sound`, `u16 mask`). Тайл 12×9 px, чанк 24×18 px.
- `BLK_MDSC`: `u32 count`, затем `count × (u32 pad, u32 number, u32 x, u32 y)`.
- `BLK_SDSC`/`BLK_ADSC`/`BLK_TDSC`: `u32 count`, затем
  `count × (u16 param1, u16 param2, u32 number, i32 x, i32 y, string name)`.
- `BLK_CGRP`: `u32 count`, затем `string name`, `u32 cellsCount`, `cellsCount × (u16 x, u16 y)`.
- `BLK_SENV`: `i32`, `float×3`, `u32 soundCount`, 3 строки, затем звуки.
- `BLK_WTHR`: `u16 type`, `u16 intensity`.
- `BLK_DOOR`: `u32 count`, затем 6 строк на дверь.
- `BLK_LFLS`: `u32 levelFloors`.
- Строка: `u32 len` + cp1251 байты.

Позиции `LVL_Description.position` и `MaskDescription (x,y)` — **пиксели мира**;
спрайты рисуются от **левого верхнего угла** `(position.x, position.y)` (подтверждено
`GoldenLandEditor::LevelViewer::drawAnimations`).

Анимация: кадры нарезаются из `anim_<i>.csx` по `height` из `.lao`; `delay` — мс на кадр;
кадры зациклены (как `Animation::update` в редакторе).

## 4. Архитектура

Новые слои (зависимости только вниз):

```
app (glview) → game (level loader/view) → render → assets
                     ↘ platform (SDL2)
```

- `assets` (расширение): `lvl`, `lao`, `sef`, `csx_frames`.
- `platform`: инициализация SDL2, окно, рендерер, ввод, время, пути.
- `render`: `Texture` (RGBA8 → SDL_Texture), `Camera`, `draw_sprite`, `draw_background`.
- `game`: `Level`, `LevelLoader`, `LevelAnimation`.
- `app`: цикл, разбор аргументов, debug-оверлей, скриншот.

`assets` по-прежнему без SDL; `platform/render` — единственные места с SDL2.

### 4.1 SDL2

- Версия: `release-2.32.10`, подключается через CMake `FetchContent` (пин, `GIT_SHALLOW`).
- Сборка статически (`SDL_SHARED=OFF`, `SDL_STATIC=ON`, `SDL_TEST=OFF`).
- `SDL_Renderer` (2D), vsync; текстуры `SDL_PIXELFORMAT_RGBA32`, `SDL_ScaleModeNearest`
  (SDL2: `SDL_HINT_RENDER_SCALE_QUALITY=0`).
- Окно по умолчанию 1280×720, resizable.

## 5. Рендер и камера

- Мир — пиксели фона (`mapWidth × mapHeight`).
- `Camera { float x, y, zoom; }` (x,y — левый верхний угол видимой области в мире).
- Экранная точка: `(world - camera.xy) * zoom`.
- Порядок: фон → статика (по `number`) → анимации (по `number`).
- Спрайты рисуются с `SDL_RenderCopyF` в прямоугольник
  `((pos - cam) * zoom, size * zoom)`; `SDL_RenderSetScale` не используется (чтобы
  сохранить точность для фона и спрайтов).
- Фон рисуется одним `SDL_RenderCopyF` с source rect = видимая область (для больших карт).
- Zoom: 0.25…4.0, шаг колеса 1.1; зум к курсору.
- Панорама: WASD/стрелки (скорость `600/zoom` px/с), drag ЛКМ.
- Ограничение камеры: не выходить за пределы карты (с допуском на экран).

## 6. CLI `glview`

```
glview --game-dir <dir> [--level <name>] [--width <px>] [--height <px>]
       [--screenshot <png>] [--frames <n>]
```

- `--level` по умолчанию `l1_1`.
- `--screenshot`: отрисовать кадр(ы) и сохранить PNG (через `SDL_RenderReadPixels` +
  `stbi_write_png`), затем выйти. Для CI/проверки.
- `--frames`: сколько кадров отрисовать перед скриншотом (по умолчанию 1; для анимаций).
- Коды выхода: `0` успех, `1` аргументы, `2` I/O, `3` формат/загрузка.

## 7. Ошибки и логирование

- Нет каталога игры / уровня → понятная ошибка, exit 2/3.
- Отсутствует `layer.jpg` или `LVL` → фатально (exit 3).
- Отсутствует отдельная статика/анимация → warning, пропустить объект, продолжить.
- Ошибки формата — с указанием файла/блока/offset.
- Логи через `gl::log`.

## 8. Тестирование

Unit (синтетика):

- `LVL`: все 12 блоков, границы, строки cp1251, битые размеры.
- `LAO`: пары height/delay, неверный размер файла.
- `SEF`: извлечение `pack`, комментарии/пробелы.
- `CSX`-нарезка: высота/количество кадров, остаток.

Integration (`GL_GAME_DIR`):

- распарсить все `levels/lvl/*.lvl` из `levels.pak` (175) — 0 ошибок;
- распарсить все `data/animated/*.lao` — 0 ошибок;
- для `l1_1` загрузить фон, 2 статики, 2 анимации; проверить размеры/кадры.

Render:

- `glview --level l1_1 --screenshot out.png --width 1280 --height 720`;
  проверить, что PNG создан, размер 1280×720, и в нём есть неоднородные пиксели
  (не сплошной цвет).

## 9. Критерии готовности

1. Windows x64: `cmake --build build` без предупреждений.
2. `glview --game-dir I:\Games\Златогорье-2 --level l1_1` показывает реальный уровень:
   фон, 2 статики, 2 анимации; панорама/зум работают.
3. `--screenshot` создаёт корректный PNG.
4. Unit + integration + render-тесты зелёные.
5. `assets` не зависит от SDL; `platform/render` — единственные SDL-слои.
6. Документы обновлены (`STATUS`, `PROJECT`, `research`).

## 10. Риски

- Точные anchor-правила для статики могут отличаться от «левый верхний угол» — проверить
  визуально на `l1_1` и при расхождении зафиксировать поправку.
- Размер фона 2760×2016 (3.3 МБ JPEG) — держать одну текстуру, не копировать на кадр.
- SDL2 через FetchContent требует сеть при первой конфигурации.
- Скриншот-тест на CI без дисплея: использовать `SDL_VIDEODRIVER=dummy` + software renderer;
  если недоступно — пометить тест как Windows-only.

## 11. Отложено

Маски/глубина, тайловый рендер, герой и движение, триггеры/двери, звук, Android/touch,
выбор уровня через UI, миникарта.
