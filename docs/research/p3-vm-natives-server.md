# Research: Server.dll CS VM and native dispatch

Дата: 2026-11-05. Задача: Phase 1a, Task 2 (research only). Источник истины —
`Server.dll` v1.3.

## 1. Provenance

- Binary: `I:\Games\Златогорье-2\Server.dll`
- SHA-256: `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`
- PE32, image base `0x14000000`; RVA в тексте = VA − `0x14000000`.
- Ghidra `12.1.4_PUBLIC` (JDK `21.0.12.1+1`), headless export.
- Disassembler: `llvm-objdump.exe` из `llvm-mingw-20261006-ucrt-x86_64`
  (`-d -M intel`), полный дамп `.text`; использован для разрешения мест, где
  псевдокод Ghidra теряет x87-операции.
- Корпус (gitignored):
  - `.local/decomp/server/decompiled.c` (1108 функций, 1107 декомпилировано)
  - `.local/decomp/server/functions.csv`
  - `.local/decomp/server/natives/natives.csv` (65 строк)
  - `.local/decomp/server/natives/natives.c` (65 обработчиков)
- Байты данных читались напрямую из PE по `RVA → file offset`
  (`.data1`: RVA `0x8C000`, raw `0x80000`).

Ключевые функции:

| Функция | RVA | Роль |
|---|---|---|
| `FUN_1403e020` | `0x3e020` | парсер бинарного массива CS-узлов |
| `FUN_1403a010` | `0x3a010` | evaluator выражения/узла (double/ST0) |
| `FUN_1403a914` | `0x3a914` | executor (аргументы функций, часть путей) |
| `FUN_1403d944` | `0x3d944` | верхнеуровневый цикл исполнения |
| `FUN_14038fa8` | `0x38fa8` | раскладка 9 аргументов в глобальный блок |
| `FUN_14038c6c` | `0x38c6c` | проверка типов аргументов натива |
| `FUN_14039888` | `0x39888` | диспетчеризация натива по ID |
| `FUN_14042464` | `0x42464` | регистрация 65 нативов |
| `FUN_14004f14` | `0x4f14` | поиск обработчика в реестре |

## 2. Node layout (`FUN_1403e020`) vs `CsNode`

Парсер делает два прохода. Первый (RVA `0x3e020`) считает узлы и длины записей,
второй (в том же `FUN_1403e020`) заполняет объекты узлов; затем `-1`-индексы
c/d/args заменяются указателями на узлы (`-1 → 0`). Размер объекта узла —
`0x50` байт (`FUN_140057cc(0x50,...)` в `FUN_1403e020`).

Поля узла в памяти Server (для второго прохода):

| Смещение | Поле | Источник в файле |
|---|---|---|
| `+0x08` | `d` (false-ветка / next) | 4-й `i32` записи op `0..20`; 2-й `i32` у `jmp` |
| `+0x0c` | `c` (true-ветка / next) | 3-й `i32` записи op `0..20`; 1-й `i32` у `jmp` |
| `+0x10` | `a` | 1-й `i32` |
| `+0x14` | `b` | 2-й `i32` |
| `+0x18..0x38` | `args[9]` | индексы узлов-аргументов `func` |
| `+0x3c` | `opcode` | `i32` |
| `+0x40` | `value` (`f64`) | литерал |
| `+0x48` | `text` (указатель) | C-строка |

Файловая раскладка (совпадает с `gl::assets::parse_nodes`,
`src/assets/src/cs.cpp:43-89`):

| opcode | Размер записи | Поля файла |
|---|---|---|
| `0..20`, `50` | `20` байт | `opcode,a,b,c,d` (5×`i32`) |
| `21`, `24` | `12` байт | `opcode` + `f64 value` |
| `22`, `23` | `5 + len` | `opcode` + NUL-terminated cp1251-строка |
| `48` | `24 + 4·k` | `opcode,c,d,f64 value`, затем `args[0..]` до `-1` |
| `49` | `12` байт | `opcode,c,d` |

Свидетельства: `FUN_1403e020` (case-блоки первого прохода для op
`0..0x14`, `0x15/0x18`, `0x16/0x17`, `0x30`, `0x31`, `0x32`; второй проход
`node+0x3c = opcode`, `node+0x10 = a`, `node+0x14 = b`, `node+0x0c = c`,
`node+0x08 = d`, `node+0x40 = value`, `node+0x48 = text`). Для `48` аргументы
начинаются с файлового смещения `+0x14`; минимум — 1 слот `-1` (размер `0x18`).

Отличия от `gl::assets::CsNode`:

- `CsNode` — обычная C++-структура (индексы, `std::string`, `double`); Server
  держит узлы как 32-битные объекты с указателями и разрешает c/d/args в
  указатели сразу при парсинге. Семантика полей совпадает.
