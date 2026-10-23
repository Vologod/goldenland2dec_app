# Аудит паритета P0 (assets/formats) и P1 (level renderer)

Дата: 2026-10-23.
Режим: **read-only**. Код, тесты и данные игры не изменялись; коммитов нет.
Границы: P0 (PAK/VFS/CSX/SDB/ITM/BMP/LVL/LAO/SEF/PAD/CSX-frames) и P1
(загрузчик уровня, фон, статика, анимации, камера, скриншот). P2–P4
упоминаются только там, где они дают или не дают evidence для P0/P1.

## 0. Метод и критерии статуса

Проверены:

- `docs/STATUS.md`, `docs/PROJECT.md`, `docs/DECISIONS.md`, все `docs/research/*`;
- спеки/планы P0/P1 и фактические исходники `src/assets`, `src/game`
  (`level_loader`, `walk_grid`), `src/render`, `src/app/level_screen`,
  `src/platform`, `tools/glview`;
- тесты `tests/assets`, `tests/game/test_real_*`, `tests/render`, `tests/tools`.

Дополнительно (read-only) прогнано на целевой установке:

- `build\bin\glpak.exe info "I:\Games\Златогорье-2"` — 16 архивов;
- `glpak list --archive levels.pak` — 175 `bitmaps/layer.jpg`, 84 `anim_*.csx`;
- извлечены все 175 `levels/lvl/*.lvl` во временный каталог вне репозитория и
  разобраны `BLK_ADSC`/`BLK_SDSC` (PowerShell, little-endian, cp1251);
- извлечены `*.lao` для `l3_2`, `l4_1`, `l4_2`, `l89_1_a`.

Статусы:

- **verified** — поведение/формат подтверждены источником из оригинала
  (`Server.dll`/`Client.dll`/`GoldenLand.exe`: RVA/offset/чтение live-памяти) **или**
  полным прогоном реальных данных, причём тест пинит именно это утверждение
  (точные counts/размеры/0 расхождений), а не «файл разобрался».
- **partial** — реализация работает на реальных данных, но семантика полей/правил
  взята из вторичных источников (GoldenLandEditor, XnView plugin, README
  сообщества), либо прогон покрывает не все данные, либо тест не пинит
  заявленный в документах count.
- **unverified** — evidence из оригинала нет; это выбор порта, предположение
  или «правдоподобная» реконструкция.

Важно: `docs/research/formats.md` в основном опирается на вторичные источники
(`fersatgit/Goldenland-2-Cold-Heaven`, `DarkContact/GoldenLandEditor`,
`AlexKimov/goldenland-file-formats`), а не на декомпиляцию оригинальных
бинарников. Поэтому «разбирается на всех реальных файлах» само по себе даёт
**partial**, а не **verified** для семантики формата.

## 1. Главная находка аудита (P1)

Загрузчик анимаций **игнорирует `BLK_ADSC.number`/`name`** и связывает
описание с файлом и LAO по **позиции после сортировки по `number`**:

`src/game/src/level_loader.cpp:164-181` — сортировка `lvl->animations` по
`number`, затем `anim_<i>.csx` и `lao[i]`, где `i` — индекс в отсортированном
массиве; `:200-206` — `number/x/y` берутся из `descriptions[i]`.

На реальных данных это даёт расхождения на четырёх уровнях. Проверено
извлечением LVL (2026-10-23):

| Level | `BLK_ADSC` (number, name, x, y) | Файлы | LAO | Что делает порт |
|---|---|---|---|---|
| `l89_1_a` | `1 L89_1_A_anim_1 (688,1106)`, `2 …anim_2 (982,2148)`, `4 …anim_4 (2200,1866)`, `0 …anim_0 (1388,2395)` | `anim_0..anim_4` (5) | 4 | сортирует `[0,1,2,4]`, грузит `anim_0, anim_1, anim_2, anim_3`; `anim_4` не рисуется, для `number=4` взят `anim_3` |
| `l4_1` | два `number=0`: `L4_1_ANIM_0 (57,305)`, `L4_1_ANIM_0_1 (1932,691)` | только `anim_0` | 1 | второй объект пытается загрузить `anim_1.csx` → warning + пропуск; второй спрайт не рисуется |
| `l4_2` | два `number=0` | только `anim_0` | 1 | то же: второй объект пропускается |
| `l3_2` | `1 (432,783)`, `0 (2087,230)`, `0 (579,-272)` | `anim_0`, `anim_1` | 2 | сортирует `[0,0,1]`, грузит `anim_0, anim_1` для двух `number=0`, описание `number=1` отбрасывается (count=min(3,2)) |

