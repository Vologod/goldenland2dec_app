# P4d Hero Wear Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Рисовать hero из `wear.pak` по engine-схеме HAD/IAD/SEQ: точный композит тела и слоёв экипировки, анкоры, тень, idle/walk.

**Architecture:** `gl_assets` получает три парсера (HAD, SEQ, расширенный IAD). SDL-free `gl_game::load_hero_visual` читает набор, декодирует CSX и собирает готовые листы кадров формулами `FUN_0042279C`. `gl_app::LevelScreen` загружает листы в текстуры и рисует hero как PAD (`position - anchor`), локальные эмпирические константы удаляются.

**Tech Stack:** C++20, CMake/Ninja, doctest, SDL2, существующие `gl_assets`/`gl_game`/`gl_render`; данные — `I:\Games\Златогорье-2` (`GL_GAME_DIR`).

## Global Constraints

- C++20; слои только вниз: `assets` ← `game` ← `app`; `gl_assets`/`gl_game` без SDL.
- Имена: типы `PascalCase`, функции/переменные `snake_case`, файлы `snake_case`.
- Без комментариев в коде без необходимости; без «TBD/TODO».
- Ассеты игры не коммитить; реальные тесты — только через `GL_GAME_DIR` (skip 77).
- Оригинал — источник истины: формулы/константы только из `docs/research/p4d-hero-wear.md`; подгонка запрещена.
- Ошибки формата — `ErrorCode::format`; отсутствующий слой — warning + пропуск (как в engine).
- После каждой задачи: сборка, целевые тесты; в конце — полный `ctest` с `GL_GAME_DIR`.
- Коммит — только по указанию пользователя.

---

### Task 1: HAD parser (`gl_assets`)

**Files:**
- Create: `src/assets/include/gl/assets/wear_had.hpp`
- Create: `src/assets/src/wear_had.cpp`
- Modify: `CMakeLists.txt` (add `src/assets/src/wear_had.cpp` после `iad.cpp`)
- Test: `tests/assets/test_wear_had.cpp`, `tests/assets/CMakeLists.txt`

**Interfaces:**
- Consumes: `gl::assets::ByteReader`, `Result`, `Error`.
- Produces:
  `struct WearHadAnimation { std::uint32_t type_mask, rows, columns, delay_ms, frame_width, frame_height; std::int32_t anchor_x, anchor_y; float movement_x, movement_y; std::int32_t layer_offset_x, layer_offset_y; std::int32_t shadow_offset_x, shadow_offset_y; std::uint32_t shadow_width, shadow_height; std::int32_t shadow_anchor_x, shadow_anchor_y; };`
  `Result<std::vector<WearHadAnimation>> parse_wear_had(std::span<const std::uint8_t> data);`

- [ ] **Step 1: Write the failing test**

```cpp
#include "gl/assets/wear_had.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace {
void put_u32(std::vector<std::uint8_t>& d, std::size_t o, std::uint32_t v) {
    std::memcpy(d.data() + o, &v, sizeof(v));
}
void put_f32(std::vector<std::uint8_t>& d, std::size_t o, float v) {
    std::memcpy(d.data() + o, &v, sizeof(v));
}
} // namespace

TEST_CASE("parse_wear_had reads one animation record") {
    std::vector<std::uint8_t> data(0x6C, 0);
    std::memcpy(data.data(), "HAD ", 4);
    put_u32(data, 4, static_cast<std::uint32_t>(data.size()));
    put_u32(data, 8, 0x4);
    std::size_t o = 0xC;
    put_u32(data, o + 0, 0x4);        // mask
    put_u32(data, o + 4, 92);         // size: 4 + 88 payload
    put_u32(data, o + 8, 88);         // delay
    put_u32(data, o + 12, 12);        // columns
    put_u32(data, o + 16, 158);       // layer_off_x
    put_u32(data, o + 20, 142);       // layer_off_y
    put_u32(data, o + 24, 48);        // frame_width
    put_u32(data, o + 28, 92);        // frame_height
    put_u32(data, o + 32, 24);        // anchor_x
    put_u32(data, o + 36, 77);        // anchor_y
    put_f32(data, o + 40, 5.5f);      // movement_x
    put_f32(data, o + 44, 4.0f);      // movement_y
    put_u32(data, o + 48, 8);         // crops bytes
    put_u32(data, o + 56, 148);       // shadow off_x
    put_u32(data, o + 60, 202);       // shadow off_y
    put_u32(data, o + 64, 55);        // shadow w
    put_u32(data, o + 68, 32);        // shadow h
    put_u32(data, o + 72, 34);        // shadow anchor x
    put_u32(data, o + 76, 17);        // shadow anchor y
    put_u32(data, o + 80, 8);         // shadow crops bytes

    auto parsed = gl::assets::parse_wear_had(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->size() == 1);
    const auto& a = parsed->front();
    CHECK(a.type_mask == 0x4);
    CHECK(a.rows == 5);
    CHECK(a.delay_ms == 88);
    CHECK(a.columns == 12);
    CHECK(a.frame_width == 48);
    CHECK(a.frame_height == 92);
    CHECK(a.anchor_x == 24);
    CHECK(a.anchor_y == 77);
    CHECK(a.movement_x == doctest::Approx(5.5f));
    CHECK(a.movement_y == doctest::Approx(4.0f));
    CHECK(a.layer_offset_x == 158);
    CHECK(a.layer_offset_y == 142);
    CHECK(a.shadow_offset_x == 148);
    CHECK(a.shadow_offset_y == 202);
    CHECK(a.shadow_width == 55);
    CHECK(a.shadow_height == 32);
    CHECK(a.shadow_anchor_x == 34);
    CHECK(a.shadow_anchor_y == 17);
}

TEST_CASE("parse_wear_had marks 9-row animations") {
    std::vector<std::uint8_t> data(0x6C, 0);
    std::memcpy(data.data(), "HAD ", 4);
    put_u32(data, 4, static_cast<std::uint32_t>(data.size()));
    std::size_t o = 0xC;
    put_u32(data, o + 0, 0x10);
    put_u32(data, o + 4, 92);
    put_u32(data, o + 8, 82);
    put_u32(data, o + 12, 12);
    put_u32(data, o + 16, 154);
    put_u32(data, o + 20, 140);
    put_u32(data, o + 24, 65);
    put_u32(data, o + 28, 95);
    put_u32(data, o + 32, 28);
    put_u32(data, o + 36, 79);
    put_u32(data, o + 48, 8);
    put_u32(data, o + 80, 8);
    auto parsed = gl::assets::parse_wear_had(data);
    REQUIRE(parsed.has_value());
    CHECK(parsed->front().rows == 9);
}

TEST_CASE("parse_wear_had rejects bad input") {
    std::vector<std::uint8_t> data(0x20, 0);
    CHECK_FALSE(gl::assets::parse_wear_had(data).has_value());
    std::memcpy(data.data(), "HAD ", 4);
    put_u32(data, 4, 0x20);
    put_u32(data, 8, 0x4);
    put_u32(data, 0xC, 0x4);
    put_u32(data, 0x10, 0x1000); // size beyond end
    CHECK_FALSE(gl::assets::parse_wear_had(data).has_value());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build; build\tests\assets\gl_assets_tests.exe --test-case=*wear_had*`
Expected: FAIL/compile error: `wear_had.hpp` not found.

- [ ] **Step 3: Write minimal implementation**

`src/assets/include/gl/assets/wear_had.hpp`:

```cpp
#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {

struct WearHadAnimation {
    std::uint32_t type_mask = 0;
    std::uint32_t rows = 0;
    std::uint32_t columns = 0;
    std::uint32_t delay_ms = 0;
    std::uint32_t frame_width = 0;
    std::uint32_t frame_height = 0;
    std::int32_t anchor_x = 0;
    std::int32_t anchor_y = 0;
    float movement_x = 0.0f;
    float movement_y = 0.0f;
    std::int32_t layer_offset_x = 0;
    std::int32_t layer_offset_y = 0;
    std::int32_t shadow_offset_x = 0;
    std::int32_t shadow_offset_y = 0;
    std::uint32_t shadow_width = 0;
    std::uint32_t shadow_height = 0;
    std::int32_t shadow_anchor_x = 0;
    std::int32_t shadow_anchor_y = 0;
};

Result<std::vector<WearHadAnimation>> parse_wear_had(std::span<const std::uint8_t> data);

} // namespace gl::assets
```

`src/assets/src/wear_had.cpp`:

