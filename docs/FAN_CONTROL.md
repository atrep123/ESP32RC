# Fan Control Feature (v1.2)

## Overview

The ESP32RC firmware now includes integrated fan control capability (v1.2+). This feature automatically manages a PWM-controlled cooling fan based on ESP32 internal temperature readings, with support for both automatic and manual operation modes.

### Key Benefits

- **Thermal Management**: Prevents thermal throttling under sustained high-current loads
- **Efficient Cooling**: Fan only runs when needed (linear interpolation between temp thresholds)
- **Flexible Control**: Both automatic temperature-based and manual PWM modes
- **Serial Command Interface**: Easy control and monitoring via serial terminal
- **Configurable Thresholds**: Adjust fan behavior via `config.h` without recompiling (requires rebuild)

## Hardware Configuration

### Pin Assignments

| Component | GPIO | Purpose |
|-----------|------|---------|
| Fan PWM | GPIO 12 | PWM fan speed control (configurable) |
| PWM Frequency | N/A | 1 kHz (same as power control) |
| PWM Channel | 1 | Uses hardware PWM channel 1 (power uses channel 0) |

### Typical Fan Wiring

```
ESP32 GPIO 12 ──→ MOSFET gate (or PWM fan controller)
                 ↓
            Fan Motor
                 ↓
            GND (ESP32 or external)
```

**Note**: If using a dedicated PWM fan controller/ESC, connect GPIO 12 to its signal input. For direct MOSFET gate drive, ensure adequate current capability and gate voltage compatibility.

## Configuration

All fan settings are defined in `include/config.h` and require recompilation to change:

```c
// Fan control settings (v1.2 feature)
#define FAN_PIN 12                // GPIO 12 for fan PWM control
#define PWM_FAN_CHANNEL 1         // ESP32 PWM channel 1
#define FAN_PWM_FREQUENCY 1000    // 1kHz PWM frequency
#define FAN_ENABLED 1             // Enable fan control (1=yes, 0=no)

// Fan temperature control thresholds
#define FAN_TEMP_LOW 35.0         // Start fan at 35°C
#define FAN_TEMP_HIGH 50.0        // Max fan speed at 50°C
#define FAN_SPEED_MIN 30          // Minimum PWM: ~12% (30/255)
#define FAN_SPEED_MAX 255         // Maximum PWM: 100%
```

### Recommended Settings

**For Indoor/Lab**: Conservative cooling
```c
#define FAN_TEMP_LOW 40.0         // Start at 40°C
#define FAN_TEMP_HIGH 55.0        // Max at 55°C
#define FAN_SPEED_MIN 50          // ~20% minimum
```

**For Outdoor/High-Load**: Aggressive cooling
```c
#define FAN_TEMP_LOW 30.0         // Start at 30°C
#define FAN_TEMP_HIGH 45.0        // Max at 45°C
#define FAN_SPEED_MIN 80          // ~31% minimum
```

**For Fanless/No-Fan**: Disable feature
```c
#define FAN_ENABLED 0             // Disable fan control entirely
```

## Operation

### Temperature Monitoring

The firmware continuously reads the ESP32's internal temperature sensor:

```
Temperature ≤ FAN_TEMP_LOW (35°C)
    ↓
Fan OFF (0% PWM)

Temperature between FAN_TEMP_LOW and FAN_TEMP_HIGH
    ↓
Linear ramp from FAN_SPEED_MIN (30) to FAN_SPEED_MAX (255)
    ↓
Fan adjusts speed proportionally

Temperature ≥ FAN_TEMP_HIGH (50°C)
    ↓
Fan at MAX (255 PWM = 100%)
```

**Example**: At 42.5°C (midpoint between 35°C and 50°C):
```
Speed = 30 + (0.5 × (255 - 30))
      = 30 + 112.5
      ≈ 143 PWM (~56%)
```

### Operation Modes

#### Automatic Mode (Default)

Fan speed is automatically controlled based on temperature. This is the default operating mode.

- Updates every 100ms (MONITOR_INTERVAL)
- Uses linear interpolation between configured thresholds
- Includes hysteresis (10 PWM units) to prevent rapid speed changes
- Recommended for most applications

#### Manual Mode

User-specified PWM value (0-255). Fan runs at fixed speed regardless of temperature.

- Useful for system testing or custom thermal management
- Overrides automatic mode
- Use serial commands: `fan speed <0-255>`

#### Off Mode

Fan disabled (speed 0).

- Useful for thermal stress testing or low-power operation
- Use serial command: `fan off`
- Use config: `#define FAN_ENABLED 0` to disable feature entirely

## Serial Commands

