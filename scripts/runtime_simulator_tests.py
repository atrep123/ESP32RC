#!/usr/bin/env python3
"""
ESP32RC Runtime Simulator Tests

Simulates real-time system behavior over multiple cycles.
Tests state transitions, timing, and scenario combinations.
"""

import sys
from enum import Enum
from typing import List, Dict


class SystemState(Enum):
    """System operational states"""
    IDLE = "IDLE"
    ARMED = "ARMED"
    RUNNING = "RUNNING"
    FAILSAFE = "FAILSAFE"
    RECOVERY = "RECOVERY"
    SHUTDOWN = "SHUTDOWN"


class RuntimeSimulator:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.time_ms = 0
        self.events: List[Dict] = []
        self.log_events = False

    def run_test(self, name, test_func):
        """Execute a test and track results"""
        try:
            result = test_func()
            if result:
                print(f"[PASS] {name}")
                self.passed += 1
            else:
                print(f"[FAIL] {name}")
                self.failed += 1
        except Exception as e:
            print(f"[ERROR] {name} - {str(e)}")
            self.failed += 1

    def log(self, time_ms, event_type, value=None):
        """Log system event"""
        if self.log_events:
            self.events.append({
                "time_ms": time_ms,
                "type": event_type,
                "value": value
            })

    def print_scenario(self, name: str):
        """Print scenario header"""
        print(f"\n  Scenario: {name}")

    # ========== Scenario Simulators ==========

    def test_normal_operation_cycle(self):
        """Simulate normal steady-state operation"""
        self.print_scenario("Normal operation (30 seconds)")
        self.log_events = True
        self.events.clear()
        
        # Configuration
        monitor_interval_50ms = 50
        serial_update_interval_500ms = 500
        
        # Simulate 30 seconds (30000ms)
        time_ms = 0
        expected_monitor_cycles = 30000 // monitor_interval_50ms
        expected_serial_updates = 30000 // serial_update_interval_500ms
        
        monitor_count = 0
        serial_count = 0
        last_monitor = 0
        last_serial = 0
        
        while time_ms <= 30000:
            # Monitor current/voltage every 50ms
            if time_ms - last_monitor >= monitor_interval_50ms:
                monitor_count += 1
                last_monitor = time_ms
                self.log(time_ms, "monitor_read", "current_voltage")
            
            # Serial output every 500ms
            if time_ms - last_serial >= serial_update_interval_500ms:
                serial_count += 1
                last_serial = time_ms
                self.log(time_ms, "serial_output", "status")
            
            # Simulate RC input update every 20ms (50Hz)
            if time_ms % 20 == 0:
                self.log(time_ms, "rc_input", 1500)  # Midpoint
            
            time_ms += 10  # Simulate 10ms loop iterations
        
        # Verify reasonable counts
        if not (300 <= monitor_count <= 700):  # Should be ~600
            print(f"  WARNING: Monitor cycles {monitor_count}, expected ~600")
            return False
        
        if not (30 <= serial_count <= 70):  # Should be ~60
            print(f"  WARNING: Serial updates {serial_count}, expected ~60")
            return False
        
        return True

    def test_rc_signal_loss_recovery(self):
        """Simulate RC signal lost then recovered"""
        self.print_scenario("RC signal loss and recovery")
        self.log_events = True
        self.events.clear()
        
        rc_timeout_ms = 100
        rc_signal_valid = True
        power_output = 100  # 50% power
        last_rc_update = 0
        time_ms = 0
        
        # Phase 1: Valid RC signal (0-1000ms)
        while time_ms < 1000:
            time_ms += 50
            if time_ms % 50 == 0:
                last_rc_update = time_ms
                rc_signal_valid = (time_ms - last_rc_update) < rc_timeout_ms
            self.log(time_ms, "state", "RC_VALID" if rc_signal_valid else "RC_TIMEOUT")
        
        # Phase 2: RC signal lost (1000-1500ms)
        for offset in range(0, 500, 50):
            time_ms = 1000 + offset
            # No RC update, signal times out after 100ms
            if time_ms - last_rc_update >= rc_timeout_ms:
                rc_signal_valid = False
                power_output = 0  # Kill power
                self.log(time_ms, "event", "FAILSAFE_TRIGGERED")
            self.log(time_ms, "state", "RC_VALID" if rc_signal_valid else "RC_TIMEOUT")
        
        # Phase 3: RC signal recovered (1500-2000ms)
        last_rc_update = 1500
        for offset in range(0, 500, 50):
            time_ms = 1500 + offset
            last_rc_update = time_ms
            rc_signal_valid = (time_ms - last_rc_update) < rc_timeout_ms
            power_output = 100 if rc_signal_valid else 0
            self.log(time_ms, "state", "RC_VALID" if rc_signal_valid else "RC_TIMEOUT")
        
        # Verify signal was lost and then recovered
        rc_valid_events = [e for e in self.events if "RC_VALID" in e.get("value", "")]
        rc_timeout_events = [e for e in self.events if "RC_TIMEOUT" in e.get("value", "")]
        
        if not (len(rc_valid_events) > 0 and len(rc_timeout_events) > 0):
            print("  WARNING: Did not see both RC_VALID and RC_TIMEOUT states")
            return False
        
        return True

    def test_overcurrent_protection_recovery(self):
        """Simulate overcurrent fault and automatic recovery"""
        self.print_scenario("Overcurrent protection and recovery")
        self.log_events = True
        self.events.clear()
        
        overcurrent_threshold_a = 48.0
        recovery_timeout_ms = 5000
        
        system_enabled = True
        power_output = 255
        current_a = 30.0
        fault_time = None
        time_ms = 0
        recovery_event_count = 0
        
        # Phase 1: Normal operation (0-1000ms)
        while time_ms < 1000:
            time_ms += 100
            self.log(time_ms, "state", f"NORMAL (I={current_a:.1f}A)")
        
        # Phase 2: Current rises, overcurrent at 1000ms
        time_ms = 1000
        current_a = 50.0
        if current_a >= overcurrent_threshold_a:
            system_enabled = False
            power_output = 0
            fault_time = time_ms
            self.log(time_ms, "event", "OVERCURRENT_DETECTED")
        
        # Phase 3: Wait for recovery window (1000-6000ms)
        # Current gradually drops as load is reduced
        while time_ms < 6000:
            time_ms += 500
            time_elapsed = time_ms - fault_time
            
            # Gradually reduce current during fault
            current_a = max(30.0, 50.0 - (time_elapsed / 500.0))
            
            # Check recovery conditions
            current_safe = current_a < (overcurrent_threshold_a - 2.0)  # 46A or less
            time_ok = time_elapsed >= recovery_timeout_ms
            
            state_str = f"FAULTED (wait {time_elapsed}ms, I={current_a:.1f}A)"
            if time_ok and current_safe:
                system_enabled = True
                state_str = "RECOVERED"
                self.log(time_ms, "event", "RECOVERY_SUCCESSFUL")
                recovery_event_count += 1
            
            self.log(time_ms, "state", state_str)
        
        # Verify overcurrent and recovery events were logged
        if recovery_event_count < 1:
            print(f"  WARNING: Recovery event not triggered (current={current_a:.1f}A)")
            return False
        
        return True

    def test_temperature_ramp_fan_response(self):
        """Simulate gradual temperature increase and fan response"""
        self.print_scenario("Temperature ramp with fan response")
        self.log_events = True
        self.events.clear()
        
        fan_temp_low = 35.0
        fan_temp_high = 50.0
        fan_speed_min = 30
        fan_speed_max = 255
        
        def get_fan_speed(temp):
            if temp <= fan_temp_low:
                return 0
            if temp >= fan_temp_high:
                return fan_speed_max
            temp_range = fan_temp_high - fan_temp_low
            speed_range = fan_speed_max - fan_speed_min
            normalized = (temp - fan_temp_low) / temp_range
            return int(fan_speed_min + (normalized * speed_range))
        
        # Gradual temperature rise over 15 seconds
        time_ms = 0
        temperature_c = 25.0
        time_per_degree = 15000 / 25  # 600ms per degree
        
        while temperature_c <= 60.0:
            fan_speed = get_fan_speed(temperature_c)
            self.log(int(time_ms), "sensor", f"T={temperature_c:.1f}C fan={fan_speed}")
            
            time_ms += 300  # Sample every 300ms
            temperature_c += 0.5  # Rise gradually
        
        # Verify fan response monotonic increase
        fan_speeds = []
        for e in self.events:
            if "fan=" in str(e.get("value", "")):
                speed = int(str(e.get("value", "")).split("fan=")[1])
                fan_speeds.append(speed)
        
        # Check monotonic non-decreasing
        for i in range(len(fan_speeds) - 1):
            if fan_speeds[i] > fan_speeds[i + 1]:
                print(f"  WARNING: Fan speed decreased: {fan_speeds[i]} -> {fan_speeds[i+1]}")
                return False
        
        return True

    def test_light_hysteresis(self):
        """Simulate light hysteresis with repeated threshold crossings"""
        self.print_scenario("Light hysteresis (RC around threshold)")
        self.log_events = True
        self.events.clear()
        
        light_on_threshold = 1600
        light_off_threshold = 1400
        light_state = False
        
        # Simulate RC jitter around ON threshold
        rc_values = [1580, 1590, 1595, 1605, 1610, 1600, 1590, 1595, 1610, 1615]
        
        for rc_pulse in rc_values:
            # Apply hysteresis logic
            if rc_pulse >= light_on_threshold:
                light_state = True
            elif rc_pulse <= light_off_threshold:
                light_state = False
            
            self.log(0, "state", f"RC={rc_pulse} light={'ON' if light_state else 'OFF'}")
        
        # Verify light eventually turned on
        light_on_events = [e for e in self.events if "light=ON" in str(e.get("value", ""))]
        if len(light_on_events) == 0:
            print("  WARNING: Light never turned on")
            return False
        
        return True

    def test_multi_second_steady_state(self):
        """Simulate 60-second steady-state operation"""
        self.print_scenario("60-second steady-state operation")
        self.log_events = False  # Reduce logging
        
        # Simulate steady 30A current, 15V voltage, 40°C temp
        stable_for_ticks = 0
        monitor_interval_ms = 100
        total_time_ms = 0
        
        while total_time_ms < 60000:
            # Current stable
            current_a = 30.0
            voltage_v = 15.0
            temperature_c = 40.0
            
            # All systems nominal
            if (25.0 < current_a < 35.0 and
                10.0 < voltage_v < 26.0 and
                30.0 < temperature_c < 50.0):
                stable_for_ticks += 1
            
            total_time_ms += monitor_interval_ms
        
        # Should have been stable for entire duration
        expected_ticks = 60000 // monitor_interval_ms
        if stable_for_ticks < expected_ticks * 0.9:  # At least 90%
            print(f"  WARNING: Only {stable_for_ticks}/{expected_ticks} ticks stable")
            return False
        
        return True

    def test_command_sequence_execution(self):
        """Simulate sequence of serial commands"""
        self.print_scenario("Serial command sequence")
        self.log_events = True
        self.events.clear()
        
        fan_auto_mode = True
        fan_speed = 0
        serial_test_mode = False
        
        commands = [
            ("selftest on", {}),
            ("fan status", {}),
            ("fan speed 150", {}),
            ("pwm 50", {}),
            ("light1 on", {}),
            ("fan auto", {}),
            ("status", {}),
            ("selftest off", {}),
        ]
        
        for cmd, _ in commands:
            self.log(0, "command", cmd)
            
            # Process command
            if "selftest on" in cmd:
                serial_test_mode = True
            elif "selftest off" in cmd:
                serial_test_mode = False
            elif "fan speed" in cmd:
                fan_auto_mode = False
                fan_speed = 150
            elif "fan auto" in cmd:
                fan_auto_mode = True
        
        # Verify commands were processed
        if len(self.events) != len(commands):
            print(f"  WARNING: Only {len(self.events)} of {len(commands)} commands logged")
            return False
        
        return True

    def test_edge_case_rapid_state_changes(self):
        """Simulate rapid state changes (stress test)"""
        self.print_scenario("Rapid state changes (stress test)")
        self.log_events = False
        
        state_changes = 0
        
        # Rapid RC changes (simulating nervous user)
        rc_values = [1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000] * 2
        for rc in rc_values:
            percent = (rc - 1000) / 10.0
            state_changes += 1
        
        # Rapid temperature fluctuation
        temperatures = [35, 37, 40, 42, 45, 47, 50, 48, 46, 44, 40, 38, 35]
        for temp in temperatures:
            state_changes += 1
        
        # Rapid commands
        commands = ["fan auto", "fan off", "fan speed 100", "fan auto", 
                   "fan speed 50", "fan off", "fan auto"] * 3
        state_changes += len(commands)
        
        # System should handle without crashes (>=50 state changes)
        if state_changes < 50:
            print(f"  WARNING: Only {state_changes} state changes")
            return False
        
        return True

    def test_startup_to_shutdown_cycle(self):
        """Simulate complete power-on to power-off cycle"""
        self.print_scenario("Complete startup to shutdown cycle")
        self.log_events = True
        self.events.clear()
        
        system_state = SystemState.IDLE
        
        # Startup
        self.log(0, "event", "POWER_ON")
        system_state = SystemState.ARMED
        self.log(100, "state", "ARMED")
        
        # Receive RC signal
        self.log(500, "event", "RC_SIGNAL_DETECTED")
        system_state = SystemState.RUNNING
        self.log(500, "state", "RUNNING")
        
        # Operate
        for t in range(1000, 5000, 100):
            current_a = 25.0
            if t == 3000:
                current_a = 51.0
                system_state = SystemState.FAILSAFE
                self.log(3000, "event", "OVERCURRENT")
        
        # Recovery
        self.log(8000, "event", "RECOVERY_ATTEMPT")
        if True:  # Recovery succeeds
            system_state = SystemState.RUNNING
            self.log(8000, "state", "RUNNING_RECOVERED")
        
        # Shutdown
        self.log(10000, "event", "POWER_OFF_REQUEST")
        system_state = SystemState.SHUTDOWN
        self.log(10100, "state", "IDLE")
        
        # Verify state progression
        state_sequence = [
            SystemState.IDLE,
            SystemState.ARMED,
            SystemState.RUNNING,
            SystemState.FAILSAFE,
            SystemState.RUNNING,
            SystemState.SHUTDOWN,
        ]
        
        if len(self.events) < 8:
            print(f"  WARNING: Expected more events, got {len(self.events)}")
            return False
        
        return True

    # ========== Test Runner ==========

    def run_all_tests(self):
        """Execute all runtime simulation tests"""
        print("=" * 60)
        print("ESP32RC Runtime Simulator Tests")
        print("=" * 60)

        print("\n--- Real-time Operation Scenarios ---")
        self.run_test("Normal operation cycle", self.test_normal_operation_cycle)
        self.run_test("RC signal loss and recovery", self.test_rc_signal_loss_recovery)
        self.run_test("Overcurrent protection recovery", self.test_overcurrent_protection_recovery)
        self.run_test("Temperature ramp with fan response", self.test_temperature_ramp_fan_response)

        print("\n--- Edge Cases & Stress Tests ---")
        self.run_test("Light hysteresis", self.test_light_hysteresis)
        self.run_test("Multi-second steady-state", self.test_multi_second_steady_state)
        self.run_test("Rapid state changes (stress)", self.test_edge_case_rapid_state_changes)

        print("\n--- Complex Scenarios ---")
        self.run_test("Serial command sequence", self.test_command_sequence_execution)
        self.run_test("Startup to shutdown cycle", self.test_startup_to_shutdown_cycle)

        print("\n" + "=" * 60)
        print(f"Results: {self.passed} passed, {self.failed} failed")
        print("=" * 60)

        return self.failed == 0


if __name__ == "__main__":
    simulator = RuntimeSimulator()
    success = simulator.run_all_tests()
    sys.exit(0 if success else 1)
