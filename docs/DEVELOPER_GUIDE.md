# ESP32RC Developer Guide - Extending the System

## Overview

This guide helps developers add new features, modify existing functionality, and maintain the ESP32RC codebase with full consideration for architecture, testing, and documentation.

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│              src/main.cpp (1100 lines)              │
│         Core Firmware Implementation                │
├─────────────────────────────────────────────────────┤
│ ┌──────────────┐  ┌──────────────┐ ┌─────────────┐ │
│ │   RC Input   │  │   Power      │ │ Light & Fan │ │
│ │   Capture    │  │   Control    │ │  Control    │ │
│ ├──────────────┤  ├──────────────┤ ├─────────────┤ │
│ │ 4-channel    │  │ PWM GPIO 25  │ │ GPIO 26/27  │ │
│ │ ISR-based    │  │ 16.67 kHz    │ │ GPIO 12 (F) │ │
│ │ 50 Hz update │  │ 0-255 range  │ │ 1 kHz       │ │
│ └──────────────┘  └──────────────┘ └─────────────┘ │
│                                                     │
│ ┌──────────────┐  ┌──────────────┐ ┌─────────────┐ │
│ │   Safety     │  │  Monitoring  │ │  Recovery   │ │
│ │   System     │  │   & Serial   │ │  Mechanism  │ │
│ ├──────────────┤  ├──────────────┤ ├─────────────┤ │
│ │ Voltage      │  │ INA219 I2C   │ │ 5-sec delay │ │
│ │ Current      │  │ 1 Hz monitor │ │ Auto state  │ │
│ │ Temperature  │  │ Serial UI    │ │ Reset       │ │
│ └──────────────┘  └──────────────┘ └─────────────┘ │
├─────────────────────────────────────────────────────┤
│        include/config.h (75 lines)                  │
│      All Constants & Configuration                  │
├─────────────────────────────────────────────────────┤
│  Atomic Helpers + ISR + Memory Management           │
└─────────────────────────────────────────────────────┘
```

## Key Components

### 1. RC Input System

**Files**: `src/main.cpp` (lines ~50-150)

**Key Functions**:
- `void attachRCInterrupts()` - Setup ISRs for 4 PWM channels
- `void IRAM_ATTR handleRCInterrupt*()` - Fast ISR handlers
- `uint16_t readRCValue(uint8_t channel)` - Atomic read wrapper
- `uint32_t readRCUpdateTime(uint8_t channel)` - Atomic timestamp read

**Architecture**:
- ISR-based capture with `micros()` timing
- Volatile global state for RC data
- Atomic helpers prevent torn reads during ISR updates
- 50 Hz update rate (20ms between updates)

**Extending RC**:
```cpp
// To add 5th RC channel:
1. Add #define RC_CHANNEL_5_PIN in config.h
2. Add volatile variables in setup(): rc_values[5], rc_times[5]
3. Add ISR handler (copy pattern from Ch0-3)
4. Call attachInterrupt in attachRCInterrupts()
5. Add unit test for ch5 mapping
6. Add integration test for ch5 failsafe
```

### 2. Power Control System

**Files**: `src/main.cpp` (lines ~200-350)

**Key Functions**:
- `void setupPowerControl()` - Initialize PWM output (GPIO 25)
- `void setPowerPercentage(uint16_t percentage)` - Apply power level
- `uint16_t mapRCToPower(uint16_t rcValue)` - Transform RC to power
- `bool isRCSignalValid(uint8_t channel)` - Check timeout

**Control Flow**:
```
RC Channel (1000-2000 µs)
        ↓
    Timeout Check (1000 µs min or 100 ms since last)
        ↓
    Map to 0-100 % with deadband
        ↓
    Apply safety checks (voltage, current, temp)
        ↓
    Convert to PWM (0-255 on GPIO 25)
```

**Extending Power**:
```cpp
// To add ramp limiting (smoother acceleration):
1. Add #define POWER_RAMP_RATE_MS in config.h
2. Create static uint32_t last_power_update = 0
3. In setPowerPercentage(), check elapsed time
4. Limit power change: new_pwr = current + clamp(target, RAMP_RATE)
5. Test with integration test: "test_power_ramp_limiting()"
```

### 3. Safety System

**Files**: `src/main.cpp` (lines ~500-700)

**Key Functions**:
- `bool checkSafetyLimits()` - Master safety check
- `bool isCurrentValid()` - INA219 current range
- `bool isVoltageValid()` - Over/under voltage detection
- `bool isPowerSignalValid()` - RC signal presence
- `void emergencyShutdown()` - Immediate power cutoff
- `void resetSystemState()` - Recovery mechanism

**Safety State Machine**:
```
ARMED (normal) ──────────┐
       ↓                  │
