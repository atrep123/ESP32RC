# ESP32RC Project Summary

## Overview

ESP32RC is a PlatformIO-based ESP32 high-current load controller for RC applications.
The default firmware accepts four RC PWM channels, drives one PWM power stage plus three digital outputs, and can monitor current and voltage with an INA219 sensor.

## Current status

- Active firmware: `src/main.cpp`
- Build target: `platformio.ini` -> `env:esp32dev`
- Optional bring-up target: `platformio.ini` -> `env:esp32dev_selftest`
- Configuration header: `include/config.h`
- Legacy reference sketches: `legacy/` contains older `.ino` files that are not part of the default PlatformIO image

## Implemented features

- 4-channel RC PWM input with ISR capture
- PWM power control output
- Two digital light outputs
- One digital auxiliary output
- INA219 current and voltage monitoring
- Overcurrent, undervoltage, and overvoltage shutdown
- Channel-specific RC failsafe behavior
- Serial diagnostics output

## Safety behavior

- Outputs start in a safe OFF state
- Power output stays disabled until the first valid `CH1` pulse arrives
- Losing `CH1` disables throttle even if other channels are still active
- Light and auxiliary outputs require fresh pulses on their own channels
- Missing INA219 hardware no longer triggers repeated sensor reads
- Saved current-scale calibration can be reused after reboot

## Electrical envelope

- Current target: up to 50A with appropriate external power hardware
- Stock monitored voltage range: 10V to 26V
- Default overcurrent shutdown: 48A
- PWM resolution: 8-bit
- PWM frequency: 1kHz

The 26V ceiling comes from the stock INA219-based monitoring path.
Systems above that need different voltage sensing and updated firmware limits.

## Build verification

`platformio run` succeeds for `env:esp32dev`.

## File map

- `src/main.cpp`: active firmware
- `include/config.h`: pins, thresholds, and current scaling
- `docs/QUICKSTART.md`: setup flow
- `docs/BRINGUP.md`: first-flash and hardware validation checklist
- `docs/HARDWARE.md`: wiring and component notes
- `docs/EXAMPLES.md`: example use cases
- `docs/ARCHITECTURE.md`: control-flow and safety diagrams
