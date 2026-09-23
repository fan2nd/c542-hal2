# Letter Shell LED demo

Status: implemented and cross-compiled. Physical LED/button behavior and Stop1
power consumption still require board verification; no serial port was present
on the development machine during this change.

## Confirmed requirements

- Integrate Letter Shell into the existing STM32C542 demo.
- Add shell commands to turn the LED on and off.
- A physical button press toggles the LED.
- A sleep command turns the LED off and executes WFI.
- Only the button may wake the demo from sleep (user decision).
- Sleep must use Cortex-M DeepSleep (user decision).
- Select Stop1 and resume execution after wake-up (user decision).

## Existing hardware configuration

- Target: NUCLEO-C542RC, STM32C542RCT6.
- LED_0: PA5, active high (`generated/parts/mx_led.h`).
- BUTTON_0: PC13, active high; EXTI13 rising edge is enabled
  (`generated/parts/mx_button.h`, `generated/hal/mx_gpio_default.c`).
- USART2: PA2 TX, PA3 RX, 115200 baud, 8N1, no flow control
  (`generated/hal/mx_usart2.c`).
- `main.c`: system/application initialization and polling loop.
- `app_console.c`: UART queue, error recovery, shell commands and command table.
- `app_board.c`: button debounce, wake event, Stop1 entry and clock restoration.
- Generated drivers are unchanged.

## Console behavior

- LED starts off. The wake-up button press turns it on.
- Serial input received before/during sleep is discarded, including commands
  queued after `sleep`. Re-enter commands after wake-up.
- Commands: `led on`, `led off`, `led toggle`, `led status`, `sleep`, `help`.
- One toggle per debounced press; holding the button does not repeat.
- Debounce is 20 ms (`BUTTON_DEBOUNCE_MS`). Release the button before `sleep`;
  a held button causes the command to be rejected.
- CR, LF, and CRLF terminals are accepted. Tab completion, four history
  entries, arrow keys, Backspace and Delete are available.
- The local console has no password. No arbitrary-address execution or
  variable-writing commands are exposed.
- Lines are limited to 126 characters. Oversized lines and RX errors discard
  the whole current line; press Enter and re-enter the command.
- RX uses a 256-byte queue (255 usable bytes), with no flow control. Excessive
  pasted input can overflow it and is rejected rather than partially executed.

```text
led on
led off
led toggle
led status
help
sleep
```

Connect to the board's USART2/ST-LINK virtual COM port at 115200 8N1 with local
echo disabled. After `sleep`, only the physical USER button resumes the shell.

Letter Shell is vendored under `letter_shell/` with its MIT license and pinned
revision in `letter_shell/UPSTREAM.md`. Project options live in `shell_config.h`.

## Implementation constraints

- Use HAL_PWR_EnterStopMode(HAL_PWR_LOW_PWR_MODE_WFI, HAL_PWR_STOP1_MODE),
  which sets SLEEPDEEP, executes WFI, and clears SLEEPDEEP after wake-up.
- For Stop mode, restore the system/peripheral clocks before resuming serial
  communication. Check RAM retention and PC13 wake-up configuration.
- Do not unconditionally rerun mx_rcc_init after WFI: a pending interrupt can
  prevent Stop entry, leaving PSIS ready, and HAL_RCC_PSI_SetConfig rejects
  reconfiguration of ready outputs. Restore clocks using retained settings.
- Restore a correctly clocked SysTick before RCC calls that use tick-based
  timeouts; update it again after restoring the normal system clock.
- Suspend SysTick while sleeping to prevent periodic wake-ups; restore it
  after the wake-up event. Mask serial/DMA wake sources during sleep.
- Finish transmitting the sleep message before entering WFI.
- The existing button debounce uses HAL_GetTick and defaults to 10 ms.
  Its time-based filter cannot be reused unchanged across suspended ticks:
  a wake-up press soon after the previous press could be discarded.
- Rising-edge-only button interrupts do not report release; account for
  release when enforcing one toggle per physical press.
- Keep generated peripheral configuration and application changes separate
  using the project's existing extension points where practical.
- EXTI callback registration is enabled; UART callback registration is
  disabled, so use the HAL's weak UART callback overrides.
