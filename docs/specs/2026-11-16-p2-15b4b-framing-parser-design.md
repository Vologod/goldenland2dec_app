# P2-15b4b — BT_DIARY / BT_HSTRY Framing Parser Design

Status: approved (2026-11-16)
Phase: P2-15b4b (framing only; after b4a research)
Layers: `assets` (parser/model) + `tools/glgame` (dump)

## 1. Goal

Разобрать `BT_DIARY` и `BT_HSTRY` структурно и сохранить raw-байты:
DIARY entries (`key`, declared count, header+records 0x58) и sparse
records; HSTRY `head`, 40 слотов `(text, id)`, `tail`. Поля 0x58 и
ring-семантика не интерпретируются. Точный consume 28/28, E2E dump,
real-oracle. Это также даёт P2-15c raw round-trip.

## 2. Evidence

`docs/research/p2-15b4-diary-hstry.md` (D-077) и
`p2-15a-save-payload.md` §`BT_DIARY`/§`BT_HSTRY`:

- DIARY writer `FUN_14058768` 60171-60226, reader `FUN_140588c8`
  60232-60358; `u32 entry_count`, per entry `u32 block_count =
  records+1` + `block_count*0x58`, затем `u32 sparse_count +
  sparse_count*0x58`. Flatten `FUN_14058b1c` ставит header `+0x04=-1`;
  record — 22 dword, копируются verbatim (писатель/читатель не
  интерпретируют поля). 28 saves: 1,013 entries, 4,855 blocks
  (1,013 headers + 3,842 records), sparse 0, exact consume; slot0
  76/371, slot2 75/377, slot21 1/2.
- HSTRY body: `u32 head`, 40 × (`u32 text_len + cp1251 + u32 id`),
  `u32 tail`; 28/28 exact; head 1..37; tail 1 в 26 saves / 2 в 2;
  все 1,120 ids `0`. Host methods в `GoldenLand.exe` → ring semantics
  blocked.
- Port currently skips both bodies; no data model.

## 3. Scope

Входит:

- `SaveBlock58`/`SaveDiaryEntry`/`SaveHistorySlot`/`SaveHistory` в
  `SavePayload`; `diary`, `diary_sparse`, `history`.
- `parse_diary`, `parse_history`; exact consume; malformed → `format`.
- `glgame --dump-save`: `diary=<n> diary_blocks=<m> sparse=<k>
  hstry_slots=40`.
- Unit/real/EE tests; D-078; STATUS/PROJECT/parity/research.

Не входит:

- Интерпретация 0x58 полей и pointer-like значений, sparse semantics,
  HSTRY ring/head/tail порядок, journal UI, save write (P2-15c),
  live-oracle, host `GoldenLand.exe`.

## 4. API

`src/assets/include/gl/assets/save_payload.hpp`:

```cpp
using SaveBlock58 = std::array<std::uint8_t, 0x58>;

struct SaveDiaryEntry {
    std::uint32_t key = 0;
    std::uint32_t declared_count = 0;
    std::vector<SaveBlock58> blocks;
};

struct SaveHistorySlot {
    std::string text;
    std::uint32_t id = 0;
};

struct SaveHistory {
    std::uint32_t head = 0;
    std::array<SaveHistorySlot, 40> slots;
    std::uint32_t tail = 0;
};
```

`SavePayload` += `std::vector<SaveDiaryEntry> diary;
std::vector<SaveBlock58> diary_sparse; SaveHistory history;`.

## 5. Parser rules

DIARY:

1. `u32 entry_count`.
2. Per entry: `u32 block_count`; `block_count == 0` → `format`
   (`BT_DIARY entry <i> has no header`); `block_count*0x58` bytes
   available, иначе `format`; copy в `blocks`.
3. `key = u32(block0+0x00)`, `declared_count = u32(block0+0x04)`.
   `declared_count` не сверяется с `blocks.size()-1` (оригинал копирует
   без проверки; corpus invariant проверяется оракулом).
4. `u32 sparse_count`; `sparse_count*0x58` bytes, иначе `format`;
   copy в `diary_sparse`.
5. Exact end; trailing → `format`.

HSTRY:

1. `u32 head`.
2. Ровно 40 слотов: `u32 len`, `len` байт cp1251 → UTF-8,
   `u32 id`; усечение → `format`.
3. `u32 tail`; exact end; trailing → `format`.

`--dump-save` добавляет в конец:
` diary=<diary.size()> diary_blocks=<sum blocks> sparse=<diary_sparse.size()> hstry_slots=40`.
Существующие поля/порядок не меняются.

## 6. Tests

Unit (`tests/assets/test_save_payload.cpp`):

- diary synthetic: 2 entries (0 records, 2 records), sparse 1; key/
  declared_count/blocks bytes/sparse; header count consistency;
- diary malformed: `block_count=0`, truncated blocks, truncated sparse,
  trailing → `format`;
- history synthetic: `head=7`, 40 slots (empty + cp1251 text), ids,
  `tail=2`; cp1251→UTF-8; truncated slot, 39 slots, trailing →
  `format`;
- existing `make_body` empty DIARY/HSTRY заменяются валидными пустыми
  (`diary: count 0 + sparse 0`; `history: head 0 + 40 empty slots +
  tail 0`).

Real (`tests/assets/test_save_payload_real.cpp`):

- 28 saves: diary entries `1013`, blocks `4855`, records `3842`, sparse
  `0`, `declared_count == blocks.size()-1` 0 mismatches, exact consume;
  slot0 `76/371`, slot2 `75/377`, slot21 `1/2`; slot0 first entry
  `key=0 declared_count=4 blocks=5`; slot21 `key=482 declared_count=1
  blocks=2`.
- history: 40 slots 28/28; head `1..37`; tail `1`×26/`2`×2; all ids 0
  (`1120/1120`); slot0 `head=5`, `tail=1`, 40 non-empty texts.

App (`tests/app/test_glgame_screenshot.cpp`):

- `--dump-save 0` asserts `diary=76 diary_blocks=371 sparse=0
  hstry_slots=40`; existing substrings unchanged.

## 7. Acceptance

1. DIARY/HSTRY exact consume 28/28; totals/pins above.
2. Malformed framing rejected with `ErrorCode::format`.
3. E2E dump field; full `ctest` with `GL_GAME_DIR` green.
4. Docs: D-078, STATUS/PROJECT/parity/research; 0x58/ring semantics
   remain unverified.

## 8. Risks

- 0x58 fields are opaque/pointer-like; parser stores raw only.
- `declared_count` mismatch is not rejected (original copies without
  validation); corpus invariant is oracle-only.
- HSTRY slot count is fixed at 40; head/tail order is not applied.
- No journal UI/quest model; no runtime application.
- P2-15c must write diary blocks and history slots back verbatim.

## 9. References

- `docs/research/p2-15b4-diary-hstry.md`;
  `docs/research/p2-15a-save-payload.md` §`BT_DIARY`/§`BT_HSTRY`;
- D-077; specs/plans P2-15b3/b4a.
