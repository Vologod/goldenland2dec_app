# P5c2 — Armor class / weight extraction (focused research)

Дата: 2026-11-25. Статус: extraction (математика снята; маппинг item
prototype и wrapper-vtable остаётся — см. «Открытые вопросы»).

Источник: `Client.dll`, SHA-256
`FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`,
size `1,331,200`, `ImageBase = 0x12000000` (проверено `llvm-objdump -p`).
`RVA = VA − 0x12000000`.

Метод: `llvm-objdump -d --x86-asm-syntax=intel` + декомпилятор из
`.local/decomp/client/decompiled.c`; константы сняты из `.data1`
(`llvm-objdump -s --section=.data1`).

## 1. `FUN_1201b1c4` — универсальный модификатор стата (разобран)

Сигнатура: `__thiscall float10 FUN_1201b1c4(float value, int statId)`,
`ecx = this` (wrapper персоны), `ret 0x8` (2 стек-аргумента).

`this` — «client person wrapper» (см. §2). Поля-контейнер модификаторов
(заполняются отдельно):
- `this+0x48` — count активных записей (int)
- `this+0x18` — gate (если 0 → модификаторы не применяются)
- `this+0x44` — массив записей, stride `0x14` (20 байт): `{+0 id,
  +4 int value, +8 uint flags, +0xc uint cond, ...}`
- `this+0x4c` — bitmap «активности» записей

Алгоритм (подтверждён дизассемблером `0x1201b1c4..0x1201ba9f`):
1. `base = value` (double).
2. Если `count==0` или `this+0x18==0` → вернуть `base`.
3. `override = DAT_12123c88 = 0.0` (`+0x12123c88`, dword 0),
   `have_override = false`, пустой множество дельт.
4. Для каждой активной записи с `id == statId`, если
   `FUN_12020e68(this, entry) != 0` (проверка условий/времени, §6):
   - `flags & 0x40 == 0 && flags & 0x80 == 0` **или** `have_override`:
     ветка override: первый раз `have_override=true; override=value`;
     далее `if (|override| < |value|) override = value;`
   - `flags & 0x40` (flat): добавить `(float)value` в дельты.
   - `flags & 0x80` (percent): добавить `value * 0.01 * base`
     (`fild value; fmul [0x12123c84]=0.01; fmul base`).
5. Если `have_override` → вернуть `override` (дельты игнорируются).
6. Иначе `base += Σ дельт`; вернуть `base`.

Итог: `0x40` — плоская прибавка, `0x80` — процент от базы, всё прочее —
«set/override» (берётся максимальный по модулю). Это объясняет, почему
броня/вес идут через `FUN_1201b1c4` — там накладываются экипировочные
модификаторы статов `0x1d` (броня) и `0x1e` (вес).

## 2. Person wrapper и запись статов

- `FUN_1201b034(this, personId)` (RVA `0x1b034`) — «выбрать персону»:
  копирует **0x47 dword (71 int)** из записи, полученной
  `vtable[this+4](personId)`, в `this+0x50 .. this+0x16b`; затем
  `vtable[+0xc]()` (добивает зависимые бонусы) и
  `vtable[+0x10](this+0x40)` (строит контейнер модификаторов +0x40).
  При `nullptr` обнуляет `this+0x50..this+0x168`.
- `this+0x14` — текущий `personId`; `this+0x50` — начало копии записи.

Сверка записи с `BT_HERO` (`parse_hero_stats`, body offset `0x0C`):

| wrapper | record | BT_HERO body | поле |
|---|---|---|---|
| `+0x50` | [0] | `0x0C` | strength |
| `+0x54` | [1] | `0x10` | constitution |
| `+0x58` | [2] | `0x14` | dexterity |
| `+0x5c` | [3] | `0x18` | perception |
| `+0x60` | [4] | `0x1C` | intelligence |
| `+0x64` | [5] | `0x20` | wisdom |
| `+0x68` | [6] | `0x24` | luck |
| `+0x6c` | [7] | `0x28` | reputation |
| `+0x70` | [8] | `0x2C` | experience |
| `+0x7c` | [11] | `0x38` | skills[0..26] (`+4*id`) |