- Never call HAL_Delay from the higher-priority button/UART interrupt handlers.
- UART interrupt handlers only enqueue bytes or flag errors; the main loop
  executes commands. GPIO sampling also runs between short TX chunks so help
  output does not block button processing for the duration of the full message.
- The EXTI callback only records a wake event. After clock restoration, reject
  an input that is not still pressed after 20 ms and return to Stop1.
- SRAM1 and both SRAM2 pages retain data. Clock/RX restoration failure causes
  a system reset rather than continued execution with invalid peripheral state.

## Available build tools

The following executables were located and their versions checked:

- `C:/Users/admin/AppData/Local/stm32cube/bundles/cmake/4.2.3+st.1/bin/cmake.exe`
- `C:/Users/admin/AppData/Local/stm32cube/bundles/ninja/1.13.2+st.1/bin/ninja.exe`
- `C:/Users/admin/AppData/Local/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin/arm-none-eabi-gcc.exe`

Add these bin directories to the build process PATH, then configure and build
from this directory with preset `debug_GCC_NUCLEO-C542RC`. The older CubeIDE
CMake 3.28.1 does not meet this project's CMake 3.30 minimum.

```powershell
cmake --preset debug_GCC_NUCLEO-C542RC
cmake --build --preset debug_GCC_NUCLEO-C542RC
```

Outputs: `build/debug_GCC_NUCLEO-C542RC/c542.elf` and `c542.bin`. Load the ELF
with your usual STM32 programmer/debugger, or load the BIN at `0x08000000`.

## Verification

Completed on 2026-09-23: GCC 14.3.1 firmware build, PowerShell test-script
syntax validation, upstream file hash comparison, and ELF disassembly confirming
the Stop path sets SLEEPDEEP and executes `wfi`. Build size: 35,896 bytes text,
184 bytes data, 3,072 bytes BSS; raw binary: 36,080 bytes. No board was flashed
and the serial/physical tests below were not run because no COM port was present.

The subsequent split into `app_board` and `app_console` also passed the same
STM32 `cube-cmake` preset build. Command behavior and the serial smoke test are
unchanged; hardware behavior has not been revalidated after the split.

After flashing, run the included Windows/.NET serial check (no extra package):

```powershell
powershell -ExecutionPolicy Bypass -File .\test_demo.ps1 -Port COM5
powershell -ExecutionPolicy Bypass -File .\test_demo.ps1 -Port COM5 -Interactive
```

Replace COM5 with the actual port and close other serial terminals first. The
interactive run requires physical button presses and verifies two sleep/wake
cycles, rejection of serial wake attempts, discarded sleeping input, and no
repeated toggles while holding USER. Also inspect the physical LED when asked.

Cross-compilation and script syntax checks do not prove hardware wake behavior
or low current. Disconnect the debugger for low-power measurements: debug Stop
settings can keep clocks active. ST-LINK and board LEDs also contribute to USB
supply current; measure the MCU rail when checking Stop1 consumption.

## VS Code CMake recovery

On 2026-09-23, CMake Tools 1.24.42 failed during project initialization with
`No cache object found`. The existing File API reply index had an empty
`objects` array, and `.cmake/api/v1/query/client-vscode/` contained no query.
This is a missing File API cache object, not a firmware compilation error.
The failed driver initialization also prevented STM32 code indexing from
obtaining an active project.

Recovery performed: added a standard `client-vscode/query.json` requesting
`cache` v2, `codemodel` v2, `toolchains` v1 and `cmakeFiles` v1 under the build
directory, then configured with the STM32 extension's `cube-cmake`. The preset
file was temporarily deleted during diagnosis and subsequently restored by
the user. A failed intermediate configure also left Windows linker settings
in the cache; `cube-cmake --fresh --preset debug_GCC_NUCLEO-C542RC` regenerated
the ARM configuration. Building with the same wrapper then succeeded.

Validation: the generated File API index now contains all four requested
objects and the code model contains target `c542`. In VS Code, open this
`c542_cmake` folder, run `Developer: Reload Window`, select configure preset
`debug_GCC_NUCLEO-C542RC` if necessary, and build. The extension UI reload itself
was not automated; the wrapper build and its generated data were verified.