Fault Detected           │
(over-I, under-V, etc)   │
       ↓                  │
SHUTDOWN ────→ Wait 5 sec ─┘
        (recovery timeout)
       ↓
Conditions OK?
  Yes → ARMED
  No  → Stay SHUTDOWN
```

**Extending Safety**:
```cpp
// To add temperature cutoff:
1. Add #define TEMP_MAX_CELSIUS in config.h
2. In checkSafetyLimits(), add:
   if (readSystemTemperature() > TEMP_MAX_CELSIUS)
       emergencyShutdown();
3. Add unit test for temp threshold
4. Add integration test for temp recovery
5. Update FAN_CONTROL.md documentation
```

### 4. Fan Control System (v1.2)

**Files**: `src/main.cpp` (lines ~800-900)

**Key Functions**:
- `float readSystemTemperature()` - ESP32 internal sensor
- `uint8_t calculateFanSpeed(float temp)` - Linear interpolation
- `void updateFanControl()` - Periodic update loop
- `void setFanSpeed(int16_t speed)` - Manual override

**Fan Algorithm**:
```
System Temperature (°C)
       ↓
35°C or below → Speed = 30 (30%)
35-50°C       → Speed = linear interpolation
50°C or above → Speed = 255 (100%)
       ↓
Apply 10-point hysteresis (prevents flutter)
       ↓
PWM Output (GPIO 12, channel 1, 1 kHz)
```

**Extending Fan**:
```cpp
// To add PID-based temperature control:
1. Create PID state in main.cpp:
   static float pid_integral = 0;
   static float pid_last_error = 0;
2. Replace calculateFanSpeed() with PID calculation
3. Update config.h with KP, KI, KD constants
4. Add unit test: "test_pid_temperature_response()"
5. Add runtime test: "test_fan_stability_with_load_change()"
```

### 5. Monitoring & Serial Interface

**Files**: `src/main.cpp` (lines ~950-1050)

**Key Functions**:
- `void monitoringLoop()` - 1 Hz monitoring cycle
- `void printStatus()` - Serial status output
- `void processSerialCommand(String cmd)` - Command parser
- `void readCurrentVoltage()` - INA219 I2C read

**Serial Commands**:
```
Commands:
  status        - Print current state
  power X       - Set power percentage (0-100)
  light 0|1     - Turn light on/off
  fan status    - Print fan info
  fan auto      - Enable auto mode
  fan off       - Turn off fan
  fan speed X   - Manual speed (30-255)
  recovery      - Trigger recovery test
  dump          - Debug dump

Example: "power 75" → Sets power to 75%
```

**Extending Serial**:
```cpp
// To add telemetry logging:
1. Create command: "telemetry start"
2. In processSerialCommand(), add:
   else if (cmd == "telemetry start")
       telemetry_enabled = true;
3. In monitoringLoop(), if telemetry_enabled:
       log_telemetry_point(millis(), power, current, speed);
4. Create "telemetry dump" command to print log
5. Test with integration test
```

## Development Workflow

### 1. Planning a Feature

Before coding:

```
1. Create issue on GitHub (title, description, acceptance criteria)
2. Update PROJECT_NOTES.md with planning notes
3. Sketch architecture in memory/session/dev_notes.md
4. Identify affected components (RC, power, safety, fan, monitor)
5. Design test strategy (unit, integration, runtime tests)
```

### 2. Implementation Checklist

For each feature:

```
Code:
  [ ] Update include/config.h - Add constants
  [ ] Implement in src/main.cpp - Core logic
  [ ] Add unit tests - Mathematical correctness
  [ ] Add integration tests - Component interaction
  [ ] Add runtime tests - System behavior
  [ ] Verify both builds compile cleanly
  [ ] Run full test suite (56 tests)

