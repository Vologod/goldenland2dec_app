# P2-15b1a — Save entities parser Design

Status: implemented (2026-11-14)
Phase: P2-15b1a (implementation; first increment of full P2-15b)
Layers: `assets` only

## 1. Goal

Расширить `gl::assets::SavePayload`: разобрать `BT_ENTS` всех tail-секций
(type + body), `BT_PNAME` (names), `BT_RGINF` (8×u32), с точным
потреблением 2599/2599 секций. Restore в симуляцию не входит (b1b).

## 2. Evidence

`docs/research/p2-15b1-save-entities.md`:

- 28 saves, 2599 tail sections; type histogram `{2:14507, 8:354, 16:12090,
  32:2591}`; type 1 terminator; type 4 никогда не сериализуется.
- Body formulas:
  - type 2: `540 + (n1+n2+n3+n4) + M1+M2+M3 + 0x14*c`; min 552; tail
    `0x84 + 0x14*c`; containers via `FUN_1405ca18`/`FUN_1404c890`.
  - type 8: `8 + M`; min 12.
  - type 0x10: `32 + (n1+n2+n3+n4) + M`; min 36.
  - type 0x20: `28 + (n1+n2+n3)`; min 28.
  - `M = 4` if flag==0; else `8 + sum(0xc0 + 4 + 0x14*m_i)`.
  - strings: `u32 n + min(n,0x104)` bytes (retail n<=63).
- `BT_PNAME`: zero-terminated `u32 len + cp1251` names, count == type-2
  persons 2599/2599; terminator zero-length entry.
- `BT_RGINF`: 36 B = u32 latch + 8×u32 datetime
  (year,month,day,weekday,hour,min,sec,ms).

## 3. API

`src/assets/include/gl/assets/save_payload.hpp`:

```cpp
struct SaveEntity {
    std::uint32_t type = 0;
    std::vector<std::uint8_t> body;
};

struct SaveRginf {
    std::uint32_t latch = 0;
    std::array<std::uint32_t, 8> datetime{};
};

struct SaveTailEntry {
    std::string name;
    std::uint32_t raw_len = 0;
    std::vector<std::uint8_t> section;
    SaveRginf rginf;
    std::vector<std::string> person_names;
    std::vector<SaveEntity> entities;
};
```

`SavePayload` retains `rginf`/`ents` raw (b1b), and `level_tail_index`.
Accessors are the existing fields (`tail`, `level_tail_index`).

`SaveEntity.body` contains only the type-specific body bytes; the leading
4-byte `u32 type` word is stored separately in `SaveEntity.type` and is not
part of `body`.

Parser behavior:

- At `parse_save_payload`, after decompressing each tail section, parse:
  1. `BT_RGINF`: require 36 B; latch + 8 u32.
  2. `BT_PNAME`: read `u32 len + cp1251` until zero-length;
     require exact section/tag consumption rules from research.
  3. `BT_ENTS`: read `u32 type`; type 1 terminates; map type to body size;
     copy body; continue until terminator; require consumed == tag body len.
- Unknown type / truncated body / missing terminator / RGINF != 36 →
  `ErrorCode::format` (whole payload fails).
- PNAME == type-2 count is a research invariant; parser does NOT fail on
  mismatch (original does not enforce it), but tests assert it.
- `BT_SCRN` remains skipped; no other block changes.

## 4. Tests

Unit (`tests/assets/test_save_payload.cpp`):

- synthetic tail with RGINF + PNAME (2 names + terminator) + one minimal
  type-8 entity (12 B) + terminator; assert rginf/names/entities.
- type 0x20/0x10/type2 minimal valid bodies (formula-built), strings/containers.
- malformed: unknown type, truncated body, missing terminator, RGINF wrong
  size, PNAME unterminated → `format` error.
- existing synthetic tail fixtures updated to valid bodies.

Real (`tests/assets/test_save_payload_real.cpp` extension or new file):

- all 28 selected sections and all 2599 tail sections parse exactly:
  histogram `{2:14507,8:354,16:12090,32:2591}`, terminator 2599/2599,
  consumed==body length, PNAME==type2 2599/2599, RGINF datetime non-zero
  where expected.
- slot0/slot2 pinned selected-section entity/pname counts.

App (`glgame --dump-save`):

- line extended with `entities=<n> pnames=<n> rginf_latch=<u32>` for the
  selected level; E2E test updated with reference values.

## 5. Acceptance

1. All 2599 sections parse exactly (0 mismatches), histogram matches.
2. PNAME==type2 2599/2599; RGINF fields populated.
3. Synthetic malformed cases return `format`, no crash/overflow.
4. `--dump-save` extended line and E2E test pass.
5. Full `ctest` with `GL_GAME_DIR`; docs/STATUS/parity.

## 6. Out of scope

- Restore persons/entities into simulation (b1b).
- Named field decoding beyond RGINF/type/body (many fields unverified).
- Inventory item-record semantics (`0xC0` sub-array) — not interpreted.
- `BT_DIARY`/`BT_HSTRY`, `BT_VARS` values, `BT_TRIBE`, save write.

## 7. Risks

- Body formulas depend on string cap semantics; retail n<=63, but parser
  must still handle `min(n,0x104)` correctly.
- PNAME zero-termination is empirical; parser follows 2599/2599 evidence.
- Type-4 forged record: parser rejects as unknown type (documented).
- Large entity bodies (max 138768) — use bounded reads, no unbounded alloc
  beyond stream length.

## 8. References

- `docs/research/p2-15b1-save-entities.md` (types/body/parse/RGINF/PNAME).
- `docs/specs/2026-11-13-p2-15b1-entities-research-design.md`.
- `docs/specs/2026-11-12-p2-15b-save-load-design.md`.
