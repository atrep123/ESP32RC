#!/usr/bin/env python3
"""
ESP32RC Build Sanity Tests

Tests to verify that the firmware builds correctly, contains expected symbols,
and has proper configuration settings.
"""

import subprocess
import sys
import re
from pathlib import Path


class BuildSanityTest:
    def __init__(self, project_dir):
        self.project_dir = Path(project_dir)
        self.passed = 0
        self.failed = 0
        self.warnings = 0

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
            print(f"[ERROR] {name} - {e}")
            self.failed += 1

    def test_build_env_esp32dev(self):
        """Test that esp32dev environment builds successfully"""
        result = subprocess.run(
            ["pio", "run", "-e", "esp32dev"],
            cwd=self.project_dir,
            capture_output=True,
            text=True
        )
        return result.returncode == 0

    def test_build_env_selftest(self):
        """Test that esp32dev_selftest environment builds successfully"""
        result = subprocess.run(
            ["pio", "run", "-e", "esp32dev_selftest"],
            cwd=self.project_dir,
            capture_output=True,
            text=True
        )
        return result.returncode == 0

    def test_config_header_exists(self):
        """Test that config.h exists and is readable"""
        config_path = self.project_dir / "include" / "config.h"
        return config_path.exists() and config_path.is_file()

    def test_config_header_constants(self):
        """Test that critical config constants are defined"""
        config_path = self.project_dir / "include" / "config.h"
        content = config_path.read_text()
        
        required_defines = [
            "RC_MIN_PULSE",
            "RC_MAX_PULSE",
            "MAX_CURRENT_A",
            "MIN_VOLTAGE_V",
            "MAX_VOLTAGE_V",
            "OVERCURRENT_THRESHOLD",
            "CURRENT_SCALE",
            "MONITOR_INTERVAL",
        ]
        
        for define in required_defines:
            if f"#define {define}" not in content:
                print(f"  Missing define: {define}")
                return False
        return True

    def test_main_cpp_exists(self):
        """Test that main.cpp exists"""
        main_path = self.project_dir / "src" / "main.cpp"
        return main_path.exists() and main_path.is_file()

    def test_main_cpp_functions(self):
        """Test that critical functions are present in main.cpp"""
        main_path = self.project_dir / "src" / "main.cpp"
        content = main_path.read_text()
        
        required_functions = [
            "setup()",
            "loop()",
            "mapRCToPower",
            "updatePowerControl",
            "updateLightControl",
            "updateAuxOutput",
            "checkSafetyLimits",
            "readCurrentVoltage",
            "isChannelSignalValid",
        ]
        
        for func in required_functions:
            if func not in content:
                print(f"  Missing function: {func}")
                return False
        return True

    def test_atomic_reads_present(self):
        """Test that atomic read helpers are present"""
        main_path = self.project_dir / "src" / "main.cpp"
        content = main_path.read_text()
        
        atomic_helpers = [
            "readRCValue",
            "readRCUpdateTime",
            "readRCChannelSeen",
        ]
        
        for helper in atomic_helpers:
            if helper not in content:
                print(f"  Missing atomic helper: {helper}")
                return False
        return True

    def test_recovery_mechanism_present(self):
        """Test that recovery mechanism is implemented"""
        main_path = self.project_dir / "src" / "main.cpp"
        content = main_path.read_text()
        
        recovery_items = [
            "system_fault_time",
            "RECOVERY_TIMEOUT_MS",
            "recovery_attempted",
            "resetSystemState",
        ]
        
        for item in recovery_items:
            if item not in content:
                print(f"  Missing recovery item: {item}")
                return False
        return True

    def test_platformio_ini_selftest_env(self):
        """Test that platformio.ini contains esp32dev_selftest"""
        ini_path = self.project_dir / "platformio.ini"
        content = ini_path.read_text()
        return "[env:esp32dev_selftest]" in content

    def test_platformio_ini_selftest_flag(self):
        """Test that selftest environment enables SERIAL_SELF_TEST"""
        ini_path = self.project_dir / "platformio.ini"
        content = ini_path.read_text()
        
        # Find the selftest section
        if "[env:esp32dev_selftest]" not in content:
            return False
        
        selftest_section = content.split("[env:esp32dev_selftest]")[1]
        
        # Should stop at next section or EOF
        if "[env:" in selftest_section:
            selftest_section = selftest_section.split("[env:")[0]
        
        return "-DENABLE_SERIAL_SELF_TEST=1" in selftest_section

    def test_no_psram_flag_in_default(self):
        """Test that PSRAM flag has been removed from default env"""
        ini_path = self.project_dir / "platformio.ini"
        content = ini_path.read_text()
        
        # Get default environment section
        default_section = content.split("[env:esp32dev]")[1]
        if "[env:" in default_section:
            default_section = default_section.split("[env:")[0]
        
        # Should NOT contain PSRAM flag in default (it was removed)
        if "-DBOARD_HAS_PSRAM" in default_section:
            print("  PSRAM flag still in default environment")
            return False
        return True

    def test_changelog_overvoltage_correct(self):
        """Test that CHANGELOG has correct overvoltage default"""
        changelog_path = self.project_dir / "CHANGELOG.md"
        content = changelog_path.read_text()
        
        # Should have 26V, not 60V
        if "default 60V" in content:
            print("  CHANGELOG still contains old 60V default")
            return False
        if "default 26V" not in content and "26V" not in content:
            self.warnings += 1
            print("  WARNING: No 26V reference found in CHANGELOG")
        return True

    def run_all_tests(self):
        """Run all test suites"""
        print("=" * 60)
        print("ESP32RC Build Sanity Tests")
        print("=" * 60)
        
        print("\n--- Build Tests ---")
        self.run_test("Build esp32dev environment", self.test_build_env_esp32dev)
        self.run_test("Build esp32dev_selftest environment", self.test_build_env_selftest)
        
        print("\n--- Configuration Tests ---")
        self.run_test("Config header exists", self.test_config_header_exists)
        self.run_test("Config header constants defined", self.test_config_header_constants)
        self.run_test("PlatformIO selftest environment exists", self.test_platformio_ini_selftest_env)
        self.run_test("PlatformIO selftest flag set", self.test_platformio_ini_selftest_flag)
        self.run_test("PSRAM flag removed from default", self.test_no_psram_flag_in_default)
        self.run_test("CHANGELOG overvoltage correct", self.test_changelog_overvoltage_correct)
        
        print("\n--- Source Code Tests ---")
        self.run_test("Main.cpp exists", self.test_main_cpp_exists)
        self.run_test("Main.cpp functions present", self.test_main_cpp_functions)
        self.run_test("Atomic read helpers present", self.test_atomic_reads_present)
        self.run_test("Recovery mechanism present", self.test_recovery_mechanism_present)
        
        print("\n" + "=" * 60)
        print(f"Results: {self.passed} passed, {self.failed} failed, {self.warnings} warnings")
        print("=" * 60)
        
        return self.failed == 0


if __name__ == "__main__":
    project_dir = Path(__file__).parent.parent
    tester = BuildSanityTest(project_dir)
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)