- Server-резолвер аргументов `func` считает терминатором **`-1` или `0`**
  (`decompiled.c` второй проход, `if (iVar5 == -1 || iVar5 == 0)`); порт
  использует только `-1` (`src/assets/src/cs.cpp:69-79`). В 555 реальных `.cs`
  терминолатор всегда `-1`, индекс `0` среди аргументов не встречается
  (`funcs=35356`, проверено скриптом).
- Первый проход Server для неизвестных opcode (`0x19..0x2f`) не имеет case:
  запись трактуется как 4 байта. Валидными такие opcode не считаются; порт
  отвергает их (`cs_opcode_valid`, `src/assets/src/cs.cpp:162`).

## 3. Opcode semantics `0..0x32`

Общая модель верхнего уровня (`FUN_1403d944`, RVA `0x3d944`):

1. `node = *(program+0xb4)`.
2. `r = FUN_1403a010(vm, node)` (результат — `double`/ST0).
3. Если `r == 0.0` (`_DAT_14090858` = `0.0`), следующий узел — `node->d`
   (`+0x08`); иначе — `node->c` (`+0x0c`).
4. Если следующий указатель `0`, исполнение завершается. Цикл также
   останавливается, когда `DAT_14088dd8 != 0` (это выставляет `Exit`).

Таким образом, `c` — true-ветка, `d` — false/next. Для не-условных узлов
компилятор ставит `c == d` либо `-1/-1`.

Роли evaluator/executor:

- `FUN_1403a010` — evaluator, используется верхним циклом и рекурсивно для
  операндов. Здесь `0x00`/`0x02` **сокращают вычисления** (см. ниже).
- `FUN_1403a914` — executor, используется `FUN_14038fa8` при подготовке
  аргументов `func` (и частью парсера выражений `FUN_1403b2dc`). Здесь
  `0x00`/`0x02` вычисляют **оба** операнда; `0x17`/`0x31`/`0x32` имеют
  отдельные ветки. На верхнем уровне скрипта executor не используется.

Ниже — семантика evaluator (если не указано иное). Приведение к `int32`
везде выполняется x87 `fistp` с CW, выставленным в truncate
(`or eax,0xc00`, затем `fldcw`), т.е. усечение к нулю, затем `fild` обратно в
double.

### 3.1 Логические и битовые

| op | RVA case | Семантика | Результат |
|---|---|---|---|
| `0x00` `\|\|` | `0x3a7cb` | eval `a`; если `a != 0` → `1`, иначе eval `b` → `(b != 0)` | double `0/1`; short-circuit по `a` |
| `0x01` `^^` | `0x3a769` | eval `a`, eval `b`; `(a!=0) XOR (b!=0)` | double `0/1` (`_DAT_14090558` = `1.0`) |
| `0x02` `&&` | `0x3a71e` | eval `a`; если `a == 0` → `0`, иначе eval `b` → `(b != 0)` | double `0/1`; short-circuit по `a` |
| `0x03` `\|` | `0x3a6b0` | `int32(a) \| int32(b)` (truncate) | double от `fild` int32 |
| `0x04` `^` | `0x3a642` | `int32(a) ^ int32(b)` | double |
| `0x05` `&` | `0x3a5d4` | `int32(a) & int32(b)` | double |
| `0x0c` `<<` | `0x3a3d8` | `int32(a) << (cl & 31)` (`shl edx,cl`) | double |
| `0x0d` `>>` | `0x3a368` | `int32(a) >> (cl & 31)` (`sar edx,cl`, арифметический) | double |
| `0x13` `~` | `0x3a1e2` | операнд — **`b`**: `~int32(b)` | double |
| `0x14` `!` | `0x3a1ae` | операнд — **`b`**: `int32(b) == 0 ? 1 : 0` | double |

### 3.2 Сравнения `0x06..0x0b`

`0x06` (`0x3a592`) `!=`, `0x07` (`0x3a550`) `==`, `0x08` (`0x3a50e`) `>=`,
`0x09` (`0x3a4cc`) `<=`, `0x0a` (`0x3a48a`) `>`, `0x0b` (`0x3a448`) `<`.
Все — сравнение двух double (`fcompp` + `fnstsw` + x87 flags), результат
double `1.0`/`0.0`. Строки в evaluator не имеют отдельного типа (см. 3.5).

### 3.3 Арифметика `0x0e..0x12`

