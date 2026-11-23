# P6a8 — Person dialog/injury natives (design)

Status: approved (2026-11-23)
Scope: `Server.dll` natives `0x700000E RS_GetDialogEnabled` and
`0x7000013 RS_SetInjured`: runtime model, native handlers, wiring, tests.
One subproject. This refines the blocked verdict of D-103 for
implementation; D-103 stays historical.

## Source of truth

- `I:\Games\Златогорье-2\Server.dll` v1.3, SHA-256
  `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`,
  image base `0x14000000`.
- Handlers: `0x410b4` (`RS_GetDialogEnabled`), `0x41c34`
  (`RS_SetInjured`).
- Decompiled corpus: `.local/decomp/server/decompiled.c` and
  `natives/natives.c` (gitignored).
- Usage in shipped `scripts.pak` (555 `.cs`, plus
  `scripts.update.3.pak`): `RS_GetDialogEnabled` 100 calls / 34 files
  (99 plain + 1 legacy-XOR file `l2.p588_kor_aferist.d588`),
  `RS_SetInjured` 8 calls / 2 files
  (`l1_2.p397_healer_experimentator`, `zlato_scen2/l18.p152_bone_chamber_wizard`).
- `RS_SetInjured` is called with person `"Hero"` and int `0` or `1`.
  `RS_GetDialogEnabled` is called with one int argument (samples: `2`);
  the handler ignores that argument.

## Type gates

Both handlers call `FUN_14038c6c` (RVA `0x38c6c`):

- `RS_GetDialogEnabled`: `FUN_14038c6c(0, -1)` — exactly one numeric
  argument; wrong arity/type returns `DAT_14091460` = `0.0`.
- `RS_SetInjured`: `FUN_14038c6c(3, 0, -1)` — string + numeric; wrong
  arity/type returns `DAT_14091518` = `0.0`.

## `RS_GetDialogEnabled` semantics

Native (`0x140410b4`):

```
percent = FUN_14009f08(DAT_14088e00, FUN_14027f7c(DAT_14088dc4))
return FUN_14006288((float)percent * 0.01f) ? 1.0 : 0.0
```

`FUN_14027f7c` (RVA `0x27f7c`) returns the first valid id from the
controlled-party list at `world+0x1cf4` (validity bitmap `+0x1cf8`,
count `+0x1cfc`). The same list is averaged by `FUN_140254bc`
(RVA `0x254bc`, party level). In single player the first controlled
member is the hero; NPCs never define `SKILL_TALKING` in shipped `.scr`,
so only the hero yields a meaningful result. Port: evaluate `"Hero"`.

`FUN_14009f08` (RVA `0x9f08`) with `FUN_1400cb64` (RVA `0xcb64`,
`ch_max_skill_value` = 15) and `FUN_1400d4c0` (RVA `0xd4c0`,
effective INT, `ch_max_primary` = 30):

```
talking = clamp(skill[0x15] = SKILL_TALKING, 0, 15)
if talking == 0: return 0
if talking > 14: return 100
v = FUN_14007fb0((float)(INT_eff + 10 + talking * 4), 0x36)
if clamp(skill[0x10] = SKILL_TRADE, 0, 15) > 9: v += 20.0f
if (gv_addon != 0):                                  // DAT_1407fc64+0x8c
    flags = entity+0xf8
    if flags & 0x10:     v += 30.0f                  // DAT_1408dac8
    if flags & 0x100000: v += 10.0f                  // DAT_1408dac4
    if flags & 0x200000: v += 15.0f                  // DAT_1408dac0
return FUN_1400619c(v)                               // round half away from zero
```

- `FUN_1400d4c0` = `round(FUN_14007fb0(INT_base, 10)) +
  (SKILL_TALKING > 9 ? 2 : 0)`, no gate bonus, clamp `[1, 30]`; equals
  `PersonRuntime::get_parameter(INTELLIGENCE)` under the accepted D-050
  assumptions.
- `FUN_14007fb0` (RVA `0x7fb0`) is treated as identity (empty modifier
  table), already accepted for all person parameters by D-050.
- `FUN_1400619c` (RVA `0x619c`): `trunc(v + (v > 0 ? 0.5f : -0.5f))`
  (`DAT_1408d7a0` = `0.5f`, `DAT_1408d7a4` = `-0.5f`,
  `DAT_1408d7a8` = `0.0f`).

Probability gate `FUN_14006288` (RVA `0x6288`), constants
`DAT_1408d7cc` = `0.0f`, `DAT_1408d7c8` = `1.0f`,
`DAT_1408d7c4` = `1/32767f` (`0x38000100`):

```
if p <= 0: false
if p > 1:  true
return (float)rand() * (1.0f / 32767.0f) < p
```

`_rand()` is the shared MSVC CRT stream (D-030); the port uses the same
level RNG service as P6a5 and consumes one value only when
`0 < p <= 1`. `MsvcRandom::coin` already implements this formula.

## `RS_SetInjured` semantics

Native (`0x14041c34`): type gate, `person = FUN_1402914c(name)`,
`FUN_14018848(person, arg)`, return `0.0`.

`FUN_14018848` (RVA `0x18848`) maintains the per-person ordered list at
`person+0x110c` (count `+0x14f4`, capacity `+0x14f8`) of 0x14-byte
records and a fixed key `DAT_140790e8` (20 bytes):

```
remove all records equal to the key
if arg != 0: append one copy of the key (when count < capacity)
```

