# P6a6 — Weather state native (design)

Status: approved (2026-11-19)
Task: `P6a6`
Scope: `src/game`, `src/script`, `tools/glgame`, `tests/`; no rendering.

## 1. Evidence

`RS_SetWeather` (`0x14041204`, one int arg, returns `0.0`):
maps `1 -> 0x10001`, `2 -> 0x10002`, any other value -> `0`, then calls
`FUN_1405c1fc(weather_object, id)`. No other observable state.

## 2. Runtime

`gl::game::WeatherRuntime` (`weather_runtime.hpp/cpp`):
`set_from_script(int code)` applies the mapping; `current()` returns the
stored id; `clear()` resets to `0`. Initial `0` (clear weather/no override).

## 3. Native

`WeatherAccess` in `NativeServices`; `0x700000B` supported (27/65), numeric
arg, returns `0.0`; missing access/wrong type returns `0.0`. `glgame` attaches
the runtime and clears it with the session.

## 4. Acceptance

1. Unit: mapping 1/2/other, overwrite, clear.
2. Native: numeric dispatch/`0.0`, string rejection, missing access.
3. Screenshot; full `ctest`; docs/DECISIONS D-101.

## 5. Non-goals

Weather rendering/particles, duration, save/load, `scripts/weather.scr`
runtime.
