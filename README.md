# RT-Spark FreeRTOS Music Player

Public demonstration edition

This project was built for **BCA182 Laboratory Activity 2** using the RT-Thread RT-Spark / Spark-1 STM32F407 board. This public edition plays eight newly written demonstration melodies through the onboard buzzer. The firmware uses C++, STM32Cube HAL and FreeRTOS.

The LCD shows song artwork, playback progress, a five-second selection countdown and volume changes. Four onboard buttons handle song selection and playback controls.

## Features

- Eight demonstration melodies selected using stored binary bits that toggle on button taps.
- Song confirmation with a five-second countdown and timeout.
- Nonblocking note scheduling and TIM2 interrupt waveform generation on PB0.
- Play, pause, resume, manual stop and replay after stopping or completion.
- Pause preserves the current entry and remaining tone/rest duration.
- ST7789 LCD artwork, progress bar, pending-song display and volume feedback.
- Nineteen RGB pixels: green while confirming, blue while playing, red while paused, off when stopped.
- Five volume levels controlled through waveform duty, with UART instructions and diagnostics at 115200 baud.
- Three FreeRTOS application tasks, LCD/UART mutexes and a one-shot confirmation timer.
- Shallow Cortex-M4 idle sleep with normal interrupt wake-up.
- STM32Cube HAL; no Arduino framework or RT-Thread runtime.

## Hardware

| Component | Implementation |
| --- | --- |
| Board | RT-Thread RT-Spark / Spark-1, HW1.1 mappings |
| MCU | STM32F407ZGT6, Cortex-M4F, 1 MiB internal flash |
| Display | Onboard 240 × 240 ST7789V3, FSMC/8080 **8-bit** interface |
| Input | Four active-low onboard arrow keys with internal pull-ups |
| Status | Onboard 19-pixel addressable RGB matrix |
| Sound | Onboard BUZ1, transistor-driven from PB0 |
| Programming | Onboard ST-Link through USB-DBG |
| Diagnostics | USART1 through the ST-Link virtual COM port |

PlatformIO's `black_f407zg` target selects the MCU. Peripheral pins follow the RT-Spark mapping rather than the generic board target. The onboard ES8388 headphone codec is not used by this player.

## Demonstration playlist

These short patterns were written for this edition, not copied from the original Arcade arrangements. Each song has 24 entries: 20 tones and four rests.

| Binary | Index | Song | Duration |
| --- | ---: | --- | ---: |
| `000` | 0 | Pulse Path | 7.92 s |
| `001` | 1 | Amber Steps | 7.92 s |
| `010` | 2 | Blue Orbit | 7.92 s |
| `011` | 3 | Quiet Grid | 9.36 s |
| `100` | 4 | Copper Loop | 8.16 s |
| `101` | 5 | Night Signal | 8.40 s |
| `110` | 6 | Glass Ripple | 8.16 s |
| `111` | 7 | Homeward Line | 8.40 s |

Frequencies are rounded to whole Hz using 12-tone equal temperament with A4 at 440 Hz. A frequency of zero is a rest. Timing is stored directly in milliseconds: each rhythmic unit lasts 240 ms. Sounded entries leave a 35 ms gap within their interval; rests remain silent for the whole interval. No professor-song tempo scale is used.

The demonstration melodies and their abstract artwork are provided under the [demonstration MIT license](LICENSES/Demonstration-MIT.txt). The font and hardware port retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Button controls

All buttons use 25 ms debounce. The pending selection starts at `000` and persists after release.

| Gesture | Action |
| --- | --- |
| Tap DOWN | Toggle bit 0 on debounced release |
| Tap LEFT | Toggle bit 1 on debounced release |
| Tap RIGHT | Toggle bit 2 on debounced release |
| Tap UP | Open confirmation for the pending song |
| Release UP, then press UP within five seconds | Confirm and play that song from entry zero |
| Hold UP ≥1500 ms outside confirmation | PLAYING → PAUSED; PAUSED → PLAYING; STOPPED → replay confirmed song |
| Hold DOWN ≥1500 ms outside confirmation | Stop PLAYING/PAUSED playback, silence PB0 and reset progress |
| Hold LEFT alone ≥1200 ms, then release | Volume down one level |
| Hold RIGHT alone ≥1200 ms, then release | Volume up one level |

Outside confirmation, a long hold does not also count as a short selection tap. Release the button before repeating the action. For volume adjustment, hold LEFT or RIGHT alone without pressing any other button. During confirmation, bit changes and long-gesture actions are ignored; pressing UP confirms the captured candidate.

