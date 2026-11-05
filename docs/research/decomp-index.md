# Decomp reference index

Дата: 2026-11-05. Справочный корпус для переписывания логики в переносимый
C++20. Это **не** matching-decompilation: псевдокод не собирается и не
сравнивается байт-в-байт.

## Server.dll

- SHA-256: `418E256063748F2E90DB35AE7C6ABE6EAC401FB86908CB5F27EBF565A21CC837`
- Ghidra: `12.1.4_PUBLIC`; JDK `21.0.12.1+1`
- Функций: **1108**; декомпилировано: **1107**; thunk: **1**; ошибок: **0**
- Корпус (gitignored): `.local/decomp/server/`
  - `functions.csv` — `rva,size,name,thunk,callers` (1108 строк + заголовок)
  - `decompiled.c` — псевдокод каждой функции с RVA/размером
  - `meta.txt` — SHA-256, версии, счётчики
- Размер `decompiled.c`: ~2.47 МБ; `functions.csv`: ~31 КБ.

## Server.dll — native handlers

- Регистрация нативов: `FUN_14042464` (RVA `0x42464`), 65 вызовов
  `FUN_14004d00(registry, id, handler)`.
- Вызов по ID: `FUN_14039888` (RVA `0x39888`) → `FUN_14004f14`.
- Корпус (gitignored): `.local/decomp/server/natives/`
  - `natives.csv` — `id,rva,name,created` (65 строк + заголовок)
  - `natives.c` — псевдокод каждого handler'а с ID/RVA/именем
- Экспорт 2026-11-05: `done natives=65 created=33 ok=65 failed=0`.

## Воспроизведение

```powershell
$env:JAVA_HOME="$PWD\.local\toolchains\jdk-21.0.12.1+1"
$proj="$env:TEMP\opencode\ghidra_glserver"
New-Item -ItemType Directory -Force -Path $proj | Out-Null
& "$PWD\.local\toolchains\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" `
  $proj glserver -import "I:\Games\Златогорье-2\Server.dll" -overwrite `
  -scriptPath "$PWD\tools\ghidra" `
  -postScript ExportDecomp.java "I:\goldenland2dec\.local\decomp\server"
```

Native handlers выгружаются вторым проходом по тому же проекту:

```powershell
& "$PWD\.local\toolchains\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" `
  $proj glserver -process Server.dll -noanalysis `
  -scriptPath "$PWD\tools\ghidra" `
  -postScript ExportNativeHandlers.java "$env:TEMP\native_handlers.csv" `
  "$PWD\.local\decomp\server\natives"
```

`native_handlers.csv` строится из блока `FUN_14042464`: пары
`FUN_14005e78(..., s_NAME_..., 0xID)` и `FUN_14004d00(..., 0xID, handler)`.

Каталог проекта Ghidra должен существовать заранее (`New-Item`), иначе
`analyzeHeadless` падает с `Directory not found`.

## Ограничения

- Псевдокод — вход для ручного переписывания, а не исходник: типы, классы,
  vtable и границы структур восстанавливаются отдельно.
- В корпусе нет `Client.dll` и `GoldenLand.exe`; они выгружаются в следующих
  фазах по той же команде (заменить путь и имя проекта).
- Корпус не коммитится (`.local/` в `.gitignore`); в репозиторий попадают
  только этот индекс, `tools/ghidra/ExportDecomp.java` и
  `tools/ghidra/ExportNativeHandlers.java`.
