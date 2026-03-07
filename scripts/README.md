# Scripts Documentation

## Overview

This directory contains Python scripts for testing, verification, and build support for the ESP32RC project. All scripts are designed to run without external test framework dependencies using only Python 3.7+ standard library.

## Scripts

### 1. run_all_tests.py

**Purpose**: Master test orchestrator that runs all 56 tests across 4 suites

**Usage**:
```bash
python scripts/run_all_tests.py
```

**Output**:
```
ESP32RC Comprehensive Test Suite
======================================================================

Running: Build Sanity Tests (15 tests)     [~8s]
Running: Unit Tests (16 tests)              [~2s]
Running: Integration Tests (16 tests)       [~2s]
Running: Runtime Simulator Tests (9 tests)  [~3s]

======================================================================
COMPREHENSIVE TEST SUMMARY

Test Suites Run:
  [PASS] Build Sanity Tests (15 tests)
  [PASS] Unit Tests (16 tests)
  [PASS] Integration Tests (16 tests)
  [PASS] Runtime Simulator Tests (9 tests)

Suite Results: 4/4 suites passed
Total Tests: 56 passed, 0 failed

SUCCESS: ALL TESTS PASSED
```

**Exit Code**: 0 (success), 1 (any test failed)

**Integration**: Used in GitHub Actions CI/CD pipeline

---

### 2. build_sanity_tests.py

**Purpose**: Fast build verification and configuration validation

**Usage**:
```bash
python scripts/build_sanity_tests.py
```

**Tests** (15 total):
1. **esp32dev build success** - Compile production firmware
2. **esp32dev_selftest build success** - Compile self-test firmware
3. **SERIAL_SELF_TEST flag present** - Self-test environment configured
4. **PSRAM disabled in defaults** - No PSRAM flag in esp32dev
5. **RC channel defines exist** - All 4 RC pins defined
6. **PWM definitions present** - Power and fan PWM configs
7. **Safety thresholds defined** - Voltage/current/temp limits
8. **Recovery timeout defined** - 5-second timeout present
9. **Critical functions compiled** - mapRCToPower, readCurrentVoltage, etc.
10. **Atomic read helpers present** - readRCValue, readRCUpdateTime, etc.
11. **Fan control functions present** - calculateFanSpeed, updateFanControl
12. **Main setup contains pinMode calls** - GPIO initialization
13. **Main setup contains PWM setup** - ledcSetup calls
14. **recovery_allowed_time variable used** - Recovery mechanism in use
15. **Handler interrupts attached** - attachInterrupt calls present

**Execution Time**: ~8-10 seconds

**When to Use**: 
- Before running full test suite
- Verifying build environment setup
- CI/CD pipeline first stage

---

### 3. unit_tests.py

**Purpose**: Validate individual mathematical functions and transformations

**Usage**:
```bash
python scripts/unit_tests.py
```

**Test Categories** (16 total):

**RC Input Tests** (3):
- `test_rc_pulse_to_percentage()` - 1000µs→0%, 1500µs→50%, 2000µs→100%
- `test_rc_deadband_detection()` - ±50µs deadband around 1500µs
- `test_rc_timeout_detection()` - 100ms timeout threshold

**Power Control Tests** (2):
- `test_pwm_value_clamping()` - 0-255 bounds enforcement
- `test_percentage_to_pwm_conversion()` - Linear scaling 0-100% → 0-255

**Light Control Tests** (1):
- `test_light_threshold_hysteresis()` - ON/OFF switching with hysteresis

**Fan Control Tests** (2):
- `test_fan_temperature_mapping()` - Linear 35-50°C → 30-255 PWM
- `test_fan_speed_hysteresis()` - 10-point hysteresis prevents flutter

**Current Measurement Tests** (2):
- `test_current_scaling_application()` - mA → A conversion
- `test_overcurrent_threshold_comparison()` - Threshold detection

**Voltage Safety Tests** (1):
- `test_voltage_range_validation()` - Min/max voltage checks

**Time-based Tests** (2):
- `test_recovery_timeout_logic()` - 5-second window
- `test_monitoring_interval_timing()` - 1000ms intervals

**Edge Case Tests** (3):
- `test_pwm_clamping_edge_cases()` - Boundary values
- `test_divide_by_zero_protection()` - Safe division
- `test_boolean_logic_gates()` - Safety AND/OR logic

**Execution Time**: ~2-3 seconds

