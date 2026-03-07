# Firmware Source Documentation

## Overview

The `src/` directory contains the main firmware implementation for ESP32RC. The primary source file is `main.cpp`, which implements the complete control system including RC input capture, power management, safety features, and fan control.

## main.cpp

**Purpose**: Complete ESP32RC firmware implementation

**Size**: ~1100 lines (v1.2)

**Compilation Targets**:
- `esp32dev`: Production firmware (319 KB)
- `esp32dev_selftest`: Self-test variant (331 KB)

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│              setup() - Initialization               │
├──────────────────────────────────────────────────────┤
│ 1. Serial initialization (115200 baud)             │
│ 2. GPIO configuration (pins, digital outputs)      │
│ 3. PWM setup (power 16.67kHz, fan 1kHz)            │
│ 4. INA219 sensor initialization                    │
│ 5. RC interrupt attachment (ISR handlers)          │
│ 6. NVS preferences loading (calibration)           │
│ 7. System state reset                              │
└──────────────────────────────────────────────────────┘
        ↓ (startup complete)
┌──────────────────────────────────────────────────────┐
│    loop() - Main Control Loop (~50 Hz effective)    │
├──────────────────────────────────────────────────────┤
│ 1. Read RC values (safe atomic read)               │
│ 2. Check RC signal validity (timeout detection)    │
│ 3. Map RC channels to outputs                      │
│ 4. Output control (power, lights, auxiliary)       │
│ 5. Every 20ms: Safety checks (current, voltage)    │
│ 6. Every 1Hz:  Monitoring/diagnostics              │
│ 7. Every 1Hz:  Fan control update                  │
│ 8. Serial command processing (non-blocking)        │
└──────────────────────────────────────────────────────┘
```

## Key Subsystems

### 1. RC Input Capture (ISR-based)

**Lines**: ~150-250

**Key Functions**:
- `void IRAM_ATTR handleRCInterrupt*()` - 4 ISR handlers (GPIO 34, 35, 32, 33)
- `uint16_t readRCValue(uint8_t channel)` - Atomic-safe read
- `uint32_t readRCUpdateTime(uint8_t channel)` - Timestamp read
- `bool readRCChannelSeen(uint8_t channel)` - Signal presence check

**State**:
```cpp
volatile uint16_t rc_values[4] = {0, 0, 0, 0};      // Pulse widths (µs)
volatile uint32_t rc_times[4] = {0, 0, 0, 0};       // Last update time
volatile uint8_t rc_channel_seen[4] = {0, 0, 0, 0}; // Signal valid flag
volatile uint32_t last_isr_time = 0;                // ISR throttling
```

**How It Works**:
1. GPIO generates interrupt on rising/falling edge
2. ISR measures pulse width via `micros()`
3. Store in volatile variable (atomic-safe for 16-bit values on 32-bit CPU)
4. Main loop reads via atomic helper to prevent torn reads
5. 50 Hz update rate (20ms between updates typical)

**Safety**:
- ISR handler must be < 100 µs (uses `IRAM_ATTR` for speed)
- Volatile prevents compiler optimization issues
- Atomic read helpers guarantee 16-bit consistency

### 2. Power Control System

**Lines**: ~280-380

**Key Functions**:
- `void setupPowerControl()` - GPIO/PWM initialization
- `void setPowerPercentage(uint16_t percentage)` - Apply power (0-100%)
- `uint16_t mapRCToPower(uint16_t rcValue)` - Convert RC pulse to %

**State**:
```cpp
uint16_t current_power_percentage = 0;  // 0-100%
uint16_t target_power_percentage = 0;   // RC-commanded
uint8_t power_pwm_value = 0;            // 0-255 PWM
```

**Control Flow**:
```
RC Pulse (1000-2000 µs)
        ↓
Map to 0-100% (with deadband)
        ↓
Apply safety checks (overcurrent, voltage, RC valid)
        ↓
Convert to PWM (0-255)
        ↓
