# P4d — hero wear render (design)

Дата: 2026-10-25. Статус: реализовано (подход 1: композит листов при загрузке); automated acceptance passed, manual gate OPEN. Источник истины по форматам и адресам:
`docs/research/p4d-hero-wear.md`; решения — D-041…D-043 (D-043 добавить при
завершении).

## 1. Цель

Заменить эмпирический hero-рендер (`wear/noweapon_thrw/animation/tb_stay.csx`
+ подобранные вручную offsets в `level_screen.cpp`) на engine-схему из
`GoldenLand.exe`: HAD + IAD + SEQ + композитор `FUN_0042279C`. Hero должен
рисоваться из `wear.pak` с якорем `anchor_x/anchor_y` и слоями экипировки,
совпадая с оригиналом для эталонного slot0-набора.

## 2. Scope

- Парсеры `HAD`, расширенный `IAD`, `SEQ` в `gl_assets`.
- SDL-free загрузчик и композитор `HeroVisual` в `gl_game`.
- Рендер hero в `LevelScreen`: тело, тень, idle (`tb_stay`) и walk (`tb_go`),
  направления/зеркала как у PAD, кадры по HAD `delay`.
- `glview`/`glgame`: явный outfit (набор + 4 слота); default — slot0.
- Unit/synthetic, real-data, golden-композит, screenshot-тесты; полный `ctest`.
- Manual gate пользователя.

## 3. Non-goals

- Чтение надетых предметов из save и маппинг item→tech/`herowear.scr`
  (P2c/P5): outfit задаётся явно; маппинг — отдельная задача.
- Состояния `hits*/die/suffer/attack`, боевые анимации, оружие в руке как
  отдельный слой (парсер/композитор их поддерживают, рендер состояний — нет).
- Точная формула `CanInteract` (P4c остаётся AABB) и shadow-порядок за
  пределами engine-формулы.
- Мультиплеер, Android-специфика.

## 4. Архитектура

Слои только вниз: `assets` (парсеры/CSX) ← `game` (композитор/HeroVisual) ←
`app` (SDL-рендер). `gl_assets` не зависит от SDL; `gl_game` тоже.

```
GameVfs ──► parse_wear_had ─┐
         ──► parse_iad       ├─► load_hero_visual ─► HeroVisual (RGBA листы)
         ──► parse_wear_seq  │        │
         ──► decode_csx ─────┘        └─► LevelScreen ─► Texture ─► SDL
```

Композит выполняется один раз при загрузке уровня (как в оригинале);
дальше рендер — выбор кадра/строки и blit.

## 5. Компоненты и API

### 5.1 `gl_assets`: `wear_had.hpp/.cpp`

```cpp
struct WearHadAnimation {
    std::uint32_t type_mask = 0;
    std::uint32_t rows = 0;            // 5 или 9 (см. mask)
    std::uint32_t columns = 0;
    std::uint32_t delay_ms = 0;
    std::uint32_t frame_width = 0, frame_height = 0;
    std::int32_t anchor_x = 0, anchor_y = 0;
    float movement_x = 0.0f, movement_y = 0.0f;
    std::int32_t layer_offset_x = 0, layer_offset_y = 0;
    std::int32_t shadow_offset_x = 0, shadow_offset_y = 0;
    std::uint32_t shadow_width = 0, shadow_height = 0;
    std::int32_t shadow_anchor_x = 0, shadow_anchor_y = 0;
};
Result<std::vector<WearHadAnimation>> parse_wear_had(std::span<const std::uint8_t> data);
```

Правила: magic `HAD `, записи с `0xC`, `next = 0x04 + size`; маска —
однобитная и из известного набора (0x1..0x80000); `columns/frame` > 0;
crop-таблицы (body и shadow) валидируются по размеру и пропускаются
(композитор использует IAD-crops). Строки: 9 для маски `0x10`, `0x20`,
`0x200`, иначе 5. Конец — offset `0x469C` из заголовка.

### 5.2 `gl_assets`: `iad.hpp` (расширение)

`IadBlock` получает корректные имена и поля:

```cpp
struct IadBlock {
    std::uint32_t type, size, frames, frame_width, frame_height;
    std::int32_t layer_offset_x, layer_offset_y;      // было image_width/height
    std::vector<IadCrop> crops;
    std::uint32_t shadow_frame_width, shadow_frame_height;
    std::int32_t shadow_layer_offset_x, shadow_layer_offset_y;
    std::vector<IadCrop> shadow_crops;
};
```