**When to Use**:
- Validating mathematical correctness
- Testing edge cases and boundary conditions
- No hardware required
- Fast feedback loop during development

---

### 4. integration_tests.py

**Purpose**: Validate component interactions and system data flows

**Usage**:
```bash
python scripts/integration_tests.py
```

**Test Categories** (16 total):

**RC → Power Control** (3):
- `test_rc_signal_to_power_control_direct()` - Full mapping chain
- `test_rc_signal_loss_trigger_failsafe()` - Timeout → 0% power
- `test_multi_channel_independent_control()` - 4 channels independent

**Safety System** (4):
- `test_overcurrent_triggers_shutdown()` - High I → shutdown
- `test_automatic_recovery_after_timeout()` - Fault → wait → recover
- `test_recovery_conditions_must_all_pass()` - AND logic for recovery
- `test_recovery_resets_fault_state()` - State reset on recovery

**Temperature & Fan** (3):
- `test_temperature_rise_increases_fan()` - Temp↑ → Speed↑
- `test_fan_saturation_at_maximum()` - Speed caps at 255
- `test_fan_manual_mode_override()` - Serial command override

**Light Control** (1):
- `test_light_on_off_with_hysteresis()` - RC threshold hysteresis

**Power Monitoring** (2):
- `test_current_scaling_affects_all_calculations()` - Global scaling
- `test_voltage_and_current_monitoring_independent()` - Parallel checks

**Serial Interface** (1):
- `test_fan_control_commands_execute()` - Fan commands work

**System State** (2):
- `test_proper_startup_sequence()` - Boot initialization
- `test_safe_shutdown_sequence()` - Graceful powerdown

**Execution Time**: ~2-3 seconds

**When to Use**:
- Validating component interactions
- Testing state transitions
- Verifying data flow between systems
- No hardware required

---

### 5. runtime_simulator_tests.py

**Purpose**: Simulate extended system operation and stress scenarios

**Usage**:
```bash
python scripts/runtime_simulator_tests.py
```

**Test Scenarios** (9 total):

**Normal Operation** (1):
- `test_normal_operation_cycle()` - 30s steady operation with monitoring

**Failure Scenarios** (3):
- `test_rc_signal_loss_and_recovery()` - RC signal 1s valid, 500ms loss, 500ms recovery
- `test_overcurrent_protection_and_recovery()` - Fault at 1s, auto-recovery at 6s
- `test_temperature_ramp_fan_response()` - Linear temp ramp 25-60°C, verify fan linearity

**Stress Testing** (3):
- `test_light_control_hysteresis_under_rapid_changes()` - 50+ threshold crossings
- `test_60_second_stable_operation()` - Long-running stability
- `test_rapid_state_transitions()` - 50+ state changes in 10s

**Complex Scenarios** (2):
- `test_serial_command_sequence_execution()` - Multiple commands in sequence
- `test_complete_power_on_to_shutdown_lifecycle()` - Full cycle boot→run→fail→recover→shutdown

**Simulation Resolution**: 100 ms timesteps

**Execution Time**: ~3-5 seconds

**When to Use**:
- Validating system behavior over extended periods
- Stress testing state machines
- Verifying timing-dependent behavior
- No hardware required

---

### 6. build_metadata.py

**Purpose**: Extract and display build metadata and version information

**Usage**:
```bash
python scripts/build_metadata.py
```

**Output**:
```
ESP32RC Build Metadata
==================================================
Git Information:
  Branch: main
  Commit: 47ff662...
  Tag: v1.2 (v1.2-0-g47ff662)
  Latest: 2026-03-07 12:34:56 UTC
  Status: clean

Firmware Information:
  Version: 1.2
  Build Date: 2026-03-07
  Build Time: 12:34:56
  Build Size: 319 KB (esp32dev), 331 KB (esp32dev_selftest)

Environment Information:
  Python: 3.x
  PlatformIO: 6.x
  Arduino: 3.20017
```

**When to Use**:
- Before releases (verify clean state)
- Documentation generation
- Build artifact tracking

---

### 7. hardware_verification.py

**Purpose**: Interactive hardware testing and verification via serial interface

**Usage**:
```bash
python scripts/hardware_verification.py
```

**Prompts**:
```
ESP32RC Hardware Verification Tool
==================================================

Select verification scenario:
1. Power control test (ramp power 0-100%)
2. RC input test (verify 4 channels)
3. Current measurement test
4. Voltage monitoring test
5. Temperature sensor test
6. Fan control test
7. Light control test
8. Full system test
9. Exit

Choice: [1-9]
```

