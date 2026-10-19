# P0 — Assets and Formats Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a portable C++20 library `gl_assets` and CLI `glpak` that mount the original
Goldenland 2 archives, extract every file, and decode CSX/SDB/ITM/BMP — verified against the
real game data at `I:\Games\Златогорье-2`.

**Architecture:** `gl_assets` is a platform-free static library: a byte reader, cp1251
conversion, PAK reader, VFS with update-overlay precedence, and self-contained format decoders.
`glpak` is a thin desktop CLI over it. Tests are doctest unit tests on synthetic bytes plus
an integration test gated by `GL_GAME_DIR`.

**Tech Stack:** C++20, CMake ≥ 3.20, Ninja, LLVM-MinGW (clang), miniz 3.0.2, stb_image/
stb_image_write (commit `013ac3beddff3dbffafd5177e7972067cd2b5083`), doctest v2.5.3.

## Global Constraints

- C++20, no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- `gl_assets` must not include SDL, Windows, or Android headers. Standard library allowed.
- All strings crossing the API are UTF-8; game data is cp1251.
- Paths are normalized to lowercase, forward slashes, case-insensitive lookup.
- Never commit or copy game assets. Tests may read `GL_GAME_DIR` at runtime only.
- No DRM, no `protect.dll`, no original binaries.
- Warnings are errors for first-party targets: MSVC `/W4 /WX`, Clang `-Wall -Wextra -Werror`.
- Every task ends with a commit. Do not commit `build/` or downloaded toolchains.
- Exact expected archive facts:
  - `scripts.pak`: 4485 files, 9 324 686 uncompressed bytes.
  - `sdb.pak`: 45 files, 1 411 188 bytes.
  - `items.pak`: 3786 files, 28 847 595 bytes.
  - `engineres.pak`: 974 files.
- Target game directory for integration/acceptance: `I:\Games\Златогорье-2`.

## File Structure

```
CMakeLists.txt
cmake/CompilerWarnings.cmake
.clang-format
third_party/miniz/{miniz.c,miniz.h,miniz_common.h,miniz_tdef.c,miniz_tdef.h,
                   miniz_tinfl.c,miniz_tinfl.h,miniz_zip.c,miniz_zip.h,LICENSE}
third_party/stb/{stb_image.h,stb_image_write.h}
third_party/doctest/doctest.h
src/assets/include/gl/assets/{bytes,result,log,byte_reader,cp1251,pak,vfs,csx,sdb,itm,image}.hpp
src/assets/src/{byte_reader,cp1251,pak,vfs,csx,sdb,itm,image,stb_impl}.cpp
tools/glpak/main.cpp
tests/assets/{CMakeLists.txt,test_main.cpp,test_result.cpp,test_byte_reader.cpp,
              test_cp1251.cpp,test_pak.cpp,test_vfs.cpp,test_csx.cpp,test_sdb.cpp,
              test_itm.cpp,test_image.cpp,test_real_data.cpp}
```

Responsibilities:

- `byte_reader` — bounds-checked little-endian reads over `std::span`.
- `cp1251` — cp1251→UTF-8 and virtual path normalization.
- `pak` — PAK header/FAT parsing and entry decompression (streaming for files).
- `vfs` — mount loose files + base/update paks with precedence.
- `csx` — paletted RLE sprite → indexed + RGBA.
- `sdb` — string database (optional `SDB ` signature, XOR 0xAA).
- `itm` — item records with conditional blocks.
- `image` — BMP/PNG/JPG via stb, PNG export for debugging.
- `glpak` — CLI commands.

---

