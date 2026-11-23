# P6a13 — Entity party flags (design)

Status: approved (2026-11-21)
Scope: `PersonState.party_role`, `PersonRuntime` accessors, `PartyRuntime`
role updates, glgame attachment, tests. No AI/follow/save.

## Evidence

`docs/research/p6a10-party-runtime.md` "Entity party flag `+0x5a8`".

- Add sets the resolved person `+0x5a8 = 1` after the party-size check.
- Remove returns 4 on an empty party without touching the person;
  otherwise it sets `+0x5a8 = 2` before removing the first matching slot,
  so a known unlisted person also becomes 2.
- The original hero entity stays 0 (the port's synthetic `"Hero"`
  PersonState is a normal known person for tests; there is no
  hero-name special case).

## Game model

- `PersonState` gains `std::int32_t party_role = 0`.
- `PersonRuntime::set_party_role(name, role)` updates only known persons;
  `party_role(name)` returns the role, 0 for unknown.
- `PartyRuntime::attach(PersonRuntime*)`; `add`/`remove`/`clear` update
  roles when attached:
  - add: full → `-1` before touching roles; otherwise append and set 1
    for a known person;
  - remove: empty → `4` before touching roles; otherwise set 2 for a
    known person, then remove one matching slot if present;
  - clear: reset current members to 0, then clear the list.
- glgame calls `party_.attach(&person_runtime_)` in
  `rebuild_person_runtime`.

## Non-goals

- Hero id `+0xf28`, `+0x44`, `+0x5c0`, name copy.
- AI/follow, death-driven party cleanup, save/load and rendering.

## Tests

- `tests/game/test_party_runtime.cpp`: roles 1/2/0; empty remove leaves
  role; known unlisted remove sets 2; full add leaves role; clear resets.
- `tests/game/test_person_runtime.cpp`: accessors for known/unknown.
- Full `ctest` with `GL_GAME_DIR`.

## Acceptance

Roles modelled and tested; party natives unchanged (36/65); full `ctest`
green; STATUS/PROJECT/DECISIONS updated.

## Residual

AI/follow, hero-id link, death cleanup and save/load remain out of scope.