| op | RVA case | Семантика |
|---|---|---|
| `0x0e` `+` | `0x3a33e` | eval `a`, eval `b`, `faddp` → `a + b` (double) |
| `0x0f` `-` | `0x3a314` | eval `a`, eval `b`, `fsubrp` → `a - b` (double) |
| `0x10` `*` | `0x3a2ea` | eval `a`, eval `b`, `fmulp` → `a * b` (double) |
| `0x11` `/` | `0x3a2a1` | eval `b`; если `b == 0.0` → `_DAT_14090550` = `1e10`; иначе eval `a`, `fdiv` → `a / b` (double) |
| `0x12` `%` | `0x3a222` | eval `b`; `ib = trunc(b)`; если `ib == 0` → `1e10`; иначе eval `a`, `idiv`, вернуть **остаток** `trunc(a) % ib` |

Важно: `1e10` = `10000000000.0` = `0x4202A05F20000000`. Это не ошибка и не
`0.0`: деление/модуль на ноль возвращают truthy-значение.

Порядок вычисления операндов: `+ - *` — `a`, затем `b`; `/` и `%` — `b`,
затем `a`.

### 3.4 Константы/переменные `0x15..0x18`

| op | RVA case | Семантика evaluator |
|---|---|---|
| `0x15` `num_var` | `0x3a19f` | `fld node->value` (`+0x40`) |
| `0x18` `num` | `0x3a19f` | `fld node->value` (`+0x40`) |
| `0x16` `str` | `0x3a18d` | `0.0` (строковый литерал в числовом evaluator = 0) |
| `0x17` `str_var` | `0x3a173` | `variable_host->get_numeric(text)` (vtable `+0xc`), double |

Executor `0x17` (`0x3aadd`) сначала спрашивает тип переменной (vtable `+0x14`);
если тип `3` (string), идёт строковая ветка (`0x3b289`), иначе возвращает
числовое значение.

### 3.5 `0x30` `func`

Evaluator (`0x3a153`) и executor (`0x3aabd`) вызывают
`FUN_14039888(node->value, node)` и возвращают результат обработчика (double).
Перед вызовом `FUN_14038fa8(node->value, node)` раскладывает до 9 аргументов в
глобальный блок:

- тип аргумента: `DAT_14089744 + i*8`, значение: `DAT_14089740 + i*8`;
- пустой аргумент (указатель `0`) → тип `-1`;
- узел `0x16` (`str`) → тип `3`, значение = `node->text`;
- узел `0x17` (`str_var`) → тип переменной; если `3` — тип `3` и строковый
  указатель, иначе числовой тип/значение;
- узел `0x18` (`num`) → тип `0` (int), значение = `trunc(exec(node))`;
- всё остальное (`0x15 num_var`, арифметика, вложенный `func`, сравнения) →
  тип `2` (float), значение = `float(exec(node))`.

`FUN_14039888` округляет `node->value` до int (`ROUND`), ищет ID в реестре
(`FUN_14004f14`); при отсутствии возвращает `_DAT_14090530` = `0.0`.

### 3.6 `0x31` `jmp`

Evaluator (`0x3a141`) возвращает `0.0`. Поэтому верхний цикл всегда берёт
`d` (`+0x08`). Executor (`0x3aa45`) проверяет, что `d` входит в список
допустимых целей переходов (`vm+0x418`, массив `+0x1c0`, счётчик `+0xc0`),
и тоже возвращает `0.0`.

В 555 реальных `.cs`: 85167 узлов `jmp`, у **всех** `c == d`; из них 71628
указывают на `index+1`, 555 — терминальные `c=d=-1`.

### 3.7 `0x32` `assign`

Evaluator (`0x3a036`) и executor (`0x3a93a`):

1. Берёт `dest = node[a]` (тип `23 str_var`) и `src = node[b]`.
2. Спрашивает тип приёмника (vtable `+0x14`).
3. `type 0` (int): eval `src`, `trunc` → `set_int(dest, value)`; возвращает
   исходное double.
4. `type 1` (DWORD): eval `src`, `trunc` с поправкой на unsigned
   (`DAT_14090668`/`DAT_14090760`), `set_int`; возвращает double.
5. `type 2` (float): eval `src`, `set_float`; возвращает double.
6. `type 3` (string): если `src` — `0x16`, копирует строку; если `0x17` и
   тип источника `3` — копирует строку; иначе **ничего не пишет** и
   возвращает `0.0`.

Возвращённое значение затем используется верхним циклом для выбора `c`/`d`.
В 555 реальных `.cs`: 39051 `assign`, у **всех** `c == d` (обычно не
`index+1`, а следующая управляющая вершина). Все приёмники — `str_var`; в
`zlato_vars.scr` 781 `int`, 2 `float`, 2 `DWORD`, 0 `string`.

### 3.8 Прочие opcode

- `0x19..0x2f`: не имеют case в парсере; evaluator (`0x3a7fd`) возвращает
  `0.0`. Валидными в `.cs` не считаются.
- `0x15`/`0x18` используют только `value`, без c/d.

## 4. Delta table vs current port

