# ESP32RC Documentation Hub

Complete documentation for the ESP32RC high-current load controller project.

## Quick Start

**First time?** Start here:
1. [QUICKSTART.md](QUICKSTART.md) - Get up and running in 5 minutes
2. [HARDWARE.md](HARDWARE.md) - Understand the hardware
3. [BRINGUP.md](BRINGUP.md) - First flash and validation

## User Documentation

| Document | Purpose | Audience |
|----------|---------|----------|
| [QUICKSTART.md](QUICKSTART.md) | Get firmware on ESP32 | Everyone |
| [HARDWARE.md](HARDWARE.md) | Wiring diagrams and components | Hardware integrators |
| [EXAMPLES.md](EXAMPLES.md) | Example use cases and configurations | Application builders |
| [BRINGUP.md](BRINGUP.md) | First-flash validation checklist | Hardware testers |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Common issues and solutions | Everyone (fixing problems) |

## Developer Documentation

| Document | Purpose | Audience |
|----------|---------|----------|
| [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) | How to extend the system | Firmware developers |
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design and control flow | System architects |
| [TESTING_STRATEGY.md](TESTING_STRATEGY.md) | Test framework (56 tests) | QA engineers |
| [BUILD_PIPELINE.md](BUILD_PIPELINE.md) | CI/CD workflow | DevOps engineers |
| [HARDWARE_VERIFICATION.md](HARDWARE_VERIFICATION.md) | End-to-end hardware testing | Hardware engineers |

## Configuration & Release

| Document | Purpose | Audience |
|----------|---------|----------|
| [RELEASING.md](RELEASING.md) | Version tagging and release process | Release managers |
| [../include/README.md](../include/README.md) | Configuration guide (config.h) | Anyone modifying parameters |
| [../src/README.md](../src/README.md) | Firmware implementation details | Firmware developers |

## Scripts & Tools

| Document | Purpose |
|----------|---------|
| [../scripts/README.md](../scripts/README.md) | All test scripts and utilities |

## Testing

### Run All Tests (56 total)

```bash
python scripts/run_all_tests.py
```

**Test Coverage** (15 + 16 + 16 + 9):
- 15 build sanity tests ✅
- 16 unit tests ✅
- 16 integration tests ✅
- 9 runtime simulation tests ✅

**Expected Result**: `56/56 PASS` in ~15 seconds

### Run Specific Test Suite

```bash
python scripts/build_sanity_tests.py    # 15 tests
python scripts/unit_tests.py             # 16 tests
python scripts/integration_tests.py      # 16 tests
python scripts/runtime_simulator_tests.py # 9 tests
```

### Hardware Testing

```bash
# After flashing to device
python scripts/hardware_verification.py
```

## Version Information

| Version | Date | Features |
|---------|------|----------|
| v1.2 | 2026-03-07 | Fan control, recovery, comprehensive docs |
| v1.1.1 | 2026-03-07 | Atomic reads, recovery mechanism, tests |
| v1.1 | Earlier | Initial feature set |

## Build Commands

```bash
# Production firmware
pio run -e esp32dev                    # 319 KB

# Self-test firmware
pio run -e esp32dev_selftest           # 331 KB

# Both in parallel
pio run -e esp32dev -e esp32dev_selftest

# Verbose output
pio run -v -e esp32dev

# Clean and rebuild
pio run --target clean -e esp32dev
pio run -e esp32dev
```

## Serial Interface

**Connection**: USB-C to ESP32-DevKit  
**Baud Rate**: 115200 (8N1)  
**Commands Available**:
- `status` - Print current state
- `power X` - Set power (0-100%)
- `light 0|1` - Control light
- `fan status` - Print fan info
- `fan auto|off|speed X` - Fan control
- `recovery` - Test recovery mechanism

## Project Statistics

**Codebase**:
- Firmware: 1100 lines (`src/main.cpp`)
- Configuration: 75 lines (`include/config.h`)
- Documentation: 5000+ lines (12 major docs)
- Tests: 56 tests across 4 suites
- Build Sizes: 319 KB (prod), 331 KB (test)

**Features**:
- ✅ 4-channel RC input (ISR-based)
- ✅ PWM power control (20A typical)
- ✅ Temperature-based fan control
- ✅ Current/voltage monitoring (INA219)
- ✅ Automatic safety recovery (5-second window)
- ✅ Serial diagnostics interface
- ✅ GitHub Actions CI/CD

**Quality**:
- ✅ 56 tests (all passing)
- ✅ Zero compiler warnings
- ✅ Atomic-safe reads
- ✅ Comprehensive documentation
- ✅ Production-ready code

## Key Concepts

### RC Signal Handling
- 4 independent PWM channels captured via GPIO ISRs
- 1000 µs = 0%, 1500 µs = 50%, 2000 µs = 100%
- 100 ms timeout for signal loss detection
- Atomic-safe reads prevent ISR/loop race conditions

