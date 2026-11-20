# P6a2 — Time-event runtime (design)

Status: approved (2026-11-19)
Task: `P6a2` implementation (follow-up to D-095)
Scope: `src/game`, `src/script`, `tools/glgame`, `tests/`; no save/UI.

## 1. Evidence

- `RS_SetEvent` builds a descriptor with arg0 at `+0x08`, arg1 at `+0x10`,
  gets the current datetime (`clock` vtable `+0x18`), calls vtable `+0`, then
  `FUN_1405b9b4` appends a 0x24-byte entry and returns the handle (`eax`,
  counter `clock+0x38`) as double.
- Clock vtable `+0` is `0x1405b38c`; it adds descriptor `+0x08` to days,
  `+0x10` to hours and `+0x14` to minutes. SetEvent writes arg0 at `+0x08`
  and arg1 at `+0x10`, so **due = now + arg0 days + arg1 hours** (the
  `decompiled.c:30300-30315` minutes path is a different timer, not
  SetEvent).
- `FUN_1405b80c(clock, handle, 0)` returns `1` when the current datetime is
  at/past the entry datetime; `FUN_1405b6c4` removes by handle and resets the
  counter when the list becomes empty; `ClearEvent` returns `0.0`.

## 2. Runtime

`gl::game::TimeEventRuntime` (`time_event_runtime.hpp/cpp`):

```cpp
struct TimeEvent { GameDatetime due; };
class TimeEventRuntime {
  public:
    using Now = std::function<GameDatetime()>;
    void set_now(Now now);
    std::int32_t set_event(std::int32_t days, std::int32_t hours);
    std::int32_t get_event(std::int32_t handle) const;
    void clear_event(std::int32_t handle);
    void clear();
    gl::script::TimeEventAccess access();
};
```

Handles start at 1 and increment; `clear_event` of the last entry resets the
counter to 1. `set_event` stores
`due = add_milliseconds(now, days*86400000 + hours*3600000)`; signed offsets
are allowed (negative days combine with positive hours).
`get_event(handle)` returns `1` iff the event exists and `now >= due`, else
`0`. `clear_event` removes by handle (unknown handle is a no-op).

## 3. Natives

`TimeEventAccess` in `NativeServices`; `0x7000005..7` become supported:
- `SetEvent(days, hours)` -> `double(handle)`;
- `GetEvent(handle)` -> `1.0`/`0.0`;
- `ClearEvent(handle)` -> `0.0`.

Missing access returns `0.0`. `glgame` attaches the runtime and resets it on
New Game/successful load next to `QuestRuntime`.

## 4. Acceptance

1. Unit: handle monotonicity, due before/after (`get_event` 0 then 1), clear,
   unknown handle, negative/mixed offsets, `clear()`.
2. Native dispatch: values/returns, missing access, `0x5..7` supported
   (24/65).
3. glgame screenshot regression; full `ctest`.
4. Docs/DECISIONS D-096; due computation derived from 0x1405b38c.

## 5. Non-goals

Save/load of timers, wall-clock synchronization, trigger/location natives,
journal UI.