All fan commands are available via serial terminal (115200 baud). Use when `ENABLE_SERIAL_SELF_TEST` is enabled or at any time if serial interface is active.

### Command Syntax

```
fan status           – Display fan status and current temperature
fan auto             – Switch to automatic temperature-based control
fan off              – Turn off fan (PWM = 0)
fan speed <0-255>    – Set manual speed (0=off, 255=max)
```

### Example Session

```
> fan status
--- Fan Status ---
Fan Enabled: YES
Auto Mode: ON
Current Speed: 0 (0-255)
Temperature: 32.5°C
Temp Range: 35.0°C - 50.0°C

> fan speed 128
Fan: speed set to 128

> fan status
--- Fan Status ---
Fan Enabled: YES
Auto Mode: OFF
Current Speed: 128 (0-255)
Temperature: 33.2°C
Temp Range: 35.0°C - 50.0°C

> fan auto
Fan: switched to AUTO mode

> fan status
--- Fan Status ---
Fan Enabled: YES
Auto Mode: ON
Current Speed: 0 (0-255)
Temperature: 33.5°C
Temp Range: 35.0°C - 50.0°C

> fan off
Fan: turned OFF
```

## Implementation Details

### Core Functions

1. **readSystemTemperature()**
   - Reads ESP32 internal temperature sensor
   - Returns temperature in Celsius
   - Accuracy: ±5°C typical
   - Update rate: 100ms (MONITOR_INTERVAL)

2. **calculateFanSpeed(temperature)**
   - Computes required PWM value based on temperature
   - Linear interpolation between FAN_TEMP_LOW and FAN_TEMP_HIGH
   - Returns 0 to 255 PWM value
   - Stateless (no hysteresis applied here)

3. **updateFanControl()**
   - Called every 100ms from main loop
   - Reads temperature
   - Calculates new speed
   - Applies hysteresis (10 PWM threshold)
   - Updates PWM output via `ledcWrite(PWM_FAN_CHANNEL, fan_speed)`

4. **setFanSpeed(speed)**
   - Manual speed setter
   - Accepts 0-255 or -1 (return to auto)
   - Updates fan_auto_mode flag
   - Validates input range

### State Variables

```c
int fan_speed = 0;                    // Current PWM value (0-255)
float last_temp_reading = 25.0;       // Last ESP32 temp (Celsius)
unsigned long last_fan_update = 0;    // Last update timestamp
bool fan_auto_mode = true;            // Auto mode enabled flag
```

### Update Timing

```
Main Loop (every millisecond)
  ├─ updateFanControl()
  │   └─ If (current_time - last_fan_update >= 100ms)
  │       ├─ Read temperature
  │       ├─ Calculate new speed
  │       ├─ Apply hysteresis (only update if Δ > 10 or speed = 0/255)
  │       └─ Update PWM
  │
```

## Thermal Characteristics

### ESP32 Temperature Sensor

- **Location**: Internal to ESP32 die
- **Range**: Typically -10°C to +80°C
- **Accuracy**: ±5°C (non-calibrated)
- **Represents**: Die temperature (not ambient)
- **Resolution**: ~0.1°C

### Calibration Notes

The ESP32 temperature sensor is **not factory-calibrated** for accuracy. Expected behavior:

- Starts at ~25°C at room temperature (nominal)
- Reads accurately relative to changes (ΔT is reliable)
- Absolute readings may be off by ±5°C
- For critical applications, use external temperature sensor (future feature)

### Typical Temperature Profile

| System State | Die Temperature |
|--------------|-----------------|
| Idle (no load) | 30-35°C |
| Light load | 35-40°C |
| Medium load (20A) | 40-50°C |
| Heavy load (40A) | 50-60°C |
| Sustained 50A | 60-70°C |

**Note**: Ambient temperature affects die temperature. The ranges above assume ~25°C ambient in a lab setting.

## Troubleshooting

### Fan Not Spinning

**Symptom**: `fan status` shows non-zero speed, but fan doesn't spin

