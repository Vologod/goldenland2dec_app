# P4d: hero wear (HAD/IAD/SEQ) и композитор

Дата: 2026-10-25. Источники: `GoldenLand.exe` SHA-256
`7854019E6C0A6C2BFDE6425C5799AFCA654D174A31CDAA7A624C3E17842109FF`,
`Client.dll` SHA-256
`FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`
(обе v1.3, `I:\Games\Златогорье-2`); данные `wear.pak`
(`noweapon_thrw.had` SHA-256 `DAB3D056F808E83F08E5792F6A8AC2D785AB2DD335C6CE2386D66384C826B772`,
`armor/dospeh_almaz/dospeh_almaz.iad` SHA-256
`07F71B6974587A19068C98074C626D8F29E9667940C8470071DD6E0DB699CF88`).
Инструменты: Ghidra 12.1.4 headless (дизассемблер/декомпилятор), разбор
бинарных данных на Python. Все RVA — от базы образа `0x400000` (`GoldenLand.exe`)
и `0x12000000` (`Client.dll`).

## Краткий итог

Точная схема hero-рендера извлечена и **больше не BLOCKED**:

1. Hero рисуется не из `persons.pak`, а из `wear.pak`.
2. `wear/<set>/<set>.had` — метаданные всех анимаций набора (кадр, колонки,
   якорь, скорость, смещения слоёв, тени).
3. `wear/<set>/<category>/<item>/<item>.iad` — геометрия CSX-слоя по анимациям
   (кадр, шаг, смещение слоя, crop-таблица и shadow-crop-таблица).
4. `wear/<set>/seq/<anim>_<layer>.seq` — порядок отрисовки слоя на каждый кадр.
5. `FUN_0042279C` собирает из базового CSX и 5 слоёв готовый лист кадров
   `columns × frame_w` на `rows × frame_h`, где слой ставится в кадр по
   формуле ниже. `Client.dll` рисует готовый PersonVisual как PAD:
   `draw = position - anchor`.
6. Анкор кадра — поля HAD `anchor_x/anchor_y` (они же PersonVisual block
   `+0x10/+0x12`), а не размер кадра.

## Функции (RVA)

`GoldenLand.exe`:

- `0x42279C` (`FUN_0042279c`) — композитор тела и тени: 5 слоёв + база,
  seq-порядок, crop-таблицы IAD.
- `0x423334` (`FUN_00423334`) — парсер IAD: type-блоки, две таблицы
  (crops и shadow crops).
- `0x4239C0` (`FUN_004239c0`) — загрузчик wear-набора: HAD, 5 слоёв IAD+CSX,
  seq, 12 анимаций, затем `FUN_0042279C` на каждую.
- `0x42C200` (`FUN_0042c200`) — обёртка: `FUN_004239c0(DAT_00469d58,
  this+0x50, person_visual)`.
- `0x42A87C` (`FUN_0042a87c`) — создание PersonVisual: PAD-ветка
  (`persons/<name>/<name>.pad` + `FUN_0042a5c0`) или wear-ветка
  (`FUN_0042c200`).
- `0x42A5C0` (`FUN_0042a5c0`) — PAD-ветка; подтверждает layout
  PersonVisual-блока (та же семантика, что `PadAnimation`).
- `0x469D58` — глобальный экземпляр wear-менеджера; `0x42CA7C`/`0x42D7DC`
  пишут указатель.
- Строки: `wear\%s\%s.had`, `wear\%s\%s\%s\%s.iad`,
  `wear\%s\animation\%s`, `wear\%s\shadows\%s`,
  `wear\%s\%s\%s\animation\%s`, `wear\%s\%s\%s\shadows\%s`,
  `wear\%s\seq\%s_%s.seq`, `scripts\persons_res\%s.prs`
  (`0x484528..0x4845E0`).

`Client.dll`:

- `0x1202C424` — рисование PersonRenderItem: тело из `+0x48`, тень из
  `+0x4C`, позиция `camera + item+0x2C/0x30` (тень `+0x34/0x38`),
  src/dst rect `item+0x0C`, alpha 1.0 (body) / 0.5 (shadow).
- `0x120E094`/`0x1202E148` — проходы теней (pass 2) и тел (pass 7);
  `0x1202C8AC` — подготовка списка и вызов отрисовки.
- `0x12038CC4` — выбор активного PersonVisual из двух (entity `+0x18C`/
  `+0x198`), копирование 0x108 байт и установка указателя в entity `+0x3C8`.

## HAD (`wear/<set>/<set>.had`)

