# P6a13 — Entity party flags implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Model `entity+0x5a8` party role in the port.

**Architecture:** `PersonState.party_role` with `PersonRuntime` accessors;
`PartyRuntime` updates it through an attached `PersonRuntime`.

**Tech Stack:** C++20, doctest, CMake/Ninja.

## Global Constraints

- No SDL in game/script; `AGENTS.md` naming.
- Docs UTF-8 without BOM via `write`/`edit`.
- Full `ctest` with `GL_GAME_DIR`.

---

### Task 1: Person role state

**Files:**
- Modify: `src/game/include/gl/game/person_runtime.hpp`
- Modify: `src/game/src/person_runtime.cpp`
- Test: `tests/game/test_person_runtime.cpp`

**Interfaces:**
- Produces: `PersonState::party_role` (int32, default 0),
  `void PersonRuntime::set_party_role(std::string_view, std::int32_t)`,
  `std::int32_t PersonRuntime::party_role(std::string_view) const`.

- [ ] **Step 1: Write the failing test**

Add to `tests/game/test_person_runtime.cpp`:

```cpp
TEST_CASE("person party role updates known persons only") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);
    CHECK(runtime.party_role("Hero") == 0);
    runtime.set_party_role("Hero", 1);
    CHECK(runtime.party_role("Hero") == 1);
    runtime.set_party_role("missing", 2);
    CHECK(runtime.party_role("missing") == 0);
}
```

- [ ] **Step 2: Implement**

`person_runtime.hpp`: add `std::int32_t party_role = 0;` to
`PersonState`; declare both methods.

`person_runtime.cpp`:

```cpp
void PersonRuntime::set_party_role(std::string_view person, std::int32_t role) {
    PersonState *state = find(person);
    if (state != nullptr) state->party_role = role;
}

std::int32_t PersonRuntime::party_role(std::string_view person) const {
    const PersonState *state = find(person);
    return state == nullptr ? 0 : state->party_role;
}
```

- [ ] **Step 3: Run**

`cmake --build build; build\bin\gl_game_tests.exe --test-case="*party role*"`

### Task 2: PartyRuntime role updates

**Files:**
- Modify: `src/game/include/gl/game/party_runtime.hpp`
- Modify: `src/game/src/party_runtime.cpp`
- Test: `tests/game/test_party_runtime.cpp`

**Interfaces:**
- Consumes: `PersonRuntime::set_party_role`.
- Produces: `void PartyRuntime::attach(PersonRuntime*)`.

- [ ] **Step 1: Write the failing test**

Add to `tests/game/test_party_runtime.cpp` (include
`gl/game/person_runtime.hpp`):

```cpp
TEST_CASE("party runtime updates entity roles") {
    auto persons = gl::game::PersonRuntime::create({}, std::nullopt);
    PartyRuntime party;
    party.attach(&persons);
    CHECK(party.add("Hero") == 3);
    CHECK(persons.party_role("Hero") == 1);
    CHECK(party.remove("Hero") == 4);
    CHECK(persons.party_role("Hero") == 2);
    CHECK(party.remove("Hero") == 4);
    CHECK(persons.party_role("Hero") == 2);
    party.clear();
    CHECK(persons.party_role("Hero") == 0);
}
```

Add edge cases: full party add returns -1 without touching a new role;
empty party remove leaves a known person at 0; known but unlisted remove
sets 2.

- [ ] **Step 2: Implement**

`party_runtime.hpp`: forward include `person_runtime.hpp`; add
`void attach(PersonRuntime *persons) { persons_ = persons; }` and
`PersonRuntime *persons_ = nullptr;`; move `clear()` to the .cpp.

`party_runtime.cpp`:

```cpp
std::int32_t PartyRuntime::add(std::string_view name) {
    if (members_.size() >= kMaxParty) return -1;
    members_.emplace_back(name);
    if (persons_ && persons_->has_person(name)) persons_->set_party_role(name, 1);
    return static_cast<std::int32_t>(kMaxParty - members_.size());
}

std::int32_t PartyRuntime::remove(std::string_view name) {
    if (members_.empty()) return static_cast<std::int32_t>(kMaxParty);
    if (persons_ && persons_->has_person(name)) persons_->set_party_role(name, 2);
    const auto it = std::find(members_.begin(), members_.end(), name);
    if (it != members_.end()) members_.erase(it);
    return static_cast<std::int32_t>(kMaxParty - members_.size());
}

void PartyRuntime::clear() {
    if (persons_)
        for (const std::string &name : members_)
            if (persons_->has_person(name)) persons_->set_party_role(name, 0);
    members_.clear();
}
```

- [ ] **Step 3: Run**

`cmake --build build; build\bin\gl_game_tests.exe --test-case="*party*"`

### Task 3: glgame attachment

**Files:**
- Modify: `tools/glgame/main.cpp`

- [ ] **Step 1: Implement**

In `rebuild_person_runtime`, after creating `person_runtime_`, call
`party_.attach(&person_runtime_);`.

- [ ] **Step 2: Build**

`cmake --build build`

### Task 4: Docs and full verification

**Files:**
- Modify: `docs/DECISIONS.md` (D-109)
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`

- [ ] **Step 1: Docs**
  - D-109: role semantics, edge cases, residuals.
  - STATUS: P6a13 done with commands/output.
  - PROJECT: P6a 36/65, entity party role done.
- [ ] **Step 2: Full check**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake --build build
ctest --test-dir build --output-on-failure
```

## Self-review

- Spec coverage: role state, party transitions, glgame, tests, docs.
- No placeholders.
- Types consistent (`party_role`, `set_party_role`, `attach`).
