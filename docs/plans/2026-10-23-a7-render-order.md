# A7: Y-sort, анкоры и crops — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Привести порядок отрисовки персонажей к `Client.dll`: сортировка по Y/X, отдельные проходы тени и тела, без изменения статиков/LAO и PAD-анкоров.

**Architecture:** `app/person_render` получает чистые функции сортировки и прямоугольника кадра; `LevelScreen` строит отсортированный список индексов и рисует два прохода. Симуляция и ассеты не меняются.

**Tech Stack:** C++20, CMake+Ninja, doctest, SDL2, LLVM-MinGW.

## Global Constraints

- C++20, `-Wall -Wextra -Werror`, форматирование по `.clang-format`.
- `gl_assets`/`gl_game` остаются SDL-free; сортировка — только в `app`.
- Порядок из оригинала: статики → LAO-анимации → динамические сущности.
- Для `EF_PERSON`: Y (`+0x1c`), затем X (`+0x18`); тень — проход 2, тело — проход 7.
- PAD-анкоры: `draw = position - anchor`; источник кадра — полная ячейка.
- Скриншоты/PNG не коммитить; ассеты игры не коммитить.
- Полный `ctest` запускать с `$env:GL_GAME_DIR = "I:\Games\Златогорье-2"`.
- Toolchain PATH:
  `$tools="I:\goldenland2dec\.local\toolchains"; $env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"`

---

### Task 1: Сортировка персонажей по `(Y, X)`

**Files:**
- Modify: `src/app/include/gl/app/person_render.hpp`
- Modify: `src/app/src/person_render.cpp`
- Test: `tests/app/test_person_render.cpp`

**Interfaces:**
- Produces:

```cpp
struct PersonDrawKey {
    float x = 0.0f;
    float y = 0.0f;
};

std::vector<std::size_t> sort_person_indices(std::span<const PersonDrawKey> keys);
```

- [ ] **Step 1: Write the failing test**

Добавить в `tests/app/test_person_render.cpp`:

```cpp
TEST_CASE("person draw order sorts by Y then X") {
    const std::array<gl::app::PersonDrawKey, 5> keys{{
        {10.0f, 20.0f}, {5.0f, 20.0f}, {5.0f, 19.0f}, {5.0f, 19.0f}, {50.0f, 10.0f},
    }};
    const std::vector<std::size_t> order = gl::app::sort_person_indices(keys);
    CHECK(order == std::vector<std::size_t>{4, 2, 3, 1, 0});
}

TEST_CASE("person draw order is stable for equal keys") {
    const std::array<gl::app::PersonDrawKey, 3> keys{{
        {7.0f, 42.0f}, {7.0f, 42.0f}, {7.0f, 42.0f},
    }};
    const std::vector<std::size_t> order = gl::app::sort_person_indices(keys);
    CHECK(order == std::vector<std::size_t>{0, 1, 2});
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:
```powershell
cmake --build build --target gl_person_render_tests; build\bin\gl_person_render_tests.exe
```
Expected: ошибка компиляции: `sort_person_indices` не объявлена.

- [ ] **Step 3: Write minimal implementation**

В `person_render.hpp` добавить `#include <span>`, `#include <vector>` и объявления:

```cpp
struct PersonDrawKey {
    float x = 0.0f;
    float y = 0.0f;
};

std::vector<std::size_t> sort_person_indices(std::span<const PersonDrawKey> keys);
```

В `person_render.cpp`:

```cpp
#include <algorithm>
#include <numeric>

std::vector<std::size_t> sort_person_indices(std::span<const PersonDrawKey> keys) {
    std::vector<std::size_t> order(keys.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [keys](std::size_t a, std::size_t b) {
        if (keys[a].y != keys[b].y) return keys[a].y < keys[b].y;
        return keys[a].x < keys[b].x;
    });
    return order;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run:
```powershell
cmake --build build --target gl_person_render_tests; build\bin\gl_person_render_tests.exe
```
Expected: все case проходят.

- [ ] **Step 5: Commit**

```powershell
git add src/app/include/gl/app/person_render.hpp src/app/src/person_render.cpp tests/app/test_person_render.cpp
git commit -m "feat(app): add person draw-order key and stable sort (A7)"
```

---

### Task 2: Полная ячейка кадра вместо crops

**Files:**
- Modify: `src/app/include/gl/app/person_render.hpp`
- Modify: `src/app/src/person_render.cpp`
- Modify: `src/app/src/level_screen.cpp`
- Test: `tests/app/test_person_render.cpp`

**Interfaces:**
- Consumes: `gl::assets::PadAnimation` (готовый PAD-парсер).
- Produces:

```cpp
struct FrameRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

FrameRect frame_rect(std::size_t column, std::size_t row,
                     int frame_width, int frame_height);