Легенда verdict: `match` — поведение совпадает; `port-fix` — доказанное
расхождение, кандидат на тест/исправление в Task 5; `unverified` — из корпуса
не удаётся доказать (не менять код).

| opcode | Server.dll behavior (RVA) | port behavior (file:line) | verdict |
|---|---|---|---|
| `0x00 \|\|` | eval `a`; если `a != 0` — `1` без eval `b` (`0x3a7cb`, `je 0x3a8eb`) | eval обоих операндов (`src/script/src/script_vm.cpp:396-418`, вызов из `237-243`) | `port-fix` |
| `0x02 &&` | eval `a`; если `a == 0` — `0` без eval `b` (`0x3a71e`, `je 0x3a753`) | eval обоих операндов (`script_vm.cpp:396-418`) | `port-fix` |
| `0x11 /` | всегда double `a/b` (`0x3a2c8`); `b==0` → `1e10` (`0x3a2d8`, `_DAT_14090550`) | `int/int` → int32-деление (`script_vm.cpp:511-522`); ноль → error + `0` (`473-477`, `495-498`, `519-521`) | `port-fix` |
| `0x12 %` | `trunc(a) % trunc(b)` (`idiv`, `0x3a279`); `b==0` → `1e10` (`0x3a28f`) | double-операнды → error (`478-480`); ноль → error (`501-504`, `524-527`) | `port-fix` |
| `0x0c <<` | `int32(a) << (cl&31)` (`shl`, `0x3a432`) | `uint32` сдвиг; count вне `0..31` → error (`554-562`) | `port-fix` |
| `0x0d >>` | `int32(a) >> (cl&31)` арифметический (`sar`, `0x3a3c2`) | `uint32` логический `>>` (`560-561`); count вне `0..31` → error | `port-fix` |
| `0x13 ~` | операнд `b` (+0x14), `~int32(b)` (`0x3a1e2`) | операнд `a` (`script_vm.cpp:384-387`), `apply_unary` (`572-582`) | `port-fix` |
| `0x14 !` | операнд `b` (+0x14), `b==0` (`0x3a1ae`) | операнд `a` (`384-387`), `is_true` (`89-100`, `583`) | `port-fix` |
| `0x0e/0x0f/0x10` | результат всегда double (`faddp 0x3a35a`, `fsubrp 0x3a330`, `fmulp 0x3a306`) | для int-операндов результат `int32` (`513-516`) | `match` в диапазоне; overflow/DWORD `unverified` |
| `0x03/0x04/0x05` | int32 bit-op над `trunc` double, результат `fild` (signed) (`0x3a6b0/0x3a642/0x3a5d4`) | `uint32`, если операнд DWORD; clamp (`541-569`) | `match` в диапазоне; DWORD/overflow `unverified`; в данных opcode нет |
| `0x06..0x0b` | сравнение double, `1.0/0.0` (`0x3a448..0x3a592`) | типизированное сравнение + строковые правила (`624-650`) | `match` для numeric int/float; строки не встречаются |
| `0x01 ^^` | eval обоих, `(a!=0) XOR (b!=0)` (`0x3a769`) | то же (`401-417`) | `match` (opcode в данных нет) |
| `0x30 func` | аргументы типизируются по opcode (`FUN_14038fa8`); `FUN_14039888`; возврат double (`0x3a153`) | eval args как `Value`; VM-internal D_Say/D_Answer/D_CloseDialog берут первый arg через `to_uint32`; остальные → host (`424-456`) | VM-internal `match` на реальных данных (все dialog-args — `0x18 num` → type 0); native dispatch `port-fix` в Task 4 |
| `0x31 jmp` | возвращает `0.0` → цикл берёт `d` (`0x3a141`); executor проверяет `d` (`0x3aa45`) | берёт `c` (`script_vm.cpp:246-252`) | `match` (85167/85167 `c==d`); структурное различие |
| `0x32 assign` | возвращает значение; цикл берёт `c` при `!=0`, иначе `d` (`0x3a036`) | всегда `next = node.d` (`script_vm.cpp:285`) | `match` (39051/39051 `c==d`) |
| `0x32` string dest | numeric src → приёмник не меняется, возврат `0.0` (`0x3a09f-0x3a0b7`) | число → строка (`script_vm.cpp:608-620`) | `unverified`; string-переменных в данных нет |
| top-level `21..24` | цикл использует c/d узла (по умолчанию `0`) → завершение | no-op fallthrough (`script_vm.cpp:301-311`) | `match` по D-023 (задокументированное отклонение) |
| `0x15/0x18` value | `fld node->value` (`0x3a19f`) | `node.value` (`348-350`) | `match` |
| `0x16 str` | `0.0` в числовом контексте (`0x3a18d`); как arg — type 3 (`FUN_14038fa8`) | string (`351-352`) | `unverified` в общем контексте; `match` как аргумент |
| `0x17 str_var` | numeric через variable host (`0x3a173`) | `Value` переменной (`353-354`) | `unverified` для string-переменных |

