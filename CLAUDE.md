# ArcMind Life Counter

## Project Overview

Embedded C/C++ Magic: The Gathering life counter for the JC3636K518 /
Waveshare ESP32-S3 Knob Touch LCD 1.8. It uses Arduino CLI, ESP32 Arduino core,
and LVGL 8.3.11.

## Build And Flash

```bash
make build
make upload
```

The equivalent compile command is:

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,PartitionScheme=huge_app" \
  arcmind
```

## Critical Constraints

- Keep LVGL pinned to 8.3.11.
- Preserve the `huge_app` partition scheme.
- PSRAM is required for display and canvas buffers.
- Wi-Fi and Bluetooth are intentionally disabled.
- Keep RTC8M powered during light sleep; LEDC backlight PWM depends on it.
- GPIO 16, 17, and 18 conflict with QSPI display lines. Audio stays disabled
  unless the board is rewired.
- Do not replace queued encoder handling with direct UI calls from an ISR.

## Source Layout

```text
arcmind/
  arcmind.ino          setup, loop, battery sampling, and sleep behavior
  knob.c               UI screens, state, navigation, timers, and input
  knob.h               public application API
  pincfg.h             board pin assignments
  lv_conf.h            LVGL feature and memory configuration
  scr_st77916.h        display and touch initialization
  bidi_switch_knob.*   rotary encoder driver
  hal/                 LVGL hardware adapters
```

## Architecture

- `knob.c` owns the application state and all LVGL screens.
- Encoder events enter a fixed queue through `knob_change()` and are consumed
  by `knob_process_pending()` in the Arduino loop.
- The active LVGL screen determines how encoder events are routed.
- The menu overlay lives on `lv_layer_top()` and is shared by single-player and
  multiplayer modes.
- NVS namespace `arcmind` stores brightness, auto-dim, mirror, timer duration,
  and table layout preferences.

## Active Screens

- Intro
- Single-player counter
- Multiplayer counter
- Multiplayer player menu
- Player rename
- Commander damage selection and adjustment
- Damage-to-all adjustment
- Settings

Legacy dice, standalone player-selection, standalone damage, and old turn
counter screens are intentionally absent.

## Conventions

- Public application functions use the `knob_*` prefix for compatibility with
  the encoder driver and existing call sites.
- Internal functions use `snake_case`.
- Constants use `UPPERCASE_WITH_UNDERSCORES`.
- Keep UI changes compatible with a 360x360 circular display.
- Run `make build` after changes to C/C++ sources or LVGL configuration.