```

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("frame rect is the full PAD cell") {
    const gl::app::FrameRect rect = gl::app::frame_rect(3, 2, 40, 85);
    CHECK(rect.x == 120);
    CHECK(rect.y == 170);
    CHECK(rect.w == 40);
    CHECK(rect.h == 85);
}

TEST_CASE("frame rect ignores PAD crops") {
    gl::assets::PadAnimation animation;
    animation.frame_width = 32;
    animation.frame_height = 64;
    animation.frames_per_row = 12;
    animation.crops = {{{1, 2, 3, 4}}};
    const gl::app::FrameRect rect = gl::app::frame_rect(2, 0, animation.frame_width,
                                                        animation.frame_height);
    CHECK(rect.x == 64);
    CHECK(rect.y == 0);
    CHECK(rect.w == 32);
    CHECK(rect.h == 64);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:
```powershell
cmake --build build --target gl_person_render_tests; build\bin\gl_person_render_tests.exe
```
Expected: ошибка компиляции: `FrameRect`/`frame_rect` не объявлены.

- [ ] **Step 3: Write minimal implementation**

```cpp
struct FrameRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

FrameRect frame_rect(std::size_t column, std::size_t row,
                     int frame_width, int frame_height) {
    if (frame_width <= 0 || frame_height <= 0) return {};
    return FrameRect{
        static_cast<int>(column) * frame_width,
        static_cast<int>(row) * frame_height,
        frame_width,
        frame_height,
    };
}
```

В `level_screen.cpp` заменить построение `SDL_Rect source` для тела на:

```cpp
const FrameRect body_rect =
    frame_rect(column, placement.row, animation.frame_width, animation.frame_height);
const SDL_Rect source{body_rect.x, body_rect.y, body_rect.w, body_rect.h};
```

И для тени:

```cpp
const FrameRect shadow_rect = frame_rect(column, shadow.row, animation.shadow_width,
                                         animation.shadow_height);
const SDL_Rect source{shadow_rect.x, shadow_rect.y, shadow_rect.w, shadow_rect.h};
```

- [ ] **Step 4: Run test to verify it passes**

Run:
```powershell
cmake --build build --target gl_person_render_tests; build\bin\gl_person_render_tests.exe
```
Expected: все case проходят.

- [ ] **Step 5: Commit**

```powershell
git add src/app/include/gl/app/person_render.hpp src/app/src/person_render.cpp src/app/src/level_screen.cpp tests/app/test_person_render.cpp
git commit -m "refactor(app): use full PAD frame cells for person sprites (A7)"
```

---

### Task 3: Два прохода отрисовки и подключение сортировки

**Files:**
- Modify: `src/app/include/gl/app/level_screen.hpp`
- Modify: `src/app/src/level_screen.cpp`
- Test: `tests/app/test_person_render.cpp` (unit-порядок уже покрыт)
- Test: `tests/render/test_glview_screenshot.cpp` (существующий real-тест)
- Test: `tests/app/test_glgame_screenshot.cpp` (существующий real-тест)

**Interfaces:**
- Consumes: `sort_person_indices`, `FrameRect`, `PersonDrawKey`.
- Produces: приватный `enum class PersonPass { shadow, body };` и
  `draw_person(..., PersonPass pass, std::uint64_t elapsed_ms)`.

- [ ] **Step 1: Write the failing test (наблюдаемый порядок)**

В `tests/app/test_person_render.cpp` уже проверен порядок ключей; для
интеграции добавить проверку, что два персонажа с одним Y и разным X
сортируются по X:

```cpp
TEST_CASE("same baseline sorts by X") {
    const std::array<gl::app::PersonDrawKey, 2> keys{{{120.0f, 300.0f}, {84.0f, 300.0f}}};
    const std::vector<std::size_t> order = gl::app::sort_person_indices(keys);
    CHECK(order == std::vector<std::size_t>{1, 0});
}
```

Run:
```powershell
cmake --build build --target gl_person_render_tests; build\bin\gl_person_render_tests.exe
```
Expected: PASS (этот шаг фиксирует контракт; падение — только если Task 1 неверен).

- [ ] **Step 2: Refactor `draw_person`**

В `level_screen.hpp`:

```cpp
enum class PersonPass { shadow, body };

void draw_person(SDL_Renderer* renderer, const Person& person,
                 std::uint64_t elapsed_ms, PersonPass pass) const;
```

Добавить члены:

```cpp
std::vector<gl::app::PersonDrawKey> person_keys_;
std::vector<std::size_t> person_order_;
```

В `level_screen.cpp` включить `gl/app/person_render.hpp` уже включён.
В `draw_person` вычисления оставить общими, а рисование обернуть:

```cpp
if (pass == PersonPass::shadow) {
    if (!shadow_texture) return;
    const PersonShadowPlacement shadow =
        place_person_shadow_px(animation, direction, movement.x, movement.y);
    if (shadow.row >= animation.shadow_crops.size()) return;
    // существующий код тени, source через frame_rect(...)
    return;
}

