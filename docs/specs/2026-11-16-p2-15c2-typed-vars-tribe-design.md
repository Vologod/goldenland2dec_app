# P2-15c2 — Typed `BT_VARS` / `BT_TRIBE` Encoders Design

Status: approved (2026-11-16)
Phase: P2-15c2 (save write; typed blocks)
Layers: `assets` (body encoders) + `script` (VM→SaveVar adapter)

## 1. Goal

Кодировать `BT_VARS` и `BT_TRIBE` bodies из типизированных моделей:
`SaveVar` (после обновления из VM) и `SaveTribeTable`; на 28 saves
re-encoded body байт-в-байт равен исходному. Новые VM-глобалы не
добавляются (политика `c4`), HERO/tail остаются raw (`c3`).

## 2. Evidence

- `BT_VARS`: `u32 count`; per entry `u32 type` (0 int, 1 DWORD, 2 float,
  3 string), `u32 name_len + cp1251`, type<3 — 8 raw bytes f64 LE,
  type3 — `u32 len + cp1251` (`p2-15a-save-payload.md` §`BT_VARS `).
  Port: `SaveVar{type,name,value[8],text_value}`; VM `Value =
  variant<int32,uint32,double,string>`; `ScriptVm::globals()` (b2).
- `BT_TRIBE`: `u32 count`; count×(u32 id + string name); count² matrix
  bytes в entry-порядке (`p2-15a` §`BT_TRIBE`; b3).
- Real corpus: slot0 vars 1002 `{905,94,3}`, type3 0/28; tribe 234×234.
- Реализованы: `parse_save_payload` (raw bodies), `save_var_number`,
  `save_globals_from_vars`, `apply_saved_globals`, `SaveTribeTable`.

## 3. Scope

Входит:

- `assets::encode_vars_body(span<const SaveVar>)`.
- `assets::encode_tribe_body(const SaveTribeTable&)`.
- `script::update_vars_from_globals(saved, vm)`.
- Unit + real byte-equality tests; D-080; docs.

Не входит:

- Новые глобалы/удаление отсутствующих в save (c4), typed HERO/tail/
  ENTITIES (c3), запись файла/`game_save` (c4), type3 corpus добавить,
  header date/name.

## 4. API

`src/assets/include/gl/assets/save_payload.hpp`:

```cpp
Result<std::vector<std::uint8_t>> encode_vars_body(std::span<const SaveVar> vars);
Result<std::vector<std::uint8_t>> encode_tribe_body(const SaveTribeTable &table);
```

`src/script/include/gl/script/globals.hpp`:

```cpp
std::vector<gl::assets::SaveVar> update_vars_from_globals(
    const std::vector<gl::assets::SaveVar> &saved, const ScriptVm &vm);
```

(`#include "gl/script/script_vm.hpp"` in globals.cpp only.)

## 5. Encoding rules

`encode_vars_body`:

1. `u32 count = vars.size()` (bounds-check `<= u32`).
2. Per var in order: `u32 type` (0..3 else `format`); `u32 name_len +
   cp1251(name)`; type<3 → copy the 8 `value` bytes verbatim (already
   LE f64); type3 → `u32 text_len + cp1251(text_value)`.
3. Empty vector → 4-byte body. No trailing bytes.

`encode_tribe_body`:

1. `u32 count = entries.size()`.
2. Per entry: `u32 id`, `u32 name_len + cp1251(name)`.
3. Matrix: `matrix.size() == count*count` else `format`; append bytes
   verbatim (entry-order row-major). Empty table → `u32 0` and no matrix.

`update_vars_from_globals`:

1. Copy `saved` (order/name/type preserved).
2. For each var, find `vm.globals()` by name; missing → keep raw.
3. `type < 3`: numeric Value (`int32/uint32/double`) → write
   `static_cast<double>(value)` as 8 LE bytes into `var.value`; string
   Value → keep raw.
4. `type == 3`: string Value → `text_value`; otherwise keep raw.
5. Never append/remove vars; return the new vector.

## 6. Tests

Unit (`tests/assets/test_save_payload.cpp`):

- `encode_vars_body` synthetic: types 0/1/2 (raw 8 bytes preserved) and
  type 3 string; exact byte sequence; empty → `{0,0,0,0}`; invalid type 4
  → `format`.
- `encode_tribe_body`: synthetic 3 entries ids `{7,1,4}` and 9 matrix
  bytes → exact body; empty table → 4 bytes; matrix size mismatch →
  `format`.

Script unit (`tests/script/test_saved_globals.cpp`):

- adapter: int32→f64 bits, uint32, double, string (type3), missing name
  keeps raw, numeric/string mismatch keeps raw, order preserved, no
  append.

Real (`tests/assets/test_save_payload_real.cpp`):

- 28 saves: `encode_tribe_body(payload.tribes)` byte-equals the raw
  `BT_TRIBE` body slice (`blocks[3]`, `decompressed[offset..offset+size]`).

Real (`tests/script/test_real_save_vars.cpp`, new):

- 28 saves: parse payload; parse `zlato_vars.scr`; `ScriptVm` set_globals
  + apply saved overlay; `update_vars_from_globals(payload.vars, vm)`;
  `encode_vars_body` byte-equals the raw `BT_VARS` body slice
  (`blocks[2]`). Skip without `GL_GAME_DIR`.

App/E2E: not needed for c2 (library-only); existing dump strings unchanged.

## 7. Acceptance

1. `BT_VARS` body byte-equality 28/28 via VM overlay → adapter → encoder.
2. `BT_TRIBE` body byte-equality 28/28.
3. Unit adapters/encoders/invalid paths.
4. Полный `ctest` с `GL_GAME_DIR` зелёный.
5. Docs D-080/STATUS/PROJECT/parity/research; no new-global claim.

## 8. Risks

- Only saved names are updated; runtime-created globals require a policy
  in c4.
- Type field comes from `SaveVar`; VM variant type may differ for unknown
  names (kept raw).
- Byte-equality relies on exact int/uint/double→f64 conversion and
  lossless cp1251 names; the oracle verifies 28/28.
- Type 3 is absent from the corpus (unit-only).
- `encode_tribe_body` assumes entry-order rows (b3 contract).

## 9. References

- `docs/research/p2-15a-save-payload.md` §`BT_VARS `/§`BT_TRIBE`;
  `docs/research/p2-15b4-diary-hstry.md` (typed-block boundaries).
- Specs/plans b2/b3/c1; D-075/D-076/D-079.
