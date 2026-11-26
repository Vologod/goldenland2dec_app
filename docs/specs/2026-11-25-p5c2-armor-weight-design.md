# P5c2 — Character sheet armor class / weight (design spec)

Дата: 2026-11-25. Статус: **proposed** (нужно одобрение перед реализацией).

Источник фактов: `docs/research/p5c2-armor-weight-extraction.md`
(`Client.dll` SHA-256 `FBF213B4…AB5C16`, `Server.dll`
`418E2560…2CC837`). Подпроект: **P5** (игровые системы), UI-часть — **P7**.

## 1. Цель

Показывать в листе персонажа реальные значения вместо `—` для:
- `Класс брони` (UI id 17) ← `FUN_1201f7a0`;
- `Вес` (UI id 29) ← `FUN_1201e92c`;
- `Очки действия` (state id 18) ← `FUN_1201ecd8`;
- `Шанс критического удара` (UI id 26) ← `FUN_1201e384`.

Значения вычисляются по формулам оригинала из загруженного сейва
(`HeroStats` + экипировка + ITM), а не читаются готовыми.

## 2. Не входит в scope

- Остальной combat (6 типов урона), resistance (12), immunity (6),
  `Очки действия` — отдельные инкременты P5c3..P5c6.
- Условные модификаторы статов (time-of-day: `effect.flags` биты
  `2/4/8/0x200`) — в первой версии пропускаются (residual).
- Флаги режима (`DAT_12106ab8/bc0+0x8c`, `this+0xf8&0x2000`) — считаются
  0 (обычная игра); residual.
- Изменение runtime/сохранения; только чтение.

## 3. Точная модель (из оригинала)

### 3.1 Модификаторы стата (`FUN_1201b1c4`)

Вход: `base` (дробное), `stat_id`. Собираются все эффекты надетых
предметов, у которых `effect.type == stat_id` и эффект применим
(`effect.flags & (0x2|0x4|0x8|0x200) == 0`), при `(item.flags & 0x80)==0`.

```
override = 0.0; have_override = false; deltas = []
for each applicable effect:
  if (flags & 0x40) == 0 && (flags & 0x80) == 0 or have_override:
     if !have_override: have_override = true; override = amount
     else if |override| < |amount|: override = amount
  elif flags & 0x40: deltas.add(amount)
  else (flags & 0x80): deltas.add(amount * 0.01 * base)
return have_override ? override : base + Σdeltas
```
float32-арифметика как в оригинале (`float`), `amount` — знаковый int32.

### 3.2 Класс брони

Атрибуты (`dexterity`, `perception`, `strength`, `constitution`) берутся
**после** применения стат-модификаторов экипировки (stat ids 9/8/6/7):
`round(FUN_1201b1c4(raw, id))`, `<2 → 1`, `≥30 → 30`
(`ch_max_primary_value` default 30) (`FUN_120204e8/20550/20620/205b8`).

Щит = надетый предмет с `(item.flags & 0x400000) != 0` (активный набор
A; порт по умолчанию A), иначе `-1`.

```
base = (f32)(dexterity + skills[6]) * 0.334f
if shield != -1:
    base += (f32)(u32)itm.unknown_b[3] * ((f32)durability * 0.005f + 0.5f)
value = stat_modifiers(base, stat_id=0x1d)
// флаг режима == 0 в обычной игре → множитель 1.35 не применяется
armor = clamp(lround(value), 0, 50)
```

### 3.3 Вес

```
weight_cur_x10 = Σ over carried (equipped + hot + main) count * weight_x10
base = (f32)(strength*20 + constitution*20 + level)   // strength/constitution after mods
maxv = stat_modifiers(base, stat_id=0x1e)
if skills[6] >= 5:  maxv *= 1.5      // double, как в оригинале
if person_flags & 0x2000: maxv *= 1.1  // this+0xf8 = record[42] (save +0xB4)
// global?0x8c == 0 в обычной игре
weight_max_x10 = clamp(lround(maxv), 1, 9999)
```