// pass == PersonPass::body
// существующий код тела, source через frame_rect(...)
```

- [ ] **Step 3: Build the sorted order in `render`**

```cpp
if (options.draw_persons) {
    if (!persons_.empty()) {
        const std::uint64_t target = elapsed_ms;
        if (target > last_simulation_ms_) {
            simulation_.advance(static_cast<double>(target - last_simulation_ms_) / 1000.0);
            last_simulation_ms_ = target;
        }
    }
    simulated_ms_ = simulation_.time_ms();

    person_keys_.clear();
    person_keys_.reserve(persons_.size());
    for (const Person& person : persons_) {
        const gl::game::PersonMovement& movement = simulation_.persons()[person.instance];
        person_keys_.push_back(gl::app::PersonDrawKey{movement.x, movement.y});
    }
    person_order_ = gl::app::sort_person_indices(person_keys_);

    for (std::size_t index : person_order_) {
        draw_person(renderer, persons_[index], simulated_ms_, PersonPass::shadow);
    }
    for (std::size_t index : person_order_) {
        draw_person(renderer, persons_[index], simulated_ms_, PersonPass::body);
    }
}
```

Инвариант: `person_keys_[i]` соответствует `persons_[i]`, а
`person.instance < simulation_.persons().size()` для всех `persons_`
(симуляция создаётся из `level_.persons`, а `persons_` — только валидные
индексы этого массива). Поэтому индексы `person_order_` применимы к
`persons_` напрямую.

- [ ] **Step 4: Run build and targeted tests**

```powershell
cmake --build build
build\bin\gl_person_render_tests.exe
$env:GL_GAME_DIR="I:\Games\Златогорье-2"; build\bin\gl_game_tests.exe --test-case="*l1_1*"
```
Expected: build успешен; unit tests PASS; real l1_1 PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/app/include/gl/app/level_screen.hpp src/app/src/level_screen.cpp tests/app/test_person_render.cpp
git commit -m "feat(app): draw person shadows and bodies in Y-sorted passes (A7)"
```

---

### Task 4: Скриншоты, документы, полная проверка

**Files:**
- Modify: `docs/research/p4b2-render-order.md` (уже обновлён; дополнить evidence)
- Modify: `docs/DECISIONS.md` (D-036)
- Modify: `docs/STATUS.md`
- Modify: `docs/PROJECT.md`

**Steps:**

- [ ] **Step 1: Original screenshots**

Пользователь присылает 2–3 скриншота оригинала (`F8` → `Screenshots\ShotNNNN.bmp`)
с подписями уровня/сцены. Сохранить пути и SHA-256 в `STATUS.md`; сами BMP не
коммитить.

- [ ] **Step 2: Port screenshots**

```powershell
build\bin\glview.exe --game-dir "I:\Games\Златогорье-2" --level l1_1 --screenshot %TEMP%\a7_before.png --sim-ms 0
build\bin\glview.exe --game-dir "I:\Games\Златогорье-2" --level l1_1 --screenshot %TEMP%\a7_after.png --sim-ms 10000
```
Проверить: exit 0, PNG 1280×720, кадры различаются; повторный прогон
побайтово совпадает (SHA-256).

- [ ] **Step 3: Full test suite**

```powershell
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
ctest --test-dir build --output-on-failure
```
Expected: все тесты PASS, 0 failed; зафиксировать время и число тестов.

- [ ] **Step 4: Decision**

Добавить D-036:

> A7: порядок отрисовки — как в `Client.dll`: статики и LAO-анимации отдельными
> проходами до динамических сущностей; `EF_PERSON` сортируется по Y, затем X;
> тени (alpha 0.5) и тела (alpha 1.0) — два прохода по одному списку. Порт
> использует `std::stable_sort`; при полном равенстве ключей сохраняется
> порядок массива, тогда как оригинальный MSVC `qsort` нестабилен. Анкоры PAD
> (`position - anchor`) и полная ячейка кадра сохранены.

- [ ] **Step 5: Update STATUS/PROJECT and commit**

```powershell
git add docs/DECISIONS.md docs/STATUS.md docs/PROJECT.md docs/research/p4b2-render-order.md
git commit -m "docs(a7): record Y-sort parity, anchors and acceptance evidence"
```

## Self-Review

- Покрытие spec: сортировка (Task 1), frame rect/crops (Task 2), два прохода и
  интеграция (Task 3), evidence/документы (Task 4).
- Плейсхолдеров нет; команды и сигнатуры заданы.
- Имена согласованы: `PersonDrawKey`, `sort_person_indices`, `FrameRect`,
  `frame_rect`, `PersonPass`.
- Риск: индексация `person_keys_` при пропущенных персонах; в Task 3 указан
  инвариант и вариант защиты.
