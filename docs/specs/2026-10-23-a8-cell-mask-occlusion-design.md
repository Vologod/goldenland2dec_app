# A8: cell mask / foreground occlusion — дизайн

## 1. Цель

Воспроизвести перекрытие персонажей foreground-объектами уровня (пример —
игрок за перилами деревянной вышки в `Shot0001.bmp`). Оригинал хранит
`BLK_MDSC` (`number,x,y`) и `levels/pack/<pack>/bitmaps/masks/mask_<number>.csx`;
alpha=0-силуэт маски — occluding foreground, координаты — левый верхний угол.

Основание: `docs/research/p4b2-render-order.md` (A8), `Shot0001.bmp`,
`mask_45.csx` (687×741, (885,1275)), red-overlay alignment.

## 2. Поведение

- Маски загружаются для уровня из `LvlData.masks`; отсутствующая/битая маска —
  warning, уровень продолжает грузиться.
- Для каждой маски строится RGBA-текстура `foreground`:
  - `mask.alpha < 128` → пиксель фона `background[x+px, y+py]`, alpha=255;
  - иначе alpha=0.
- `foreground` — мировой объект; baseline = `y + height`, tie-break X =
  `x + width/2`.
- При отрисовке тел (проход `body`) маски и персонажи идут одним списком по
  baseline: персонаж с `y < mask.baseline` рисуется раньше и перекрывается
  маской; персонаж с `y > mask.baseline` рисуется позже и остаётся сверху.
  Тени (`shadow`) обрабатываются так же отдельным проходом.
- Статики/LAO-анимации остаются до динамических проходов.
- Порядок при полном равенстве baseline — персонаж раньше маски (маска
  перекрывает); зафиксировать в DECISIONS.

## 3. Границы

Входит:

- `LevelMask` в `gl::game::Level`, загрузка `mask_<number>.csx`;
- `compose_mask_foreground` (чистая функция, тестируемая без SDL);
- merge масок в проходы теней/тел в `LevelScreen`;
- `--camera-x/--camera-y` в `glview` для screenshot-проверки выравнивания;
- unit/real-тесты, docs.

Не входит:

- точная семантика `ApplyCellMask`/флагов ячеек (`+0x8/+0xc`) и `r_masks_*`;
- `alt/mask_<number>.csx` (в l1_1 отсутствует; fallback не реализуется);
- маски для статиков/LAO отдельным проходом (они остаются в фоне);
- hero/player и интерактивный gate (появятся позже).

## 4. Архитектура

### 4.1 `gl::game::Level`

```cpp
struct LevelMask {
    std::uint32_t number = 0;
    std::int32_t x = 0, y = 0;
    gl::assets::Image image;
};
struct Level {
    ...
    std::vector<LevelMask> masks;
};
```

`load_level`: после `level.background` пройти `lvl->masks`, прочитать
`levels/pack/<pack>/bitmaps/masks/mask_<number>.csx`, декодировать CSX,
сложить в `level.masks`. Ошибка чтения — warning и пропуск.

### 4.2 `gl::app::compose_mask_foreground`

```cpp
gl::assets::Image compose_mask_foreground(const gl::assets::Image& background,
                                          const gl::assets::Image& mask,
                                          int x, int y);
```

Выход того же размера, что `mask`; alpha=0 вне силуэта; для каждого
`mask.alpha < 128` берётся фон в мире `(x+px, y+py)`, если он в границах.
Пустая/нулевая маска → пустой Image.

### 4.3 `LevelScreen`

```cpp
struct MaskOverlay {
    gl::render::Texture texture;
    std::int32_t x, y;
    float baseline;
    float center_x;
};
std::vector<MaskOverlay> masks_;
std::vector<std::size_t> mask_order_;
```

- В `load` после текстур уровня: для каждой `LevelMask` вызвать
  `compose_mask_foreground`, создать текстуру, baseline/center_x, отсортировать
  `mask_order_` по `(baseline, center_x)`.
- В `render` для каждого прохода (`shadow`, `body`) слить
  `person_order_` и `mask_order_`:
  - ключ персонажа — `(movement.y, movement.x)`;
  - ключ маски — `(baseline, center_x)`;
  - меньшее — рисовать; равенство — персонаж раньше.
- Маски рисуются как обычные текстуры в мировых координатах через
  `draw_texture`.
- `LevelScreenOptions::draw_masks` (по умолчанию `true`) отключает маски для
  A/B-проверки.

### 4.4 `glview`

Добавлены `--camera-x <px> --camera-y <px>` (screenshot-режим):
после `LevelScreen::load` и `set_viewport` вызывается
`screen.pan(target - camera)`. Для проверки occlusion добавлены
`--place-person <index> <tile_x> <tile_y>` (телепорт через
`LevelSimulation::place_person`) и `--no-masks`.

## 5. Тесты

- Unit `compose_mask_foreground`:
  - alpha=0 → пиксель фона, alpha=255;
  - alpha=255 → alpha=0;
  - выход за границы фона → alpha=0;
  - пустые/нулевые размеры → пустой Image.
- Real `l1_1`:
  - `level.masks.size() == 62`;
  - `mask_45` 687×741, x=885, y=1275;
  - `compose_mask_foreground` для mask_45 даёт ровно **182090** непрозрачных
    пикселей (значение из red-overlay проверки);
  - текстура mask_45 создаётся без ошибок.
- Screenshot: `glview l1_1 --camera-x 885 --camera-y 1275 --screenshot` —
  PNG 1280×720, exit 0; вышка не «двоится» относительно фона (визуальная
  проверка). Полный `ctest` зелёный.
- Manual gate: пользователь проверяет, что при заходе персонажа за перила
  нижняя часть скрыта; в порте пока нет управляемого героя, поэтому это
  возможно только после появления player/hero (P4c/P5) — отметить.

## 6. Приёмка

- `cmake --build build` — успех.
- `ctest` с `GL_GAME_DIR` — все тесты.
- Real-тест маски 45 проходит.
- `Shot0001` сопоставлен с `mask_45` (red-overlay совпал); ограничение
  интерактивной проверки зафиксировано.

## 7. Риски

- Порядок «маска vs персонаж» при равном baseline и точное поведение теней
  могут отличаться от `ApplyCellMask`; проверка — visual gate.
- `alt`-маски и режимы `r_masks_mode` не реализованы.
- Глобальная маска может перекрыть объект, который в оригинале был бы поверх;
  merge по baseline смягчает, но не доказывает полный паритет.
