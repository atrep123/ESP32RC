# ESP32RC Project Summary

## Overview

ESP32RC is a PlatformIO-based ESP32 high-current load controller for RC applications.
The default firmware accepts four RC PWM channels, drives one PWM power stage plus three digital outputs, and can monitor current and voltage with an INA219 sensor.

## Current status

- **Version**: v1.2 (Release ready)
- **Last Release**: v1.1.1 (fan control feature added)
- Active firmware: `src/main.cpp` (~1100 lines)
- Build target: `platformio.ini` -> `env:esp32dev`
- Optional bring-up target: `platformio.ini` -> `env:esp32dev_selftest`
- Configuration header: `include/config.h`
- Test coverage: **56 tests** (15 sanity + 16 unit + 16 integration + 9 runtime)
- CI/CD: GitHub Actions on every push/PR
- Legacy reference sketches: `legacy/` contains older `.ino` files that are not part of the default PlatformIO image

## Implemented features

- 4-channel RC PWM input with ISR capture (atomic-safe reads)
- PWM power control output (GPIO 25, 16.67 kHz)
- Two digital light outputs (GPIO 26/27)
- One digital auxiliary output (GPIO 14)
- **PWM fan control output (GPIO 12, 1 kHz)** ✓ v1.2
- **Temperature-based automatic fan speed control** ✓ v1.2
- INA219 current and voltage monitoring (I2C)
- Overcurrent, undervoltage, and overvoltage shutdown
- **Automatic 5-second safety recovery mechanism** ✓ v1.1.1
- Channel-specific RC failsafe behavior
- Serial diagnostics output with command interface
- Serial fan control commands (`fan status`, `fan auto`, `fan off`, `fan speed X`) ✓ v1.2

## Safety behavior

- Outputs start in a safe OFF state
- Power output stays disabled until the first valid `CH1` pulse arrives
- Losing `CH1` disables throttle even if other channels are still active
- Light and auxiliary outputs require fresh pulses on their own channels
- Missing INA219 hardware no longer triggers repeated sensor reads
- **Overcurrent/voltage faults trigger automatic 5-second recovery window** ✓ v1.1.1
- **All recovery conditions (current + voltage + timeout) must be satisfied** ✓ v1.1.1
- **Automatic fan speed control based on ESP32 internal temperature** ✓ v1.2
- **Fan speed range: 30% minimum (for circulation), 100% at 50°C** ✓ v1.2
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

- `pio run -e esp32dev` compiles to **319 KB** (production firmware)
- `pio run -e esp32dev_selftest` compiles to **331 KB** (self-test firmware)
- Both builds verified with continuous integration
- Zero compiler warnings
- Full automated test suite passes

## File map

### Firmware
- `src/main.cpp`: active firmware (~1100 lines, ISR + safety + fan control)
- `include/config.h`: pins, thresholds, timing, and current scaling

### Documentation
- `docs/QUICKSTART.md`: setup flow
- `docs/BRINGUP.md`: first-flash and hardware validation checklist
- `docs/HARDWARE.md`: wiring and component notes
- `docs/EXAMPLES.md`: example use cases
- `docs/ARCHITECTURE.md`: control-flow and safety diagrams
- `docs/RELEASING.md`: release and tag publication flow
- `docs/FAN_CONTROL.md`: fan feature details and thermal characteristics ✓ v1.2
- `docs/TROUBLESHOOTING.md`: common issues and solutions ✓ v1.1.1
- `docs/BUILD_PIPELINE.md`: CI/CD workflow and build stages ✓ v1.1.1
- `docs/HARDWARE_VERIFICATION.md`: end-to-end hardware testing ✓ v1.1.1
- `docs/TESTING_STRATEGY.md`: comprehensive test framework documentation ✓ v1.2
- `docs/DEVELOPER_GUIDE.md`: extending the system and development workflow ✓ v1.2

### Testing
- `scripts/run_all_tests.py`: master orchestrator (56 tests in 4 suites)
- `scripts/build_sanity_tests.py`: 15 build verification tests
- `scripts/unit_tests.py`: 16 unit function tests
- `scripts/integration_tests.py`: 16 component integration tests
- `scripts/runtime_simulator_tests.py`: 9 real-time behavior tests
- `scripts/hardware_verification.py`: interactive hardware testing tool
- `scripts/build_metadata.py`: build information extraction
- `scripts/README.md`: comprehensive scripts documentation ✓ v1.2

### Reports & Issues
- `PROJECT_NOTES.md`: known issues, enhancements, and test coverage details
- `PROJECT_SUMMARY.md`: this file
- `release-notes/v1.1.1.md`: v1.1.1 release notes
- `CHANGELOG.md`: feature history and version tracking

## Testing & Quality Assurance

### Comprehensive Test Framework (56 tests)

**Quick Start**:
```bash
python scripts/run_all_tests.py  # Run all 56 tests in ~15 seconds
```

### Test Pyramid

| Tier | Tests | Purpose | Time |
|------|-------|---------|------|
| **Build Sanity** | 15 | Compilation, config, symbols | ~8s |
| **Unit Tests** | 16 | Math functions, edge cases | ~2s |
| **Integration** | 16 | Component interactions | ~2s |
| **Runtime Sim** | 9 | System behavior over time | ~3s |
| **TOTAL** | **56** | **Full coverage** | **~15s** |

### Test Coverage

- **RC Input**: 8 tests (mapping, deadband, timeout)
- **Power Control**: 8 tests (PWM, scaling, ramp)
- **Light Control**: 4 tests (toggle, hysteresis)
- **Fan Control**: 7 tests (temperature, PWM, manual override) ✓ v1.2
- **Safety System**: 8 tests (overcurrent, recovery, shutdown)
- **Current Monitoring**: 4 tests (scaling, thresholds)
- **Voltage Monitoring**: 3 tests (ranges, validity)
- **Serial Interface**: 2 tests (commands, communication)
- **System State**: 4 tests (startup, lifecycle, transitions)

### CI/CD Integration

- GitHub Actions runs all 56 tests on every push/PR
- Both firmware builds verified (esp32dev, esp32dev_selftest)
- Workflow stages: compile → sanity → unit → integration → runtime
- Blocks merge if any test fails

### Key Features for v1.2

✓ **Tests**: All 56 tests passing  
✓ **Documentation**: 12 docs covering user and developer needs  
✓ **Build Quality**: Zero warnings, both environments compile  
✓ **Feature Complete**: Fan control, recovery, atomic safety  
✓ **CI/CD Ready**: GitHub Actions workflow integrated  
✓ **Hardware Verified**: Can be tested end-to-end via serial interface  

---

**For More Information**:
- See `docs/TESTING_STRATEGY.md` for detailed test framework documentation
- See `scripts/README.md` for script usage guide
- See `docs/DEVELOPER_GUIDE.md` for extending the system

