# P6a9 - Clock query natives (design)

Status: approved (2026-11-19)
Scope: game calendar helper, script access, glgame, tests.

Evidence: RS_GetCurrentTimeOfDayI (0x6000001, 0x14040db8) gets the 8-word clock datetime and returns its +0x10 hour as double. RS_GetDaysFromBeginningI (0x6000002, 0x14040de8) calls FUN_1405b140(datetime), which returns the absolute day number from year/month/day (known leap-year formula), as double. RS_GetDayOrNight (0x6000000) is clock vtable +0xc and stays unsupported until that slot is mapped.

Runtime: ClockAccess { current_hour, days_from_beginning } in NativeServices; glgame binds both to calendar_.now(). game::days_from_beginning(GameDatetime) implements FUN_1405b140 exactly.

Acceptance: hour/days values, day increments, native 0.0/return types, arity; screenshot and full ctest.

Non-goals: day/night slot, clock speed, save/load.
