$ErrorActionPreference = "Stop"
$repo=(Resolve-Path "$PSScriptRoot/../..").Path
$build=Join-Path ([IO.Path]::GetTempPath()) ("vxl-diag-"+[guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory $build | Out-Null
$exe=Join-Path $build "diagnostics.exe"
& gcc -DAPP_DIAGNOSTIC_MODE=1 -std=c11 -Wall -Wextra -Werror -Wpedantic -include "$PSScriptRoot/mock_hal.h" -I "$repo/Core/Inc" "$repo/Core/Src/board_diagnostics.c" "$repo/Core/Src/i2c_scanner.c" "$PSScriptRoot/test_diagnostics.c" -o $exe
if($LASTEXITCODE -ne 0){throw "Diagnostic compile failed"}
& $exe
if($LASTEXITCODE -ne 0){throw "Diagnostic tests failed"}