→ клиентская запись персоны **совпадает** с `BT_HERO` person-record,
который порт уже парсит (`gl::assets::HeroStats`). `HeroStats` содержит
strength/constitution/dexterity/perception/experience/reputation/skills —
достаточно для брони и веса.

## 3. Геттеры атрибутов (навык-подобные, с бонусом)

Все: `__thiscall int(this, personId)`; при смене id вызывают
`FUN_1201b034`; значение = `round(FUN_1201b1c4(recordField, statId))`
(+ бонус `this+<bonus>` если `this+0xc != 0`), затем `if (<2) =1`,
cap = `DAT_12106ac0+0x8c`.

| функция | RVA | поле базы | stat id | поле бонуса |
|---|---|---|---|---|
| `FUN_12020620` | `0x20620` | `this+0x50` (strength) | 6 | `this+0x16c` |
| `FUN_120205b8` | `0x205b8` | `this+0x54` (constitution) | 7 | `this+0x170` |
| `FUN_12020550` | `0x20550` | `this+0x5c` (perception) | 8 | `this+0x178` |
| `FUN_120204e8` | `0x204e8` | `this+0x58` (dexterity) | 9 | `this+0x174` |

(Бонусные поля `0x16c..0x178` заполняет `vtable[+0xc]`; для голого героя
без экипировки/бафов = 0.)

`FUN_1201fab4(this, personId, skillId)` (RVA `0x1fab4`):
`clamp(base[0x7c+4*id] + (this+0xc ? bonus[0x198+4*id] : 0), 0,
DAT_12106ac4+0x8c)`; `id>0x1a → 0`.

`FUN_1201fb10(this, personId)` (уровень, RVA `0x1fb10`): exp =
`FUN_1201fbd4` (`this+0x70`, clamp `0..500000`); уровень 1; пока
`exp>99`: шаг `+50`, `level++`, условие `level*step+step <= exp`;
clamp `1..99`.

`FUN_1201fbd4` RVA `0x1fbd4`; `FUN_12004794` (round) RVA `0x4794` =
`lround(value)` (half away from zero: `x>0 → x+0.5`, иначе `x-0.5`,
затем усечение; константы `+0x12123528=0.5`, `+0x1212352c=-0.5`,
`+0x12123530=0.0`).

## 4. `Класс брони` — `FUN_1201f7a0` (RVA `0x1f7a0`, size 277)

Дословно (дизасм `0x1201f7a0..0x1201f8b2`, подтверждён построчно):
```
if personId != this+0x14: FUN_1201b034(this, personId)
item   = vtable[+0x18](personId, -1)      // надетый «броневой» item
proto  = vtable[+0x28](item)              // прототип item (может быть 0)
dext   = FUN_120204e8(this, personId)     // dexterity (stat 9)
skill6 = FUN_1201fab4(this, personId, 6)  // навык 6
armor  = (float)(dext + skill6) * 0.334f  // DAT_12123f14
if proto != 0:
    dur = vtable[+0x2c](item)             // прочность item (int)
    armor += (unsigned(proto+0x24)) *
             (dur * 0.005f + 0.5f)        // DAT_12123f0c=0.005, f10=0.5
armor = FUN_1201b1c4(armor, 0x1d)         // модификаторы стата «броня»
if DAT_12106bc0+0x8c != 0 and FUN_1201fab4(personId,4) >= 15:
    armor *= 1.35f                        // DAT_12123f08
result = round(armor); clamp 0..50
```
`unsigned(proto+0x24)`: `fild [proto+0x24]` + при отрицательном
`fadd 2^32` (`[0x12123f18]=0.0f`, `[0x12123f1c]=0x4f800000` = 2^32) —
т.е. чтение как unsigned 32-bit. Для обычных (положительных) значений
поправка = 0.

Константы `.data1`:
`+0x12123f14=0.334f`, `+0x12123f0c=0.005f`, `+0x12123f10=0.5f`,
`+0x12123f08=1.35f`, `+0x12123f18=0.0f`, `+0x12123f1c=2^32`.

## 5. `Вес` — `FUN_1201e92c` (RVA `0x1e92c`, size 335)