Заголовок: `"HAD "` (4), `u32 @4` = 0x469C (смещение служебного списка;
не используется для анимаций), `u32 @8` = маска присутствующих анимаций
(`noweapon_thrw` = `0xD07B5`, popcount 11), дальше записи с `0xC`.

Запись:

```
+0x00 u32 type_mask       (1,2,4,...,0x80000; см. таблицу анимаций)
+0x04 u32 size            (payload после этого поля; next = +0x04 + size)
+0x08 u32 delay           (HAD: 88 у noweapon; HAD-анимации)
+0x0C u32 columns         (12)
+0x10 u32 layer_off_x     (смещение композиции, НЕ анкор)
+0x14 u32 layer_off_y
+0x18 u32 frame_width
+0x1C u32 frame_height
+0x20 u32 anchor_x        -> PersonVisual block +0x10
+0x24 u32 anchor_y        -> PersonVisual block +0x12
+0x28 f32 movement_x      (_go/_run; 5.833333/10.416667)
+0x2C f32 movement_y      (5.0/9.166667)
+0x30 u32 crops_table_bytes (480 = 60×8)
+0x34 ... crops           (60×8: u16 x,y,x2,y2; тело/кадр base)
...      shadow record:   u32 off_x, u32 off_y, u32 shadow_w, u32 shadow_h,
                          u32 shadow_anchor_x, u32 shadow_anchor_y,
                          u32 shadow_crops_bytes,
                          shadow crops (u16×4, 8 байт × N)
```

`size` включает всё после `type` (включая поле `size`); следующая запись =
`+0x04 + size`. Shadow record начинается сразу после crops:
`shadow = +0x34 + crops_table_bytes` (не фиксированный `+0x214`; для
`tb_go`/`run` таблица больше). Проверено: `rt_stay` next `0x53C`,
`tb_stay` `0xA6C`, ..., конец `0x469C`.

Анимации (маска → имя; порядок массива имён `GoldenLand.exe` RVA `0x445220`):

```
0x00001 rt_stay   0x00002 rt_fun    0x00004 tb_stay   0x00008 tb_fun
0x00010 tb_go     0x00020 rt_go     0x00040 (cast; имя NULL)
0x00080 suffer    0x00100 die       0x00200 run (idx 15)
0x00400 ss_attack 0x10000 hits0     0x20000 hits1
0x40000 hits2     0x80000 hits3
```

`noweapon_thrw` содержит 11 записей; `hits1`/`rt_fun`/`tb_fun`/`cast`
отсутствуют. Колонок всегда 12; рядов 5, кроме `tb_go`/`rt_go`/`run`
(9 рядов; в композиторе `rows = 9` для `type ∈ {4,5,15}`).

`noweapon_thrw.had` (извлечено):

| anim | mask | cols | fw×fh | anchor | movement | layer_off |
|---|---|---|---|---|---|---|
| rt_stay | 1 | 12 | 65×92 | (34,78) | 0,0 | (148,141) |
| tb_stay | 4 | 12 | 48×92 | (24,77) | 0,0 | (158,142) |
| tb_go | 0x10 | 12 | 65×95 | (28,79) | 5.8333,5.0 | (154,140) |
| rt_go | 0x20 | 12 | 75×96 | (34,79) | 5.8333,5.0 | (148,140) |
| suffer | 0x80 | 12 | 53×97 | (24,82) | 0,0 | (158,137) |
| die | 0x100 | 12 | 128×126 | (87,77) | 0,0 | (95,142) |
| ss_attack | 0x400 | 12 | 66×94 | (35,79) | 0,0 | (147,140) |
| hits0 | 0x10000 | 12 | 96×98 | (57,83) | 0,0 | (125,136) |
| hits2 | 0x40000 | 12 | 134×118 | (67,103) | 0,0 | (115,116) |
| hits3 | 0x80000 | 12 | 136×118 | (68,103) | 0,0 | (114,116) |
| run | 0x200 | 12 | 68×100 | (31,82) | 10.4167,9.1667 | (151,137) |

(`layer_off` — поля HAD `+0x10/+0x14`.)

## IAD (`.../<item>.iad`)

Заголовок: `"IAD "` (4), `u32 version = 1` (4). Первый type-блок начинается
с `0x4`; значение `version` одновременно является `type = 1`.

Type-блок (little-endian):

