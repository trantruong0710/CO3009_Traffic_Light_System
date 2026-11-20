# TV3 implementation and validation

> Historical review of 18 November. The runtime is now integrated and Amber coupling has been corrected. Current board commissioning instructions are in [BOARD_BRINGUP.md](BOARD_BRINGUP.md); the pre-integration limitations below describe that older revision, not the current source.

Reviewed 18 November 2026 against the existing v1.2 public headers and the supplied group specification. Scope: TV3 source and tests only. Main, shared headers, config, scheduler, GPIO/traffic FSMs and peripheral configuration are not changed by this branch.

## Delivered

- `Core/Src/dht20.c`: completes the user's switch-based FSM; uses the declared enum type, preserves the CRC/status checks and sample conversion, and retries failed sensors after a dedicated two-second error wait. Normal measurement interval is at least two seconds after completion.
- `Core/Src/i2c_scanner.c`: preserves the user's incremental scan; one probe per callback, ACK/NACK/error accounting, NULL bus handling, and corrected timeout comment.
- `Core/Src/lcd.c`: 16x2 HD44780/PCF8574 driver with named constants; bounded init steps, dirty-cell rendering, mode/phase/color display, six tuning fields (including Red and linked Green), temperature with one decimal, humidity, one-second notices and error placeholders. Up to four changed cells per call, at most 16 nibble transactions. The cursor snapshot remains fixed while searching each callback, so moving the next cursor does not skip cells. Failed init/data writes restart the LCD handshake and invalidate the display cache.
- `Core/Src/fsm_tuning.c`: BTN2 SHORT select; BTN3/4 DOWN +/-1; BTN2 LONG save. Ignores unrelated SHORT/LONG actions, consumes at most one action/call, and only runs in TUNING. BTN1 mode-exit cancellation belongs to TV1's mode manager. There is no held-button auto-repeat and no BTN4 LONG cancel in the agreed spec.

The older `HUONG_DAN_TV3.md` is a learning document, with alternative UI policies such as SHORT adjustment, BTN4 LONG Cancel, read-only Red and a two-second notice. It is **not** the acceptance specification for this branch. The source and this validation note follow the supplied group spec instead. That learning document is deliberately left unchanged and is not included in the implementation commit.

## Repeatable checks

From the repository root in PowerShell:

```powershell
./tests/tv3/run.ps1
./tests/tv3/build_arm.ps1 -Compiler 'FULL/PATH/TO/arm-none-eabi-gcc.exe'
```

Host runner requires GCC on PATH (or pass `-Compiler`). It uses the real application headers with a test-only HAL shim and config/action stubs. It compiles with `-std=c11 -Wall -Wextra -Werror -Wpedantic`.

Host result: **PASS** TV3-01..05 logic groups. Tests decode the PCF8574 transactions to a simulated LCD instead of inspecting implementation-private buffers. Coverage includes every mode/phase and tuning field, dirty and Red-linked views, sensor placeholders, copy/truncate/expiry of notices, initialization and data-write failures, known DHT20 25 C / 50% / CRC 0x23 frame, invalid CRC/status, HAL ERROR/BUSY/TIMEOUT, 100/80/2000 ms boundaries, BUSY deadline, retry, uint32 wrap, 112 scanner addresses, NACK/error distinction, all six adjustment fields, one DOWN action per press, ignored SHORT/LONG, save success/failure, cancel API contract, staged isolation, Green/Amber boundaries and scanner exclusivity.

ARM result: **compile/link PASS** using STM32CubeIDE 1.18.0 ARM GCC 13.3.1, Cortex-M3/thumb and the project's actual HAL/CMSIS headers, startup and linker script. All four TV3 translation units compile with `-Werror`. Generated `syscalls.c` has existing unused-parameter warnings and the existing linker script produces an RWX LOAD-segment warning; neither is hidden or modified here.

**Important limit:** the ARM smoke link uses the existing bring-up main.c, where only scanner is serviced. `--gc-sections` removes unreferenced module functions. This proves the translation units compile for ARM and the existing bring-up image links; it does not prove LCD/Tuning runtime integration or all application dependencies are linked. Host tests use TV1 contract stubs, not production config/router.

## Required integration with TV1

1. Initialize DHT20 and LCD after I2C1, then schedule their services (DHT20 100 ms; LCD 200 ms per spec). TIM2 must start and scheduler dispatch must run. Call LCD setters with actual mode/phase; prevent the automatic renderer from overwriting Tuning view.
2. Call tuning reset once on entry; keep traffic ALL_RED via the traffic/mode modules. Route DOWN/SHORT/LONG according to the specification. Cancel staged changes when BTN1 leaves Tuning. Only apply active configuration after the normal safe transition.
3. Scanner is diagnostic-only. Do not run/reschedule scanner_init in normal runtime or from ISR. While an initialized scan is incomplete, LCD and DHT20 suppress bus accesses. This is cooperative sequencing, not a lock for concurrent RTOS/DMA users.
4. Supply config_get_staged/get_red_s/begin_edit/adjust_staged/save_edit/cancel_edit/is_dirty and mode_manager_get_mode/take_action. Do not ship the host config/router stubs in firmware.
5. **Known TV1 mismatch:** inspected `origin/feature/core-runtime` at `78f86d043141c7ddde68af4d4d82734b0016d2b7`. Its config_adjust_staged changes Amber without compensating Green. A separate executable probe using that actual config.c produces R1 15 -> 16, G2=10, A2=4 when incrementing A2. The group spec requires R1=15, G2=9, A2=4. TV1 must correct the atomic trial-config coupling rule before integrated acceptance. TV3 delegates configuration writes through the frozen API and does not patch TV1's file. The host stub implements the required contract, so its success must not be claimed as production TV1 acceptance.

## Hardware status: PENDING

LCD assumption: 7-bit address 0x27 (override TV3_LCD_ADDRESS if measured differently); PCF8574 RS=P0, RW=P1, E=P2, backlight=P3 active high, D4..D7=P4..P7. Confirm the actual controller, mapping, voltage/pull-ups and wiring to I2C1 PB8/PB9. A different backpack mapping requires a driver change, not only an address change.

DHT20 address 0x38 and status protocol must match the actual device revision. Uncalibrated status is reported as SENSOR_ERROR; revision-specific register recovery is not implemented. No sample-freshness watchdog is claimed if the scheduler stops calling the driver.

Blocking HAL calls have internal waits; a two-ms timeout argument is **not** a hard execution-time bound. Four-cell rendering is a bounded work count, not measured WCET. Measure callback duration and scheduler jitter under normal bus traffic, NACK and stuck SDA/SCL; adapt to IT/DMA with TV1 if needed. Notices render incrementally; confirm their readability at 200-ms service cadence. Actual board sign-off, timing and 30-minute stability are still pending.
