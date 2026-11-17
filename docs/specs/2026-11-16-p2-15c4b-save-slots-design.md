# P2-15c4b — Save Slots and `game_save` Design

Status: approved (2026-11-16)
Phase: P2-15c4b (slot file write + console command)
Layers: `core` (slot path) + `tools/glgame`

## 1. Goal

Дать запись snapshot-сохранения в слот `<save-dir>/slot<N>.sav`:
`glgame --save-dir <dir>` переопределяет каталог сохранений (list/load/
dump/write/menu), `--write-save-slot <n>` пишет после load, console
`game_save <n>` пишет текущий загруженный payload и обновляет список
слотов. Snapshot-ограничение c4a сохраняется; UI save menu — c4c.

## 2. Evidence

- c4a: `encode_save_payload`, `patch_save_header`, `core::write_save_file`,
  `glgame --write-save <path>`; snapshot boundaries.
- `SaveManager::scan(dir)` already supports an arbitrary save directory
  (`src/core/src/save_manager.cpp:80`); `resolve_save_slot` currently
  hardcodes `game_dir/Saves` (`tools/glgame/main.cpp:1998`).
- `--exec "<command>"` runs console commands after load
  (`main.cpp:768-770`), so `game_save` can be tested headlessly.
- Slot file name `slot<N>.sav` is `slot_index_from_filename`'s contract
  (`save_manager.cpp:62-78`).

## 3. Scope

Входит:

- `core::save_slot_path(save_dir, slot)`.
- `glgame --save-dir <dir>` (default `<game-dir>/Saves`) used by
  `--list-saves`, `--load-save`, `--dump-save`, slot write, menu scan.
- `--write-save-slot <n>` (snapshot write, title from `--save-title`).
- console `game_save <n>` (same write, refreshes slots, returns a message).
- Tests: core path, E2E temp save-dir (copy slot0, write slot1/slot2,
  originals and real `Saves/` unchanged, written file parses).
- D-082; docs.

Не входит:

- UI save menu (c4c), live state serialization (c3), slot limits/замена
  политики (любой N≥0), `game_load` console command, implicit writes.

## 4. API

`src/core/include/gl/core/save_manager.hpp`:

```cpp
std::filesystem::path save_slot_path(const std::filesystem::path &save_dir, int slot);
```

`glgame` CLI:

```text
--save-dir <dir>       Save directory (default <game-dir>/Saves)
--write-save-slot <n>  Write the loaded snapshot to <save-dir>/slot<n>.sav
```

Console:

```text
game_save <n>          Write the loaded snapshot to slot <n>
```

## 5. Rules

1. `save_slot_path(dir, slot)` = `dir / ("slot" + std::to_string(slot) + ".sav")`;
   negative slots are rejected by the callers (`argument` error).
2. `--save-dir` sets the resolved directory for list/load/dump/write and
   the `SaveManager` scan; the default remains `<game-dir>/Saves`.
3. `--write-save-slot <n>` after a successful `--load-save` patches the
   header (date/weekday/title, as c4a), encodes, writes via
   `write_save_file`, logs `save write: path=<...> bytes=<n>`, rescans the
   save directory and rebuilds the menu list. Without a loaded payload →
   `save write: no loaded save payload` + exit 4.
4. `game_save <n>` requires exactly one non-negative integer argument and a
   loaded payload; on success returns
   `game_save: slot <n> written (<bytes> bytes)` and rescans; errors are
   `argument`/`io` results without partial state.
5. No implicit writes; real `Saves/` is touched only when the user passes
   that directory explicitly (or runs `game_save` in an interactive session
   with the default directory — a deliberate user action).

## 6. Tests

Core (`tests/core/test_save_manager.cpp`):

- `save_slot_path(dir, 0)` / `(dir, 21)` names.

App (`tests/app/test_glgame_screenshot.cpp`):

- Temp save dir: copy the real `Saves/slot0.sav` into it; record its
  bytes; run `--save-dir <temp> --load-save 0 --write-save-slot 1
  --save-title "SlotOne"`; assert code 0, `save write: path=`, temp
  `slot1.sav` parses with title `SlotOne` and the same decompressed
  bodies, temp `slot0.sav` bytes unchanged, and the real
  `Saves/slot0.sav` bytes unchanged.
- Console: `--save-dir <temp> --load-save 0 --exec "game_save 2"`; assert
  temp `slot2.sav` exists, parses, and output contains
  `game_save: slot 2 written`.
- `--write-save-slot 1` without `--load-save` → exit 4 + log.

## 7. Acceptance

1. Slot write produces a parseable snapshot with the same block bodies.
2. `--save-dir` redirects all save operations; real saves untouched by
   tests.
3. Console `game_save <n>` works after load and refreshes slots.
4. Полный `ctest` с `GL_GAME_DIR` зелёный.
5. Docs D-082/STATUS/PROJECT/parity/research; snapshot boundary stated.

## 8. Risks

- Snapshot only (c3), UI save menu c4c.
- Any N≥0 allowed; overwriting is the caller's choice; no slot cap.
- Non-ASCII paths use `path_from_utf8`; log uses narrow `.string()`.
- `game_save` in an interactive session with the default directory
  overwrites a real slot — deliberate user action, not a test path.
- Temp+rename best-effort atomicity (c4a).

## 9. References

- c4a spec/D-081; `src/core/src/save_manager.cpp`; `tools/glgame/main.cpp`
  (`--load-save`, `--exec`, registry).