```cpp
#include "gl/assets/wear_had.hpp"
#include "gl/assets/byte_reader.hpp"

#include <bit>
#include <cstring>
#include <string>

namespace gl::assets {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

bool known_mask(std::uint32_t mask) {
    switch (mask) {
        case 0x1: case 0x2: case 0x4: case 0x8: case 0x10: case 0x20: case 0x40:
        case 0x80: case 0x100: case 0x200: case 0x400: case 0x10000:
        case 0x20000: case 0x40000: case 0x80000:
            return true;
        default:
            return false;
    }
}

std::uint32_t rows_for_mask(std::uint32_t mask) {
    return (mask == 0x10 || mask == 0x20 || mask == 0x200) ? 9 : 5;
}

std::int32_t to_i32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
} // namespace

Result<std::vector<WearHadAnimation>> parse_wear_had(std::span<const std::uint8_t> data) {
    if (data.size() < 0x10) return format_error("wear had is shorter than its header");
    if (std::memcmp(data.data(), "HAD ", 4) != 0) return format_error("bad wear had magic");

    std::uint32_t end = 0;
    std::memcpy(&end, data.data() + 4, sizeof(end));
    if (end < 0xC || end > data.size()) return format_error("invalid wear had data end");

    std::vector<WearHadAnimation> out;
    std::size_t offset = 0xC;
    while (offset + 8 <= end) {
        std::uint32_t mask = 0, size = 0;
        std::memcpy(&mask, data.data() + offset, sizeof(mask));
        std::memcpy(&size, data.data() + offset + 4, sizeof(size));
        if (!known_mask(mask)) return format_error("unknown wear had animation mask");
        if (size < 4 + 44 + 28 || (size % 4) != 0 || offset + 4 + size > end)
            return format_error("invalid wear had record size");

        ByteReader r(data.subspan(offset + 8, size - 4));
        WearHadAnimation a;
        a.type_mask = mask;
        a.rows = rows_for_mask(mask);
        std::uint32_t raw = 0;
        if (!r.read_u32le(a.delay_ms) || !r.read_u32le(a.columns) ||
            !r.read_u32le(raw)) return format_error("truncated wear had fields");
        a.layer_offset_x = to_i32(raw);
        if (!r.read_u32le(raw)) return format_error("truncated wear had fields");
        a.layer_offset_y = to_i32(raw);
        if (!r.read_u32le(a.frame_width) || !r.read_u32le(a.frame_height) ||
            !r.read_u32le(raw)) return format_error("truncated wear had fields");
        a.anchor_x = to_i32(raw);
        if (!r.read_u32le(raw)) return format_error("truncated wear had fields");
        a.anchor_y = to_i32(raw);
        if (!r.read_u32le(raw)) return format_error("truncated wear had fields");
        a.movement_x = std::bit_cast<float>(raw);
        if (!r.read_u32le(raw)) return format_error("truncated wear had fields");
        a.movement_y = std::bit_cast<float>(raw);

        std::uint32_t crops_bytes = 0;
        if (!r.read_u32le(crops_bytes) || (crops_bytes % 8) != 0)
            return format_error("invalid wear had body crop table");
        if (!r.skip(crops_bytes)) return format_error("truncated wear had body crop table");

        if (!r.read_u32le(raw)) return format_error("truncated wear had shadow");
        a.shadow_offset_x = to_i32(raw);
        if (!r.read_u32le(raw)) return format_error("truncated wear had shadow");
        a.shadow_offset_y = to_i32(raw);
        if (!r.read_u32le(a.shadow_width) || !r.read_u32le(a.shadow_height) ||
            !r.read_u32le(raw)) return format_error("truncated wear had shadow");
        a.shadow_anchor_x = to_i32(raw);
        if (!r.read_u32le(raw)) return format_error("truncated wear had shadow");
        a.shadow_anchor_y = to_i32(raw);
        std::uint32_t shadow_bytes = 0;
        if (!r.read_u32le(shadow_bytes) || (shadow_bytes % 8) != 0)
            return format_error("invalid wear had shadow crop table");
        if (!r.skip(shadow_bytes)) return format_error("truncated wear had shadow crop table");
        if (!r.at_end()) return format_error("wear had record has trailing bytes");

        if (a.columns == 0 || a.frame_width == 0 || a.frame_height == 0)
            return format_error("wear had animation has empty frame");
        out.push_back(a);
        offset += 4 + size;
    }
    if (offset != end) return format_error("wear had records do not fill the data");
    if (out.empty()) return format_error("wear had contains no animations");
    return out;
}
} // namespace gl::assets
```

`CMakeLists.txt`: add `src/assets/src/wear_had.cpp` after `src/assets/src/iad.cpp`.

`tests/assets/CMakeLists.txt`: add `test_wear_had.cpp`.

- [ ] **Step 4: Run tests**

Run: `cmake --build build; build\tests\assets\gl_assets_tests.exe --test-case=*wear_had*`
Expected: PASS (3 cases).

- [ ] **Step 5: Commit**

```bash
git add src/assets/include/gl/assets/wear_had.hpp src/assets/src/wear_had.cpp CMakeLists.txt tests/assets/test_wear_had.cpp tests/assets/CMakeLists.txt
git commit -m "feat(assets): parse wear HAD animation records"
```

---

### Task 2: SEQ parser (`gl_assets`)

**Files:**
- Create: `src/assets/include/gl/assets/wear_seq.hpp`
- Create: `src/assets/src/wear_seq.cpp`
- Modify: `CMakeLists.txt`, `tests/assets/CMakeLists.txt`
- Test: `tests/assets/test_wear_seq.cpp`

**Interfaces:**
- Produces: `struct WearSequence { std::uint32_t stride, rows; std::vector<std::uint8_t> order; };` (index `row*stride + col`), `Result<WearSequence> parse_wear_seq(std::span<const std::uint8_t>)`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "gl/assets/wear_seq.hpp"
#include <doctest/doctest.h>
#include <cstdint>
#include <vector>

TEST_CASE("parse_wear_seq reads stride, rows and order") {
    std::vector<std::uint8_t> data{12};
    for (int i = 0; i < 24; ++i) data.push_back(static_cast<std::uint8_t>(2 + (i % 2)));
    auto seq = gl::assets::parse_wear_seq(data);
    REQUIRE(seq.has_value());
    CHECK(seq->stride == 12);
    CHECK(seq->rows == 2);
    REQUIRE(seq->order.size() == 24);
    CHECK(seq->order[0] == 2);
    CHECK(seq->order[1] == 3);
    CHECK(seq->order[23] == 3);
}

