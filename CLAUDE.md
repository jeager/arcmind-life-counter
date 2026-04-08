# Knobby MTG Life Counter — CLAUDE.md

## Project Overview

Embedded C MTG life counter for the **JC3636K518** (Waveshare ESP32-S3 Knob Touch LCD).
Built with Arduino CLI + LVGL 8.3. GUI-heavy, hardware-specific project with no traditional build system.

## Hardware

| Component | Details |
|-----------|---------|
| CPU | ESP32-S3 @ 240 MHz |
| Display | 1.8" IPS, 360×360, ST77916 driver (QSPI) |
| Touch | CST816S (I2C) |
| Input | Incremental rotary encoder (GPIO 7, 8) |
| Backlight | PWM via LEDC on GPIO 47 |
| Battery | ADC pin 1 (voltage monitoring) |

Pin assignments are in `knobby/pincfg.h`.

## Build & Flash

**Prerequisites** (one-time):
```bash
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install lvgl@8.3.11
arduino-cli lib install ESP32_Display_Panel@1.0.0
arduino-cli lib install ESP32_IO_Expander@1.0.1
arduino-cli lib install esp-lib-utils@0.1.2
```

**Compile:**
```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio" \
  knobby
```

**Flash** (find port with `arduino-cli board list`):
```bash
arduino-cli upload \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio" \
  -p /dev/cu.usbmodem1 \
  knobby
```

## Critical Constraints

- **LVGL must be 8.3.x** — 8.4+ breaks the project. Do not upgrade.
- **No WiFi/Bluetooth** — disabled at startup for power savings. Do not add wireless features without understanding power impact.
- **PSRAM required** — display buffers live in PSRAM. Hardware config is fixed.
- **Light sleep uses RTC8M clock** — keep `esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M, ESP_PD_OPTION_ON)` when modifying sleep code; backlight PWM (LEDC) needs it.

## Source Layout

```
knobby/
├── knobby.ino          # Arduino entry point: setup(), loop(), sleep logic
├── knob.c              # Core app logic — all GUI, screens, state (2400+ lines)
├── knob.h              # Public API for knob.c
├── pincfg.h            # All GPIO pin assignments
├── lv_conf.h           # LVGL config: color depth, heap size, timing
├── scr_st77916.h       # Display + touch init, LVGL driver registration
├── bidi_switch_knob.c  # Rotary encoder driver (modified Espressif component)
└── hal/                # LVGL HAL: display flush, input read, tick callbacks
```

## Architecture

- **`knob.c`** is monolithic — all screen logic and state lives here as static globals.
- Encoder events are queued (max 8/frame) via `knob_change()` ISR → processed by `knob_process_pending()` each loop.
- Screen transitions use a simple state machine; `handle_knob_event()` routes encoder events to the active screen.
- LVGL timers handle UI refresh, blink effects, and auto-dim (30s inactivity → light sleep).
- NVS (Non-Volatile Storage) persists brightness setting across reboots.

## Screens

| Screen | Purpose |
|--------|---------|
| `screen_intro` | Startup animation |
| `screen_main` | Primary life counter (1v1) |
| `screen_select` | Game mode selection |
| `screen_damage` | Enemy/commander damage tracking |
| `screen_settings` | Brightness control |
| `screen_dice` | D20 roller |
| `screen_multiplayer` | 4-player mode |
| `screen_multiplayer_*` | Multiplayer submenus |

## Life Tracking

- Range: -999 to +999
- Default starting life: 40
- Color coding: red (<11), yellow (11–30), green (30–40), purple (>40)
- Commander damage: 4 separate pools per player

## Code Conventions

- Public API: `knob_*` prefix (e.g., `knob_gui()`, `knob_read_battery_voltage()`)
- Internal functions: `snake_case` (e.g., `build_main_screen()`, `change_life()`)
- Constants: `UPPERCASE_WITH_UNDERSCORES`
- LVGL callbacks: descriptive, no prefix convention enforced
- Avoid adding global state outside `knob.c` unless hardware-specific

## LVGL Usage Notes

- Color depth: 16-bit RGB565 (`LV_COLOR_DEPTH 16`)
- LVGL heap: 48 KB
- Display refresh: 15 ms timer
- Input polling: 30 ms
- Display buffer: 72 rows × 360px × 2 bytes (~51 KB, in PSRAM)
- Custom 7-segment-style digit rendering (not standard LVGL fonts)