**Requirements**:
- Firmware already flashed to ESP32
- Device connected via USB (serial port)
- Terminal config: 115200 baud, 8N1

**When to Use**:
- After flashing firmware to hardware
- Before deployment
- Troubleshooting hardware issues
- Verify all GPIO/sensors working

---

## Running Tests Locally

### Quick Test (5 seconds)

```bash
python scripts/unit_tests.py
```

### Comprehensive Test (15 seconds)

```bash
python scripts/run_all_tests.py
```

### Specific Test Suite

```bash
python scripts/build_sanity_tests.py    # 15 tests, ~8s
python scripts/unit_tests.py             # 16 tests, ~2s
python scripts/integration_tests.py      # 16 tests, ~2s
python scripts/runtime_simulator_tests.py # 9 tests, ~3s
```

### With Verbose Output

```bash
# Most scripts support -v flag
python scripts/unit_tests.py -v
python scripts/integration_tests.py -v
```

## Test Results Interpretation

### All Tests Pass ✓

```
Results: 56 passed, 0 failed, 0 warnings
```

**Action**: Safe to commit, no issues found

### Some Tests Fail ✗

```
Results: 54 passed, 2 failed
  FAILED: test_rc_signal_to_power_control_direct
  FAILED: test_overcurrent_triggers_shutdown
```

**Action**: 
1. Review failed test logic in corresponding script
2. Check if code changes broke assumptions
3. Debug with added print statements
4. Fix code or test expectation

### Build Sanity Fails ✗

```
ERROR: esp32dev build failed
ERROR: Function mapRCToPower not found in binary
```

**Action**:
1. Verify src/main.cpp contains function
2. Check PlatformIO environment configured
3. Run `pio run --target clean -e esp32dev`
4. Check for syntax errors in config.h

## Development Tips

### Adding a Test

1. Choose appropriate suite (unit/integration/runtime)
2. Copy test method template
3. Implement test logic (arrange, act, assert)
4. Add to `run_all_tests()` list in that script
5. Run: `python scripts/run_all_tests.py`
6. Verify it passes before committing

### Debugging a Test

1. Add `print(f"DEBUG: var={var}")` statements
2. Run individual test suite: `python scripts/unit_tests.py`
3. Check output for diagnostic info
4. Modify test to isolate issue
5. Remove debug prints before committing

### Running Tests in CI/CD

GitHub Actions automatically runs:
```bash
python scripts/run_all_tests.py
```

On every push/PR. Check Actions tab for results.

## Performance Notes

**Typical Execution Times**:
- Build sanity: 8-10 seconds
- Unit tests: 2-3 seconds
- Integration tests: 2-3 seconds
- Runtime simulator: 3-5 seconds
- **Total**: ~15 seconds

**Optimization**: Tests run serially in run_all_tests.py but could be parallelized in CI/CD

## Troubleshooting

### "ModuleNotFoundError: No module named 'platformio'"

```bash
# Install PlatformIO
pip install platformio

# Or verify it's in PATH
pio --version
```

### "Permission denied" when running script

```bash
# Make script executable (Linux/Mac)
chmod +x scripts/*.py

# Or run with python directly (all platforms)
python scripts/unit_tests.py
```

### Tests fail with "UnicodeEncodeError"

```bash
# Might be Windows encoding issue
# Scripts should auto-detect and use ASCII, but if not:
export PYTHONIOENCODING=utf-8
python scripts/run_all_tests.py
```

### Serial port not found (hardware_verification.py)

```bash
# List available serial ports
python -m serial.tools.list_ports

# Specify port manually if auto-detection fails
python scripts/hardware_verification.py --port COM3
```

## References

- **Testing Strategy**: [docs/TESTING_STRATEGY.md](../docs/TESTING_STRATEGY.md)
- **Developer Guide**: [docs/DEVELOPER_GUIDE.md](../docs/DEVELOPER_GUIDE.md)
- **Hardware Verification**: [docs/HARDWARE_VERIFICATION.md](../docs/HARDWARE_VERIFICATION.md)
- **Build Pipeline**: [docs/BUILD_PIPELINE.md](../docs/BUILD_PIPELINE.md)

---

**Last Updated**: 2026-03-07  
**Test Coverage**: 56 tests across 4 suites  
**Python Version**: 3.7+  
**Dependencies**: None (standard library only)