Documentation:
  [ ] Add/update docs/*.md (relevant architecture file)
  [ ] Update README.md if user-facing
  [ ] Update TROUBLESHOOTING.md if adding diagnostics
  [ ] Add examples to EXAMPLES.md if applicable

Verification:
  [ ] All 56 tests pass locally
  [ ] GitHub Actions CI passes
  [ ] Manual hardware test (if hardware available)
  [ ] Update CHANGELOG.md with feature description
```

### 3. Code Style

**C++ Style**:
```cpp
// Names: snake_case for functions, UPPER_CASE for constants
#define SAFETY_TIMEOUT_MS 5000
void process_safety_state() { }

// Variables: descriptive, indicate type/units
uint16_t power_percentage;  // 0-100
uint32_t last_update_time_ms;
bool is_safety_armed;

// Comments: Explain Why, not What
// Recovery disabled for 5s to allow current to stabilize
recovery_allowed_time = millis() + SAFETY_TIMEOUT_MS;

// Spacing: 4 spaces, consistent indentation
for (uint8_t i = 0; i < 4; i++) {
    rc_values[i] = read_value(i);
}
```

**Python Test Style**:
```python
def test_feature_behavior_description(self):
    """Descriptive docstring of test purpose"""
    # Arrange - Setup test data
    input_value = 42
    expected = 84
    
    # Act - Execute function
    result = function_under_test(input_value)
    
    # Assert - Verify result
    if result != expected:
        return False
    
    return True
```

### 4. Branching Strategy

```bash
# Feature
git checkout -b feature/fan-control
# Work and commit
git commit -m "feat: Add PWM fan control with temperature monitoring"

# Bug fix
git checkout -b fix/rc-timeout-not-working
# Work and commit
git commit -m "fix: Increase RC timeout detection window"

# Documentation
git checkout -b docs/update-readme
# Work and commit
git commit -m "docs: Add installation instructions"
```

## Testing Your Changes

### Before Commit

```bash
# 1. Run unit tests (verification of logic)
python scripts/unit_tests.py

# 2. Run integration tests (verification of flows)
python scripts/integration_tests.py

# 3. Run runtime tests (verification of behavior)
python scripts/runtime_simulator_tests.py

# 4. Run full suite
python scripts/run_all_tests.py

# 5. Verify both firmware builds
pio run -e esp32dev
pio run -e esp32dev_selftest

# 6. Check for syntax issues
python -m py_compile scripts/*.py
```

### Adding New Tests

**Unit Test Template**:
```python
def test_my_feature_behavior(self):
    """Test that my feature does X when Y"""
    # Test mathematical correctness
    input_data = ...
    expected = ...
    result = calculate_something(input_data)
    
    if result != expected:
        print(f"FAIL: Expected {expected}, got {result}")
        return False
    
    return True
```

**Integration Test Template**:
```python
def test_my_feature_system_interaction(self):
    """Test that my feature integrates correctly with X"""
    # Test component interaction
    system_state = {...}
    trigger_event(system_state)
    new_state = get_system_state()
    
    if not verify_expected_transition(system_state, new_state):
        print("FAIL: State transition incorrect")
        return False
    
    return True
```

**Runtime Test Template**:
```python
def test_my_feature_over_time(self):
    """Test that my feature behaves correctly over X seconds"""
    # Simulate extended operation
    system_state = {...}
    
    for second in range(10):
        update_system(system_state, second)
        if check_for_violations(system_state):
            print(f"FAIL: Violation at {second}s")
            return False
    
    return True
```

## Common Development Tasks

### Adding a New GPIO Output

1. **Define in config.h**:
   ```cpp
   #define MY_OUTPUT_PIN 2  // GPIO 2
   #define MY_OUTPUT_CHANNEL 2  // PWM channel
   ```

2. **Setup in main.cpp**:
   ```cpp
   void setupMyOutput() {
       pinMode(MY_OUTPUT_PIN, OUTPUT);
       ledcSetup(MY_OUTPUT_CHANNEL, 1000, 8);  // 1 kHz, 8-bit
       ledcAttachPin(MY_OUTPUT_PIN, MY_OUTPUT_CHANNEL);
   }
   ```

3. **Use in logic**:
   ```cpp
   void setMyOutput(uint8_t value) {
       value = constrain(value, 0, 255);
       ledcWrite(MY_OUTPUT_CHANNEL, value);
   }
   ```

4. **Test it**:
   ```python
   def test_my_output_range(self):
       # Test 0-255 output range
       # Test PWM frequency (1 kHz)
       # Test GPIO pin assignment
   ```

### Adding an I2C Sensor

1. **Include driver library in platformio.ini**:
   ```ini
   lib_deps = 
       adafruit/Adafruit_INA219@^1.2.3
   ```

2. **Initialize in main.cpp**:
   ```cpp
   Adafruit_INA219 ina219_sensor(0x40);  // I2C address
   
   void setupSensors() {
       if (!ina219_sensor.begin()) {
           Serial.println("INA219 not found!");
       }
   }
   ```

3. **Read values**:
   ```cpp
   float readSensorValue() {
       return ina219_sensor.getCurrent_mA() / 1000.0;  // A
   }
   ```

4. **Mock in tests**:
   ```python
   def mock_sensor_read(self, sensor_type, value):
       # Simulate sensor reading for integration test
       self.sensor_values[sensor_type] = value
   ```

### Modifying Safety Thresholds

1. **Update config.h**:
   ```cpp
   #define SAFETY_CURRENT_MAX_A 50.0  // Max 50A
   #define SAFETY_VOLTAGE_MIN_V 9.0   // Min 9V
   #define SAFETY_TIMEOUT_MS 5000     // 5 second recovery
   ```

2. **Update safety check**:
   ```cpp
   bool checkSafetyLimits() {
       float current = readCurrentVoltage();
       
       if (current > SAFETY_CURRENT_MAX_A) {
           emergencyShutdown();
           return false;
       }
       
       return true;
   }
   ```

3. **Add test case**:
   ```python
   def test_overcurrent_at_new_threshold(self):
       # Verify new threshold is enforced
       if not test_threshold(SAFETY_CURRENT_MAX_A * 1.1, False):
           return False  # Should fail
       if not test_threshold(SAFETY_CURRENT_MAX_A * 0.9, True):
           return False  # Should pass
       
       return True
   ```

## Debugging Tips

### Compile Error: "undefined reference to `function_name`"

```bash
# Check that function is declared in config.h or main.cpp
grep "function_name" include/config.h src/main.cpp

# Verify function name spelling (case-sensitive)
# Rebuild with verbose output
pio run -v -e esp32dev
```

### Test Fails but Code Looks Correct

```bash
# Add debug output to test
print(f"DEBUG: Input={input}, Output={output}, Expected={expected}")

# Run test individually
python -c "from scripts.unit_tests import UnitTests; t = UnitTests(); print(t.test_my_function())"

# Check constantvalues in config.h
grep "MY_CONSTANT" include/config.h scripts/*.py
```

### Serial Output Shows Strange Characters

```bash
# Config.h might have typo preventing console setup
# Check SERIAL_BAUD rate matches monitor speed
pio device monitor --baud 115200

# Verify Serial.begin() is called in setup()
grep "Serial.begin" src/main.cpp
```

## Performance Considerations

### Memory Usage

```
ESP32 has 320 KB RAM

Typical allocation:
- Stack: ~10 KB
- Global variables: ~5 KB
- String buffers: ~5 KB
- Reserve: ~300 KB free

Avoid:
- Large local arrays: char buf[1000];
- String concatenation in loop: str += "data";
- Deep recursion: > 10 levels
```

### Timing Constraints

```
ISR execution: < 100 µs
  - RC capture ISR: ~20 µs
  - Safety check: ~50 µs total
  
Main loop rate: ~50 Hz
  - RC update: 20 ms
  - Monitoring: 1000 ms
  - Serial processing: on demand

Never do slow operations in ISR:
  - I2C reads (blocking, ~1 ms)
  - Serial writes (blocking)
  - Long calculations (> 50 µs)
```

## Release Checklist

Before tagging a new version:

```
Code Quality:
  [ ] All 56 tests pass
  [ ] GitHub Actions CI passes
  [ ] No compiler warnings
  [ ] No static analysis issues
  [ ] Code review completed

Documentation:
  [ ] CHANGELOG.md updated with changes
  [ ] Release notes written (release-notes/vX.X.X.md)
  [ ] README.md reflects new features
  [ ] API documentation updated
  [ ] Examples updated if applicable

Testing:
  [ ] Hardware verification completed (if available)
  [ ] Manual testing on target hardware
  [ ] Edge cases tested
  [ ] Regression test suite passes

Release:
  [ ] Version number bumped (semantic versioning)
  [ ] git tag created: git tag -a vX.X.X -m "Release vX.X.X"
  [ ] Tag pushed: git push origin vX.X.X
  [ ] GitHub release created from tag
  [ ] Changelog added to release notes
```

## Resources

- **Architecture**: [docs/ARCHITECTURE.md](ARCHITECTURE.md)
- **Hardware Details**: [docs/HARDWARE.md](HARDWARE.md)
- **Testing Strategy**: [docs/TESTING_STRATEGY.md](TESTING_STRATEGY.md)
- **Fan Feature**: [docs/FAN_CONTROL.md](FAN_CONTROL.md)
- **Troubleshooting**: [docs/TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- **Build System**: [docs/BUILD_PIPELINE.md](BUILD_PIPELINE.md)
- **Configuration**: [include/config.h](../include/config.h)
- **Main Code**: [src/main.cpp](../src/main.cpp)

## Getting Help

1. **Check existing documentation**: Start with TROUBLESHOOTING.md
2. **Review PROJECT_NOTES.md**: Known issues and workarounds
3. **Check git log**: See how previous developers solved problems
4. **Run test suites**: Identify which component is failing
5. **Add debug output**: Instrument code to see what's happening
6. **Serial monitor**: Connect to hardware and observe behavior

---

**Last Updated**: 2026-03-07  
**Version**: 1.2  
**Maintainers**: ESP32RC Team