### Task 0: Toolchain and project skeleton

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/CompilerWarnings.cmake`
- Create: `.clang-format`
- Create: `tests/assets/CMakeLists.txt`
- Create: `tests/assets/test_main.cpp`
- Create: `tests/assets/test_smoke.cpp`
- Create: `third_party/**` (downloaded, pinned)

**Interfaces:**
- Produces: working `cmake -S . -B build -G Ninja`, `cmake --build build`,
  `ctest --test-dir build --output-on-failure`.

- [ ] **Step 1: Install build tools**

Run in PowerShell:

```powershell
winget install --id Kitware.CMake -e --accept-source-agreements --accept-package-agreements
winget install --id Ninja-build.Ninja -e --accept-source-agreements --accept-package-agreements
$tools = "$env:USERPROFILE\.local\toolchains"
New-Item -ItemType Directory -Force $tools | Out-Null
$zip = "$tools\llvm-mingw-20261006-ucrt-x86_64.zip"
Invoke-WebRequest -Uri "https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/llvm-mingw-20261006-ucrt-x86_64.zip" -OutFile $zip
Expand-Archive -LiteralPath $zip -DestinationPath $tools -Force
```

Expected: `C:\Program Files\CMake\bin\cmake.exe`, `ninja.exe` on PATH after a new shell,
and `$tools\llvm-mingw-20261006-ucrt-x86_64\bin\clang++.exe`.

- [ ] **Step 2: Vendor pinned third-party sources**

```powershell
$root = "I:\goldenland2dec"
$miniz = "$root\third_party\miniz"
$stb = "$root\third_party\stb"
$doctest = "$root\third_party\doctest"
New-Item -ItemType Directory -Force $miniz,$stb,$doctest | Out-Null
$minizFiles = @("miniz.c","miniz.h","miniz_common.h","miniz_tdef.c","miniz_tdef.h",
  "miniz_tinfl.c","miniz_tinfl.h","miniz_zip.c","miniz_zip.h","LICENSE")
foreach ($f in $minizFiles) {
  Invoke-WebRequest -Uri "https://raw.githubusercontent.com/richgel999/miniz/3.0.2/$f" -OutFile "$miniz\$f"
}
$stbCommit = "013ac3beddff3dbffafd5177e7972067cd2b5083"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/$stbCommit/stb_image.h" -OutFile "$stb\stb_image.h"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/$stbCommit/stb_image_write.h" -OutFile "$stb\stb_image_write.h"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/doctest/doctest/v2.5.3/doctest/doctest.h" -OutFile "$doctest\doctest.h"
```

Expected: 10 miniz files, 2 stb headers, 1 doctest header.

- [ ] **Step 3: Write the root CMake files**

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(goldenland2dec LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(GL_BUILD_TESTS "Build tests" ON)
option(GL_BUILD_TOOLS "Build CLI tools" ON)

add_subdirectory(third_party/miniz)

add_library(gl_assets STATIC
    src/assets/src/byte_reader.cpp
    src/assets/src/cp1251.cpp
    src/assets/src/pak.cpp
    src/assets/src/vfs.cpp
    src/assets/src/csx.cpp
    src/assets/src/sdb.cpp
    src/assets/src/itm.cpp
    src/assets/src/image.cpp
    src/assets/src/stb_impl.cpp
)
target_include_directories(gl_assets PUBLIC src/assets/include)
target_link_libraries(gl_assets PUBLIC miniz)
target_include_directories(gl_assets PRIVATE third_party/stb)
include(cmake/CompilerWarnings.cmake)
gl_enable_warnings(gl_assets)

if (GL_BUILD_TOOLS)
    add_subdirectory(tools/glpak)
endif()

if (GL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/assets)
endif()
```

`third_party/miniz/CMakeLists.txt`:

```cmake
add_library(miniz STATIC miniz.c miniz_tdef.c miniz_tinfl.c miniz_zip.c)
target_include_directories(miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
```

`cmake/CompilerWarnings.cmake`:

```cmake
function(gl_enable_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()
```

`.clang-format`:

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
NamespaceIndentation: None
SortIncludes: CaseSensitive
```

- [ ] **Step 4: Write a smoke test**

`tests/assets/CMakeLists.txt`:

```cmake
add_executable(gl_assets_tests
    test_main.cpp
    test_smoke.cpp
)
target_include_directories(gl_assets_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
target_link_libraries(gl_assets_tests PRIVATE gl_assets)
gl_enable_warnings(gl_assets_tests)

add_test(NAME gl_assets_tests COMMAND gl_assets_tests)
```

`tests/assets/test_main.cpp`:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
```

`tests/assets/test_smoke.cpp`:

```cpp
#include <doctest/doctest.h>

TEST_CASE("smoke: doctest is wired") {
    CHECK(1 + 1 == 2);
}
```

Create empty `src/assets/src/*.cpp` files and placeholder headers listed in File Structure
so the library target configures. Each placeholder is `#pragma once` plus the namespace.

- [ ] **Step 5: Configure, build, run**

```powershell
$env:PATH = "$env:USERPROFILE\.local\toolchains\llvm-mingw-20261006-ucrt-x86_64\bin;$env:PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: configure succeeds, build succeeds without warnings, 1 test passes.

- [ ] **Step 6: Commit**

```powershell
git add CMakeLists.txt cmake .clang-format third_party tests/assets src/assets
git commit -m "build: add cmake project, pinned deps and doctest smoke test"
```

---

### Task 1: Result, logging, ByteReader

**Files:**
- Create: `src/assets/include/gl/assets/bytes.hpp`
- Create: `src/assets/include/gl/assets/result.hpp`
- Create: `src/assets/include/gl/assets/log.hpp`
- Create: `src/assets/src/log.cpp`
- Create: `src/assets/include/gl/assets/byte_reader.hpp`
- Create: `src/assets/src/byte_reader.cpp`
- Create: `tests/assets/test_result.cpp`
- Create: `tests/assets/test_byte_reader.cpp`
- Modify: `tests/assets/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `gl::ErrorCode{io, format, argument, unsupported}`, `gl::Error{code,message}`.
  - `gl::Result<T>` with `has_value()`, `operator bool`, `value()`, `error()`.
  - `gl::VoidResult = Result<std::monostate>`.
  - `gl::assets::Bytes = std::vector<std::uint8_t>`.
  - `gl::log(LogLevel, std::string_view)`.
  - `ByteReader(std::span<const std::uint8_t>)` with `read_u8`, `read_u16le`, `read_u32le`,
    `read_u64le`, `read_bytes(size_t, std::span<const std::uint8_t>&)`, `skip`, `remaining`,
    `position`, `at_end`.

- [ ] **Step 1: Write failing tests**

`tests/assets/test_result.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/result.hpp"

TEST_CASE("Result holds a value") {
    gl::Result<int> r(42);
    REQUIRE(r.has_value());
    CHECK(r.value() == 42);
}

TEST_CASE("Result holds an error") {
    gl::Result<int> r(gl::Error{gl::ErrorCode::format, "bad"});
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code == gl::ErrorCode::format);
    CHECK(r.error().message == "bad");
}
```

`tests/assets/test_byte_reader.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/byte_reader.hpp"

using gl::assets::ByteReader;

TEST_CASE("ByteReader reads little-endian integers") {
    const std::uint8_t data[] = {1,2,3,4,5,6,7,8};
    ByteReader r(data);
    std::uint32_t v = 0;
    REQUIRE(r.read_u32le(v));
    CHECK(v == 0x04030201u);
    std::uint64_t q = 0;
    REQUIRE(r.read_u64le(q));
    CHECK(q == 0x0807060504030201ull);
    CHECK(r.at_end());
}

TEST_CASE("ByteReader fails without advancing on short input") {
    const std::uint8_t data[] = {1,2,3};
    ByteReader r(data);
    std::uint32_t v = 0;
    CHECK_FALSE(r.read_u32le(v));
    CHECK(r.position() == 0);
}

TEST_CASE("ByteReader reads byte spans") {
    const std::uint8_t data[] = {'a','b','c'};
    ByteReader r(data);
    std::span<const std::uint8_t> out;
    REQUIRE(r.read_bytes(2, out));
    CHECK(out.size() == 2);
    CHECK(out[0] == 'a');
    CHECK(r.remaining().size() == 1);
}
```

Add both files to `gl_assets_tests` in `tests/assets/CMakeLists.txt`.

- [ ] **Step 2: Run tests to verify they fail**

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: compile fails because `gl/assets/result.hpp` and `byte_reader.hpp` do not exist.

- [ ] **Step 3: Implement the headers and source**

`bytes.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <vector>
namespace gl::assets { using Bytes = std::vector<std::uint8_t>; }
```

`result.hpp`:

```cpp
#pragma once
#include <string>
#include <variant>
namespace gl {
enum class ErrorCode { io, format, argument, unsupported };
struct Error { ErrorCode code; std::string message; };
template <typename T>
class Result {
public:
    Result(T value) : storage_(std::move(value)) {}
    Result(Error error) : storage_(std::move(error)) {}
    bool has_value() const { return storage_.index() == 0; }
    explicit operator bool() const { return has_value(); }
    T& value() { return std::get<0>(storage_); }
    const T& value() const { return std::get<0>(storage_); }
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }
    const Error& error() const { return std::get<1>(storage_); }
private:
    std::variant<T, Error> storage_;
};
using VoidResult = Result<std::monostate>;
}
```

`log.hpp`:

```cpp
#pragma once
#include <string_view>
namespace gl {
enum class LogLevel { debug, info, warning, error };
void set_log_level(LogLevel level);
LogLevel log_level();
void log(LogLevel level, std::string_view message);
}
```

Implement `log` in a new `src/assets/src/log.cpp` and add it to the root CMake source list:

```cpp
#include "gl/assets/log.hpp"
#include <cstdio>
namespace gl {
static LogLevel g_level = LogLevel::info;
void set_log_level(LogLevel level) { g_level = level; }
LogLevel log_level() { return g_level; }
void log(LogLevel level, std::string_view message) {
    if (level < g_level) return;
    const char* name = level == LogLevel::debug ? "debug" :
                       level == LogLevel::info ? "info" :
                       level == LogLevel::warning ? "warning" : "error";
    std::fprintf(stderr, "[%s] %.*s\n", name, static_cast<int>(message.size()), message.data());
}
}
```

`byte_reader.hpp`:

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace gl::assets {
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) : data_(data) {}
    bool read_u8(std::uint8_t& out);
    bool read_u16le(std::uint16_t& out);
    bool read_u32le(std::uint32_t& out);
    bool read_u64le(std::uint64_t& out);
    bool read_bytes(std::size_t n, std::span<const std::uint8_t>& out);
    bool skip(std::size_t n);
    std::span<const std::uint8_t> remaining() const { return data_.subspan(pos_); }
    std::size_t position() const { return pos_; }
    bool at_end() const { return pos_ >= data_.size(); }
private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};
}
```

`byte_reader.cpp`:

```cpp
#include "gl/assets/byte_reader.hpp"
namespace gl::assets {
bool ByteReader::read_u8(std::uint8_t& out) {
    if (pos_ + 1 > data_.size()) return false;
    out = data_[pos_++];
    return true;
}
bool ByteReader::read_u16le(std::uint16_t& out) {
    if (pos_ + 2 > data_.size()) return false;
    out = static_cast<std::uint16_t>(data_[pos_]) |
          static_cast<std::uint16_t>(data_[pos_ + 1] << 8);
    pos_ += 2;
    return true;
}
bool ByteReader::read_u32le(std::uint32_t& out) {
    if (pos_ + 4 > data_.size()) return false;
    out = static_cast<std::uint32_t>(data_[pos_]) |
          (static_cast<std::uint32_t>(data_[pos_ + 1]) << 8) |
          (static_cast<std::uint32_t>(data_[pos_ + 2]) << 16) |
          (static_cast<std::uint32_t>(data_[pos_ + 3]) << 24);
    pos_ += 4;
    return true;
}
bool ByteReader::read_u64le(std::uint64_t& out) {
    if (pos_ + 8 > data_.size()) return false;
    out = 0;
    for (int i = 7; i >= 0; --i) out = (out << 8) | data_[pos_ + i];
    pos_ += 8;
    return true;
}
bool ByteReader::read_bytes(std::size_t n, std::span<const std::uint8_t>& out) {
    if (pos_ + n > data_.size()) return false;
    out = data_.subspan(pos_, n);
    pos_ += n;
    return true;
}
bool ByteReader::skip(std::size_t n) {
    if (pos_ + n > data_.size()) return false;
    pos_ += n;
    return true;
}
}
```

- [ ] **Step 4: Run tests to verify they pass**

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/assets tests/assets CMakeLists.txt
git commit -m "feat(assets): add result type, logger and bounds-checked byte reader"
```

---

### Task 2: cp1251 conversion and path normalization

**Files:**
- Create: `src/assets/include/gl/assets/cp1251.hpp`
- Create: `src/assets/src/cp1251.cpp`
- Create: `tests/assets/test_cp1251.cpp`
- Modify: `tests/assets/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `std::string cp1251_to_utf8(std::span<const std::uint8_t>)`.
  - `std::string normalize_virtual_path(std::string_view)` — backslashes to `/`,
    collapse duplicate `/`, strip leading/trailing `/`, lowercase ASCII and `А–Я` (0xC0–0xDF).
  - `bool is_safe_relative_path(std::string_view)` — false for `..`, absolute paths, drive letters.

- [ ] **Step 1: Write failing tests**

`tests/assets/test_cp1251.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/cp1251.hpp"
#include <string>

TEST_CASE("cp1251: ASCII passes through") {
    const std::uint8_t s[] = {'H','e','l','l','o'};
    CHECK(gl::assets::cp1251_to_utf8(s) == "Hello");
}

TEST_CASE("cp1251: Cyrillic word") {
    const std::uint8_t s[] = {0xCF,0xF0,0xE8,0xE2,0xE5,0xF2}; // Привет
    CHECK(gl::assets::cp1251_to_utf8(s) == "Привет");
}

TEST_CASE("cp1251: special characters") {
    const std::uint8_t s[] = {0xA8,0xB8,0x88,0xB9}; // Ё ё € №
    CHECK(gl::assets::cp1251_to_utf8(s) == "Ёё€№");
}

TEST_CASE("paths normalize case and separators") {
    CHECK(gl::assets::normalize_virtual_path("Scripts\\DIALOGS\\Foo.CS") == "scripts/dialogs/foo.cs");
    CHECK(gl::assets::normalize_virtual_path("/A//B/") == "a/b");
    CHECK(gl::assets::normalize_virtual_path("ПРИВЕТ") == "привет");
}

TEST_CASE("unsafe paths are rejected") {
    CHECK_FALSE(gl::assets::is_safe_relative_path("../x"));
    CHECK_FALSE(gl::assets::is_safe_relative_path("a/../../x"));
    CHECK_FALSE(gl::assets::is_safe_relative_path("/abs"));
    CHECK_FALSE(gl::assets::is_safe_relative_path("C:/x"));
    CHECK(gl::assets::is_safe_relative_path("scripts/dialogs/x.cs"));
}
```

- [ ] **Step 2: Run tests to verify they fail**

Expected: compile fails, headers missing.

- [ ] **Step 3: Implement**

`cp1251.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
namespace gl::assets {
std::string cp1251_to_utf8(std::span<const std::uint8_t> input);
std::string normalize_virtual_path(std::string_view path);
bool is_safe_relative_path(std::string_view path);
}
```

`cp1251.cpp` — include the full table:

```cpp
#include "gl/assets/cp1251.hpp"
#include <array>

namespace gl::assets {
namespace {
constexpr char16_t kCp1251[128] = {
    0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,
    0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
    0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
    0x0098,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
    0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,
    0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
    0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,
    0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457,
    0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,
    0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
    0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,
    0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
    0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,
    0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
    0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,
    0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F,
};

void append_utf8(std::string& out, char16_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}
}

std::string cp1251_to_utf8(std::span<const std::uint8_t> input) {
    std::string out;
    out.reserve(input.size());
    for (std::uint8_t b : input) append_utf8(out, b < 0x80 ? b : kCp1251[b - 0x80]);
    return out;
}

std::string normalize_virtual_path(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    bool last_slash = false;
    for (char ch : path) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c == '\\') c = '/';
        if (c == '/' && last_slash) continue;
        last_slash = c == '/';
        if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
        else if (c >= 0xC0 && c <= 0xDF) c = static_cast<unsigned char>(c + 0x20);
        out.push_back(static_cast<char>(c));
    }
    std::size_t begin = 0;
    while (begin < out.size() && out[begin] == '/') ++begin;
    std::size_t end = out.size();
    while (end > begin && out[end - 1] == '/') --end;
    return out.substr(begin, end - begin);
}

bool is_safe_relative_path(std::string_view path) {
    if (path.empty()) return false;
    if (path.front() == '/' || path.front() == '\\') return false;
    if (path.size() >= 2 && path[1] == ':') return false;
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t slash = path.find_first_of("/\\", start);
        std::string_view part = path.substr(start, slash == std::string_view::npos
                                                      ? std::string_view::npos : slash - start);
        if (part == "..") return false;
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return true;
}
}
```

- [ ] **Step 4: Run tests**

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/assets tests/assets
git commit -m "feat(assets): add cp1251 conversion and virtual path normalization"
```

---

### Task 3: PAK archive reader

**Files:**
- Create: `src/assets/include/gl/assets/pak.hpp`
- Create: `src/assets/src/pak.cpp`
- Create: `tests/assets/test_pak.cpp`
- Modify: `tests/assets/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `struct PakEntry { std::string name; uint32_t size; uint32_t offset; };`
  - `class PakArchive`:
    - `static Result<PakArchive> open_from_file(const std::filesystem::path&)`
    - `static Result<PakArchive> open_from_memory(Bytes)`
    - `const std::vector<PakEntry>& entries() const`
    - `const PakEntry* find(std::string_view) const`
    - `Result<Bytes> read(const PakEntry&) const`
  - `constexpr std::uint64_t kMaxEntrySize = 512ull * 1024 * 1024;`
  - `std::string pak_entry_key(std::string_view)` → normalized lookup key.

- [ ] **Step 1: Write failing tests**

`tests/assets/test_pak.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/pak.hpp"
#include <miniz.h>
#include <filesystem>
#include <fstream>

using namespace gl::assets;

namespace {
void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}
Bytes zlib_compress(const Bytes& in) {
    mz_ulong out_len = mz_compressBound(in.size());
    Bytes out(out_len);
    REQUIRE(mz_compress(out.data(), &out_len, in.data(), in.size()) == MZ_OK);
    out.resize(out_len);
    return out;
}
Bytes make_pak(const std::vector<std::pair<std::string, Bytes>>& files) {
    Bytes header = {'P','A','K',' ',0,0,0,0};
    put_u32(header, static_cast<std::uint32_t>(files.size()));
    Bytes fat;
    std::vector<Bytes> payloads;
    for (const auto& [name, content] : files) {
        put_u32(fat, static_cast<std::uint32_t>(name.size()));
        fat.insert(fat.end(), name.begin(), name.end());
        Bytes compressed = zlib_compress(content);
        Bytes payload;
        put_u32(payload, 0);
        put_u32(payload, static_cast<std::uint32_t>(content.size()));
        payload.insert(payload.end(), compressed.begin(), compressed.end());
        put_u32(fat, static_cast<std::uint32_t>(payload.size()));
        put_u32(fat, 0); // offset patched below
        payloads.push_back(std::move(payload));
    }
    std::uint32_t running = static_cast<std::uint32_t>(header.size() + fat.size());
    std::size_t pos = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        pos += 4 + files[i].first.size() + 4;
        fat[pos + 0] = running & 0xFF;
        fat[pos + 1] = (running >> 8) & 0xFF;
        fat[pos + 2] = (running >> 16) & 0xFF;
        fat[pos + 3] = (running >> 24) & 0xFF;
        pos += 4;
        running += static_cast<std::uint32_t>(payloads[i].size());
    }
    Bytes pak = header;
    pak.insert(pak.end(), fat.begin(), fat.end());
    for (const auto& p : payloads) pak.insert(pak.end(), p.begin(), p.end());
    return pak;
}
}

TEST_CASE("PAK: reads zlib entries from memory") {
    Bytes pak = make_pak({{"scripts/a.scr", {'h','e','l','l','o'}}});
    auto archive = PakArchive::open_from_memory(std::move(pak));
    REQUIRE(archive.has_value());
    REQUIRE(archive->entries().size() == 1);
    CHECK(archive->entries()[0].name == "scripts/a.scr");
    auto data = archive->read(archive->entries()[0]);
    REQUIRE(data.has_value());
    CHECK(Bytes(data->begin(), data->end()) == Bytes{'h','e','l','l','o'});
}

TEST_CASE("PAK: rejects bad magic and truncated data") {
    Bytes bad = {'N','O','P','E',0,0,0,0};
    auto r = PakArchive::open_from_memory(std::move(bad));
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code == gl::ErrorCode::format);

    Bytes pak = make_pak({{"a", {'x'}}});
    pak.resize(pak.size() - 1);
    auto t = PakArchive::open_from_memory(std::move(pak));
    // Truncation may be detected at read time, not open time.
    if (t.has_value()) {
        auto data = t->read(t->entries()[0]);
        CHECK_FALSE(data.has_value());
    }
}

TEST_CASE("PAK: opens from a file") {
    Bytes pak = make_pak({{"a", {'x','y'}}, {"b", {'z'}}});
    auto path = std::filesystem::temp_directory_path() / "gl_pak_test.pak";
    { std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(pak.data()), pak.size()); }
    auto archive = PakArchive::open_from_file(path);
    REQUIRE(archive.has_value());
    CHECK(archive->entries().size() == 2);
    auto data = archive->read(archive->entries()[1]);
    REQUIRE(data.has_value());
    CHECK(Bytes(data->begin(), data->end()) == Bytes{'z'});
    std::filesystem::remove(path);
}

TEST_CASE("PAK: lookup is case-insensitive and slash-normalized") {
    Bytes pak = make_pak({{"Scripts\\A.SCR", {'x'}}});
    auto archive = PakArchive::open_from_memory(std::move(pak));
    REQUIRE(archive.has_value());
    CHECK(archive->find("scripts/a.scr") != nullptr);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Expected: compile fails, `pak.hpp` missing.

- [ ] **Step 3: Implement**

`pak.hpp`:

```cpp
#pragma once
#include "gl/assets/byte_reader.hpp"
#include "gl/assets/bytes.hpp"
#include "gl/assets/result.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace gl::assets {
constexpr std::uint64_t kMaxEntrySize = 512ull * 1024 * 1024;

struct PakEntry {
    std::string name;
    std::uint32_t size = 0;
    std::uint32_t offset = 0;
};

std::string pak_entry_key(std::string_view name);

class PakArchive {
public:
    static Result<PakArchive> open_from_file(const std::filesystem::path& path);
    static Result<PakArchive> open_from_memory(Bytes data);

    const std::vector<PakEntry>& entries() const { return entries_; }
    const PakEntry* find(std::string_view name) const;
    Result<Bytes> read(const PakEntry& entry) const;

private:
    PakArchive() = default;
    static Result<PakArchive> parse_memory(ByteReader& reader, std::uint64_t file_size);

    std::filesystem::path path_;
    Bytes memory_;
    bool in_memory_ = false;
    std::uint64_t file_size_ = 0;
    std::vector<PakEntry> entries_;
};
}
```

`pak.cpp` — key logic:

```cpp
#include "gl/assets/pak.hpp"
#include "gl/assets/cp1251.hpp"
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <miniz.h>

namespace gl::assets {
namespace {
bool read_stream_u32(std::istream& in, std::uint32_t& out) {
    unsigned char b[4];
    if (!in.read(reinterpret_cast<char*>(b), 4)) return false;
    out = std::uint32_t(b[0]) | (std::uint32_t(b[1]) << 8) |
          (std::uint32_t(b[2]) << 16) | (std::uint32_t(b[3]) << 24);
    return true;
}
Result<Bytes> read_payload(std::span<const std::uint8_t> payload, std::uint64_t entry_size) {
    if (entry_size < 4) return Error{ErrorCode::format, "entry too small"};
    ByteReader r(payload);
    std::uint32_t compressed = 0;
    if (!r.read_u32le(compressed)) return Error{ErrorCode::format, "missing compression flag"};
    if (compressed == 0) {
        std::uint32_t unpacked = 0;
        if (!r.read_u32le(unpacked)) return Error{ErrorCode::format, "missing unpacked size"};
        if (unpacked > kMaxEntrySize) return Error{ErrorCode::format, "entry too large"};
        Bytes out(unpacked);
        mz_ulong out_len = unpacked;
        int status = mz_uncompress(out.data(), &out_len, r.remaining().data(), r.remaining().size());
        if (status != MZ_OK || out_len != unpacked)
            return Error{ErrorCode::format, "zlib decompression failed"};
        return out;
    }
    Bytes out(r.remaining().begin(), r.remaining().end());
    if (out.size() > kMaxEntrySize) return Error{ErrorCode::format, "entry too large"};
    return out;
}
}

std::string pak_entry_key(std::string_view name) { return normalize_virtual_path(name); }

const PakEntry* PakArchive::find(std::string_view name) const {
    const std::string key = pak_entry_key(name);
    for (const auto& e : entries_) if (pak_entry_key(e.name) == key) return &e;
    return nullptr;
}

Result<PakArchive> PakArchive::parse_memory(ByteReader& reader, std::uint64_t file_size) {
    std::span<const std::uint8_t> magic;
    if (!reader.read_bytes(8, magic)) return Error{ErrorCode::format, "truncated header"};
    if (std::memcmp(magic.data(), "PAK \0\0\0\0", 8) != 0)
        return Error{ErrorCode::format, "bad PAK magic"};
    std::uint32_t count = 0;
    if (!reader.read_u32le(count)) return Error{ErrorCode::format, "missing file count"};
    if (count > 1'000'000) return Error{ErrorCode::format, "unreasonable file count"};
    PakArchive archive;
    archive.file_size_ = file_size;
    archive.entries_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t name_len = 0;
        if (!reader.read_u32le(name_len) || name_len == 0 || name_len > 4096)
            return Error{ErrorCode::format, "bad entry name length"};
        std::span<const std::uint8_t> name;
        if (!reader.read_bytes(name_len, name))
            return Error{ErrorCode::format, "truncated entry name"};
        std::uint32_t size = 0, offset = 0;
        if (!reader.read_u32le(size) || !reader.read_u32le(offset))
            return Error{ErrorCode::format, "truncated entry header"};
        if (size < 4 || std::uint64_t(offset) + size > file_size)
            return Error{ErrorCode::format, "entry outside archive"};
        archive.entries_.push_back({std::string(reinterpret_cast<const char*>(name.data()), name.size()),
                                    size, offset});
    }
    return archive;
}

Result<PakArchive> PakArchive::open_from_memory(Bytes data) {
    ByteReader reader(data);
    auto parsed = parse_memory(reader, data.size());
    if (!parsed) return parsed;
    parsed->memory_ = std::move(data);
    parsed->in_memory_ = true;
    return std::move(parsed);
}

Result<PakArchive> PakArchive::open_from_file(const std::filesystem::path& path) {
    std::error_code ec;
    const std::uint64_t file_size = std::filesystem::file_size(path, ec);
    if (ec) return Error{ErrorCode::io, "cannot stat PAK: " + path.string()};
    std::ifstream in(path, std::ios::binary);
    if (!in) return Error{ErrorCode::io, "cannot open PAK: " + path.string()};
    char magic[8];
    if (!in.read(magic, 8) || std::memcmp(magic, "PAK \0\0\0\0", 8) != 0)
        return Error{ErrorCode::format, "bad PAK magic"};
    std::uint32_t count = 0;
    if (!read_stream_u32(in, count) || count > 1'000'000)
        return Error{ErrorCode::format, "bad PAK file count"};
    PakArchive archive;
    archive.path_ = path;
    archive.file_size_ = file_size;
    archive.entries_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t name_len = 0;
        if (!read_stream_u32(in, name_len) || name_len == 0 || name_len > 4096)
            return Error{ErrorCode::format, "bad entry name length"};
        std::string name(name_len, '\0');
        if (!in.read(name.data(), name_len))
            return Error{ErrorCode::format, "truncated entry name"};
        std::uint32_t size = 0, offset = 0;
        if (!read_stream_u32(in, size) || !read_stream_u32(in, offset))
            return Error{ErrorCode::format, "truncated entry header"};
        if (size < 4 || std::uint64_t(offset) + size > file_size)
            return Error{ErrorCode::format, "entry outside archive"};
        archive.entries_.push_back({std::move(name), size, offset});
    }
    return archive;
}

Result<Bytes> PakArchive::read(const PakEntry& entry) const {
    if (entry.size < 4 || entry.size > kMaxEntrySize) return Error{ErrorCode::format, "bad entry size"};
    Bytes payload(entry.size);
    if (in_memory_) {
        if (std::uint64_t(entry.offset) + entry.size > memory_.size())
            return Error{ErrorCode::format, "entry outside archive"};
        std::memcpy(payload.data(), memory_.data() + entry.offset, entry.size);
    } else {
        std::ifstream in(path_, std::ios::binary);
        if (!in) return Error{ErrorCode::io, "cannot open PAK: " + path_.string()};
        in.seekg(static_cast<std::streamoff>(entry.offset));
        if (!in.read(reinterpret_cast<char*>(payload.data()), entry.size))
            return Error{ErrorCode::format, "truncated entry payload"};
    }
    return read_payload(payload, entry.size);
}
}
```

Add `src/assets/src/log.cpp` to CMake and include it in Task 1 if not already.

- [ ] **Step 4: Run tests**

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all PAK tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/assets tests/assets CMakeLists.txt
git commit -m "feat(assets): parse PAK archives and decompress zlib entries"
```

---

### Task 4: GameVfs with update overlays

**Files:**
- Create: `src/assets/include/gl/assets/vfs.hpp`
- Create: `src/assets/src/vfs.cpp`
- Create: `tests/assets/test_vfs.cpp`
- Modify: `tests/assets/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `struct ArchiveInfo { std::string name; std::size_t entry_count; bool is_update; int update_number; };`
  - `struct VfsEntry { std::string path; std::uint64_t size; std::string source; };`
  - `class GameVfs`:
    - `static Result<GameVfs> mount(const std::filesystem::path& game_dir)`
    - `Result<Bytes> read(std::string_view virtual_path) const`
    - `bool exists(std::string_view virtual_path) const`
    - `std::vector<VfsEntry> list() const`
    - `const std::vector<ArchiveInfo>& archives() const`
    - `const PakArchive* archive(std::string_view file_name) const` — exact archive file name.

Precedence: loose file `<game_dir>/<path>` → loose `<game_dir>/Data/<path>` →
`*.update.N.pak` by descending N → base `*.pak`.

- [ ] **Step 1: Write failing tests**

`tests/assets/test_vfs.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/vfs.hpp"
#include <miniz.h>
#include <filesystem>
#include <fstream>

using namespace gl::assets;

namespace {
void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}
Bytes make_pak(const std::vector<std::pair<std::string, Bytes>>& files) {
    Bytes header = {'P','A','K',' ',0,0,0,0};
    put_u32(header, static_cast<std::uint32_t>(files.size()));
    Bytes fat;
    std::vector<Bytes> payloads;
    for (const auto& [name, content] : files) {
        put_u32(fat, static_cast<std::uint32_t>(name.size()));
        fat.insert(fat.end(), name.begin(), name.end());
        mz_ulong bound = mz_compressBound(content.size());
        Bytes compressed(bound);
        mz_compress(compressed.data(), &bound, content.data(), content.size());
        compressed.resize(bound);
        Bytes payload;
        put_u32(payload, 0);
        put_u32(payload, static_cast<std::uint32_t>(content.size()));
        payload.insert(payload.end(), compressed.begin(), compressed.end());
        put_u32(fat, static_cast<std::uint32_t>(payload.size()));
        put_u32(fat, 0);
        payloads.push_back(std::move(payload));
    }
    std::uint32_t running = static_cast<std::uint32_t>(header.size() + fat.size());
    std::size_t pos = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        pos += 4 + files[i].first.size() + 4;
        fat[pos + 0] = running & 0xFF;
        fat[pos + 1] = (running >> 8) & 0xFF;
        fat[pos + 2] = (running >> 16) & 0xFF;
        fat[pos + 3] = (running >> 24) & 0xFF;
        pos += 4;
        running += static_cast<std::uint32_t>(payloads[i].size());
    }
    Bytes pak = header;
    pak.insert(pak.end(), fat.begin(), fat.end());
    for (const auto& p : payloads) pak.insert(pak.end(), p.begin(), p.end());
    return pak;
}
void write_file(const std::filesystem::path& path, const Bytes& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
}
}

TEST_CASE("VFS: loose file wins, then update N descending, then base") {
    auto dir = std::filesystem::temp_directory_path() / "gl_vfs_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "Data");
    write_file(dir / "Data" / "world.pak", make_pak({{"a/b.txt", {'b','a','s','e'}}}));
    write_file(dir / "Data" / "world.update.1.pak", make_pak({{"a/b.txt", {'u','1'}}}));
    write_file(dir / "Data" / "world.update.2.pak", make_pak({{"a/b.txt", {'u','2'}}}));

    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto v = vfs->read("A\\B.TXT");
    REQUIRE(v.has_value());
    CHECK(std::string(v->begin(), v->end()) == "u2");

    write_file(dir / "a" / "b.txt", Bytes{'l','o','o','s','e'});
    auto vfs2 = GameVfs::mount(dir);
    REQUIRE(vfs2.has_value());
    auto v2 = vfs2->read("a/b.txt");
    REQUIRE(v2.has_value());
    CHECK(std::string(v2->begin(), v2->end()) == "loose");

    std::filesystem::remove_all(dir);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Expected: compile fails, `vfs.hpp` missing.

- [ ] **Step 3: Implement**

`vfs.hpp`:

```cpp
#pragma once
#include "gl/assets/bytes.hpp"
#include "gl/assets/pak.hpp"
#include "gl/assets/result.hpp"
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gl::assets {
struct ArchiveInfo {
    std::string name;
    std::size_t entry_count = 0;
    bool is_update = false;
    int update_number = 0;
};

struct VfsEntry {
    std::string path;
    std::uint64_t size = 0;
    std::string source;
};

class GameVfs {
public:
    static Result<GameVfs> mount(const std::filesystem::path& game_dir);
    Result<Bytes> read(std::string_view virtual_path) const;
    bool exists(std::string_view virtual_path) const;
    std::vector<VfsEntry> list() const;
    const std::vector<ArchiveInfo>& archives() const { return archives_; }
    const PakArchive* archive(std::string_view file_name) const;

private:
    struct Location {
        const PakArchive* pak = nullptr;
        const PakEntry* entry = nullptr;
        std::string source;
    };
    std::optional<std::filesystem::path> loose_path(std::string_view virtual_path) const;

    std::filesystem::path game_dir_;
    std::vector<std::unique_ptr<PakArchive>> paks_;
    std::vector<ArchiveInfo> archives_;
    std::map<std::string, Location, std::less<>> index_;
};
}
```

`vfs.cpp` — key algorithm:

```cpp
namespace {
int update_number(const std::string& filename) {
    const std::string marker = ".update.";
    auto pos = filename.rfind(marker);
    if (pos == std::string::npos) return -1;
    auto end = filename.find(".pak", pos);
    if (end == std::string::npos) return -1;
    int value = 0;
    for (std::size_t i = pos + marker.size(); i < end; ++i) {
        if (filename[i] < '0' || filename[i] > '9') return -1;
        value = value * 10 + (filename[i] - '0');
    }
    return value;
}
}

std::optional<std::filesystem::path> GameVfs::loose_path(std::string_view vp) const {
    std::string native(vp);
    for (char& c : native) if (c == '/') c = std::filesystem::path::preferred_separator;
    const auto direct = game_dir_ / native;
    if (std::filesystem::is_regular_file(direct)) return direct;
    const auto in_data = game_dir_ / "Data" / native;
    if (std::filesystem::is_regular_file(in_data)) return in_data;
    return std::nullopt;
}

Result<GameVfs> GameVfs::mount(const std::filesystem::path& game_dir) {
    if (!std::filesystem::is_directory(game_dir))
        return Error{ErrorCode::io, "game directory not found: " + game_dir.string()};
    GameVfs vfs;
    vfs.game_dir_ = game_dir;
    std::vector<std::filesystem::path> pak_paths;
    std::error_code ec;
    for (const auto& dir : {game_dir, game_dir / "Data"}) {
        if (!std::filesystem::is_directory(dir, ec)) continue;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".pak")
                pak_paths.push_back(entry.path());
        }
    }
    struct Loaded { int priority; std::size_t index; };
    std::vector<Loaded> loaded;
    for (const auto& path : pak_paths) {
        auto archive = PakArchive::open_from_file(path);
        if (!archive) return archive.error();
        const int n = update_number(path.filename().string());
        const int priority = n < 0 ? 0 : 1000 + n;
        ArchiveInfo info{path.filename().string(), archive->entries().size(), n >= 0, n};
        vfs.paks_.push_back(std::make_unique<PakArchive>(std::move(archive.value())));
        vfs.archives_.push_back(info);
        loaded.push_back({priority, vfs.paks_.size() - 1});
    }
    std::sort(loaded.begin(), loaded.end(), [](const Loaded& a, const Loaded& b) {
        return a.priority < b.priority;
    });
    for (const auto& item : loaded) {
        const PakArchive& pak = *vfs.paks_[item.index];
        for (const auto& entry : pak.entries()) {
            const std::string key = pak_entry_key(entry.name);
            vfs.index_[key] = Location{&pak, &entry, vfs.archives_[item.index].name};
        }
    }
    return vfs;
}

bool GameVfs::exists(std::string_view vp) const {
    if (loose_path(vp)) return true;
    return index_.find(normalize_virtual_path(vp)) != index_.end();
}

Result<Bytes> GameVfs::read(std::string_view vp) const {
    if (const auto loose = loose_path(vp)) {
        std::ifstream in(*loose, std::ios::binary | std::ios::ate);
        if (!in) return Error{ErrorCode::io, "cannot open loose file"};
        const auto size = in.tellg();
        in.seekg(0);
        Bytes out(static_cast<std::size_t>(size));
        if (!in.read(reinterpret_cast<char*>(out.data()), out.size()))
            return Error{ErrorCode::io, "cannot read loose file"};
        return out;
    }
    const auto it = index_.find(normalize_virtual_path(vp));
    if (it == index_.end()) return Error{ErrorCode::io, "file not found: " + std::string(vp)};
    return it->second.pak->read(*it->second.entry);
}

std::vector<VfsEntry> GameVfs::list() const {
    std::vector<VfsEntry> out;
    out.reserve(index_.size());
    for (const auto& [key, loc] : index_)
        out.push_back({key, loc.entry->size, loc.source});
    return out;
}

const PakArchive* GameVfs::archive(std::string_view file_name) const {
    for (std::size_t i = 0; i < archives_.size(); ++i)
        if (archives_[i].name == file_name) return paks_[i].get();
    return nullptr;
}
```

`list()` reports the merged archive view only; loose files are not listed. This is expected.

Add `#include <algorithm>` and `<fstream>` in `vfs.cpp`.

- [ ] **Step 4: Run tests**

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: VFS tests pass, including update precedence.

- [ ] **Step 5: Commit**

```powershell
git add src/assets tests/assets
git commit -m "feat(assets): add game VFS with loose files and update pak overlays"
```

---

### Task 5: CSX sprite decoder

**Files:**
- Create: `src/assets/include/gl/assets/csx.hpp`
- Create: `src/assets/src/csx.cpp`
- Create: `tests/assets/test_csx.cpp`
- Modify: `tests/assets/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `struct Rgba { std::uint8_t r, g, b, a; };`
  - `struct CsxImage { uint32_t width, height, transparent_index; std::vector<uint32_t> palette_bgr; std::vector<uint8_t> indices; std::vector<Rgba> rgba; };`
  - `Result<CsxImage> decode_csx(std::span<const std::uint8_t> data);`

RLE rules (from `Xcsx.dpr`):
- `0x69` → transparent, count 1.
- `0x6A` → next byte color, next byte count.
- `0x6B` → next byte literal color, count 1.
- `0x6C` → transparent, next byte count.
- any other byte → literal palette index, count 1.
- Decode exactly `width` pixels per row; `row_offsets[y+1]` bounds the row.
- Transparent index: highest palette index whose BGR equals `transparent_bgr`; if none,
  fall back to `transparent_bgr & 0xFF`.

- [ ] **Step 1: Write failing tests**

`tests/assets/test_csx.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/csx.hpp"

using namespace gl::assets;

namespace {
void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}
Bytes make_csx(Bytes rows, std::vector<std::uint32_t> offsets, std::uint32_t width,
               std::uint32_t height, std::vector<std::uint32_t> palette) {
    Bytes b;
    put_u32(b, static_cast<std::uint32_t>(palette.size()));
    put_u32(b, palette[1]); // transparent = second palette entry
    for (auto c : palette) put_u32(b, c);
    put_u32(b, width);
    put_u32(b, height);
    for (auto o : offsets) put_u32(b, o);
    b.insert(b.end(), rows.begin(), rows.end());
    return b;
}
}

TEST_CASE("CSX: decodes literals, repeats and transparency") {
    // Palette: black, green (transparent), red in BGR.
    std::vector<std::uint32_t> palette = {0x000000, 0x00FF00, 0xFF0000};
    // Row 0: idx0, idx2, run idx1 x2. Row 1: transparent x4.
    Bytes row0 = {0x00, 0x02, 0x6A, 0x01, 0x02};
    Bytes row1 = {0x6C, 0x04};
    Bytes rows = row0; rows.insert(rows.end(), row1.begin(), row1.end());
    std::vector<std::uint32_t> offsets = {0, static_cast<std::uint32_t>(row0.size()),
                                          static_cast<std::uint32_t>(rows.size())};
    Bytes data = make_csx(rows, offsets, 4, 2, palette);
    auto img = decode_csx(data);
    REQUIRE(img.has_value());
    CHECK(img->width == 4);
    CHECK(img->height == 2);
    CHECK(img->transparent_index == 1);
    CHECK(img->indices == Bytes{0, 2, 1, 1, 1, 1, 1, 1});
    CHECK(img->rgba[0].r == 0x00);
    CHECK(img->rgba[0].a == 0xFF);
    CHECK(img->rgba[1].r == 0xFF); // red
    CHECK(img->rgba[2].a == 0x00); // transparent
    CHECK(img->rgba[4].a == 0x00);
}

TEST_CASE("CSX: rejects a row with the wrong pixel count") {
    std::vector<std::uint32_t> palette = {0x000000, 0x00FF00};
    Bytes rows = {0x00, 0x00};
    std::vector<std::uint32_t> offsets = {0, 2, 2};
    Bytes data = make_csx(rows, offsets, 4, 1, palette);
    auto img = decode_csx(data);
    REQUIRE_FALSE(img.has_value());
    CHECK(img.error().code == gl::ErrorCode::format);
}

TEST_CASE("CSX: literal escape 0x6B for special palette indices") {
    std::vector<std::uint32_t> palette(256, 0x000000);
    palette[0x69] = 0x0000FF; // red in BGR
    palette[1] = 0x00FF00;    // transparent
    Bytes rows = {0x6B, 0x69};
    std::vector<std::uint32_t> offsets = {0, 2, 2};
    Bytes data = make_csx(rows, offsets, 1, 1, palette);
    auto img = decode_csx(data);
    REQUIRE(img.has_value());
    CHECK(img->indices[0] == 0x69);
    CHECK(img->rgba[0].r == 0xFF);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Expected: compile fails, `csx.hpp` missing.

- [ ] **Step 3: Implement**

`csx.hpp`:

```cpp
#pragma once
#include "gl/assets/bytes.hpp"
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {
struct Rgba { std::uint8_t r = 0, g = 0, b = 0, a = 0; };

struct CsxImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t transparent_index = 0;
    std::vector<std::uint32_t> palette_bgr;
    std::vector<std::uint8_t> indices;
    std::vector<Rgba> rgba;
};

Result<CsxImage> decode_csx(std::span<const std::uint8_t> data);
}
```

`csx.cpp`:

```cpp
#include "gl/assets/csx.hpp"
#include "gl/assets/byte_reader.hpp"

namespace gl::assets {
Result<CsxImage> decode_csx(std::span<const std::uint8_t> data) {
    ByteReader r(data);
    std::uint32_t color_count = 0, transparent_bgr = 0;
    if (!r.read_u32le(color_count) || !r.read_u32le(transparent_bgr))
        return Error{ErrorCode::format, "truncated CSX header"};
    if (color_count == 0 || color_count > 256)
        return Error{ErrorCode::format, "bad CSX palette size"};
    CsxImage image;
    image.palette_bgr.resize(color_count);
    for (auto& c : image.palette_bgr)
        if (!r.read_u32le(c)) return Error{ErrorCode::format, "truncated CSX palette"};
    if (!r.read_u32le(image.width) || !r.read_u32le(image.height))
        return Error{ErrorCode::format, "truncated CSX dimensions"};
    if (image.width == 0 || image.height == 0 || image.width > 16384 || image.height > 16384)
        return Error{ErrorCode::format, "bad CSX dimensions"};
    std::vector<std::uint32_t> offsets(image.height + 1);
    for (auto& o : offsets)
        if (!r.read_u32le(o)) return Error{ErrorCode::format, "truncated CSX row offsets"};
    const auto rows = r.remaining();

    image.transparent_index = transparent_bgr & 0xFF;
    for (int i = static_cast<int>(color_count) - 1; i >= 0; --i) {
        if (image.palette_bgr[static_cast<std::size_t>(i)] == transparent_bgr) {
            image.transparent_index = static_cast<std::uint32_t>(i);
            break;
        }
    }

    image.indices.assign(static_cast<std::size_t>(image.width) * image.height, 0);
    for (std::uint32_t y = 0; y < image.height; ++y) {
        if (offsets[y] > offsets[y + 1] || offsets[y + 1] > rows.size())
            return Error{ErrorCode::format, "bad CSX row offsets"};
        std::size_t pos = offsets[y];
        const std::size_t end = offsets[y + 1];
        std::uint32_t x = 0;
        while (pos < end) {
            const std::uint8_t code = rows[pos++];
            std::uint32_t color = 0, count = 1;
            switch (code) {
                case 0x69: color = image.transparent_index; break;
                case 0x6A:
                    if (pos + 2 > end) return Error{ErrorCode::format, "truncated CSX run"};
                    color = rows[pos++]; count = rows[pos++]; break;
                case 0x6B:
                    if (pos + 1 > end) return Error{ErrorCode::format, "truncated CSX literal"};
                    color = rows[pos++]; break;
                case 0x6C:
                    if (pos + 1 > end) return Error{ErrorCode::format, "truncated CSX transparent run"};
                    color = image.transparent_index; count = rows[pos++]; break;
                default: color = code; break;
            }
            if (color >= color_count || count == 0 || x + count > image.width)
                return Error{ErrorCode::format, "invalid CSX run"};
            for (std::uint32_t i = 0; i < count; ++i)
                image.indices[static_cast<std::size_t>(y) * image.width + x + i] =
                    static_cast<std::uint8_t>(color);
            x += count;
        }
        if (x != image.width) return Error{ErrorCode::format, "CSX row has wrong width"};
    }

    image.rgba.resize(image.indices.size());
    for (std::size_t i = 0; i < image.indices.size(); ++i) {
        const std::uint8_t index = image.indices[i];
        const std::uint32_t bgr = image.palette_bgr[index];
        image.rgba[i] = Rgba{static_cast<std::uint8_t>((bgr >> 16) & 0xFF),
                             static_cast<std::uint8_t>((bgr >> 8) & 0xFF),
                             static_cast<std::uint8_t>(bgr & 0xFF),
                             index == image.transparent_index ? std::uint8_t{0} : std::uint8_t{255}};
    }
    return image;
}
}
```

- [ ] **Step 4: Run tests**

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: CSX tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/assets tests/assets
git commit -m "feat(assets): decode CSX paletted RLE sprites"
```

---

### Task 6: SDB string database

**Files:**
- Create: `src/assets/include/gl/assets/sdb.hpp`
- Create: `src/assets/src/sdb.cpp`
- Create: `tests/assets/test_sdb.cpp`
- Modify: `tests/assets/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `struct SdbEntry { std::uint32_t id; std::string value; };`
  - `class Sdb`:
    - `static Result<Sdb> parse(std::span<const std::uint8_t>)`
    - `const std::string* find(std::uint32_t id) const`
    - `const std::vector<SdbEntry>& entries() const`
    - `bool had_signature() const`

- [ ] **Step 1: Write failing tests**

`tests/assets/test_sdb.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/sdb.hpp"

using namespace gl::assets;

namespace {
void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}
Bytes entry(std::uint32_t id, const std::string& text) {
    Bytes b; put_u32(b, id); put_u32(b, static_cast<std::uint32_t>(text.size()));
    b.insert(b.end(), text.begin(), text.end()); return b;
}
}

TEST_CASE("SDB: parses plain files") {
    Bytes data = {'S','D','B',' '};
    Bytes e = entry(7, "Hello");
    data.insert(data.end(), e.begin(), e.end());
    auto sdb = Sdb::parse(data);
    REQUIRE(sdb.has_value());
    CHECK(sdb->had_signature());
    REQUIRE(sdb->find(7) != nullptr);
    CHECK(*sdb->find(7) == "Hello");
}

TEST_CASE("SDB: XOR 0xAA when signature is absent") {
    const std::uint8_t raw[] = {0xCF,0xF0,0xE8,0xE2,0xE5,0xF2}; // Привет
    Bytes text;
    for (std::uint8_t b : raw) text.push_back(static_cast<std::uint8_t>(b ^ 0xAA));
    Bytes data = entry(3, std::string(text.begin(), text.end()));
    auto sdb = Sdb::parse(data);
    REQUIRE(sdb.has_value());
    CHECK_FALSE(sdb->had_signature());
    REQUIRE(sdb->find(3) != nullptr);
    CHECK(*sdb->find(3) == "Привет");
}

TEST_CASE("SDB: rejects truncated entries") {
    Bytes data = {'S','D','B',' ', 1, 0, 0, 0, 10, 0, 0, 0};
    auto sdb = Sdb::parse(data);
    REQUIRE_FALSE(sdb.has_value());
    CHECK(sdb.error().code == gl::ErrorCode::format);
}
```

Note: the helper `entry` XORs the text bytes by 0xAA, but `entry` itself writes the text length. For the no-signature case, the length must not be XORed. The test writes the length plain and the text XORed, which matches the spec hypothesis. If the real data behaves differently, adjust the decoder after integration.

- [ ] **Step 2: Run tests to verify they fail**

- [ ] **Step 3: Implement**

`sdb.hpp`:

```cpp
#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gl::assets {
struct SdbEntry { std::uint32_t id = 0; std::string value; };

class Sdb {
public:
    static Result<Sdb> parse(std::span<const std::uint8_t> data);
    const std::string* find(std::uint32_t id) const;
    const std::vector<SdbEntry>& entries() const { return entries_; }
    bool had_signature() const { return had_signature_; }
private:
    bool had_signature_ = false;
    std::vector<SdbEntry> entries_;
};
}
```

`sdb.cpp`:

```cpp
#include "gl/assets/sdb.hpp"
#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"
#include <cstring>

namespace gl::assets {
Result<Sdb> Sdb::parse(std::span<const std::uint8_t> data) {
    ByteReader r(data);
    Sdb sdb;
    if (data.size() >= 4 && std::memcmp(data.data(), "SDB ", 4) == 0) {
        sdb.had_signature_ = true;
        r.skip(4);
    }
    while (!r.at_end()) {
        std::uint32_t id = 0, len = 0;
        if (!r.read_u32le(id) || !r.read_u32le(len))
            return Error{ErrorCode::format, "truncated SDB entry"};
        std::span<const std::uint8_t> bytes;
        if (!r.read_bytes(len, bytes))
            return Error{ErrorCode::format, "truncated SDB string"};
        std::vector<std::uint8_t> tmp(bytes.begin(), bytes.end());
        if (!sdb.had_signature_)
            for (auto& b : tmp) b ^= 0xAA;
        sdb.entries_.push_back({id, cp1251_to_utf8(tmp)});
    }
    return sdb;
}

const std::string* Sdb::find(std::uint32_t id) const {
    for (const auto& e : entries_) if (e.id == id) return &e.value;
    return nullptr;
}
}
```

- [ ] **Step 4: Run tests**

- [ ] **Step 5: Commit**

```powershell
git add src/assets tests/assets
git commit -m "feat(assets): parse SDB string databases with xor fallback"
```

---

### Task 7: ITM item decoder

**Files:**
- Create: `src/assets/include/gl/assets/itm.hpp`
- Create: `src/assets/src/itm.cpp`
- Create: `tests/assets/test_itm.cpp`
- Modify: `tests/assets/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `enum class ItemType : std::uint32_t` with values 0..28 and the names from the format docs.
  - `struct ItemTime { std::uint8_t hi, lo; std::uint16_t turns() const; };`
  - `struct ItemEffect { std::uint32_t type, amount, flags; ItemTime duration, delay; };`
  - `struct Item` with all fields listed in `docs/research/formats.md` and
    `bool has_damage/has_spell/has_charge/has_healing/has_recipe/has_damage_type`.
  - `Result<Item> parse_item(std::span<const std::uint8_t> data);`

Conditional blocks:
- damage: type 0..7 or 11..15; 6 × u32.
- spell: type 6, 21, 22; spell_id u32 + zero u32.
- charge: type 6; u32.
- healing: type 19 or 20; health u32 + mana u32.
- recipe: type 26; 3 × u32.
- damage_type: type 8, 9, 10; u32.
- effects: always `count u32`, then `count × (type u32, amount u32, flags u32, duration hi/lo, delay hi/lo)`.

- [ ] **Step 1: Write failing tests**

`tests/assets/test_itm.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/itm.hpp"

using namespace gl::assets;

namespace {
void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}
void put_u64(Bytes& b, std::uint64_t v) { put_u32(b, static_cast<std::uint32_t>(v)); put_u32(b, static_cast<std::uint32_t>(v >> 32)); }
void put_str(Bytes& b, const std::string& s) { put_u32(b, static_cast<std::uint32_t>(s.size())); b.insert(b.end(), s.begin(), s.end()); }
Bytes base(std::uint32_t type, std::uint32_t flags) {
    Bytes b;
    put_u32(b, 4);          // magic
    put_u32(b, 100);        // description id
    put_u32(b, type);
    put_u32(b, flags);
    put_u32(b, 1);          // max stack
    put_str(b, "");         // puppet
    put_str(b, "");         // icon
    put_u64(b, 0);          // unknown
    put_u32(b, 10);         // price
    put_u32(b, 20);         // weight
    for (int i = 0; i < 6; ++i) put_u32(b, 0); // unknown dwords
    put_u32(b, 0xFFFFFFFF); // material
    return b;
}
}

TEST_CASE("ITM: parses a sword with damage and effects") {
    Bytes b = base(0, 0);
    put_u32(b, 0); put_u32(b, 0); put_u32(b, 0); put_u32(b, 0);
    put_u32(b, 0); put_u32(b, 0); put_u32(b, 0);
    for (int i = 0; i < 6; ++i) put_u32(b, static_cast<std::uint32_t>(i + 1));
    put_u32(b, 0); // effects count
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    CHECK(item->type == ItemType::sword);
    CHECK(item->has_damage);
    CHECK(item->damage[0] == 1);
    CHECK(item->damage[5] == 6);
    CHECK(item->effects.empty());
}

TEST_CASE("ITM: parses a potion with healing") {
    Bytes b = base(19, 0);
    put_u32(b, 0); put_u32(b, 0); put_u32(b, 0); put_u32(b, 0);
    put_u32(b, 0); put_u32(b, 0); put_u32(b, 0);
    put_u32(b, 50); put_u32(b, 25); // health, mana
    put_u32(b, 0); // effects
    auto item = parse_item(b);
    REQUIRE(item.has_value());
    CHECK(item->type == ItemType::potion);
    CHECK(item->has_healing);
    CHECK(item->healing_health == 50);
    CHECK(item->healing_mana == 25);
}

TEST_CASE("ITM: rejects a bad magic") {
    Bytes b = base(0, 0);
    b[0] = 0;
    auto item = parse_item(b);
    REQUIRE_FALSE(item.has_value());
    CHECK(item.error().code == gl::ErrorCode::format);
}
```

Note: `base()` omits the seven attribute dwords (level + 6 attributes) so each test adds them.
The test adds `put_u32` seven times (level, strength, wisdom, endurance, intelligence,
perception, agility). Keep the code and tests in sync.

- [ ] **Step 2: Run tests to verify they fail**

- [ ] **Step 3: Implement**

`itm.hpp`:

```cpp
#pragma once
#include "gl/assets/result.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gl::assets {
enum class ItemType : std::uint32_t {
    sword = 0, axe, spear, bow, crossbow, rifle, staff, blunt,
    bullets, bolts, arrows, helmet, cuirass, shield, leggings, bracers,
    amulet, ring, money, potion, food, book, scroll, smith_material,
    alchemy_ingredient, quest_item, recipe, junk, stone
};

struct ItemTime {
    std::uint8_t hi = 0;
    std::uint8_t lo = 0;
    std::uint16_t turns() const { return static_cast<std::uint16_t>(hi * (hi < 2 ? 30 : 60) + lo); }
};

struct ItemEffect {
    std::uint32_t type = 0;
    std::uint32_t amount = 0;
    std::uint32_t flags = 0;
    ItemTime duration;
    ItemTime delay;
};

struct Item {
    std::uint32_t description_id = 0;
    ItemType type = ItemType::sword;
    std::uint32_t flags = 0;
    std::uint32_t max_stack = 0;
    std::string puppet_name;
    std::string icon_name;
    std::uint64_t unknown = 0;
    std::uint32_t price = 0;
    std::uint32_t weight_x10 = 0;
    std::uint32_t unknown_a = 0;
    std::array<std::uint32_t, 5> unknown_b{};
    std::uint32_t material = 0xFFFFFFFFu;
    std::string shell;
    std::uint32_t level = 0, strength = 0, wisdom = 0, endurance = 0;
    std::uint32_t intelligence = 0, perception = 0, agility = 0;
    std::array<std::uint32_t, 6> damage{};
    bool has_damage = false;
    std::uint32_t spell_id = 0;
    bool has_spell = false;
    std::uint32_t charge = 0;
    bool has_charge = false;
    std::uint32_t healing_health = 0, healing_mana = 0;
    bool has_healing = false;
    std::array<std::uint32_t, 3> recipe{};
    bool has_recipe = false;
    std::uint32_t damage_type = 0;
    bool has_damage_type = false;
    std::vector<ItemEffect> effects;
};

Result<Item> parse_item(std::span<const std::uint8_t> data);
}
```

`itm.cpp` — parse with a small helper and the exact order above. Use `ByteReader` and
`cp1251_to_utf8`; return `Error{ErrorCode::format, ...}` on any short read or bad magic.
Read the string helper locally:

```cpp
Result<std::string> read_string(ByteReader& r) {
    std::uint32_t len = 0;
    if (!r.read_u32le(len)) return Error{ErrorCode::format, "truncated string length"};
    std::span<const std::uint8_t> bytes;
    if (!r.read_bytes(len, bytes)) return Error{ErrorCode::format, "truncated string"};
    return cp1251_to_utf8(bytes);
}
```

Then:

```cpp
Result<Item> parse_item(std::span<const std::uint8_t> data) {
    ByteReader r(data);
    std::uint32_t magic = 0;
    if (!r.read_u32le(magic) || magic != 4) return Error{ErrorCode::format, "bad ITM magic"};
    Item item;
    std::uint32_t type = 0;
    if (!r.read_u32le(item.description_id) || !r.read_u32le(type) ||
        !r.read_u32le(item.flags) || !r.read_u32le(item.max_stack))
        return Error{ErrorCode::format, "truncated ITM header"};
    item.type = static_cast<ItemType>(type);
    auto puppet = read_string(r); if (!puppet) return puppet.error();
    auto icon = read_string(r); if (!icon) return icon.error();
    item.puppet_name = std::move(puppet.value());
    item.icon_name = std::move(icon.value());
    if (!r.read_u64le(item.unknown) || !r.read_u32le(item.price) || !r.read_u32le(item.weight_x10) ||
        !r.read_u32le(item.unknown_a))
        return Error{ErrorCode::format, "truncated ITM core"};
    for (auto& v : item.unknown_b) if (!r.read_u32le(v)) return Error{ErrorCode::format, "truncated ITM core"};
    if (!r.read_u32le(item.material)) return Error{ErrorCode::format, "truncated ITM material"};
    if (item.flags & (1u << 4)) {
        auto shell = read_string(r); if (!shell) return shell.error();
        item.shell = std::move(shell.value());
    }
    if (!r.read_u32le(item.level) || !r.read_u32le(item.strength) || !r.read_u32le(item.wisdom) ||
        !r.read_u32le(item.endurance) || !r.read_u32le(item.intelligence) ||
        !r.read_u32le(item.perception) || !r.read_u32le(item.agility))
        return Error{ErrorCode::format, "truncated ITM requirements"};
    const std::uint32_t t = type;
    item.has_damage = (t <= 7) || (t >= 11 && t <= 15);
    if (item.has_damage)
        for (auto& v : item.damage) if (!r.read_u32le(v)) return Error{ErrorCode::format, "truncated ITM damage"};
    item.has_spell = (t == 6 || t == 21 || t == 22);
    if (item.has_spell) {
        std::uint32_t zero = 0;
        if (!r.read_u32le(item.spell_id) || !r.read_u32le(zero))
            return Error{ErrorCode::format, "truncated ITM spell"};
    }
    item.has_charge = (t == 6);
    if (item.has_charge && !r.read_u32le(item.charge)) return Error{ErrorCode::format, "truncated ITM charge"};
    item.has_healing = (t == 19 || t == 20);
    if (item.has_healing && (!r.read_u32le(item.healing_health) || !r.read_u32le(item.healing_mana)))
        return Error{ErrorCode::format, "truncated ITM healing"};
    item.has_recipe = (t == 26);
    if (item.has_recipe)
        for (auto& v : item.recipe) if (!r.read_u32le(v)) return Error{ErrorCode::format, "truncated ITM recipe"};
    item.has_damage_type = (t == 8 || t == 9 || t == 10);
    if (item.has_damage_type && !r.read_u32le(item.damage_type))
        return Error{ErrorCode::format, "truncated ITM damage type"};
    std::uint32_t count = 0;
    if (!r.read_u32le(count)) return Error{ErrorCode::format, "truncated ITM effects count"};
    if (count > 1000) return Error{ErrorCode::format, "unreasonable ITM effects count"};
    item.effects.resize(count);
    for (auto& e : item.effects) {
        if (!r.read_u32le(e.type) || !r.read_u32le(e.amount) || !r.read_u32le(e.flags) ||
            !r.read_u8(e.duration.hi) || !r.read_u8(e.duration.lo) ||
            !r.read_u8(e.delay.hi) || !r.read_u8(e.delay.lo))
            return Error{ErrorCode::format, "truncated ITM effect"};
    }
    return item;
}
```

- [ ] **Step 4: Run tests**

- [ ] **Step 5: Commit**

```powershell
git add src/assets tests/assets
git commit -m "feat(assets): parse ITM item records"
```

---

### Task 8: Image decode and PNG export

**Files:**
- Create: `src/assets/include/gl/assets/image.hpp`
- Create: `src/assets/src/image.cpp`
- Create: `src/assets/src/stb_impl.cpp`
- Create: `tests/assets/test_image.cpp`
- Modify: `tests/assets/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `struct Image { std::uint32_t width, height; std::vector<Rgba> rgba; };`
  - `Result<Image> decode_image(std::span<const std::uint8_t> data);`
  - `bool write_png(const std::filesystem::path&, const Image&);`

- [ ] **Step 1: Write failing test**

`tests/assets/test_image.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/image.hpp"

using namespace gl::assets;

namespace {
Bytes make_2x2_bmp() {
    Bytes b;
    auto u16 = [&](std::uint16_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); };
    auto u32 = [&](std::uint32_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
                                      b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF); };
    b.push_back('B'); b.push_back('M');
    u32(70); u32(0); u32(54);       // file header
    u32(40); u32(2); u32(2); u16(1); u16(24);
    u32(0); u32(16); u32(2835); u32(2835); u32(0); u32(0);
    // bottom row: blue, green; top row: red, white (BGR, rows padded to 8 bytes)
    b.insert(b.end(), {255,0,0, 0,255,0, 0,0});
    b.insert(b.end(), {0,0,255, 255,255,255, 0,0});
    return b;
}
}

TEST_CASE("Image: decodes a 2x2 BMP to RGBA") {
    Bytes bmp = make_2x2_bmp();
    auto image = decode_image(bmp);
    REQUIRE(image.has_value());
    CHECK(image->width == 2);
    CHECK(image->height == 2);
    // stb returns top-down; the BMP file stored the bottom row first.
    CHECK(image->rgba[0].r == 255); // red top-left
    CHECK(image->rgba[0].g == 0);
    CHECK(image->rgba[0].b == 0);
    CHECK(image->rgba[2].r == 0);   // blue bottom-left
    CHECK(image->rgba[2].b == 255);
    CHECK(image->rgba[3].r == 255); // white bottom-right
    CHECK(image->rgba[3].g == 255);
    CHECK(image->rgba[3].b == 255);
}

TEST_CASE("Image: rejects garbage") {
    const std::uint8_t data[] = {1,2,3,4};
    auto image = decode_image(data);
    CHECK_FALSE(image.has_value());
}
```

- [ ] **Step 2: Run tests to verify they fail**

- [ ] **Step 3: Implement**

`image.hpp`:

```cpp
#pragma once
#include "gl/assets/csx.hpp"
#include "gl/assets/result.hpp"
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace gl::assets {
struct Image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<Rgba> rgba;
};

Result<Image> decode_image(std::span<const std::uint8_t> data);
bool write_png(const std::filesystem::path& path, const Image& image);
}
```

`stb_impl.cpp`:

```cpp
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
```

`image.cpp`:

```cpp
#include "gl/assets/image.hpp"
#include <stb_image.h>
#include <stb_image_write.h>

namespace gl::assets {
Result<Image> decode_image(std::span<const std::uint8_t> data) {
    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()),
                                            &w, &h, &channels, 4);
    if (!pixels) return Error{ErrorCode::format, stbi_failure_reason()};
    Image image;
    image.width = static_cast<std::uint32_t>(w);
    image.height = static_cast<std::uint32_t>(h);
    image.rgba.resize(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < image.rgba.size(); ++i) {
        image.rgba[i] = Rgba{pixels[i * 4 + 0], pixels[i * 4 + 1], pixels[i * 4 + 2], pixels[i * 4 + 3]};
    }
    stbi_image_free(pixels);
    return image;
}

bool write_png(const std::filesystem::path& path, const Image& image) {
    const std::string narrow = path.string();
    return stbi_write_png(narrow.c_str(), static_cast<int>(image.width),
                          static_cast<int>(image.height), 4, image.rgba.data(),
                          static_cast<int>(image.width) * 4) != 0;
}
}
```

`stb_impl.cpp` must be compiled into `gl_assets` (already listed in the root CMake).

- [ ] **Step 4: Run tests**

- [ ] **Step 5: Commit**

```powershell
git add src/assets tests/assets
git commit -m "feat(assets): decode images with stb and export PNG"
```

---

### Task 9: glpak CLI

**Files:**
- Create: `tools/glpak/CMakeLists.txt`
- Create: `tools/glpak/main.cpp`

**Interfaces:**
- Consumes all `gl_assets` APIs.
- Produces exit codes: `0` success, `1` arguments, `2` I/O, `3` format, `4` mismatch.

Commands:

```
glpak info <game_dir>
glpak list <game_dir> [--archive <name>]
glpak extract <game_dir> --all --out <dir>
glpak extract <game_dir> --archive <name> --out <dir>
glpak extract <game_dir> --file <virtual/path> --out <dir>
glpak extract <game_dir> --prefix <prefix> --out <dir>
glpak decode-csx <game_dir> --file <path> --out <png>
glpak decode-sdb <game_dir> --file <path> --out <txt>
glpak decode-itm <game_dir> --file <path>
```

- [ ] **Step 1: Write the CLI**

`tools/glpak/CMakeLists.txt`:

```cmake
add_executable(glpak main.cpp)
target_link_libraries(glpak PRIVATE gl_assets)
gl_enable_warnings(glpak)
```

`main.cpp` — implement argument dispatch, `info`, `list`, `extract`, and the three decoders.
`extract --all` walks the merged VFS view; `extract --archive <name>` uses
`GameVfs::archive(name)` and writes every entry of that archive (so base-archive totals can be
verified against the expected counts). Use `std::cout`/`std::cerr`,
`std::filesystem::create_directories`, and `is_safe_relative_path` before writing extracted
files. For `decode-sdb`, write `id<TAB>value` lines in UTF-8. For `decode-itm`, print
`field=value` lines. For `decode-csx`, call `write_png`.

- [ ] **Step 2: Build and smoke-test the CLI**

```powershell
cmake --build build
build\bin\glpak.exe info "I:\Games\Златогорье-2"
```

Expected: prints archive names and counts; `scripts.pak` shows 4485, `sdb.pak` 45,
`items.pak` 3786, `engineres.pak` 974.

- [ ] **Step 3: Commit**

```powershell
git add tools/glpak CMakeLists.txt
git commit -m "feat(tools): add glpak CLI for info, list, extract and decoders"
```

---

### Task 10: Real-data integration and acceptance

**Files:**
- Create: `tests/assets/test_real_data.cpp`
- Modify: `tests/assets/CMakeLists.txt`
- Modify: `docs/STATUS.md`, `docs/PROJECT.md`, `docs/research/00-game-build.md`

**Interfaces:**
- Consumes `gl_assets` only.

- [ ] **Step 1: Write the integration test**

`tests/assets/test_real_data.cpp`:

```cpp
#include <doctest/doctest.h>
#include "gl/assets/pak.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/itm.hpp"
#include "gl/assets/csx.hpp"
#include "gl/assets/vfs.hpp"
#include <cstdlib>
#include <filesystem>

using namespace gl::assets;

TEST_CASE("real data: archive counts and samples") {
    const char* dir = std::getenv("GL_GAME_DIR");
    if (!dir) { MESSAGE("GL_GAME_DIR not set; skipping"); return; }
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto count_for = [&](std::string_view name) -> std::size_t {
        for (const auto& a : vfs->archives()) if (a.name == name) return a.entry_count;
        return 0;
    };
    CHECK(count_for("scripts.pak") == 4485);
    CHECK(count_for("sdb.pak") == 45);
    CHECK(count_for("items.pak") == 3786);
    CHECK(count_for("engineres.pak") == 974);

    auto hero = vfs->read("sdb/hero.sdb");
    REQUIRE(hero.has_value());
    auto sdb = Sdb::parse(*hero);
    CHECK(sdb.has_value());

    auto item = vfs->read("items/data/0.itm");
    REQUIRE(item.has_value());
    auto parsed = parse_item(*item);
    CHECK(parsed.has_value());
}
```

Add to `tests/assets/CMakeLists.txt`:

```cmake
add_executable(gl_assets_real_tests test_main.cpp test_real_data.cpp)
target_include_directories(gl_assets_real_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
target_link_libraries(gl_assets_real_tests PRIVATE gl_assets)
gl_enable_warnings(gl_assets_real_tests)
add_test(NAME gl_assets_real_tests COMMAND gl_assets_real_tests)
```

- [ ] **Step 2: Run unit + integration**

```powershell
$env:GL_GAME_DIR = "I:\Games\Златогорье-2"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass; the real-data test is not skipped.

- [ ] **Step 3: Full extraction acceptance run**

Base-archive totals must be verified per archive (the merged VFS view hides files overridden
by `*.update.3.pak`):

```powershell
$out = "$env:TEMP\gl_extract"
Remove-Item -Recurse -Force $out -ErrorAction SilentlyContinue
foreach ($pak in @("scripts.pak","sdb.pak","items.pak","engineres.pak")) {
    build\bin\glpak.exe extract "I:\Games\Златогорье-2" --archive $pak --out (Join-Path $out $pak)
}
$scripts = Get-ChildItem -LiteralPath "$out\scripts.pak" -Recurse -File | Measure-Object Length -Sum
$sdb = Get-ChildItem -LiteralPath "$out\sdb.pak" -Recurse -File | Measure-Object Length -Sum
$items = Get-ChildItem -LiteralPath "$out\items.pak" -Recurse -File | Measure-Object Length -Sum
$engine = Get-ChildItem -LiteralPath "$out\engineres.pak" -Recurse -File | Measure-Object Length -Sum
"scripts:   $($scripts.Count) files, $($scripts.Sum) bytes"
"sdb:       $($sdb.Count) files, $($sdb.Sum) bytes"
"items:     $($items.Count) files, $($items.Sum) bytes"
"engineres: $($engine.Count) files"
```

Expected:

```
scripts:   4485 files, 9324686 bytes
sdb:       45 files, 1411188 bytes
items:     3786 files, 28847595 bytes
engineres: 974 files
```

If a number differs, do not adjust the expected value — investigate the decoder first.
Record the totals for the remaining archives in `docs/research/00-game-build.md`.

- [ ] **Step 4: Decode samples**

```powershell
build\bin\glpak.exe decode-csx "I:\Games\Златогорье-2" --file "engineres/globalmap/hero.csx" --out "$env:TEMP\hero.png"
```

Expected: `hero.png` is created (a 390-byte sprite sheet) and opens in an image viewer.
Optionally decode `engineres/editor/person.csx` as well. Record the paths used in
`docs/STATUS.md`.

- [ ] **Step 5: Update docs**

`docs/STATUS.md`:

```markdown
## Текущая задача

**P0 — Ассеты и форматы.** Реализация завершена, приёмка пройдена.

Проверено:
- `cmake --build build` — успех, без предупреждений.
- `ctest --test-dir build --output-on-failure` — все тесты зелёные (GL_GAME_DIR задан).
- `glpak extract --all`: scripts 4485/9 324 686, sdb 45/1 411 188, items 3786/28 847 595.
- `glpak decode-csx` — PNG декодирован и проверен визуально.

Следующая задача: P1 — рендер изометрии и уровень.
```

Also mark P0 `[x]` in `docs/PROJECT.md`, add the new archive totals to
`docs/research/00-game-build.md`, and add a DECISIONS entry if any format assumption changed.

- [ ] **Step 6: Commit**

```powershell
git add tests/assets docs
git commit -m "test(assets): verify real archives and record P0 acceptance results"
```

---

## Self-Review

- Spec coverage: PAK/zlib, VFS/update precedence, CSX, SDB, ITM, BMP, CLI, unit + real-data
  tests, acceptance totals, docs updates — all have tasks.
- No placeholders: every task has concrete files, interfaces, tests and implementation code.
- Type consistency: `Bytes`, `Result<T>`, `Rgba`, `Item`, `CsxImage` are defined once and reused
  with the same names in later tasks.
- Deferred by design: LVL/PAD/MDF/ANI/scripts, rendering, audio, Android — P1+.
