# Include Directory Documentation

## Overview

The `include/` directory contains header files and configuration constants for the ESP32RC firmware. The primary file is `config.h`, which centralizes all configurable parameters.

## config.h

**Purpose**: Centralized configuration for GPIO pins, thresholds, timing constants, and safety parameters

**Size**: ~75 lines

**Organization**: Organized by functional subsystem

### GPIO Pin Configuration

```c
// RC PWM Inputs (GPIO numbers)
#define RC_CHANNEL_0_PIN 34
#define RC_CHANNEL_1_PIN 35
#define RC_CHANNEL_2_PIN 32
#define RC_CHANNEL_3_PIN 33

// Power Output (PWM) 
#define POWER_PIN 25
#define PWM_POWER_CHANNEL 0

// Light Outputs (Digital)
#define LIGHT_PIN_1 26
#define LIGHT_PIN_2 27

// Auxiliary Output (Digital)
#define AUXILIARY_PIN 14

// Fan Control (PWM)
#define FAN_PIN 12
#define PWM_FAN_CHANNEL 1
```

### PWM Configuration

```c
// Power stage
#define PWM_FREQUENCY_HZ 16667  // 16.67 kHz
#define PWM_RESOLUTION_BITS 8   // 0-255 range

// Fan control
#define FAN_PWM_FREQUENCY_HZ 1000  // 1 kHz
#define FAN_PWM_RESOLUTION_BITS 8  // 0-255 range
```

### RC Input Timing

```c
#define RC_MIN_PULSE_US 1000    // 1000 µs → 0% power
#define RC_MAX_PULSE_US 2000    // 2000 µs → 100% power
#define RC_DEADBAND_US 50       // ±50 µs deadband around center
#define RC_TIMEOUT_MS 100       // Lost signal after 100ms
```

### Safety Thresholds

**Voltage Limits**:
```c
#define SAFETY_VOLTAGE_MIN_V 9.0    // Minimum battery voltage
#define SAFETY_VOLTAGE_MAX_V 26.0   // Maximum battery voltage
```

**Current Limits**:
```c
#define SAFETY_CURRENT_MAX_A 50.0   // Maximum allowable current
#define OVERCURRENT_THRESHOLD_A 48.0  // Triggers shutdown
```

**Temperature Control**:
```c
#define FAN_TEMP_LOW 35.0         // Fan min temp (30% speed)
#define FAN_TEMP_HIGH 50.0        // Fan max temp (100% speed)
#define FAN_SPEED_MIN 30          // Minimum PWM (for circulation)
#define FAN_SPEED_MAX 255         // Maximum PWM (100%)
#define FAN_HYSTERESIS 10         // Speed change hysteresis
```

### Safety Recovery

```c
#define SAFETY_TIMEOUT_MS 5000      // Recovery window (5 seconds)
#define SAFETY_CHECK_INTERVAL_MS 20 // Check every 20ms
#define MONITORING_INTERVAL_MS 1000 // Monitor every 1 second
```

### Current Scaling

```c
#define CURRENT_SCALE_MA_PER_COUNT 2.0  // mA per ADC count
```

This can be calibrated based on INA219 sensor characteristics.

### I2C Configuration

```c
#define INA219_I2C_ADDRESS 0x40  // Default I2C address
#define I2C_SPEED 100000         // 100 kHz I2C bus
```

### Serial Configuration

```c
#define SERIAL_BAUD_RATE 115200  // USB serial baud rate
```

### Build Configuration Flags

```c
// Defined by platformio.ini for specific environments:
// #define ENABLE_SERIAL_SELF_TEST  // Only in esp32dev_selftest build
```

## Modifying Configuration

### Common Changes

**Increasing Current Limit**:
```c
// Before
#define SAFETY_CURRENT_MAX_A 50.0
#define OVERCURRENT_THRESHOLD_A 48.0

// After (for 75A limit)
#define SAFETY_CURRENT_MAX_A 75.0
#define OVERCURRENT_THRESHOLD_A 73.0
```

