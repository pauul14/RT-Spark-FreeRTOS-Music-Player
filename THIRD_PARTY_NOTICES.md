# Third-party notices

This file lists the sources and licenses used by the project. The newly authored demonstration data and artwork have a scoped MIT license. The remaining original application code does not yet have a general reuse license. Third-party components retain their own licenses.

## Demonstration melodies and artwork

`include/custom_songs.h` contains eight newly authored note patterns written for this edition. They are not transcriptions of the private project's songs. Frequencies were calculated from equal temperament; durations and rests were specified directly for these patterns. No HiBit melody arrays, professor data or downloaded arrangements are included.

The abstract artwork arrays `bca_art_0` through `bca_art_7` in `include/player_ui_assets.h` were also created for this edition. They use colored bars and lines rather than the private edition's song-specific art.

These new melody arrays and artwork arrays use MIT, with the full text in [LICENSES/Demonstration-MIT.txt](LICENSES/Demonstration-MIT.txt). That license applies only to these newly authored components. It does not relicense font-derived data or existing application code.

## BCA UI raster font

Generated glyphs in `include/player_ui_assets.h` derive from Lato.

- Copyright (c) 2010-2014 by tyPoland Lukasz Dziedzic, `team@latofonts.com`.
- Source: <https://github.com/google/fonts/tree/main/ofl/lato>.
- License: SIL Open Font License 1.1; full notice in [include/player_ui_font_LICENSE.txt](include/player_ui_font_LICENSE.txt).
- Reserved Font Name: **Lato**. Modified raster subsets use **BCA UI**, not the Reserved Font Name.
- The generated font data remains under SIL OFL 1.1. Original TTF files and generation archives remain local. Firmware compilation uses the generated header.

## RT-Spark LCD and RGB hardware port

Copyright (c) 2006-2022, RT-Thread Development Team.

Official BSP: <https://github.com/RT-Thread-Studio/sdk-bsp-stm32f407-spark>.

Hardware logic in `src/checkpoint_ui.c` was adapted from:

- `libraries/Board_Drivers/lcd/drv_lcd.c`
- `libraries/Board_Drivers/led_matrix/drv_matrix_led.c`
- `projects/03_driver_lcd/board/CubeMX_Config/Src/stm32f4xx_hal_msp.c`

The LCD/matrix drivers declare Apache-2.0. Full text is in [LICENSES/Apache-2.0.txt](LICENSES/Apache-2.0.txt). The port replaces RT-Thread runtime APIs with STM32 HAL initialization, local rendering and FreeRTOS synchronization. The existing source attribution is retained. No RT-Thread runtime is linked. No standalone upstream NOTICE file was found in the inspected BSP tree.

The CubeMX MSP file carries an additional 2018 STMicroelectronics BSD-style redistribution notice, retained verbatim in [LICENSES/ST-CubeMX-2018-NOTICE.txt](LICENSES/ST-CubeMX-2018-NOTICE.txt). Preserve its copyright, conditions and disclaimer for adapted portions; do not imply ST endorsement.

## Framework dependencies

PlatformIO installs these dependencies separately; their source files are not included in this repository:

- FreeRTOS Kernel V10.3.1: Copyright (C) 2020 Amazon.com, Inc. or its affiliates; MIT notice in kernel sources. `freertos_build.py` selects the kernel, ARM_CM4F port and `heap_4.c` from STM32CubeF4.
- STM32CubeF4 HAL/device support: component-specific STMicroelectronics notices apply.
- CMSIS and toolchain components retain their respective upstream licenses.

The installed packages contain their own license files. Dependency sources and firmware binaries are excluded from Git. Their required notices should be reviewed before distributing a binary release.

## Application code license

A general reuse license for the original application code has not yet been selected. Do not apply the demo-data MIT license to the whole repository. The owner can choose an application-code license separately, while preserving the component notices above.

## Withheld materials

- Professor `song.h` / `song_def.h`: no redistribution grant found; unused by the active player.
- Favorites MIDI, extracted melodies/CSVs, WAVs and reports: accessibility was not treated as redistribution permission.
- All historical `reference/` checkpoints and upstream clones, original flash backups, firmware images, logs and local tools.
- IDE/environment settings and personal file paths.

These materials remain unchanged in the separate development project and are not copied into this public edition. The Git ignore rules also exclude these categories if they are later placed here.