TEST_CASE("parse_wear_seq rejects bad data") {
    CHECK_FALSE(gl::assets::parse_wear_seq({}).has_value());
    std::vector<std::uint8_t> bad_stride{0, 1};
    CHECK_FALSE(gl::assets::parse_wear_seq(bad_stride).has_value());
    std::vector<std::uint8_t> too_big{2, 5, 1};
    CHECK_FALSE(gl::assets::parse_wear_seq(too_big).has_value());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `build\tests\assets\gl_assets_tests.exe --test-case=*wear_seq*`
Expected: FAIL (header missing).

- [ ] **Step 3: Implement**

`wear_seq.hpp`:

```cpp
#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {
struct WearSequence {
    std::uint32_t stride = 0;
    std::uint32_t rows = 0;
    std::vector<std::uint8_t> order;
};
Result<WearSequence> parse_wear_seq(std::span<const std::uint8_t> data);
} // namespace gl::assets
```

`wear_seq.cpp`:

```cpp
#include "gl/assets/wear_seq.hpp"
#include <string>

namespace gl::assets {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }
}

Result<WearSequence> parse_wear_seq(std::span<const std::uint8_t> data) {
    if (data.empty()) return format_error("wear seq is empty");
    WearSequence seq;
    seq.stride = data[0];
    if (seq.stride == 0) return format_error("wear seq stride is zero");
    if ((data.size() - 1) % seq.stride != 0) return format_error("wear seq size is not a multiple of stride");
    seq.rows = static_cast<std::uint32_t>((data.size() - 1) / seq.stride);
    seq.order.assign(data.begin() + 1, data.end());
    for (std::uint8_t value : seq.order)
        if (value > 4) return format_error("wear seq draw order exceeds 4");
    return seq;
}
} // namespace gl::assets
```

Add `src/assets/src/wear_seq.cpp` and `test_wear_seq.cpp` to CMake; add test target file.

- [ ] **Step 4: Run tests**

Run: `cmake --build build; build\tests\assets\gl_assets_tests.exe --test-case=*wear_seq*`
Expected: PASS (2 cases).

- [ ] **Step 5: Commit**

```bash
git add src/assets/include/gl/assets/wear_seq.hpp src/assets/src/wear_seq.cpp CMakeLists.txt tests/assets/test_wear_seq.cpp tests/assets/CMakeLists.txt
git commit -m "feat(assets): parse wear SEQ layer order"
```

---

### Task 3: Extend IAD parser (layer offsets + shadow crops)

**Files:**
- Modify: `src/assets/include/gl/assets/iad.hpp`
- Modify: `src/assets/src/iad.cpp`
- Test: `tests/assets/test_iad.cpp`

**Interfaces:**
- Produces: `IadCrop { std::int16_t x1, y1, x2, y2; }`;
  `IadBlock` fields `frame_width/height`, `layer_offset_x/y`, `crops`,
  `shadow_frame_width/height`, `shadow_layer_offset_x/y`, `shadow_crops`.
- Consumed later: `compose_wear_body/shadow` (Task 4) and `load_hero_visual` (Task 5).

- [ ] **Step 1: Update failing tests**

Replace `image_width/image_height`/`w/h` expectations and add shadow fields:

```cpp
    CHECK(block.layer_offset_x == 165);
    CHECK(block.layer_offset_y == 154);
    REQUIRE(block.crops.size() == 2);
    CHECK(block.crops[0].x1 == 1);
    CHECK(block.crops[0].y1 == 2);
    CHECK(block.crops[0].x2 == 32);
    CHECK(block.crops[0].y2 == 36);
    CHECK(block.crops[1].x1 == 3);

TEST_CASE("parse_iad reads shadow crops and offsets") {
    std::vector<std::uint8_t> data(88, 0);
    std::memcpy(data.data(), "IAD ", 4);
    put_u32(data, 4, 1);
    put_u32(data, 8, 80);   // block size: bytes after type
    put_u32(data, 12, 12);  // frames
    put_u32(data, 16, 36);  // frame width
    put_u32(data, 20, 37);  // frame height
    put_u32(data, 24, 165); // layer offset x
    put_u32(data, 28, 154); // layer offset y
    put_u32(data, 32, 8);   // body crops bytes
    put_u16(data, 36, 1);
    put_u16(data, 38, 5);
    put_u16(data, 40, 32);
    put_u16(data, 42, 36);
    put_u32(data, 44, 40);  // shadow frame width
    put_u32(data, 48, 20);  // shadow frame height
    put_u32(data, 52, 152); // shadow layer offset x
    put_u32(data, 56, 205); // shadow layer offset y
    put_u32(data, 60, 8);   // shadow crops bytes
    put_u16(data, 64, 3);
    put_u16(data, 66, 6);
    put_u16(data, 68, 33);
    put_u16(data, 70, 18);

    auto parsed = gl::assets::parse_iad(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->size() == 1);
    const auto& b = parsed->front();
    CHECK(b.shadow_frame_width == 40);
    CHECK(b.shadow_frame_height == 20);
    CHECK(b.shadow_layer_offset_x == 152);
    CHECK(b.shadow_layer_offset_y == 205);
    REQUIRE(b.shadow_crops.size() == 1);
    CHECK(b.shadow_crops[0].x1 == 3);
    CHECK(b.shadow_crops[0].y1 == 6);
    CHECK(b.shadow_crops[0].x2 == 33);
    CHECK(b.shadow_crops[0].y2 == 18);
}
```

The old synthetic test has no shadow section; keep it passing (shadow optional when the remaining size is 0).

- [ ] **Step 2: Run test to verify it fails**

Run: `build\tests\assets\gl_assets_tests.exe --test-case=*iad*`
Expected: compile error on `layer_offset_x`/`x1`.

- [ ] **Step 3: Implement**

`iad.hpp`:

```cpp
struct IadCrop {
    std::int16_t x1 = 0;
    std::int16_t y1 = 0;
    std::int16_t x2 = 0;
    std::int16_t y2 = 0;
};

struct IadBlock {
    std::uint32_t type = 0;
    std::uint32_t size = 0;
    std::uint32_t frames = 0;
    std::uint32_t frame_width = 0;
    std::uint32_t frame_height = 0;
    std::int32_t layer_offset_x = 0;
    std::int32_t layer_offset_y = 0;
    std::vector<IadCrop> crops;
    std::uint32_t shadow_frame_width = 0;
    std::uint32_t shadow_frame_height = 0;
    std::int32_t shadow_layer_offset_x = 0;
    std::int32_t shadow_layer_offset_y = 0;
    std::vector<IadCrop> shadow_crops;
};
```

`iad.cpp`: in the block loop read `layer_offset_x/y` as i32; read crops into `x1,y1,x2,y2`; then:

```cpp
        const std::size_t shadow_pos = offset + kHeaderSize + table_size;
        if (shadow_pos + 20 <= block_end) {
            std::uint32_t sw = 0, sh = 0, shadow_size = 0, off_x = 0, off_y = 0;
            if (!read_u32_at(data, shadow_pos, sw) ||
                !read_u32_at(data, shadow_pos + 4, sh) ||
                !read_u32_at(data, shadow_pos + 8, off_x) ||
                !read_u32_at(data, shadow_pos + 12, off_y) ||
                !read_u32_at(data, shadow_pos + 16, shadow_size))
                return format_error("truncated iad shadow header");
            block.shadow_frame_width = sw;
            block.shadow_frame_height = sh;
            block.shadow_layer_offset_x = static_cast<std::int32_t>(off_x);
            block.shadow_layer_offset_y = static_cast<std::int32_t>(off_y);
            if ((shadow_size % 8) != 0 || shadow_pos + 20 + shadow_size > block_end)
                return format_error("invalid iad shadow crop table");
            const std::size_t shadow_count = shadow_size / 8;
            block.shadow_crops.reserve(shadow_count);
            for (std::size_t i = 0; i < shadow_count; ++i) {
                IadCrop crop;
                const std::size_t entry = shadow_pos + 20 + i * 8;
                if (!read_i16_at(data, entry, crop.x1) || !read_i16_at(data, entry + 2, crop.y1) ||
                    !read_i16_at(data, entry + 4, crop.x2) || !read_i16_at(data, entry + 6, crop.y2))
                    return format_error("truncated iad shadow crop entry");
                block.shadow_crops.push_back(crop);
            }
        } else if (shadow_pos != block_end) {
            return format_error("truncated iad shadow section");
        }
```

Add a local `read_i16_at` overload already present; adapt existing crop loop to `x1/y1/x2/y2`.

- [ ] **Step 4: Run tests**

Run: `cmake --build build; build\tests\assets\gl_assets_tests.exe --test-case=*iad*`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/assets/include/gl/assets/iad.hpp src/assets/src/iad.cpp tests/assets/test_iad.cpp
git commit -m "feat(assets): extend IAD with layer offsets and shadow crops"
```

---

### Task 4: Wear compositor core (`gl_game`)

**Files:**
- Create: `src/game/include/gl/game/hero_wear.hpp`
- Create: `src/game/src/hero_wear.cpp`
- Modify: `src/game/CMakeLists.txt`
- Test: `tests/game/test_hero_wear.cpp`, `tests/game/CMakeLists.txt`

**Interfaces:**
- Consumes: `WearHadAnimation`, `IadBlock`, `WearSequence`, `Image`, `Rgba`.
- Produces:
  `struct WearLayerImages { const gl::assets::IadBlock* block; const gl::assets::Image* body; const gl::assets::Image* shadow; const gl::assets::WearSequence* seq; };`
  `gl::assets::Image compose_wear_body(const WearHadAnimation&, const gl::assets::Image& base, std::span<const WearLayerImages>);`
  `gl::assets::Image compose_wear_shadow(const WearHadAnimation&, const gl::assets::Image& base, std::span<const WearLayerImages>);`

- [ ] **Step 1: Write failing compositor tests**

```cpp
#include "gl/game/hero_wear.hpp"
#include <doctest/doctest.h>
#include <cstdint>
#include <vector>

using namespace gl;
using namespace gl::game;

namespace {
assets::Image make_image(std::uint32_t w, std::uint32_t h, assets::Rgba c) {
    assets::Image img; img.width = w; img.height = h; img.rgba.assign(w * h, c); return img;
}
} // namespace

TEST_CASE("compose_wear_body places a layer crop by engine formula") {
    assets::WearHadAnimation a;
    a.type_mask = 0x4; a.rows = 1; a.columns = 2; a.frame_width = 2; a.frame_height = 2;
    a.layer_offset_x = 3; a.layer_offset_y = 1;
    auto base = make_image(4, 2, assets::Rgba{1, 0, 0, 255});
    auto layer_img = make_image(4, 2, assets::Rgba{9, 9, 9, 255});
    assets::IadBlock block;
    block.frames = 2; block.frame_width = 2; block.frame_height = 2;
    block.layer_offset_x = 3; block.layer_offset_y = 1;
    block.crops = {{0, 0, 2, 2}, {0, 0, 2, 2}};
    assets::WearSequence seq; seq.stride = 2; seq.rows = 1; seq.order = {2, 2};
    WearLayerImages layer{&block, &layer_img, nullptr, &seq};
    WearLayerImages layers[1] = {layer};
    auto out = compose_wear_body(a, base, layers);
    CHECK(out.width == 4);
    CHECK(out.height == 2);
    // dest_x = col*2 + 3 + 0 - 3 = col*2 ; dest_y = 0 + 1 + 0 - 1 = 0
    CHECK(out.rgba[0].r == 9);
    CHECK(out.rgba[1].r == 9);
    CHECK(out.rgba[2].r == 9); // base at column 1, layer order 2 overwrites
}

TEST_CASE("compose_wear_shadow clips to the sheet") {
    assets::WearHadAnimation a;
    a.rows = 2; a.columns = 1; a.frame_width = 2; a.frame_height = 2;
    a.shadow_width = 2; a.shadow_height = 2;
    a.shadow_offset_x = 0; a.shadow_offset_y = 0;
    auto base = make_image(2, 4, assets::Rgba{3, 0, 0, 255});
    assets::IadBlock block;
    block.frames = 1; block.shadow_frame_width = 2; block.shadow_frame_height = 2;
    block.shadow_layer_offset_x = 0; block.shadow_layer_offset_y = 0;
    block.shadow_crops = {{0, 0, 2, 2}, {0, 0, 2, 2}};
    auto layer_img = make_image(2, 4, assets::Rgba{7, 0, 0, 255});
    WearLayerImages layer{&block, nullptr, &layer_img, nullptr};
    WearLayerImages layers[1] = {layer};
    auto out = compose_wear_shadow(a, base, layers);
    CHECK(out.width == 2);
    CHECK(out.height == 4);
    CHECK(out.rgba[0].r == 7);
    CHECK(out.rgba[6].r == 7);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build; build\tests\game\gl_game_tests.exe --test-case=*wear*`
Expected: FAIL (symbols missing).

- [ ] **Step 3: Implement**

`hero_wear.hpp` (Task 5 adds loader declarations here):

```cpp
#pragma once
#include "gl/assets/iad.hpp"
#include "gl/assets/image.hpp"
#include "gl/assets/result.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/assets/wear_had.hpp"
#include "gl/assets/wear_seq.hpp"
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace gl::game {

struct WearLayerImages {
    const gl::assets::IadBlock* block = nullptr;
    const gl::assets::Image* body = nullptr;
    const gl::assets::Image* shadow = nullptr;
    const gl::assets::WearSequence* seq = nullptr;
};

gl::assets::Image compose_wear_body(const gl::assets::WearHadAnimation& animation,
                                    const gl::assets::Image& base,
                                    std::span<const WearLayerImages> layers);
gl::assets::Image compose_wear_shadow(const gl::assets::WearHadAnimation& animation,
                                      const gl::assets::Image& base,
                                      std::span<const WearLayerImages> layers);

struct HeroOutfit {
    std::string wear_set = "noweapon_thrw";
    std::string weapon;
    std::string shield;
    std::string elbarms;
    std::string helmet;
    std::string armor;
};

} // namespace gl::game
```

`hero_wear.cpp` — compositor part:

```cpp
#include "gl/game/hero_wear.hpp"
#include "gl/assets/csx.hpp"
#include "gl/assets/log.hpp"
#include <cstring>

namespace gl::game {
namespace {
void blit(const gl::assets::Image& src, gl::assets::Image& dst, int dx, int dy,
          int sx, int sy, int w, int h) {
    if (w <= 0 || h <= 0) return;
    const int x0 = std::max(0, -dx);
    const int y0 = std::max(0, -dy);
    const int x1 = std::min(w, static_cast<int>(dst.width) - dx);
    const int y1 = std::min(h, static_cast<int>(dst.height) - dy);
    if (x0 >= x1 || y0 >= y1) return;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const int ssx = sx + x;
            const int ssy = sy + y;
            if (ssx < 0 || ssy < 0 || ssx >= static_cast<int>(src.width) ||
                ssy >= static_cast<int>(src.height)) continue;
            const gl::assets::Rgba p = src.rgba[static_cast<std::size_t>(ssy) * src.width + ssx];
            if (p.a == 0) continue;
            dst.rgba[static_cast<std::size_t>(dy + y) * dst.width + (dx + x)] = p;
        }
    }
}

void draw_body_layer(const gl::assets::WearHadAnimation& a, std::uint32_t col,
                     std::uint32_t row, const WearLayerImages& layer,
                     gl::assets::Image& dst) {
    if (layer.block == nullptr || layer.body == nullptr || layer.seq == nullptr) return;
    const auto& b = *layer.block;
    const std::size_t index = static_cast<std::size_t>(row) * a.columns + col;
    if (index >= b.crops.size()) return;
    const auto crop = b.crops[index];
    const int dx = static_cast<int>(col * a.frame_width) + b.layer_offset_x + crop.x1 -
                   a.layer_offset_x;
    const int dy = static_cast<int>(row * a.frame_height) + b.layer_offset_y + crop.y1 -
                   a.layer_offset_y;
    const int sx = static_cast<int>(col * b.frame_width) + crop.x1;
    const int sy = static_cast<int>(row * b.frame_height) + crop.y1;
    blit(*layer.body, dst, dx, dy, sx, sy, crop.x2 - crop.x1, crop.y2 - crop.y1);
}

void draw_shadow_layer(const gl::assets::WearHadAnimation& a, std::uint32_t col,
                       std::uint32_t shadow_row, const WearLayerImages& layer,
                       gl::assets::Image& dst) {
    if (layer.block == nullptr || layer.shadow == nullptr) return;
    const auto& b = *layer.block;
    const std::size_t index = static_cast<std::size_t>(shadow_row) * a.columns + col;
    if (index >= b.shadow_crops.size()) return;
    const auto crop = b.shadow_crops[index];
    const int dx = static_cast<int>(col * b.shadow_frame_width) + b.shadow_layer_offset_x +
                   crop.x1 - a.shadow_offset_x;
    const int dy = static_cast<int>(shadow_row * b.shadow_frame_height) +
                   b.shadow_layer_offset_y + crop.y1 - a.shadow_offset_y;
    const int sx = static_cast<int>(col * b.shadow_frame_width) + crop.x1;
    const int sy = static_cast<int>(shadow_row * b.shadow_frame_height) + crop.y1;
    blit(*layer.shadow, dst, dx, dy, sx, sy, crop.x2 - crop.x1, crop.y2 - crop.y1);
}
} // namespace

gl::assets::Image compose_wear_body(const gl::assets::WearHadAnimation& animation,
                                    const gl::assets::Image& base,
                                    std::span<const WearLayerImages> layers) {
    gl::assets::Image out;
    out.width = animation.columns * animation.frame_width;
    out.height = animation.rows * animation.frame_height;
    out.rgba.assign(static_cast<std::size_t>(out.width) * out.height, gl::assets::Rgba{});
    for (std::uint32_t row = 0; row < animation.rows; ++row) {
        for (std::uint32_t col = 0; col < animation.columns; ++col) {
            const std::size_t index = static_cast<std::size_t>(row) * animation.columns + col;
            for (std::uint32_t order = 0; order <= 4; ++order) {
                for (const WearLayerImages& layer : layers) {
                    if (layer.seq == nullptr || index >= layer.seq->order.size()) continue;
                    if (layer.seq->order[index] == order)
                        draw_body_layer(animation, col, row, layer, out);
                }
                if (order == 1) {
                    blit(base, out, static_cast<int>(col * animation.frame_width),
                         static_cast<int>(row * animation.frame_height),
                         static_cast<int>(col * animation.frame_width),
                         static_cast<int>(row * animation.frame_height),
                         static_cast<int>(animation.frame_width),
                         static_cast<int>(animation.frame_height));
                }
            }
        }
    }
    return out;
}

gl::assets::Image compose_wear_shadow(const gl::assets::WearHadAnimation& animation,
                                      const gl::assets::Image& base,
                                      std::span<const WearLayerImages> layers) {
    gl::assets::Image out;
    out.width = animation.columns * animation.shadow_width;
    out.height = (animation.rows * 2 - 2) * animation.shadow_height;
    out.rgba.assign(static_cast<std::size_t>(out.width) * out.height, gl::assets::Rgba{});
    for (std::uint32_t row = 0; row < animation.rows * 2 - 2; ++row) {
        for (std::uint32_t col = 0; col < animation.columns; ++col) {
            for (const WearLayerImages& layer : layers)
                draw_shadow_layer(animation, col, row, layer, out);
            blit(base, out, static_cast<int>(col * animation.shadow_width),
                 static_cast<int>(row * animation.shadow_height),
                 static_cast<int>(col * animation.shadow_width),
                 static_cast<int>(row * animation.shadow_height),
                 static_cast<int>(animation.shadow_width),
                 static_cast<int>(animation.shadow_height));
        }
    }
    return out;
}
} // namespace gl::game
```

Also add includes `<algorithm>`, `<cstddef>`. `HeroVisual` and the loader are
added in Task 5. In Task 4, write header with only compositor/HeroOutfit. Task 5 patch adds rest. Good.

Add `src/game/src/hero_wear.cpp` to `src/game/CMakeLists.txt`; `test_hero_wear.cpp` to tests/game.

- [ ] **Step 4: Run tests**

Run: `cmake --build build; build\tests\game\gl_game_tests.exe --test-case=*wear*`
Expected: PASS (2 cases).

- [ ] **Step 5: Commit**

```bash
git add src/game/include/gl/game/hero_wear.hpp src/game/src/hero_wear.cpp src/game/CMakeLists.txt tests/game/test_hero_wear.cpp tests/game/CMakeLists.txt
git commit -m "feat(game): compose wear body and shadow sheets"
```

---

### Task 5: HeroVisual loader + real-data parity tests

**Files:**
- Modify: `src/game/include/gl/game/hero_wear.hpp` (add `HeroAnimation`, `HeroVisual`, `load_hero_visual`)
- Modify: `src/game/src/hero_wear.cpp` (loader)
- Create: `tests/assets/test_hero_wear_real.cpp`
- Modify: `tests/assets/CMakeLists.txt` (add file to `gl_assets_real_tests`)

**Interfaces:**
- Consumes: parsers from Tasks 1–3, compositor Task 4, `GameVfs`, `decode_csx`.
- Produces: `Result<HeroVisual> load_hero_visual(const GameVfs&, const HeroOutfit&)`; `HeroVisual::find(mask)`, `animations()`, `outfit()`.

- [ ] **Step 1: Write failing real tests**

```cpp
#include "gl/assets/iad.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/assets/wear_had.hpp"
#include "gl/assets/wear_seq.hpp"
#include "gl/game/hero_wear.hpp"
#include <doctest/doctest.h>
#include <cstdlib>
#include <string>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::game::HeroOutfit slot0() {
    gl::game::HeroOutfit o;
    o.wear_set = "noweapon_thrw";
    o.armor = "dospeh_almaz";
    o.elbarms = "ponogi_goluboy";
    o.helmet = "shlem_goluboy";
    return o;
}
} // namespace
```

(Use `gl::game::HeroOutfit`, not `assets::`.)

```cpp
TEST_CASE("real wear: HAD noweapon_thrw has engine values" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto data = vfs->read("wear/noweapon_thrw/noweapon_thrw.had");
    REQUIRE(data.has_value());
    auto had = gl::assets::parse_wear_had(data.value());
    REQUIRE(had.has_value());
    CHECK(had->size() == 11);
    const gl::assets::WearHadAnimation* tb_stay = nullptr;
    for (const auto& a : *had) if (a.type_mask == 0x4) tb_stay = &a;
    REQUIRE(tb_stay != nullptr);
    CHECK(tb_stay->delay_ms == 88);
    CHECK(tb_stay->columns == 12);
    CHECK(tb_stay->frame_width == 48);
    CHECK(tb_stay->frame_height == 92);
    CHECK(tb_stay->anchor_x == 24);
    CHECK(tb_stay->anchor_y == 77);
    CHECK(tb_stay->layer_offset_x == 158);
    CHECK(tb_stay->layer_offset_y == 142);
    CHECK(tb_stay->rows == 5);
    const auto* tb_go = [&]() -> const gl::assets::WearHadAnimation* {
        for (const auto& a : *had) if (a.type_mask == 0x10) return &a;
        return nullptr;
    }();
    REQUIRE(tb_go != nullptr);
    CHECK(tb_go->rows == 9);
    CHECK(tb_go->delay_ms == 82);
    CHECK(tb_go->frame_width == 65);
    CHECK(tb_go->frame_height == 95);
    CHECK(tb_go->movement_x == doctest::Approx(5.833333f));
    CHECK(tb_go->movement_y == doctest::Approx(5.0f));
}