To select **Homeward Line**, starting from `000`:

1. Tap DOWN → `001`.
2. Tap LEFT → `011`.
3. Tap RIGHT → `111`.
4. Tap UP to open Homeward Line confirmation.
5. Release UP, then press it again before the countdown expires.

Tapping a bit again clears it. Set bits **before** opening confirmation. Changing pending bits does not interrupt playback. Confirmation starts the new song from its beginning; timeout retains the current song without restarting it.

Volume starts at 60%. UI levels 20/40/60/80/100% map to waveform duties 10/20/30/40/50%. HIGH and LOW times retain the same total musical period. Rests, pause and stop hold PB0 LOW.

## Hardware pin configuration

The active firmware uses these RT-Spark pin assignments.

| Function | Pin(s) | Configuration |
| --- | --- | --- |
| UP / select / confirm / pause / replay | PC5 | Active-low input, pull-up |
| DOWN / bit 0 / stop | PC1 | Active-low input, pull-up |
| LEFT / bit 1 / volume down | PC0 | Active-low input, pull-up |
| RIGHT / bit 2 / volume up | PC4 | Active-low input, pull-up |
| Buzzer | PB0 | GPIO toggled by TIM2 interrupt; LOW is silent |
| USART1 TX / RX | PA9 / PA10 | AF7, 115200, 8N1, no flow control |
| Matrix data | PA7 | AF2, TIM3 CH2, approximately 800 kHz |
| Matrix buffer enable | PF2 | Active LOW, 74LVC1G125 output enable |
| Matrix DMA | DMA1 Stream5 / Channel5 | TIM3 CH2 transmit |
| LCD D0-D3 | PD14, PD15, PD0, PD1 | AF12 FSMC |
| LCD D4-D7 | PE7, PE8, PE9, PE10 | AF12 FSMC |
| LCD read / write | PD4 / PD5 | FSMC NOE / NWE |
| LCD command/data | PD13 | FSMC A18 |
| LCD chip select | PG10 | FSMC NE3, bank 3 |
| LCD reset | PD3 | Active LOW |
| LCD backlight | PF9 | HIGH enables backlight |

TIM2 buzzer generation is independent of TIM3 RGB signaling. SysTick supplies both HAL timing and the 1 kHz FreeRTOS tick.

## FreeRTOS architecture

| Application task | Responsibility | Loop delay | Stack / priority |
| --- | --- | --- | --- |
| `update_lcd_leds_thread` | Read shared state; update LCD, progress/countdown and RGB | 50 ms | 512 words / 1 |
| `polling_buttons` | Debounce buttons, handle selection and playback controls, and advance notes | 5 ms | 512 words / 1 |
| `adjust_volume` | Apply waveform duty and update the shared volume state | 50 ms | 256 words / 1 |

FreeRTOS also creates its idle and timer-service tasks. The five-second one-shot timer callback only notifies the button task. Timeout transitions and UART output run in normal task context.

LCD rendering uses `lcd_mutex`; UART output uses `uart_mutex`. Short critical sections protect shared state. TIM2 performs waveform transitions while the button task advances notes without long playback delays.

`vApplicationIdleHook()` clears SLEEPDEEP and executes `DSB`, `WFI`, `ISB`. Tickless idle is disabled. The hook does not change clocks, mask interrupts, suspend SysTick, or enter STOP/STANDBY.

## Dependencies and installation

The last successful local build used:

| Dependency | Validated version / role |
| --- | --- |
| PlatformIO Core / IDE | Build and package management |
| `ststm32` platform | 19.7.1 |
| STM32CubeF4 package | 1.28.1 |
| FreeRTOS kernel | 10.3.1, supplied by STM32CubeF4 |
| ARM GCC | 7.2.1 |
| OpenOCD | PlatformIO-managed ST-Link uploader |

These versions describe the tested environment. The platform version is currently unpinned in `platformio.ini`. `freertos_build.py` compiles the Cube kernel, GCC ARM_CM4F port and `heap_4.c`. No Arduino library is required.