```
+0x00 u32 type_mask
+0x04 u32 size        (next = +0x04 + size)
+0x08 u32 frames      (колонок; 12)
+0x0C u32 frame_w     (шаг по X в CSX слоя)
+0x10 u32 frame_h     (шаг по Y)
+0x14 u32 layer_off_x (используется как смещение слоя в композите)
+0x18 u32 layer_off_y
+0x1C u32 crops_bytes (÷8 = число crop-записей)
+0x20 ... crops       (u16 x1,y1,x2,y2; 8 байт; bounds источника)
...      u32 F         (shadow frame step_x)
...      u32 G         (shadow frame step_y)
...      u32 H         (shadow layer_off_x)
...      u32 I         (shadow layer_off_y)
...      u32 shadow_crops_bytes (÷8)
...      shadow crops  (u16 x1,y1,x2,y2; 8 байт)
```

В памяти движок хранит каждую запись как 4×u16 → 4×u32 (16 байт), поэтому
композитор читает crop со страйдом `0x10`; в файле страйд `8`.
`layer_off_x/y` (поля `+0x14/+0x18`) — не «image_width/height»: это смещение
слоя в композите. `F/G` — шаг кадров shadow-CSX слоя, `H/I` — смещение
shadow-слоя. `crops` = 60 записей для 5 рядов, 108 для 9;
`shadow_crops` = `12*(2*rows-2)` (96 для 5, 192 для 9).

`dospeh_almaz.iad` type `0x4` (tb_stay): `fw=36, fh=37, off=(165,154)`,
crop[0] = `(1,5,32,36)`.

## SEQ (`wear/<set>/seq/<anim>_<layer>.seq`)

61 байт: `byte 0` = stride (12), далее `rows × stride` байт; `rows` = 5 для
stay/attack/hits/die/suffer и 9 для go/run. Индекс в массиве:
`[layer][row][col]`; значение — порядок отрисовки (0..4) на кадре.
Базовое тело рисуется в проходе `1` **после** слоёв с порядком ≤1.
Для `tb_stay`: armor=2, elbarms=3, helmet=3, shield=2, weapon=2/4.

## Формула композиции (FUN_0042279C)

Для кадра `(row, col)` анимации `anim`, слоя `layer`:

```
crop   = IAD[type(anim)].crops[row*columns + col]     # x1,y1,x2,y2
fsheet = IAD[type(anim)].frame_w/frame_h
lo     = IAD[type(anim)].layer_off_x/y
aoff   = HAD[anim].layer_off_x/y                      # 158,142 для tb_stay
fw/fh  = HAD[anim].frame_width/height
dest_x = col*fw + lo.x + crop.x1 - aoff.x
dest_y = row*fh + lo.y + crop.y1 - aoff.y
src_x  = col*fsheet.w + crop.x1
src_y  = row*fsheet.h + crop.y1
w      = crop.x2 - crop.x1
h      = crop.y2 - crop.y1
```

Тело: базовый CSX `wear/<set>/animation/<anim>.csx`, кадр
`(col*fw, row*fh, fw, fh)` → `(col*fw, row*fh)`.
Итоговый лист: `columns*fw × rows*fh`; кадр рисуется в мире как
`position - (anchor_x, anchor_y)` (PAD-семантика; Client `FUN_1202c424`).

Тень: отдельный лист. Базовый лист `columns*shadow_w ×
(2*rows-2)*shadow_h`; базовый shadow-кадр
`wear/<set>/shadows/<anim>.csx` кладётся в
`(col*shadow_w, shadow_row*shadow_h)`. Слои тени используют `shadow_crops`
IAD (индекс `shadow_row*12 + col`), шаг `F/G`, смещение `H/I` и
`shadow_off` из HAD:

```
dest_x = col*F + H + crop2.x1 - sox
dest_y = shadow_row*G + I + crop2.y1 - soy
```

Порядок: сначала слои 0..4, затем базовый shadow-кадр. `shadow_row`
пробегает `0 .. 2*rows-3`. Точная семантика `shadow_anchor` (HAD
`+0x10/+0x14` после `shadow_w/h`) и порядок теней ещё не сверены на
пикселях.

Проверка формулы на `tb_stay`, row 0/4, `dospeh_almaz` + `ponogi_goluboy`
+ `shlem_goluboy`: композит совпал с оригинальным внешним видом героя
(синяя броня, синий шлем, поножи; спина/лицо). Смещения совпали с ранее
подобранными вручную: armor `(165+1-158,154+5-142)=(8,17)` (ранее 7,16–18),
legs `(167+0-158,179+9-142)=(9,46)`, helmet `(176+0-158,142+3-142)=(18,3)`.
Это подтверждает семантику `layer_off` и `aoff`; прежние «empirical»
offsets были следствием подгонки, а не engine-правила.