TEST_CASE("real wear: IAD armor and seq parse" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto iad_data = vfs->read("wear/noweapon_thrw/armor/dospeh_almaz/dospeh_almaz.iad");
    REQUIRE(iad_data.has_value());
    auto blocks = gl::assets::parse_iad(iad_data.value());
    REQUIRE(blocks.has_value());
    const gl::assets::IadBlock* type4 = nullptr;
    for (const auto& b : *blocks) if (b.type == 0x4) type4 = &b;
    REQUIRE(type4 != nullptr);
    CHECK(type4->frames == 12);
    CHECK(type4->frame_width == 36);
    CHECK(type4->frame_height == 37);
    CHECK(type4->layer_offset_x == 165);
    CHECK(type4->layer_offset_y == 154);
    REQUIRE(type4->crops.size() == 60);
    CHECK(type4->crops[0].x1 == 1);
    CHECK(type4->crops[0].y1 == 5);
    CHECK(type4->shadow_frame_width == 40);
    CHECK(type4->shadow_frame_height == 20);
    CHECK(type4->shadow_layer_offset_x == 152);
    CHECK(type4->shadow_layer_offset_y == 205);
    REQUIRE(type4->shadow_crops.size() == 96);

    auto seq_data = vfs->read("wear/noweapon_thrw/seq/tb_stay_armor.seq");
    REQUIRE(seq_data.has_value());
    auto seq = gl::assets::parse_wear_seq(seq_data.value());
    REQUIRE(seq.has_value());
    CHECK(seq->stride == 12);
    CHECK(seq->rows == 5);
    for (std::uint8_t v : seq->order) CHECK(v == 2);
}

