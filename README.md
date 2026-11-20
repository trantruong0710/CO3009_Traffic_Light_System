# CO3009 Traffic Light System

Two-road controller for XNUCLEO-F103RB / STM32F103RB with Auto, Manual and Tuning, O(1) scheduler ISR, software timers, buttons, traffic interlocks, LCD and DHT20. The full runtime is integrated; hardware commissioning is still required.

## First board flash

The default profile is DIAGNOSTIC (`APP_DIAGNOSTIC_MODE=1` in `Core/Inc/board_config.h`). It runs a debugger-controlled bring-up task, samples raw buttons, scans I2C and services LCD/DHT20. Traffic pins boot as inputs and are driven only on an explicit debugger command. It intentionally does not run the Auto traffic cycle.

Follow [BOARD_BRINGUP.md](docs/BOARD_BRINGUP.md), including the measurement tables. Capture the actual four button levels, both traffic color truth tables and LCD address/mapping. Then enter them in board_config.h, set both verification flags, change APP_DIAGNOSTIC_MODE to 0 and rebuild. Configured runtime is rejected at compile time if verification or color codes are missing. Never flash the synthetic TestRuntime build.

## Build and flash in STM32CubeIDE

1. Import this directory as an existing project; leave Copy projects unchecked.
2. Refresh the project (F5), save files, Clean and Build. Source is under Core; host tests under tests are not firmware sources.
3. Connect the correct SWD programmer and power for the actual board. Select the project ELF from its Debug directory in Debug Configurations; use ST-LINK and SWD.
4. Debug to program, then Resume (F8) after stopping at main. Diagnostic expressions are g_board_diag and g_i2c_scan.

The project targets STM32F103xB, uses a 64 MHz system/timer clock and TIM2 PSC=6399, ARR=99 for a nominal 10 ms tick. I2C1 is PB8/PB9 with AFIO remap. Measure timing and check wiring on hardware.

## Host and ARM checks

```powershell
./tests/tv2/run.ps1
./tests/tv3/run.ps1
./tests/bringup/run.ps1
./tests/tv3/build_arm.ps1 -Compiler 'FULL/PATH/TO/arm-none-eabi-gcc.exe' -Profile Diagnostic
# Only after board_config.h contains verified measurements:
./tests/tv3/build_arm.ps1 -Compiler 'FULL/PATH/TO/arm-none-eabi-gcc.exe' -Profile ConfiguredRuntime
```

Hardware PASS requires actual board testing. Legacy docs/TV3_VALIDATION.md records an older pre-integration review: its missing-runtime and Amber-coupling observations were fixed on current main. Use BOARD_BRINGUP.md for current commissioning instructions. Generated syscalls may warn about unused parameters; the supplied linker script may produce an RWX segment warning under new GCC versions; no warning is evidence of board verification.

Runtime button mapping: BTN1 SHORT changes mode; Manual BTN2 SHORT NEXT, BTN3 DOWN STOP, BTN4 DOWN RESTART; Tuning BTN2 SHORT selects, BTN3/4 DOWN adjust, BTN2 LONG saves, BTN1 exits and cancels unsaved edits. Button held auto-repeat is not implemented by the specification. Green 5..60 s, Amber 2..5 s; Red is derived and edited through the opposite Green; Amber edits preserve the opposite Red.

## Ownership

TV1: core runtime/config/mode integration. TV2: button/traffic/Auto/Manual. TV3: LCD/DHT20/scanner/Tuning. Board configuration and bring-up are shared integration work. Preserve public APIs and review measured hardware changes with the team. There is no automatic push or merge performed by the local build scripts.