`parse_iad` дополнительно читает после crops: `F,G` (shadow frame), `H,I`
(shadow layer offset), `shadow_crops_bytes/crops`. Валидация: `%8 == 0`,
границы блока; `size` отсчитывается от поля `size` (`next = start + 4 + size`,
как в реальных файлах). Существующие тесты обновляются под новые имена;
`image_width/height` больше не используются.

### 5.3 `gl_assets`: `wear_seq.hpp/.cpp`

```cpp
struct WearSequence {
    std::uint32_t stride = 0;   // колонок (12)
    std::uint32_t rows = 0;
    std::vector<std::uint8_t> order; // rows*stride, [row*stride + col]
};
Result<WearSequence> parse_wear_seq(std::span<const std::uint8_t> data);
```

Валидация: `stride == 12`, `(size-1) % stride == 0`, значения `<= 4`.

### 5.4 `gl_game`: `hero_wear.hpp/.cpp`

```cpp
struct HeroOutfit {
    std::string wear_set = "noweapon_thrw";
    std::string weapon;   // слой weapon
    std::string shield;   // shield
    std::string elbarms;  // elbarms
    std::string helmet;   // helmet
    std::string armor;    // armor
};

struct HeroAnimation {
    gl::assets::WearHadAnimation header;
    std::vector<gl::assets::Rgba> body;    // columns*fw × rows*fh
    std::vector<gl::assets::Rgba> shadow;  // columns*sw × (2*rows-2)*sh
};

class HeroVisual {
public:
    const HeroAnimation* find(std::uint32_t type_mask) const;
    const HeroOutfit& outfit() const;
private:
    std::map<std::uint32_t, HeroAnimation> animations_;
    HeroOutfit outfit_;
};

Result<HeroVisual> load_hero_visual(const gl::assets::GameVfs& vfs,
                                    const HeroOutfit& outfit);
```

Загрузка для каждой HAD-записи:

1. `wear/<set>/animation/<anim>.csx` → base body (CSX→RGBA).
   Проверка: `width == columns*fw && height == rows*fh`.
2. `wear/<set>/shadows/<anim>.csx` → base shadow.
   Проверка: `columns*sw × (2*rows-2)*sh`.
3. Для каждого слоя в порядке `weapon, shield, elbarms, helmet, armor`
   (индексы 0..4 движка):
   - `wear/<set>/<cat>/<item>/<item>.iad`, блок по маске;
     `frames == columns`, `crops >= rows*columns`,
     `shadow_crops >= (2*rows-2)*columns`;
   - `wear/<set>/<cat>/<item>/animation/<anim>.csx`: `columns*b.fw × rows*b.fh`;
   - `wear/<set>/<cat>/<item>/shadows/<anim>.csx`:
     `columns*F × (2*rows-2)*G`;
   - `wear/<set>/seq/<anim>_<cat>.seq`: `stride == columns`, `rows == rows`.
4. Композит тела и тени (см. §6), сохранить листы.

Отсутствующий/нечитаемый слой: warning с путём и причиной, слой
пропускается (в оригинале отсутствие seq также удаляет слой), hero рисуется
без него. Отсутствующая базовая анимация: warning, анимация не добавляется.
Отсутствует `tb_stay` → `HeroVisual` считается невалидным (caller рисует
маркер + warning). Ошибки формата в присутствующих файлах — `format` error
из парсера (не глушить).

### 5.5 `gl_app`: `LevelScreen`

Удаляются `hero_base_`, `hero_go_base_`, `hero_armor_`, `hero_legs_`,
`hero_bow_base_`, `hero_helmet_` и `draw_hero_base` с константами.
Вместо них: `std::optional<gl::game::HeroVisual> hero_visual_` и текстуры
готовых листов (`std::map<uint32_t, HeroTextures>`), создаваемые в `load()`
из `HeroVisual`.

Рендер (pass body):
- `anim = hero.has_goal && path? tb_go : tb_stay`;
- `direction`: для `tb_stay` — `hero.direction` (8), для `tb_go` —
  `direction8_to16(hero.direction)`; row/mirror — `pad_person_row(type, dir)`;
- колонка = `(elapsed_ms / delay_ms) % columns` (delay из HAD; 0 → кадр 0);
- draw = `to_screen(camera, hero.x - anchor_x, hero.y - anchor_y, fw, fh)`;
- blit листа с `SDL_FLIP_HORIZONTAL` при mirror.
Рендер (pass shadow):
- row = `pad_shadow_row(type, dir).row` (identity, 8/16);
- draw = `hero.x - shadow_anchor_x`, `hero.y - shadow_anchor_y`, размер
  `shadow_width/height`;