TEST_CASE("real wear: slot0 visual composes expected sheets" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto visual = gl::game::load_hero_visual(vfs.value(), slot0());
    REQUIRE(visual.has_value());
    const auto* idle = visual->find(0x4);
    REQUIRE(idle != nullptr);
    CHECK(idle->body.width == 576);
    CHECK(idle->body.height == 460);
    CHECK(idle->shadow.width == 660);
    CHECK(idle->shadow.height == 256);
    const auto* go = visual->find(0x10);
    REQUIRE(go != nullptr);
    CHECK(go->body.width == 780);
    CHECK(go->body.height == 855);
    const auto any_alpha = [&](int x, int y, int w, int h) {
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
                if (idle->body.rgba[static_cast<std::size_t>(yy) * idle->body.width + xx].a != 0)
                    return true;
        return false;
    };
    // Armor crop lands at (8,17), elbarms at (9,46), helmet at (18,3).
    CHECK(any_alpha(8, 17, 31, 31));
    CHECK(any_alpha(9, 46, 26, 42));
    CHECK(any_alpha(18, 3, 12, 20));
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build; build\tests\assets\gl_assets_real_tests.exe --test-case=*real wear*`
Expected: FAIL (loader missing).

- [ ] **Step 3: Implement loader**

Add to `hero_wear.hpp` after `HeroOutfit`:

```cpp
struct HeroAnimation {
    gl::assets::WearHadAnimation header;
    gl::assets::Image body;
    gl::assets::Image shadow;
};

class HeroVisual {
public:
    const HeroAnimation* find(std::uint32_t type_mask) const {
        auto it = animations_.find(type_mask);
        return it == animations_.end() ? nullptr : &it->second;
    }
    const HeroOutfit& outfit() const { return outfit_; }
    const std::map<std::uint32_t, HeroAnimation>& animations() const { return animations_; }

private:
    friend Result<HeroVisual> load_hero_visual(const gl::assets::GameVfs&, const HeroOutfit&);
    std::map<std::uint32_t, HeroAnimation> animations_;
    HeroOutfit outfit_;
};

Result<HeroVisual> load_hero_visual(const gl::assets::GameVfs& vfs, const HeroOutfit& outfit);
```

Add to `hero_wear.cpp` (includes `<optional>`, `<string>`):

```cpp
namespace {
bool load_csx(const gl::assets::GameVfs& vfs, const std::string& path,
              gl::assets::Image& out, std::string& reason) {
    auto data = vfs.read(path);
    if (!data) { reason = "cannot read " + path; return false; }
    auto image = gl::assets::decode_csx(data.value());
    if (!image) { reason = "cannot decode " + path + ": " + image.error().message; return false; }
    out.width = image->width;
    out.height = image->height;
    out.rgba = std::move(image->rgba);
    return true;
}

bool check_dims(const gl::assets::Image& image, std::uint64_t w, std::uint64_t h,
                const std::string& path, std::string& reason) {
    if (image.width == w && image.height == h) return true;
    reason = path + ": expected " + std::to_string(w) + "x" + std::to_string(h) +
             ", got " + std::to_string(image.width) + "x" + std::to_string(image.height);
    return false;
}

const gl::assets::IadBlock* find_block(const std::vector<gl::assets::IadBlock>& blocks,
                                       std::uint32_t mask) {
    for (const auto& b : blocks) if (b.type == mask) return &b;
    return nullptr;
}

std::string animation_name(std::uint32_t mask) {
    switch (mask) {
        case 0x1: return "rt_stay"; case 0x2: return "rt_fun";
        case 0x4: return "tb_stay"; case 0x8: return "tb_fun";
        case 0x10: return "tb_go"; case 0x20: return "rt_go";
        case 0x40: return "cast"; case 0x80: return "suffer";
        case 0x100: return "die"; case 0x200: return "run";
        case 0x400: return "ss_attack"; case 0x10000: return "hits0";
        case 0x20000: return "hits1"; case 0x40000: return "hits2";
        case 0x80000: return "hits3"; default: return "";
    }
}

struct LayerFiles {
    const char* category = nullptr;
    const std::string* item = nullptr;
    std::vector<gl::assets::IadBlock> blocks;
};

bool load_layer_files(const gl::assets::GameVfs& vfs, const std::string& set,
                      LayerFiles& layer) {
    if (layer.item == nullptr || layer.item->empty()) return false;
    const std::string base = "wear/" + set + "/" + layer.category + "/" + *layer.item + "/";
    auto data = vfs.read(base + *layer.item + ".iad");
    if (!data) {
        gl::log(gl::LogLevel::warning, "wear layer '" + base + "': cannot read IAD");
        return false;
    }
    auto blocks = gl::assets::parse_iad(data.value());
    if (!blocks) {
        gl::log(gl::LogLevel::warning, "wear layer '" + base + "': " + blocks.error().message);
        return false;
    }
    layer.blocks = std::move(blocks.value());
    return true;
}
} // namespace

Result<HeroVisual> load_hero_visual(const gl::assets::GameVfs& vfs, const HeroOutfit& outfit) {
    HeroVisual visual;
    visual.outfit_ = outfit;

    LayerFiles layers[5] = {
        {"weapon", &outfit.weapon, {}},
        {"shield", &outfit.shield, {}},
        {"elbarms", &outfit.elbarms, {}},
        {"helmet", &outfit.helmet, {}},
        {"armor", &outfit.armor, {}},
    };
    for (auto& layer : layers) load_layer_files(vfs, outfit.wear_set, layer);

    const std::string set = "wear/" + outfit.wear_set + "/";
    auto had_data = vfs.read(set + outfit.wear_set + ".had");
    if (!had_data)
        return Error{ErrorCode::io, "cannot read wear HAD for set '" + outfit.wear_set + "'"};
    auto had = gl::assets::parse_wear_had(had_data.value());
    if (!had) return had.error();

    for (const auto& header : had.value()) {
        const std::string anim = animation_name(header.type_mask);
        if (anim.empty()) continue;
        HeroAnimation out;
        out.header = header;
        std::string reason;
        const std::string body_path = set + "animation/" + anim + ".csx";
        if (!load_csx(vfs, body_path, out.body, reason)) {
            gl::log(gl::LogLevel::warning, reason + " (animation skipped)");
            continue;
        }
        if (!check_dims(out.body, static_cast<std::uint64_t>(header.columns) * header.frame_width,
                        static_cast<std::uint64_t>(header.rows) * header.frame_height,
                        body_path, reason))
            return Error{ErrorCode::format, reason};

        const std::string shadow_path = set + "shadows/" + anim + ".csx";
        if (!load_csx(vfs, shadow_path, out.shadow, reason))
            return Error{ErrorCode::format, reason};
        if (!check_dims(out.shadow,
                        static_cast<std::uint64_t>(header.columns) * header.shadow_width,
                        static_cast<std::uint64_t>(header.rows * 2 - 2) * header.shadow_height,
                        shadow_path, reason))
            return Error{ErrorCode::format, reason};

        struct LoadedLayer {
            gl::assets::Image body;
            gl::assets::Image shadow;
            gl::assets::WearSequence seq;
            const gl::assets::IadBlock* block = nullptr;
            bool valid = false;
        };
        std::array<LoadedLayer, 5> loaded{};
        for (std::size_t i = 0; i < 5; ++i) {
            const auto& layer = layers[i];
            const gl::assets::IadBlock* block = find_block(layer.blocks, header.type_mask);
            if (block == nullptr) continue;
            if (block->frames != header.columns)
                return Error{ErrorCode::format, std::string(layer.category) + " IAD frame count mismatch"};
            if (block->crops.size() < static_cast<std::size_t>(header.rows) * header.columns)
                return Error{ErrorCode::format, std::string(layer.category) + " IAD crop count mismatch"};
            if (block->shadow_crops.size() <
                static_cast<std::size_t>(header.rows * 2 - 2) * header.columns)
                return Error{ErrorCode::format, std::string(layer.category) + " IAD shadow crop count mismatch"};

            const std::string layer_base = set + layer.category + "/" + *layer.item + "/";
            auto& dst = loaded[i];
            if (!load_csx(vfs, layer_base + "animation/" + anim + ".csx", dst.body, reason)) {
                gl::log(gl::LogLevel::warning, reason + " (layer skipped)");
                continue;
            }
            if (!check_dims(dst.body,
                            static_cast<std::uint64_t>(header.columns) * block->frame_width,
                            static_cast<std::uint64_t>(header.rows) * block->frame_height,
                            layer_base + "animation/" + anim + ".csx", reason))
                return Error{ErrorCode::format, reason};

            if (!load_csx(vfs, layer_base + "shadows/" + anim + ".csx", dst.shadow, reason)) {
                gl::log(gl::LogLevel::warning, reason + " (shadow layer skipped)");
                continue;
            }
            if (!check_dims(dst.shadow,
                            static_cast<std::uint64_t>(header.columns) * block->shadow_frame_width,
                            static_cast<std::uint64_t>(header.rows * 2 - 2) * block->shadow_frame_height,
                            layer_base + "shadows/" + anim + ".csx", reason))
                return Error{ErrorCode::format, reason};

            auto seq_data = vfs.read(set + "seq/" + anim + "_" + layer.category + ".seq");
            if (!seq_data) {
                gl::log(gl::LogLevel::warning,
                        "wear seq missing: " + anim + "_" + layer.category + " (layer skipped)");
                continue;
            }
            auto seq = gl::assets::parse_wear_seq(seq_data.value());
            if (!seq) return seq.error();
            if (seq->stride != header.columns || seq->rows != header.rows)
                return Error{ErrorCode::format, "wear seq size mismatch: " + anim + "_" + layer.category};
            dst.seq = std::move(seq.value());
            dst.block = block;
            dst.valid = true;
        }

        std::vector<WearLayerImages> composed;
        composed.reserve(5);
        for (auto& layer : loaded)
            if (layer.valid)
                composed.push_back(WearLayerImages{layer.block, &layer.body, &layer.shadow, &layer.seq});

        out.body = compose_wear_body(header, out.body, composed);
        out.shadow = compose_wear_shadow(header, out.shadow, composed);
        visual.animations_[header.type_mask] = std::move(out);
    }
    if (visual.animations_.find(0x4) == visual.animations_.end())
        return Error{ErrorCode::format, "wear visual has no tb_stay animation"};
    return visual;
}
```

**Notes for Step 3:**
- Add `#include <array>` to `hero_wear.cpp`.
- Removing layer collection code below is intentional; do not keep both variants.
- `LayerFiles::blocks` stays alive for the whole function, so `block` pointers in
  `composed` are valid during composition.

- [ ] **Step 4: Run real tests**

Run: `$env:GL_GAME_DIR="I:\Games\Златогорье-2"; cmake --build build; build\tests\assets\gl_assets_real_tests.exe --test-case="*real wear*"`
Expected: PASS (3 cases).

- [ ] **Step 5: Add sweep + golden test**

Add to `test_hero_wear_real.cpp`:

```cpp
TEST_CASE("real wear: all sets parse HAD/IAD/SEQ" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const char* sets[] = {"bows", "noweapon_thrw", "onehand_wpn", "two_axes_maces",
                          "two_spears_staffs", "two_swords", "xbows_guns"};
    for (const char* set_name : sets) {
        auto had_data = vfs->read(std::string("wear/") + set_name + "/" + set_name + ".had");
        REQUIRE(had_data.has_value());
        auto had = gl::assets::parse_wear_had(had_data.value());
        REQUIRE(had.has_value());
        CHECK(!had->empty());
        for (const auto& entry : vfs->list()) {
            if (entry.path.rfind(std::string("wear/") + set_name + "/", 0) != 0) continue;
            if (entry.path.size() < 7) continue;
            const std::string ext = entry.path.substr(entry.path.size() - 4);
            if (ext == ".iad") {
                auto data = vfs->read(entry.path);
                REQUIRE(data.has_value());
                CHECK(gl::assets::parse_iad(data.value()).has_value());
            } else if (ext == ".seq") {
                auto data = vfs->read(entry.path);
                REQUIRE(data.has_value());
                CHECK(gl::assets::parse_wear_seq(data.value()).has_value());
            }
        }
    }
}
```

Add golden hash:
```cpp
TEST_CASE("real wear: slot0 idle sheet matches golden hash" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto visual = gl::game::load_hero_visual(vfs.value(), slot0());
    REQUIRE(visual.has_value());
    const auto* idle = visual->find(0x4);
    REQUIRE(idle != nullptr);
    std::uint64_t hash = 1469598103934665603ull;
    for (const auto& p : idle->body.rgba) {
        hash ^= (static_cast<std::uint64_t>(p.r) << 24) | (static_cast<std::uint64_t>(p.g) << 16) |
                (static_cast<std::uint64_t>(p.b) << 8) | p.a;
        hash *= 1099511628211ull;
    }
    CHECK(hash == 0x1A0C2E25C84A1174ull);
}
```
The expected hash was computed from the engine-exact composition of
`noweapon_thrw` slot0 idle (Almaz + Goluboy + Shlem_goluboy) with the research
formula. If it differs, do not update the number blindly: compare the first
differing pixel and the layer offsets against §6 before changing the test.

- [ ] **Step 6: Run full targeted tests**

Run: `$env:GL_GAME_DIR="I:\Games\Златогорье-2"; build\tests\assets\gl_assets_real_tests.exe --test-case="*real wear*"`
Expected: PASS (5 cases), 0 failed.

- [ ] **Step 7: Commit**

```bash
git add src/game/include/gl/game/hero_wear.hpp src/game/src/hero_wear.cpp tests/assets/test_hero_wear_real.cpp tests/assets/CMakeLists.txt
git commit -m "feat(game): load wear hero visual and verify engine parity"
```

---

### Task 6: LevelScreen + CLI + screenshot tests

**Files:**
- Modify: `src/app/include/gl/app/level_screen.hpp`
- Modify: `src/app/src/level_screen.cpp`
- Modify: `tools/glview/main.cpp`
- Modify: `tools/glgame/main.cpp`
- Modify: `tests/render/test_glview_screenshot.cpp`
- Modify: `tests/app/test_glgame_screenshot.cpp` (if it references hero flags)
- Modify: `tools/glview/CMakeLists.txt` if needed (no new sources)

**Interfaces:**
- Consumes: `gl::game::HeroOutfit`, `load_hero_visual`, `HeroVisual::find`, `HeroAnimation`, `pad_person_row`, `pad_shadow_row`, `person_frame_index`.
- Produces: `LevelScreenOptions::draw_hero_wear`, `LevelScreen::load(..., const gl::game::HeroOutfit* hero_outfit = nullptr)`.

- [ ] **Step 1: Add failing screenshot test**

In `tests/render/test_glview_screenshot.cpp` add:

```cpp
TEST_CASE("glview renders the wear hero for slot0") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    TempDir temp;
    const fs::path a = temp.path / "hero_wear_a.png";
    const fs::path b = temp.path / "hero_wear_b.png";
    const fs::path plain = temp.path / "hero_plain.png";
    const std::vector<std::string> common = {"--no-persons", "--no-masks",
                                             "--camera-x", "1982", "--camera-y", "94",
                                             "--sim-ms", "0"};
    auto with = common;
    with.push_back("--hero-wear");
    CHECK(run_glview(glview_arguments(game_dir, a, with)) == 0);
    CHECK(run_glview(glview_arguments(game_dir, b, with)) == 0);
    CHECK(run_glview(glview_arguments(game_dir, plain, common)) == 0);
    REQUIRE(fs::exists(a));
    REQUIRE(fs::exists(b));
    REQUIRE(fs::exists(plain));

    int aw = 0, ah = 0, ac = 0, pw = 0, ph = 0, pc = 0;
    const StbiImage a_pixels = load_png(a, aw, ah, ac);
    const StbiImage b_pixels = load_png(b, aw, ah, ac);
    const StbiImage p_pixels = load_png(plain, pw, ph, pc);
    REQUIRE(a_pixels != nullptr);
    REQUIRE(b_pixels != nullptr);
    REQUIRE(p_pixels != nullptr);
    CHECK(aw == 1280);
    CHECK(ah == 720);
    std::uint64_t diff = 0;
    for (int i = 0; i < aw * ah; ++i) {
        const unsigned char* pa = a_pixels.get() + i * 4;
        const unsigned char* pb = b_pixels.get() + i * 4;
        const unsigned char* pp = p_pixels.get() + i * 4;
        if (pa[0] != pb[0] || pa[1] != pb[1] || pa[2] != pb[2] || pa[3] != pb[3])
            ADD_FAILURE("non-deterministic hero frame");
        if (pa[0] != pp[0] || pa[1] != pp[1] || pa[2] != pp[2] || pa[3] != pp[3])
            ++diff;
    }
    CAPTURE(diff);
    CHECK(diff > 500);
}
```

Add a second check in the same case:
```cpp
    const fs::path later = temp.path / "hero_wear_later.png";
    auto later_args = with;
    for (auto& arg : later_args) if (arg == "0") arg = "1000";
    CHECK(run_glview(glview_arguments(game_dir, later, later_args)) == 0);
    // load and compare `later` with `a`; require at least one differing pixel.
```
Use the local `load_png` and count differing pixels between `a` and `later`.

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build; build\tests\render\glview_screenshot_tests.exe --test-case="*wear hero*"`
Expected: FAIL (`--hero-wear` unknown / no hero).

- [ ] **Step 3: Modify LevelScreen**

Header changes:

```cpp
#include <gl/game/hero_wear.hpp>
...
struct LevelScreenOptions {
    ...
    bool draw_hero = true;
    bool draw_hero_wear = false;
    // remove draw_hero_base / draw_hero_armor
};
...
static Result<LevelScreen> load(SDL_Renderer* renderer, gl::assets::GameVfs& vfs,
                                std::string_view level_name, bool load_persons = true,
                                const gl::game::HeroOutfit* hero_outfit = nullptr);
```

Private:
```cpp
    struct HeroTextures {
        gl::render::Texture body;
        std::optional<gl::render::Texture> shadow;
    };
    std::map<std::uint32_t, HeroTextures> hero_textures_;
    std::map<std::uint32_t, gl::assets::WearHadAnimation> hero_headers_;
```
Remove `hero_base_`, `hero_go_base_`, `hero_armor_`, `hero_legs_`,
`hero_bow_base_`, `hero_helmet_`. Add `#include <map>`.
Replace `draw_hero_base` with `void draw_hero_wear(SDL_Renderer*, std::uint64_t) const;`.
Update move ctor member list (`hero_textures_`, `hero_headers_`).

In `load()` after `ensure_hero()`:
```cpp
    if (hero_outfit != nullptr) {
        auto visual = gl::game::load_hero_visual(vfs, *hero_outfit);
        if (!visual) {
            gl::log(gl::LogLevel::warning,
                    "level '" + screen.level_.name + "': hero wear: " +
                        visual.error().message);
        } else {
            for (const auto& [mask, anim] : visual->animations()) {
                auto body = gl::render::Texture::from_image(renderer, anim.body);
                auto shadow = gl::render::Texture::from_image(renderer, anim.shadow);
                if (!body || !shadow) {
                    gl::log(gl::LogLevel::warning,
                            "level '" + screen.level_.name + "': cannot upload hero wear sheet");
                    continue;
                }
                screen.hero_headers_[mask] = anim.header;
                screen.hero_textures_[mask] = HeroTextures{std::move(body.value()),
                                                           std::move(shadow.value())};
            }
        }
    }
```

Remove all old hero base/armor/legs/helmet load blocks.

In `render()`:
```cpp
                        if (pass == PersonPass::body) {
                            if (options.draw_hero_wear && !hero_textures_.empty()) {
                                draw_hero_wear(renderer, elapsed_ms);
                            } else {
                                draw_hero_marker(renderer);
                            }
                        } else if (pass == PersonPass::shadow) {
                            if (options.draw_hero_wear && !hero_textures_.empty())
                                draw_hero_wear(renderer, elapsed_ms);
                        }
```
(Shadow draw from the same function is acceptable; it checks `pass`.)
Better keep separate: `draw_hero_wear(renderer, elapsed_ms, PersonPass pass)` private
signature; body pass draws body, shadow pass draws shadow.

Implement:

```cpp
void LevelScreen::draw_hero_wear(SDL_Renderer* renderer, std::uint64_t elapsed_ms,
                                 PersonPass pass) const {
    if (!hero_ || !(camera_.zoom > 0.0f)) return;
    const gl::game::Hero& hero = hero_->controller.hero();
    const bool moving = hero.has_goal && !hero.path.empty();
    const std::uint32_t mask = moving ? 0x10u : 0x4u;
    auto tex = hero_textures_.find(mask);
    auto hdr = hero_headers_.find(mask);
    if (tex == hero_textures_.end() || hdr == hero_headers_.end()) return;
    const auto& header = hdr->second;
    const std::uint8_t direction = moving ? gl::game::direction8_to16(hero.direction)
                                          : hero.direction;
    const auto type = moving ? gl::assets::PadAnimationType::tb_go
                             : gl::assets::PadAnimationType::tb_stay;
    const std::size_t column = person_frame_index(
        elapsed_ms, static_cast<std::int32_t>(header.delay_ms),
        static_cast<std::int32_t>(header.columns));

    if (pass == PersonPass::shadow) {
        if (!tex->second.shadow) return;
        const auto row = gl::assets::pad_shadow_row(type, direction);
        const SDL_Rect source{static_cast<int>(column * header.shadow_width),
                              static_cast<int>(row.row * header.shadow_height),
                              static_cast<int>(header.shadow_width),
                              static_cast<int>(header.shadow_height)};
        const SDL_FRect dest = gl::render::to_screen(
            camera_, hero.x - static_cast<float>(header.shadow_anchor_x),
            hero.y - static_cast<float>(header.shadow_anchor_y),
            static_cast<float>(header.shadow_width),
            static_cast<float>(header.shadow_height));
        if (!rect_visible(dest, screen_w_, screen_h_)) return;
        SDL_SetTextureAlphaMod(tex->second.shadow->get(), 128);
        draw_source(renderer, *tex->second.shadow, source, dest);
        SDL_SetTextureAlphaMod(tex->second.shadow->get(), 255);
        return;
    }

    const auto row = gl::assets::pad_person_row(type, direction);
    const SDL_Rect source{static_cast<int>(column * header.frame_width),
                          static_cast<int>(row.row * header.frame_height),
                          static_cast<int>(header.frame_width),
                          static_cast<int>(header.frame_height)};
    const SDL_FRect dest = gl::render::to_screen(
        camera_, hero.x - static_cast<float>(header.anchor_x),
        hero.y - static_cast<float>(header.anchor_y),
        static_cast<float>(header.frame_width),
        static_cast<float>(header.frame_height));
    if (!rect_visible(dest, screen_w_, screen_h_)) return;
    draw_source(renderer, tex->second.body, source, dest,
                row.mirror ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
```

`gl::assets::pad_shadow_row` accepts `PadAnimationType` and direction; for
`tb_go` it expects 16-direction input (identity 0..15); feed `direction`
(already 16 for moving, 8 for idle). For idle shadow, rows 0..7 identity; feed
`direction` (0..7). Good.

- [ ] **Step 4: CLI and glgame**

`tools/glview/main.cpp`:
- Replace `hero_base`/`hero_armor` with `bool hero_wear=false; gl::game::HeroOutfit outfit;`.
- Defaults: `wear_set="noweapon_thrw"`, `armor="dospeh_almaz"`, `elbarms="ponogi_goluboy"`, `helmet="shlem_goluboy"`.
- `--hero-wear` sets `hero_wear=true`.
- Parse `--hero-set`, `--hero-armor`, `--hero-elbarms`, `--hero-helmet`,
  `--hero-shield`, `--hero-weapon`, `--hero-outfit none` (none clears all
  layer names).
- Remove `--hero-base`/`--hero-armor`.
- Help text update.
- At LevelScreen::load: pass `options.hero_wear ? &options.outfit : nullptr`;
  set `screen_options.draw_hero_wear = options.hero_wear;`.
- Remove `draw_hero_base`/`draw_hero_armor` assignments.

`tools/glgame/main.cpp`:
- In `load_level` (around line 1148):
```cpp
        gl::game::HeroOutfit hero_outfit;
        auto screen = gl::app::LevelScreen::load(window_.renderer(), vfs_, name,
                                                 !options_.no_persons, &hero_outfit);
```
- In the level render options (around line 1598) replace `options.draw_hero_base = true;`
  with `options.draw_hero_wear = true;`.
Keep menu behavior unchanged.

- [ ] **Step 5: Update existing tests for removed flags**

Replace `--hero-base`/`--hero-armor` in tests with `--hero-wear`
(and `--hero-outfit none` where a bare base was used). Replace
`options.draw_hero_base`/`draw_hero_armor` references in code/tests. Keep
`--no-persons` semantics.

- [ ] **Step 6: Run targeted tests**

Run:
```
cmake --build build
build\tests\render\glview_screenshot_tests.exe
build\tests\app\glgame_screenshot_tests.exe
```
Expected: PASS; `--hero-wear` renders, deterministic, differs from plain.

- [ ] **Step 7: Commit**

```bash
git add src/app/include/gl/app/level_screen.hpp src/app/src/level_screen.cpp tools/glview/main.cpp tools/glgame/main.cpp tests/render/test_glview_screenshot.cpp tests/app/test_glgame_screenshot.cpp
git commit -m "feat(app): render hero from wear HAD/IAD/SEQ sheets"
```

---

### Task 7: Full verification + docs

**Files:**
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`, `docs/DECISIONS.md`,
  `docs/research/p4d-hero-wear.md` (only if implementation discovered new facts)
- Modify: `docs/specs/2026-10-25-p4d-hero-wear-design.md` (mark implemented)

- [ ] **Step 1: Build and run full ctest**

```powershell
$tools="I:\goldenland2dec\.local\toolchains"
$env:PATH="$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
$env:GL_GAME_DIR="I:\Games\Златогорье-2"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests passed, 0 failed; record durations/counts.

- [ ] **Step 2: Manual screenshots**

```powershell
build\bin\glview.exe --game-dir "I:\Games\Златогорье-2" --level l1_1 `
  --no-persons --no-masks --camera-x 1982 --camera-y 94 `
  --hero-wear --screenshot "$env:TEMP\p4d_hero_wear.png" --sim-ms 0
build\bin\glview.exe --game-dir "I:\Games\Златогорье-2" --level l1_1 `
  --no-persons --no-masks --camera-x 1982 --camera-y 94 `
  --hero-wear --screenshot "$env:TEMP\p4d_hero_wear_t1.png" --sim-ms 1000
```

Check: hero slot0 (синяя броня/шлем/поножи), тень, кадры различаются.

- [ ] **Step 3: Update docs**

- `STATUS.md`: P4d implemented, exact commands/counts, known limits,
  manual gate status.
- `PROJECT.md`: P4d `[~]`/`[x]` в таблице, краткий результат.
- `DECISIONS.md`: D-043 — P4d композитор: подход «композит при загрузке»,
  layer order `weapon,shield,elbarms,helmet,armor`, draw `position-anchor`,
  shadow formula; ссылка на research/spec.
- Spec: header «Реализовано …».
- Research: исправить неточности, если код выявил (без переписывания фактов).

- [ ] **Step 4: Commit**

```bash
git add docs/STATUS.md docs/PROJECT.md docs/DECISIONS.md docs/specs/2026-10-25-p4d-hero-wear-design.md docs/research/p4d-hero-wear.md
git commit -m "docs(p4d): record hero wear implementation and acceptance"
```

---

## Self-Review

- Spec coverage: HAD/IAD/SEQ (§5.1–5.3) → Tasks 1–3; композитор и
  `HeroVisual` (§5.4, §6) → Tasks 4–5; рендер/CLI (§5.5–5.6) → Task 6;
  тесты (§7) → Tasks 1–6; приёмка/доки (§8, §11) → Task 7.
- Placeholder scan: в плане нет TBD/TODO; golden FNV-хэш
  `0x1A0C2E25C84A1174` вычислен заранее по engine-формуле для slot0 idle.
- Type consistency: `IadCrop.x1/y1/x2/y2`, `WearLayerImages.block/body/shadow/seq`,
  `compose_wear_body/shadow`, `HeroOutfit`, `HeroAnimation`, `HeroVisual::find`,
  `LevelScreenOptions::draw_hero_wear` — имена совпадают во всех задачах.

