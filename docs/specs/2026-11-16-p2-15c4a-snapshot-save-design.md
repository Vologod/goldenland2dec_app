# P2-15c4a — Snapshot Save Write Design

Status: approved (2026-11-16)
Phase: P2-15c4a (file write; snapshot only)
Layers: `assets` (header patch) + `core` (file write) + `tools/glgame`

## 1. Goal

Записать загруженный `SavePayload` в валидный `.sav` по явному пути:
typed VARS/TRIBE (c2), HERO/tail/ENTS raw (как загружены), header
date/title patched. Реальные `Saves/` не перезаписываются без явного
пути. Это snapshot: изменения HERO/мира ещё не сериализуются (`c3`),
слоты/`game_save` — `c4b`.

## 2. Evidence

- Container/encoder: c1 (`encode_save_payload`, D-079); VARS/TRIBE typed
  encoders: c2 (D-080).
- Header layout (`parse_save_header`, `save.cpp:24-46`): magic 20 B;
  `u32 version` +0x14; `u16 year` +0x18, month +0x1a, weekday +0x1c,
  day +0x1e, hour +0x20, minute +0x22, second +0x24 (SYSTEMTIME order:
  the parser currently skips the weekday word); title cp1251 64 B at
  +0x28..+0x67.
- Writer `FUN_14031b64` uses `GetLocalTime` and a save name (35375-35396);
  header_raw preserves unknown bytes, only date/name are updated here.
- `glgame --load-save` already parses the full payload; store it for the
  later write.

## 3. Scope

Входит:

- `SaveHeader.weekday` (parse the SYSTEMTIME weekday word).
- `assets::patch_save_header(SavePayload&, const SaveHeader&)`.
- `core::write_save_file(path, span<const uint8_t>)` (tmp+rename).
- `glgame --write-save <path>` + `--save-title <text>`; keeps the loaded
  `SavePayload`; logs `save write: path=<...> bytes=<n>`.
- Unit header patch, core write, E2E temp-file parse/equality.
- D-081; STATUS/PROJECT/parity/research.

Не входит:

- Запись `Saves/slot<N>.sav`/`game_save`/UI (c4b).
- Typed HERO/tail/ENTS/world state (c3), new/runtime globals.
- Byte-identical-with-original; original-game load is an optional manual
  gate.

## 4. API

`src/assets/include/gl/assets/save.hpp`:

```cpp
struct SaveHeader {
    ...
    std::uint16_t weekday = 0;
};
VoidResult patch_save_header(SavePayload &payload, const SaveHeader &header);
```

(`patch_save_header` lives in `save_payload.hpp`/`save_payload.cpp`.)

`src/core/include/gl/core/save_manager.hpp`:

```cpp
gl::VoidResult write_save_file(const std::filesystem::path &path,
                               std::span<const std::uint8_t> bytes);
```

`glgame` CLI:

```text
--write-save <path>   Write the loaded save payload to <path>
--save-title <text>   Title for --write-save (default "Quick save")
```

## 5. Rules

`patch_save_header`:

1. Title encoded cp1251; `> 64` bytes → `ErrorCode::format`.
2. `version` u32 at +0x14; `year/month/weekday/day/hour/minute/second`
   u16 at +0x18/+0x1a/+0x1c/+0x1e/+0x20/+0x22/+0x24; title zero-filled
   64 B at +0x28.
3. Other `header_raw` bytes unchanged.

`write_save_file`:

1. Create parent directories (if any); error → `ErrorCode::io`.
2. Write `path + ".tmp"`, flush/close; error → `io`.
3. Rename tmp → path; if rename fails because the target exists, remove
   the target and retry once; on failure remove tmp and return `io`
   (best-effort atomicity documented).

`glgame`:

1. `load_save` success stores a copy of the payload in `loaded_payload_`;
   failure keeps the previous payload.
2. After a successful `--load-save`, if `--write-save` is set: copy the
   payload, set `version` unchanged, date/weekday/time from local clock,
   title from `--save-title`; `patch_save_header`; `encode_save_payload`;
   `write_save_file`; log `save write: path=<...> bytes=<n>`.
3. `--write-save` without a loaded payload → error log and exit 4.
4. No implicit write to `GL_GAME_DIR/Saves`.

## 6. Tests

Unit (`tests/assets/test_save_payload.cpp`):

- `patch_save_header`: build a parsed payload, patch with fixed
  version/date/weekday/time/title `"TestSave"`, parse header from
  `header_raw` (or `parse_save_header` on encoded file) and compare;
  title > 64 → `format`.
- `SaveHeader.weekday` parse: synthetic header with weekday word, compare.

Core (`tests/core/test_save_manager.cpp`):

- `write_save_file` writes bytes, reads them back, overwrites an existing
  file, and leaves no `.tmp`; missing parent is created.

App (`tests/app/test_glgame_screenshot.cpp`):

- `--load-save 0 --write-save <temp>/out.sav --save-title "TestSave"`:
  code 0, log `save write: path=`, file exists; `parse_save_payload`
  returns level `L93_4`, title `TestSave`, `header.weekday` in 0..6,
  date fields equal the local clock; `decompressed` equals the source
  slot0 payload; VARS/TRIBE bodies equal (typed encoders used).

## 7. Acceptance

1. Written file parses with `parse_save_payload`; all block bodies equal
   the loaded source; header title/date/weekday patched.
2. `write_save_file` unit (create/overwrite/no-tmp).
3. `--write-save` E2E on a temp path; no writes to real `Saves/`.
4. Полный `ctest` с `GL_GAME_DIR` зелёный.
5. Docs D-081/STATUS/PROJECT/parity/research; snapshot limitation stated.

## 8. Risks

- Snapshot only: HERO/tail/ENTS/world state are re-emitted from the
  loaded raw bytes, not current simulation (`c3`).
- Date fields map to SYSTEMTIME order; `time_a/b/c` semantics are inferred
  from that layout (OQ-3 remains).
- Non-atomic overwrite fallback on Windows (remove+rename) in a tiny
  window.
- Title is capped at 64 cp1251 bytes; unrepresentable characters are
  substituted by the cp1251 helper.
- No slot UI/`game_save` yet (`c4b`).

## 9. References

- `docs/research/p2-15a-save-payload.md` §Writer/§P2-15c;
  `src/assets/src/save.cpp`; D-079/D-080.
