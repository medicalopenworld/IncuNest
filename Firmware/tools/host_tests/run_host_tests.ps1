# Compila y ejecuta las suites Unity de host (sustituye a `pio test -e native`).
#
#   .\Firmware\tools\host_tests\run_host_tests.ps1            # todas
#   .\Firmware\tools\host_tests\run_host_tests.ps1 test_pid   # una suite
#
# Usa el g++ de MSYS2 (C:\msys64\mingw64\bin), que es el mismo compilador de
# host que usaba pre_native.py con PlatformIO, y el CMake/Ninja de la
# instalacion de ESP-IDF si estan; si no, los de MSYS2.
param(
  [string]$Suite = "",
  [switch]$Clean
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $here "build"

$mingw = "C:\msys64\mingw64\bin"
if (-not (Test-Path "$mingw\g++.exe")) {
  Write-Error "No se encuentra $mingw\g++.exe (MSYS2 mingw64). Es el compilador de host de los tests."
}
$env:PATH = "$mingw;$env:PATH"

# CMake: el de ESP-IDF si existe (misma version que usa idf.py), si no el del PATH.
$cmake = Get-ChildItem "C:\Espressif\tools\cmake\*\bin\cmake.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
$cmake = if ($cmake) { $cmake.FullName } else { "cmake" }
$ninja = Get-ChildItem "C:\Espressif\tools\ninja\*\ninja.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
$generatorArgs = if ($ninja) { @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$($ninja.FullName)") }
                 elseif (Test-Path "$mingw\ninja.exe") { @("-G", "Ninja") }
                 else { @("-G", "MinGW Makefiles") }

if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

& $cmake -S $here -B $build @generatorArgs `
  -DCMAKE_C_COMPILER="$mingw\gcc.exe" -DCMAKE_CXX_COMPILER="$mingw\g++.exe" `
  -DCMAKE_BUILD_TYPE=Debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $build --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
if (-not (Test-Path $ctest)) { $ctest = "ctest" }
$filter = if ($Suite) { @("-R", "^$Suite$") } else { @() }
& $ctest --test-dir $build --output-on-failure @filter
exit $LASTEXITCODE
