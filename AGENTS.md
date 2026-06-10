# ArcMind Agent Guide

## Scope

This repository contains firmware for an ESP32-S3, 360x360 touch display, and
rotary encoder. Treat hardware pin assignments, LVGL memory settings, and power
management as device-specific constraints.

## Working Rules

- Build with `make build` before committing firmware changes.
- Keep LVGL at 8.3.11 and use the `huge_app` partition scheme.
- Preserve user changes in a dirty worktree.
- Keep edits focused; `arcmind/knob.c` is already large and should not gain
  unrelated abstractions.
- Never call LVGL directly from the encoder ISR. Queue input through
  `knob_change()` and process it in the main loop.
- Maintain both touch and rotary encoder behavior for interactive controls.
- Avoid enabling Wi-Fi, Bluetooth, or audio without accounting for power and
  pin conflicts.

## Important Paths

- `arcmind/arcmind.ino`: startup, battery ADC, sleep, and main loop
- `arcmind/knob.c`: UI, application state, navigation, timers, and input
- `arcmind/pincfg.h`: board pins and hardware feature flags
- `arcmind/lv_conf.h`: LVGL compile-time configuration
- `arcmind/hal/`: LVGL hardware adapters

## Validation

Use:

```bash
make build
```

For hardware validation, verify single-player and multiplayer navigation,
encoder life changes, touch controls, settings persistence, auto-dim wakeup,
and the multiplayer timer.
