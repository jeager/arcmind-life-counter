# ArcMind Life Counter

ArcMind is an open-source Magic: The Gathering life counter for the
JC3636K518 / Waveshare ESP32-S3 Knob Touch LCD 1.8.

## Features

- Single-player and four-player life tracking from -999 to 999
- Commander damage tracking
- Damage-to-all controls
- Configurable multiplayer turn timer and first-player selection
- Rectangular and round-table multiplayer layouts
- Player renaming, mirrored text, brightness control, and auto-dim
- Battery voltage estimate and persistent settings
- Rotary encoder and touch input

The shared menu switches modes contextually: it shows `Multiplayer` from the
single-player screen and `1 Player` from the multiplayer screen.

## Hardware

| Component | Details |
| --- | --- |
| MCU | ESP32-S3 at 240 MHz |
| Display | 1.8-inch 360x360 IPS, ST77916 QSPI |
| Touch | CST816S over I2C |
| Input | Incremental rotary encoder |
| Backlight | LEDC PWM on GPIO 47 |
| Battery | ADC monitoring on GPIO 1 |

Pin assignments are defined in `arcmind/pincfg.h`.

## Build

Install [Arduino CLI](https://docs.arduino.cc/arduino-cli/installation/), then:

```bash
arduino-cli core update-index \
  --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32 \
  --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install lvgl@8.3.11
arduino-cli lib install ESP32_Display_Panel@1.0.0
arduino-cli lib install ESP32_IO_Expander@1.0.1
arduino-cli lib install esp-lib-utils@0.1.2
```

Build with:

```bash
make build
```

Or invoke Arduino CLI directly:

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,PartitionScheme=huge_app" \
  arcmind
```

Connect the device and flash it with:

```bash
make upload
```

`make upload` checks `/dev/cu.usbmodem101` and `/dev/cu.usbmodem1101`.
Use `arduino-cli board list` and the direct upload command for any other port.

## Constraints

- Use LVGL 8.3.11. Later major versions are not compatible with this code.
- PSRAM is required for display and rotated-text buffers.
- The `huge_app` partition scheme is required; OTA is not used.
- Wi-Fi and Bluetooth are disabled to reduce power consumption.
- GPIO 16, 17, and 18 are shared with the QSPI display, so audio is disabled
  unless the hardware is rewired.

## Project Layout

```text
arcmind/
  arcmind.ino          Arduino entry point and power management
  knob.c               Application state, UI, navigation, and input handling
  knob.h               Public application API
  arcmind_logo.*       Startup logo asset
  skull.*              Eliminated-player overlay asset
  pincfg.h             Board pin assignments
  lv_conf.h            LVGL configuration
  scr_st77916.h        Display and touch initialization
  bidi_switch_knob.*   Rotary encoder driver
  hal/                 LVGL display, input, and tick adapters
```

This is a non-commercial hobby project and is not affiliated with the hardware
vendors or Wizards of the Coast.