Сигнатура `__thiscall int(this, personId, int* cur, int* max)`
(`ret 0xc`); возвращает 1 при успехе, пишет в out-параметры:
```
if cur:  *cur = (DAT_12106ab8+0x8c != 0) ? 13
                : round(vtable[+0x3c](personId))   // текущий вес
if max:
    a = FUN_12020620(personId)   // strength (attr stat 6)
    b = FUN_120205b8(personId)   // constitution (attr stat 7)
    lvl = FUN_1201fb10(personId)
    base = a*20 + b*20 + lvl
    m = FUN_1201b1c4((float)base, 0x1e)      // модификаторы стата «вес»
    if FUN_1201fab4(personId,6) >= 5: m *= 1.5      // double @+0x12123e80
    if DAT_12106bc0+0x8c == 0 and (this+0xf8 & 0x2000): m *= 1.1f
    *max = round(m); clamp 1..9999
return 1
```
Константы: `+0x12123e80` (double) = `1.5` (lo `0`, hi `0x3ff80000`);
`+0x12123e90` (float) `0x3f8ccccd` = `1.1f`.

Замечание по UI: обе величины пишутся как `int`. Пользовательский
ориентир `Вес 132/204.6` — вторая часть с дробью, значит отображение
`Вес` в листе, вероятно, не равно сырым `cur/max` этого геттера
(возможно, max = capacity в других единицах/десятых). Требует сверки по
реальному сейву/скриншоту — Open Question.

## 6. `FUN_12020e68` (применимость модификатора, RVA `0x20e68`)

Проверяет `entry.flags` (по `entry+8`):
- `&2`: требует `*(float*)(this+0x3c) == DAT_12123f28`;
- `&4`: требует `*(float*)(this+0x3c) == DAT_12123f24`;
- `&8`: сравнивает упакованное время `entry+0xc` с `this+0x2c`
  (час) и `this+0x30` (тик) — интервал времени суток;
- `&0x200`: всегда отклонить;
- иначе принять.
`this+0x3c/0x2c/0x30` — время/состояние мира, заполняется в
`FUN_1201b034` через `vtable[+0xc]`.

## 7. Что уже есть в порте

- `parse_hero_stats` → strengths/exp/skills (все входы для брони/веса).
- `HeroInventory` (equipped) + save item `+0xB4` = durability/condition
  (`docs/research/p5-save-equipment.md`).
- `ItemCatalog`/`ItemTemplate` из ITM: type/flags/effects/weight_x10.

## Открытые вопросы / следующий шаг

> Обновление 2026-11-25: пункты 1–5 ниже **закрыты** — см. §8–§11
> (vtable `PTR_FUN_12131340`, prototype `+0x28 = unknown_b[3]`,
> модификаторы из ITM effects, проверка на slot0). Остались только
> уточнения из §12.

1. **Wrapper vtable не найдена по адресу.** Найдены только vtable
   контейнеров (`0x12123c10` — sparse set ctor `FUN_1201adb4`;
   `0x12124024` — временный sparse-vector внутри `FUN_1201b1c4`).
   Семантика методов wrapper выведена из вызовов, но не подтверждена
   таблицей:
   - `+0x18(personId, slot)` → item (slot `-1` в броне = «броневой» слот);
   - `+0x1c/+0x20/+0x24(personId)` → слоты оружия;
   - `+0x28(item)` → **прототип item**: `+4` type (0..7 = оружие,
     подтверждено `FUN_1201aedc`), `+8` flags, `+0x24` armor, `+0x84`
     damage;
   - `+0x2c(item)` → прочность/condition (`0..100`, ср. save `+0xB4`);
   - `+0x3c(personId)` → текущий переносимый вес (int).
2. **Маппинг ITM-файла → prototype `+0x24` (armor) / `+0x84` (damage).**
   ITM `Item` (`itm.hpp`) не содержит полей armor/damage для брони;
   вопрос — откуда прототип берёт `+0x24`. Кандидаты: `unknown_b[5]`,
   effects, или отдельный ITM-блок. Нужно найти загрузчик item-прототипа
   (вероятно `Server.dll`/ресурс-менеджер) и снять соответствие
   file-offset → runtime-offset.
