# ESP32RC

ESP32RC is an ESP32-based high-current load controller for RC PWM inputs.
The active firmware is the PlatformIO project in `src/main.cpp`.
It reads up to four RC channels, drives one PWM power output plus three digital outputs, and can monitor current and voltage with an INA219 sensor.

**Latest Release**: [v1.3.0](release-notes/v1.3.0.md) - Precision Edition (March 7, 2026)
- ✅ 67/67 tests passing
- ✅ DS18B20 temperature sensor, SD telemetry logging, web dashboard
- ✅ 100% backward compatible

## Active firmware

- Build target: `platformio.ini` -> `env:esp32dev`
- Main source: `src/main.cpp`
- Configuration: `include/config.h`

## Features

### Core (v1.0+)
- 4-channel RC PWM input captured with GPIO interrupts
- PWM power output on `GPIO 25`
- Two light outputs on `GPIO 26` and `GPIO 27`
- One auxiliary output on `GPIO 14`
- INA219 current and voltage monitoring over I2C
- Overcurrent, undervoltage, and overvoltage shutdown
- RC failsafe with channel-specific validation
- Serial status output for diagnostics

### v1.3.0 Precision Edition ✨
- **🌡️ DS18B20 Temperature Sensor** (±0.5°C precision on GPIO 19)
  - 12-bit external temperature sensor with auto-fallback to internal ESP32 sensor
  - Improved fan control response and stability
- **💾 SD Card Telemetry Logging** (CSV historical data)
  - Automatic SD card initialization and format detection
  - 1000ms configurable logging interval (timestamp, power%, current, voltage, temperature, fan_speed)
  - Non-blocking operation maintains real-time control responsiveness
- **🌐 Web Dashboard Monitoring** (Real-time browser interface)
  - WiFi AP mode: "ESP32RC-Dashboard" / "ESP32RC123"
  - JSON API endpoint: `/api/status` (11 system state fields)
  - HTML dashboard with 6 metric cards and 500ms auto-refresh
  - Mobile-friendly responsive design

**All v1.3.0 features are optional and backward compatible** — firmware works with any combination of hardware present.

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

## Quick Start with v1.3.0 Features

### Enable SD Telemetry Logging
1. Insert a microSD card (formatted FAT32)
2. Connect to ESP32: GPIO 5 (CS), 23 (MOSI), 24 (MISO), 18 (CLK)
3. Flash firmware with `SD_CARD_ENABLED=1` (default)
4. Data logged to `/telemetry.csv` automatically

### Enable Web Dashboard
1. Flash firmware with `WEB_DASHBOARD_ENABLED=1` (default)
2. Connect to WiFi: "ESP32RC-Dashboard" / "ESP32RC123"
3. Open browser: `http://192.168.4.1`
4. View live metrics and JSON API at `/api/status`

### Enable DS18B20 Temperature Sensor
1. Connect DS18B20 to GPIO 19 (with 4.7kΩ pull-up)
2. Flash firmware with `TEMP_SENSOR_ENABLED=1` (default)
3. Read ±0.5°C precision temperature (fallback to internal if unavailable)

See [release-notes/v1.3.0.md](release-notes/v1.3.0.md) for detailed feature documentation and setup guides.

## Repo layout

- `src/main.cpp`: active firmware used by PlatformIO
- `include/config.h`: pin assignments and safety thresholds
- `docs/`: hardware, quick start, examples, and architecture notes
- `legacy/`: older standalone Arduino sketches kept for reference and not included in the PlatformIO build

## Documentation

- **Release Notes**: [v1.3.0 Precision Edition](release-notes/v1.3.0.md) - Latest features and migration guide
- **Quick Start**: [docs/QUICKSTART.md](docs/QUICKSTART.md)
- **First Hardware Setup**: [docs/BRINGUP.md](docs/BRINGUP.md)
- **Hardware & Schematics**: [docs/HARDWARE.md](docs/HARDWARE.md)
- **Examples & Projects**: [docs/EXAMPLES.md](docs/EXAMPLES.md)
- **System Architecture**: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- **Release Process**: [docs/RELEASING.md](docs/RELEASING.md)

## License

MIT. See `LICENSE`.
