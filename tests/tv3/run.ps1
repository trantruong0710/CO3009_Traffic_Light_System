param([string]$Compiler = "gcc")
$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$build = Join-Path $PSScriptRoot "build"
New-Item -ItemType Directory -Force -Path $build | Out-Null
$source = @("lcd.c", "dht20.c", "i2c_scanner.c", "fsm_tuning.c") | ForEach-Object { Join-Path $repo "Core/Src/$_" }
$exe = Join-Path $build "test_tv3.exe"
& $Compiler -std=c11 -Wall -Wextra -Werror -Wpedantic -include "$PSScriptRoot/mock_hal.h" -I "$repo/Core/Inc" @source "$PSScriptRoot/test_tv3.c" -o $exe
if ($LASTEXITCODE -ne 0) { throw "Host compile failed" }
& $exe
if ($LASTEXITCODE -ne 0) { throw "TV3 tests failed" }
