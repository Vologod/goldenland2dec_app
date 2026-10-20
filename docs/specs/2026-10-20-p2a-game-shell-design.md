# Design: P2a — Game shell and console (Windows)

Статус: **утверждён**.
Дата: 2026-10-20.
Базируется на: `docs/specs/2026-10-19-goldenland2-android-port-design.md`, `docs/DECISIONS.md`.

## 1. Цель

Превратить dev-viewer (`glview`) в оболочку игры `glgame`: читать оригинальный
`GoldenLand.ini`, показывать меню, запускать уровень, открывать консоль (`~`) с
командами движка, хранить состояние приложения и заложить границу `ServerCore`/`ClientCore`.

P2a — каркас; полноценные игровые системы (P3–P7), save-сериализация и Android (P9) позже.

## 2. Границы

Входит:

- `gl_core`: конфиг `.ini` (чтение/запись с сохранением неизвестных строк и комментариев),
  реестр команд, парсер консольной строки, история команд.
- Текстовый рендер (встроенный ASCII bitmap-font) для меню/консоли.
- Состояния приложения: `Boot → Menu → Level → (Pause/Console overlay) → Exit`.
- Меню: New Game (загрузить `l1_1`), Exit; Continue — заглушка (save-сериализация позже).
- Консоль: `~`, ввод, история, `help`, `exit`, `d_console`, `gv_pause`,
  `gv_free_camera`, `r_resolution`, `r_windowed`, `cl_username`, `game_save` (заглушка).
- Скелет границы `ServerCore`/`ClientCore`: очередь команд/событий в одном процессе,
  тик сервера, клиент читает события; без сокетов.
- Новый exe `glgame`; `glview` остаётся dev-инструментом.
- Тесты: конфиг, команды, состояния, screenshot-тест оболочки.

Не входит:

- Полная десериализация `.sav` и реальный save/load мира.
- Скриптовая машина, персонажи, бой, магия (P3–P5).
- Полный набор консольных команд оригинала.
- Android/touch (P9), звук (P8), UI-экраны P7.

## 3. Данные и форматы

### 3.1 `GoldenLand.ini`

Текст cp1251, строки:

- `// комментарий` (в т.ч. после значения);
- `ключ значение` (разделитель — таб/пробелы);
- `bind DIK_<KEY>, vk_<action>` (запятая; хранится как строка, без интерпретации в P2a);
- значения: строка без кавычек (`cl_username Вася`), int (`r_windowed 1`), float (`gv_minimap_alpha 0.8`).

Модель: массив строк (raw) + индекс `key → line index`. Запись меняет значение в
существующей строке, сохраняя комментарии/порядок/неизвестные строки; новые ключи
добавляются в конец. Кодировка — cp1251; на API — UTF-8.

Ключи P2a: `cl_username`, `r_windowed`, `r_resolution` (два int), `d_console`,
`gv_pause`, `gv_free_camera`, `gv_skip_logo`, `bind`. `gv_free_camera` — runtime-only
заглушка (камера не размыкается), `gv_skip_logo` — no-op (логотип в P2a не показывается).

### 3.2 Консоль

- Открытие/закрытие `~` (тильда/обратная кавычка), ввод ASCII/UTF-8.
- Строка: `команда [аргументы]`, кавычки `"..."` для пробелов.
- Реестр: имя, краткое описание, обработчик `(span<string_view> args) -> Result<string>`.
- `help` — список команд; `help <cmd>` — детали; неизвестная команда — сообщение.
- История: стрелки вверх/вниз, до 64 строк; `Tab` — автодополнение имени (по префиксу).
- Вывод — в лог и в overlay (последние N строк).

### 3.3 Состояния

```
Boot -> Menu -> Level -> Exit
         ^        |
         +-- Pause/Console overlay
```

`Level` использует существующие `gl_game::Level` + `gl_render` (фон/статика/анимации).
`Pause` замораживает анимации; `Console` — overlay поверх текущего состояния.

### 3.4 ServerCore/ClientCore (скелет)

```cpp
struct Command { std::string name; std::vector<std::string> args; };
class ServerCore {
public:
    void submit(Command);
    void tick(double dt);
    std::vector<Event> drain_events();
};
class ClientCore {
public:
    void on_event(const Event&);
    // рендер/ввод остаются на клиенте
};
```
В P2a команды консоли (`gv_pause`, `r_windowed`, ...) идут через `ServerCore`; тик
обновляет время/паузу. События — `PausedChanged`, `ConfigChanged`, `LogMessage`.
Сокеты не используются; граница сохраняется для будущего мультиплеера.

