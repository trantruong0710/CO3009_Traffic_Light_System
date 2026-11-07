# CO3009 Traffic Light System

Two-road traffic light course project for XNUCLEO-F103RB / STM32F103RB.

## Development status

GPIO, I2C1 and TIM2 are configured. The project contains 13 public headers
following the team's v1.2 design and a shared-state implementation in global.c.
The scheduler, drivers and traffic FSMs are not implemented yet. TIM2 is
configured for a nominal 10 ms period but is not started. Hardware verification
is pending.

## Build

1. Open STM32CubeIDE and select File > Import > General > Existing Projects into Workspace.
2. Select this repository directory; leave "Copy projects into workspace" unchecked.
3. Import CO3009_Firmware. Refresh (F5) after changing Git branches.
4. Select Project > Clean, then Build Project for CO3009_Firmware.

Development environment: STM32CubeIDE 1.7.0, GNU Tools for STM32
9-2020-q2-update. The .ioc records STM32Cube FW_F1 V1.8.7.

## Flash

1. Connect and power the board with the appropriate SWD programmer.
2. Select CO3009_Firmware and configure an STM32 Cortex-M C/C++ Run/Debug
   configuration using Debug/CO3009_Firmware.elf and the connected programmer.
3. Build, program and run the firmware.

The current firmware initializes peripherals only. Confirm the actual GPIO
mapping, traffic-light color codes, button levels, I2C devices and timer period
on the board before completing hardware verification.

## Source structure and ownership

| Owner | Files / modules |
| --- | --- |
| TV1 | app_types, global, config, scheduler, software_timer, mode_manager, main.c and .ioc |
| TV2 | button, traffic_light, fsm_automatic, fsm_manual |
| TV3 | lcd, dht20, fsm_tuning |

Application headers are in Core/Inc; source files are in Core/Src.
Each member uses their own GitHub account and submits changes through a PR.
