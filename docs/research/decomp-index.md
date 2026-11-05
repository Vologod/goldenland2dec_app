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

Каталог проекта Ghidra должен существовать заранее (`New-Item`), иначе
`analyzeHeadless` падает с `Directory not found`.

## Ограничения

- Псевдокод — вход для ручного переписывания, а не исходник: типы, классы,
  vtable и границы структур восстанавливаются отдельно.
- В корпусе нет `Client.dll` и `GoldenLand.exe`; они выгружаются в следующих
  фазах по той же команде (заменить путь и имя проекта).
- Корпус не коммитится (`.local/` в `.gitignore`); в репозиторий попадают
  только этот индекс и `tools/ghidra/ExportDecomp.java`.
