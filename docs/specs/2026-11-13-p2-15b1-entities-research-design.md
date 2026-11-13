# P2-15b1 — Save entities/persons restore research (design)

Status: implemented (research complete 2026-11-13)
Phase: P2-15b1 research (docs-only; first increment of full P2-15b)
Layers: research only

## 1. Goal

Разобрать tail-снимок сейва (`BT_RGINF`, `BT_ENTS`, `BT_PNAME`) до уровня,
достаточного для восстановления persons/entities, и выдать scope
реализации. Production-код не меняется.

## 2. Evidence / provenance

| Artifact | SHA-256 |
|---|---|
| `Server.dll` | `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837` |
| `Client.dll` | `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16` |
| `.local/decomp/server/decompiled.c` | `F51F931EE7EC8207B8633360D254AB0D8AF975CA9E0BCBC5E62F3011825FB2FA` |
| 28 saves manifest | `F996E2C1762CB8BD6151576B7BAE311190FC86E95B6A4C7FA06389A2AF0E603B` |

Из `docs/research/p2-15a-save-payload.md`:

- Tail section (после zlib) содержит `BT_RGINF` (36 B) и `BT_ENTS`
  (`u32 type` + body, terminator type 1); `BT_PNAME` — имена persons,
  читается отдельным lookup 38563, не `FUN_140325fc`.
- `FUN_14032c2c` пишет snapshot: `BT_PNAME` 35980, `BT_RGINF` 36040,
  `BT_ENTS` 36043-36105; фильтры `(flags & 0x3e)==2`, `+0x150==0`.
- `FUN_1405c83c(type)` создаёт entity; vtable `+4` читает тело
  (`FUN_1401e424` для persons); `FUN_140368d4` устанавливает в мир.
- `BT_ENTS` record body size per type неизвестен (OQ-12) — блокер.

## 3. Research questions

1. **Entity type map.** Какие `type` встречаются в `BT_ENTS` 28 save
   (histogram); `FUN_1405c83c` dispatch; для каждого type — класс/фабрика
   (RVA) и reader vtable `+4` (RVA/lines).
2. **Body layout/size.** Для каждого type: точный body size (constant или
   self-delimiting), поля; как reader определяет конец записи; как
   разобрать список без ошибок (terminator type 1).
3. **`BT_RGINF` (36 B).** Поля `u32 + 0x20`, semantics; consumers
   `+0x26c0`/`+0x26a0`; нужно ли для restore региона/уровня.
4. **`BT_PNAME`.** Формат, порядок, связь с `BT_ENTS`/persons; lookup
   `FUN_...38563`; обязателен ли для restore.
5. **Person restore.** Какие поля person/entity восстанавливаются из
   тела (`FUN_1401e424`/`FUN_140368d4`): позиция, направление, route,
   параметры, inventory? Что берётся из SEF/пересчитывается.
6. **Port mapping.** `PersonInstance`, `PersonRuntime`, `InventoryRuntime`,
   `LevelSimulation`: какие поля можно заполнить; что отсутствует/
   unverified; порядок restore относительно SEF/NPC loot.
7. **Оракулы.** Как проверять restore 0/N: сравнение entity body
   байт-в-байт, поля person, inventory counts; какие 28 save/slot0/slot2
   использовать.

## 4. Methods

- Ghidra headless/decompiled: factory `FUN_1405c83c`, vtables `+4`,
  readers, `FUN_14032c2c`/`FUN_140325fc`, 38563 lookup, `FUN_140368d4`.
- Python scan всех 28 tail sections: entity type histogram, body sizes,
  terminator, `BT_RGINF`/`BT_PNAME` presence/content.
- Live read-only diff — только если static body size нельзя доказать;
  user-assisted, без записи.
- Скрипты `%TEMP%`, не коммитить.

## 5. Deliverable

`docs/research/p2-15b1-save-entities.md`:

1. Type histogram 28/28 + per-type factory/reader RVA/lines.
2. Body layout/size per type; алгоритм разбора списка; `BT_RGINF` layout;
   `BT_PNAME` формат/связь.
3. Restore flow: что оригинал берёт из body vs SEF; порядок.
4. Port mapping и gap-таблица (есть/нет/unverified).
5. Verdict + декомпозиция реализации (парсер entities, restore persons,
   tests) с acceptance.
6. Open Questions (без TBD).

## 6. Acceptance criteria

1. Все entity types из 28 save перечислены; histogram/terminator 28/28.
2. Для каждого type — фабрика/reader RVA + body size/layout, или
   `unverified` с exact next check (не догадка).
3. `BT_RGINF` поля и `BT_PNAME` связь описаны или unverified+next.
4. Restore mapping на порт с указанием, что можно восстановить сейчас.
5. Verdict/декомпозиция с acceptance и тестами.
6. Docs-only: нет изменений `src/tests/tools`; UTF-8 без BOM/NUL.

## 7. Out of scope

- Реализация restore (отдельный spec/plan).
- Диалоги/квесты/`BT_DIARY`/`BT_HSTRY`, `BT_VARS` values, `BT_TRIBE`.
- Save write/P2-15c.
- Изменение существующих tariff/parsers.

## 8. Risks

- Body size может зависеть от типа и вложенных count-полей; без reader
  невозможно безопасно разбить список — тогда scope реализации
  ограничивается raw tail + named lookup, а restore persons блокируется.
- Часть entity types может отсутствовать в `l1_1` (тестировать 28 save).
- Живой diff может быть недоступен; тогда static-only с явными
  unverified.

## 9. References

- `docs/research/p2-15a-save-payload.md` (tail, `BT_RGINF`/`BT_ENTS`,
  `FUN_14032c2c`, OQ-10/11/12).
- `docs/specs/2026-11-12-p2-15b-save-load-design.md`.
- `docs/research/p5-save-equipment.md` (BT_HERO, reader patterns).
- `docs/research/p3b2b3-transfers-server.md` (person/container patterns).
