# P2-15b — Save load (first increment) Design

Status: implemented (first increment) (2026-11-12)
Phase: P2-15b (first increment of save load; follows P2-15a research)
Layers: `assets` (decoder) + `app`/`tools/glgame` (load/CLI); no write path

## 1. Goal

Загрузить `.sav` в порт: декодировать payload, выбрать уровень по `BT_INFO`,
загрузить SEF + динамический tail-снимок (raw), применить hero
(inventory/equipment/stats/outfit), сохранить decoded `BT_VARS`/tail для
диагностики. Continue в меню и `--load-save` грузят уровень из save.
Запись save не входит.

## 2. Evidence / provenance

- Research: `docs/research/p2-15a-save-payload.md` (7 блоков 28/28, reader
  `FUN_1403210c`, writer `FUN_14031b64`, tail dictionary, minimum read set).
- Saves: `.sav` header 112 B; zlib body; 7 блоков
  `BT_SCRN | BT_INFO | BT_VARS | BT_TRIBE | BT_DIARY | BT_HSTRY | BT_HERO`.
- `BT_SCRN` = client preview (8 + 500×375×4), skip by tag+size.
- Tail dictionary после zlib: `u32 count`, per entry `name_len+name`,
  `section_len+section`; section = `u32 raw_len + u32 zlib_len + zlib`;
  содержит `BT_RGINF` + `BT_ENTS` (+ `BT_PNAME` для другого lookup).
- `BT_INFO`: `string1` (session/global), `u32 +0x218`, 0x20 time blob,
  `string2` (slot description), `string3` = level id, `u32 +0x26c4`
  (party level; loot = +1), 8 B `+0x26c8` (research slot0: consumed 100/100).
- `BT_VARS`: `u32 count`, per var `u32 type`, `name_len+name` cp1251,
  value: type<3 = 8 raw bytes (f64/int), type 3 = строка.
- `BT_HERO`: существующие `parse_hero_inventory/equipment/stats` (P5a).
- `BT_ENTS`: список `u32 type` + type-specific body, terminator type 1;
  разбиение на записи требует OQ-12 → в этом инкременте только raw body.

## 3. Scope

Входит:

- `gl::assets::SavePayload` decoder (контейнер, блоки, `BT_INFO`,
  `BT_VARS`, tail, `BT_RGINF`/`BT_ENTS` raw, `BT_HERO` body).
- `glgame::load_save(slot)` — level из `BT_INFO` + hero pipeline P5a;
  Continue и `--load-save <slot>`.
- `--dump-save <slot>` headless (level/vars/tail/hero counts) для тестов.
- Real 28-save decode (level id + tail присутствует), slot0/slot2 pinned,
  E2E `--load-save 0`.

Не входит:

- Запись `.sav`, `game_save`, quick save.
- `BT_DIARY`/`BT_HSTRY`, `BT_TRIBE` semantics.
- Именованные поля `BT_ENTS`/person restore (OQ-12), `BT_RGINF` semantics
  (OQ-11), `BT_VARS` values в VM (OQ-5), faction AI (OQ-13).
- Preview rendering (`BT_SCRN`).

## 4. Architecture

### 4.1 `assets` — `save_payload.hpp/.cpp`

```cpp
struct SaveBlockView {
    std::string tag;          // ровно 8 байт, включая pad 0x20
    std::uint32_t offset = 0; // в decompressed stream
    std::uint32_t size = 0;
};

struct SaveInfo {
    std::string level_id;
    std::array<std::uint8_t, 0x20> time{};
    std::uint32_t field_218 = 0;
    std::int32_t party_level = 0;
    std::array<std::uint8_t, 8> field_26c8{};
};

struct SaveVar {
    std::uint32_t type = 0;
    std::string name;
    std::array<std::uint8_t, 8> value{};
    std::string text_value; // type == 3
};

struct SaveTailEntry {
    std::string name;
    std::uint32_t raw_len = 0;
    std::vector<std::uint8_t> section;
};

struct SavePayload {
    SaveHeader header;
    std::vector<std::uint8_t> decompressed;
    std::vector<SaveBlockView> blocks;
    SaveInfo info;
    std::vector<SaveVar> vars;
    std::vector<SaveTailEntry> tail;
    std::optional<std::size_t> level_tail_index; // по info.level_id
    std::vector<std::uint8_t> rginf; // raw body из выбранной секции
    std::vector<std::uint8_t> ents;  // raw body
};

Result<SavePayload> parse_save_payload(std::span<const std::uint8_t> file);
```

Правила:

- `parse_save_header` + zlib(`raw[112..]`); ошибки — `ErrorCode::format`.
- Блоки: tag 8 байт, u32 LE size, body; требовать 7 известных тегов в
  порядке research; `BT_SCRN`/`BT_DIARY`/`BT_HSTRY`/`BT_TRIBE`/`BT_HERO`
  сохраняются как views; `BT_HERO` body доступен hero-парсерам через
  `decompressed`.
- `BT_INFO`: `string1` (`u32 len + cp1251`), `u32 +0x218`, 0x20 time,
  `string2` (`u32 len + cp1251`), `string3` (`u32 len + cp1251`, level id,
  не пуст), `u32 +0x26c4` (party level), 8 B `+0x26c8`; весь body должен
  быть израсходован ровно (100/100 для slot0).
- `BT_VARS`: count; тип 0..3; name cp1251; type<3 — 8 байт; type 3 —
  `u32 len + bytes`; unknown type — format error.
- Tail: count; name + section; decompress section (`raw_len`/`zlib_len`);
  выбрать секцию по `info.level_id`; внутри найти `BT_RGINF` (36 B) и
  `BT_ENTS` (raw до конца секции); отсутствие — format error.
