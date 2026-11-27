# P5f — Weapon/shield sets A/B: state, trigger, consumers

Дата: 2026-11-27. Статус: research done.

Источники: `Client.dll` SHA-256
`FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`
(`ImageBase = 0x12000000`), `GoldenLand.ini` (bindings), save `slot0.sav`,
ITM-корпус, `.local/decomp/client`.

## 1. Состояние и маски

- Активный набор: `DAT_12110f28+0x6E4` (0=A, 1=B). **Не сейвится**;
  ctor (`DAT_12110f28`) ставит 0 (`131450`); на загрузке/новой игре
  клиентский объект пересоздаётся → A.
- Setter `FUN_120c7008` (RVA `0xc7008`):
  `(this, set, force)`; при `force!=0 || set!=current` пишет `+0x6E4=set`
  и `+0x710=1` (dirty/refresh).
- Экипировочные маски (`item.flags & 0xFFF000`):

| mask | слот |
|---|---|
| `0x001000` | weapon A |
| `0x002000` | weapon B |
| `0x400000` | shield A (type 13) |
| `0x800000` | shield B (type 13) |
| `0x004000` | helmet (общий) |
| `0x100000` | armor (общий) |
| `0x200000` | elbarms/leggings (общий) |
| `0x008000` | amulet |
| `0x010000/0x020000` | rings A/B |
| `0x040000/0x080000` | bracelets A/B |

## 2. Триггеры

- **Клавиша Z**: `GoldenLand.ini` `bind DIK_Z, vk_weapon_toggle`
  (`vk_weapon_toggle` default scan code `0x36` — это DIK_Z).
  В клиенте toggle-код `0x10`: `iVar4 = (0x6E4+1); if (1<iVar4) iVar4=0;
  FUN_120c7008(...)` (декомп. `59436-59458`).
- **UI-кнопки**:
  - инвентарь: `FUN_12069e14` (RVA `0x69e14`), кнопки `0x54/0x55`
    (в `FUN_1205625c` case `0x27`), переключает `0x1D4`/`0x6E4`;
  - лист персонажа: `FUN_1207d0fc` (RVA `0x7d0fc`), состояние `+0x22C`.
- Инвентарный экран берёт активный набор в `screen+0x1D4` из
  `local_player+0x6E4` (`FUN_1203cfd0`; p5b1 §).
- Неактивная пара weapon/shield в инвентаре рисуется с prop
  `0x57=(0,0)` (p5b1), т.е. иконка неактивного набора невидима; слот/кнопка
  для переключения существует.

## 3. Потребители активного набора

- Wrapper `DAT_12110efc` vtable `PTR_FUN_12131340`: `+0x18(personId,set)`,
  weapon getters `+0x1c..+0x24` — `set = 0/1` или `-1` (активный);
  shield/ammo `0x400000/0x800000`, weapon слоты (p5c2 §8).
- Combat-статы/лист вызывают getters с `-1` → активный набор.
- Отрисовка куклы/оверлеев: `this+0x1D4` (0=A,1=B); неактивная пара
  (`0x1000/0x400000` при 1, `0x2000/0x800000` при 0) не рисуется
  (p5b2 §, `FUN_120683a8`).
- Герой на уровне: outfit resolver выбирает weapon/shield по активной
  паре; смена набора должна сразу переприменить outfit.

## 4. Slot0 oracle

Экипировка slot0: weapon A 486 (axe, type1), weapon B 554 (sword,
type0), shield A отсутствует, shield B 353 (type13, `unknown_b[3]=20`,
durability 100). Armor/helmet/elbarms общие.

- A: `armor=15` (P5c2, без fallback на B-щит); weapon 486 damage
  `[47,62,72,83,0,0]`.
- B: `armor=35` (=15+20 щит B); weapon 554 damage `[0,0,14,22,12,26]`
  → row0=0, row1 (dmg[4,5])=pierce>0, row2 (dmg[2,3])=slash>0;
  мод `{8,−2}` применяется к обоим наборам (контейнер модификаторов из всех надетых предметов, без фильтра набора), поэтому primary perception = 13 и в A, и в B.
- Вес не меняется (все три контейнера считаются).

## 5. Решение порта (D-117)

- Состояние `active_weapon_set` (0/1), default A, не сейвится;
  сброс в A при load/new game.
- Триггеры: **Z** и клик по **неактивному** weapon/shield слоту в
  инвентаре/листе. Оба слота (A/B) существуют как click-target;
  иконка неактивной пары не рисуется (parity с prop `0x57`).
- Combat-статы и outfit используют **только активный** набор, без
  fallback (slot0 A armor=15).
- Неактивный слот показывает тот же прямоугольник, но без иконки;
  клик переключает набор. Активная пара подсвечивается рамкой (port UX).
- Сохранение активного набора в сейв — вне scope.