The key bytes (`.data` at RVA `0x790e8`) are
`16 00 00 00 cf ff ff ff 81 00 00 00 00 00 00 00 ff ff ff ff`,
i.e. dwords `{22, -49, 129, 0, -1}`. `arg` is therefore a membership
toggle for the fixed record: non-zero ensures presence at the tail,
zero ensures absence. The record is not a simple boolean flag stored
elsewhere.

Unknown person name: the original calls `FUN_14018848` with a null
person and crashes; the port uses a no-op (same policy as D-102/D-107).

## Model and API

`gl::game` (`person_runtime.hpp/.cpp`):

```cpp
struct PersonEffect { std::array<std::int32_t, 5> words{}; };

struct PersonState {
    ...
    std::vector<PersonEffect> effects;  // ordered 0x14-byte records
};

void set_injured_records(std::vector<PersonEffect> &records, bool injured);

class PersonRuntime {
    void set_injured(std::string_view person, bool injured);
    bool injured(std::string_view person) const;
    std::int32_t dialog_percent(std::string_view person) const;
};
```

- `set_injured_records` is the SDL-free `FUN_14018848` machine on an
  ordered record vector; `PersonRuntime::set_injured` applies it to a
  known person only. `injured` reports membership (test/consumer
  accessor).
- `dialog_percent` implements `FUN_14009f08` with
  `get_parameter(INTELLIGENCE)`, clamped `SKILL_TALKING`/`SKILL_TRADE`,
  float arithmetic, `+20.0f` for trade > 9, and
  `round_half_away_from_zero`. `gv_addon` is `0` (base game) and the
  `+0xf8` bonuses are not applied.
- The 20-byte record list has no other producers in the port; other
  status-effect entries and consumers stay out of scope.

`gl::script`:

```cpp
struct PersonAccess {
    ...
    std::function<void(std::string_view person, bool)> set_injured;
    std::function<std::int32_t()> dialog_percent;  // bound to the hero
};

struct RandomAccess {
    std::function<std::int32_t(std::int32_t, std::int32_t)> range;
    std::function<bool(double)> coin;
};
```

## Native handlers

- `0x700000E RS_GetDialogEnabled` — `supported`, arity 1. Require one
  numeric argument (ignored). Missing `person.dialog_percent` or
  `random.coin`, or wrong type → `0.0`. Otherwise
  `p = static_cast<float>(percent) * 0.01f`; return `coin(p) ? 1.0 : 0.0`.
- `0x7000013 RS_SetInjured` — `supported`, arity 2. Require string +
  numeric; `set_injured(name, value != 0.0)`; missing service or type
  failure → `0.0`; return `0.0`.
- Registry: supported `36` → `38` of `65`.

## Wiring

- `PersonRuntime::access()` binds `set_injured` and
  `dialog_percent` (the latter calls `dialog_percent("Hero")`).
- `glgame`: `RandomAccess::coin` uses the same RNG as `range` (level
  simulation stream, production fallback with `--seed`).
- `glscript`: `coin` on the tool RNG (seed `1`), matching `range`.

## Tests

- `tests/game/test_person_runtime.cpp`:
  - `dialog_percent`: talking 0 → 0; talking 15 → 100; mid values =
    `round(intel + 10 + talking*4)`; `SKILL_TRADE` 9 vs 10 (`+20`);
    clamped skills/INT; negative/absent skills.
  - `set_injured`: add, remove, repeated add (single record, tail),
    ordering against a pre-existing other record via
    `set_injured_records`, unknown person no-op, `injured` membership.
- Native tests (same file or `tests/script`): wrong arity/type → `0.0`;
  `GetDialogEnabled` with stub `dialog_percent` + deterministic `coin`
  (0 and 1); `SetInjured` forwards name/flag; support/arity values.
- `tests/script/test_native_registry.cpp`: `0x700000E` is now
  `supported`; supported count/lookup unchanged otherwise.
- Real-script tests:
  - `scripts/dialogs/l1_2.p397_healer_experimentator.d397.age.cs` —
    reach `RS_SetInjured("Hero",1)` and `("Hero",0)`; assert the runtime
    record toggles.
  - `scripts/dialogs/l12_1/l32.p214_feather_girl.d214.age.cs` —
    `RS_GetDialogEnabled` with hero `SKILL_TALKING` 0 returns `0.0` and
    drives the script's comparison branch.
- Full `ctest` with `GL_GAME_DIR`.

## Non-goals / residual

- `FUN_14007fb0` with a non-empty modifier table, `manager+0xC` gate
  bonuses, `gv_addon` (`DAT_1407fc64+0x8c`) and `entity+0xf8` flag
  bonuses (D-050 assumptions).
- Injury/effect consumers (animation, limbs, speed), other effect
  producers, list capacity `+0x14f8`, save/load interpretation of the
  record list.
- Controlled party beyond the hero (companions / multiplayer).
- The semantic meaning of the ignored `RS_GetDialogEnabled` argument.

## Acceptance

- Registry shows `0x700000E` and `0x7000013` as `supported` (38/65).
- Unit/native/real-script tests above pass; full `ctest` green with
  `GL_GAME_DIR`; commands and output recorded in `STATUS.md`.
- `docs/research/p6a8-person-dialog-injury.md`, `STATUS.md`,
  `PROJECT.md`, `DECISIONS.md` updated.

## References

- `docs/research/p6a8-person-dialog-injury.md`
- `docs/research/p3b1-person-parameters-server.md` (§4, §7)
- `docs/research/p3-vm-natives-server.md` (§5, usage counts)
- D-050 (person-parameter assumptions), D-103 (historical blocked
  verdict), D-030 (MSVC RNG).
