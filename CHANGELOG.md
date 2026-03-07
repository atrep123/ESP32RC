# Changelog

All notable changes to the ESP32RC project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Added a GitHub Actions release workflow that can publish releases from tags or via manual dispatch.
- Added a checked-in release notes file for `v1.1.0`.
- Added `docs/RELEASING.md` to document the release and tag workflow.
- Added `.gitattributes` and `.editorconfig` to keep line endings and basic text formatting consistent across Windows and CI.

## [1.1.0] - 2026-03-06

### Added
- Added a dedicated bring-up checklist in `docs/BRINGUP.md` for first flash and first hardware power-up.
- Added a `legacy/` folder with a small README for older standalone Arduino sketches kept only for reference.
- Added an `esp32dev_selftest` PlatformIO environment for serial-controlled output bring-up without a live RC signal.
- Added self-test serial commands for INA219 inspection and runtime current-scale tuning.
- Added persistence for calibrated current scale using ESP32 `Preferences`.
- Added a GitHub Actions workflow that builds both PlatformIO environments on pushes and pull requests.

### Changed
- Updated the main documentation to describe the active PlatformIO firmware in `src/main.cpp`.
- Moved old experimental `.ino` sketches out of the repository root into `legacy/`.
- Updated the stock monitored voltage limit in documentation and config to `26V` to match the default INA219-based monitoring path.
- Added an opt-in serial self-test mode gated behind the self-test build configuration.

### Fixed
- Fixed a startup failsafe issue where default RC values could allow non-zero PWM before the first valid `CH1` pulse.
- Fixed throttle validation so power output depends on fresh `CH1` data instead of unrelated active channels.
- Fixed ISR-shared RC timestamp handling by treating those values as interrupt-updated state.
- Fixed repeated INA219 reads when the sensor is not present.

## [1.0.0] - 2025-11-20

### Added
- Initial release of ESP32RC high-current load controller
- 4-channel RC PWM input with interrupt-based reading
- Variable power control (0-100%) with PWM output for MOSFET gates
- Digital light control with hysteresis for 2 independent channels
- Auxiliary output control via RC channel 4
- Real-time current and voltage monitoring using INA219 sensor
- Overcurrent protection with automatic shutdown
- Overvoltage and undervoltage protection
- RC signal loss detection with failsafe behavior
- Comprehensive serial output with system status
- Configurable safety thresholds and limits
- Support for loads up to 50A (with appropriate external hardware)
- PlatformIO project configuration for ESP32
- Complete documentation:
  - README with features, setup instructions, and operation guide
  - HARDWARE.md with detailed schematics, BOM, and assembly instructions
  - EXAMPLES.md with 5 practical use cases and customization guides
  - QUICKSTART.md for rapid setup and troubleshooting
- MIT license

### Technical Details
- Platform: ESP32 (Arduino framework)
- PWM Frequency: 1kHz (configurable)
- PWM Resolution: 8-bit (0-255)
- RC Signal: Standard 1000-2000μs pulses
- Current Sensor: Adafruit INA219 via I2C
- Monitoring Interval: 100ms for safety checks
- Serial Output: 500ms update interval at 115200 baud

### Safety Features
- Automatic shutdown on overcurrent (configurable threshold, default 48A)
- Automatic shutdown on overvoltage (configurable, default 26V)
- Automatic shutdown on undervoltage (configurable, default 10V)
- Failsafe mode on RC signal loss (>100ms timeout)
- Input validation for all RC PWM signals
- Safe default state (all outputs zero/off)

### Hardware Support
- ESP32 development boards (all variants)
- INA219 current/voltage sensor
- Standard RC receivers with PWM output
- External power stage (MOSFET + gate driver)
- Voltage range: 10-60V (configurable)
- Current range: 0-50A+ (with proper external hardware)

### Configuration
- Pin assignments fully configurable via config.h
- Safety limits adjustable in config.h
- PWM frequency and resolution configurable
- Light control thresholds with hysteresis
- RC signal parameters customizable

### Documentation
- Complete hardware assembly guide with schematics
- Bill of Materials (BOM) with recommended parts
- Wiring diagrams for various configurations
- 5 detailed use case examples:
  1. RC boat motor controller
  2. RC truck with lights
  3. High-power LED controller
  4. Electric skateboard ESC
  5. Battery-powered winch control
- Calibration procedures for current sensor
- Troubleshooting guide
- Safety guidelines and warnings
- Quick start guide for rapid setup

### Known Limitations
- Requires external power stage (MOSFET + driver) for high current
- Current sensor scaling factor must be adjusted for specific hardware
- No built-in wireless connectivity (ESP32 WiFi/BLE not utilized in v1.0)
- No data logging to SD card (can be added, see EXAMPLES.md)
- Single direction motor control (bidirectional requires additional hardware)

### Future Enhancements (Planned)
- Watchdog timer integration for automatic recovery
- EEPROM/Preferences for configuration persistence
- WiFi web interface for remote monitoring
- Telemetry output for RC systems
- Battery state-of-charge estimation
- Regenerative braking support
- Multi-motor control
- Bluetooth app control
- Data logging to SD card
- OLED display support

## Release Notes

### Version 1.1.0
This release hardens the runtime safety behavior of the active PlatformIO firmware and aligns the repository around that firmware as the source of truth.

Highlights:
- Safe startup behavior with no output before the first valid `CH1` pulse
- Throttle gated strictly by live `CH1` data
- Optional serial self-test firmware for bring-up without a live RC transmitter
- Runtime and persisted current-scale calibration using ESP32 `Preferences`
- Cleaned repository layout with legacy sketches moved out of the root
- GitHub Actions build coverage for both PlatformIO environments

### Version 1.0.0
This is the initial stable release of ESP32RC. The system has been designed with safety as the top priority and includes comprehensive protection features for high-current applications. All core features are implemented and tested for safety-critical behavior.

The documentation is comprehensive and includes everything needed to build and deploy the system, from complete hardware schematics to practical use cases.

This release is suitable for:
- RC vehicles (boats, trucks, crawlers)
- High-power LED control
- Motor speed controllers
- Battery-powered equipment
- DIY electric vehicles
- Robotics applications

**Important**: Always follow safety guidelines when working with high-current systems. Start testing with low currents and gradually increase to rated values.

---

For more information, see the [README](../README.md) and [documentation](.).