### Power Control
- PWM output on GPIO 25 at 16.67 kHz
- 8-bit resolution (0-255 maps to 0-100%)
- Safety checks before each output update
- Current/voltage monitoring with INA219

### Fan Control (v1.2)
- Automatic temperature-based speed control
- Temperature range: 35-50°C maps to 30-255 PWM
- Hysteresis to prevent oscillation
- Manual speed override via serial commands

### Safety System
- Automatic shutdown on overcurrent/voltage faults
- Configurable recovery window (5 seconds default)
- All recovery conditions must pass simultaneously:
  - Current stabilized
  - Voltage in range
  - Timeout elapsed
  - RC signal present

### Testing Strategy
- **Unit Tests**: Mathematical functions in isolation
- **Integration Tests**: Component interactions and data flows
- **Runtime Simulation**: Extended behavior over time
- **Build Sanity**: Compilation and configuration verification

## Documentation Navigation

```
📁 docs/ (You are here)
├── README.md (overview - this file)
├── QUICKSTART.md (start here!)
├── HARDWARE.md
├── EXAMPLES.md
├── BRINGUP.md
├── ARCHITECTURE.md
├── DEVELOPER_GUIDE.md
├── TESTING_STRATEGY.md
├── BUILD_PIPELINE.md
├── HARDWARE_VERIFICATION.md
├── TROUBLESHOOTING.md
└── RELEASING.md

📁 src/
├── README.md (firmware architecture)
└── main.cpp (~1100 lines)

📁 include/
├── README.md (configuration guide)
└── config.h (~75 lines)

📁 scripts/
├── README.md (script documentation)
├── run_all_tests.py (56 tests)
├── build_sanity_tests.py (15 tests)
├── unit_tests.py (16 tests)
├── integration_tests.py (16 tests)
├── runtime_simulator_tests.py (9 tests)
├── hardware_verification.py
└── build_metadata.py
```

## Getting Help

### Problem: Build Fails
→ See [TROUBLESHOOTING.md](TROUBLESHOOTING.md#build-issues)

### Problem: Tests Fail
→ See [BUILD_PIPELINE.md](BUILD_PIPELINE.md) or [TESTING_STRATEGY.md](TESTING_STRATEGY.md)

### Problem: Hardware Not Working
→ See [BRINGUP.md](BRINGUP.md) or [TROUBLESHOOTING.md](TROUBLESHOOTING.md)

### Want to Extend System
→ See [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md)

### Want to Understand Architecture
→ See [ARCHITECTURE.md](ARCHITECTURE.md)

### Need Release/Version Info
→ See [RELEASING.md](RELEASING.md)

## Contributing

1. **Make changes** to firmware or tests
2. **Run tests**: `python scripts/run_all_tests.py`
3. **Verify build**: `pio run -e esp32dev`
4. **Update docs** if behavior changes
5. **Commit with clear message**: `git commit -m "feat: describe change"`
6. **Push to GitHub**: `git push origin main`
7. **GitHub Actions** runs full CI/CD pipeline

## Quick Reference

### Common Commands

```bash
# Build
pio run -e esp32dev

# Test
python scripts/run_all_tests.py

# Monitor Serial
pio device monitor --baud 115200

# Flash to Device
pio run -e esp32dev --target upload

# Hardware Testing
python scripts/hardware_verification.py

# Check Config
grep "#define" include/config.h
```

### Serial Commands

```bash
# System Status
> status
Power: 0% | Temp: 28.3°C | Fan: 30/255 PWM | Current: 0.0A | Voltage: 12.8V

# Set Power
> power 75

# Control Fan
> fan auto
> fan off
> fan speed 150

# Diagnostics
> recovery
> dump
```

## Version History

**v1.2** (Current, 2026-03-07)
- ✅ Fan control with temperature monitoring
- ✅ Comprehensive documentation (12 docs)
- ✅ Expanded test suite (56 tests)
- ✅ Developer guide for extensions

**v1.1.1** (2026-03-07)
- ✅ Automatic 5-second recovery mechanism
- ✅ Atomic-safe RC value reads
- ✅ Build sanity tests (15 tests)
- ✅ Troubleshooting and build pipeline docs

**v1.1** (Earlier)
- ✅ Core RC control, power output
- ✅ Light and auxiliary outputs
- ✅ Current/voltage monitoring
- ✅ Serial diagnostics

## Support

- **GitHub Issues**: Report bugs and request features
- **Documentation**: Comprehensive docs in this directory
- **Tests**: Run `python scripts/run_all_tests.py` to verify
- **Hardware**: Use `hardware_verification.py` for device testing

## License

See [LICENSE](../LICENSE) file

---

**Last Updated**: 2026-03-07  
**Status**: Production Ready (v1.2)  
**Test Coverage**: 56 tests, all passing  
**Build Quality**: Zero warnings