### 3.4 Очки действия (`FUN_1201ecd8`)

```
overload = (weight_cur_x10 - weight_max_x10) / 10   // integer div, <1 → 0
base = skills[26]*(2/3) + (dexterity + (perception>>1)) + (5.0f - overload)
if skills[6] > 14: base += 5.0f
action_points = lround(stat_modifiers(base, stat_id=0x1b))
action_points = (<6 ? 5 : >59 ? 60 : value)
```
`skill 26` = `HeroStats.skills[26]`; `perception>>1` — целочисленно.
`level = hero_level_from_experience(experience)` (уже есть).
`weight_x10`, `count` — из `InventoryItem`/`Item`.

### 3.5 Форматирование для UI

Значения веса — в десятых; в листе делятся на 10:
- `cur`: `1320 → "132"`;
- `max`: целое при кратности 10 (`1920 → "192"`), иначе один знак
  (`2046 → "204.6"`).
Строка: `"<cur>/<max>"`. Броня/AP/крит: целое.

### 3.6 Шанс критического удара (`FUN_1201e384`)

```
if skills[24] == 0: crit = 0
else:
    luck = attribute(raw_luck, stat_id=0xc)   // FUN_12020400
    base = (luck*0.334f + skills[24]*0.667f) * 0.5f
    crit = lround(stat_modifiers(base, stat_id=0x33))
    crit = (<1 ? 0 : >9 ? 10 : value)
```

### 3.7 Урон (6 строк, `FUN_1201f5e0/f2d0/f458/edfc/ef98/f134`)

Оружие = надетый предмет набора A (`flags & 0x001000`); нет — урон 0.
Физические строки (Дробящее `damage[0,1]` stat `0x22`; Рубящее
`damage[2,3]` stat `0x21`; Колющее `damage[4,5]` stat `0x23`):
```
a = 1 + strength*0.01
factor = (durability*0.005 + 0.5) * a
vb = weapon_skill(type)  // type1 -> skills[13], 0 -> [12], 2 -> [10], ...
min = lround(mods(statId)(damage[i]*factor + (damage[i]==0 ? 0 : vb)))
max = lround(mods(statId)(damage[i+1]*factor + (damage[i]==0 ? 0 : vb)))
```
Стихийные (Яд mod0/skill5; Холод mod1/skill4; Огонь mod2/skill0):
сумма модов оружия с `id==idx`, `+5` при skill>4; `sum==0 → 0`, иначе
`1/sum`. Формат: `min-max` или `0`.

### 3.8 Базовая точность (`FUN_1201e660`)

```
base = (perception*1.5 + 2*skills[24] + (luck>>1))*0.334 + 50
if weapon (set A):
    base = (unsigned(unknown_b[4]) + base) * 0.6
    skill bonus по типу оружия (см. research §16), +15 если skill>9
    statId = (type in 3/4/5) ? 0x38 : 0x32
else:
    if skills[14] > 9: base += 20; statId = 0x32
accuracy = clamp(lround(mods(statId)(base)), 0..90)   // строка UI "N%"
```
Скрытый 2-й аргумент геттера в call-site листа = 0.

### 3.9 Сопротивления и иммунитеты

Физические (UI 30..32): сумма по armor-слотам (`flags & 0x304000` —
armor/elbarms/helmet) их `damage[idx]*(durability*0.005+0.5)`, затем
`sum*mult + skills[6] + attr*0.334`, `mods(0x13)`, clamp 0..90:
(30,Дробящее)=constitution/idx0/mult0.334, (31,Колющее)=dexterity/idx4/
0.334, (32,Рубящее)=perception/idx2/0.333.

