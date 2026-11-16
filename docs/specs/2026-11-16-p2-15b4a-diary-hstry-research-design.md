# P2-15b4a — BT_DIARY / BT_HSTRY Research Design

Status: implemented (2026-11-16); verdict in `docs/research/p2-15b4-diary-hstry.md`
Phase: P2-15b4a (research-only; docs-only)
Layers: none (static decompilation + corpus scans)

## 1. Goal

Определить, что можно реализовать в порте для `BT_DIARY` (журнал/квесты) и
`BT_HSTRY` (лог событий) без догадок: framing, поля 0x58-записей, sparse
секция, ring-порядок лога; зафиксировать verdict и декомпозицию.

## 2. Questions

- DIARY: структура контейнера, entry header (`+0x00`, `+0x04`), 0x58-record
  layout, sparse-секция, кто создаёт/читает записи.
- HSTRY: точный body-схема, `head`/`tail`, порядок 40 слотов, смысл
  `u32 id`, host-методы writer/reader (`GoldenLand.exe`).
- Что реализуемо сейчас (framing), что требует дальнейшего research.

## 3. Method

- `Server.dll` SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`,
  `decompiled.c` corpus в `.local/decomp/server/`.
- Дизассемблер/декомпилятор для `FUN_14058768` (0x58768), `FUN_140588c8`
  (0x588c8), `FUN_14058b1c` (0x58b1c), `FUN_14058d68` (0x58d68),
  `FUN_14058de0` (0x58de0), `FUN_14059030` (0x59030), `FUN_140591b8`
  (0x591b8), `FUN_140592d8`/accessors range.
- Read-only Python scans 28 `Saves/slot*.sav`: DIARY entry/block/record
  counts, sparse count, per-field value distributions; HSTRY head/tail/
  slot/id statistics.
- HSTRY host: определить, находятся ли writer/reader методы в
  `GoldenLand.exe`; если да — зафиксировать как блокер без decompile host.

## 4. Scope

Входит: research документ, verdict, decomposition, D-077, STATUS/parity.

Не входит: implementation, save write, live-oracle, изменение кода,
запуск оригинальных бинарников.

## 5. Acceptance

1. DIARY container framing подтверждён или опровергнут на 28/28.
2. 0x58-record описан на уровне подтверждённых полей; неизвестные явно
   помечены `unverified`.
3. HSTRY body-схема и corpus значения подтверждены; host-методы
   локализованы или явно `blocked`.
4. Verdict `scoped` с декомпозицией: framing parser (implementable),
   record semantics (blocked/next), HSTRY ring (blocked/next).
5. Docs-only: `src/`, `tests/`, `tools/` не менялись; UTF-8 no BOM/NUL.

## 6. Risks

- 0x58 записи могут содержать runtime-указатели/индексированные id, не
  переносимые между сохранениями; интерпретация без accessor'ов опасна.
- HSTRY host методы могут быть недоступны без decompile `GoldenLand.exe`
  или live-наблюдения.
- `unverified` поля нельзя объявлять parity.

## 7. References

- `docs/research/p2-15a-save-payload.md` §`BT_DIARY`/§`BT_HSTRY`, OQ-8/OQ-9.
- D-071…D-076.
