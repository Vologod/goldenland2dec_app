# P2-15b3 — `BT_TRIBE` Relation Matrix Design

Status: approved (2026-11-16)
Phase: P2-15b3 (P2-15b read domains; after b1b1–b1b3, b2)
Layers: `assets` (parser/API) + `tools/glgame` (dump)

## 1. Goal

Декодировать и хранить `BT_TRIBE` relation matrix (234×234 байт,
row-major в порядке entry-списка), дать lookup API `relation(id_a, id_b)`
с default `2`, `index_of`/`find` и row accessor; подтвердить 28/28 на
реальных save. Runtime-применения нет: ярлыки значений 0–4 (OQ-7) не
доказаны, AI/faction-потребителей в порте нет.

## 2. Evidence

`docs/research/p2-15a-save-payload.md` §`BT_TRIBE`:

- Body: `u32 count`, затем `count × (u32 id, u32 name_len, cp1251 name)`,
  затем `count × count` single bytes row-major. Reader `FUN_140168e4` RVA
  `0x168e4` (16996-17032), writer `FUN_14016828` RVA `0x16828`
  (16955-16990), global object `DAT_14089fc0`.
- Reader вставляет `(name,id)` в таблицу (vtable `+0x1c`), затем читает
  `count` строк по `count` байт; `FUN_14016998` (RVA `0x16998`,
  17038-17045) возвращает байт матрицы с default `2` при отсутствующем id.
- Corpus: все 28 saves `count=234`, заголовок 4128 B, matrix 54756 B,
  body 58884 B; значения `{0:1257, 1:6096, 2:1519260, 3:6552, 4:3}`;
  диагональ `3` для всех 234. Смысл значений 0..4 — `unverified` (OQ-7);
  потребители 18551/18564/19275/19370/19669/38961 не декомпилированы.
- Independent scan (2026-11-16, temporary Python, not committed):
  порядок строк = порядок entry-списка, **не id** — slot0 первый entry
  `(106, "L12_3_citizen")`, slot2 первый `(16, "snake")`; ids не плотные;
  ids уникальны во всех 28 saves, имена дублируются во всех 28 (например
  `citizen`), поэтому name-lookup first-match.
  Per-slot histograms: slot0 `{0:103, 1:232, 2:54186, 3:234, 4:1}`, slot2
  `{0:82, 1:232, 2:54207, 3:234, 4:1}`, slot21 `{0:22, 1:3, 2:54497,
  3:234}`. First row bytes: slot0 `3,2,2,2,2,2,2,0,2,0,0,2,0,0,2,0`;
  slot2 `3,2,2,2,2,2,2,0,2,0,2,2,0,0,2,0`; slot21 first 16 all `2` except
  `3` at index 0. Scripts:
  `ECE2486B5678961B9293E9D331F79A955160B05973A984843226B9BAC711080B`
  (matrix scan) and
  `FDA2F8256210B4F9F732DBBE70182B6F2310C8B42FCE241B50805468234039E9`
  (duplicate scan).

Port: `SaveTribeTable.entries` already parsed; matrix is currently
skipped (`save_payload.cpp:156-164`); no lookup API.

## 3. Scope

Входит:

- `SaveTribeTable.matrix` storage + `count()`.
- `index_of(id)`, `find(id)`, `find(name)` (first match),
  `relation(id_a, id_b)` (default 2), `row(index)` span.
- `parse_tribe` reads the matrix exactly (no skip), malformed checks.
- `glgame --dump-save` adds `tribe_matrix=<count>`.
- Tests: assets unit synthetic ordering/API/malformed, real 28-save matrix
  oracle, E2E dump string.
- Docs: D-076, STATUS/PROJECT/parity/research.

Не входит:

- Runtime faction/AI применение, декомпиляция consumer'ов и ярлыки 0–4
  (OQ-7), `PersonInstance.tribe_id`, save write (P2-15c), live-oracle.
- Enforcement уникальности id/имён в парсере (corpus 28/28 уникальные id,
  имена дублируются; name-lookup first-match).

## 4. API

`src/assets/include/gl/assets/save_payload.hpp`:

```cpp
struct SaveTribeTable {
    std::vector<SaveTribeEntry> entries;
    std::vector<std::uint8_t> matrix;

    std::size_t count() const { return entries.size(); }
    std::optional<std::size_t> index_of(std::uint32_t id) const;
    const SaveTribeEntry *find(std::uint32_t id) const;
    const SaveTribeEntry *find(std::string_view name) const;
    std::int32_t relation(std::uint32_t id_a, std::uint32_t id_b) const;
    std::span<const std::uint8_t> row(std::size_t index) const;
};
```