Доказательство short-circuit на реальных данных: 1800 top-level логических
узлов (`0x00` = 148, `0x02` = 2100 всего, из них верхнеуровневых 1800);
8 из них имеют вызов `func` в поддереве операнда, т.е. разница в
short-circuit наблюдаема на поставляемых скриптах.

## 5. Native table (65/65)

Обозначения arg types: `0` = int, `1` = DWORD, `2` = float, `3` = string;
`0` и `1` взаимозаменяемы в `FUN_14038c6c`. `arity` — число ожидаемых типов;
`FUN_14038c6c` проверяет первые `arity` аргументов и **игнорирует лишние**.
`[]` — `FUN_14038c6c(-1)`, аргументов нет. Все обработчики возвращают double;
fallback-константа при несовпадении типов — `0.0`, кроме отмеченных.

| ID | name | RVA | arity | arg types | return | side effects | support |
|---|---|---|---|---|---|---|---|
| 16777216 | Exit | `0x3fd38` | 1 | `0` | 0.0 | при arg0 != 0 пишет `DAT_14088dd8=1` (останов VM) | supported |
| 16777217 | Signal | `0x3fd6c` | 1 | `3` | 0.0 | `MessageBoxA(0,arg0,"Script: Signal",0)` | supported |
| 16777218 | Console | `0x3fda4` | 1 | `3` | 0.0 | вывод через vtable `DAT_140891bc+0xcc` | supported |
| 16777219 | Cmd | `0x3fdf4` | 1 | `3` | 0.0 | `DAT_14088e2c->vtable+0x10(arg0,1)` (консольная команда) | supported |
| 16777220 | D_Say | `0x3fc10` | 1 | `1` | 0.0 | `FUN_14043d44(DAT_14088de8,arg0)` — добавить фразу | vm_internal |
| 16777221 | D_CloseDialog | `0x3fc60` | 1 | `1` | 0.0 | `DAT_14088de8+0x5528 = 1` — закрыть диалог | vm_internal |
| 16777222 | D_Answer | `0x3fc9c` | 1 | `1` | 0.0 | `FUN_14043cf8(DAT_14088de8,arg0)` — добавить ответ | vm_internal |
| 16777223 | D_PlaySound | `0x3fcec` | 1 | `3` | 0.0 | `FUN_1404345c(DAT_14088de8,arg0)` — звук | supported |
| 33554432 | LE_CastEffect | `0x3fe2c` | 4 | `3,3,0,0` | 0.0 | `FUN_140347e4(DAT_14088dc4,arg0,arg1,arg2)` | unsupported: effects |
| 33554433 | LE_DelEffect | `0x3fe88` | 1 | `3` | 0.0 | `FUN_140348dc(DAT_14088dc4,arg0)` | unsupported: effects |
| 33554434 | LE_CastMagic | `0x3feb8` | 3 | `3,0,0` | 0.0 | `FUN_14034228(level,arg0,0,0,0,arg1,arg2,0,0)` | unsupported: magic/effects |
| 50331648 | WD_LoadArea | `0x3ff1c` | 2 | `3,3` | 0.0 | копирует arg1 в `level+0x5f54`; `FUN_1400607c("map %s")` + console | unsupported: world/level |
| 50331649 | WD_SetCellsGroupFlag | `0x43008` | 3 | `3,3,0` | 0.0 | нет (только проверка типов; тело пустое) | unsupported: world/level (no-op) |
| 50331650 | RS_SetTribesRelation | `0x40078` | 3 | `3,3,3` | 0.0 | `FUN_140162e4` ×2, `FUN_1401fdd4`, `FUN_140162b0` — отношение племён | unsupported: factions |
| 50331651 | RS_GetTribesRelation | `0x400e4` | 2 | `3,3` | computed int | `FUN_14016998` — отношение племён | unsupported: factions |
| 50331652 | RS_StartDialog | `0x41ff0` | 2 | `3,3` | 0.0 | поиск персон; `FUN_140441d8(dialog,...)` | unsupported: dialogue engine |
| 50331653 | WD_SetVisible | `0x423e8` | 2 | `3,0` | 0.0 | `FUN_1400607c("set_anim_object_visibility %s %d")` + console | unsupported: world/level |
| 50331654 | C_FINISHED | `0x3fb8c` | 1 | `0` | 0.0 | `FUN_1400607c("ch_finish_game %d")` + console | unsupported: scenario flow |
| 50331655 | WD_TitlesAndLoadArea | `0x40034` | 3 | `0,3,3` | 0.0 | `FUN_14025a78(level,arg0,arg1,arg2)` | unsupported: world/level |
| 50331656 | C_TitlesAndFINISHED | `0x3fbd8` | 2 | `0,0` | 0.0 | `FUN_14025b8c(level)` | unsupported: scenario flow |
| 67108864 | RS_GetPersonParameterI | `0x404f0` | 2 | `3,3` | computed double | поиск персоны + параметр по имени (`EXPERIENCE`, `STRENGTH`, …) | unsupported: persons |
| 67108865 | RS_SetPersonParameterI | `0x409d4` | 3 | `3,3,0` | 0.0 | поиск персоны; `FUN_14019674`, `FUN_1400aa18`, `FUN_1400b824`; пишет `+0x1500` | unsupported: persons |
| 67108866 | RS_AddPerson_1 | `0x41814` | 5 | `3,3,0,0,0` | 0.0 | заполняет шаблон `DAT_140893c0`; `FUN_1401f9bc`; `_strncpy` | unsupported: persons |
| 67108867 | RS_AddPerson_2 | `0x41900` | 8 | `3,0,0,3,3,3,3,3` | 0.0 | заполняет шаблон; `FUN_1401fb1c`; `_strncpy` ×2 | unsupported: persons |
| 67108868 | RS_IsPersonExistsI | `0x41cb4` | 2 | `3,3` | computed int | `FUN_14036b58(level,arg0,arg1)` | unsupported: persons |
| 67108869 | RS_AddExp | `0x40d04` | 1 | `0` | 0.0 | текущая персона `FUN_14027f7c`; `FUN_140529fc(player,id,arg0)` | unsupported: persons |
| 67108870 | RS_DelPerson | `0x41cf8` | 1 | `3` | 0.0 | `FUN_14027cf0(level,arg0)` | unsupported: persons |
| 67108871 | RS_AddToHeroPartyName | `0x41d28` | 1 | `3` | -1.0 или int | `FUN_1402914c`, `FUN_14016fcc` — добавить в партию | unsupported: party |
| 67108872 | RS_RemoveFromHeroPartyName | `0x41e24` | 1 | `3` | -1.0 или int | `FUN_1402914c`, `FUN_14017134` — удалить из партии | unsupported: party |
| 67108873 | RS_TestHeroHasPartyName | `0x41f20` | 1 | `3` | 0.0/1.0 | `FUN_14016c88` — проверка партии | unsupported: party |
| 67108874 | RS_AllyCmd | `0x42308` | 2 | `3,3` | 0.0 | `FUN_14016c88`, `FUN_140421a8` | unsupported: party/AI |
| 67108875 | RS_ShowMessage | `0x42434` | 1 | `0` | 0.0 | `FUN_14025c48(level,arg0)` — сообщение | unsupported: UI/dialog |
| 67108876 | RS_GetPersonSkillI | `0x40944` | 2 | `3,3` | computed double | `FUN_1402914c`, `FUN_1400cb64` — навык персоны | unsupported: persons/skills |
| 83886080 | RS_TestPersonHasItem | `0x40144` | 2 | `3,3` | 0.0/1.0 | поиск персоны/предмета; `FUN_1404c574` | unsupported: inventory |
| 83886081 | RS_PersonTransferItemI | `0x401dc` | 4 | `3,3,3,0` | computed double | `FUN_14048790` — передача предмета | unsupported: inventory |
| 83886082 | RS_GetItemCountI | `0x40d40` | 2 | `3,3` | computed int | `FUN_1404c574` — количество предмета | unsupported: inventory |
| 83886083 | RS_PersonTransferAllItemsI | `0x40fb0` | 2 | `3,3` | 0.0 | `FUN_14048518` — передать все предметы | unsupported: inventory |
| 83886084 | RS_PersonAddItem | `0x41320` | 3 | `3,3,0` | 0.0 | `FUN_1404cd28` — добавить предмет; пишет `+0x18` | unsupported: inventory |
| 83886085 | RS_PersonRemoveItem | `0x41478` | 3 | `3,3,0` | 0.0 | `FUN_1404cae8` — удалить предмет; пишет `+0x18` | unsupported: inventory |
| 83886086 | RS_PersonAddItemToTrade | `0x412a4` | 3 | `3,3,0` | 0.0 | `FUN_1404cd28` на trade-контейнере; пишет `+0x18` | unsupported: trade |
| 83886087 | RS_PersonRemoveItemToTrade | `0x41400` | 3 | `3,3,0` | 0.0 | `FUN_1404cae8` на trade-контейнере; пишет `+0x18` | unsupported: trade |
| 83886088 | RS_GetMoney | `0x41120` | 0 | `[]` | computed int | текущая персона; `FUN_1404c494` | unsupported: inventory/persons |
| 100663296 | RS_GetDayOrNight | `0x40d98` | 0 | `[]` | computed double | time-объект `DAT_14088e14`, vtable `+0xc` | unsupported: time |
| 100663297 | RS_GetCurrentTimeOfDayI | `0x40db8` | 0 | `[]` | computed int | time vtable `+0x18` | unsupported: time |
| 100663298 | RS_GetDaysFromBeginningI | `0x40de8` | 0 | `[]` | computed int | `FUN_1405b140` | unsupported: time |
| 100663299 | RS_AddTime | `0x40f34` | 2 | `0,0` | 0.0 | `FUN_1405b504(time,args)` | unsupported: time |
| 117440512 | RS_QuestComplete | `0x403dc` | 1 | `3` | 0.0 | `FUN_140591b8(quest,arg0)` | unsupported: quests |
| 117440513 | RS_StageEnable | `0x4040c` | 2 | `3,3` | 0.0 | `FUN_14059030(quest,arg0,arg1)` | unsupported: quests |
| 117440514 | RS_QuestEnable | `0x40448` | 2 | `3,3` | 0.0 | `FUN_14059f6c` | unsupported: quests |
| 117440515 | RS_StageComplete | `0x404b4` | 2 | `3,3` | 0.0 | `FUN_14058de0` | unsupported: quests |
| 117440516 | RS_StorylineQuestEnable | `0x40484` | 1 | `3` | 0.0 | `FUN_1405a21c` | unsupported: quests |
| 117440517 | RS_SetEvent | `0x40e2c` | 2 | `0,0` | computed double | `FUN_1405b9b4(time)` — установить событие | unsupported: events/time |
| 117440518 | RS_GetEvent | `0x40ec8` | 1 | `0` | computed int | `FUN_1405b80c(time,arg0,0)` | unsupported: events |
| 117440519 | RS_ClearEvent | `0x40f04` | 1 | `0` | 0.0 | `FUN_1405b6c4(time,arg0)` | unsupported: events |
| 117440520 | RS_SetLocationAccess | `0x41634` | 2 | `3,0` | 0.0 | копирует arg0, добавляет `"_state"`, set через vtable `DAT_1407f9a0+0x1c` | unsupported: world state |
| 117440521 | RS_EnableTrigger | `0x416e8` | 2 | `3,0` | 0.0 | `FUN_14043030`, поиск объекта/триггера, запись флагов | unsupported: triggers |
| 117440522 | RS_GetRandMinMaxI | `0x41bf8` | 2 | `0,0` | computed int | `FUN_140061e4(min,max)` | unsupported: RNG |
| 117440523 | RS_SetWeather | `0x41204` | 1 | `0` | 0.0 | `FUN_1405c1fc(weather,id)` | unsupported: weather |
| 117440524 | RS_SetSpecialPerk | `0x41264` | 1 | `0` | 0.0 | текущая персона; `FUN_14009b38` | unsupported: perks |
| 117440525 | RS_PassToTradePanel | `0x41558` | 0 | `[]` | 0.0 | `FUN_14028404`, `DAT_14088de8+0x5528=1` | unsupported: trade/UI |
| 117440526 | RS_GetDialogEnabled | `0x410b4` | 1 | `0` | computed double | `FUN_14009f08`, `FUN_14006288` | unsupported: dialog/persons |
| 117440527 | RS_SetUndeadState | `0x415e0` | 2 | `3,0` | 0.0 | поиск персоны; пишет `+0xf58` | unsupported: persons |
| 117440528 | RS_GlobalMap | `0x415a0` | 0 | `[]` | 0.0 | `FUN_140286b0`; `DAT_14088de8+0x5528=1` | unsupported: UI/global map |
| 117440531 | RS_SetInjured | `0x41c34` | 2 | `3,0` | 0.0 | `FUN_1402914c`, `FUN_14018848` | unsupported: persons |
| 117440532 | RS_SetDoorState | `0x41c74` | 2 | `3,0` | 0.0 | `FUN_14029388`, `FUN_14024228` | unsupported: world/doors |

