# P6a9b - Day/night light native (design)

Status: implemented (2026-11-19)
Scope: game calendar, script clock access, glgame, tests.

Evidence: clock vtable +0xc = 0x1405bb0c reads hour (+0x20), minute (+0x24), second (+0x28). Returns 0.0 for hour<7 or >=22; 1.0 for 8..20 and hour 12; sunrise (hour<12) = (minute + second/60)/60; sunset (hour>12) = 1 - (minute + second/60)/60. Constants 1/60 at 0x14092b00/b10, 1.0/0.0 at b08/b18/b1c.

day_night_factor implements this; ClockAccess.day_night wired to calendar_.now(); RS_GetDayOrNight (0x6000000) returns double. Registry 31/65.

Non-goals: rendering light/weather, clock speed/save.
