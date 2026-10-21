# Design: P3c — Dialogs + SDB + Cyrillic font

Статус: **утверждён**.
Дата: 2026-10-20.
Базируется на: P3a (CS VM, `gl_script`), `docs/DECISIONS.md`.

## 1. Цель

Сделать диалоги видимыми: связать `dialogs_data()` (id фраз из `.cs`) с текстами
`sdb/dialogs/dialogsphrases.sdb`, добавить кириллический TTF-шрифт и экран диалога
в `glgame` (фраза NPC + варианты ответов).

## 2. Границы

Входит:

- `gl_render`: TTF-шрифт через `stb_truetype` (UTF-8, атлас, перенос строк), fallback на
  ASCII bitmap-font.
- `gl_script`: `DialogSession` — VM + SDB, текущая фраза, ответы, выбор ответа.
- `glgame`: экран диалога (`--dialog <path.cs>` и пункт меню `Test Dialog`), выбор ответа.
- `glscript --sdb <path>`: печать текстов фраз для id.
- Тесты: SDB-связка, сессия диалога на реальном `demon.d1.age.cs`, screenshot экрана.

Не входит:

- Привязка диалогов к NPC/движку (P4), портреты/озвучка, полный UI P7.
- AgeScript-компилятор (в `.scr` его нет; это конфиги).
- Редактор диалогов и SDB.

## 3. SDB (подтверждено)

`sdb/dialogs/dialogsphrases.sdb` (12040 записей) — `Sdb::parse` уже читает.
`D_Say`/`D_Answer` node `value` = id; пример: `demon.d1.age.cs` → `2684`
(«Кто ты, старик?»), `2686`, `2696`; ответы `2719`, `1529`. VFS учитывает
`sdb.update.3.pak`.

## 4. TTF-шрифт

- `third_party/stb/stb_truetype.h` (пин того же stb-коммита).
- `gl_render::Font`:
  - `static Result<Font> load_from_file(SDL_Renderer*, const std::filesystem::path&, float pixel_height)`;
  - UTF-8 → codepoints; glyph atlas (ASCII + Cyrillic U+0400–U+04FF + punctuation U+2000–U+206F);
  - `draw_text(renderer, utf8, x, y, color)`, `measure(utf8)`, `line_height()`;
  - word wrap по ширине (пробелы/переносы), `\n`;
  - fallback: если шрифт не загружен — существующий ASCII `draw_text` (`?` для не-ASCII).
- Поиск шрифта: `--font <path>`; иначе `C:\Windows\Fonts\segoeui.ttf`, `arial.ttf`,
  `consola.ttf`; если нет — fallback.
- Атлас — одна RGBA-текстура; nearest/linear (linear для TTF).

## 5. DialogSession

```cpp
namespace gl::script {
struct DialogLine { std::uint32_t phrase_id; std::string text; };
class DialogSession {
public:
    DialogSession(ScriptVm& vm, const gl::assets::Sdb& sdb);
    gl::Result<void> start();                    // run to wait_user/end
    bool waiting() const;
    std::string_view npc_text() const;           // current say text (UTF-8)
    std::span<const DialogLine> answers() const; // answer id + text
    gl::Result<void> choose(std::size_t index);  // user_input + run to next wait/end
    VmStatus status() const;
};
}
```
- `start()`: run VM to `wait_user`/`end`; if `wait_user`, map `dialogs_data()[0]` to SDB text
  and each answer id; missing SDB id → `"<phrase N>"` (id visible, no crash).
- `choose(i)`: bounds check; `vm.user_input(i)`; run to next wait/end; refresh.
- `npc_text`/answers valid while `waiting()`.

## 6. glgame

- CLI `--dialog <virtual/path.cs>` → загрузить globals (`zlato_vars.scr`), SDB, CS; открыть
  экран диалога (минуя меню).
- Меню: пункт `Test Dialog` (только если найден `demon.d1.age.cs`) → тот же экран.
- Экран: тёмная панель снизу, текст NPC (TTF, перенос), список ответов (нумерация,
  Up/Down/мышь, Enter/клик), `Esc` → меню/выход. Если `wait_user` без ответов —
  сообщение и `Esc`.
- `--screenshot` + `--dialog` рендерит кадр диалога (для теста).

## 7. Тестирование

Unit:

- UTF-8 декодер (ASCII, кириллица, 3/4-байтные); атлас/measure (если шрифт доступен).
- `DialogSession` на синтетическом VM: фраза/ответы/выбор/конец; missing SDB id → `<phrase N>`.
- Real: `demon.d1.age.cs` + globals + SDB → `npc_text()` непустой, ответы ≥1,
  `choose(0)` → следующая фраза непустая/конец.

Render:

- Если найден системный TTF: `glgame --dialog ... --screenshot` 1280×720, ≥2 цвета;
  иначе тест помечается skipped (77).

## 8. Критерии готовности

1. Реальный диалог отображается с кириллицей; ответы выбираются.
2. SDB-связка id→текст; missing id не падает.
3. TTF fallback на ASCII при отсутствии шрифта.
4. Тесты зелёные; `gl_assets`/`gl_script` SDL-free.
5. Документы обновлены; ограничение `?` для save-слотов закрыто (TTF).

## 9. Риски

- stb_truetype + атлас: аккуратность с UV/питчем, кэш глифов.
- Системный шрифт на Android отсутствует — P9 должен приложить свободный шрифт.
- Длинные фразы/переносы: тесты на узкую ширину.
- SDB может не содержать id — fallback `<phrase N>`.

## 10. Отложено

NPC-диалоги (P4), портреты/звук, полный UI P7, AgeScript-компилятор (не требуется).