- `index_of`: линейный поиск по `entries` (234), `nullopt` при отсутствии.
- `find(id)`/`find(name)`: первый матч; `find(name)` — first-match из-за
  дубликатов имён.
- `relation`: если любой id отсутствует, или `matrix.size() !=
  count()*count()` → `2`; иначе `matrix[ia*count()+ib]` как `int32_t`.
- `row(index)`: `matrix.subspan(index*count(), count())`; для
  `index >= count()` — пустой span.

## 5. Parser

`parse_tribe`:

1. `u32 count`; entry loop как сейчас.
2. `count²` (64-bit) должно совпадать с оставшимся размером **ровно**;
   недостаток → `relation matrix is truncated`; лишние байты →
   `has trailing bytes`; `count == 0` → пустая матрица.
3. Матрица копируется в `out.matrix` (`std::vector<uint8_t>`), порядок
   байтов не меняется (row-major, строки в порядке `entries`).

`glgame --dump-save` добавляет `tribe_matrix=<count>` в конец строки
(существующие подстроки не меняются).

## 6. Tests

Unit (`tests/assets/test_save_payload.cpp`):

- synthetic `count=3`, ids `{7,1,4}`, имена `{"a","b","c"}` и матрица
  9 байт в порядке entry-списка: `relation(7,1)` = `matrix[0*3+1]`,
  `relation(1,7)` = `matrix[1*3+0]`, `relation(7,7)` = диагональ;
  `index_of(4)==2`, `index_of(9)==nullopt`; `find("b")->id==1`;
  `row(1)` bytes 3..5; `row(3)` пустой.
- duplicate name: entries `(1,"dup"),(2,"dup")` → `find("dup")->id==1`.
- `relation` с неизвестным id → `2`; матрица неверного размера (если
  собрать вручную) → `2`.
- malformed matrix: truncated → `format`; trailing byte → `format`.

Real (`tests/assets/test_save_payload_real.cpp`, extend):

- 28 saves: `count==234`, `matrix.size()==54756`, диагональ `3`,
  для всех `i,j` `relation(entries[i].id, entries[j].id) ==
  matrix[i*234+j]` (счётчик mismatches, assert 0 — не 1.5M assertions),
  `relation(0xFFFFFFFF, entries[0].id)==2`, `relation(entries[0].id,
  0xFFFFFFFF)==2`, `entries[i].id` уникальны;
- corpus histogram `{0:1257,1:6096,2:1519260,3:6552,4:3}`;
- pins slot0/2/21: count, первые 6 entries `(id,name)`, первые 16 байт
  row(0), per-slot histogram как в §2.

App (`tests/app/test_glgame_screenshot.cpp`):

- в `--dump-save 0` case добавить
  `CHECK(output.find("tribe_matrix=234") != std::string::npos)`.

## 7. Acceptance

1. Parser stores the exact `count²` matrix on 28/28 saves; no skip and no
   trailing bytes.
2. Relation API uses entry order (not id), default `2` for unknown ids,
   and all `i,j` map consistently (0 mismatches).
3. Pins/histograms/id-uniqueness/name-duplicate first-match verified.
4. E2E `--dump-save 0` contains `tribe_matrix=234`.
5. Полный `ctest` с `GL_GAME_DIR=I:\Games\Златогорье-2` зелёный.
6. Docs: D-076, STATUS/PROJECT/parity/research; OQ-7 labels remain
   unverified and are not claimed.

## 8. Risks

- OQ-7 meaning of values 0–4 remains unproven; only storage/lookup parity
  is claimed.
- Name duplicates are common (28/28 saves); `find(name)` is first-match
  and must not be used where exact tribe identity matters — id is the key.
- Unique ids are a corpus fact, not enforced; duplicate ids would make
  `index_of` first-match.
- Matrix is not applied to any runtime faction system yet; future AI must
  call `relation`/`index_of` and use ids (`person+0x150c`), not names.
- Save write (`P2-15c`) must preserve entry order and row-major matrix.

## 9. References

- `docs/research/p2-15a-save-payload.md` §`BT_TRIBE`, OQ-7/OQ-13.
- `src/assets/src/save_payload.cpp` `parse_tribe`;
  `src/game/src/save_restore.cpp` tribe id→name.
- Specs/plans P2-15b1b1/b1b2/b1b3/b2; D-071…D-075.