- `BT_ENTS` не разбивается на записи; `ents` — raw тело (после tag+size).
- `decode_save_body` остаётся совместимым (может переиспользовать
  `parse_save_payload`; поведение и ошибки — те же).

### 4.2 `tools/glgame` — загрузка

- `GameApp::load_save(int slot)`: path из `saves_`/synthetic; читает файл;
  `parse_save_payload`; при ошибке — `save_message_` + warning, state не
  меняется.
- При успехе: `pending_*` из существующего `load_equip_save` (stats,
  inventory, equipment, outfit) на том же `decompressed` body;
  `load_level(info.level_id)`; `rebuild_person_runtime(level.persons)`;
  `apply_loaded_equip_save()`; сохранить `last_save_payload_` для dump;
  `state_.transition(level)`.
- Continue (`save_activate`) вызывает `load_save(slot.slot)`; при успехе
  закрывает save-slots screen, при ошибке остаётся и печатает сообщение.
- `--load-save <slot>`: на старте загружает save (level из `BT_INFO`);
  `--level` игнорируется при `--load-save`. Hero outfit/inventory — как
  `--equip-save`.
- `--dump-save <slot>`: headless, без SDL-окна; печатает:
  `save: slot=N level=<id> vars=<count> tail=<count> section=<bytes>
  rginf=<len> ents=<len> hero_items=<n> hero_equipped=<n>`; exit 0/2.

## 5. Data flow

```text
file -> parse_save_header -> inflate -> block views
     -> BT_INFO (level_id, party_level, time)
     -> BT_VARS (count/types/names/raw values)
     -> tail dictionary -> selected section -> BT_RGINF/BT_ENTS raw
     -> BT_HERO body -> parse_hero_stats/inventory/equipment
load_save: level_id -> load_level(SEF) -> hero apply -> app state=level
```

## 6. Testing

Unit (`tests/assets/test_save_payload.cpp`):

- synthetic save builder: header + zlib(body с 7 блоками и tail) →
  decode; проверить level id, vars (типы/имена/raw), tail selected,
  rginf/ents, skip `BT_SCRN`.
- ошибки: bad magic, truncated block, unknown block order, missing selected
  tail, bad VARS type, tail zlib failure.
- `decode_save_body` совместимость: hero body байт-в-байт.

Real (`tests/assets/test_save_payload_real.cpp`, skip без `GL_GAME_DIR`):

- все 28 slots: header valid, 7 блоков, level id не пуст, selected tail
  найден; 28/28.
- slot0: level `L93_4`; vars count 1002, types `{0:905,1:94,2:3}`; tail
  count 213; selected section 8551; `BT_RGINF` 36; `BT_ENTS` raw length;
  `BT_HERO` hero_items/equipped counts.
- slot2: level `L6_3`; analogous pinned values из независимого Python
  reference (не из порта).
- determinism: два decode одного файла дают идентичные bytes/поля.

App (`tests/app/test_glgame_screenshot.cpp`):

- `glgame --dump-save 0` → exit 0, строка `level=L93_4`, counts > 0.
- `glgame --load-save 0 --width ... --screenshot` → exit 0, лог
  `level 'L93_4'`, hero inventory/outfit applied, нет write.
- Continue path (если доступен headless) — level id из save.

Verification commands:

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
build\bin\gl_assets_tests.exe --test-case="*save payload*"
build\bin\gl_assets_real_tests.exe --test-case="*save payload*"
build\bin\gl_game_tests.exe
build\bin\glgame_screenshot_tests.exe --test-case="*save*"
ctest --test-dir build --output-on-failure
```

## 7. Acceptance criteria

1. `parse_save_payload` декодирует все 28 save: 7 блоков, непустой level id,
   selected tail; 28/28.
2. slot0/slot2 pinned: level ids `L93_4`/`L6_3`, vars/tail/section/rginf/ents
   lengths и types из независимого reference; hero counts совпадают с P5a.
3. `glgame --dump-save 0` exit 0 с ожидаемой строкой; `--load-save 0`
   загружает `L93_4`, применяет hero outfit/inventory/stats, state=level.
4. Continue грузит уровень из save (не только outfit), ошибки не ломают меню.
5. Нет записи файлов/памяти игры; `BT_SCRN`/`BT_DIARY`/`BT_HSTRY` не
   применяются.
6. Полный `ctest` с `GL_GAME_DIR` — 100% passed; docs/STATUS/PROJECT/parity
   обновлены.

## 8. Risks / limitations

- `BT_ENTS` raw не разбит на записи: persons не восстанавливаются в
  симуляцию (OQ-12); это документированное ограничение инкремента.
- `BT_VARS` values не применяются к VM (OQ-5); хранятся raw + names/count.
- `BT_INFO` 0x20 time blob и `+0x26c8` сохраняются raw без интерпретации.
- `BT_TRIBE`/`BT_DIARY`/`BT_HSTRY` пропускаются по tag+size.
- Hero pipeline зависит от P5a; при отсутствии/битом save — naked fallback
  как сейчас.
- `--load-save` меняет приоритет над `--level`; документируется в help.

## 9. References

- `docs/research/p2-15a-save-payload.md` (container, reader, min read set,
  OQ-1…OQ-13).
- `docs/specs/2026-11-12-p2-15a-save-payload-research-design.md`.
- `docs/research/p5-save-equipment.md` (`BT_HERO`, P5a).
- `docs/research/p3b2b2-npc-loot-server.md` §15 (`BT_INFO+0x26c4`).