- alpha 0.5, тем же проходом, что тени NPC.
Y-sort/mask используют прежний `hero_draw_key` (x,y); маркер-хитбокс P4c
не меняется.

Если `hero_visual_` нет — текущий маркер и warning (без тихой подмены).

### 5.6 CLI

`glview`:
- `--hero-wear` — рисовать hero из wear (вместо `--hero-base`);
- `--hero-set <name>` (default `noweapon_thrw`);
- `--hero-armor/--hero-elbarms/--hero-helmet/--hero-shield/--hero-weapon <dir>`
  (default slot0: `dospeh_almaz`, `ponogi_goluboy`, `shlem_goluboy`, пусто,
  пусто); `--hero-outfit none` — без слоёв.
Старые `--hero-base/--hero-armor` удалить или оставить алиасами
`--hero-wear` (решение: удалить, обновить тесты/доки).

`glgame`: default outfit slot0; флаги не добавляются (GUI-гейт).

## 6. Формулы композиции (engine-exact)

Тело, кадр `(row,col)`, слой `layer`:

```
b      = iad[type_mask]
crop   = b.crops[row*b.frames + col]
dest_x = col*fw + b.layer_offset_x + crop.x1 - aoff.x
dest_y = row*fh + b.layer_offset_y + crop.y1 - aoff.y
src_x  = col*b.frame_width  + crop.x1
src_y  = row*b.frame_height + crop.y1
w = crop.x2 - crop.x1; h = crop.y2 - crop.y1
```

где `fw/fh` = HAD frame, `aoff` = HAD layer_offset. Порядок: слои с `seq<=1`
(в порядке индексов 0..4), затем база (полный кадр `fw×fh` в
`(col*fw,row*fh)`), затем слои `seq=2,3,4`.

Тень, `shadow_row`:

```
crop2  = b.shadow_crops[shadow_row*b.frames + col]
dest_x = col*F + H + crop2.x1 - shoff.x
dest_y = shadow_row*G + I + crop2.y1 - shoff.y
src_x  = col*F + crop2.x1; src_y = shadow_row*G + crop2.y1
w = crop2.x2-crop2.x1; h = crop2.y2-crop2.y1
```

где `F/G/H/I` — IAD shadow frame/offset, `shoff` — HAD shadow_offset.
Порядок: слои 0..4, затем базовый shadow-кадр
`(col*shadow_w, shadow_row*shadow_h)`. Лист тени:
`columns*sw × (2*rows-2)*sh`.

Наложение: `src` копируется как RGBA с alpha-blend поверх dst
(порядок детерминирован, без сглаживания).

## 7. Тесты

### 7.1 Unit/synthetic (`tests/assets/test_wear_had.cpp`, `test_wear_seq.cpp`, `test_iad.cpp`)

- HAD: magic/версия; запись tb_stay с delay/cols/frame/anchor/layer_off/
  movement/shadow; rows 5/9; truncation, unknown mask, size за границей.
- IAD: расширенные поля, shadow crops, неверный `%8`, выход за блок.
- SEQ: stride/rows, значения >4, битый размер.

### 7.2 Real (`tests/assets/test_real_data.cpp`)

- `noweapon_thrw.had` — 11 записей; точные значения таблицы из research
  (`tb_stay` 88/12/48×92/24,77/158,142; `tb_go` 82/65×95/28,79/5.8333,5.0;
  `run` 65/68×100/31,82/10.4167,9.1667).
- `dospeh_almaz.iad` — 11 блоков; type4: 12, 36×37, off(165,154),
  F/G/H/I (40,20,152,205), crops 60, shadow_crops 96.
- `ponogi_goluboy.iad`, `shlem_goluboy.iad` — type4 off и crops.
- `tb_stay_armor.seq` — 12/5, все 2; `tb_go_weapon.seq` — 12/9, 2/4.
- Sweep: все 7 наборов (`bows`, `noweapon_thrw`, `onehand_wpn`,
  `two_axes_maces`, `two_spears_staffs`, `two_swords`, `xbows_guns`) — HAD
  парсится, количество записей >0, 0 ошибок; для каждого IAD/SEQ файлов
  набора — parse без ошибок (по списку VFS).

### 7.3 Compositor (`tests/game/test_hero_wear.cpp` — synthetic; `tests/assets/test_hero_wear_real.cpp` — real)

- slot0 outfit (`noweapon_thrw` + Almaz + Goluboy + Shlem_goluboy):
  `tb_stay` лист 576×460, `tb_go` 780×855 (12×65, 9×95);
  shadow `tb_stay` 660?256.
