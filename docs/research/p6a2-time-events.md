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
  entry datetime and returns `1` when the current time is past it, else `0`.
- `RS_ClearEvent` (0x7000007, 1 int): `FUN_1405b6c4(clock, arg)` removes the
  entry with `[0] == arg`, returning `1` on removal and `0` otherwise.
- Thunk returns: `SetEvent` computed double (`eax`), `GetEvent` computed int,
  `ClearEvent` `0.0` (`0x14091440`).

## 2. Interpretation

The one-int argument to `GetEvent`/`ClearEvent` is the **handle** returned by
`SetEvent` (clock counter), not an event id. Events are scheduled timers with
a due datetime; `GetEvent` is a “due now” predicate, not a value read.

## 3. Blocked

- Clock vtable `+0`: how the current datetime and the `arg0/arg1`
  descriptor produce the due datetime (date arithmetic, units, clamp).
- Entry datetime layout vs `GameDatetime` and the `[0]`/`+0x38` counter’s
  exact role.
- `SetEvent` return value: `FUN_1405b9b4` is decompiled `void`; the thunk
  converts `eax`, so the returned handle is inferred, not proven.

**Verdict: `blocked (scoped)`.** A faithful implementation needs clock
vtable `+0` or a user-assisted live capture; a plain handle→value map would
be wrong.

## 4. Next checks

1. Decompile the `DAT_14088e14` vtable slot `+0` and its argument contract.
2. Trace `FUN_1405b9b4` callers/register setup for the hidden event descriptor.
3. Live: at a known calendar time call `SetEvent(a,b)`, record the returned
   value and `GetEvent` before/after the due time.
4. Then implement `TimeEventRuntime` on `GameCalendar`; keep `0x5..7`
   unsupported until then.