Поле `name` (`L89_1_A_anim_4`, `L4_1_ANIM_0_1`) прямо указывает суффикс
файла; текущая схема ему противоречит. Это **кандидат на блокировку
паритета**: на этих локациях анимации будут отсутствовать или стоять не на
своих местах. Нужна сверка с загрузчиком оригинала
(`GoldenLand.exe`/`Server.dll`, чтение `BLK_ADSC`) и фикс: файл — по
`number`/`name`, LAO — по своему индексу.

## 2. P0 — ассеты и форматы

| # | Поведение/формат | Реализация | Evidence в docs/research | Статус | Пробел |
|---|---|---|---|---|---|
| P0-1 | PAK: magic `PAK \0\0\0\0`, count, FAT (name_len/name/size/offset) | `src/assets/src/pak.cpp:124-156`, `:177-217`; `pak.hpp:13-19` | `formats.md:12-39`; counts `00-game-build.md:25-42` | **partial** | Формат из вторичных источников; нет SHA-256 ни одного `.pak` (кроме `scripts.pak` в `p3a-cs-format.md:53-58`). Точные суммы распаковки 4 архивов из STATUS не пинятся тестом. |
| P0-2 | PAK payload: `mode==0` raw, иначе zlib (`stored` + поток) | `pak.cpp:73-89`, `:219-279` | `formats.md:25-39`; D-013 | **partial** | На реальных данных наблюдались только mode 0/1; ветка «любое `mode != 0` — zlib» (`pak.cpp:75-88`, `:230-243`) для других значений не подтверждена. Семантика `mode` из оригинала не извлечена. |
| P0-3 | PAK: запрет дубликатов/перекрытий, лимиты count/name_len/512 МиБ | `pak.cpp:53-108`, `:134`, `:140-151` | Только unit-тесты `tests/assets/test_pak.cpp:224-263`; в research нет | **unverified** | Это защитные предположения порта, не поведение оригинала. Ошибки формата/границы, которые оригинал мог принимать, здесь отвергаются. |
| P0-4 | VFS: loose → update.N (по убыванию) → base; root/Data | `src/assets/src/vfs.cpp:97-144`, `:151-175` | `specs/2026-10-19-goldenland2-android-port-design.md:148-151` (дизайн), research нет | **unverified** | Приоритет update-архивов и loose-файлов не подтверждён ни `GoldenLand.exe`, ни `Client/Server.dll`. Глобальный overlay всех `*.update.N.pak` (не только одноимённого base) — допущение. |
| P0-5 | Нормализация виртуальных путей (casefold, `\`→`/`) | `src/assets/src/cp1251.cpp:145-196`; D-011 | Нет в research; только D-011 и unit-тесты | **unverified** | Эвристика «валидный UTF-8 vs cp1251» и диапазон casefold — решение порта; утверждение «имена в `.pak` ASCII» не проверено на всех архивах. |
| P0-6 | cp1251 → UTF-8 | `cp1251.cpp:6-23`, `:77-82`; `test_cp1251.cpp` | `formats.md:7-10` | **verified** (стандартная таблица) | Байт `0x98` отображён в `U+0098` — редкий/неопределённый cp1251; для игровых строк не критично. |
| P0-7 | CSX: заголовок, палитра, `transparent_bgr`, индекс прозрачности | `src/assets/src/csx.cpp:11-42` | `formats.md:41-58`; правила — `XnView plugin/Xcsx.dpr` (вторичный) | **partial** | «Индекс = наибольший элемент палитры, равный `transparent_bgr`, иначе младший байт» — из плагина, не из оригинала. Прогон 19 704 непустых `.csx` описан, но тест пинит только `decoded > 0` (`test_real_data.cpp:124-126`). |
| P0-8 | CSX RLE: `0x69/0x6A/0x6B/0x6C`, literal, точная ширина строки | `csx.cpp:44-79` | `formats.md:54` («точный RLE — в `Xcsx.dpr`; проверить»), `plans/2026-10-19-p0-assets-formats.md` (Task 5) | **partial** | Пиксельная семантика не сверена с оригинальным декодером. `csx.cpp:34` допускает байты после `offsets[height]` — поведение оригинала неизвестно; `offsets[0] != 0` не проверяется. |
| P0-9 | CSX лимиты: 64 Ми пикселей, dimension 1 000 000, palette ≤256 | `csx.cpp:7-8`, `:17-18`, `:25-29`; D-015 | `formats.md:56-58`; D-015 | **partial** | Лимит 64 Ми выбран чуть выше наблюдённого максимума 58 380 000 (`1400×41700`); для аддонов/других сборок не проверен. |
| P0-10 | SDB: опциональная подпись `SDB `, XOR `0xAA` без подписи | `src/assets/src/sdb.cpp:7-26`; `sdb.hpp:11-18` | `formats.md:60-70`; `p3c-dialogs-sdb-font.md:9-10`; `test_real_data.cpp:100-103` | **partial** | XOR-ветка не доказана на реальных `.sdb` (неизвестно, есть ли файлы без подписи); дубликаты id и битые строки не описаны. Реальный прогон 315 файлов не пинит count. |
| P0-11 | ITM: magic 4, поля, условные блоки по типу | `src/assets/src/itm.cpp:17-89`; `itm.hpp:31-58` | `formats.md:72-89` (README сообщества); D-012/D-014 (корпус 1848) | **partial** | Таблица блоков (damage 0–7/11–14/28, spell 6/21/22, charge 6, healing 19/20, recipe 26, damage_type 8–10, effects кроме 21) выведена из корпуса и README, не из декомпиляции. `unknown` qword/резервные поля не валидируются. |
| P0-12 | ITM: неизвестный `ItemType` проходит без блоков; хвост — ошибка | `itm.cpp:26-27`, `:75-87`; D-012 | D-012 (решение), не research | **unverified** | Поведение для типов вне 0–28 и для padding в реальных файлах не подтверждено оригиналом; D-012 сам говорит «пересмотреть». |
| P0-13 | BMP/PNG/JPG → RGBA (stb_image), PNG export | `src/assets/src/image.cpp:13-48`; `test_image.cpp` | `specs/...goldenland2...:159` (stb), `00-game-build.md` (BMP в items) | **partial** | 1938 `.bmp` из `items.pak` **не входят** в real-data sweep (`test_real_data.cpp:67-117` только `.csx/.sdb/.itm`). Декодер stb, а не оригинальный GDI/DirectDraw; палитровые/RLE BMP не проверены. |
| P0-14 | LVL: 12 блоков строго по порядку, точный payload | `src/assets/src/lvl.cpp:196-309`; `test_lvl.cpp`; `test_real_levels.cpp:60-98` | `formats.md:91-110`; тест пинит `175/175` | **verified** (структура/парсинг) | `BLK_LVER` не проверяется на 3.1 (`lvl.cpp:207-214`); `BLK_WTHR`/`SENV`/`DOOR` только валидируются, поля не моделируются. Семантика полей из редактора, не из бинарников. |
| P0-15 | LVL: тайлы/чанки и правило проходимости | `src/game/src/walk_grid.cpp:10-32`; `lvl.cpp:236-246` | `p4b2-walkability-pathfinding.md:8-24`, `:73-96` — live-память `GoldenLand.exe`, `world+0x330`, `FUN_00426234`, 0/51520 | **verified** (правило проходимости) | Сам LVL-парсер хранит чанки в файловом порядке; column-major применяется только в `build_walk_grid`. Геометрия «чанк 24×18, тайл 12×9» — из редактора (`formats.md:99-100`), для рендера P1 не проверена. |
| P0-16 | LAO: пары `(u32 height, u32 delay)`, размер кратен 8, пустой допустим | `src/assets/src/lao.cpp:11-31`; `test_lao.cpp`; `test_real_levels.cpp:100-138` | `formats.md:112-118`; тест пинит `33/33`, 33 pack | **verified** (парсинг) | `delay` объявлен миллисекундами, но единица/поведение `delay==0` и первого кадра не извлечены из `Client.dll`; это влияет на P1. |
| P0-17 | SEF: `pack`, persons, cell_groups, top-level key/value | `src/assets/src/sef.cpp:391-483` | `formats.md:119-145`; `p4b-person-movement.md:21-33` (`FUN_1401edc4`, offsets полей Person); тест пинит `261/261` и 16 persons `l1_1` | **partial** | Синтаксис — самописный лексер; экранирования в строках нет; неизвестные top-level блоки молча пропускаются; `weather` только int; `pack` обязателен. Поля Person частично подтверждены `Server.dll`, синтаксис — нет. |
| P0-18 | PAD: заголовок, маски, crops, shadow, trailer | `src/assets/src/pad.cpp:196-259`; `test_real_persons.cpp:46-87` | `formats.md:146-198` (GoldenLandEditor + корпус) | **partial** | Формат из вторичного парсера; спец-хвост `l0.p1081_interwizards07` и trailer-таблицы выведены из корпуса. Тест пинит `276/276`, но **не** 2450 анимаций (только `animations > 0`, `test_real_persons.cpp:86`). |
| P0-19 | CSX → вертикальные кадры по высоте LAO | `src/assets/src/csx_frames.cpp:12-44`; `test_csx_frames.cpp` | `p1-level-rendering.md:30-34`; `formats.md:112-118` | **partial** | «Кадры — вертикальные полосы фиксированной высоты» взято из редактора/LAO; на всех 84 `anim_*.csx` не проверялось. Последний короткий кадр — допущение порта. |
| P0-20 | CS/person_script/save (соседние assets, вне P0/P1) | `cs.cpp`, `person_script.cpp`, `save.cpp` | `p3a-cs-format.md`, `formats.md:126-145` | **partial** | Приведено для полноты; их evidence относится к P3a/P4a и в этот аудит не входит. |

## 3. P1 — загрузчик уровня и рендер

| # | Поведение/формат | Реализация | Evidence | Статус | Пробел |
|---|---|---|---|---|---|
| P1-1 | SEF → pack → LVL → `bitmaps/layer.jpg` | `src/game/src/level_loader.cpp:33-55`, `:74-113` | `p1-level-rendering.md:8-19`; реально 175 `layer.jpg` (audit run 2026-10-23), но тест только `l1_1` | **partial** | Путь `layer.jpg` подтверждён для всех 175 pack-каталогов только ручной проверкой, не тестом. Несовпадение фона и `BLK_MPSZ` — только warning (`:107-113`), поведение оригинала неизвестно. |
| P1-2 | Статика: `static_<number>.csx`, сортировка по `number`, левый верхний угол | `level_loader.cpp:115-142`; `level_screen.cpp:332-336` | `p1-level-rendering.md:21-27`; `specs/2026-10-19-p1-level-viewer-design.md:67-72` — `GoldenLandEditor::LevelViewer` | **partial** | Anchor «левый верхний угол» и порядок по `number` — из редактора, не из `Client.dll`. Маски/глубина/Y-сортировка не учитываются (в P1 scope исключены, но для паритета это пробел). Дубликаты `number` (например, `l32` — `0,1,2,0,5`) грузят один файл дважды; не проверено. |
| P1-3 | Анимации: `BLK_ADSC` ↔ `anim_<i>.csx` ↔ `lao[i]` | `level_loader.cpp:144-208` (особенно `:164-181`, `:200-206`) | `p1-level-rendering.md:29-37` — редактор; тест только `l1_1` | **unverified + дефект** | Используется индекс после сортировки, а не `number`/`name`; на `l89_1_a`, `l4_1`, `l4_2`, `l3_2` это даёт пропуск/подмену анимаций (см. §1). Нужен фикс и all-level тест. |
| P1-4 | Тайминг анимации: `(elapsed_ms / delay) % frames`, `delay==0 → 0` | `src/render/src/animation.cpp:5-9`; `level_screen.cpp:338-345` | `p1-level-rendering.md:33-37` — `Animation::update` редактора | **unverified** | Формула и единица `delay` не извлечены из `Client.dll`; `delay==0`/отрицательный `delay` — защита порта. Скриншот использует `(frame+1)*16 ms` (`tools/glview/main.cpp:231-234`) — тестовое решение, не оригинал. |
| P1-5 | Порядок кадра: фон → статика → анимации | `level_screen.cpp:325-345` | `p1-level-rendering.md:46-51`; `specs/...p1...:104` — порядок задан спекой | **unverified vs оригинал** | Оригинальный клиент использует маски/глубину и, вероятно, Y/объектный порядок; в P1 это сознательно исключено. Для визуального паритета всей кампании порядок нужно извлекать из `Client.dll`. |
| P1-6 | Размер тайла 12×9, чанк 24×18; позиционирование | `level_screen.cpp:25-26`, `:461-468`; `person_render` (не P1) | `p4b-person-movement.md:35-37` — live/`Server.dll` для `tile*12+6, tile*9+4`; `formats.md:99-100` — чанк из редактора | **partial** | 12×9 подтверждено только для позиционирования Person, не для рендера статики/фона. Anchor'ы статики/анимаций — см. P1-2/P1-3. |
| P1-7 | Камера: clamp к миру, центрирование малой карты, pan `600/zoom`, zoom `0.25…4.0` к курсору | `src/render/src/camera.cpp:8-66`; `level_screen.cpp:472-499`; `tools/glview/main.cpp:303-334` | Нет в research; только `p1-level-rendering.md:49-53` (описание порта) | **unverified vs оригинал** | Это viewer-поведение, а не реконструкция камеры игры (в оригинале камера следует за героем). Для P1 допустимо, для паритета P10 — нет. |
| P1-8 | Nearest-neighbor, RGBA32, `SDL_BLENDMODE_BLEND` для всех текстур | `src/platform/src/sdl_context.cpp:24`; `src/render/src/texture.cpp:57-72` | `specs/...p1...:91-97` (nearest), P1 fix-round (blend) | **partial** | Nearest соответствует заявлению спеки, но не сверен с DirectDraw-фильтрацией оригинала. CSX не имеет полупрозрачности (alpha только 0/255), поэтому BLEND ≈ colorkey; для масок/эффектов это может измениться. |
| P1-9 | Ошибки загрузчика: SEF/LVL/фон фатальны, статика/анимации — warning+skip | `level_loader.cpp:74-208`; `specs/...p1...:126-132` | Спека P1; тест только «нет уровня → format» (`test_level_loader.cpp:98-107`) | **partial** | Не покрыты тестами: отсутствующий LAO при наличии ADSC, несовпадение count ADSC/LAO, битый CSX статики, несовпадение фона и MPSZ. Семантика «оригинал продолжает/падает» не извлечена. |
| P1-10 | Реальный прогон загрузчика | `tests/assets/test_level_loader.cpp` | Только `l1_1` и `l6_2_2`; STATUS заявляет приёмку P1 на `l1_1` | **partial** | Нет sweep «загрузить все 175 уровней и проверить statics/animations»; из-за этого дефект §1 не пойман. `glpak list` показывает 175 фонов и 84 anim-файла, но загрузчик на них не прогонялся. |
| P1-11 | Скриншот/детерминизм | `tools/glview/main.cpp:212-254`; `tests/render/test_glview_screenshot.cpp` | P1 acceptance (`STATUS.md`), тесты пинят размер и `diff > 1000` px | **partial** | Тесты не проверяют заявленные в STATUS SHA-256/точные количества; детерминизм — свойство harness, не паритет с оригиналом. |
| P1-12 | Маски/триггеры уровня | `lvl.cpp` парсит `BLK_MDSC`/`BLK_TDSC`; `level_loader.cpp` не грузит `bitmaps/masks/mask_*.csx` / `bitmaps/triggers/trigger_*.csx` | `specs/...p1...:49-50` говорит «данные парсятся» (в P1 не рисуются) | **unverified** | Изображения масок/триггеров не загружаются вообще; для P1 это заявлено как «не рисуются», но спека §3 говорит, что данные парсятся. Для P4+ понадобится. |

## 4. Сквозные проблемы evidence

1. **Хэши не соответствуют AGENTS.** `00-game-build.md:7-12` даёт **MD5**
   (`GoldenLand.exe 6EE2…`, `Client.dll D6E5…`, `Server.dll E3E0A262…`).
   `p4b-person-movement.md:3-5` и `p4b2-walkability-pathfinding.md:3-4`
   называют ту же 32-символьную строку `E3E0A262…` **SHA-256** — это MD5.
   Для P1 вообще нет SHA-256 `Client.dll`, хотя весь рендер должен сверяться
   с ним.
2. **Нет fingerprint'ов архивов.** `formats.md` не содержит SHA-256 ни одного
   `.pak` (только размеры/counts в `00-game-build.md`). Исключение —
   `scripts.pak` в `p3a-cs-format.md:53-58`. Утверждения «проверено на всех
   `.pak`» нельзя воспроизвести по документу.
3. **Заявленные counts не пинятся тестами.** `STATUS.md`/`formats.md`:
   `.csx` 19 704, `.sdb` 315, `.itm` 1848, PAD-анимации 2450, «4 архива
   полностью распакованы и суммы совпали». `test_real_data.cpp:119-126`
   проверяет только `failures == 0` и `decoded > 0`;
   `test_real_persons.cpp:86` — только `animations > 0`; CLI-тест распаковки
   (`tests/tools/test_glpak_extract.cpp`) использует синтетический архив.
4. **P0 не сверялся с оригинальными бинарниками.** Для CSX/SDB/ITM/PAK/VFS
   evidence — вторичные проекты сообщества + успешный парсинг корпуса. По
   D-029 это `partial/unverified`, а не `verified`. Исключения: правило
   проходимости LVL (live-память) и позиционные формулы Person (`Server.dll`),
   но они относятся к P4b/P4b2, а не к P0-декодерам.
5. **P1 не сверялся с `Client.dll`.** Anchor'ы, порядок, тайминг и камера
   подтверждены только `GoldenLandEditor` (`LevelViewer::drawAnimations`,
   `Animation::update`) или являются выбором порта. Для визуального паритета
   нужен отдельный reverse `Client.dll` (спрайтовый батчинг, маски, глубина,
   тик анимации).

## 5. Приоритетные пробелы (сводка)

1. **blocks parity** — `BLK_ADSC.number/name` игнорируется; анимации
   пропускаются/подменяются на `l3_2`, `l4_1`, `l4_2`, `l89_1_a`
   (`src/game/src/level_loader.cpp:164-181`).
2. **blocks parity** — порядок отрисовки и anchor'ы статики/анимаций
   (левый верхний угол, по `number`, без масок/глубины) не подтверждены
   `Client.dll` (`level_loader.cpp:115-142`, `level_screen.cpp:332-345`).
3. **blocks parity** — тайминг анимаций (`delay` как ms, `delay==0 → frame 0`,
   сброс на t=0) не извлечён из оригинала (`src/render/src/animation.cpp:5-9`).
4. **blocks parity** — VFS update/loose precedence не подтверждён оригиналом;
   глобальный overlay `*.update.N.pak` может подменять файлы не из «своего»
   архива (`src/assets/src/vfs.cpp:97-144`).
5. **minor** — CSX RLE/transparency/trailing-bytes семантика только из
   `Xcsx.dpr`; пиксельного сравнения с оригиналом нет
   (`src/assets/src/csx.cpp:34`, `:44-79`).
6. **minor** — ITM условные блоки выведены из корпуса/README, не из
   декомпиляции; неизвестные типы и padding не подтверждены
   (`src/assets/src/itm.cpp:39-86`).
7. **minor** — заявленные counts (CSX/SDB/ITM/PAD/распаковка) не пинятся
   тестами (`tests/assets/test_real_data.cpp:119-126`,
   `tests/assets/test_real_persons.cpp:86`).
8. **minor** — SDB XOR-ветка не доказана на реальных файлах; дубликаты id
   не описаны (`src/assets/src/sdb.cpp:10-25`).
9. **minor** — SEF-лексер самописный, без escape'ов; неизвестные блоки молча
   пропускаются; только `l1_1` поля проверены точно
   (`src/assets/src/sef.cpp:391-483`).
10. **minor** — все 175 уровней не прогоняются через `load_level`; тест
    только `l1_1`/`l6_2_2` (`tests/assets/test_level_loader.cpp`).
11. **minor** — `BLK_MPSZ`/фон: несовпадение только warning; `BLK_LVER`
    не проверяется на 3.1 (`src/game/src/level_loader.cpp:107-113`,
    `src/assets/src/lvl.cpp:207-214`).
12. **minor** — 1938 BMP не входят в real-data sweep; декодер stb не сверен
    с оригиналом (`src/assets/src/image.cpp:13-48`).
13. **minor** — PAK-лимиты/перекрытия/`mode != 0` — защитные предположения
    порта (`src/assets/src/pak.cpp:53-108`, `:73-89`).
14. **minor** — PAD trailer/2450 анимаций и LAO delay-семантика не пинятся
    (`src/assets/src/pad.cpp:162-193`, `src/assets/src/lao.cpp:19-26`).
15. **minor** — evidence integrity: MD5 выдан за SHA-256, нет хэшей `.pak`
    и `Client.dll` (`docs/research/00-game-build.md:7-12`,
    `docs/research/p4b-person-movement.md:3-5`).

## 6. Что не проверялось

- Ghidra/IDA-сессия по `Client.dll` для спрайтового рендера, масок, глубины,
  порядка и тика анимации — не выполнялась.
- Полный `load_level` по всем 175 уровням — не выполнялся (только разбор
  `BLK_ADSC`/`BLK_SDSC` и наличие файлов).
- Пиксельное сравнение кадров порта с оригинальной игрой — не выполнялось.
- Android/touch (P1b) — вне границ.