## 4. Архитектура

```
glgame (app) → core (config/commands/state) → assets/game/render
      ↘ platform (SDL2)
```

- `gl_core` (новый, SDL-free): `Config`, `CommandRegistry`, `ConsoleBuffer`, `StateMachine`,
  `ServerCore`/`ClientCore` интерфейсы (без SDL).
- `gl_render`: добавляет `BitmapFont` (ASCII 5×7/8×8) и `draw_text` для меню/консоли.
- `gl_app`/`glgame`: окно, ввод, состояния, меню, консоль, запуск уровня.
- `glview` остаётся для быстрого просмотра уровней.

## 5. Меню и консоль (визуал)

- Меню: тёмный фон, заголовок «Goldenland 2», пункты `New Game`, `Continue` (disabled),
  `Exit`; выбор стрелками/мышью, Enter.
- Консоль: полупрозрачная панель снизу, строка ввода `> `, история вывода, мигающий курсор.
- Текст: встроенный bitmap-font, масштаб 2×; ASCII. Русские строки UI — позже (P7).
- `--screenshot` для `glgame`: меню и/или уровень (для тестов).

## 6. CLI `glgame`

```
glgame --game-dir <dir> [--level l1_1] [--config <ini>] [--width <px>] [--height <px>]
       [--screenshot <png>] [--menu] [--exec "<console command>"]
```
- `--menu` — старт в меню (по умолчанию).
- `--level` + `--screenshot` — сразу загрузить уровень и сохранить кадр (для тестов).
- `--exec` — выполнить консольную команду при старте (можно несколько раз).
- Коды выхода: 0 успех, 1 аргументы, 2 I/O, 3 формат/загрузка.

## 7. Ошибки и логирование

- Нет `--game-dir`/каталога игры → понятная ошибка (2).
- Отсутствующий INI (в т.ч. `--config`) — не ошибка: загружаются defaults с warning;
  `save` создаёт файл и родительские каталоги (D-020).
- Битый INI: сохранить, что распарсилось, предупредить.
- Неизвестная команда/аргументы — сообщение в консоль, не фатально.
- Лог через `gl::log` (info при старте: конфиг, состояние, counts).

## 8. Тестирование

Unit (`gl_core`, без SDL):

- Config: комментарии, bind, строки с пробелами, int/float/bool, round-trip с сохранением
  неизвестных строк, cp1251→UTF-8→cp1251.
- CommandRegistry: регистрация, парсинг кавычек, help, неизвестная команда, ошибки аргументов.
- ConsoleBuffer: история, автодополнение по префиксу.
- StateMachine: допустимые/недопустимые переходы.

Integration/render:

- `glgame --game-dir ... --screenshot menu.png --menu` → 1280×720, ≥2 цвета.
- `glgame --game-dir ... --level l1_1 --screenshot level.png --exec "gv_pause 1"` → кадр.
- Консоль: `--exec "help"` не падает; `--exec "unknown"` не фатально.

## 9. Критерии готовности

1. `glgame --game-dir I:\Games\Златогорье-2` показывает меню; New Game грузит `l1_1`;
   `~` открывает консоль; `help`, `gv_pause`, `exit` работают.
2. Изменение `gv_pause`/`r_windowed` сохраняется в INI без потери `bind`/комментариев.
3. `--screenshot` меню и уровня работает; тесты зелёные.
4. `gl_core` не зависит от SDL; `gl_assets`/`gl_game` остаются SDL-free.
5. Документы обновлены (`STATUS`, `PROJECT`, `DECISIONS` при отклонениях).

## 10. Риски

- Формат INI может иметь несколько значений на строку/экранирование — парсер line-based,
  неизвестные строки сохраняются как есть.
- Текст только ASCII в P2a; русские подписи — P7.
- Save/load — только заглушки; не путать с принятием P2c.
- Меню/консоль на bitmap-font требуют ручной проверки читаемости.

## 11. Отложено

Полная десериализация `.sav`, Continue/New Game с реальным миром, весь набор команд,
скрипты/NPC/бой, P7 UI, звук, Android/touch.
