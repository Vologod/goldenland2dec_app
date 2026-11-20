# P6a2 — Time-event natives research

Status: complete (2026-11-19)
Scope: `docs/`; follow-up to D-094.

## 1. Evidence

`Server.dll` SHA-256 `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`;
disassembly `0x14040e2c..0x14040f34`; decompiled
`FUN_1405b9b4`/`FUN_1405b80c`/`FUN_1405b6c4`.

- `RS_SetEvent` (0x7000005, 2 int args): reads the clock datetime via
  `DAT_14088e14` vtable `+0x18`, zeroes an 8-dword descriptor, stores arg0 at
  descriptor `+0x08` and arg1 at `+0x10`, calls clock vtable `+0`, then
  `FUN_1405b9b4` and returns `eax` as a double.
- `FUN_1405b9b4` appends a 0x24-byte entry to the clock list
  (`+0x30` data, `+0x34` count); entry `[0]` is `clock+0x38` (a counter),
  `[1..8]` a datetime. The counter increments per append.
- `RS_GetEvent` (0x7000006, 1 int): `FUN_1405b80c(clock, arg, 0)` scans the
  list; if entry `[0] == arg` it compares the current datetime with the
  entry datetime and returns `1` when the current time is at/past it, else `0`.
- `RS_ClearEvent` (0x7000007, 1 int): `FUN_1405b6c4(clock, arg)` removes the
  entry with `[0] == arg`, returning `1` on removal and `0` otherwise.
- Thunk returns: `SetEvent` computed double (`eax`), `GetEvent` computed int,
  `ClearEvent` `0.0` (`0x14091440`).

## 2. Interpretation

The one-int argument to `GetEvent`/`ClearEvent` is the **handle** returned by
`SetEvent` (clock counter), not an event id. Events are scheduled timers with
a due datetime; `GetEvent` is a “due now” predicate, not a value read.

## 3. Resolved (2026-11-19)

- Clock vtable `+0` is `0x1405b38c`; it adds descriptor `+0x08` to days,
  `+0x10` to hours, `+0x14` to minutes and `+0x1c` to milliseconds of the
  supplied datetime. SetEvent writes arg0 at `+0x08` and arg1 at `+0x10`,
  so **due = now + arg0 days + arg1 hours**.
- `SetEvent` returns the pre-increment counter (`clock+0x38`) as a double;
  `GetEvent`/`ClearEvent` consume that handle. Removing the last entry resets
  the counter to `1`.
- No opaque event value is stored; only the due datetime.

**Verdict: `implementable scoped`**, implemented as D-096
(`TimeEventRuntime`); save/load of timers and a New Game clock base remain
out of scope.

## 4. Remaining checks

1. Save/load of the timer list (not modeled).
2. New Game clock base: glgame advances the calendar only when a save
   initialized it; without that events use a frozen datetime.
3. Live timing (equality boundary) if a real capture is available.