3. **Контейнер модификаторов (`this+0x40`)** заполняется
   `vtable[+0xc]`/`vtable[+0x10]` из экипировки/эффектов. Нужно снять,
   какие item effects дают stat id `0x1d` (броня) / `0x1e` (вес) и с
   какими flags (`0x40`/`0x80`/override).
4. **Глобалы-капы и флаги режима:** `DAT_12106ac0+0x8c`,
   `DAT_12106ac4+0x8c` (caps), `DAT_12106ab8+0x8c`,
   `DAT_12106bc0+0x8c` (режимные флаги; для обычной игры вероятно 0),
   `this+0xf8 & 0x2000`. Источник значений — конфиг/инициализация.
5. **Формат `Вес` в UI** (`132/204.6`) — сверить с реальным сейвом.

## Команды проверки (воспроизводимость)

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$od="$tools\llvm-mingw-20261006-ucrt-x86_64\bin\llvm-objdump.exe"
& $od -d --x86-asm-syntax=intel --no-show-raw-insn `
  --start-address=0x1201f7a0 --stop-address=0x1201f8b8 "I:\Games\Златогорье-2\Client.dll"
& $od -d --x86-asm-syntax=intel --no-show-raw-insn `
  --start-address=0x1201e92c --stop-address=0x1201ea7b "I:\Games\Златогорье-2\Client.dll"
& $od -s --section=.data1 "I:\Games\Златогорье-2\Client.dll"
```

## 8. Wrapper vtable — найдена (2026-11-25, продолжение)

Синглтон-обёртка персоны = `DAT_12110efc` (alloc 0x28c в
`FUN_120c92f8`, ctor `FUN_120b3784`). Vtable = `PTR_FUN_12131340`
(`0x12131340`). Методы (RVA = dword записей таблицы):

| слот | RVA | роль |
|---|---|---|
| `+0x04` | `0xb3990` | get person record (0x47 int) |
| `+0x0c` | `0xb0278` | rebuild modifier container из экипировки |
| `+0x10` | `0xb0690` | инициализация sub-контейнера `this+0x40` |
| `+0x18` | `0xb1708` | get **shield/ammo** item (active wear set) |
| `+0x1c..+0x24` | `0xb1bf8/0xb2080/0xb2508` | get weapon-слоты |
| `+0x28` | `0xb07fc` | `itemIndex → instance+4` |
| `+0x2c` | `0xb389c` | durability = `instance+0xB4` (default 100) |
| `+0x3c` | `0xb096c` | current weight = Σ `count*weight_x10` |

`+0x18(personId, -1)`: param `0/1` = набор A/B, `-1` = активный набор;
возвращает элемент с экипировочной маской `0x400000`/`0x800000`
(**shield**) либо item типа `8..10`/`13`; иначе `-1`.

## 9. Prototype/instance layout — найдено

`Server.dll` `FUN_1404f4fc` (RVA `0x4f4fc`) строит runtime item record
из def-объекта `src` (менеджер `DAT_140891bc+0xa0`, vtable `+0xc`):
```
inst+0x08 = src+0x04  (type)
inst+0x0C = src+0x08  (flags)
inst+0x10 = src+0x0C  (price)
inst+0x14 = src+0x10  (weight_x10)
inst+0x20 = src+0x1C  (style = ITM unknown_b[1])
inst+0x28 = src+0x24  (ARMOR = ITM unknown_b[3])
inst+0x30 = src+0x2C  (material)
inst+0x78 = src+0x74  (modifier count)
inst+0x7C = src+0x78  (modifier array ptr)
inst+0x80..0x94 = src+0x7C..0x90 (damage[6], для типов 0..7,11..14,0x1c)
inst+0xB0 = 1 (count), inst+0xB4 = 100 (durability)
```
Модификаторы = 20-байтные записи `{id=effect.type, value=effect.amount,
flags=effect.flags, cond=packed time, extra=0xFFFFFFFF}` (маппинг
proof 741/741, `docs/research/p2-15c3b-item-modifiers.md`).

`FUN_120b0278` (`+0x0c`) очищает контейнер `this+0x40` и для каждого
надетого item с `(flags & 0x80)==0` и `mod_count != 0` копирует его
модификаторы в контейнер (`+0x44` данные, `+0x48` count, `+0x4c`
bitmap). Т.е. стат-модификаторы `0x1d/0x1e` приходят из **ITM effects**
надетых предметов.

## 10. Эмпирическая проверка (slot0, реальный сейв)

`Saves/slot0.sav` (zlib после 112-байтного header, tag `BT_HERO`) —
экипировка (11 предметов), `+0xA8` = def id:

| id | type | wt | dur | inst+0x28 | inst+0x88 |
|---|---|---|---|---|---|
| 72 | 15 | 1 | 100 | 0 | 0 |
| 397 | 14 | 36 | 100 | 0 | 61 |
| 280 | 11 | 21 | 100 | 0 | 50 |
| 318 | 12 | 98 | 100 | 0 | 84 |
| 554 | 0 | 26 | 91 | 0 | 14 |
| 486 | 1 | 45 | 100 | 0 | 72 |
| **353** | **13 (shield)** | 23 | 100 | **20** | 0 |

`+0x28` совпал с ITM `unknown_b[3]`: shield 353 `unknown_b=0,10,0,20,0`
→ 20; armor 318 `unknown_b=0,13,0,0,0` → 0. `+0x88` = ITM `damage[2]`
(318: 56,0,**84**,0,47,0; 397: 35,0,**61**,0,41,0; 280: 41,0,**50**...).

**Проверка модели на slot0:**
- HeroStats: S=30 C=25 D=18 EXP=81099 (level 40);
  skills[6]=10, skills[4]=0.
- `Вес cur` = Σ`count*weight_x10` по 3 контейнерам = 8+1058+254 = **1320**
  → отображение `/10` = **132** — точно совпало с ориентиром
  пользователя `Вес 132/…`.
- `Вес max` raw = `(30*20+25*20+40)` = 1140; при skills[6]>=5 `*1.5` →
  1710 (`/10 = 171.0`). Ориентир `204.6` требует ещё `+0x1e`-модификаторов
  экипировки (сейв пользователя не подтверждён) — сходится по структуре.
- `Класс брони` = `(18+10)*0.334 + 20*(100*0.005+0.5)` = 9.352 + 20 =
  29.35 → round 29 (модификаторы `0x1d` и флаг-множитель не учтены).

Вывод: **модель подтверждена** (вес cur — 0/N); `unknown_b[3]` —
броня щита, `damage[2]` — боевое значение; `+0xa8` id валиден.

## 11. Итоговая формула для порта

```
armor = lround((dexterity + skills[6]) * 0.334
             + shield.unknown_b[3] * (shield_durability * 0.005 + 0.5)
             + mods(0x1d))
