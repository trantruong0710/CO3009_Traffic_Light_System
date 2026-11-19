param([Parameter(Mandatory=$true)][string]$Compiler)
$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$build = Join-Path $PSScriptRoot "build/arm"
New-Item -ItemType Directory -Force -Path $build | Out-Null
$common = @("-mcpu=cortex-m3", "-mthumb", "-std=gnu11", "-DUSE_HAL_DRIVER", "-DSTM32F103xB", "-O0", "-g3", "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra")
foreach ($include in @("Core/Inc", "Drivers/STM32F1xx_HAL_Driver/Inc", "Drivers/STM32F1xx_HAL_Driver/Inc/Legacy", "Drivers/CMSIS/Device/ST/STM32F1xx/Include", "Drivers/CMSIS/Include")) { $common += @("-I", (Join-Path $repo $include)) }
$source = @(Get-ChildItem -LiteralPath "$repo/Core/Src" -Filter *.c)
$source += @(Get-ChildItem -LiteralPath "$repo/Drivers/STM32F1xx_HAL_Driver/Src" -Filter *.c)
$objects = @()
foreach ($file in $source) {
    $obj = Join-Path $build ($file.BaseName + ".o")
    $extra = @()
    if ($file.Name -in @("lcd.c", "dht20.c", "i2c_scanner.c", "fsm_tuning.c")) { $extra += "-Werror" }
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
Write-Output "This builds the current bring-up main.c. Unreferenced TV3 functions are dead-stripped; full application integration remains TV1's task."