`wisdom = attribute(raw_wisdom, stat 0xb) + (skills[21]>4 ? 1 : 0)`;
`perception` — как выше. Для каждой строки:
```
magic[i]  = clamp(lround(mods(statId)(wisdom + skills[k] + skills[17])), 0..90)
immune[i] = clamp(lround(mods(statId)(perception + skills[k] + skills[17])), 0..90)
```
UI 33..38 (магия): (k,stat) = (4,0x11),(5,0x12),(0,0x0d),(1,0x0e),
(2,0x0f),(3,0x10). UI 43..48 (иммунитет): (4,0x2e),(5,0x2f),(0,0x2a),
(1,0x2b),(2,0x2c),(3,0x2d). Строки `"N%"`.

Стихийные сопротивления (UI 39..41): `mods(skills[k], statId)` +
`10` за каждый из `skills[17]/skills[1]/skills[20] > 4`, clamp 0..90:
(39,Огонь)=skills[22]/stat5, (40,Холод)=skills[25]/stat4,
(41,Яд)=skills[20]/stat3.

## 4. Изменения API

### 4.1 Новый SDL-free модуль `game`

`src/game/include/gl/game/combat_stats.hpp` (+ `.cpp`):

```cpp
namespace gl::game {
struct StatItemEffect { std::int32_t type; std::int32_t amount; std::uint32_t flags; };
struct EquippedStatItem {
    std::uint32_t type, flags, durability;
    const std::array<std::uint32_t,5>* unknown_b;
    const std::vector<gl::assets::ItemEffect>* effects;
};
struct CarriedItem { std::int32_t count; std::uint32_t weight_x10; };
struct StatInputs {
    std::int32_t strength, constitution, dexterity, experience;
    std::array<std::int32_t,27> skills;
    std::vector<EquippedStatItem> equipped;
    std::vector<CarriedItem> carried; // hot + main + equipped
};
struct ArmorWeight {
    std::int32_t armor_class;
    std::int32_t weight_cur_x10;
    std::int32_t weight_max_x10;
};
ArmorWeight compute_armor_weight(const StatInputs&);
std::string format_armor(std::int32_t);
std::string format_weight(std::int32_t cur_x10, std::int32_t max_x10);
}
```
`stat_modifiers` — приватная функция модуля (тестируется через
`compute_armor_weight` и/или отдельный `apply_stat_modifiers`).

### 4.2 `build_character_sheet`

Добавить перегрузку с контекстом (старую сохранить):

```cpp
CharacterSheet build_character_sheet(const PersonRuntime&,
    const std::function<std::string(std::uint32_t)>& ui_text,
    std::string_view display_name, std::string_view person,
    const ArmorWeight* armor_weight);
```
При `nullptr` строки 17/29/18/26 остаются `—` (`modeled=false`), как сейчас.
Иначе:
- строка 17 (`kCombatIds` index 8) = `format_armor(armor_class)`, modeled;
- строка 29 (index 9) = `format_weight(cur, max)`, modeled;
- строка 26 (index 7) = `format_armor(crit_chance)`, modeled;
- state строка 18 (index 5) = `format_armor(action_points)`, modeled.

### 4.3 Wiring `glgame`

В `open_character()` (tools/glgame/main.cpp):
1. Собрать `StatInputs` из `pending_hero_stats_` (или `PersonRuntime`
   параметров) + `inventory_runtime_.hero_inventory()` +
   `item_catalog_.find(item_id)->item`.
2. `compute_armor_weight(inputs)`.
3. Передать в `build_character_sheet(...)`.

Если `pending_hero_stats_` нет (нет сейва) — `nullptr` (строки `—`).

## 5. Проверка

### 5.1 Unit (`gl_game_tests`, новый файл)
- flat `0x40`, percent `0x80` (`amount*0.01*base`), override (max |value|,
  дельты игнорируются), смешанный набор.
- броня без щита; со щитом (unknown_b[3]/durability); clamp 0/50.
- вес: `*1.5` при `skills[6]>=5`; clamp 1/9999; `cur` — суммирование.
- атрибуты: модификаторы stat ids 6/7/8/9/12 применяются до формул.
- AP: `*1.5`-независимо; `skills[6]>14` +5; overload; clamp 5/60.
- крит: `skills[24]==0 → 0`; percent 0x33; clamp 0/10.
- формат: `1320→"132"`, `2112→"211.2"`, `2046→"204.6"`.

