
$ErrorActionPreference = "Stop"

# Run from repository root regardless of current directory.
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path

# Build outside the repository to avoid committing .exe files.
$buildDir = Join-Path ([IO.Path]::GetTempPath()) (
    "co3009-tv2-" + [guid]::NewGuid().ToString("N")
)

New-Item -ItemType Directory -Path $buildDir | Out-Null

Push-Location $repoRoot

try {
    Get-Command gcc -ErrorAction Stop | Out-Null

    $common = @("-std=c11", "-Wall", "-Wextra", "-Werror")

    function Run-Test {
        param(
            [string]$Name,
            [string[]]$Defines,
            [string[]]$Includes,
            [string[]]$Sources
        )

        Write-Host "`n=== $Name ==="

        $exe = Join-Path $buildDir "$Name.exe"

        & gcc @common @Defines @Includes @Sources -o $exe

        if ($LASTEXITCODE -ne 0) {
            throw "Compile FAILED: $Name"
        }

        & $exe

        if ($LASTEXITCODE -ne 0) {
            throw "Test FAILED: $Name"
        }
    }

    $trafficFlags = @(
        "-DTRAFFIC_HW_VERIFIED=1",
        "-DTRAFFIC_TEST",
        "-DTL1_RED_CODE=0",
        "-DTL1_AMBER_CODE=1",
        "-DTL1_GREEN_CODE=2",
        "-DTL1_OFF_CODE=3",
        "-DTL2_RED_CODE=0",
        "-DTL2_AMBER_CODE=2",
        "-DTL2_GREEN_CODE=1",
        "-DTL2_OFF_CODE=3"
    )

    Run-Test -Name "button" `
        -Defines @("-DBUTTON_HW_VERIFIED=1") `
        -Includes @("-Itests/mocks", "-ICore/Inc") `
        -Sources @(
            "Core/Src/button.c",
            "tests/mocks/test_button.c"
        )

    Run-Test -Name "traffic" `
        -Defines $trafficFlags `
        -Includes @("-Itests/traffic_mocks", "-ICore/Inc") `
        -Sources @(
            "Core/Src/traffic_light.c",
            "Core/Src/global.c",
            "tests/mocks/test_traffic.c"
        )

    Run-Test -Name "automatic" `
        -Defines @() `
        -Includes @("-ICore/Inc") `
        -Sources @(
            "Core/Src/fsm_automatic.c",
            "Core/Src/global.c",
            "tests/mocks/test_fsm_automatic.c"
        )

    Run-Test -Name "manual" `
        -Defines @() `
        -Includes @("-ICore/Inc") `
        -Sources @(
            "Core/Src/fsm_manual.c",
            "Core/Src/global.c",
            "tests/mocks/test_fsm_manual.c"
        )

    Run-Test -Name "integration" `
        -Defines (@("-DBUTTON_HW_VERIFIED=1") + $trafficFlags) `
        -Includes @("-Itests/integration_mocks", "-ICore/Inc") `
        -Sources @(
            "Core/Src/button.c",
            "Core/Src/traffic_light.c",
            "Core/Src/fsm_automatic.c",
            "Core/Src/fsm_manual.c",
            "Core/Src/global.c",
            "tests/test_tv2_integration.c"
        )

    Write-Host "`nPASS: ALL TV2 SOFTWARE TESTS" -ForegroundColor Green
}
finally {
    Pop-Location
    Remove-Item -LiteralPath $buildDir `
        -Recurse -Force -ErrorAction SilentlyContinue
}