**Changing Fan Temperature Setpoint**:
```c
// Before
#define FAN_TEMP_LOW 35.0
#define FAN_TEMP_HIGH 50.0

// After (lower fan engagement)
#define FAN_TEMP_LOW 30.0    // Start fan at 30°C
#define FAN_TEMP_HIGH 45.0   // Full speed at 45°C
```

**Adjusting PWM Frequency**:
```c
// Before (16.67 kHz)
#define PWM_FREQUENCY_HZ 16667

// After (5 kHz for larger MOSFETs)
#define PWM_FREQUENCY_HZ 5000
```

**Changing Serial Port Speed**:
```c
// Before
#define SERIAL_BAUD_RATE 115200

// After (for low-speed connections)
#define SERIAL_BAUD_RATE 9600
```

### Testing Changes

After modifying `config.h`:

```bash
# 1. Verify compilation
pio run -e esp32dev

# 2. Run unit tests (verify logic still works)
python scripts/unit_tests.py

# 3. Run integration tests (verify interactions)
python scripts/integration_tests.py

# 4. Run all tests
python scripts/run_all_tests.py
```

## Derived Constants

The following values are calculated or depend on config.h:

| Derived Value | Calculation | Purpose |
|--------------|-----------|---------|
| PWM Steps | (RC_MAX - RC_MIN) / PWM_MAX | RC→Power conversion |
| Fan Interpolation | (TEMP_HIGH - TEMP_LOW) / (SPEED_MAX - SPEED_MIN) | Temp→Fan conversion |
| Current Range | SAFETY_CURRENT_MAX * CURRENT_SCALE | Full measurement range |
| Voltage Range | SAFETY_VOLTAGE_MAX - SAFETY_VOLTAGE_MIN | Full measurement range |

## Hardware Dependencies

Some constants are tied to specific hardware:

| Constant | Hardware | Notes |
|----------|----------|-------|
| RC_CHANNEL_*_PIN | ESP32 GPIO | Can be remapped to other GPIO |
| POWER_PIN | IRF3205 MOSFET | Requires 16Hz+ PWM |
| FAN_PIN | PWM fan header | 1 kHz standard for fans |
| INA219_I2C_ADDRESS | Adafruit INA219 | I2C address jumper configurable |
| SERIAL_BAUD_RATE | USB interface | Limited to 230400 max on USB |

## Safety-Critical Constants

These constants affect system safety and should be modified only after testing:

```c
SAFETY_TIMEOUT_MS       // Recovery delay
SAFETY_CURRENT_MAX_A    // Maximum safe current
OVERCURRENT_THRESHOLD_A // Shutdown trigger
SAFETY_VOLTAGE_MIN_V    // Under-voltage shutdown
SAFETY_VOLTAGE_MAX_V    // Over-voltage shutdown
RC_TIMEOUT_MS           // Signal loss detection
```

**Before Changing Safety Constants**:
1. Review PROJECT_NOTES.md for system limitations
2. Update integration tests with new thresholds
3. Run full test suite: `python scripts/run_all_tests.py`
4. Update TESTING_STRATEGY.md with new limits
5. Test on actual hardware before deployment

## Documentation References

- **Main Firmware**: [src/](../src/README.md)
- **Configuration Details**: [include/config.h](config.h)
- **Developer Guide**: [docs/DEVELOPER_GUIDE.md](../docs/DEVELOPER_GUIDE.md)
- **Hardware Reference**: [docs/HARDWARE.md](../docs/HARDWARE.md)
- **Project Notes**: [PROJECT_NOTES.md](../PROJECT_NOTES.md)

## Compilation

To compile with a custom configuration:

```bash
# Standard build
pio run -e esp32dev

# With verbose output (shows config.h parsing)
pio run -v -e esp32dev

# Self-test build (includes ENABLE_SERIAL_SELF_TEST definition)
pio run -e esp32dev_selftest
```

## Verification

To verify configuration is correct:

```bash
# Extract compiled constants (if debugging info available)
pio run -e esp32dev --verbose

# Check resulting binary size
ls -la .pio/build/esp32dev/firmware.bin
```

---

**Last Updated**: 2026-03-07  
**Version**: 1.2  
**Status**: Production Ready
