# Исследование: окружение сборки (Windows)

Дата: 2026-10-19.

`winget install` зависает на неуправляемом `msiexec.exe` и не завершается за 10 минут,
поэтому тулчейн установлен **переносимо** в `.local/toolchains` (каталог в `.gitignore`,
права администратора не требуются).

## Установленное

| Инструмент | Версия | Путь |
|---|---|---|
| CMake | 4.4.4 | `.local/toolchains/cmake/cmake-4.4.4-windows-x86_64/bin/cmake.exe` |
| Ninja | 1.13.2 | `.local/toolchains/ninja/ninja.exe` |
| LLVM-MinGW | 20261006 (LLVM 23.1.3, UCRT) | `.local/toolchains/llvm-mingw-20261006-ucrt-x86_64/bin/clang.exe`, `clang++.exe` |

Контрольные суммы загрузок:

- `llvm-mingw-20261006-ucrt-x86_64.zip` — SHA256
  `317492c456aa27ee607a5919f1d2d38dcdc1112516a24d0bf4b00d078f52d17a` (совпала с GitHub).
- `cmake-4.4.4-windows-x86_64.zip` — 54 400 562 байт.
- `ninja-win.zip` — 291 570 байт.

## Как настроить сессию PowerShell

```powershell
$tools = "I:\goldenland2dec\.local\toolchains"
$env:PATH = "$tools\llvm-mingw-20261006-ucrt-x86_64\bin;$tools\ninja;$tools\cmake\cmake-4.4.4-windows-x86_64\bin;$env:PATH"
```

Сборка без Visual Studio:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

## Замечания

- `winget` не использовать; при следующей задаче просто выставить PATH как выше.
- LLVM-MinGW использует UCRT; целевые Windows-бинарники самодостаточны.
- Для Android (P9) потребуется NDK r28+ и JDK 17 — устанавливаются отдельно.
