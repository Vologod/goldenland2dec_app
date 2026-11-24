# P7b2 — character sheet: doll render and fonts (research)

Дата: 2026-11-24. Статус: закрыт (факты из `Client.dll`, фикс в дереве).

## Источники

- `Client.dll` SHA-256
  `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`
- `Server.dll` SHA-256
  `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`
- `Data/engineres.pak` SHA-256
  `3094DD15B99AC3C0167F7D1F8C7761DB0E787544FB9EB01A85082C852278C737`
- Каталог игры: `I:\Games\Златогорье-2`.
- Дизассемблер: Ghidra headless, дамп
  `.local/decomp/client/decompiled.c` (`ExportDecomp.java`), RVA в
  комментариях `/* rva=... */`.
- Скрипты игры: `glpak.exe extract` → `scripts/fonts.scr`,
  `scripts/ui/inv_gui.scr`; архивы `scripts.pak`, `engineres.pak`.

## Кукла листа

- Экран инвентаря/листа создаётся в `FUN_1205fe20` (RVA `0x5fe20`):
  фоны `engineres/newinv/inventory_secondary.bmp` (`+0x1bc`) и
  `inventory_skills.bmp` (`+0x1c0`), доп. `weapon_slot1.bmp` (`+0x1c4`).
- Отрисовка — `FUN_120661cc` (RVA `0x661cc`, size 8668). Ветка
  `*(this+0x1d0)`: `0` = ХАРАКТЕРИСТИКИ, `1` = НАВЫКИ.
- Контейнер куклы — `scripts/ui/inv_gui.scr`, объект 2:
  `GUI_DD_CONTAINER (8, 296, 270x320)`, `IMAGE_BASE ""` (пустой).
  Кукла — часть фона BMP, не отдельный спрайт:
  в прямоугольнике `(8,296,270x320)` фона 42 021 не-фоновых пикселей,
  241 индекс палитры (`inventory_secondary.bmp`; в skills — 42 201/245).
- Экипировка вокруг куклы рисуется иконками предметов 55x55 по маскам
  (`FUN_120688f0`, RVA `0x688f0`): `0x4000 (67,307)`,
  `0x8000 (168,308)`, `0x10000 (0,354)`, `0x20000 (233,354)`,
  `0x40000 (4,402)`, `0x80000 (229,402)`, `0x1000 (22,472)`,
  `0x400000 (216,472)`, `0x2000 (16,466)`, `0x800000 (211,466)`,
  `0x100000 (16,547)`, `0x200000 (216,547)`. Совпадает с портом
  `kPuppetSlots`.
- Поверх фигуры оригинал дорисовывает `puppet_name`-оверлеи wear-сетов:
  `FUN_120683a8` (RVA `0x683a8`), маски слоёв `0x200000, 0x100000,
  0xF8000, 0x4000, 0xC00000, 0x3000`, активный набор
  `this+0x1d4` (0=A, 1=B). В порте это остаётся residual P5b2
  (фигура в BMP «голая», без оверлеев одежды).

## Шрифты листа

- Именованные шрифты: `scripts/fonts.scr` (без .scr-ссылок в
  `inv_gui.scr`; выбор — в коде экрана).
- `FUN_120661cc` перед текстом вызывает
  `FUN_1202fe38(DAT_121106a4, name)` (RVA `0x2fe38`, выбор текущего
  шрифта по имени):
  - заголовки/вкладки/имя — `heads_interface` (строки 68535, 68560,
    68582; имя — `FUN_12030b4c(... 0xf, 10, text, 0xfa, 0x1a)`);
  - строки — `main_interface` (68532, 68537, 68569, 68589).
- `fonts.scr`: `main_interface` = `pala.ttf`, size 12, weight 500;
  `heads_interface` = `pala.ttf`, size 14, weight 600. Arial в листе
  не используется.
- Размер — в пунктах: `FUN_1202ffd0` (RVA `0x2ffd0`) считает
  `MulDiv(size, GetDeviceCaps(LOGPIXELSY), 72)` и вызывает
  `CreateFontA(-height, ...)` (строка 37268-37286 дампа). При 96 DPI:
  12 pt → 16 px, 14 pt → 18.67 px (18-19 px).
- Цвет текста: `*(DAT_121106a4+0x14) = 0` для активной вкладки/строк
  и `0x464646` (70,70,70) для неактивной вкладки
  (строки 68562, 68587).

## Геометрия текста (замер по BMP)

Боксы строк — светлые прямоугольники фонов; замер `PIL`+порог по
яркости на `inventory_secondary.bmp`/`inventory_skills.bmp`:

- атрибуты: label `22..151`, value `196..225` (правее `−` 172..189 и
  левее `+` 235..255); `Очки опыта`: label `22..202`, value `214..255`.
- state/combat: label `299..567`, value `585..631`.
- resistance: label `665..919`, value `948..999`.
- immunity: col1 label `668..759`, value `773..817`; col2 label
  `844..936`, value `949..994`.
- skills: label `291..429/537..675/783..921`, value
  `457..492/703..737/950..982`; «Очки распределения»: label
  `517..711`, value `739..783`.

Вертикаль: GDI `DT_VCENTER` центрирует клетку шрифта, поэтому порт
берёт `y = box_top + (box_h - text_height)/2`, где
`text_height = ascent+descent` (`Font::text_height()`), а не
`line_height`: у Palatino `line_gap` = 682/2048 em (~5 px при 16 px),
из-за чего текст уезжал вверх и обрезался clip-ом строки.

## Порт до фикса (отклонение)

До 2026-11-24 порт рисовал куклу как world-hero `HeroPortrait`
(`pad_person_row(tb_stay, 4)`) в `(8,296,270x320)` поверх уже
нарисованной фигуры из BMP, атекст — `arial.ttf` 18/16
(`tools/glgame/main.cpp`, `load_character_fonts`).

## Фикс (2026-11-24)

- `HeroPortrait` удалён (`src/app/{include/gl/app,src}/hero_portrait.*`):
  кукла берётся только из фона.
- `CharacterOverlay::open` без `HeroOutfit*`; фон/иконки/кукла —
  из BMP + `HeroInventory`/`ItemCatalog`.
- Шрифт листа → `engineres/fonts/ttf/pala.ttf`: заголовки 17 px,
  строки 16 px (17 вместо 19 — чтобы `БОЕВЫЕ ХАРАКТЕРИСТИКИ` влезал
  в полосу 245 px; ширина строки при 18 px = 253 px, при 17 ≈ 239 px).
- `resolve_hero_outfit(prefer_b)` и `--character` outfit-ветка
  откатаны (нужны были только для world-куклы).

## Остаточные расхождения

- Wear-оверлеи `puppet_name` на кукле (P5b2-1).
- Начертание: `pala.ttf` vs системный «Palatino Linotype»
  (fonts.scr `type_face`), weight 500/600 не воспроизводится
  (stb_truetype без синтетического веса).
- Точная точка привязки текста заголовков (центр полосы по
  скриншотам ручного гейта; `FUN_12030a88` принимает x/y регистрами,
  восстановление сигнатуры неоднозначно).
