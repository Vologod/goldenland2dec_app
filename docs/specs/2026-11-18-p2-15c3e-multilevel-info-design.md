# P2-15c3e — Multi-level tails and `BT_INFO` level id (design)

Status: implemented (2026-11-18)
Task: `P2-15c3e` (library + tests; follow-up to P2-15c3c, D-085)
Scope: `src/assets`, `tests/`; no CLI/level-transition changes (c3f/P6).

## 1. Context

`encode_save_payload` writes the tail dictionary entries verbatim and copies the
`BT_INFO ` body from the loaded decompressed stream. A save after a level
change needs:

- the old level's tail snapshot stored in the dictionary,
- the new level's entry added/updated,
- `BT_INFO ` re-encoded with the new level id, while preserving the session
  and description strings, `+0x218`, the 0x20 time blob, party level and the
  8-byte `+0x26c8` tail.

The original writer (`FUN_14031b64` 35386-35396) writes `BT_INFO ` as
`string session, u32 +0x218, 0x20 time, string description, string level_id,
u32 party_level, 8B +0x26c8`. The parser reads exactly that order (507-528)
but currently discards `session` and `description`.

## 2. Goal

Assets-only API to:

1. re-encode `BT_INFO ` byte-identically from `SaveInfo`;
2. parse one tail section into a `SaveTailEntry`;
3. upsert a level entry (replace by name or append);
4. switch the current level id and selected-tail index.

## 3. API

### 3.1 `SaveInfo`

Add `std::string session; std::string description;`; `parse_save_payload`
stores them. No other field changes.

### 3.2 `encode_info_body(const SaveInfo&)`

Writes the exact order above (cp1251 length-prefixed strings like
`FUN_1400116c`). Requires a non-empty level id; rejects sizes/u32 overflow.
`update_info_block(SavePayload&)` = `replace_block_body(payload, 1,
encode_info_body(payload.info))`.

### 3.3 `parse_tail_section_entry(std::string_view name, span section)`

Public wrapper over the existing section parser; returns `SaveTailEntry` with
`name`, `section`, `raw_len`, `rginf`, `person_names`, `entities`; exact
consume (PNAME/RGINF/ENTS framing).

### 3.4 `upsert_tail_entry(SavePayload&, SaveTailEntry)`

- `name` non-empty, `section` non-empty, `raw_len == section.size()`
  (or sets `raw_len`);
- existing name → replace in place (dictionary order preserved);
- new name → append;
- update `level_tail_index` to the entry index; return the index.

### 3.5 `set_current_level(SavePayload&, std::string level_id)`

Sets `info.level_id` and `level_tail_index` to the matching tail entry;
returns `format` if no entry exists. Does not rebuild the block; the caller
calls `update_info_block` before encoding.

## 4. Ordering / semantics

- Tail dictionary order = loaded order; new levels append. Original writer
  dumps the map order, which is not recoverable from the save; append is a
  documented non-claim.
- Replacing an entry keeps its position.
- `BT_INFO ` session/description are preserved; only `level_id` changes.
- `BT_RGINF`/`BT_PNAME`/`BT_ENTS` inside a new section are produced by
  `P2-15c3c`'s `build_selected_tail`; c3e only stores/serialises.

## 5. Error handling

- Empty level id, empty tail name, empty section, `raw_len` mismatch,
  non-u32 size, malformed section → `format`.
- `set_current_level` without a matching entry → `format`.
- No partial mutation before validation.

## 6. Acceptance

1. `encode_info_body(parse(payload).info) == BT_INFO body` on 28/28 saves.
2. Upsert of every loaded tail entry with itself leaves `payload.tail`
   unchanged; `encode→parse` preserves names/sections.
3. Level id change + `update_info_block` + `encode_save_payload` → parse
   returns the new `info.level_id`, `level_tail_index`, same section, and the
   old level entry still present.
4. Adding a synthetic new level entry increases the tail count by one and
   survives encode→parse.
5. Full `ctest`; no CLI changes.

## 7. Tests

- `tests/assets/test_info_body.cpp` — synthetic round-trip/truncation.
- `tests/assets/test_tail_index.cpp` — parse/upsert/switch unit cases.
- `tests/assets/test_save_payload_real.cpp` — 28/28 BT_INFO byte identity,
  upsert identity, level change and new-level round-trip.

## 8. Non-goals / non-claims

- No automatic level transitions (P6/c3f).
- Dictionary order for new levels (append) is a port choice.
- No calendar/time update (`BT_INFO` 0x20, c3g).
- No `BT_SCRN ` refresh.