if flag_bc0 != 0 and skills[4] >= 15: armor = armor * 1.35
armor = clamp(lround(armor), 0, 50)

weight_cur_10 = Σ count * weight_x10   (equipped + hot + main)
weight_max = mods(0x1e)( strength*20 + constitution*20 + level )
if skills[6] >= 5: weight_max *= 1.5
if flag_bc0 == 0 and (person_flag_0xf8 & 0x2000): weight_max *= 1.1
weight_max = clamp(lround(weight_max), 1, 9999)
```
`mods(id)` = `FUN_1201b1c4`: flat `0x40` add, `0x80` → `value*0.01*base`,
иначе override (max |value|, дельты игнорируются). Для UI значения веса
делятся на 10 (cur: 1320→132; max: 1710→171.0).

## 12. Что осталось уточнить (не блокеры)

- Флаг-множители `DAT_12106bc0+0x8c`, `DAT_12106ab8+0x8c`,
  `this+0xf8 & 0x2000`: для обычной игры вероятно 0; сверить на живой
  игре/сейве.
- Условия модификаторов (`effect.flags` биты 2/4/8/0x200 и packed
  time) — реализовать только безусловные, остальные отметить как
  residual.
- Точное форматирование `Вес` в UI (`.` и `/10`) — подтвердить на
  скриншоте листа; cur=132 уже сошёлся.

## 13. Модификаторы атрибутов + «Очки действия» (2026-11-25)

**Атрибуты читаются геттерами, которые применяют стат-модификаторы
экипировки** (`FUN_1201b1c4(field, statId)` + бонус `this+0x16c..`):

| геттер | RVA | поле | stat id |
|---|---|---|---|
| `FUN_12020620` | `0x20620` | strength `+0x50` | 6 |
| `FUN_120205b8` | `0x205b8` | constitution `+0x54` | 7 |
| `FUN_12020550` | `0x20550` | perception `+0x5c` | 8 |
| `FUN_120204e8` | `0x204e8` | dexterity `+0x58` | 9 |

Поэтому в формуле брони/веса/AP атрибуты — **после модификаторов**
(`round(FUN_1201b1c4(raw, id))`, `<2 → 1`). Ранее (§11) использовались
сырые значения; это исправлено.

**`Очки действия` — `FUN_1201ecd8(this, personId, flag)`**
(RVA `0x1ecd8`):
```
(cur,max) = FUN_1201e92c(...)             // вес, §5
overload = (cur - max) / 10; if (<1) 0    // integer div
if flag != 0: overload = 0
base = skills[26]*(2/3) + (dexterity + (perception>>1))
       + (5.0f - overload)
