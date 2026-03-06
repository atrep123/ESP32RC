# ESP32RC

ESP32RC is an ESP32-based high-current load controller for RC PWM inputs.
The active firmware is the PlatformIO project in `src/main.cpp`.
It reads up to four RC channels, drives one PWM power output plus three digital outputs, and can monitor current and voltage with an INA219 sensor.

## Active firmware

- Build target: `platformio.ini` -> `env:esp32dev`
- Main source: `src/main.cpp`
- Configuration: `include/config.h`

## Features

- 4-channel RC PWM input captured with GPIO interrupts
- PWM power output on `GPIO 25`
- Two light outputs on `GPIO 26` and `GPIO 27`
- One auxiliary output on `GPIO 14`
- INA219 current and voltage monitoring over I2C
- Overcurrent, undervoltage, and overvoltage shutdown
- RC failsafe with channel-specific validation
- Serial status output for diagnostics

## Safety notes

- Power output stays off until a valid pulse is received on `CH1`.
- Power output depends only on a fresh `CH1` signal.
- Light and auxiliary outputs require fresh pulses on their own channels.
- The stock monitoring path uses `INA219`, so the default monitored bus limit is `26V`.
- If you need systems above `26V`, replace the voltage-sensing approach and update the firmware safety limits before use.

## Default pin map

| Function | Pin |
|----------|-----|
| RC CH1 power input | `GPIO 34` |
| RC CH2 light 1 input | `GPIO 35` |
| RC CH3 light 2 input | `GPIO 32` |
| RC CH4 auxiliary input | `GPIO 33` |
| Power PWM output | `GPIO 25` |
| Light 1 output | `GPIO 26` |
| Light 2 output | `GPIO 27` |
| Auxiliary output | `GPIO 14` |
| I2C SDA | `GPIO 21` |
| I2C SCL | `GPIO 22` |

## Build

```bash
platformio run
platformio run --target upload
platformio device monitor
```

Optional self-test firmware:

```bash
platformio run -e esp32dev_selftest --target upload
platformio device monitor
```

The `esp32dev_selftest` environment enables a serial-controlled bring-up mode without a live RC signal.
It also exposes sensor and current-scale calibration commands over serial, including saving the calibrated scale in ESP32 `Preferences`.

## Repo layout

- `src/main.cpp`: active firmware used by PlatformIO
- `include/config.h`: pin assignments and safety thresholds
- `docs/`: hardware, quick start, examples, and architecture notes
- `legacy/`: older standalone Arduino sketches kept for reference and not included in the PlatformIO build

## Documentation

- `docs/QUICKSTART.md`
- `docs/BRINGUP.md`
- `docs/HARDWARE.md`
- `docs/EXAMPLES.md`
- `docs/ARCHITECTURE.md`

## License

MIT. See `LICENSE`.
