param(
    [Parameter(Mandatory=$true)][string]$Compiler,
    [ValidateSet("Diagnostic", "ConfiguredRuntime", "TestRuntime")]
    [string]$Profile = "Diagnostic"
)
$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$build = Join-Path $PSScriptRoot ("build/arm-" + $Profile)
New-Item -ItemType Directory -Force -Path $build | Out-Null
$common = @("-mcpu=cortex-m3", "-mthumb", "-std=gnu11", "-DUSE_HAL_DRIVER", "-DSTM32F103xB", "-O0", "-g3", "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra")
if ($Profile -eq "Diagnostic") {
    $common += "-DAPP_DIAGNOSTIC_MODE=1"
} elseif ($Profile -eq "ConfiguredRuntime") {
    $common += "-DAPP_DIAGNOSTIC_MODE=0"
} else {
    # COMPILE TEST ONLY: synthetic values, never flash this image to a board.
    $common += @("-DAPP_DIAGNOSTIC_MODE=0", "-DBUTTON_HW_VERIFIED=1", "-DTRAFFIC_HW_VERIFIED=1",
        "-DTL1_RED_CODE=0", "-DTL1_AMBER_CODE=1", "-DTL1_GREEN_CODE=2", "-DTL1_OFF_CODE=3",
        "-DTL2_RED_CODE=0", "-DTL2_AMBER_CODE=2", "-DTL2_GREEN_CODE=1", "-DTL2_OFF_CODE=3")
    Write-Warning "TEST RUNTIME: synthetic pin codes; DO NOT FLASH this image."
}
foreach ($include in @("Core/Inc", "Drivers/STM32F1xx_HAL_Driver/Inc", "Drivers/STM32F1xx_HAL_Driver/Inc/Legacy", "Drivers/CMSIS/Device/ST/STM32F1xx/Include", "Drivers/CMSIS/Include")) { $common += @("-I", (Join-Path $repo $include)) }
$source = @(Get-ChildItem -LiteralPath "$repo/Core/Src" -Filter *.c)
$source += @(Get-ChildItem -LiteralPath "$repo/Drivers/STM32F1xx_HAL_Driver/Src" -Filter *.c)
$objects = @()
foreach ($file in $source) {
    $obj = Join-Path $build ($file.BaseName + ".o")
    $extra = @()
    if ($file.Name -in @("lcd.c", "dht20.c", "i2c_scanner.c", "fsm_tuning.c", "board_diagnostics.c", "main.c", "traffic_light.c", "button.c")) { $extra += "-Werror" }
    & $Compiler @common @extra -c $file.FullName -o $obj
    if ($LASTEXITCODE -ne 0) { throw "ARM compile failed: $($file.Name)" }
    $objects += $obj
}
$startup = Join-Path $build "startup.o"
& $Compiler -mcpu=cortex-m3 -mthumb -c "$repo/Core/Startup/startup_stm32f103rbtx.s" -o $startup
if ($LASTEXITCODE -ne 0) { throw "Startup assemble failed" }
$objects += $startup
$elf = Join-Path $build "CO3009_Firmware.elf"
& $Compiler -mcpu=cortex-m3 -mthumb --specs=nano.specs --specs=nosys.specs -T "$repo/STM32F103RBTX_FLASH.ld" "-Wl,--gc-sections" "-Wl,-Map=$build/CO3009_Firmware.map" @objects "-Wl,--start-group" -lc -lm "-Wl,--end-group" -o $elf
if ($LASTEXITCODE -ne 0) { throw "ARM link failed" }
Write-Output "ARM compile/link PASS: $elf"
Write-Output "Profile: $Profile. Build/link is not board verification. TestRuntime must never be flashed."