## PersonVisual-блок (0x2C) и PAD-семантика

Из `FUN_0042a5c0`/`FUN_0042a87c`:

```
+0x00 u16 delay
+0x02 u16 frames_per_row
+0x04 u16 frame_width
+0x06 u16 frame_height
+0x08 f32 movement_x
+0x0C f32 movement_y
+0x10 u16 anchor_x
+0x12 u16 anchor_y
+0x14 ptr crops
+0x18 u16 shadow_width
+0x1A u16 shadow_height
+0x1C u16 shadow_anchor_x
+0x1E u16 shadow_anchor_y
+0x20 ptr shadow_crops
+0x24 surface body
+0x28 surface shadow
```

Совпадает по смыслу с `PadAnimation` (порт `pad.hpp`): одинаковые поля
anchors/frame/crops/movement. Значит hero-путь и PAD-путь используют одну
модель отрисовки; отличаются только источником кадров (композит vs готовый PAD).

## `herowear.scr`

`scripts/herowear.scr` (17253 B): секции `category weapon|armor|helmet|
elbarms|shield`, внутри `group "<Group>" ["<wear_set>"]` и
`item "<TECH>"`. Для weapon второй аргумент группы задаёт базовый набор
(`bows`, `onehand_wpn`, `two_swords`, `two_axes_maces`,
`two_spears_staffs`, `xbows_guns`, `noweapon_thrw`).

Парсер — `FUN_00422EEC`. Категории сравниваются с массивом указателей
`0x445260` (`weapon`, `armor`, `shield`, `helmet`, `elbarms` — строки
`0x484410/18/20/28/30`); индекс категории = `iVar6`. Наборы оружия —
массив `0x4452AC` (7 имён). Конфиг PersonVisual: поля `+4`=weapon,
`+8`=armor, `+0xC`=shield, `+0x10`=helmet, `+0x14`=elbarms.

Отсюда layer index в композиторе (loader: `0→+4, 1→+0xC, 2→+0x14,
3→+0x10, 4→+8`):

```
layer 0 = weapon
layer 1 = shield
layer 2 = elbarms
layer 3 = helmet
layer 4 = armor
```

При равном `seq`-порядке слои рисуются в этом порядке (цикл `layer=0..4`).

Категория → каталог: `armor/`, `helmet/`, `elbarms/`, `shield/`, оружие —
базовый набор. Имя каталога — `<Group>` в нижнем регистре (`Dospeh_almaz` →
`armor/dospeh_almaz`, `Ponogi_goluboy` → `elbarms/ponogi_goluboy`,
`Shlem_goluboy` → `helmet/shlem_goluboy`).

Tech-коды (`ARM_1_13_1`, `HLM_1_12_1`, `ELB_1_11_1`, `SPR_1_...`) в
`.itm`/`items.pak`/`scripts.pak` (кроме самого `herowear.scr` и `*.inv`)
не найдены; в ITM есть `unknown_b[1]`, совпадающий со style-номером
(`318 → 13 → ARM_1_13_1`, `280 → 12 → HLM_1_12_1`, `397 → 11 →
ELB_1_11_*`). Точная функция построения tech-строки из item instance
**не извлечена** — открытый вопрос для P5 (inventory), не блокирует
геометрию P4d.

## Что не подтверждено

- Точная привязка item instance → tech-код/группа (`unknown_b`, вариант).
- Порядок/смещения shadow-композита (`2*rows-2`, shadow_anchor
  `+0x1C/+0x1E`) — нужна сверка с live-кадром или тестом.
- Массив направлений для wear-рядов: 8/16-direction mapping и выбор
  `tb_stay/tb_go/run` по состоянию героя (P4b-правила применимы, но
  для hero не проверены).
- Как выбирается базовый набор при оружии в руке (weapon-категория
  `herowear.scr`) — следует из скрипта, но не проверено live.

## Реализация (2026-10-25)

Формулы и форматы выше реализованы: `parse_wear_had`, `parse_wear_seq`,
расширенный `parse_iad`, `gl::game::load_hero_visual` +
`compose_wear_body`/`compose_wear_shadow`, рендер hero в `LevelScreen`,
`glview --hero-wear`, `glgame` (slot0). Golden FNV-1a композита slot0 idle
`0x1A0C2E25C84A1174` совпал; HAD/IAD/SEQ проверены на реальных данных.
Открыто: manual gate, точные shadow anchors/порядок, item→tech
(`herowear.scr`) и состояния `run`/`hits`/`die`.
