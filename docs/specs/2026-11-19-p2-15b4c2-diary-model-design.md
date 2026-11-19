# P2-15b4c2 — Read-only DIARY header model (design)

Status: implemented (2026-11-19)
Task: `P2-15b4c2` (follow-up to P2-15b4c/b4c1, D-091/D-092)
Scope: `src/game`, `tests/`; no UI, no write.

## 1. Context

D-091 mapped the DIARY header block; D-092 blocked the record payload.
This task exposes the header fields as a read-only model for a future
journal UI and for the c3d datetime/flag write, while keeping every record
block raw.

## 2. API (`gl/game/diary.hpp`)

```cpp
struct DiaryEntry {
    std::uint32_t key = 0;
    std::uint32_t record_count = 0;
    std::uint32_t end_time_flag = 0;
    std::uint32_t status = 0;
    std::uint32_t text_id = 0;
    GameDatetime start;                 // +0x14..+0x30
    std::optional<GameDatetime> end;    // +0x34..+0x50, iff flag == 1
    std::vector<SaveBlock58> blocks;    // raw header + records
};
Result<std::vector<DiaryEntry>> decode_diary_entries(const SavePayload &payload);
```

## 3. Semantics

- `blocks[0]` is the header; `+0x04` must equal `blocks.size()-1` (D-091).
- `+0x08` is `end_time_flag`; `1` ⇒ parse datetime B, `0` ⇒ `end = nullopt`
  and B must be zero (D-091 invariant); other values ⇒ `format`.
- `+0x00` key (must equal the parser entry key), `+0x0c` status, `+0x10`
  text id.
- Datetime A always parsed (8×u32, D-088 layout); invalid values → `format`.
- Record blocks (`blocks[1..]`) are copied raw; no interpretation.
- The payload `diary_sparse` is not part of this model (sparse is 0 in the
  corpus; keep raw).

## 4. Acceptance

1. 28 saves / 1,013 entries decode; key/count/flags/text id match the corpus
   scan; datetime A valid 1,013/1,013; `end` present 797/1,013 and exactly
   iff flag 1; key 0..550, status 0/1, text id 180..510.
2. Header count/key mismatch, unknown flag, invalid A/B datetime, non-zero B
   with flag 0, zero blocks → `format`; truncated blocks are rejected by
   `parse_save_payload`.
3. Raw blocks are byte-equal to the payload after decode.
4. Full `ctest`; no write/UI.

## 5. Non-goals

- Record payload semantics (blocked, b4c1a/b4c1b).
- DIARY write (c3d), sparse records, HSTRY, journal UI.