Write to GPIO 25 (16.67 kHz)
```

**PWM Conversion**:
- RC 1000 µs → 0% → PWM 0
- RC 1500 µs → 50% → PWM 128
- RC 2000 µs → 100% → PWM 255

### 3. Light Control

**Lines**: ~390-450

**Key Functions**:
- `void setupLightOutputs()` - GPIO initialization
- `void setLight(uint8_t light_num, uint8_t state)` - ON/OFF control
- `bool checkLightThreshold(uint16_t rcValue)` - Hysteresis logic

**Outputs**:
- GPIO 26: Light 1 (controlled by RC Channel 2)
- GPIO 27: Light 2 (controlled by RC Channel 3)

**Behavior**:
- RC Value < 1450 µs → Light OFF
- RC Value > 1550 µs → Light ON
- 100 µs hysteresis prevents flicker

### 4. Auxiliary Output

**Lines**: ~460-500

**Key Function**:
- `void setAuxiliary(uint8_t state)` - ON/OFF control

**Output**:
- GPIO 14: Auxiliary output (controlled by RC Channel 4)

**Behavior**:
- RC Value < 1450 µs → Aux OFF
- RC Value > 1550 µs → Aux ON

### 5. Fan Control (v1.2)

**Lines**: ~560-650

**Key Functions**:
- `float readSystemTemperature()` - Read ESP32 internal temp
- `uint8_t calculateFanSpeed(float temp)` - Compute PWM (30-255)
- `void updateFanControl()` - Periodic update (~1 Hz)
- `void setFanSpeed(int16_t speed)` - Manual override (-1 = auto)

**State**:
```cpp
float current_temperature = 0.0;        // °C
int16_t fan_speed_manual = -1;          // -1 = auto mode
uint8_t fan_speed_pwm = 30;             // 30-255 (30-100%)
uint32_t last_fan_update = 0;           // 1 Hz throttling
uint8_t last_pwm_value = 30;            // For hysteresis
```

**Temperature to Speed Mapping**:
```
Temp ≤ 35°C  → Speed = 30% (minimum circulation)
35-50°C      → Speed = linear interpolation
Temp > 50°C  → Speed = 100% (maximum cooling)
```

**Hysteresis Logic**:
- Prevents PWM oscillation near setpoint
- Only change speed if ΔSpeed > 10 PWM units
- Smooth fan response without flutter

### 6. Current & Voltage Monitoring

**Lines**: ~700-800

**Key Functions**:
- `void readCurrentVoltage()` - Read INA219 via I2C
- `bool checkSafetyLimits()` - Master safety check
- `bool isCurrentValid()` - Current range check
- `bool isVoltageValid()` - Voltage range check

**State**:
```cpp
float measured_current_a = 0.0;         // A
float measured_voltage_v = 0.0;         // V
uint32_t last_current_read_time = 0;    // 1 Hz monitoring
```

**Safety Thresholds** (from config.h):
- Voltage: 9V - 26V (overcurrent/undervoltage shutdown)
- Current: 0A - 50A (overcurrent shutdown at 48A)
- Monitoring interval: 1 Hz (1000 ms)

**I2C Protocol**:
```
Address: 0x40 (default)
Speed: 100 kHz
Read current: 2-byte register at 0x01
Read voltage: 2-byte register at 0x02
```

### 7. Safety System & Recovery

**Lines**: ~850-950

**Key Functions**:
- `void emergencyShutdown()` - Immediate power cutoff
- `void resetSystemState()` - Full state reset
- `bool checkSafetyLimits()` - Integrated safety check
- `bool isPowerSignalValid()` - RC signal validity

**State Machine**:
```
ARMED (normal operation)
        ↓
Safety check fails (overcurrent, voltage, etc)
        ↓
emergencyShutdown() → Set fault flag, disable power
        ↓
Wait 5 seconds (SAFETY_TIMEOUT_MS)
        ↓
Check all conditions (current + voltage + timeout)
        ↓
All pass? → resetSystemState() → ARMED
        ↓