Проверка: 65 строк, ID уникальны и совпадают с `natives.csv`; для всех
`supported`/`vm_internal` есть тело в `natives.c`.

### 5.1 Использование нативов в 555 поставляемых `.cs`

Только эти ID встречаются в `scripts.pak` (вызовов `func` с данным ID):

| ID | name | calls |
|---|---|---|
| 16777222 | D_Answer | 13730 |
| 16777220 | D_Say | 8068 |
| 16777221 | D_CloseDialog | 6119 |
| 16777223 | D_PlaySound | 1154 |
| 117440522 | RS_GetRandMinMaxI | 893 |
| 117440513 | RS_StageEnable | 845 |
| 117440515 | RS_StageComplete | 517 |
| 67108864 | RS_GetPersonParameterI | 470 |
| 67108869 | RS_AddExp | 334 |
| 83886080 | RS_TestPersonHasItem | 320 |
| 83886082 | RS_GetItemCountI | 312 |
| 83886081 | RS_PersonTransferItemI | 284 |
| 83886084 | RS_PersonAddItem | 281 |
| 83886085 | RS_PersonRemoveItem | 242 |
| 67108865 | RS_SetPersonParameterI | 236 |
| 117440512 | RS_QuestComplete | 221 |
| 50331650 | RS_SetTribesRelation | 220 |
| 117440514 | RS_QuestEnable | 141 |
| 67108868 | RS_IsPersonExistsI | 138 |
| 117440525 | RS_PassToTradePanel | 129 |
| 67108870 | RS_DelPerson | 112 |
| 117440526 | RS_GetDialogEnabled | 100 |
| 117440520 | RS_SetLocationAccess | 68 |
| 67108871 | RS_AddToHeroPartyName | 54 |
| 117440516 | RS_StorylineQuestEnable | 53 |
| 100663298 | RS_GetDaysFromBeginningI | 49 |
| 50331648 | WD_LoadArea | 30 |
| 117440524 | RS_SetSpecialPerk | 27 |
| 67108873 | RS_TestHeroHasPartyName | 20 |
| 100663299 | RS_AddTime | 20 |
| 50331653 | WD_SetVisible | 16 |
| 67108872 | RS_RemoveFromHeroPartyName | 15 |
| 67108866 | RS_AddPerson_1 | 15 |
| 67108867 | RS_AddPerson_2 | 15 |
| 67108874 | RS_AllyCmd | 11 |
| 117440523 | RS_SetWeather | 10 |
| 117440531 | RS_SetInjured | 8 |
| 33554434 | LE_CastMagic | 7 |
| 117440521 | RS_EnableTrigger | 7 |
| 117440527 | RS_SetUndeadState | 7 |
| 100663296 | RS_GetDayOrNight | 6 |
| 117440519 | RS_ClearEvent | 1 |