if skills[6] > 14: base += 5.0f
ap = clamp(lround(FUN_1201b1c4(base, 0x1b)), 5..60)   // <6→5, >59→60
```
Константы `.data1`: `+0x12123ea4 = 0.6666667f` (2/3),
`+0x12123ea8 = 5.0f`.

**Исправленный slot0** (модификаторы из сейва; активный набор = A):
- dex getter: 18 +2 (shield 353 `{9,2}`) = **20** (моды применяются
  независимо от набора);
- con: 25 +1+2+2+2 = **32**; perception: 12 +1+2-2 = **13**.
- **armor** = round((20+10)*0.334 + 0 + 5) = **15** (щит набора A
  `mask 0x400000` отсутствует; 0x1d +5 от ring 233). Совпало с
  ориентиром пользователя `Класс брони 15`.
- **weight max** = (30*20 + 32*20 + 40) * 1.5 = **1920**;
- **weight cur** = **1320** (→132);
- **AP** = round(10*(2/3) + (20 + 6) + 5 + 5) = **43**
  (0x1b +5 от sword 554).

### 13.1 Активный набор A/B

`local_player+0x6E4` (0=A, 1=B) — **runtime UI-состояние, не сейвится**
(инициализируется 0; переключается клавишами через `FUN_120c7008`,
`p5b1-inventory-ui.md`). Порт следует уже принятому решению — набор **A**:
- weapon `mask 0x001000`, shield `mask 0x400000`, armor `mask 0x100000`,
  helmet `0x004000`, elbarms `0x200000`.
Геттер щита (`+0x18`) с `param=-1` берёт активный набор; для A это
`0x400000`. В slot0 предметов `0x400000` нет → щита нет.

Остаток: бонусные поля `this+0x16c..` (vtable `+0xc`) и бонусные навыки
`this+0x198` не моделируются (для slot0 не влияют на проверенные
значения).

## 14. Шанс критического удара — `FUN_1201e384` (2026-11-25)

RVA `0x1e384`, size 239. Не зависит от оружия.
```
skill = FUN_1201fab4(person, 0x18)         // skills[24]
if skill == 0: crit = 0
luck  = FUN_12020400(person)               // this+0x68 (record[6]), stat id 0xc
base  = (luck*0.334f + skill*0.667f) * 0.5f
crit  = lround(FUN_1201b1c4(base, 0x33))    // стат «крит»
clamp: <1 → 0, >9 → 10
```
Константы `.data1`: `+0x12123e28 = 0.334f`, `+0x12123e2c = 0.667f`,
`+0x12123e24 = 0.5f` (плюс `e20 = 0.15f` в cheat-ветке `*5`, не
моделируется). `luck` — после модификаторов атрибута (stat 12).

**slot0:** luck=9, skills[24]=15, моды `0x33`: ring 233 `{51,3,0x81}`
(+3%) и axe 486 `{51,5,0x81}` (+5%) → `6.5055 * 1.08 = 7.026` → **7**.

## 15. Урон — физический и стихийный (2026-11-25)

Оружие = надетый предмет набора A (`flags & 0x001000`).

**Физический урон** (3 геттера): берут ITM `damage[6]` пары и
масштабируют:
```
a = 1 + strength*0.01                     // FUN_1201c3f4, strength после модов
factor = (durability*0.005 + 0.5) * a
vb = weapon_skill(type)                   // FUN_1201aedc по типу оружия
min = round(mods(statId)( damage[i]   * factor + (damage[i]==0 ? 0 : vb) ))
max = round(mods(statId)( damage[i+1] * factor + (damage[i]==0 ? 0 : vb) ))
```
| геттер | RVA | пара damage | stat id | UI |
|---|---|---|---|---|
| `FUN_1201f5e0` | `0x1f5e0` | `[0],[1]` | `0x22` | Дробящее |
| `FUN_1201f2d0` | `0x1f2d0` | `[2],[3]` | `0x21` | Рубящее |
| `FUN_1201f458` | `0x1f458` | `[4],[5]` | `0x23` | Колющее |

`weapon_skill` (FUN_1201aedc): type 3/4/5 → skills[9]; `flags&0x10` →
skills[11]; 0→[12], 1→[13], 2→[10], 6→[8], 7→[7]; иначе 0.

**Стихийный урон** (3 геттера): суммируют моды **оружия** с `word0==idx`;
`+5`, если соответствующий навык > 4; при `sum==0` → 0, иначе (обычная
игра, флаг режима 0) → `1/sum`:
| геттер | RVA | mod idx | skill id | UI |
|---|---|---|---|---|
| `FUN_1201edfc` | `0x1edfc` | 0 | `skills[5]` | Яд |
| `FUN_1201ef98` | `0x1ef98` | 1 | `skills[4]` | Холод |
| `FUN_1201f134` | `0x1f134` | 2 | `skills[0]` | Огонь |

Формат строки: `min-max`, либо `0` при `0/0`.

**Оракул (`.local/p7b3/original_characteristics.png`, slot0) —
совпало 6/6:** Дробящее `76-96`, Колющее `0`, Рубящее `125-141`,
Огонь `0`, Холод `1-30`, Яд `0`. (Оружие набора A резолвится
каталогом: damage `47,62,85,97,0,0`, cold-мод +30; НЕ `486.itm`.)

**Вес max — флаг `0x2000`.** `this+0xf8` = `record[42]` (BT_HERO body
`+0xB4`); в slot0 `record[42]=0x3567`, бит `0x2000` **установлен** →
множитель `*1.1` применяется (при `global?0x8c==0`). С капом атрибута
(§17, con 32→30): `(30*20 + 30*20 + 40)*1.5*1.1 = 2046` (→`204.6`) —
**совпало с оригиналом**.

## 16. Точность, сопротивления, иммунитеты — DONE (2026-11-25)

`Базовая точность` `FUN_1201e660` (RVA `0x1e660`, `ret 0x8`) разобрана
полностью:
- Скрытый 2-й аргумент (`unaff_EDI`) в call-site листа
  (`FUN_1205e0cc` → `push 0x0; push person; call`) = **0**; вклад
  `arg*-5` исчезает.
- `instance+0x2C` = `src+0x28` дефа = ITM `unknown_b[4]` (у оружия
  набора A slot0 = **70**).
```
base = (perception*1.5 + 2*skills[24] + (luck>>1))*0.334 + 50
if weapon:
    base = (unsigned(unknown_b[4]) + base) * 0.6
    skill bonus: type 3/4/5 -> skills[9]; flags&0x10 -> skills[11];
                 0->[12],1->[13],2->[10],6->[8],7->[7]; if >9: +15
    statId = (type in 3/4/5) ? 0x38 : 0x32