- Смещения (row0/row4): armor `(8,17)`/`(9,12)`, elbarms `(9,46)`/`(13,37)`,
  helmet `(18,3)`/`(18,1)` — проверяются по пикселям на границах crop
  (цвет слоя), не «на глаз».
- Golden SHA-256 композита `tb_stay` row0..4 col0 (RGBA) — фиксируется в
  тесте; изменение формулы ломает тест.
- Композит без слоёв == base CSX (попиксельно).
- Детерминизм: два прогона побайтово.

### 7.4 Screenshot (`tests/render/test_glview_screenshot.cpp`)

- `glview --hero-wear` на `l1_1`: PNG 1280×720, hero отличается от
  `--no-hero` > 500 px; повтор побайтово; `--hero-outfit none` отличается от
  slot0; кадр t=0 vs t=1000 ms различается (анимация).
- Камера/позиция — `--camera-x 1982 --camera-y 94`, как в P4d-исследовании.

### 7.5 Полный набор

`ctest --test-dir build --output-on-failure` с `GL_GAME_DIR` — все тесты,
0 failed.

## 8. Критерии приёмки

1. `cmake --build build` — успех.
2. Новые тесты зелёные; `gl_assets_tests`, `gl_assets_real_tests`,
   `gl_game_tests`, `glview_screenshot_tests`,
   `glgame_screenshot_tests` — 0 failed.
3. Полный `ctest` с `GL_GAME_DIR` — все passed.
4. Скриншот `glview --hero-wear` показывает hero из wear.pak с slot0-набором;
   координаты/размер соответствуют anchor; тень присутствует.
5. Manual desktop gate: пользователь сравнивает hero (спина/лицо, idle/walk)
   с оригиналом; без GUI-подтверждения P4d не принят.
6. Обновлены `STATUS.md`, `PROJECT.md`, `DECISIONS.md` (D-043), research.

## 9. Риски и открытые вопросы

- **Item→tech/`herowear.scr`** не извлечён: outfit задаётся явно; это
  ограничение документируется, не влияет на геометрию. → `STATUS` Open
  Questions, задача P5.
- **Shadow anchor/порядок** подтверждён кодом, но не пиксельным сравнением;
  при расхождении — сверка live и отдельный fix, без ослабления тестов.
- **Mirror/anchor при `SDL_FLIP_HORIZONTAL`**: anchor не меняется (как PAD);
  проверить на боковых направлениях.
- **Направление hero** (`hero.direction`, 8) для `tb_go` — оставляем текущую
  P4c-семантику `direction8_to16`; при расхождении с оригиналом — отдельная
  задача паритета.
- **Производительность**: 11 анимаций × до 5 слоёв; листы ~1–3 МБ RGBA;
  грузить только при `draw_hero`, освобождать с уровнем.

## 10. Файлы

Новые:
- `src/assets/include/gl/assets/wear_had.hpp`, `src/assets/src/wear_had.cpp`
- `src/assets/include/gl/assets/wear_seq.hpp`, `src/assets/src/wear_seq.cpp`
- `src/game/include/gl/game/hero_wear.hpp`, `src/game/src/hero_wear.cpp`
- `tests/assets/test_wear_had.cpp`, `tests/assets/test_wear_seq.cpp`
- `tests/game/test_hero_wear.cpp`

Изменяемые:
- `src/assets/include/gl/assets/iad.hpp`, `src/assets/src/iad.cpp`
- `CMakeLists.txt` (исходники), `src/game/CMakeLists.txt`, тестовые
  `CMakeLists.txt`
- `src/app/include/gl/app/level_screen.hpp`, `src/app/src/level_screen.cpp`
- `tools/glview` (флаги), `tools/glgame` (default outfit)
- `tests/assets/test_iad.cpp`, `test_real_data.cpp`,
  `tests/render/test_glview_screenshot.cpp`,
  `tests/app/test_glgame_screenshot.cpp` (если затрагивает hero)
- `docs/STATUS.md`, `docs/PROJECT.md`, `docs/DECISIONS.md`

## 11. Definition of Done

- Код + тесты + реальные выводы команд (build/ctest/скриншоты) в `STATUS`.
- Research обновлён фактами и путями; spec/plan соответствуют реализации.
- Manual gate выполнен пользователем; иначе P4d остаётся «automated passed,
  manual OPEN» и не принимается.
- Никаких «TBD/TODO» в коде/спеке; остатки — в `STATUS` → Open Questions.