ID `0` встречается 51 раз и отсутствует в реестре: `FUN_14039888` вернёт
`0.0`. Остальные 22 ID из `natives.csv` в поставляемых скриптах не
вызываются.

Аргументные сигнатуры нативов на реальных данных совпадают с типами
обработчиков; исключения только для `str_var`-аргументов, тип которых зависит
от объявления переменной в globals (`int`/`float`/`DWORD`).

## 6. Ambiguities / unverified

1. **x87 в псевдокоде.** `decompiled.c` для `FUN_1403a010`/`FUN_1403a914`
   теряет `fdiv`/`fmod`/`fsub` и показывает одинаковые `fcomp` для разных
   сравнений. Все семантики в §3 восстановлены по инструкциям
   `llvm-objdump`; таблица jump-таблиц: eval `0x14090580`, exec `0x14090680`.
2. **Variable host vtable.** Объект `vm+0x410` (`FUN_14038fa8`,
   `FUN_1403a010` case `0x17`) реализует `+0x4`, `+0xc`, `+0x14`, `+0x1c`,
   `+0x20`. Реализация класса в корпусе не идентифицирована; как `+0xc`
   возвращает значение string-переменной — `unverified`. В `zlato_vars.scr`
   string-переменных нет, а `0x16`/`0x17` не встречаются в поддеревьях
   сравнений, поэтому на поставляемых данных это не наблюдаемо.