Any fail? → Stay SHUTDOWN (stay disabled)
```

**Recovery Mechanism** (v1.1.1+):
- Automatic restart after 5-second fault window
- Requires ALL conditions to be safe:
  - Current < threshold
  - Voltage in range
  - 5 seconds elapsed
  - RC signal present

**Locking Mechanism**:
```cpp
uint32_t recovery_allowed_time = 0;  // When recovery can try
bool system_armed = false;           // Current safety state
bool fault_detected = false;         // Recent fault flag
```

### 8. Serial Interface

**Lines**: ~1000-1100

**Key Functions**:
- `void setupSerial()` - Serial initialization
- `void printStatus()` - Print system state
- `void processSerialCommand(String cmd)` - Command parser
- `void monitoringLoop()` - Periodic diagnostics

**Baud Rate**: 115200 (8N1)

**Serial Commands**:
```
status        → Print current state (power%, temp, current, voltage)
power X       → Set power percentage (0-100)
light 0|1     → Turn light off/on
fan status    → Print fan info
fan auto      → Enable automatic temperature control
fan off       → Turn off fan
fan speed X   → Set manual speed (30-255)
recovery      → Manually trigger recovery test
dump          → Debug information dump
```

**Output Format**:
```
[TIME_MS] Power: 75% | Temp: 42.3°C | Fan: 156/255 PWM
[TIME_MS] Current: 22.5A | Voltage: 12.8V | RC: ARMED
```

## State Variables Summary

### System State
- `bool system_armed` - Safety system armed/disarmed
- `bool fault_detected` - Recent fault flag
- `uint32_t recovery_allowed_time` - When recovery can attempt

### Power State
- `uint16_t current_power_percentage` - Current output power (0-100%)
- `uint16_t target_power_percentage` - RC-commanded power
- `uint8_t power_pwm_value` - PWM register value (0-255)

### RC State
- `volatile uint16_t rc_values[4]` - Pulse widths (ISR updated)
- `volatile uint32_t rc_times[4]` - Last update timestamps
- `volatile uint8_t rc_channel_seen[4]` - Signal presence

### Monitoring State
- `float measured_current_a` - Current sensor reading
- `float measured_voltage_v` - Voltage sensor reading
- `float current_temperature` - ESP32 internal temp

### Fan State
- `uint8_t fan_speed_pwm` - Current PWM (30-255)
- `int16_t fan_speed_manual` - Manual override (-1 = auto)
- `uint32_t last_fan_update` - Last 1 Hz update time

### Light/Aux State
- `uint8_t light_state[2]` - Light 1/2 ON/OFF
- `uint8_t aux_state` - Auxiliary output ON/OFF

## Execution Timing

**ISR Handlers** (GPIO interrupt):
- Execution time: ~20 µs
- Called at 50 Hz per channel (~20 ms period)
- Disabled during Sensor reads (INA219 I2C)

**Main Loop**:
- Target rate: 50 Hz effective (20 ms nominal)
- RC read: 1 µs
- Power control: 2 µs
- Safety check (every 20ms): 50 µs
- Monitoring (every 1000ms): 100 µs (I2C dominated)
- Fan update (every 1000ms): 5 µs

**Total Loop Time**: ~150 µs typical, 100+ ms if I2C read scheduled

## Memory Usage

```
Stack:        ~10 KB (loop + ISR)
Globals:      ~5 KB (state variables, buffers)
Heap:         ~5 KB (dynamic strings)
Free RAM:     ~300 KB (ESP32 has 320 KB total)
```

## Performance Notes

1. **ISR Latency**: Each RC channel uses one ISR, ~20 µs execution
2. **I2C Blocking**: Current/voltage reads block for ~1 ms
3. **Serial Output**: Each print is non-blocking (buffered)
4. **RC Update Rate**: 50 Hz typical (20 ms), varies with receiver

## Debugging

### Adding Diagnostic Output

```cpp
// Within main loop or specific function:
if (debug_enabled) {
    Serial.printf("DEBUG: rc_values[0] = %d\n", rc_values[0]);
}

// Toggle via command:
// > debug on
// > debug off
```

### Monitoring in Real-Time

```bash
# Serial monitor at 115200 baud
pio device monitor --baud 115200

# Or use hardware_verification.py for interactive testing
python scripts/hardware_verification.py
```

## Building & Deployment

### Compilation

```bash
# Production build
pio run -e esp32dev

# Self-test build (includes serial diagnostics)
pio run -e esp32dev_selftest

# Verbose output (see actual compiler warnings/errors)
pio run -v -e esp32dev
```

### Binary Info

```bash
# Check compiled size
ls -lh .pio/build/esp32dev/firmware.bin

# Show sections
esp32-objdump -h .pio/build/esp32dev/firmware.elf | grep -E "\.text|\.data"
```

## Testing

### Unit Tests

```bash
# Test individual functions without hardware
python scripts/unit_tests.py

# Test specific function:
# test_rc_pulse_to_percentage()
# test_fan_temperature_mapping()
# test_overcurrent_threshold()
```

### Integration Tests

```bash
# Test component interactions
python scripts/integration_tests.py

# Examples:
# test_rc_signal_to_power_control_direct()
# test_overcurrent_triggers_shutdown()
# test_automatic_recovery_after_timeout()
```

### Hardware Verification

```bash
# After flashing to device
python scripts/hardware_verification.py

# Select test scenarios:
# 1. Power control test
# 2. RC input test
# 3. Current measurement
# 4. Voltage monitoring
# 5. Temperature sensor
# 6. Fan control
# 7. Light control
# 8. Full system test
```

## Future Enhancements

Planned extensions (v1.3+):

- [ ] Over-the-air (OTA) firmware updates
- [ ] SD card telemetry logging
- [ ] Wireless control (WiFi/Bluetooth)
- [ ] Advanced load modulation
- [ ] Better temperature sensing (external sensor)
- [ ] Energy profiling and efficiency metrics

## References

- **Configuration**: [include/README.md](../include/README.md)
- **Hardware Wiring**: [docs/HARDWARE.md](../docs/HARDWARE.md)
- **Developer Guide**: [docs/DEVELOPER_GUIDE.md](../docs/DEVELOPER_GUIDE.md)
- **Architecture Diagrams**: [docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md)

---

**Last Updated**: 2026-03-07  
**Version**: 1.2  
**Total Lines**: ~1100  
**Build Status**: ✅ Both environments compile
