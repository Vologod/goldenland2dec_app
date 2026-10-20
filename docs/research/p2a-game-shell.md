# Исследование: оболочка glgame (P2a)

Дата: 2026-10-20. Источники: `I:\Games\Златогорье-2\GoldenLand.ini`,
`I:\Games\Златогорье-2\Saves\slot*.sav`, `src/core/src/{config,state,server}.cpp`,
`src/game/src/level_loader.cpp`, `tools/glgame/main.cpp`, прогон приёмки P2a.

## `GoldenLand.ini`

- Целевой файл: 2 478 байт, 110 строк, только CRLF (110 `\r\n`, 0 одиночных `\n`);
  779 байт с кодом ≥ 0x80. Кодировка — cp1251; API `gl::core::Config` отдаёт UTF-8.
- Строки: пустые, `// комментарий` (в том числе после значения), `bind ...` и
  настройки `ключ <пробелы/табы> значение`. Значения без кавычек; `r_resolution`
  хранит два числа в одной строке.
- `glgame` по умолчанию читает `<game-dir>/GoldenLand.ini` (или `--config`).
  Неизвестные строки, комментарии, `bind`, порядок и EOL сохраняются; запись идёт
  построчно в cp1251. `cl_username`, `r_windowed`, `r_resolution`, `d_console`,
  `gv_pause` читаются/пишутся оболочкой. Отсутствующий INI — defaults + warning,
  `save` создаёт файл (D-020). Запись идёт через temp-файл с последующим rename.
- `//` считается комментарием только в начале строки или после пробела/таба; внутри
  значения (например, `http://`) сохраняется. `set` игнорирует значения с `\r`/`\n`.
- `gv_free_camera` — P2a-заглушка: меняет runtime-флаг и не влияет на камеру;
  в INI не сохраняется. `gv_skip_logo` — no-op (логотип в P2a не показывается).

## Заголовок `.sav`

- Все 28 файлов `Saves\slot*.sav` целевой установки начинаются с 20 байт:
  ASCII `Goldenland II save ` (19 байт) + `00` в offset 19.
- Строки `Goldenland II save` и `Saves\slot%d.sav` также найдены в `Client.dll`
  (см. `docs/research/00-game-build.md`). Полная десериализация `.sav` в P2a не
  реализована; `game_save` — заглушка.

## Консоль и команды P2a

- Открытие/закрытие консоли — `` ` ``/`~`; вывод ограничен 256 строками, история — 64,
  на экране последние 10 строк. `Tab` дополняет имя команды по префиксу.
- Реализованный набор (9 команд): `help`, `exit`, `d_console [0|1]`,
  `gv_pause [0|1]`, `gv_free_camera [0|1]`, `r_resolution <width> <height>`,
  `r_windowed <0|1>`, `cl_username <name>`, `game_save <slot>` (заглушка).
- `gv_pause`, `gv_free_camera`, `r_windowed`, `r_resolution`, `cl_username` идут через
  `ServerCore::submit`/`tick`; оболочка применяет события `config_changed`/
  `paused_changed` к `Config` и окну. `r_resolution` приходит как
  `config_changed` с `value = "<w> <h>"`; resize и запись в INI делает оболочка.
  `gv_free_camera` меняет только runtime-состояние и не персистится. Неизвестная
  команда/лишние аргументы — лог, не фатальны.
- `d_console` из INI применяется при старте; `gv_pause`/`r_windowed`/`cl_username`
  персистятся без потери `bind`/комментариев.

## Состояния

`StateMachine`: `boot → menu → level`; из `level` разрешены `level` (reload) и
`menu`; переход в `exit` разрешён из любого состояния, после `exit` переходов нет.
`Menu`: New Game (`l1_1`), Continue отключён, Exit. `Level`: `Esc` — в меню,
`Home` — сброс камеры, `F1` — debug overlay; `gv_pause` замораживает анимации.

## Пути уровня в `glgame`

Все чтения идут через `GameVfs` по `<game-dir>`; файлы не распаковываются.

- `<game-dir>/GoldenLand.ini` — конфиг.
- `levels/single/<name>/<name>.sef` — SEF; поле `pack` даёт имя pack в lowercase.
- `levels/lvl/<pack>.lvl` — LVL.
- `levels/pack/<pack>/bitmaps/layer.jpg` — фон.
- `levels/pack/<pack>/bitmaps/static/static_<number>.csx` — статика.
- `levels/pack/<pack>/data/animated/<pack>.lao` и
  `levels/pack/<pack>/bitmaps/animated/anim_<i>.csx` — анимации.

Для `l1_1`: pack `l1_1`, фон 2760×2016, 2 статики, 2 анимации.
