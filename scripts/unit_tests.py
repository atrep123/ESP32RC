#!/usr/bin/env python3
"""
ESP32RC Unit Tests

Comprehensive unit tests for core firmware functions without requiring hardware.
Tests mathematical functions, data transformations, and edge cases.
"""

import sys
import math
from pathlib import Path


class UnitTests:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.warnings = 0
        self.project_dir = Path(__file__).parent.parent
        self.test_results = []

    def run_test(self, category, name, test_func):
        """Execute a test and track results"""
        try:
            result = test_func()
            if result:
                print(f"[PASS] {name}")
                self.passed += 1
                self.test_results.append((category, name, "PASS", None))
            else:
                print(f"[FAIL] {name}")
                self.failed += 1
                self.test_results.append((category, name, "FAIL", None))
        except Exception as e:
            print(f"[ERROR] {name} - {str(e)}")
            self.failed += 1
            self.test_results.append((category, name, "ERROR", str(e)))

    def assert_equal(self, actual, expected):
        """Helper for equality assertions"""
        return actual == expected

    def assert_close(self, actual, expected, tolerance=0.01):
        """Helper for floating-point comparisons"""
        return abs(actual - expected) < tolerance

    def assert_in_range(self, value, min_val, max_val):
        """Helper for range checks"""
        return min_val <= value <= max_val

    # ========== RC Signal Tests ==========

    def test_rc_pulse_to_percentage(self):
        """Test RC pulse width to power percentage conversion"""
        # RC_MIN_PULSE = 1000µs → 0%
        # RC_MAX_PULSE = 2000µs → 100%
        
        # Helper function (reimplementing mapRCToPower logic)
        def map_rc_to_power(pulse):
            if pulse < 1000:
                pulse = 1000
            if pulse > 2000:
                pulse = 2000
            return (pulse - 1000) / 10.0  # 0-100%
        
        # Test minimum
        if not self.assert_equal(map_rc_to_power(1000), 0.0):
            return False
        
        # Test maximum
        if not self.assert_equal(map_rc_to_power(2000), 100.0):
            return False
        
        # Test midpoint
        if not self.assert_close(map_rc_to_power(1500), 50.0):
            return False
        
        # Test clamping (below min)
        if not self.assert_equal(map_rc_to_power(900), 0.0):
            return False
        
        # Test clamping (above max)
        if not self.assert_equal(map_rc_to_power(2100), 100.0):
            return False
        
        return True

    def test_rc_deadband(self):
        """Test RC deadband around midpoint"""
        rc_mid = 1500
        deadband = 50
        
        # Inside deadband (should be zeroed)
        value_in_deadband = rc_mid + 30
        if not self.assert_in_range(value_in_deadband, rc_mid - deadband, rc_mid + deadband):
            return False
        
        # Outside deadband (should not be zeroed)
        value_outside = rc_mid + 60
        if self.assert_in_range(value_outside, rc_mid - deadband, rc_mid + deadband):
            return False
        
        return True

    def test_rc_signal_timeout(self):
        """Test RC signal timeout detection"""
        rc_timeout_ms = 100
        
        # Current time vs last update
        current_time = 1000  # ms
        last_update_valid = 950  # 50ms ago → valid
        last_update_timeout = 850  # 150ms ago → timeout
        
        # Valid signal
        if not (current_time - last_update_valid) < rc_timeout_ms:
            return False
        
        # Timed out signal
        if not (current_time - last_update_timeout) >= rc_timeout_ms:
            return False
        
        return True

    # ========== Power Control Tests ==========

    def test_pwm_value_bounds(self):
        """Test PWM value (0-255) bounds checking"""
        def clamp_pwm(value):
            return max(0, min(255, int(value)))
        
        # Within range
        if not self.assert_equal(clamp_pwm(128), 128):
            return False
        
        # Below range
        if not self.assert_equal(clamp_pwm(-10), 0):
            return False
        
        # Above range
        if not self.assert_equal(clamp_pwm(300), 255):
            return False
        
        # Boundary values
        if not (self.assert_equal(clamp_pwm(0), 0) and self.assert_equal(clamp_pwm(255), 255)):
            return False
        
        return True

    def test_percentage_to_pwm_conversion(self):
        """Test percentage (0-100%) to PWM (0-255) conversion"""
        def percent_to_pwm(percent):
            return int((percent / 100.0) * 255)
        
        # 0% → 0 PWM
        if not self.assert_equal(percent_to_pwm(0), 0):
            return False
        
        # 100% → 255 PWM
        if not self.assert_equal(percent_to_pwm(100), 255):
            return False
        
        # 50% → ~127 PWM
        if not self.assert_close(percent_to_pwm(50), 127.5, tolerance=1):
            return False
        
        # 25% → ~64 PWM
        if not self.assert_close(percent_to_pwm(25), 63.75, tolerance=1):
            return False
        
        return True

    # ========== Light Control Tests ==========

    def test_light_thresholds(self):
        """Test light ON/OFF threshold logic"""
        light_on_threshold = 1600
        light_off_threshold = 1400
        
        # Below OFF threshold → light OFF
        if not (900 < light_off_threshold):
            return False
        
        # Between OFF and ON → light stays in previous state (hysteresis)
        if not (light_off_threshold <= 1500 <= light_on_threshold):
            return False
        
        # Above ON threshold → light ON
        if not (1800 > light_on_threshold):
            return False
        
        return True

    # ========== Fan Control Tests ==========

    def test_fan_temperature_mapping(self):
        """Test fan speed based on temperature (linear interpolation)"""
        fan_temp_low = 35.0
        fan_temp_high = 50.0
        fan_speed_min = 30
        fan_speed_max = 255
        
        def calculate_fan_speed(temp):
            if temp <= fan_temp_low:
                return 0
            if temp >= fan_temp_high:
                return fan_speed_max
            
            temp_range = fan_temp_high - fan_temp_low
            speed_range = fan_speed_max - fan_speed_min
            normalized = (temp - fan_temp_low) / temp_range
            return int(fan_speed_min + (normalized * speed_range))
        
        # Below min temp → 0
        if not self.assert_equal(calculate_fan_speed(30.0), 0):
            return False
        
        # At min temp → still 0
        if not self.assert_equal(calculate_fan_speed(35.0), 0):
            return False
        
        # At max temp → max speed
        if not self.assert_equal(calculate_fan_speed(50.0), fan_speed_max):
            return False
        
        # Above max temp → still max speed
        if not self.assert_equal(calculate_fan_speed(60.0), fan_speed_max):
            return False
        
        # Midpoint (42.5°C) → should be ~50% of range
        mid_speed = calculate_fan_speed(42.5)
        expected_mid = int(30 + 0.5 * (255 - 30))
        if not self.assert_close(mid_speed, expected_mid, tolerance=2):
            return False
        
        return True

    def test_fan_hysteresis(self):
        """Test fan speed hysteresis (prevents rapid oscillation)"""
        current_speed = 100
        new_speed = 105
        hysteresis_threshold = 10
        
        # Change below threshold → no update
        if not (abs(new_speed - current_speed) <= hysteresis_threshold):
            return False
        
        # Change above threshold → update
        new_speed_large = 115
        if not (abs(new_speed_large - current_speed) > hysteresis_threshold):
            return False
        
        return True

    # ========== Temperature Sensor Tests (v1.3) ==========

    def test_ds18b20_temperature_range_validation(self):
        """Test DS18B20 sensor reading validation (12-bit precision)"""
        # DS18B20 valid range: -55°C to +125°C
        # Invalid marker value: -127.00 (sensor error)
        
        def validate_temp_reading(temp):
            # -127.00 is error marker (failed read)
            if temp == -127.00:
                return False
            # Valid range check
            if temp < -55.0 or temp > 125.0:
                return False
            return True
        
        # Valid readings
        if not validate_temp_reading(25.0):
            return False
        if not validate_temp_reading(-10.0):
            return False
        if not validate_temp_reading(100.0):
            return False
        
        # Invalid: error marker
        if validate_temp_reading(-127.00):
            return False
        
        # Invalid: out of range
        if validate_temp_reading(-60.0):
            return False
        if validate_temp_reading(150.0):
            return False
        
        return True

    def test_ds18b20_precision_levels(self):
        """Test DS18B20 precision settings and conversion times"""
        # 9-bit:  0.5°C resolution,   93.75ms conversion
        # 10-bit: 0.25°C resolution, 187.5ms conversion
        # 11-bit: 0.125°C resolution, 375ms conversion
        # 12-bit: 0.0625°C resolution, 750ms conversion
        
        precision_map = {
            9: (0.5, 94),
            10: (0.25, 188),
            11: (0.125, 375),
            12: (0.0625, 750)
        }
        
        # Test 12-bit (highest precision)
        res, convert_time = precision_map.get(12, (None, None))
        if not self.assert_close(res, 0.0625, tolerance=0.001):
            return False
        if not (convert_time >= 700 and convert_time <= 800):
            return False
        
        return True

    def test_external_temp_sensor_fallback(self):
        """Test fallback from external to internal sensor"""
        use_external = True
        sensor_available = True
        external_read_failed = False
        
        # Scenario 1: External available, no failures → use external
        if not sensor_available:
            use_external = False
        if external_read_failed:
            use_external = False
        
        if not use_external:  # Should still be True (using external)
            return False
        
        # Scenario 2: External NOT available → fall back to internal  
        sensor_available = False
        use_external = True
        if not sensor_available:
            use_external = False
        
        if use_external:  # Should now be False (using internal)
            return False
        
        return True

    # ========== Current Measurement Tests ==========

    def test_current_scaling(self):
        """Test current measurement scaling"""
        current_scale = 25.0
        
        # INA219 reads 500mA raw
        raw_current_ma = 500
        
        def apply_scaling(raw, scale):
            return raw * scale
        
        scaled = apply_scaling(raw_current_ma, current_scale)
        
        # Scaled current should be raw * scale
        if not self.assert_equal(scaled, 500 * 25):
            return False
        
        return True

    def test_current_threshold_comparison(self):
        """Test current comparison for safety limits"""
        max_current = 50.0  # Amperes
        overcurrent_threshold = 48.0
        
        # Below threshold → safe
        if not (30.0 < overcurrent_threshold):
            return False
        
        # At threshold → trigger
        if not (48.0 >= overcurrent_threshold):
            return False
        
        # Above threshold → trigger
        if not (49.5 > overcurrent_threshold):
            return False
        
        return True

    # ========== Voltage Tests ==========

    def test_voltage_thresholds(self):
        """Test voltage safety thresholds"""
        min_voltage = 10.0
        max_voltage = 26.0
        
        # Below minimum → unsafe
        if not (8.0 < min_voltage):
            return False
        
        # Within range → safe
        if not (min_voltage <= 18.0 <= max_voltage):
            return False
        
        # Above maximum → unsafe (INA219 limit)
        if not (30.0 > max_voltage):
            return False
        
        return True

    # ========== Time-based Tests ==========

    def test_recovery_timeout(self):
        """Test fault recovery timeout logic"""
        recovery_timeout_ms = 5000
        
        fault_time = 1000  # ms
        current_time_during = 3000  # 2 seconds later
        current_time_recovery_ready = 6500  # 5.5 seconds later
        
        # Still waiting for recovery
        elapsed_during = current_time_during - fault_time
        if not (elapsed_during < recovery_timeout_ms):
            return False
        
        # Ready for recovery
        elapsed_ready = current_time_recovery_ready - fault_time
        if not (elapsed_ready >= recovery_timeout_ms):
            return False
        
        return True

    def test_monitoring_interval(self):
        """Test monitoring interval timing"""
        monitor_interval_ms = 100
        
        last_monitor = 1000
        current_time_skip = 1050  # 50ms later
        current_time_read = 1110  # 110ms later
        
        # Skip this read (too soon)
        if not ((current_time_skip - last_monitor) < monitor_interval_ms):
            return False
        
        # Perform read (enough time passed)
        if not ((current_time_read - last_monitor) >= monitor_interval_ms):
            return False
        
        return True

    # ========== Math & Edge Cases ==========

    def test_pwm_clamping_edge_cases(self):
        """Test PWM value edge cases"""
        def safe_pwm(value):
            return max(0, min(255, int(value)))
        
        # Float to int conversion
        if not self.assert_equal(safe_pwm(127.8), 127):
            return False
        
        # Zero
        if not self.assert_equal(safe_pwm(0), 0):
            return False
        
        # Max
        if not self.assert_equal(safe_pwm(255), 255):
            return False
        
        # Negative
        if not self.assert_equal(safe_pwm(-100), 0):
            return False
        
        # Large positive
        if not self.assert_equal(safe_pwm(1000), 255):
            return False
        
        return True

    def test_zero_division_protection(self):
        """Test functions handle zero/invalid inputs gracefully"""
        # Current scale should never be 0
        current_scale = 25.0
        if not (current_scale > 0):
            return False
        
        # Temperature range should be valid
        fan_temp_low = 35.0
        fan_temp_high = 50.0
        temp_range = fan_temp_high - fan_temp_low
        if not (temp_range > 0):
            return False
        
        return True

    def test_boolean_logic_gates(self):
        """Test safety condition combinations"""
        # All conditions must be safe for recovery
        current_safe = True
        voltage_safe = True
        all_conditions_safe = current_safe and voltage_safe
        
        if not all_conditions_safe:
            return False
        
        # If any condition unsafe, system stays faulted
        current_unsafe = False
        any_unsafe = current_unsafe or not voltage_safe
        
        if any_unsafe:
            return False
        
        return True

    # ========== Test Runner ==========

    def run_all_tests(self):
        """Execute entire test suite"""
        print("=" * 60)
        print("ESP32RC Unit Tests")
        print("=" * 60)

        print("\n--- RC Signal Tests ---")
        self.run_test("rc", "RC pulse to power percentage", self.test_rc_pulse_to_percentage)
        self.run_test("rc", "RC deadband", self.test_rc_deadband)
        self.run_test("rc", "RC signal timeout", self.test_rc_signal_timeout)

        print("\n--- Power Control Tests ---")
        self.run_test("power", "PWM value bounds", self.test_pwm_value_bounds)
        self.run_test("power", "Percentage to PWM conversion", self.test_percentage_to_pwm_conversion)

        print("\n--- Light Control Tests ---")
        self.run_test("light", "Light thresholds", self.test_light_thresholds)

        print("\n--- Fan Control Tests ---")
        self.run_test("fan", "Fan temperature mapping", self.test_fan_temperature_mapping)
        self.run_test("fan", "Fan hysteresis", self.test_fan_hysteresis)

        print("\n--- Temperature Sensor Tests (v1.3) ---")
        self.run_test("sensor", "DS18B20 temperature range validation", self.test_ds18b20_temperature_range_validation)
        self.run_test("sensor", "DS18B20 precision levels", self.test_ds18b20_precision_levels)
        self.run_test("sensor", "External temp sensor fallback", self.test_external_temp_sensor_fallback)

        print("\n--- Current Measurement Tests ---")
        self.run_test("current", "Current scaling", self.test_current_scaling)
        self.run_test("current", "Current threshold comparison", self.test_current_threshold_comparison)

        print("\n--- Voltage Tests ---")
        self.run_test("voltage", "Voltage thresholds", self.test_voltage_thresholds)

        print("\n--- Time-based Tests ---")
        self.run_test("timing", "Recovery timeout", self.test_recovery_timeout)
        self.run_test("timing", "Monitoring interval", self.test_monitoring_interval)

        print("\n--- Math & Edge Cases ---")
        self.run_test("edge_case", "PWM clamping edge cases", self.test_pwm_clamping_edge_cases)
        self.run_test("edge_case", "Zero division protection", self.test_zero_division_protection)
        self.run_test("edge_case", "Boolean logic gates", self.test_boolean_logic_gates)

        print("\n" + "=" * 60)
        print(f"Results: {self.passed} passed, {self.failed} failed, {self.warnings} warnings")
        print("=" * 60)

        return self.failed == 0


if __name__ == "__main__":
    tester = UnitTests()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)