else:
    if skills[14] > 9: base += 20; statId = 0x32
acc = clamp(lround(mods(statId)(base)), 0..90)
```
Константы: per-множитель `1.5`, `0.334`, `+50`, weapon `*0.6`,
`+15`/`+20`, знак unsigned (0.0/2^32).

**slot0:** per=13, luck=9, skills[24]=15 → base 67.869; weapon
`(70+67.869)*0.6=82.72`; axe skill[13]=15>9 → +15 = 97.72 → round 98 →
clamp **90** — совпало с оригиналом (`Базовая точность 90%`).

**Магические сопротивления (6) — DONE.** Каждый (`FUN_12020050/200d8/
20160/201e8/20270/202f8`) = `FUN_12020380(wisdom) + skills[k] + skills[17]`,
затем `mods(statId)`, clamp 0..90. `FUN_12020380` = wisdom-геттер
(record[5], stat 0xb) + (`skills[21]>4 ? 1 : 0`). `k`=0..5, statId
`0x0d..0x12` — соответствие UI 33..38: `20270→sk4/0x11`, `202f8→sk5/0x12`,
`20050→sk0/0x0d`, `200d8→sk1/0x0e`, `20160→sk2/0x0f`, `201e8→sk3/0x10`.

**Иммунитеты (6) — DONE.** Каждый (`FUN_1201ff00/ffa8/fc60/fd08/fdb0/
fe58`) = `perception + skills[k] + skills[17]`, `mods(statId)`, clamp
0..90. UI 43..48: `ff00→sk4/0x2e`, `ffa8→sk5/0x2f`, `fc60→sk0/0x2a`,
`fd08→sk1/0x2b`, `fdb0→sk2/0x2c`, `fe58→sk3/0x2d`.

**slot0 (совпало со скриншотом):** магия `40,40,40,40,10,10`
(wisdom=10, skills[17]=0, моды stat 0x0d/0x0e/0x11/0x12 +30 у 72);
иммунитеты `13,38,38,13,38,13` (perception=13, моды stat 0x2a/0x2c/0x2f
+25 у ring 233).

**Стихийные сопротивления (3) — DONE.** Каждый = `mods(skills[k], statId)`
+ `10` за `skills[17]>4`, +`10` за `skills[1]>4`, +`10` за `skills[20]>4`;
clamp 0..90. UI 39..41: `ce0c→skills[22]/stat 5` (Огонь),
`cd4c→skills[25]/stat 4` (Холод), `cc8c→skills[20]/stat 3` (Яд).

**slot0 (совпало со скриншотом):** `11,10,24` (Огонь/Холод/Яд).

**Физические сопротивления (3) — DONE.** Сумма по 3 armor-слотам
(`flags & 0x304000` = armor 0x100000 / elbarms 0x200000 / helmet 0x4000)
их ITM `damage[idx] × (dur*0.005+0.5)`, затем `sum*mult + skill6 +
attr*0.334`, `mods(stat 0x13)`, clamp 0..90:
| getter | UI | attr | damage idx | mult |
|---|---|---|---|---|
| `FUN_1201d244` | 30 Дробящее | constitution | 0 | 0.334 |
| `FUN_1201d088` | 31 Колющее | dexterity | 4 | 0.334 |
| `FUN_1201cecc` | 32 Рубящее | perception | 2 | 0.333 |

**slot0:** `64,71,79` (с капом con=30 — **совпало с оригиналом**).

**Verdict:** весь combat-блок листа смоделирован по оригиналу (урон 6/6,
armor, weight, AP, crit, accuracy, resistance 12/12, immunity 6/6) —
**все значения совпали с оригинальным скриншотом**.
Primary-атрибуты листа (Сила..Удача) выводятся после модификаторов
экипировки + капа (`attribute_value`, stat ids 6/7/9/8/11/10/12).
Остатки — не-боевые: бонусные поля `this+0x16c..`/бонусные навыки
`this+0x198`, режимные флаги.

## 17. Капы атрибутов/навыков (2026-11-25)

Конфиг (регистрируется при старте, `Client.dll` rva `0x…`, строки
`ch_max_skill_value`/`ch_max_primary_value`; в `GoldenLand.ini` их нет →
дефолты):
- `ch_max_primary_value` default **30** (`DAT_12106ac0+0x8c`): геттеры
  атрибутов (`FUN_1202xxxx`) клампят `if (cap <= value) value = cap`.
  → Телосложение 25+7=32 → **30**; отсюда вес 204.6, Дробящее 64,
  primary Телосложение 30 — совпало с оригиналом.
- `ch_max_skill_value` default **15** (`DAT_12106ac4+0x8c`): геттер
  навыков `FUN_1201fab4` клампит до 15 (slot0 skills ≤15 — без эффекта;
  для общих сейвов учесть).

## Non-goals

- Урон/сопротивления/иммунитеты (P5c3..P5c6) — здесь только броня/вес.
- Изменение существующих runtime (P5a/P5b).
