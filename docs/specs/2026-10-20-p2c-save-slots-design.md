# Design: P2c — Save slot framework (read-only)

Статус: **утверждён**.
Дата: 2026-10-20.
Базируется на: `docs/specs/2026-10-20-p2a-game-shell-design.md`, `docs/DECISIONS.md`.

## 1. Цель

Добавить каркас сохранений: разбор заголовка `.sav`, список слотов с метаданными
(дата, название, размер, mtime), экран выбора слота в `glgame` и консольную команду
`saves`. Полная десериализация мира и запись сохранений **не входят** (P2c — только
чтение заголовков).

## 2. Границы

Входит:

- `gl_assets`: `SaveHeader` (magic/version/date/title) + `parse_save_header`.
- `gl_core`: `SaveManager` — сканирование каталога `Saves`, список/поиск слотов.
- `glgame`: пункт меню `Continue` (активен при наличии слотов) → экран списка слотов
  с метаданными; `saves` в консоли; `--list-saves` для тестов.
- Тесты: синтетика + реальные 28 сохранений.

Не входит:

- Запись/перезапись `.sav`, `game_save` остаётся заглушкой.
- Десериализация игрового мира (персонажи, локации, инвентарь) — P5/P6.
- Загрузка мира по выбранному слоту (только сообщение «не реализовано»).

## 3. Формат заголовка (подтверждён на 28 файлах)

```
offset  size  field
0       20    magic "Goldenland II save \0" (ASCII, NUL в конце)
20      4     version (u32, у целевой сборки = 17)
24      2     year  (u16)
26      2     month (u16)
28      2     unknown_a (u16)
30      2     day   (u16)
32      2     time_a (u16)
34      2     time_b (u16)
36      2     time_c (u16)
38      2     unknown_b (u16)
40      64    title (cp1251, NUL-terminated, дополнено нулями)
104     4     unknown_c (u32)
108     4     unknown_d (u32)
112     ...   payload (непрозрачный; похоже на сжатые данные, в P2c не разбирается)
```

Подтверждено: `slot0.sav` — title `Quick save`, year 2022/month 11; `slot16` — title
`Семейство кукуя`, 2013/02; `slot25` — `Прохождение артефакты`, 2013/02. `year/month/day`
совпадают с датой файла. `time_a/b/c` могут быть игровым временем; в API отдаются как
отдельные поля, без интерпретации.

## 4. API

`gl_assets/save.hpp`:
```cpp
namespace gl::assets {
struct SaveHeader {
    std::uint32_t version = 0;
    std::uint16_t year = 0, month = 0, day = 0;
    std::uint16_t time_a = 0, time_b = 0, time_c = 0;
    std::string title; // UTF-8 (cp1251 decoded, trimmed)
};
Result<SaveHeader> parse_save_header(std::span<const std::uint8_t> data);
}
```
Валидация: длина ≥ 112, magic, title в пределах 64 байт; иначе `format`.
Версия не ограничивается 17 (отдаётся как есть), но интеграционный тест проверяет 17.

`gl_core/save_manager.hpp`:
```cpp
namespace gl::core {
struct SaveSlotInfo {
    int slot = -1;
    std::filesystem::path path;
    std::uint64_t file_size = 0;
    std::filesystem::file_time_type modified;
    bool valid = false;
    gl::assets::SaveHeader header;
};
class SaveManager {
public:
    static gl::Result<SaveManager> scan(const std::filesystem::path& save_dir);
    const std::vector<SaveSlotInfo>& slots() const; // sorted by slot
    const SaveSlotInfo* find(int slot) const;
    bool has_valid_slots() const;
};
}
```
Сканирует `slot<N>.sav` (регистронезависимо), читает первые 128 байт; битые файлы
попадают в список с `valid=false`; отсутствующий каталог — пустой список (не ошибка).

## 5. UI/CLI

- Меню `glgame`: `Continue` активен, если `has_valid_slots()`; иначе серый.
- `Continue` → экран `Save slots`: строки `Slot N  YYYY-MM-DD  title  (size KB)`;
  Up/Down/мышь, Enter → сообщение `world deserialization not implemented (P2c)`,
  Esc → меню. `New Game`/`Exit` без изменений.
- Консоль: `saves` печатает тот же список.
- CLI: `--list-saves` печатает слоты и выходит 0 (для тестов/CI), без SDL-окна.

## 6. Тестирование

Unit (`gl_assets`): синтетический заголовок (ASCII и cp1251 title), неверный magic,
короткий файл, title без NUL (заполнение), пустой title.
Unit (`gl_core`): временный каталог с `slot0.sav`, `slot3.sav`, `broken.sav`, `other.txt`;
проверка индексов/сортировки/valid/поиска; отсутствующий каталог → пусто.
Integration (`GL_GAME_DIR`): `SaveManager::scan(<game>/Saves)` → 28 слотов, все valid,
version 17, `slot0.title == "Quick save"`, `slot16.title` содержит `Семейство`,
`slot25.title` содержит `Прохождение`; даты совпадают с mtime.
App: `glgame --game-dir ... --list-saves` → exit 0, 28 строк, есть `slot0`; screenshot
экрана слотов (если добавляется состояние) или меню с активным Continue.

## 7. Критерии готовности

1. `parse_save_header`/`SaveManager` покрыты тестами, реальные 28 слотов читаются.
2. `glgame --list-saves` печатает 28 слотов и выходит 0.
3. Меню: `Continue` активен; экран слотов показывает метаданные; Esc назад.
4. `game_save` остаётся заглушкой (не пишет файлы).
5. `gl_assets`/`gl_core` SDL-free; документы обновлены.

## 8. Риски

- `time_a/b/c` могут быть не временем — не интерпретируем.
- Title может быть пустым/битым — показываем пустую строку, слот остаётся valid.
- Чужие/старые версии: version отдаётся, UI не падает.
- Payload (после 112) не читаем; полная десериализация — отдельный подпроект.

## 9. Отложено

Запись сохранений, десериализация мира, автосейвы, облачные слоты, превью-скриншоты.
