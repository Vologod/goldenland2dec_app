# P2-15c1 — Save Container Writer Design

Status: approved (2026-11-16)
Phase: P2-15c1 (first save-write increment; library only)
Layers: `assets`

## 1. Goal

Собрать корректный `.sav` из разобранного `SavePayload`: header 0x68,
zlib main stream из raw-блоков, tail-словарь; round-trip
decode-equality на 28 saves и детерминизм. Без записи файлов/CLI
(`P2-15c4`), без typed-перекодирования VARS/TRIBE/HERO (`c2`/`c3`).

## 2. Evidence

`docs/research/p2-15a-save-payload.md` §Writer/§Reader/§Container:

- Файл: `0x68` (104) байта header, затем `u32 raw_len` (`0x68`), `u32
  compressed_len` (`0x6c`); header-регион `0x70` (112 байт), zlib(main
  stream) с `0x70`, затем tail dictionary.
- Main stream: 7 блоков в подтверждённом порядке `BT_SCRN `, `BT_INFO `,
  `BT_VARS `, `BT_TRIBE`, `BT_DIARY`, `BT_HSTRY`, `BT_HERO `; каждый
  `tag(8) + u32 size + body`, без padding, total = decompressed length;
  writer order = on-disk order 28/28.
- Tail dictionary: `u32 count`; per entry `u32 name_len + cp1251 name`,
  `u32 section_len`, body `u32 raw_len + u32 zlib_len + zlib(section)`
  (`section_len = 8 + zlib_len`); selected level = `BT_INFO` level id.
- Header writer `FUN_14031b64` 35386-35405; приёмка P2-15c —
  read→decode→compare, детерминизм; byte-identical с оригиналом не
  требуется (zlib level/implementation могут отличаться).
- Parser `parse_save_payload` already stores `decompressed`, `blocks`
  (offset/size/tag), `tail[].section` (raw uncompressed), `raw_len`.

## 3. Scope

Входит:

- `SavePayload.header_raw` (`std::array<std::uint8_t,0x68>`) — первые
  `0x68` (104) байта файла, за которыми идут два `u32` слова длин;
  копируются парсером.
- `assets::encode_save_payload(const SavePayload&)` ->
  `Result<std::vector<std::uint8_t>>`.
- Unit: synthetic round-trip/determinism/invalid payload.
- Real: 28-save decode-equality + determinism.
- Docs: D-079, STATUS/PROJECT/parity/research.

Не входит:

- Запись `Saves/slot*.sav`, `game_save`, UI/слоты — `P2-15c4`.
- Перекодирование `BT_VARS`/`BT_TRIBE`/`BT_HERO`/tail из игрового
  состояния — `c2`/`c3`.
- Byte-identical с оригинальным файлом; `BT_SCRN` генерация;
  изменение header даты/имени (c4 должен обновлять 0x68-заголовок).

## 4. API

`src/assets/include/gl/assets/save_payload.hpp`:

```cpp
struct SavePayload {
    ...
    std::array<std::uint8_t, 0x68> header_raw{};
};

Result<std::vector<std::uint8_t>> encode_save_payload(const SavePayload &payload);
```

`parse_save_payload` дополнительно копирует `file.subspan(0, 0x68)` в
`payload.header_raw`.

## 5. Encoding rules

1. Validate: `blocks.size() == 7`; для `i` `blocks[i].tag ==
   kBlockOrder[i]`; `offset + size <= decompressed.size()`; каждый tail
   entry имеет `raw_len == section.size()` и `raw_len > 0`.
2. Main raw stream = concat `decompressed[offset .. offset+size)` в
   порядке блоков (raw bytes, no rewrite).
3. `mz_compress` (default level); `compressed_len` from the call;
   `raw_len` = raw stream size (must fit `u32`).
4. Output = `header_raw` (`0x68`/104 B) followed by the two `u32` length
   words: `u32 raw_len` at `0x68`, `u32 compressed_len` at `0x6c`,
   compressed stream, then
   `u32 tail_count` and per entry:
   `u32 name_len + cp1251(name)`,
   `u32 8 + zlib_len`,
   `u32 raw_len`,
   `u32 zlib_len`,
   `zlib(section)`.
5. Errors: `ErrorCode::format` for invalid payload/compression/allocation
   failures. No file I/O.

## 6. Tests

Unit (`tests/assets/test_save_payload.cpp`):

- parse a `make_save` payload; `encode_save_payload`; parse the result;
  compare `header_raw`, `decompressed`, block tags/bodies, tail
  names/sections/raw_len, `info`, vars/diary/history;
- encode twice → identical bytes;
- invalid payload: 6 blocks; wrong tag order; offset+size > decompressed;
  tail `raw_len != section.size()` → `format`.

Real (`tests/assets/test_save_payload_real.cpp`):

- 28 saves: parse → encode → parse; 0/N compare `decompressed` bytes,
  header_raw (`0x68`/104 B), tail names + section bytes + raw_len, info fields,
  vars count/names/raw values, diary/history/ents counts;
- encode twice byte-identical;
- output length >= `0x70 + compressed_len` and header lengths consistent
  with the parsed result.

## 7. Acceptance

1. Decode-equality 28/28 (raw blocks/tail/header/fields).
2. Deterministic encode (same input → same bytes).
3. Invalid payload rejected with `ErrorCode::format`.
4. Полный `ctest` с `GL_GAME_DIR` зелёный.
5. Docs: D-079/STATUS/PROJECT/parity/research; no byte-identical claim.

## 8. Risks

- Recompressed zlib bytes may differ from the original file; only
  decode-equality is claimed.
- `header_raw` preserves unknown/date/title fields; c4 must patch the
  header (date/name) when creating a new save.
- Typed VARS/TRIBE/HERO re-encoding is later (c2/c3); the current writer
  re-emits raw bytes, so gameplay mutations are not yet serialized.
- Tail sections are recompressed from `section`; if a future component
  edits sections, `raw_len` must be kept in sync.

## 9. References

- `docs/research/p2-15a-save-payload.md` §Writer/§Reader/§P2-15c.
- `docs/specs/2026-11-12-p2-15b-save-load-design.md`; D-066/D-078.