3. **`FUN_1403fb04` (инициализация узла).** Не проверял, какие c/d
   выставляет конструктор для узлов без c/d (`0x15`, `0x18`, `0x16`, `0x17`).
   Для top-level value-узлов поведение задокументировано решением D-023;
   считать `c=d=0` — предположение (`unverified`).
4. **`WD_SetCellsGroupFlag`** (`0x43008`) по декомпиляции и дизассемблеру
   состоит только из проверки `FUN_14038c6c(3,3,0)` и возврата `0.0`; нет
   вызовов и записей. Непонятно, заглушка это или потерянная логика —
   `unverified`.
5. **`FUN_14038c6c` arity.** Функция проверяет только перечисленные типы;
   после `-1` возвращает 1, не проверяя наличие/тип лишних аргументов.
   Поэтому `arity` в §5 — минимальное число проверяемых аргументов, а не
   строгий максимум.
6. **ID `0`** (51 вызов) отсутствует в реестре и в `natives.csv`; в
   `FUN_14039888` для него вернётся `0.0`. Что это за конструкция в
   скриптах — `unverified`.
7. **Строковые opcode.** `0x16` в evaluator всегда `0.0`, но в качестве
   аргумента `func` (`FUN_14038fa8`) — type 3. Порт различает эти контексты
   неявно; для реальных данных (string только как arg) расхождения нет.