1. Install [VS Code](https://code.visualstudio.com/) and [PlatformIO IDE](https://platformio.org/install/ide?install=vscode).
2. Open the project directory and allow PlatformIO to install its STM32 packages.
3. Connect USB-DBG; install the official ST-Link Windows USB driver if necessary.

Alternatively, install PlatformIO Core with Python:

```powershell
python -m pip install --user platformio
```

## Build and ST-Link upload

All application sources, demo songs, generated artwork and font data needed to compile are included. PlatformIO installs the STM32 framework and toolchain dependencies. No private checkpoints, source fonts or downloaded arrangements are required.

```powershell
pio run -e black_f407zg -t clean
pio run -e black_f407zg
# Upload only after a successful build:
pio run -e black_f407zg -t upload
```

If `pio` is not on PATH, use PlatformIO's terminal or its managed executable:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e black_f407zg
```

The existing configuration uses STM32Cube and ST-Link. Successful upload output includes `Programming Finished`, `Verified OK` and `Resetting Target`. These commands do not request mass erase or option-byte changes.

For optional UART inspection, manually open the ST-Link VCP at **115200 baud, 8N1**. Windows previously assigned COM3; other PCs may assign a different port. Close other monitors first.

```powershell
pio device monitor --port COM3 --baud 115200
```

Exit with Ctrl+C. Startup output includes `BOOT: UART OK`, a `PLAYER READY | tasks=3` report, `BOOT: STARTING SCHEDULER` and control instructions. UART also reports pending songs, confirmation, timeout, transport and volume events.

## Testing and validation

The shared player engine passed the seven tests below in the private development project, which used a different playlist and artwork. The public demonstration melodies have not yet been physically tested.

| Physical test | Reported result |
| --- | --- |
| Buzzer playback | The previous playlist played on the actual board |
| Tap-to-toggle selection | Persistent combinations, including `111`, work with individual taps |
| Confirmation and timeout | Second UP confirms; five-second expiry retains current playback |
| Pause/resume | One action per long hold; current entry and remaining time preserved |
| Stop/replay | Manual stop silences the buzzer and resets progress; long UP replays after stop/completion |
| Volume gestures | LEFT/RIGHT holds adjust duty without selection changes |
| Integrated UI and responsiveness | LCD artwork/progress/countdown, RGB states and FreeRTOS remain functional |

USART1 output was also previously verified on COM3 at 115200 baud. Binary inspection confirmed `WFI` in the idle hook and the FreeRTOS idle task's call to it. Current consumption has not been measured.

A separate automated run passed **31 Cortex-M4 emulation tests** using the button and playback functions with simulated GPIO, time, RTOS services, UART and waveform output. The tests covered song mappings, bounce, gesture thresholds, timeout, pause, stop and replay. They do not measure hardware timing or power consumption. Test tools and logs remain local.

The public edition passed a clean `black_f407zg` build with no compiler or linker warnings: **38,876 bytes flash** and **19,316 bytes RAM**. All eight demo arrays passed checks for positive durations, rests and valid pitches.

## Limitations and laboratory adaptations

- The player generates one square-wave tone at a time. It does not decode MP3 files or play sampled audio, vocals or chords.
- The official BOM lists BUZ1 as an **active buzzer**, driven through an SS8050 NPN transistor. Tests on this board produced distinct musical pitches using timer-driven signals. The buzzer has limited sound quality and is not a passive speaker.
- Duty-based volume is approximate and nonlinear; displayed percentages are UI levels, not measured acoustic output or analog gain.
- Four onboard keys replace a separate USER_BUTTON. Tap-to-toggle replaces simultaneous held binary inputs; long gestures provide transport controls.
- LEFT/RIGHT gestures substitute for an external potentiometer/ADC input.
- Blocking HAL UART and its mutex can delay task-level note advancement. Waveform interrupts continue, but diagnostics can introduce scheduling jitter.
- Shallow sleep retains clocks and peripherals; frequent SysTick/audio interrupts limit power savings.
- The included demonstration patterns replace the private project's commercial arrangements. Professor material and downloaded arrangements are not included.
- FreeRTOS timers and HAL timer interrupts handle timeouts and periodic timing instead of Mbed Timeout/Ticker APIs.
- This is board-specific; another STM32F407 board needs verified wiring and drivers.

## Repository scope and licensing

The repository includes application and UI code, demonstration melodies, generated assets, configuration, documentation and license notices. It contains no private backups, professor files, original flash dumps, downloaded arrangements, or Favorites MIDI/WAV/transcriptions. Build output and local settings are ignored.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the license scope. The new demo melodies and abstract artwork use MIT. Font data uses SIL OFL 1.1; the hardware port retains Apache-2.0 and ST notices. No general reuse license has been selected for the remaining original application code. This does not affect compilation, but the repository should not be described as wholly MIT-licensed.