**Causes & Solutions**:
- Pin wiring incorrect: Verify GPIO 12 connects to fan controller
- PWM channel conflict: Check that PWM_FAN_CHANNEL 1 is free (power uses channel 0)
- Insufficient power: Fan motor may need external supply (ESP32 GPIO can't supply much current)
- Speed threshold: If FAN_SPEED_MIN > 0, fan only starts spinning at calculated speed ≥ FAN_SPEED_MIN

**Solution**:
```c
// Increase minimum speed if fan has high starting torque
#define FAN_SPEED_MIN 80   // ~31% instead of 30

// Or use manual mode to test at 100%
> fan speed 255
```

### Temperature Reading Stuck

**Symptom**: `fan status` shows same temperature for extended time

**Causes**:
- Temperature sensor not updating (firmware issue)
- No system load (temperature stable at idle)
- Internal sensor timeout

**Solution**: Check if temperature changes with load (generate 30A+ current)

### Fan Won't Shut Off

**Symptom**: Fan runs even when temperature is cool

**Causes**:
- Manual mode active (not auto): Check with `fan status`
- FAN_TEMP_LOW set too low: If set to 0°C, fan never turns off
- Speed calculation bug: Rare; clear and rebuild

**Solutions**:
```c
> fan auto                     // Switch to auto mode
> fan status                   // Verify "Auto Mode: ON"

// Or rebuild with higher thresholds:
#define FAN_TEMP_LOW 35.0      // Ensure above minimum system temp
```

### Fan Speed Jerky/Oscillates

**Symptom**: Fan speed changes rapidly or oscillates at same temperature

**Causes**:
- Hysteresis too small: Sensitive to sensor noise
- Temperature oscillating: Normal at boundary conditions
- Update rate too fast: 100ms is standard

**Solution**: Increase hysteresis in code (currently 10 PWM units):
```c
// In updateFanControl(), modify:
if (abs(new_fan_speed - fan_speed) > 20) {  // Increase from 10 to 20
    fan_speed = new_fan_speed;               // Less responsive but smoother
    ...
}
```

## Performance Metrics

### Build Size Impact

| Metric | Before v1.2 | After v1.2 | Delta |
|--------|------------|-----------|-------|
| Flash (esp32dev) | 319 KB | 319 KB | +0 KB* |
| Flash (selftest) | 330 KB | 331 KB | +1 KB |
| RAM usage | 6.9% | 6.9% | No change |

*Compiled with `-Os` (optimize for size). Exact values depend on compiler version.

### Runtime Performance

- **Temperature read**: < 1 ms
- **Speed calculation**: < 1 ms (linear math only)
- **PWM update**: << 1 ms (hardware operation)
- **Total updateFanControl()**: ~1-2% CPU (runs every 100ms for ~1ms)

### Thermal Response

| Scenario | Time to Max Speed |
|----------|------------------|
| Load changes 0A → 50A | ~5 seconds (for 35→50°C) |
| Load removes 50A → 0A | ~10 seconds (for 50→35°C) |
| Manual mode set to 255 | Immediate (< 1ms) |

## Integration with Other Features

### Compatibility

- ✅ **RC Input**: Fully compatible, independent operation
- ✅ **Power Control**: Independent PWM channels (power=0, fan=1)
- ✅ **Light Control**: No conflict
- ✅ **Auxiliary Output**: No conflict
- ✅ **Current/Voltage Monitoring**: Fan doesn't interfere
- ✅ **Safety System**: Fan can keep system cool during recovery
- ✅ **Serial Self-Test**: Full Command support

### Power Management

Fan PWM uses ESP32 hardware PWM channel 1, while power control uses channel 0:
- No channel conflicts
- Up to 4 independent PWM channels available on ESP32
- Future feature: PWM for lights (instead of ON/OFF digital)

## Future Enhancements

### Potential Improvements (v1.3+)

1. **Configurable Update Rate**: Make MONITOR_INTERVAL settable via serial
2. **Alternative Sensors**: Support external temperature sensors (I2C thermal sensor)
3. **Fan Curve Profiles**: Preset behaviors (quiet, balanced, performance)
4. **Speed Ramping**: Smooth acceleration/deceleration instead of step changes
5. **Persistent Settings**: Save temp thresholds to NVS Preferences
6. **Multi-Fan Support**: Control 2+ fans independently
7. **Load-Based Control**: Adjust fan based on current draw instead of (or with) temperature
8. **PWM for Lights**: Replace digital light outputs with PWM for brightness control

## External Temperature Sensor (v1.3)

### Overview

The internal ESP32 temperature sensor has ±5°C accuracy, which is insufficient for precise thermal management. Version 1.3 adds support for the **DS18B20 OneWire temperature sensor**, providing ±0.5°C precision.

### Hardware

**DS18B20 Specifications**:
| Parameter | Value |
|-----------|-------|
| Communication | OneWire (Dallas 1-Wire protocol) |
| Temperature Range | -55°C to +125°C |
| Accuracy | ±0.5°C (typical), ±2°C (max) |
| Precision | 12-bit (0.0625°C per LSB) |
| Conversion Time | 750ms @ 12-bit resolution |
| Package | TO-92 or waterproof probe |

### Wiring

```
    ESP32 GPIO 19 (configurable TEMP_SENSOR_PIN)
           │
           ├─[4.7kΩ][PULL-UP]─┐
           │                   │
    ┌──────┴───────────────────┤ DS18B20 DQ pin
    │                          │
   [+3.3V]                    GND

Alternatively (waterproof probe):
    GND ├─ Red wire (Vcc, 3.3V)
        ├─ Black wire (GND)
        └─ Yellow wire (DQ, digital output → GPIO 19)
```

### Configuration

Add to `config.h`:

```c
// External Temperature Sensor (DS18B20) - v1.3 feature
#define TEMP_SENSOR_PIN 19             // GPIO 19 for OneWire (configurable)
#define TEMP_SENSOR_ENABLED 1          // Enable external temperature sensor
#define TEMP_SENSOR_PRECISION 12       // 12-bit precision (0.0625°C resolution)
#define TEMP_SENSOR_REQUEST_TIMEOUT 1000  // 1 second timeout for sensor read
#define TEMP_SENSOR_USE_EXTERNAL 1     // Use external sensor when available
```

### Features

✅ **Automatic Fallback**: If sensor unavailable, automatically uses internal ESP32 sensor  
✅ **Error Detection**: Validates readings (DS18B20 returns -127.00 on error)  
✅ **High Precision**: 12-bit resolution (0.0625°C per step) vs internal sensor ±5°C  
✅ **Fast Response**: 750ms conversion time at 12-bit  
✅ **Low Power**: Passive reading, no extra current draw  

### Serial Interface

```
> fan status
--- Fan Status ---
Fan Enabled: YES
Auto Mode: ON
Current Speed: 0 (0-255)
Temperature: 42.5°C        ← Now reads from DS18B20!
Temp Range: 35°C - 50°C
```

**Behavior**:
- If DS18B20 reads successfully → uses external value
- If DS18B20 fails to read → falls back to internal sensor
- Fallback logged to serial for diagnostics

### Temperature Range Mapping

With DS18B20 external sensor:
```
25°C (room temp)      → Fan OFF (inactive)
35°C (FAN_TEMP_LOW)   → Fan 30% (minimum circulation)
40°C (midpoint)       → Fan ~50% (moderate cooling)
45°C (high load)      → Fan ~90% (active cooling)
50°C (FAN_TEMP_HIGH)  → Fan 100% (maximum cooling)
55°C+                → Fan 100% (continuous full speed)
```

### Advantages over Internal Sensor

| Feature | Internal ESP32 | DS18B20 (v1.3) |
|---------|---|---|
| Accuracy | ±5°C | ±0.5°C |
| Precision | ~1°C | 0.0625°C |
| Sensing Location | CPU die (hot spot) | External probe (more stable) |
| Requires Calibration | Yes | No (factory calibrated) |
| Response Time | Immediate | 750ms @ 12-bit |
| Immune to Noise | No | Yes (robust protocol) |

### Troubleshooting

**Sensor Not Found**:
```
WARNING: Temperature sensor not found - using internal ESP32 sensor
```
- Check GPIO 19 pin is correct
- Verify 4.7kΩ pull-up resistor
- Check DS18B20 DQ pin connection
- Can use pins 16-23 (most GPIO pins support OneWire)

**Erratic Readings**:
- Increase pull-up resistor to 10kΩ if wire run > 10 meters
- Check for electrical noise near OneWire line
- Reduce TEMP_SENSOR_REQUEST_TIMEOUT if needed
- Verify 3.3V supply is stable

**Always Reads Room Temperature**:
- May be external mounting issue (insufficient thermal coupling)
- Use thermal tape or housing to improve contact
- Ensure DS18B20 is close to heat source

### Test Coverage

**Unit Tests (v1.3)**:
- `test_ds18b20_temperature_range_validation()` - ±55 to 125°C range checks
- `test_ds18b20_precision_levels()` - 12-bit precision verification
- `test_external_temp_sensor_fallback()` - Fallback logic validation

**Integration Tests**:
- Sensor success path (external reading used)
- Sensor failure path (internal fallback)
- Temperature→fan speed mapping with higher precision

---

## References

- [ESP32 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf) - Internal temperature sensor details
- [Arduino ESP32 API](https://docs.espressif.com/projects/arduino-esp32/en/latest/) - `temperatureRead()` function
- [main.cpp](../src/main.cpp) - Fan control implementation (lines 88-1040)
- [config.h](../include/config.h) - Fan configuration constants

## Version History

- **v1.2** (2026-03-07):
  - Initial release with automatic temperature-based control
  - Serial command interface
  - Manual speed override
  - 3-point test coverage (config, functions, commands)

---

**Last Updated**: 2026-03-07  
**Status**: Stable  
**Feature Name**: Fan Control (v1.2)
