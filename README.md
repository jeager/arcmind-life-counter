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

## Factory Registration

Each unit must be registered once after flashing. Registration writes a unique
device key into NVS and stores a SHA-256 hash of that key in Supabase.

1. Copy `.env.example` to `.env` and set `SUPABASE_URL` plus
   `SUPABASE_SERVICE_ROLE_KEY` from the ArcMind backend.
2. Install Node dependencies once with `npm install`.
3. Flash the firmware with `make upload`.
4. Register the connected device:

```bash
make register
make register NOTES="Felipe"
```

Unregistered devices boot to a lock screen and cannot download firmware from the
ArcMind installer. The web installer verifies the device over USB before serving
a short-lived install token and private firmware image.

### Encryption Notes

- **Distribution:** new firmware blobs are uploaded as private Vercel Blob objects
  and are only streamed through the authenticated API proxy.
- **Transport:** the browser installer uses HTTPS plus a signed 15-minute install
  token tied to a registered device.
- **On-device secret:** each unit stores a random 256-bit key in NVS namespace
  `arcmind_lic`. The device proves ownership with HMAC-SHA256 during install.
- **Stronger hardware protection:** ESP32 flash encryption and Secure Boot can be
  added later if you need to resist custom firmware being flashed over USB.

## Automated Firmware Releases

Every commit pushed to `main` runs
`.github/workflows/publish-firmware.yml`. The workflow:

1. builds the ESP32-S3 merged firmware image with the pinned toolchain
2. assigns a version in the form `1.0.<run number>-<short commit SHA>`
3. uploads the immutable firmware image to Vercel Blob
4. replaces `firmware/life-counter/latest.json` with the new version,
   publication timestamp, checksum, and source commit

The workflow requires a GitHub Actions repository secret named
`BLOB_READ_WRITE_TOKEN`. ArcMind reads `latest.json`, so its firmware installer
automatically displays the version and last-updated date published by the
workflow.

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
