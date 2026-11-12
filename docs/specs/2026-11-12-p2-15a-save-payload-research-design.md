# P2-15a — Save payload research (design)

Status: implemented (2026-11-12)
Phase: P2-15a (research-only; follow-up to P2c header-only save slots)
Layer: research (`docs/research/`), no production code

## 1. Goal

Разобрать payload `.sav` после 112-байтного заголовка: контейнер (zlib),
каталог блоков, reader/writer в `Server.dll`, state domains, и выдать
декомпозицию следующих подпроектов (`P2-15b` read-only load, `P2-15c`
write/`game_save`). Production-код не меняется.

## 2. Evidence / provenance

| Artifact | Value |
|---|---|
| `Server.dll` | SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `.local/decomp/server/decompiled.c` | SHA-256 `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| `Saves/slot0.sav` | SHA-256 `8AC545FE9715EF11C68B1CCA8617BEB03003ECA360F07F71C26E3FC5E5C73442` |
| 28 saves (slot0..slot27, sorted hashes) | manifest SHA-256 `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |

Предварительный скан (Python `zlib.decompress(raw[112:])`, 28/28) уже дал
одинаковый набор и порядок блоков:

| # | tag (8 байт) | offset rule | size (slot0) | domain (гипотеза) |
|---|---|---|---|---|
| 1 | `BT_SCRN ` | `0x0` | 750008 (fixed) | save preview screenshot: 8 header + 500×375×4 RGBA (client consumer; Task 3 evidence) |
| 2 | `BT_INFO ` | `0xB71C4` | 100 (var) | level/party info (`+0x26c4` loot level, 1b2b2b) |
| 3 | `BT_VARS ` | после `BT_INFO` | 31035 (var) | script globals/variables |
| 4 | `BT_TRIBE` | после `BT_VARS` | 58884 (var) | tribes/factions/persons metadata |
| 5 | `BT_DIARY` | после `BT_TRIBE` | 32960 (var) | journal/quest diary |
| 6 | `BT_HSTRY` | после `BT_DIARY` | 1592 (var) | history/event log |
| 7 | `BT_HERO ` | после `BT_HSTRY` | 30388 (var) | hero + 3 inventories (P5a) |

Все 28 save: 7 блоков, порядок одинаковый; `BT_SCRN ` всегда `0x0` и
size `750008`; блоки идут подряд без padding (конец блока N = начало N+1),
последний `BT_HERO ` заканчивается ровно на decompressed length.

## 3. Research questions

1. **Контейнер:** framing (tag 8 байт без NUL, u32 little-endian size,
   body); отсутствие padding 28/28; zlib-параметры; может ли size блока
   не совпадать с фактическим body.
2. **Reader/writer:** `FUN_1403210c` (RVA `0x3210c`, reader тела save) и
   `FUN_14031b64` (RVA `0x31b64`, writer) — call-graph по каждому блоку,
   порядок write/read, checks/ошибки, версии (`version=17`).
3. **`BT_SCRN `:** подтвердить preview/thumbnail (500×375×4 + 8) и
   client-only consumer; определить, где лежит динамическое состояние мира
   (world/persons/triggers) — вероятно `BT_TRIBE`/`BT_VARS`/`BT_INFO`, а
   статика уровня перезагружается из SEF; связь с `FUN_1401edc4`/SEF.
4. **`BT_INFO`:** поля (включая `+0x26c4`), что уже известно (1b2b2b).
5. **`BT_VARS`:** script globals map (names/values/types), writer
   `FUN_14006...`/reader; нужен для диалогов/квестов.
6. **`BT_TRIBE`, `BT_DIARY`, `BT_HSTRY`:** domain, ключевые поля,
   зависимости от `BT_HERO`/persons.
7. **`BT_HERO `:** уже разобран P5a; research фиксирует только
   reader/writer mapping и место в общем payload.
8. **Декомпозиция:** минимальный read-only набор для `P2-15b` (порядок
   загрузки), полный write-path для `P2-15c`, риски/unknowns.

## 4. Methods

- Ghidra headless по `Server.dll` (уже разобранный проект), decompiled.c
  line refs для каждого reader/writer.
- Независимый Python-скан 28 save: offsets/sizes/padding, cross-check
  порядка; скрипты — в `%TEMP%`, не коммитятся.
- При неоднозначности структуры блока — точечный read-only live-diff
  (загрузить save в оригинале, сравнить память с декодированным блоком);
  только при необходимости, user-assisted.
- Никаких изменений production-кода; только документы.

## 5. Deliverable

`docs/research/p2-15a-save-payload.md`:

1. Таблица блоков 28/28 (tag/offset/size/padding/order; fixed vs var).
2. Reader/writer mapping: RVA, decompiled.c lines, call-graph, порядок.
3. State domains по блокам: что читает/пишет оригинал; что уже покрыто
   портом (`BT_HERO` P5a, `BT_INFO+0x26c4` 1b2b2b, `BT_VARS` — VM globals).
4. `BT_SCRN `: подтверждён preview (dims/pixels/client consumer); для
   `P2-15b` указано, какие блоки несут динамическое состояние мира/персонажей.
5. Вердикт/декомпозиция `P2-15b`/`P2-15c` с минимальным scope и
   acceptance-критериями.
6. Open Questions и явные unknowns (без TBD).

## 6. Acceptance criteria

1. Все 28 save разобраны; набор/порядок блоков подтверждён 28/28.
2. Для каждого блока указан reader и writer (RVA + decompiled.c line).
3. Padding/размеры объяснены или зафиксированы как unknown с планом проверки.
4. `BT_SCRN ` подтверждён как preview, а `P2-15b`-набор (какие блоки
   несут persons/quests/globals/world dynamic state) определён.
5. Декомпозиция `P2-15b`/`P2-15c` с критериями приёмки и порядком.
6. Docs-only: `ctest` не требуется; документы UTF-8 без BOM, NUL=0.

## 7. Out of scope

- Production-код, запись `.sav`, `game_save`/`game_load`, UI.
- Полная реализация world deserialization.
- Изменение существующих `BT_HERO`/`BT_INFO` парсеров.

## 8. Risks

- `BT_SCRN ` может содержать указатели/ID, зависящие от версии бинарника.
- Некоторые блоки могут писаться не симметрично reader'у (order/skip).
- Padding не нужен: блоки contiguous 28/28; проверить, что reader не
  ожидает выравнивания/скрытых под-структур.
- Live-diff требует запуска игры и может быть недоступен — тогда
  соответствующие пункты фиксируются как `unverified` с планом проверки.

## 9. References

- `docs/specs/2026-10-20-p2c-save-slots-design.md` (header, payload не читается).
- `docs/research/p5-save-equipment.md` (`BT_HERO`, writer/reader RVAs).
- `docs/research/p3b2b2-npc-loot-server.md` §15 (`BT_INFO +0x26c4`).
- `docs/research/parity-audit-p2-p3.md` P2-15 (`unverified/absent`).
