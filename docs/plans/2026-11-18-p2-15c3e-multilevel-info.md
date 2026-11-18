# P2-15c3e — Multi-level tails and BT_INFO level id (plan)

Spec: `docs/specs/2026-11-18-p2-15c3e-multilevel-info-design.md`
Status: implemented (2026-11-18)

## Preconditions

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Task 1 — `SaveInfo` + `encode_info_body` (assets)

TDD:

1. `tests/assets/test_info_body.cpp`: session/description parse/encode,
   synthetic round-trip, empty level id, oversized strings.
2. Add `session`/`description`; store them in `parse_save_payload`.
3. Implement `encode_info_body` and `update_info_block`.
4. Real: add to `tests/assets/test_save_payload_real.cpp`:
   `encode_info_body(payload.info) == block[1] body` for 28 saves.
5. Acceptance: unit pass; real 28/28 byte-equal.

## Task 2 — Tail section entry parser + upsert + current level

TDD:

1. `tests/assets/test_tail_index.cpp`:
   - `parse_tail_section_entry` on an encoded section returns fields/bytes;
   - malformed/trailing section → `format`;
   - `upsert_tail_entry` replace keeps order and updates `level_tail_index`;
   - append adds a new entry;
   - `set_current_level` updates info and index; missing entry → `format`.
2. Expose `parse_tail_section_entry` (wrapper over the static parser).
3. Implement `upsert_tail_entry`, `set_current_level`.
4. Real (same file or `test_save_payload_real.cpp`): for each slot,
   upsert every entry with itself and verify the tail vector is unchanged;
   level switch slot0→slot2 name, `update_info_block`, encode→parse: new
   level id/index/section and old entry preserved; add a synthetic
   `NEW_LEVEL` entry and verify count+1 after encode→parse.
5. Acceptance: unit pass; real identity/switch/add pass.

## Task 3 — Verification, docs, review

1. Focused:
   ```powershell
   build\bin\gl_assets_tests.exe --test-case="*info_body*,*tail_index*"
   build\bin\gl_assets_real_tests.exe --test-case="*BT_INFO*,*tail index*"
   ```
2. Full `ctest --test-dir build --output-on-failure`.
3. Update `STATUS`, `PROJECT`, `DECISIONS` (D-086); residuals (append order,
   calendar, c3f).
4. Reviewer; fix Critical/Important; UTF-8 no BOM/NUL; `git diff --check`;
   commit.