### 5.2 Real (`gl_game_tests` real-кейс, slot0, `GL_GAME_DIR`)
Оракул, полученный ручной проверкой оригинала+сейва (см. research §13–§14):
- `weight_cur_x10 == 1320` (per-container 8/1058/254);
- `weight_max_x10 == 2046` (con 25+7=32, capped 30; `(30·20+30·20+40)·1.5·1.1`);
- `armor_class == 15` (dex 18+2=20; щита набора A `0x400000` нет;
  ring 233 effect 0x1d +5; совпадает с `Класс брони 15` пользователя);
- `action_points == 43` (skills[26]=10, dex=20, per=13, sword 0x1b +5);
- `crit_chance == 7` (luck=9, skills[24]=15, 0x33 +8%);
- `accuracy == 90` (per=13, weapon unknown_b[4]=70, axe skill 13>9; clamp);
- магия-сопротивления `40,40,40,40,10,10`; иммунитеты
  `13,38,38,13,38,13`; стихийные `11,10,24`; физические `64,71,79`;
  (Дробящее 65 при con=32; скриншот 64 при con=30; Колющее/Рубящее
  совпали);
- урон (`slot0`, оружие набора A): Дробящее `76-96`, Колющее `0`,
  Рубящее `125-141`, Огонь `0`, Холод `1-30`, Яд `0` (совпало с
  оригинальным скриншотом);
- строки листа: `Класс брони = "15"`, `Вес = "132/204.6"`,
  `Очки действия = "43"`, `Шанс крита = "7%"`, `Точность = "90%"`,
  урон `min-max`/`0`.

Дополнительно: `character_overlay_tests` — значения строк 17/29
отрисовываются (golden/центрирование не меняется).

### 5.3 Команды
```powershell
$env:PATH="I:\goldenland2dec\.local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin;I:\goldenland2dec\.local\toolchains\ninja;I:\goldenland2dec\.local\toolchains\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
build\bin\gl_game_tests.exe --test-case="*armor*,*weight*,*character sheet*"
build\bin\glgame.exe --game-dir "I:\Games\Златогорье-2" --load-save 0 --screenshot "$env:TEMP\p5c2.png" --character
```

## 6. Критерии приёмки

- Unit + real с оракулом slot0 зелёные (armor 15, weight 132/204.6, AP 43,
  crit 7%, accuracy 90%, урон 6/6 и все сопротивления/иммунитеты как в
  оригинале).
- Полный `ctest` с `GL_GAME_DIR` — без регрессий.
- `—` для остальных combat/resistance/immunity сохранён.
- Manual visual gate (пользователь) — значения в листе совпадают с
  ожидаемым форматом.

## 7. Residual / Open Questions

- Условные модификаторы (time-of-day) — пропущены.
- Флаги `0x8c`, `this+0xf8&0x2000`, множители `1.35/1.1`, cheat-ветка
  крита `*5` — не применяются (обычная игра).
- Активный набор A/B (`local_player+0x6E4`, 0=A/1=B) не сейвится —
  runtime UI-состояние; порт всегда берёт **A** (weapon `0x001000`,
  shield `0x400000`). Для сейвов, где игра переключена на B, щит/оружие
  могут отличаться.
- Бонусные поля `this+0x16c..` и бонусные навыки `this+0x198` не
  моделируются.
- **Капы:** атрибуты клампятся до `ch_max_primary_value` (default 30),
  навыки — до `ch_max_skill_value` (default 15). Реализован кап атрибутов;
  отсюда con 32→30, вес `204.6`, Дробящее `64` — совпало с оригиналом.
- Точный вид `.0`/формат в листе — подтвердить скриншотом.
- Combat-лист полностью смоделирован по оригиналу и **совпал** (урон/
  armor/weight/AP/crit/accuracy/resistance 12/12/immunity 6/6). Остатки —
  не-боевые моды (`this+0x16c..`, `this+0x198`), режимные флаги.
