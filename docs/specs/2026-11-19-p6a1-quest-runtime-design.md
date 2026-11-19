# P6a1 — Quest/event runtime (design)

Status: implemented (2026-11-19)
Task: `P6a1` (first P6 slice)
Scope: `src/game`, `src/script`, `tools/glgame`, `tests/`; no save/UI/triggers/events.

## 1. Evidence

Server native thunks (`Server.dll`, SHA-256 `418E2560…CC837`;
`docs/research/p3-vm-natives-server.md:328-337`):

| id | name | args | return | target |
|---|---|---|---|---|
| `0x7000000` | `RS_QuestComplete` | 1 string | `0.0` | `FUN_140591b8(host, quest)` |
| `0x7000001` | `RS_StageEnable` | 2 strings | `0.0` | `FUN_14059030(host, quest, stage)` |
| `0x7000002` | `RS_QuestEnable` | 2 strings | `0.0` | `FUN_14059f6c(host, quest, stage)` |
| `0x7000003` | `RS_StageComplete` | 2 strings | `0.0` | `FUN_14058de0(host, quest, stage)` |
| `0x7000004` | `RS_StorylineQuestEnable` | 1 string | `0.0` | `FUN_1405a21c(host, quest)` |
| `0x7000005` | `RS_SetEvent` | 2 ints | computed | time-event list |
| `0x7000006` | `RS_GetEvent` | 1 int | computed int | time-event list |
| `0x7000007` | `RS_ClearEvent` | 1 int | `0.0` | time-event list |
| `0x7000008` | `RS_SetLocationAccess` | string,int | `0.0` | world state |
| `0x7000009` | `RS_EnableTrigger` | string,int | `0.0` | trigger runtime |

The quest/stage thunks type-check strings and always return the zero constant
(`0x140913b0/0x140913b8/…`); the VM passes string nodes. Real scripts call
e.g. `RS_StageComplete("potion","potion_4")`. Events are a time-object list
(`FUN_1405b9b4/5b80c/5b6c4`) and are out of this slice.

## 2. Runtime

`gl::game::QuestRuntime` (`quest_runtime.hpp/cpp`):

```cpp
struct QuestState { bool enabled=false; bool completed=false; bool storyline=false; };
struct StageState { bool enabled=false; bool completed=false; };
class QuestRuntime {
  public:
    void enable_quest(std::string_view id, bool storyline=false);
    void complete_quest(std::string_view id);
    bool quest_enabled(std::string_view id) const;
    bool quest_completed(std::string_view id) const;
    bool quest_storyline(std::string_view id) const;
    void enable_stage(std::string_view quest, std::string_view stage);
    void complete_stage(std::string_view quest, std::string_view stage);
    bool stage_enabled(std::string_view quest, std::string_view stage) const;
    bool stage_completed(std::string_view quest, std::string_view stage) const;
    void clear();
    gl::script::QuestAccess access();
};
```

Keys are technical script names (`std::string`). Empty names are ignored;
`complete_*` implies `enabled`; story flag sticky; operations idempotent;
`clear()` resets all state.

## 3. Native wiring

`QuestAccess` in `NativeServices` with string-view callbacks. Supported:
`0x7000000..4`. Handlers reject non-string args, mutate the runtime, and
return `double{0.0}` on both success and failure, matching the thunks.
`RS_QuestEnable` uses both names: enables the quest and its initial stage.
`0x7000005..7` remain unsupported (`requires time-event runtime`);
`0x8/0x9` remain unsupported.

## 4. glgame

`Game` owns `QuestRuntime quest_runtime_`; `DialogContext` services get
`quest_runtime_.access()`; `load_level` clears it (no persistence yet).
No journal export, UI, triggers or location behavior.

## 5. Acceptance

1. Runtime transitions/idempotence/empty names/clear unit tests.
2. Native dispatch with string names: state changes, return `0.0`; numeric
   args rejected; missing access returns `0.0`.
3. `glgame_screenshot_tests` still pass (no save/dialog regression).
4. Full `ctest`; docs/DECISIONS (D-094).

## 6. Non-goals

Save/load of quests (blocked b4c1a), DIARY write, journal UI, time events
(`0x5..7`), triggers (`0x9`), location access (`0x8`), global map/storyline
chapters.
