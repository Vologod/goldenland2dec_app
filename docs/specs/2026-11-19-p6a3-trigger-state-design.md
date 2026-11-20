# P6a3 — Trigger state runtime (design)

Status: approved (2026-11-19)
Task: `P6a3` (follow-up to P6a2)
Scope: `src/game`, `src/script`, `tools/glgame`, `tests/`; no trigger effects.

## 1. Evidence

`RS_EnableTrigger` (`0x140416e8`, args `[3,0]`, returns `0.0`):
type-checks a string and an int, gets the current level, walks entities and
matches the trigger name at entity `+0x4c` when flags `+0x14 & 0x10` is set;
then clears bit `0x200` and sets bit `0x100` when the int is nonzero
(`0xfffffeff` mask, `0x100` OR). Lookup failures are no-ops. The port has
LVL trigger descriptions (`LvlData::triggers`) but no entity trigger flags.

This slice models the observed state only: a named trigger is enabled or not.

## 2. Runtime

`gl::game::TriggerRuntime` (`trigger_runtime.hpp/cpp`):

```cpp
class TriggerRuntime {
  public:
    void reset(std::span<const gl::assets::LvlDescription> triggers);
    bool set(std::string_view name, bool enabled);
    bool enabled(std::string_view name) const;
    std::size_t size() const;
    void clear();
    gl::script::TriggerAccess access();
};
```

`reset` seeds every LVL trigger name as disabled. `set` toggles a known name
and returns whether it exists; unknown names are ignored (Server no-op).
Initial disabled is a **port choice** until entity trigger flags are mapped.

## 3. Native

`TriggerAccess` in `NativeServices`; `0x7000009` becomes supported (25/65):
string + integer args, calls `set`, returns `0.0` on every path. Missing
access returns `0.0`. `glgame` resets the runtime from
`level_->level().triggers` after each level load.

## 4. Acceptance

1. Unit: reset/known/unknown/toggle/clear.
2. Native: string+int dispatch, return `0.0`, wrong type, missing access.
3. Real: `l1_1` trigger names loaded; toggle one and re-query.
4. Screenshot regression; full `ctest`; docs/DECISIONS D-097.

## 5. Non-goals

Trigger activation effects, entity flag `+0x14`, region/area tests, save/load
of trigger state, `RS_SetLocationAccess` (0x8).
